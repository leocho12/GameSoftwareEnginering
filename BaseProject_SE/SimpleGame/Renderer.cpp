#include "stdafx.h"
#include "Renderer.h"
#include "Dependencies/freeglut.h"
#include <Windows.h>
#include <wincodec.h>
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <vector>
#pragma comment(lib, "windowscodecs.lib")
#pragma comment(lib, "ole32.lib")

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

template <class T> void Release(T *&p)
{
    if (p)
    {
        p->Release();
        p = nullptr;
    }
}
}

Renderer::Renderer(int w, int h)
{
    effects.Initialize();
    Resize(w, h);
    wchar_t path[MAX_PATH]{};
    GetModuleFileNameW(nullptr, path, MAX_PATH);
    std::wstring base(path);
    base = base.substr(0, base.find_last_of(L"\\/") + 1) + L"Assets\\";
    HRESULT hr = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    portraits[0] = LoadPng(base + L"alpha.png", portraitAspects[0]);
    portraits[1] = LoadPng(base + L"bravo.png", portraitAspects[1]);
    portraits[2] = LoadPng(base + L"enemy.png", portraitAspects[2]);
    const wchar_t *names[] = {
        L"4type", L"m4a1", L"ump45", L"hk416", L"g11", L"guard", L"ripper", L"jaeger"};
    for (int i = 0; i < int(characters.size()); ++i)
    {
        std::wstring stem = base + L"GFL\\" + names[i];
        float aspect = 1;
        if (characters[i].Load(stem + L".gfa"))
            characterTextures[i] = LoadPng(stem + L".png", aspect);
        if (!characterTextures[i])
        {
            characters[i].Clear();
            std::fprintf(stderr, "GFL character %d unavailable; using placeholder.\n", i);
        }
    }
    if (SUCCEEDED(hr))
        CoUninitialize();
}

Renderer::~Renderer()
{
    Shutdown();
}

void Renderer::Shutdown()
{
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

GLuint Renderer::LoadPng(const std::wstring &path, float &aspect)
{
    if (GetFileAttributesW(path.c_str()) == INVALID_FILE_ATTRIBUTES)
        return 0;
    IWICImagingFactory *factory = nullptr;
    IWICBitmapDecoder *decoder = nullptr;
    IWICBitmapFrameDecode *frame = nullptr;
    IWICFormatConverter *converter = nullptr;
    GLuint texture = 0;
    HRESULT hr = CoCreateInstance(
        CLSID_WICImagingFactory, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&factory));
    if (SUCCEEDED(hr))
        hr = factory->CreateDecoderFromFilename(
            path.c_str(), nullptr, GENERIC_READ, WICDecodeMetadataCacheOnLoad, &decoder);
    if (SUCCEEDED(hr))
        hr = decoder->GetFrame(0, &frame);
    if (SUCCEEDED(hr))
        hr = factory->CreateFormatConverter(&converter);
    if (SUCCEEDED(hr))
        hr = converter->Initialize(frame,
                                   GUID_WICPixelFormat32bppRGBA,
                                   WICBitmapDitherTypeNone,
                                   nullptr,
                                   0,
                                   WICBitmapPaletteTypeCustom);
    UINT w = 0, h = 0;
    if (SUCCEEDED(hr))
        hr = converter->GetSize(&w, &h);
    if (SUCCEEDED(hr) && w > 0 && h > 0 && w <= 4096 && h <= 4096)
    {
        std::vector<unsigned char> pixels(size_t(w) * h * 4);
        hr = converter->CopyPixels(nullptr, w * 4, UINT(pixels.size()), pixels.data());
        if (SUCCEEDED(hr))
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
                         pixels.data());
        }
    }
    if (FAILED(hr))
        std::fprintf(stderr,
                     "Could not decode optional PNG (HRESULT %lx).\n",
                     static_cast<unsigned long>(hr));
    Release(converter);
    Release(frame);
    Release(decoder);
    Release(factory);
    return texture;
}

void Renderer::Cube(
    float x, float y, float z, float sx, float sy, float sz, float r, float g, float b)
{
    glColor3f(r, g, b);
    glPushMatrix();
    glTranslatef(x, y, z);
    glScalef(sx, sy, sz);
    glutSolidCube(1);
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
    glTexCoord2f(mirror ? 1.0f : 0.0f, 1);
    glVertex3f(x - rx, y + .025f, z - rz);
    glTexCoord2f(mirror ? 0.0f : 1.0f, 1);
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
    return py < 100 || py > 700 || (game.storyJournalOpen && In(px, py, 120, 110, 1040, 575)) ||
           ((game.talkingActor || game.talkingNpc >= 0) && In(px, py, 745, 185, 520, 470)) ||
           (game.worldDebug && In(px, py, 15, 180, 710, 495)) ||
           (game.inventory && In(px, py, 950, 115, 310, 420));
}

int Renderer::ViewRadius() const
{
    // Cover the ground footprint at the current zoom and window aspect ratio.
    return int(std::ceil(zoom * (float(width) / height + 1.8f))) + 5;
}

void Renderer::Environment(const RpgGame &game)
{
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
                glutSolidCylinder(.16, .95, 8, 1);
                glPopMatrix();
                for (int layer = 0; layer < 3; ++layer)
                {
                    glPushMatrix();
                    glTranslatef(0, .65f + layer * .5f, 0);
                    glRotatef(-90, 1, 0, 0);
                    glColor3f(
                        .10f + variation * .035f, .24f + layer * .025f + variation * .06f, .12f);
                    glutSolidCone(.72f - layer * .14f, 1.1f + variation * .25f, 9, 1);
                    glPopMatrix();
                }
            }
            else
            {
                glColor3f(.34f + variation * .1f, .36f + variation * .08f, .32f + variation * .06f);
                glTranslatef(0, .28f, 0);
                glRotatef(variation * 180, 0, 1, 0);
                glScalef(.42f, .4f + variation * .22f, .42f);
                glutSolidIcosahedron();
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
    if (game.dollTraining.active)
    {
        DollActors(game.dollTraining);
        return;
    }
    glUseProgram(0);
    glDisable(GL_LIGHTING);
    glDisable(GL_FOG);
    glActiveTexture(GL_TEXTURE0);
    // Draw far-to-near so transparent borders blend over the objects behind them.
    std::vector<int> order;
    order.push_back(-1);
    for (int i = 0; i < int(game.npcs.size()); ++i)
        if (std::abs(game.npcs[i].position.x - game.player.x) < ViewRadius() &&
            std::abs(game.npcs[i].position.z - game.player.z) < ViewRadius())
            order.push_back(-2 - i);
    for (int i = 0; i < int(game.enemies.size()); ++i)
        if (game.enemies[i].hp > 0)
            order.push_back(i);
    std::vector<narrative::ActorId> worldOrder;
    for (const auto &entry : game.world.Actors())
    {
        const auto &actor = entry.second;
        if (!actor.available || std::abs(actor.position.x - game.player.x) > ViewRadius() ||
            std::abs(actor.position.z - game.player.z) > ViewRadius())
            continue;
        worldOrder.push_back(entry.first);
        order.push_back(-1000 - int(worldOrder.size()) + 1);
    }
    auto position = [&](int index)
    {
        if (index <= -1000)
        {
            auto p = game.world.Find(worldOrder[-1000 - index])->position;
            return WorldPoint{p.x, p.z};
        }
        if (index < -1)
        {
            auto p = game.npcs[-2 - index].position;
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
        auto p = position(index);
        if (index <= -1000)
        {
            const auto &actor = *game.world.Find(worldOrder[-1000 - index]);
            int appearance = 1 + int(actor.id % 4);
            if (!Character(appearance,
                           actor.moving ? "move" : "wait",
                           game.time,
                           p.x,
                           0,
                           p.z,
                           game.worldBodies.at(actor.id).mirror))
                Sprite(p.x, 0, p.z, 1, actor.moving ? int(game.time * 8) % 8 : 0);
            if (!shadowPass)
                Ring(p.x, .04f, p.z, .4f, .3f, .85f, 1);
        }
        else if (index < -1)
        {
            const auto &npc = game.npcs[-2 - index];
            // Shared original SD presentation; these are still separately authored NPCs.
            int appearance = 1 + npc.id % 4;
            bool attacking = npc.hp > 0 && game.time - npc.attackStarted < .35f;
            const char *clip = attacking ? "attack" : npc.moving ? "move" : "wait";
            float clock = attacking ? game.time - npc.attackStarted : game.time + npc.id * .13f;
            if (!Character(appearance,
                           clip,
                           clock,
                           p.x,
                           npc.hp <= 0 ? -.35f : 0,
                           p.z,
                           npc.mirror,
                           !attacking))
                Sprite(p.x,
                       npc.hp <= 0 ? -.35f : 0,
                       p.z,
                       int(npc.role) % 2,
                       npc.moving ? int(game.time * 8) % 8 : 0);
            if (!shadowPass)
                Ring(p.x,
                     .04f,
                     p.z,
                     .32f,
                     npc.hostile ? 1 : .3f,
                     npc.role == NpcRole::Merchant ? .9f : .5f,
                     npc.role == NpcRole::Medic ? .9f : .3f);
        }
        else if (index == -1)
        {
            bool attack = game.time - game.attackStarted < .35f;
            std::string clip = game.hp <= 0  ? "die"
                               : attack      ? "attack"
                               : game.moving ? "move"
                                             : "wait";
            float clock = attack ? game.time - game.attackStarted : game.time;
            // The original SD is a temporary visual proxy for the human commander.
            if (!Character(0,
                           clip,
                           clock,
                           p.x,
                           0,
                           p.z,
                           game.aim.x < game.player.x,
                           game.hp > 0 && !attack))
                Sprite(p.x, 0, p.z, 0, game.moving ? int(game.time * 10) % 8 : 0);
        }
        else
        {
            const auto &enemy = game.enemies[index];
            bool attacking = game.time - enemy.attackStarted < .35f;
            const char *clip = attacking ? "attack" : enemy.moving ? "move" : "wait";
            if (!Character(enemy.appearance,
                           clip,
                           attacking ? game.time - enemy.attackStarted : game.time + index * .11f,
                           p.x,
                           0,
                           p.z,
                           enemy.mirror,
                           !attacking))
                Sprite(p.x, 0, p.z, 2, int(game.time * 8) % 8);
            if (!shadowPass)
                Ring(p.x,
                     .04f,
                     p.z,
                     .35f,
                     1,
                     enemy.action == BanditAction::Attack ? .2f : .65f,
                     .15f);
        }
        if (!shadowPass)
        {
            float ratio = index <= -1000 ? game.world.Find(worldOrder[-1000 - index])->hp / 100
                          : index < -1   ? game.npcs[-2 - index].hp / 100
                          : index == -1 ? float(game.hp) / game.MaxHp()
                                        : float(game.enemies[index].hp) / game.enemies[index].maxHp;
            Line(p.x - .3f, 1.95f, p.z + .27f, p.x + .3f, 1.95f, p.z - .27f, .12f, .12f, .12f);
            Line(p.x - .3f,
                 1.96f,
                 p.z + .27f,
                 p.x - .3f + .6f * ratio,
                 1.96f,
                 p.z + .27f - .54f * ratio,
                 index < 0 ? .3f : 1,
                 .5f,
                 .3f);
        }
    }
    if (shadowPass)
        return;
    if (game.world.Ready() && game.world.Event().playerAccepted &&
        game.world.Event().state == narrative::EventState::Active)
    {
        auto target = game.world.Settings().search;
        Ring(target.x, .06f, target.z, .8f, .3f, 1, .6f);
        if (!Character(1, "wait", game.time, target.x, -.35f, target.z))
            Sprite(target.x, .0f, target.z, 1, 0);
    }
    for (const auto &drop : game.drops)
    {
        auto p = drop.position;
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
    if (game.dollTraining.active)
    {
        DollHud(game.dollTraining, game.paused);
        return;
    }
    Panel(0, 0, 1280, 98, .035f, .065f, .08f, .96f);
    Text(24, 16, "변경 마을 / 주변 사냥터", 24, .95f, .87f, .67f);
    Text(24, 53, "인간 지휘관 · 4식 SD 시험 외형", 13, .65f, .76f, .74f);
    Text(420,
         15,
         "레벨 " + std::to_string(game.level) + "  |  체력 " + std::to_string(game.hp) + " / " +
             std::to_string(game.MaxHp()),
         20);
    Panel(420, 47, 360, 13, .15f, .19f, .19f);
    Panel(420, 47, 360 * float(game.hp) / game.MaxHp(), 13, .28f, .76f, .48f);
    Text(420,
         68,
         "경험치 " + std::to_string(game.experience) + " / " +
             std::to_string(game.NextExperience()),
         14);
    Text(850,
         16,
         "공격 " + std::to_string(game.AttackPower()) + "  방어 " + std::to_string(game.Defense()),
         19);
    Text(850,
         48,
         "처치 " + std::to_string(game.kills) + "  누적 경험치 " +
             std::to_string(game.totalExperience),
         14);
    Panel(24, 115, 710, 115, .04f, .08f, .09f, .92f);
    Text(38, 126, "첫 레벨 목표 | 외곽 순찰", 18, .95f, .85f, .55f);
    Text(38, 154, game.PatrolObjective(), 16, .9f, .88f, .75f);
    Text(38, 180, game.PatrolHint(), 12);
    Text(38,
         204,
         "진행: 레벨 " + std::to_string(game.level) + "/3 · 전리품 " +
             std::to_string(std::min(game.itemsCollected, 2)) + "/2 · 보고 " +
             (game.patrolReported ? "완료" : "대기") + " · I: 능력치 확인",
         12);
    if (game.inventory)
    {
        Panel(950, 115, 310, 420, .04f, .07f, .09f, .96f);
        Text(975, 139, "가방 / 능력치 [I]", 22);
        Text(975, 192, "회복약: " + std::to_string(game.potions), 18);
        Text(975, 227, "부품 (교환 재화): " + std::to_string(game.scrap), 18);
        Text(975, 279, "레벨업 자동 성장", 18, .9f, .77f, .45f);
        Text(975, 315, "최대 체력 +20", 16);
        Text(975, 346, "공격력 +3 / 방어력 +1", 16);
        Text(975, 390, "회복약은 최대 체력 50% 회복", 13);
        Text(975, 422, "회복약 구매 3 · 판매 2 · 치료 2", 12);
        Text(975, 450, "처치 전리품·임무 보상으로 부품 확보", 12);
    }
    Panel(0, 704, 1280, 96, .035f, .065f, .08f, .96f);
    Text(
        24,
        717,
        "WASD 이동 · 마우스 조준 · 좌클릭/Space 공격 · E 줍기 · Q 회복약 · I 가방 · F 대화 · L 일지 · Esc 정지",
        16);
    Text(24,
         751,
         game.message.empty() ? "F6: Doll AI 구조 훈련 · 휠 확대/축소 · F11 전체화면"
                              : game.message,
         15,
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
        Text(365, 300, game.hp <= 0 ? "전투 불능" : "일시정지", 28);
        Text(365,
             365,
             game.hp <= 0 ? "Enter: 같은 맵에서 처음부터 재시작"
                          : "Esc: 계속 / Enter: 같은 맵 처음부터",
             18);
        Text(365, 411, "N: 새 랜덤 맵 (경험치와 아이템 초기화)", 16);
    }
}

void Renderer::Draw(const RpgGame &game, int, int)
{
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
