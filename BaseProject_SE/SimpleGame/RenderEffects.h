#pragma once
#include "Dependencies/glew.h"
#include <array>
#include <map>
#include <string>

// Compatibility-profile shader passes, Korean UI text and temporary sprite sheets.
class RenderEffects {
public:
    void Initialize();
    void Resize(int w,int h);
    void Shutdown();
    bool BeginShadow();
    void BeginScene();
    void Lit(const GLdouble* view,float time,int material=0);
    void Material(int material);
    void Composite();
    void Text(float x,float y,const std::string& utf8,float size,float r,float g,float b);
    float TextWidth(const std::string& utf8,float size);
    GLuint Sprite(int kind,int frame) const { return sprites[kind%4][frame%8]; }
    bool Ready() const { return sceneFbo!=0 && postProgram!=0; }
    std::string status;
private:
    struct Label { GLuint texture=0; int w=0,h=0; };
    std::map<std::string,Label> labels;
    std::array<std::array<GLuint,8>,4> sprites{};
    GLuint sceneFbo=0,sceneColor=0,sceneDepth=0,shadowFbo=0,shadowDepth=0;
    GLuint litProgram=0,postProgram=0;
    int width=0,height=0;
    GLdouble lightMatrix[16]{};
    Label& MakeLabel(const std::string& text,int pixels=64);
    void CreateSprites();
    void FreeScene();
};
