#include "stdafx.h"
#include "RenderEffects.h"
#include <Windows.h>
#include <algorithm>

namespace Gdiplus
{
using std::min;
using std::max;
}

#include <gdiplus.h>
#include <cmath>
#include <cstdio>
#include <vector>
#pragma comment(lib, "gdiplus.lib")

namespace
{
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
out vec3 world;
out vec4 shadowPos;
out vec4 tint;
out float distanceEye;
void main(){
 vec4 eye=gl_ModelViewMatrix*gl_Vertex;
 gl_Position=gl_ProjectionMatrix*eye;
 normalEye=normalize(gl_NormalMatrix*gl_Normal);
 world=(worldFromEye*eye).xyz;
 shadowPos=shadowFromEye*eye;
 tint=gl_Color;
 distanceEye=length(eye.xyz);
})GLSL";
const char *litFs = R"GLSL(#version 130
uniform sampler2D shadowMap;
uniform float clockTime;
uniform int material;
uniform int useShadow;
in vec3 normalEye;in vec3 world;in vec4 shadowPos;in vec4 tint;in float distanceEye;
float hash(vec2 p){return fract(sin(dot(p,vec2(127.1,311.7)))*43758.5453);}
float noise(vec2 p){vec2 i=floor(p),f=fract(p);f=f*f*(3.0-2.0*f);return mix(mix(hash(i),hash(i+vec2(1,0)),f.x),mix(hash(i+vec2(0,1)),hash(i+vec2(1)),f.x),f.y);}
void main(){
 vec3 n=normalize(normalEye),l=normalize(gl_LightSource[0].position.xyz);
 float nd=max(dot(n,l),0.0),shadow=1.0;
 vec3 sc=shadowPos.xyz/shadowPos.w*0.5+0.5;
 if(useShadow==1 && sc.x>0.0&&sc.x<1.0&&sc.y>0.0&&sc.y<1.0&&sc.z<1.0){
  shadow=0.0;float bias=max(0.00015,0.0007*(1.0-nd));
  for(int y=-1;y<=1;y++)for(int x=-1;x<=1;x++)shadow+=sc.z-bias<=texture2D(shadowMap,sc.xy+vec2(x,y)/2048.0).r?1.0:0.0;
  shadow/=9.0;
 }
 vec3 base=tint.rgb;float grain=noise(world.xz*15.0+world.y*7.0);
 if(material==1){
  float seams=step(0.975,fract(world.x*0.5))*0.12+step(0.975,fract(world.z*0.5))*0.12;
  base*=0.82+grain*0.30-seams;
  base=mix(base,vec3(0.14,0.19,0.12),noise(world.xz*0.5)*0.18);
 }else if(material==2){
  base*=0.70+grain*0.40;
  float marking=(1.0-step(0.075,abs(fract(world.x*0.25)-0.5)))*step(0.35,fract(world.z*0.28));
  base=mix(base,vec3(0.58,0.52,0.33),marking*0.65);
 }else if(material==3){
  float waves=sin(world.x*4.5+clockTime*1.6)*sin(world.z*3.5-clockTime)+noise(world.xz*3.0+clockTime*0.2);
  base=mix(vec3(0.035,0.15,0.16),vec3(0.18,0.39,0.37),waves*0.22+0.38);
  base+=vec3(0.50,0.45,0.28)*pow(max(0.0,sin(world.x*8.0+world.z*5.0-clockTime*2.0)),28.0)*0.5;
 }else if(material==4){
  float ribs=0.82+0.18*step(0.18,fract((world.x+world.z)*5.0));
  float rust=smoothstep(0.60,0.82,noise(world.xz*2.0+world.y));
  base=mix(base*ribs,vec3(0.32,0.16,0.08),rust*0.58);
 }else if(material==5){base*=0.65+noise(world.xz*3.0)*0.55;}
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
uniform sampler2D scene;uniform sampler2D depthTex;uniform vec2 texel;
in vec2 uv;
void main(){
 vec3 c=texture2D(scene,uv).rgb,bloom=vec3(0);float d=texture2D(depthTex,uv).r,occ=0.0;
 for(int y=-2;y<=2;y++)for(int x=-2;x<=2;x++){
  vec2 o=vec2(x,y);vec3 s=texture2D(scene,uv+o*texel*3.0).rgb;
  bloom+=max(s-vec3(0.62),vec3(0));
  float other=texture2D(depthTex,uv+o*texel*2.0).r;
  float delta=d-other;occ+=step(0.0007,delta)*(1.0-smoothstep(0.006,0.020,delta));
 }
 c*=1.0-min(0.18,occ/25.0*0.35);c+=bloom/25.0*0.40;
 vec3 near=(texture2D(scene,uv+vec2(texel.x,0)).rgb+texture2D(scene,uv-vec2(texel.x,0)).rgb+texture2D(scene,uv+vec2(0,texel.y)).rgb+texture2D(scene,uv-vec2(0,texel.y)).rgb)*0.25;
 // Preserve sprite detail; only a small edge blend is needed at native resolution.
 float edge=length(c-near);c=mix(c,near,clamp(edge*0.15,0.0,0.05));
 c=pow(max(c,vec3(0)),vec3(0.94));c*=vec3(1.045,1.01,0.96);
 float vignette=1.0-0.20*dot(uv-0.5,uv-0.5)*2.0;
 gl_FragColor=vec4(c*vignette,1);
})GLSL";
}

void RenderEffects::Initialize()
{
    litProgram = Program(litVs, litFs);
    postProgram = Program(postVs, postFs);
    fireProgram = Program(fireVs, fireFs);
    glGenFramebuffers(1, &shadowFbo);
    glBindFramebuffer(GL_FRAMEBUFFER, shadowFbo);
    glGenTextures(1, &shadowDepth);
    glBindTexture(GL_TEXTURE_2D, shadowDepth);
    glTexImage2D(GL_TEXTURE_2D,
                 0,
                 GL_DEPTH_COMPONENT24,
                 2048,
                 2048,
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
    status = litProgram && postProgram && shadowFbo && fireProgram
                 ? "부드러운 그림자 · 후처리 활성"
                 : "일부 렌더 효과 초기화 실패 · 기본 렌더링";
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
    if (width == w && height == h && sceneFbo)
        return;
    FreeScene();
    width = w;
    height = h;
    glGenFramebuffers(1, &sceneFbo);
    glBindFramebuffer(GL_FRAMEBUFFER, sceneFbo);
    glGenTextures(1, &sceneColor);
    glBindTexture(GL_TEXTURE_2D, sceneColor);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
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
                 w,
                 h,
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

bool RenderEffects::BeginShadow(float centerX, float centerZ, float extent)
{
    if (!shadowFbo || !litProgram)
        return false;
    glUseProgram(0);
    glBindFramebuffer(GL_FRAMEBUFFER, shadowFbo);
    glViewport(0, 0, 2048, 2048);
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
    gluLookAt(centerX - 40, extent + 40, centerZ - 35, centerX, 0, centerZ, 0, 1, 0);
    glGetDoublev(GL_MODELVIEW_MATRIX, v);
    Multiply(p, v, lightMatrix);
    glEnable(GL_POLYGON_OFFSET_FILL);
    glPolygonOffset(1.5f, 2);
    return true;
}

void RenderEffects::BeginScene()
{
    glDisable(GL_POLYGON_OFFSET_FILL);
    glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
    glBindFramebuffer(GL_FRAMEBUFFER, Ready() ? sceneFbo : 0);
    glViewport(0, 0, width, height);
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
    glUniform1i(glGetUniformLocation(litProgram, "useShadow"), shadowFbo ? 1 : 0);
    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, shadowDepth);
    glActiveTexture(GL_TEXTURE0);
    glUniform1i(glGetUniformLocation(litProgram, "shadowMap"), 1);
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
    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, sceneDepth);
    glActiveTexture(GL_TEXTURE0);
    glUniform1i(glGetUniformLocation(postProgram, "scene"), 0);
    glUniform1i(glGetUniformLocation(postProgram, "depthTex"), 1);
    glUniform2f(glGetUniformLocation(postProgram, "texel"), 1.0f / width, 1.0f / height);
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
    int count = MultiByteToWideChar(CP_UTF8, 0, text.c_str(), -1, nullptr, 0);
    std::wstring wide(size_t(std::max(1, count)), L'\0');
    MultiByteToWideChar(CP_UTF8, 0, text.c_str(), -1, &wide[0], count);
    HDC dc = CreateCompatibleDC(nullptr);
    HFONT font = CreateFontW(-pixels,
                             0,
                             0,
                             0,
                             FW_MEDIUM,
                             FALSE,
                             FALSE,
                             FALSE,
                             DEFAULT_CHARSET,
                             OUT_DEFAULT_PRECIS,
                             CLIP_DEFAULT_PRECIS,
                             ANTIALIASED_QUALITY,
                             DEFAULT_PITCH,
                             L"Malgun Gothic");
    HGDIOBJ oldFont = SelectObject(dc, font);
    SIZE size{};
    GetTextExtentPoint32W(dc, wide.c_str(), int(wide.size()) - 1, &size);
    Label label;
    label.w = std::max(1, int(size.cx) + 4);
    label.h = pixels + 12;
    BITMAPINFO info{};
    info.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    info.bmiHeader.biWidth = label.w;
    info.bmiHeader.biHeight = -label.h;
    info.bmiHeader.biPlanes = 1;
    info.bmiHeader.biBitCount = 32;
    info.bmiHeader.biCompression = BI_RGB;
    void *bits = nullptr;
    HBITMAP bitmap = CreateDIBSection(dc, &info, DIB_RGB_COLORS, &bits, nullptr, 0);
    if (bitmap && bits)
    {
        HGDIOBJ oldBitmap = SelectObject(dc, bitmap);
        std::fill_n(static_cast<unsigned char *>(bits), label.w * label.h * 4, 0);
        SetBkMode(dc, TRANSPARENT);
        SetTextColor(dc, RGB(255, 255, 255));
        TextOutW(dc, 2, 2, wide.c_str(), int(wide.size()) - 1);
        GdiFlush();
        auto *p = static_cast<unsigned char *>(bits);
        for (int i = 0; i < label.w * label.h; ++i)
        {
            unsigned char a = p[i * 4];
            p[i * 4] = p[i * 4 + 1] = p[i * 4 + 2] = 255;
            p[i * 4 + 3] = a;
        }
        glGenTextures(1, &label.texture);
        glBindTexture(GL_TEXTURE_2D, label.texture);
        glTexImage2D(
            GL_TEXTURE_2D, 0, GL_RGBA, label.w, label.h, 0, GL_RGBA, GL_UNSIGNED_BYTE, bits);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        SelectObject(dc, oldBitmap);
        DeleteObject(bitmap);
    }
    SelectObject(dc, oldFont);
    DeleteObject(font);
    DeleteDC(dc);
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
    using namespace Gdiplus;
    GdiplusStartupInput input;
    ULONG_PTR token = 0;
    if (GdiplusStartup(&token, &input, nullptr) != Ok)
        return;
    for (int kind = 0; kind < 4; ++kind)
        for (int frame = 0; frame < 8; ++frame)
        {
            Bitmap bitmap(512, 640, PixelFormat32bppARGB);
            {
                Graphics g(&bitmap);
                g.ScaleTransform(4.0f, 4.0f);
                g.SetSmoothingMode(SmoothingModeAntiAlias);
                g.Clear(Color(0, 0, 0, 0));
                float swing = std::sin(frame * 6.2831853f / 8) * 7;
                SolidBrush outline(Color(255, 24, 35, 39)),
                    coat(kind == 2   ? Color(255, 121, 58, 50)
                         : kind == 3 ? Color(255, 120, 116, 87)
                                     : Color(255, 67, 89, 83));
                SolidBrush hair(kind == 1 ? Color(255, 182, 174, 146) : Color(255, 65, 73, 72)),
                    skin(Color(255, 223, 196, 162));
                SolidBrush accent(kind == 2 ? Color(255, 240, 104, 73) : Color(255, 91, 209, 194));
                if (kind == 3)
                {
                    g.FillEllipse(&outline, 24, 65, 78, 54);
                    g.FillEllipse(&coat, 29, 62, 70, 49);
                    g.FillEllipse(&coat, 76, 50, 39, 34);
                    g.FillRectangle(&outline, 34, 105, 13, 30 + int(swing));
                    g.FillRectangle(&outline, 81, 105, 13, 30 - int(swing));
                    g.FillEllipse(&accent, 98, 59, 7, 5);
                    Point ear[] = {Point(83, 54), Point(82, 32), Point(96, 51)};
                    g.FillPolygon(&coat, ear, 3);
                }
                else
                {
                    g.FillRectangle(&outline, 43, 107, 17, 35 + int(swing));
                    g.FillRectangle(&outline, 69, 107, 17, 35 - int(swing));
                    g.FillEllipse(&outline, 31, 66, 67, 58);
                    g.FillEllipse(&coat, 35, 65, 58, 55);
                    g.FillEllipse(&hair, 28, 17, 75, 68);
                    g.FillEllipse(&skin, 39, 33, 53, 43);
                    g.FillEllipse(&hair, 29, 17, 73, 30);
                    g.FillRectangle(&hair, 28, 34, 13, 49);
                    g.FillRectangle(&hair, 91, 35, 12, 45);
                    g.FillEllipse(&outline, 49, 51, 5, 7);
                    g.FillEllipse(&outline, 76, 51, 5, 7);
                    g.FillRectangle(&coat, 23, 77 + int(swing * .4f), 19, 37);
                    g.FillRectangle(&outline, 72, 81, 43, 12);
                    g.FillRectangle(&outline, 101, 77, 19, 7);
                    g.FillRectangle(&accent, 47, 84, 12, 14);
                    g.FillRectangle(&outline, 55, 99, 10, 20);
                }
            }
            Rect rect(0, 0, 512, 640);
            BitmapData data{};
            if (bitmap.LockBits(&rect, ImageLockModeRead, PixelFormat32bppARGB, &data) == Ok)
            {
                std::vector<unsigned char> rgba(512 * 640 * 4);
                for (int y = 0; y < 640; ++y)
                    for (int x = 0; x < 512; ++x)
                    {
                        auto *src =
                            static_cast<unsigned char *>(data.Scan0) + y * data.Stride + x * 4;
                        auto *dst = &rgba[(y * 512 + x) * 4];
                        dst[0] = src[2];
                        dst[1] = src[1];
                        dst[2] = src[0];
                        dst[3] = src[3];
                    }
                glGenTextures(1, &sprites[kind][frame]);
                glBindTexture(GL_TEXTURE_2D, sprites[kind][frame]);
                glTexImage2D(
                    GL_TEXTURE_2D, 0, GL_RGBA, 512, 640, 0, GL_RGBA, GL_UNSIGNED_BYTE, rgba.data());
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
                bitmap.UnlockBits(&data);
            }
        }
    GdiplusShutdown(token);
}

void RenderEffects::Shutdown()
{
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

void RenderEffects::Fire(float time)
{
    glUseProgram(fireProgram);
    if (fireProgram)
        glUniform1f(glGetUniformLocation(fireProgram, "clockTime"), time);
    else
        glColor4f(1, .45f, .05f, .65f);
}
