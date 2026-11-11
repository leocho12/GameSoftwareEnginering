#include "stdafx.h"
#include "Renderer.h"
#include "Platform.h"
#include "RenderGeometry.h"
#include "Dependencies/stb/stb_image.h"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <fstream>
#include <iterator>
#include <limits>
#include <vector>
#include <GL/glu.h>

namespace
{
constexpr float Pi = 3.14159265f;

float UIX(int x, int w)
{
    return x * 1280.0f / w;
}

float UIY(int y, int h)
{
    return y * 800.0f / h;
}

bool In(float x, float y, float l, float t, float w, float h)
{
    return x >= l && x <= l + w && y >= t && y <= t + h;
}

}

Renderer::Renderer(int w, int h)
{
    const auto executableDirectory = Platform::ExecutableDirectory();
    effects.Initialize(executableDirectory / "Assets" / "Fonts" / "NotoSansKR.ttf");
    Resize(w, h);
    resources.Load(executableDirectory / "Assets" / "World" / "kingdom.rpack");
}

Renderer::~Renderer()
{
    Shutdown();
}

void Renderer::Shutdown()
{
    resources.Shutdown();
    for (auto &t : characterTextures)
    {
        if (t)
            glDeleteTextures(1, &t);
        t = 0;
    }
    for (auto &c : characters)
        c.Clear();
    for (auto &t : portraits)
        if (t)
        {
            glDeleteTextures(1, &t);
            t = 0;
        }
    effects.Shutdown();
}

void Renderer::Resize(int w, int h)
{
    width = std::max(1, w);
    height = std::max(1, h);
    cameraReady = false;
    effects.Resize(width, height);
}

void Renderer::Zoom(int direction)
{
    zoom = std::clamp(zoom - direction * .7f, 5.0f, 19.0f);
    cameraReady = false;
}

GLuint Renderer::LoadPng(const std::filesystem::path &path, float &aspect)
{
    std::ifstream file(path, std::ios::binary | std::ios::ate);
    if (!file)
        return 0;

    const auto fileSize = file.tellg();
    if (fileSize <= 0 || fileSize > std::numeric_limits<int>::max())
        return 0;
    const auto encodedSize = static_cast<std::size_t>(static_cast<std::streamoff>(fileSize));
    std::vector<unsigned char> encoded(encodedSize, 0);
    file.seekg(0, std::ios::beg);
    file.read(reinterpret_cast<char *>(encoded.data()), std::streamsize(encoded.size()));
    if (!file)
        return 0;

    GLuint texture = 0;
    int w = 0, h = 0;
    unsigned char *pixels = stbi_load_from_memory(
        encoded.data(), int(encoded.size()), &w, &h, nullptr, STBI_rgb_alpha);
    if (pixels && w > 0 && h > 0 && w <= 4096 && h <= 4096)
    {
        aspect = float(w) / h;
        glGenTextures(1, &texture);
        glBindTexture(GL_TEXTURE_2D, texture);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glTexImage2D(GL_TEXTURE_2D,
                     0,
                     GL_RGBA,
                     GLsizei(w),
                     GLsizei(h),
                     0,
                     GL_RGBA,
                     GL_UNSIGNED_BYTE,
                     pixels);
    }
    else if (!pixels)
        std::fprintf(stderr, "Could not decode optional PNG: %s\n", stbi_failure_reason());
    stbi_image_free(pixels);
    return texture;
}

void Renderer::Cube(
    float x, float y, float z, float sx, float sy, float sz, float r, float g, float b)
{
    glColor3f(r, g, b);
    glPushMatrix();
    glTranslatef(x, y, z);
    glScalef(sx, sy, sz);
    RenderGeometry::SolidCube(1);
    glPopMatrix();
}

void Renderer::Line(
    float ax, float ay, float az, float bx, float by, float bz, float r, float g, float b, float a)
{
    glColor4f(r, g, b, a);
    glBegin(GL_LINES);
    glVertex3f(ax, ay, az);
    glVertex3f(bx, by, bz);
    glEnd();
}

void Renderer::Ring(float x, float y, float z, float radius, float r, float g, float b, float a)
{
    glColor4f(r, g, b, a);
    glBegin(GL_LINE_LOOP);
    for (int i = 0; i < 40; ++i)
    {
        float t = 2 * Pi * i / 40;
        glVertex3f(x + std::cos(t) * radius, y, z + std::sin(t) * radius);
    }
    glEnd();
}

void Renderer::Sprite(
    float x, float y, float z, int kind, int frame, GLuint texture, float aspect, bool mirror)
{
    bool generated = !texture;
    if (!texture)
    {
        texture = effects.Sprite(kind, frame);
        aspect = .8f;
    }
    if (!texture)
        return;
    glUseProgram(0);
    glDisable(GL_LIGHTING);
    glEnable(GL_TEXTURE_2D);
    glBindTexture(GL_TEXTURE_2D, texture);
    glEnable(GL_ALPHA_TEST);
    glAlphaFunc(GL_GREATER, shadowPass ? .35f : .01f);
    float h = 1.8f, w = h * aspect;
    if (w > 1.5f)
    {
        h *= 1.5f / w;
        w = 1.5f;
    }
    float rx = .745f * w * .5f, rz = -.667f * w * .5f;
    glColor4f(1, 1, 1, 1);
    glBegin(GL_QUADS);
    glTexCoord2f(mirror ? 1.0f : 0.0f, generated ? .96f : 1);
    glVertex3f(x - rx, y + .025f, z - rz);
    glTexCoord2f(mirror ? 0.0f : 1.0f, generated ? .96f : 1);
    glVertex3f(x + rx, y + .025f, z + rz);
    glTexCoord2f(mirror ? 0.0f : 1.0f, 0);
    glVertex3f(x + rx, y + h, z + rz);
    glTexCoord2f(mirror ? 1.0f : 0.0f, 0);
    glVertex3f(x - rx, y + h, z - rz);
    glEnd();
    glDisable(GL_ALPHA_TEST);
    glDisable(GL_TEXTURE_2D);
}

void Renderer::Panel(float x, float y, float w, float h, float r, float g, float b, float a)
{
    glColor4f(r, g, b, a);
    glBegin(GL_QUADS);
    glVertex2f(x, y);
    glVertex2f(x + w, y);
    glVertex2f(x + w, y + h);
    glVertex2f(x, y + h);
    glEnd();
}

void Renderer::Text(float x, float y, const std::string &s, float size, float r, float g, float b)
{
    effects.Text(x, y, s, size, r, g, b);
}

bool Renderer::Character(int index,
                         const std::string &clip,
                         float time,
                         float x,
                         float y,
                         float z,
                         bool mirror,
                         bool loop)
{
    if (index < 0 || index >= int(characters.size()))
        return false;
    return characters[index].Draw(
        characterTextures[index], clip, time, loop, x, y, z, mirror, shadowPass);
}

void Renderer::Camera(const RpgGame &game)
{
    glViewport(0, 0, width, height);
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    auto target = game.dollTraining.active ? WorldPoint{3, 3} : game.player;
    float cameraZoom = game.dollTraining.active ? 5.0f : zoom;
    float aspect = float(width) / height;
    glOrtho(-cameraZoom * aspect, cameraZoom * aspect, -cameraZoom, cameraZoom, .1, 100);
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
    gluLookAt(target.x + 17, 20, target.z + 19, target.x, 0, target.z, 0, 1, 0);
    glGetDoublev(GL_MODELVIEW_MATRIX, model);
    glGetDoublev(GL_PROJECTION_MATRIX, projection);
    glGetIntegerv(GL_VIEWPORT, viewport);
    cameraReady = true;
    const GLfloat light[] = {-10, 25, -8, 0};
    glLightfv(GL_LIGHT0, GL_POSITION, light);
}

WorldPoint Renderer::Pick(int x, int y) const
{
    if (!cameraReady)
        return {};
    GLdouble ax, ay, az, bx, by, bz;
    gluUnProject(x, height - y, 0, model, projection, viewport, &ax, &ay, &az);
    gluUnProject(x, height - y, 1, model, projection, viewport, &bx, &by, &bz);
    if (std::abs(by - ay) < .00001)
        return {};
    double t = -ay / (by - ay);
    return {float(ax + (bx - ax) * t), float(az + (bz - az) * t)};
}

bool Renderer::IsUi(int x, int y, const RpgGame &game) const
{
    float px = UIX(x, width), py = UIY(y, height);
    return py < 110 || py > 700 || (game.storyJournalOpen && In(px, py, 120, 110, 1040, 575)) ||
           ((game.talkingActor || game.talkingNpc >= 0) && In(px, py, 745, 185, 520, 470)) ||
           (game.worldDebug && In(px, py, 15, 180, 710, 495)) ||
           (game.inventory && In(px, py, 950, 120, 310, 480));
}

int Renderer::ViewRadius() const
{
    // Cover the ground footprint at the current zoom and window aspect ratio.
    return int(std::ceil(zoom * (float(width) / height + 1.8f))) + 5;
}

void Renderer::Environment(const RpgGame &game)
{
    if (resources.Ready())
    {
        if (!shadowPass)
            effects.Lit(model, game.time, 8);
        resources.Environment(game, ViewRadius(), shadowPass);
        glUseProgram(0);
        return;
    }

    if (game.dollTraining.active)
    {
        if (!shadowPass)
        {
            effects.Lit(model, game.dollTraining.report.elapsed, 5);
            Cube(3, -.12f, 3, 7, .2f, 7, .24f, .3f, .27f);
            effects.Material(4);
            Cube(1, .25f, .6f, 1, .5f, .25f, .3f, .4f, .42f);
            glUseProgram(0);
        }
        return;
    }
    int centerX = int(std::floor(game.player.x)), centerZ = int(std::floor(game.player.z));
    int radius = ViewRadius();
    if (!shadowPass)
    {
        effects.Lit(model, game.time, 5);
        glColor3f(.29f, .35f, .23f);
        // One continuous ground surface, with procedural material in world coordinates.
        glBegin(GL_QUADS);
        glNormal3f(0, 1, 0);
        glVertex3f(float(centerX - radius) - .5f, 0, float(centerZ - radius) - .5f);
        glVertex3f(float(centerX - radius) - .5f, 0, float(centerZ + radius) + .5f);
        glVertex3f(float(centerX + radius) + .5f, 0, float(centerZ + radius) + .5f);
        glVertex3f(float(centerX + radius) + .5f, 0, float(centerZ - radius) - .5f);
        glEnd();
    }
    for (int z = centerZ - radius; z <= centerZ + radius; ++z)
        for (int x = centerX - radius; x <= centerX + radius; ++x)
        {
            auto terrain = game.Terrain(x, z);
            if (terrain == RpgGame::Ground::Dirt)
                continue;
            float variation = .5f + .5f * std::sin(float(x) * 1.73f + float(z) * 2.31f);
            if (!shadowPass)
                effects.Material(6);
            glPushMatrix();
            glTranslatef(float(x), 0, float(z));
            if (terrain == RpgGame::Ground::Tree)
            {
                // Vertical trunk and layered low-poly crown replace wall cubes.
                glColor3f(.24f, .15f, .08f);
                glPushMatrix();
                glRotatef(-90, 1, 0, 0);
                RenderGeometry::SolidCylinder(.16f, .95f, 8, 1);
                glPopMatrix();
                for (int layer = 0; layer < 3; ++layer)
                {
                    glPushMatrix();
                    glTranslatef(0, .65f + layer * .5f, 0);
                    glRotatef(-90, 1, 0, 0);
                    glColor3f(
                        .10f + variation * .035f, .24f + layer * .025f + variation * .06f, .12f);
                    RenderGeometry::SolidCone(
                        .72f - layer * .14f, 1.1f + variation * .25f, 9, 1);
                    glPopMatrix();
                }
            }
            else
            {
                glColor3f(.34f + variation * .1f, .36f + variation * .08f, .32f + variation * .06f);
                glTranslatef(0, .28f, 0);
                glRotatef(variation * 180, 0, 1, 0);
                glScalef(.42f, .4f + variation * .22f, .42f);
                RenderGeometry::SolidIcosahedron();
            }
            glPopMatrix();
        }
    for (auto p : game.fires)
    {
        if (std::abs(p.x - game.player.x) > radius || std::abs(p.z - game.player.z) > radius)
            continue;
        if (!shadowPass)
            effects.Material(4);
        Cube(p.x, .16f, p.z, .5f, .32f, .5f, .25f, .17f, .1f);
    }
    TownBuildings(game);
    glUseProgram(0);
}

void Renderer::Actors(const RpgGame &game)
{
    if (resources.Ready())
    {
        if (!shadowPass)
            effects.Lit(model, game.time, 8);
        resources.Actors(game, ViewRadius());
        glUseProgram(0);
        if (shadowPass)
            return;
    }

    glUseProgram(0);
    glDisable(GL_LIGHTING);
    glDisable(GL_FOG);
    glActiveTexture(GL_TEXTURE0);
    std::vector<int> order{-1};
    const float radius = float(ViewRadius() + 2);
    for (int i = 0; i < int(game.npcs.size()); ++i)
        if (std::abs(game.npcs[i].position.x - game.player.x) < radius &&
            std::abs(game.npcs[i].position.z - game.player.z) < radius)
            order.push_back(-2 - i);
    for (int i = 0; i < int(game.enemies.size()); ++i)
        if (game.enemies[i].hp > 0 &&
            std::abs(game.enemies[i].position.x - game.player.x) < radius &&
            std::abs(game.enemies[i].position.z - game.player.z) < radius)
            order.push_back(i);
    auto position = [&](int index)
    {
        if (index < -1)
        {
            const auto &p = game.npcs[-2 - index].position;
            return WorldPoint{p.x, p.z};
        }
        return index == -1 ? game.player : game.enemies[index].position;
    };
    std::sort(order.begin(),
              order.end(),
              [&](int a, int b)
              {
                  auto pa = position(a), pb = position(b);
                  return pa.x * 17 + pa.z * 19 < pb.x * 17 + pb.z * 19;
              });
    for (int index : order)
    {
        auto point = position(index);
        bool moving = false, mirror = false;
        int kind = 0;
        float health = float(game.hp) / game.MaxHp();
        if (index < -1)
        {
            const auto &npc = game.npcs[-2 - index];
            kind = 1 + (int(npc.role) % 2);
            moving = npc.moving;
            mirror = npc.mirror;
            health = npc.hp / 100;
        }
        else if (index >= 0)
        {
            kind = 3;
            moving = game.enemies[index].moving;
            mirror = game.enemies[index].mirror;
            health = float(game.enemies[index].hp) / game.enemies[index].maxHp;
        }
        else
        {
            moving = game.moving;
            mirror = game.aim.x < game.player.x;
        }
        if (!resources.Ready())
            Sprite(point.x,
                   health <= 0 ? -.35f : 0,
                   point.z,
                   kind,
                   moving ? int(game.time * 8) % 8 : 0,
                   0,
                   .8f,
                   mirror);
        if (!shadowPass)
        {
            Ring(point.x,
                 .035f,
                 point.z,
                 .32f,
                 index >= 0 ? 1.0f : .4f,
                 index >= 0 ? .35f : .8f,
                 .35f);
            Line(point.x - .3f,
                 1.75f,
                 point.z + .27f,
                 point.x + .3f,
                 1.75f,
                 point.z - .27f,
                 .12f,
                 .12f,
                 .12f);
            Line(point.x - .3f,
                 1.76f,
                 point.z + .27f,
                 point.x - .3f + .6f * health,
                 1.76f,
                 point.z + .27f - .54f * health,
                 index >= 0 ? 1.0f : .3f,
                 .65f,
                 .3f);
        }
    }
    if (shadowPass)
        return;
    for (const auto &drop : game.drops)
    {
        auto p = drop.position;
        if (std::hypot(p.x - game.player.x, p.z - game.player.z) > ViewRadius())
            continue;
        if (!resources.Ready())
            Cube(p.x,
                 .22f,
                 p.z,
                 .22f,
                 .30f,
                 .22f,
                 drop.potion ? .25f : .9f,
                 drop.potion ? .9f : .65f,
                 .3f);
        Ring(p.x, .04f, p.z, .3f, .8f, .8f, .3f);
    }
    glDepthMask(GL_FALSE);
    Ring(game.aim.x, .04f, game.aim.z, .22f, 1, .8f, .3f);
    for (const auto &shot : game.shots)
        Line(shot.from.x, .9f, shot.from.z, shot.to.x, .9f, shot.to.z, 1, .8f, .3f);
    // Shader-generated fire on camera-facing geometry. No animated texture sheets.
    for (auto p : game.fires)
    {
        effects.Fire(game.time);
        glBegin(GL_QUADS);
        glTexCoord2f(0, 0);
        glVertex3f(p.x - .45f * .745f, .3f, p.z + .45f * .667f);
        glTexCoord2f(1, 0);
        glVertex3f(p.x + .45f * .745f, .3f, p.z - .45f * .667f);
        glTexCoord2f(1, 1);
        glVertex3f(p.x + .45f * .745f, 1.6f, p.z - .45f * .667f);
        glTexCoord2f(0, 1);
        glVertex3f(p.x - .45f * .745f, 1.6f, p.z + .45f * .667f);
        glEnd();
    }
    glUseProgram(0);
    glDepthMask(GL_TRUE);
}

void Renderer::Hud(const RpgGame &game)
{
    const auto &state = game.expeditionState;
    const auto &needs = state.needs;
    Panel(0, 0, 1280, 110, .035f, .065f, .08f, .96f);
    Text(24, 12, "왕국 교역로 / 수색과 귀환", 21, .95f, .87f, .67f);
    Text(24, 44, state.clock.Label(), 14);
    Text(24,
         76,
         "허기 " + std::to_string(int(needs.hunger)) + " · 갈증 " +
             std::to_string(int(needs.thirst)) + " · 피로 " + std::to_string(int(needs.fatigue)),
         13);
    Text(420, 12, "체력 " + std::to_string(game.hp) + " / 100", 20);
    Panel(420, 44, 360, 12, .15f, .19f, .19f);
    Panel(420, 44, 360 * float(game.hp) / 100, 12, .28f, .76f, .48f);
    Text(420,
         65,
         "기력 " + std::to_string(int(needs.stamina)) + " / " +
             std::to_string(int(needs.MaxStamina(game.hp))),
         13);
    Panel(420, 89, 360, 8, .15f, .19f, .19f);
    Panel(420, 89, 360 * needs.stamina / 100, 8, .3f, .62f, .9f);
    Text(850, 12, "지역 경보 " + std::to_string(game.CurrentAlert()) + " / 4", 20);
    Text(850,
         45,
         "무기 숙련 " + std::to_string(state.progress.WeaponSkill()) + " · " +
             (state.progress.Veteran() ? "노련한 전투자" : "신참 모험가"),
         14);
    Text(850,
         77,
         "식량 " + std::to_string(state.rations) + " · 식수 " + std::to_string(state.water) +
             " · 은화 " + std::to_string(game.silver),
         13);
    Panel(24, 120, 710, 107, .04f, .08f, .09f, .92f);
    Text(38, 128, "첫 의뢰 | 실종 사냥꾼 수색", 18, .95f, .85f, .55f);
    Text(38, 157, game.QuestObjective(), 15, .9f, .88f, .75f);
    Text(38, 188, game.QuestHint(), 12);
    if (state.resting)
        Text(440, 247, "휴식 중 · R 또는 이동으로 중단", 20, .5f, .9f, .7f);
    if (game.inventory)
    {
        Panel(950, 120, 310, 480, .04f, .07f, .09f, .96f);
        Text(975, 138, "탐사 보급품 [I]", 22);
        Text(975, 181, "회복약: " + std::to_string(game.potions) + " [Q]", 17);
        Text(975, 220, "식량: " + std::to_string(state.rations) + " / 6 [B]", 17);
        Text(975, 259, "식수: " + std::to_string(state.water) + " / 6 [T]", 17);
        Text(975, 298, "은화: " + std::to_string(game.silver), 17);
        Text(975,
             337,
             "공격 " + std::to_string(game.AttackPower()) + " / 방어 " +
                 std::to_string(game.Defense()),
             16);
        Text(975,
             376,
             "실적: 전투 " + std::to_string(state.progress.combatWins) + " / 구조 " +
                 std::to_string(state.progress.rescues),
             14);
        Text(975, 417, "허기·갈증·피로가 기력에 영향", 13);
        Text(975, 451, "상인: 4 식량 / 5 식수 (각 은화 1)", 12);
        Text(975, 485, "마을 휴식 R: 피로·기력 회복", 13);
        Text(975, 520, "일과·이동 중에는 상점 영업 중단", 12);
        Text(975, 558, "실시간 진행 · Esc로 일시정지", 12);
    }
    Panel(0, 704, 1280, 96, .035f, .065f, .08f, .96f);
    Text(24,
         716,
         "WASD 이동 · 조준/좌클릭 공격 · G 달리기 · C 은신 · E 조사/호위/줍기 · F 대화 · L 일지",
         14);
    Text(24, 744, "Q 회복약 · B 식량 · T 식수 · R 마을 휴식 · I 보급품 · Esc 정지", 14);
    Text(24,
         774,
         game.message.empty() ? "F → 3: 단서/보고 · 소문은 주민의 실제 이동과 대면으로 전달됩니다."
                              : game.message,
         12,
         .9f,
         .79f,
         .51f);
    TownHud(game);
    WorldHud(game);
    StoryHud(game);
    if (game.paused || game.hp <= 0)
    {
        Panel(0, 0, 1280, 800, .01f, .025f, .035f, .65f);
        Panel(330, 270, 620, 250, .06f, .11f, .13f, .98f);
        Text(365, 300, game.hp <= 0 ? "탐사 실패" : "일시정지", 28);
        Text(365, 365, "Enter: 같은 시드에서 재시작 / N: 새 시드", 18);
        Text(365, 411, "재시작하면 보급품·숙련·의뢰가 초기화됩니다.", 16);
        Text(365, 460, "Esc: 계속", 16);
    }
}

void Renderer::Draw(const RpgGame &game, int, int)
{
    resources.BeginFrame();
    glUseProgram(0);
    glActiveTexture(GL_TEXTURE0);
    glDisable(GL_TEXTURE_2D);
    glDepthMask(GL_TRUE);
    auto focus = game.dollTraining.active ? WorldPoint{3, 3} : game.player;
    if (effects.BeginShadow(
            focus.x, focus.z, game.dollTraining.active ? 8.0f : float(ViewRadius() + 4)))
    {
        shadowPass = true;
        Environment(game);
        Actors(game);
        shadowPass = false;
    }
    effects.BeginScene();
    glEnable(GL_DEPTH_TEST);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    Camera(game);
    Environment(game);
    ContactShadows(game);
    Actors(game);
    effects.Composite();
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_LIGHTING);
    glDisable(GL_TEXTURE_2D);
    glDisable(GL_FOG);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    glOrtho(0, 1280, 800, 0, -1, 1);
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
    Hud(game);
    resources.Profile(game.time);
}

void Renderer::ContactShadows(const RpgGame &game)
{
    if (!effects.BeginContactShadows())
        return;
    glDisable(GL_LIGHTING);
    glDisable(GL_TEXTURE_2D);
    glDisable(GL_FOG);
    glDepthMask(GL_FALSE);
    auto focus = game.dollTraining.active ? WorldPoint{3, 3} : game.player;
    glBegin(GL_QUADS);
    auto emit = [&](WorldPoint p)
    {
        if (std::abs(p.x - focus.x) > ViewRadius() || std::abs(p.z - focus.z) > ViewRadius())
            return;
        // Just above the plaza/road surface, centered at the same world-space foot anchor.
        glTexCoord2f(0, 0);
        glVertex3f(p.x - .34f, .028f, p.z - .24f);
        glTexCoord2f(0, 1);
        glVertex3f(p.x - .34f, .028f, p.z + .24f);
        glTexCoord2f(1, 1);
        glVertex3f(p.x + .34f, .028f, p.z + .24f);
        glTexCoord2f(1, 0);
        glVertex3f(p.x + .34f, .028f, p.z - .24f);
    };
    if (game.dollTraining.active)
    {
        for (const auto &doll : game.dollTraining.dolls)
            emit({doll.position.x, doll.position.z});
    }
    else
    {
        emit(game.player);
        for (const auto &npc : game.npcs)
            emit({npc.position.x, npc.position.z});
        for (const auto &enemy : game.enemies)
            if (enemy.hp > 0)
                emit(enemy.position);
        for (const auto &entry : game.world.Actors())
            if (entry.second.available)
                emit({entry.second.position.x, entry.second.position.z});
    }
    glEnd();
    glDepthMask(GL_TRUE);
    glUseProgram(0);
}
