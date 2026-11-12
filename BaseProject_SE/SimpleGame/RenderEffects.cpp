#include "stdafx.h"
#include "RenderEffects.h"
#include "SpriteRasterizer.h"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdint>
#include <fstream>
#include <iterator>

namespace
{
constexpr int MaterialTextureSize = 1024;

std::vector<int> DecodeUtf8(const std::string &text)
{
    std::vector<int> codepoints;
    for (std::size_t index = 0; index < text.size();)
    {
        const auto first = static_cast<unsigned char>(text[index]);
        int codepoint = 0xfffd;
        std::size_t length = 1;
        if (first < 0x80)
            codepoint = first;
        else if ((first & 0xe0) == 0xc0 && index + 1 < text.size())
        {
            codepoint = ((first & 0x1f) << 6) |
                        (static_cast<unsigned char>(text[index + 1]) & 0x3f);
            length = 2;
        }
        else if ((first & 0xf0) == 0xe0 && index + 2 < text.size())
        {
            codepoint = ((first & 0x0f) << 12) |
                        ((static_cast<unsigned char>(text[index + 1]) & 0x3f) << 6) |
                        (static_cast<unsigned char>(text[index + 2]) & 0x3f);
            length = 3;
        }
        else if ((first & 0xf8) == 0xf0 && index + 3 < text.size())
        {
            codepoint = ((first & 0x07) << 18) |
                        ((static_cast<unsigned char>(text[index + 1]) & 0x3f) << 12) |
                        ((static_cast<unsigned char>(text[index + 2]) & 0x3f) << 6) |
                        (static_cast<unsigned char>(text[index + 3]) & 0x3f);
            length = 4;
        }
        codepoints.push_back(codepoint);
        index += length;
    }
    return codepoints;
}

float MaterialHash(int x, int y)
{
    uint32_t value = uint32_t(x) * 0x9e3779b9u ^ uint32_t(y) * 0x85ebca6bu;
    value ^= value >> 16;
    value *= 0x7feb352du;
    value ^= value >> 15;
    return float(value & 0xffffu) / 65535.0f;
}

float MaterialNoise(int x, int y, int cellSize)
{
    int period = MaterialTextureSize / cellSize;
    int ix = x / cellSize, iy = y / cellSize;
    float fx = float(x % cellSize) / cellSize, fy = float(y % cellSize) / cellSize;
    fx = fx * fx * (3 - 2 * fx);
    fy = fy * fy * (3 - 2 * fy);
    auto sample = [&](int dx, int dy)
    {
        return MaterialHash((ix + dx) % period, (iy + dy) % period);
    };
    float a = sample(0, 0) * (1 - fx) + sample(1, 0) * fx;
    float b = sample(0, 1) * (1 - fx) + sample(1, 1) * fx;
    return a * (1 - fy) + b * fy;
}

std::vector<unsigned char> MaterialPixels(int material)
{
    std::vector<unsigned char> pixels(MaterialTextureSize * MaterialTextureSize * 4);
    for (int y = 0; y < MaterialTextureSize; ++y)
        for (int x = 0; x < MaterialTextureSize; ++x)
        {
            float broad = MaterialNoise(x, y, 128), medium = MaterialNoise(x, y, 32);
            float detail = MaterialNoise(x, y, 8), fine = MaterialNoise(x, y, 2);
            float cell = MaterialHash(x / 16, y / 16);
            float grain = .70f + .14f * medium + .12f * detail + .08f * fine;
            std::array<float, 4> color{};
            if (material == 0)
            {
                // Fine grass blades over soil patches, rather than one blurred noise layer.
                float soil = std::clamp((broad * .6f + medium * .4f - .48f) * 3, 0.0f, 1.0f);
                color = {{(.27f + soil * .08f) * grain,
                          (.36f - soil * .05f) * grain,
                          (.19f + soil * .02f) * grain,
                          1}};
                int bladeY = y % 16 - int(cell * 6);
                int bladeX = x % 16 - (3 + int(cell * 8)) - bladeY / 4;
                if (bladeY >= 0 && bladeY < 9 && bladeX >= 0 && bladeX <= 1)
                {
                    color[0] += .07f;
                    color[1] += .10f;
                    color[2] += .025f;
                }
            }
            else if (material == 1)
            {
                // Small stones have a lit rim and a dark edge baked into the road material.
                float dx = float(x % 16) - (5 + cell * 5);
                float dy = float(y % 16) - (5 + MaterialHash(y / 16, x / 16 + 71) * 5);
                float radius = 2 + cell * 3;
                float stone = std::clamp(1 - (dx * dx + dy * dy) / (radius * radius), 0.0f, 1.0f);
                float shade =
                    grain + stone * (.22f + cell * .12f) - (stone > 0 && stone < .25f ? .12f : 0);
                color = {{.46f * shade, .43f * shade, .34f * shade, 1}};
            }
            else
            {
                // Alpha stores rust coverage; RGB carries metal grain and fine scratches.
                float scratch = x % 64 == int(MaterialHash(0, y / 96) * 64) ? .12f : 0;
                float steel = .76f + detail * .18f + fine * .10f - scratch;
                float rust = std::clamp((broad * .65f + medium * .35f - .55f) * 5, 0.0f, 1.0f);
                color = {{steel, steel, steel, rust}};
            }
            std::size_t index = (std::size_t(y) * MaterialTextureSize + x) * 4;
            for (int channel = 0; channel < 4; ++channel)
                pixels[index + channel] =
                    static_cast<unsigned char>(std::clamp(color[channel], 0.0f, 1.0f) * 255);
        }
    return pixels;
}

GLuint Program(const char *vs, const char *fs)
{
    GLuint shaders[2] = {glCreateShader(GL_VERTEX_SHADER), glCreateShader(GL_FRAGMENT_SHADER)};
    const char *sources[2] = {vs, fs};
    bool ok = true;
    for (int i = 0; i < 2; ++i)
    {
        glShaderSource(shaders[i], 1, &sources[i], nullptr);
        glCompileShader(shaders[i]);
        GLint success = 0;
        glGetShaderiv(shaders[i], GL_COMPILE_STATUS, &success);
        if (!success)
        {
            char log[4096]{};
            glGetShaderInfoLog(shaders[i], 4096, nullptr, log);
            std::fprintf(stderr, "Shader: %s\n", log);
            ok = false;
        }
    }
    GLuint p = 0;
    if (ok)
    {
        p = glCreateProgram();
        for (GLuint s : shaders)
            glAttachShader(p, s);
        glLinkProgram(p);
        GLint success = 0;
        glGetProgramiv(p, GL_LINK_STATUS, &success);
        if (!success)
        {
            char log[4096]{};
            glGetProgramInfoLog(p, 4096, nullptr, log);
            std::fprintf(stderr, "Link: %s\n", log);
            glDeleteProgram(p);
            p = 0;
        }
    }
    for (GLuint s : shaders)
        glDeleteShader(s);
    return p;
}

void Multiply(const GLdouble *a, const GLdouble *b, GLdouble *out)
{
    for (int c = 0; c < 4; ++c)
        for (int r = 0; r < 4; ++r)
        {
            out[c * 4 + r] = 0;
            for (int k = 0; k < 4; ++k)
                out[c * 4 + r] += a[k * 4 + r] * b[c * 4 + k];
        }
}

void RigidInverse(const GLdouble *m, GLdouble *out)
{
    std::fill(out, out + 16, 0.0);
    out[15] = 1;
    for (int c = 0; c < 3; ++c)
        for (int r = 0; r < 3; ++r)
            out[c * 4 + r] = m[r * 4 + c];
    for (int r = 0; r < 3; ++r)
        out[12 + r] = -(out[r] * m[12] + out[4 + r] * m[13] + out[8 + r] * m[14]);
}

const char *litVs = R"GLSL(#version 130
uniform mat4 shadowFromEye;
uniform mat4 worldFromEye;
out vec3 normalEye;
out vec3 normalWorld;
out vec3 world;
out vec4 shadowPos;
out vec4 tint;
out float distanceEye;
out vec3 resourceUv;
void main(){
 vec4 eye=gl_ModelViewMatrix*gl_Vertex;
 gl_Position=gl_ProjectionMatrix*eye;
 normalEye=normalize(gl_NormalMatrix*gl_Normal);
 normalWorld = mat3(worldFromEye) * normalEye;
 world=(worldFromEye*eye).xyz;
 shadowPos=shadowFromEye*eye;
 tint=gl_Color;
 distanceEye=length(eye.xyz);
 resourceUv=gl_MultiTexCoord0.xyz;
})GLSL";
const char *litFs = R"GLSL(#version 130
uniform sampler2D shadowMap;
uniform sampler2D grassMaterial;
uniform sampler2D roadMaterial;
uniform sampler2D metalMaterial;
uniform sampler2D resourceAtlas;
uniform float clockTime;
uniform int material;
uniform int useShadow;
in vec3 normalEye;in vec3 normalWorld;in vec3 world;in vec4 shadowPos;in vec4 tint;in float distanceEye;
in vec3 resourceUv;
float hash(vec2 p){return fract(sin(dot(p,vec2(127.1,311.7)))*43758.5453);}
float noise(vec2 p){vec2 i=floor(p),f=fract(p);f=f*f*(3.0-2.0*f);return mix(mix(hash(i),hash(i+vec2(1,0)),f.x),mix(hash(i+vec2(0,1)),hash(i+vec2(1)),f.x),f.y);}
void main(){
 vec3 n=normalize(normalEye),l=normalize(gl_LightSource[0].position.xyz);
 float nd=max(dot(n,l),0.0),shadow=1.0;
 vec3 sc=shadowPos.xyz/shadowPos.w*0.5+0.5;
 if(useShadow==1 && sc.x>0.0&&sc.x<1.0&&sc.y>0.0&&sc.y<1.0&&sc.z<1.0){
  shadow=0.0;float bias=max(0.000025,0.00015*(1.0-nd));
  shadow=sc.z-bias<=texture2D(shadowMap,sc.xy).r?1.0:0.0;
 }
 vec3 base=tint.rgb;float grain=noise(world.xz*15.0+world.y*7.0);
 if(material==1){
  float seams=step(0.975,fract(world.x*0.5))*0.12+step(0.975,fract(world.z*0.5))*0.12;
  base*=0.82+grain*0.30-seams;
  base=mix(base,vec3(0.14,0.19,0.12),noise(world.xz*0.5)*0.18);
 }else if(material==2){
  base = mix(base, texture2D(roadMaterial, world.xz * 0.25).rgb, 0.88);
  base *= 0.92 + noise(world.xz * 6.0) * 0.16;
 }else if(material==3){
  float waves=sin(world.x*4.5+clockTime*1.6)*sin(world.z*3.5-clockTime)+noise(world.xz*3.0+clockTime*0.2);
  base=mix(vec3(0.035,0.15,0.16),vec3(0.18,0.39,0.37),waves*0.22+0.38);
  base+=vec3(0.50,0.45,0.28)*pow(max(0.0,sin(world.x*8.0+world.z*5.0-clockTime*2.0)),28.0)*0.5;
 }else if(material==4){
    // Project onto each face in world units, so tall walls do not stretch roof detail.
    vec3 axis = abs(normalize(normalWorld));
    vec2 surface = axis.y > axis.x && axis.y > axis.z ? world.xz :
        (axis.x > axis.z ? world.zy : world.xy);
    float board = abs(fract(surface.y * 3.0) - 0.5);
    float seam = 1.0 - smoothstep(0.012, 0.045, board);
    float grain = noise(vec2(surface.x * 9.0, surface.y * 0.8));
    base *= 0.76 + grain * 0.35 - seam * 0.22;
 }
 else if (material == 5)
 {
    base = mix(base, texture2D(grassMaterial, world.xz * 0.25).rgb, 0.90);
    base *= 0.88 + noise(world.xz * 0.18) * 0.24;
 }
 else if (material == 8)
 {
    float tile = floor(resourceUv.z + 0.5);
    vec2 cell = vec2(mod(tile, 8.0), floor(tile / 8.0));
    vec2 atlasUv = (cell * 260.0 + 2.0 + fract(resourceUv.xy) * 256.0) / 2080.0;
    base = tint.rgb * texture2D(resourceAtlas, atlasUv).rgb;
 }
 else if (material == 7)
 {
    base *= 0.90 + noise(world.xz * 2.0 + world.y * 4.0) * 0.16;
 }
 else if (material == 6)
 {
    base *= 0.80 + noise(world.xz * 8.0 + world.y * 5.0) * 0.30;
 }
 vec3 lit=base*(vec3(0.36,0.44,0.48)+vec3(1.0,0.75,0.46)*nd*(0.20+0.80*shadow));
 float haze=smoothstep(30.0,67.0,distanceEye)*0.40;
 gl_FragColor=vec4(mix(lit,vec3(0.23,0.29,0.30),haze),1.0);
})GLSL";
const char *fireVs = R"GLSL(#version 130
out vec2 uv;
void main()
{
    uv = gl_MultiTexCoord0.xy;
    gl_Position = gl_ModelViewProjectionMatrix * gl_Vertex;
}
)GLSL";
const char *fireFs = R"GLSL(#version 130
uniform float clockTime;
in vec2 uv;
void main()
{
    float sway = sin(uv.y * 9.0 - clockTime * 5.0) * 0.055 * uv.y;
    sway += sin(uv.y * 19.0 - clockTime * 8.0) * 0.025;
    float width = (1.0 - uv.y) * 0.42;
    float edge = width - abs(uv.x - 0.5 + sway);
    float alpha = smoothstep(-0.035, 0.055, edge) * (1.0 - smoothstep(0.8, 1.0, uv.y));
    alpha *= smoothstep(0.0, 0.07, uv.y);
    if(alpha < 0.01) discard;
    vec3 color = mix(vec3(1.0, 0.22, 0.015), vec3(1.0, 0.91, 0.35),
        clamp(edge * 5.0 + (1.0 - uv.y) * 0.2, 0.0, 1.0));
    gl_FragColor = vec4(color, alpha * 0.9);
}
)GLSL";
const char *postVs = R"GLSL(#version 130
out vec2 uv;
void main(){gl_Position=gl_Vertex;uv=gl_MultiTexCoord0.xy;})GLSL";
const char *postFs = R"GLSL(#version 130
uniform sampler2D scene;uniform vec2 texel;
in vec2 uv;
void main(){
 vec3 c=texture2D(scene,uv).rgb;
 c=pow(max(c,vec3(0)),vec3(0.94));c*=vec3(1.045,1.01,0.96);
 float vignette=1.0-0.20*dot(uv-0.5,uv-0.5)*2.0;
 gl_FragColor=vec4(c*vignette,1);
})GLSL";

const char *contactFs = R"GLSL(#version 130
in vec2 uv;
void main()
{
    float radius = length((uv - 0.5) * 2.0);
    float alpha = (1.0 - smoothstep(0.08, 1.0, radius)) * 0.32;
    gl_FragColor = vec4(0.015, 0.020, 0.025, alpha);
}
)GLSL";
}

void RenderEffects::Initialize(const std::filesystem::path &fontPath)
{
    std::ifstream fontFile(fontPath, std::ios::binary | std::ios::ate);
    if (fontFile)
    {
        const auto size = fontFile.tellg();
        if (size > 0)
        {
            fontBytes.resize(std::size_t(size));
            fontFile.seekg(0, std::ios::beg);
            fontFile.read(reinterpret_cast<char *>(fontBytes.data()),
                          static_cast<std::streamsize>(fontBytes.size()));
            fontReady = fontFile.good() || fontFile.eof();
            fontReady = fontReady &&
                        stbtt_InitFont(&font, fontBytes.data(),
                                       stbtt_GetFontOffsetForIndex(fontBytes.data(), 0)) != 0;
        }
    }
    if (!fontReady)
        std::fprintf(stderr,
                     "Could not load bundled UI font: %s\n",
                     fontPath.filename().string().c_str());

    litProgram = Program(litVs, litFs);
    postProgram = Program(postVs, postFs);
    fireProgram = Program(fireVs, fireFs);
    contactProgram = Program(fireVs, contactFs);
    CreateTerrainMaterials();
    glGenFramebuffers(1, &shadowFbo);
    glBindFramebuffer(GL_FRAMEBUFFER, shadowFbo);
    glGenTextures(1, &shadowDepth);
    glBindTexture(GL_TEXTURE_2D, shadowDepth);
    glTexImage2D(GL_TEXTURE_2D,
                 0,
                 GL_DEPTH_COMPONENT24,
                 1024,
                 1024,
                 0,
                 GL_DEPTH_COMPONENT,
                 GL_UNSIGNED_INT,
                 nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, shadowDepth, 0);
    glDrawBuffer(GL_NONE);
    glReadBuffer(GL_NONE);
    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
    {
        glDeleteFramebuffers(1, &shadowFbo);
        shadowFbo = 0;
    }
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    CreateSprites();
    status = litProgram && postProgram && shadowFbo && fireProgram && contactProgram
                 ? "부드러운 그림자 · 후처리 활성"
                 : "일부 렌더 효과 초기화 실패 · 기본 렌더링";
}

void RenderEffects::CreateTerrainMaterials()
{
    auto started = std::chrono::steady_clock::now();
    float anisotropy = 1;
    if (GLEW_EXT_texture_filter_anisotropic)
    {
        glGetFloatv(GL_MAX_TEXTURE_MAX_ANISOTROPY_EXT, &anisotropy);
        anisotropy = std::min(anisotropy, 8.0f);
    }
    glActiveTexture(GL_TEXTURE0);
    glGenTextures(GLsizei(terrainMaterials.size()), terrainMaterials.data());
    for (int material = 0; material < int(terrainMaterials.size()); ++material)
    {
        auto pixels = MaterialPixels(material);
        glBindTexture(GL_TEXTURE_2D, terrainMaterials[material]);
        glTexImage2D(GL_TEXTURE_2D,
                     0,
                     GL_RGBA8,
                     MaterialTextureSize,
                     MaterialTextureSize,
                     0,
                     GL_RGBA,
                     GL_UNSIGNED_BYTE,
                     pixels.data());
        glGenerateMipmap(GL_TEXTURE_2D);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
        if (GLEW_EXT_texture_filter_anisotropic)
            glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAX_ANISOTROPY_EXT, anisotropy);
    }
    glBindTexture(GL_TEXTURE_2D, 0);
    float cpuMs =
        std::chrono::duration<float, std::milli>(std::chrono::steady_clock::now() - started)
            .count();
    std::size_t gpuBytes = 0;
    for (int side = MaterialTextureSize; side > 0; side /= 2)
        gpuBytes += std::size_t(side) * side * 4 * terrainMaterials.size();
    std::fprintf(stdout,
                 "{\"event\":\"terrain_materials_initialized\",\"texture_count\":3,"
                 "\"texture_size_px\":%d,\"tile_world_units\":4,\"anisotropy\":%.1f,"
                 "\"estimated_gpu_bytes\":%zu,\"cpu_initialize_ms\":%.3f}\n",
                 MaterialTextureSize,
                 anisotropy,
                 gpuBytes,
                 cpuMs);
}

void RenderEffects::FreeScene()
{
    if (sceneFbo)
        glDeleteFramebuffers(1, &sceneFbo);
    if (sceneColor)
        glDeleteTextures(1, &sceneColor);
    if (sceneDepth)
        glDeleteTextures(1, &sceneDepth);
    sceneFbo = sceneColor = sceneDepth = 0;
}

void RenderEffects::Resize(int w, int h)
{
    constexpr float SceneScale = .35f;
    const int targetWidth = std::max(1, int(std::lround(w * SceneScale)));
    const int targetHeight = std::max(1, int(std::lround(h * SceneScale)));
    if (width == w && height == h && sceneFbo)
        return;
    FreeScene();
    width = w;
    height = h;
    sceneWidth = targetWidth;
    sceneHeight = targetHeight;
    glGenFramebuffers(1, &sceneFbo);
    glBindFramebuffer(GL_FRAMEBUFFER, sceneFbo);
    glGenTextures(1, &sceneColor);
    glBindTexture(GL_TEXTURE_2D, sceneColor);
    glTexImage2D(GL_TEXTURE_2D,
                 0,
                 GL_RGBA8,
                 sceneWidth,
                 sceneHeight,
                 0,
                 GL_RGBA,
                 GL_UNSIGNED_BYTE,
                 nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, sceneColor, 0);
    glGenTextures(1, &sceneDepth);
    glBindTexture(GL_TEXTURE_2D, sceneDepth);
    glTexImage2D(GL_TEXTURE_2D,
                 0,
                 GL_DEPTH_COMPONENT24,
                 sceneWidth,
                 sceneHeight,
                 0,
                 GL_DEPTH_COMPONENT,
                 GL_UNSIGNED_INT,
                 nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, sceneDepth, 0);
    glDrawBuffer(GL_COLOR_ATTACHMENT0);
    glReadBuffer(GL_COLOR_ATTACHMENT0);
    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
    {
        FreeScene();
        status = "후처리 버퍼 생성 실패 · 기본 렌더링";
    }
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

int RenderEffects::SceneWidth() const
{
    return sceneWidth;
}

int RenderEffects::SceneHeight() const
{
    return sceneHeight;
}

bool RenderEffects::BeginShadow(float centerX, float centerZ, float extent)
{
    if (!shadowFbo || !litProgram)
        return false;
    glUseProgram(0);
    glBindFramebuffer(GL_FRAMEBUFFER, shadowFbo);
    glViewport(0, 0, 1024, 1024);
    glColorMask(GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE);
    glDepthMask(GL_TRUE);
    glEnable(GL_DEPTH_TEST);
    glClear(GL_DEPTH_BUFFER_BIT);
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    glOrtho(-extent, extent, -extent, extent, 1, extent * 4 + 150);
    GLdouble p[16], v[16];
    glGetDoublev(GL_PROJECTION_MATRIX, p);
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
    // Match the fixed world light (-10, 25, -8); zoom must not change shadow direction.
    gluLookAt(centerX - 40, 100, centerZ - 32, centerX, 0, centerZ, 0, 1, 0);
    glGetDoublev(GL_MODELVIEW_MATRIX, v);
    Multiply(p, v, lightMatrix);
    glEnable(GL_POLYGON_OFFSET_FILL);
    glPolygonOffset(.8f, 1);
    return true;
}

void RenderEffects::BeginScene()
{
    glDisable(GL_POLYGON_OFFSET_FILL);
    glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
    glBindFramebuffer(GL_FRAMEBUFFER, Ready() ? sceneFbo : 0);
    glViewport(0, 0, sceneWidth, sceneHeight);
    glClearColor(.16f, .22f, .24f, 1);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
}

void RenderEffects::Lit(const GLdouble *view, float time, int material)
{
    if (!litProgram)
    {
        glUseProgram(0);
        return;
    }
    GLdouble inv[16], combined[16];
    RigidInverse(view, inv);
    Multiply(lightMatrix, inv, combined);
    GLfloat a[16], b[16];
    for (int i = 0; i < 16; ++i)
    {
        a[i] = float(combined[i]);
        b[i] = float(inv[i]);
    }
    glUseProgram(litProgram);
    glUniformMatrix4fv(glGetUniformLocation(litProgram, "shadowFromEye"), 1, GL_FALSE, a);
    glUniformMatrix4fv(glGetUniformLocation(litProgram, "worldFromEye"), 1, GL_FALSE, b);
    glUniform1f(glGetUniformLocation(litProgram, "clockTime"), time);
    glUniform1i(glGetUniformLocation(litProgram, "useShadow"), 0);
    glUniform1i(glGetUniformLocation(litProgram, "resourceAtlas"), 0);
    const char *samplers[] = {"grassMaterial", "roadMaterial", "metalMaterial"};
    for (int i = 0; i < int(terrainMaterials.size()); ++i)
    {
        glActiveTexture(GL_TEXTURE2 + i);
        glBindTexture(GL_TEXTURE_2D, terrainMaterials[i]);
        glUniform1i(glGetUniformLocation(litProgram, samplers[i]), 2 + i);
    }
    glActiveTexture(GL_TEXTURE0);
    Material(material);
}

void RenderEffects::Material(int value)
{
    if (litProgram)
        glUniform1i(glGetUniformLocation(litProgram, "material"), value);
}

void RenderEffects::Composite()
{
    glUseProgram(0);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glViewport(0, 0, width, height);
    if (!Ready())
        return;
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_LIGHTING);
    glDisable(GL_FOG);
    glDisable(GL_BLEND);
    glUseProgram(postProgram);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, sceneColor);
    glUniform1i(glGetUniformLocation(postProgram, "scene"), 0);
    glUniform2f(glGetUniformLocation(postProgram, "texel"),
                1.0f / sceneWidth,
                1.0f / sceneHeight);
    glBegin(GL_QUADS);
    glTexCoord2f(0, 0);
    glVertex2f(-1, -1);
    glTexCoord2f(1, 0);
    glVertex2f(1, -1);
    glTexCoord2f(1, 1);
    glVertex2f(1, 1);
    glTexCoord2f(0, 1);
    glVertex2f(-1, 1);
    glEnd();
    glUseProgram(0);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
}

RenderEffects::Label &RenderEffects::MakeLabel(const std::string &text, int pixels)
{
    std::string cacheKey = text + "#px" + std::to_string(pixels);
    auto found = labels.find(cacheKey);
    if (found != labels.end())
        return found->second;
    if (labels.size() > 512)
    {
        for (auto &p : labels)
            glDeleteTextures(1, &p.second.texture);
        labels.clear();
    }
    Label label;
    label.h = pixels + 12;
    const auto codepoints = DecodeUtf8(text);
    float scale = 1;
    int ascent = pixels;
    int descent = 0;
    [[maybe_unused]] int lineGap = 0;
    if (fontReady)
    {
        scale = stbtt_ScaleForPixelHeight(&font, float(pixels));
        stbtt_GetFontVMetrics(&font, &ascent, &descent, &lineGap);
    }

    float textAdvance = 0;
    int previousCodepoint = 0;
    for (int codepoint : codepoints)
    {
        int advance = pixels;
        [[maybe_unused]] int leftBearing = 0;
        if (fontReady)
        {
            if (stbtt_FindGlyphIndex(&font, codepoint) == 0 && codepoint != '?')
                codepoint = '?';
            stbtt_GetCodepointHMetrics(&font, codepoint, &advance, &leftBearing);
            if (previousCodepoint)
                textAdvance += scale * stbtt_GetCodepointKernAdvance(
                                         &font, previousCodepoint, codepoint);
        }
        textAdvance += scale * advance;
        previousCodepoint = codepoint;
    }
    label.w = std::max(1, int(std::ceil(textAdvance)) + 4);

    std::vector<unsigned char> rgba(std::size_t(label.w) * label.h * 4, 0);
    if (fontReady && !codepoints.empty())
    {
        const int baseline = 4 + int(std::round(ascent * scale));
        float penX = 2;
        previousCodepoint = 0;
        for (int codepoint : codepoints)
        {
            if (stbtt_FindGlyphIndex(&font, codepoint) == 0 && codepoint != '?')
                codepoint = '?';
            if (previousCodepoint)
                penX += scale * stbtt_GetCodepointKernAdvance(&font, previousCodepoint, codepoint);

            int advance = 0;
            [[maybe_unused]] int leftBearing = 0;
            stbtt_GetCodepointHMetrics(&font, codepoint, &advance, &leftBearing);
            int x0 = 0, y0 = 0, x1 = 0, y1 = 0;
            stbtt_GetCodepointBitmapBox(&font, codepoint, scale, scale, &x0, &y0, &x1, &y1);
            const int glyphWidth = x1 - x0;
            const int glyphHeight = y1 - y0;
            if (glyphWidth > 0 && glyphHeight > 0)
            {
                std::vector<unsigned char> bitmap(std::size_t(glyphWidth) * glyphHeight, 0);
                stbtt_MakeCodepointBitmap(&font,
                                          bitmap.data(),
                                          glyphWidth,
                                          glyphHeight,
                                          glyphWidth,
                                          scale,
                                          scale,
                                          codepoint);
                const int glyphX = int(std::round(penX)) + x0;
                const int glyphY = baseline + y0;
                for (int y = 0; y < glyphHeight; ++y)
                    for (int x = 0; x < glyphWidth; ++x)
                    {
                        const int destX = glyphX + x;
                        const int destY = glyphY + y;
                        if (destX < 0 || destX >= label.w || destY < 0 || destY >= label.h)
                            continue;
                        const std::size_t dest = (std::size_t(destY) * label.w + destX) * 4;
                        rgba[dest] = rgba[dest + 1] = rgba[dest + 2] = 255;
                        const auto source = bitmap[std::size_t(y) * glyphWidth + x];
                        rgba[dest + 3] = std::max(rgba[dest + 3], source);
                    }
            }
            penX += scale * advance;
            previousCodepoint = codepoint;
        }
    }

    glGenTextures(1, &label.texture);
    glBindTexture(GL_TEXTURE_2D, label.texture);
    glTexImage2D(GL_TEXTURE_2D,
                 0,
                 GL_RGBA,
                 label.w,
                 label.h,
                 0,
                 GL_RGBA,
                 GL_UNSIGNED_BYTE,
                 rgba.data());
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    return labels.emplace(cacheKey, label).first->second;
}

float RenderEffects::TextWidth(const std::string &s, float size)
{
    return MakeLabel(s).w * size / 64.0f;
}

void RenderEffects::Text(
    float x, float y, const std::string &s, float size, float r, float g, float b)
{
    if (s.empty())
        return;
    int pixels = std::clamp(
        int(std::ceil(size * std::max(width / 1280.0f, height / 800.0f) * 1.5f)), 16, 128);
    const auto &label = MakeLabel(s, pixels);
    float w = label.w * size / pixels, h = label.h * size / pixels;
    glEnable(GL_TEXTURE_2D);
    glBindTexture(GL_TEXTURE_2D, label.texture);
    glColor4f(r, g, b, 1);
    glBegin(GL_QUADS);
    glTexCoord2f(0, 0);
    glVertex2f(x, y);
    glTexCoord2f(1, 0);
    glVertex2f(x + w, y);
    glTexCoord2f(1, 1);
    glVertex2f(x + w, y + h);
    glTexCoord2f(0, 1);
    glVertex2f(x, y + h);
    glEnd();
    glDisable(GL_TEXTURE_2D);
}

void RenderEffects::CreateSprites()
{
    for (int kind = 0; kind < 4; ++kind)
        for (int frame = 0; frame < 8; ++frame)
        {
            const auto rgba = SpriteRasterizer::Build(kind, frame);
            glGenTextures(1, &sprites[kind][frame]);
            glBindTexture(GL_TEXTURE_2D, sprites[kind][frame]);
            glTexImage2D(GL_TEXTURE_2D,
                         0,
                         GL_RGBA,
                         512,
                         640,
                         0,
                         GL_RGBA,
                         GL_UNSIGNED_BYTE,
                         rgba.data());
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        }
}

void RenderEffects::Shutdown()
{
    if (contactProgram)
        glDeleteProgram(contactProgram);
    contactProgram = 0;
    glDeleteTextures(GLsizei(terrainMaterials.size()), terrainMaterials.data());
    terrainMaterials.fill(0);
    if (fireProgram)
        glDeleteProgram(fireProgram);
    fireProgram = 0;
    FreeScene();
    if (shadowFbo)
        glDeleteFramebuffers(1, &shadowFbo);
    if (shadowDepth)
        glDeleteTextures(1, &shadowDepth);
    if (litProgram)
        glDeleteProgram(litProgram);
    if (postProgram)
        glDeleteProgram(postProgram);
    shadowFbo = shadowDepth = litProgram = postProgram = 0;
    for (auto &p : labels)
        glDeleteTextures(1, &p.second.texture);
    labels.clear();
    for (auto &row : sprites)
        for (auto &t : row)
        {
            if (t)
                glDeleteTextures(1, &t);
            t = 0;
        }
}

bool RenderEffects::BeginContactShadows()
{
    if (!contactProgram)
        return false;
    glUseProgram(contactProgram);
    return true;
}

void RenderEffects::Fire(float time)
{
    glUseProgram(fireProgram);
    if (fireProgram)
        glUniform1f(glGetUniformLocation(fireProgram, "clockTime"), time);
    else
        glColor4f(1, .45f, .05f, .65f);
}
