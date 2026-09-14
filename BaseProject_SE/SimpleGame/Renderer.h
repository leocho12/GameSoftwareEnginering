#pragma once
#include "TutorialGame.h"
#include "RenderEffects.h"
#include "CharacterAnimation.h"
#include "Dependencies/glew.h"
#include <array>
#include <string>

enum class UiCommand { None, Start, Skill, Confirm, Wait, Pause, Restart, Retry, Explore, Unit0, Unit1, Unit2, Unit3, Rotate, Retreat, Speed, DefenseDemo };
class Renderer {
public:
    Renderer(int width,int height);
    ~Renderer();
    Renderer(const Renderer&)=delete;
    Renderer& operator=(const Renderer&)=delete;
    void Resize(int width,int height);
    void Draw(const TutorialGame& game,int mouseX,int mouseY);
    Cell Pick(int x,int y,const TutorialGame& game) const;
    UiCommand HitUi(int x,int y,const TutorialGame& game) const;
    bool IsUi(int x,int y) const;
    void Shutdown();
    void Zoom(int direction);
    void Pan(float x,float z);
    void Recenter(){manualCamera=false;}
private:
    int width=1280,height=800;
    GLdouble model[16]{},projection[16]{};
    GLint viewport[4]{};
    bool cameraReady=false,combatView=false;
    bool shadowPass=false,manualCamera=false;
    float cameraX=6.5f,cameraZ=5.5f,zoom=6.0f;
    RenderEffects effects;
    std::array<CharacterAnimation,5> characters;
    std::array<GLuint,5> characterTextures{};
    bool Character(int index,const std::string& clip,float time,float x,float y,float z,bool mirror=false,bool loop=true);
    std::array<GLuint,3> portraits{{0,0,0}};
    std::array<float,3> portraitAspects{{1,1,1}};
    void Camera(const TutorialGame& game);
    void Sprite(float x,float y,float z,int kind,int frame,GLuint texture=0,float aspect=.8f,bool mirror=false);
    void Extras(const TutorialGame& game);
    void Environment(const TutorialGame& game);
    void Actors(const TutorialGame& game);
    void Hud(const TutorialGame& game,int mx,int my);
    void DefenseScene(const TutorialGame& game);
    void DefenseActors(const TutorialGame& game);
    void DefenseHud(const TutorialGame& game,int mx,int my);
    void Overlay(const TutorialGame& game);
    void Cube(float x,float y,float z,float sx,float sy,float sz,float r,float g,float b);
    void Line(float ax,float ay,float az,float bx,float by,float bz,float r,float g,float b,float alpha=1);
    void Tile(Cell c,float y,float r,float g,float b,float alpha,bool outline=false);
    void Ring(float x,float y,float z,float radius,float r,float g,float b,float alpha=1);
    void Panel(float x,float y,float w,float h,float r,float g,float b,float alpha=1);
    void Text(float x,float y,const std::string& text,float size=14,float r=.85f,float g=.89f,float b=.9f);
    void Button(float x,float y,float w,const std::string& label,bool enabled,bool selected,int mx,int my);
    GLuint LoadPng(const std::wstring& path,float& aspect);
};

