#pragma once
#include "RpgGame.h"
#include "RenderEffects.h"
#include "CharacterAnimation.h"
#include "Dependencies/glew.h"
#include <array>
#include <string>

class Renderer
{
  public:
    Renderer(int width, int height);
    ~Renderer();
    Renderer(const Renderer &) = delete;
    Renderer &operator=(const Renderer &) = delete;
    void Resize(int width, int height);
    void Draw(const RpgGame &game, int mouseX, int mouseY);
    WorldPoint Pick(int x, int y) const;
    bool IsUi(int x, int y, const RpgGame &game) const;
    void Shutdown();
    void Zoom(int direction);

  private:
    int width = 1280, height = 800;
    GLdouble model[16]{}, projection[16]{};
    GLint viewport[4]{};
    bool cameraReady = false, shadowPass = false;
    float zoom = 9;
    RenderEffects effects;
    std::array<CharacterAnimation, 5> characters;
    std::array<GLuint, 5> characterTextures{};
    std::array<GLuint, 3> portraits{};
    std::array<float, 3> portraitAspects{{1, 1, 1}};
    void Camera(const RpgGame &game);
    void Environment(const RpgGame &game);
    int ViewRadius() const;
    void Actors(const RpgGame &game);
    void Hud(const RpgGame &game);
    bool Character(int index,
                   const std::string &clip,
                   float time,
                   float x,
                   float y,
                   float z,
                   bool mirror = false,
                   bool loop = true);
    void Sprite(float x,
                float y,
                float z,
                int kind,
                int frame,
                GLuint texture = 0,
                float aspect = .8f,
                bool mirror = false);
    void Cube(float x, float y, float z, float sx, float sy, float sz, float r, float g, float b);
    void Line(float ax,
              float ay,
              float az,
              float bx,
              float by,
              float bz,
              float r,
              float g,
              float b,
              float alpha = 1);
    void Ring(float x, float y, float z, float radius, float r, float g, float b, float alpha = 1);
    void Panel(float x, float y, float w, float h, float r, float g, float b, float alpha = 1);
    void Text(float x,
              float y,
              const std::string &text,
              float size = 14,
              float r = .85f,
              float g = .89f,
              float b = .9f);
    GLuint LoadPng(const std::wstring &path, float &aspect);
};
