#include "stdafx.h"
#include "CharacterAnimation.h"
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <limits>
#include <utility>

bool CharacterAnimation::Load(const std::wstring& path){
    Clear();
    std::ifstream file(std::filesystem::path(path),std::ios::binary|std::ios::ate);
    if(!file)return false;
    const auto length=file.tellg();
    if(length<8||length>128*1024*1024)return false;
    file.seekg(0);
    size_t remaining=static_cast<size_t>(length);
    auto read=[&](void* data,size_t size){
        if(size>remaining)return false;
        if(!file.read(static_cast<char*>(data),static_cast<std::streamsize>(size)))return false;
        remaining-=size;return true;
    };
    char magic[4];uint32_t count=0;
    if(!read(magic,4)||std::memcmp(magic,"GFA1",4)||!read(&count,4)||count==0||count>32)return false;
    std::map<std::string,Clip> loaded;
    static_assert(sizeof(Vertex)==32,"GFA1 vertex layout mismatch");
    for(uint32_t n=0;n<count;++n){
        char name[32];Clip clip;uint32_t frames=0;
        if(!read(name,32)||!std::memchr(name,0,32)||!name[0]||!read(&clip.duration,4)||!read(&frames,4))return false;
        if(!std::isfinite(clip.duration)||clip.duration<=0||frames<2||frames>1000)return false;
        clip.frames.resize(frames);
        for(auto& frame:clip.frames){
            uint32_t vertices=0;
            if(!read(&vertices,4)||vertices>100000||vertices%3||size_t(vertices)*sizeof(Vertex)>remaining)return false;
            frame.resize(vertices);
            if(!read(frame.data(),frame.size()*sizeof(Vertex)))return false;
            for(const auto& v:frame){
                const float values[]={v.x,v.y,v.u,v.v,v.r,v.g,v.b,v.a};
                for(float value:values)if(!std::isfinite(value))return false;
                if(std::abs(v.x)>20||std::abs(v.y)>20||v.u<0||v.u>1||v.v<0||v.v>1||
                   v.r<0||v.r>1||v.g<0||v.g>1||v.b<0||v.b>1||v.a<0||v.a>1)return false;
            }
        }
        if(!loaded.emplace(name,std::move(clip)).second)return false;
    }
    if(remaining||!loaded.count("wait")||!loaded.count("move")||!loaded.count("attack"))return false;
    clips=std::move(loaded);return true;
}

bool CharacterAnimation::Draw(GLuint texture,const std::string& name,float time,bool loop,
    float x,float y,float z,bool mirror,bool shadow) const{
    if(!texture||clips.empty())return false;
    auto found=clips.find(name);if(found==clips.end())found=clips.find("wait");
    if(found==clips.end())return false;
    const auto& clip=found->second;
    float t=std::max(0.0f,time);
    t=loop?std::fmod(t,clip.duration):std::min(t,clip.duration);
    const size_t index=std::min(clip.frames.size()-1,size_t(t/clip.duration*(clip.frames.size()-1)));
    glUseProgram(0);glDisable(GL_LIGHTING);glActiveTexture(GL_TEXTURE0);
    glEnable(GL_TEXTURE_2D);glBindTexture(GL_TEXTURE_2D,texture);
    glEnable(GL_ALPHA_TEST);glAlphaFunc(GL_GREATER,shadow?.35f:.01f);
    // Composite overlapping source slots against world depth without letting
    // an earlier slot occlude a later one. Then write the silhouette depth.
    // Tiny per-triangle offsets are not reliable at the scene depth precision.
    GLint previousDepth;GLboolean previousWrite,previousColor[4];
    glGetIntegerv(GL_DEPTH_FUNC,&previousDepth);
    glGetBooleanv(GL_DEPTH_WRITEMASK,&previousWrite);
    glGetBooleanv(GL_COLOR_WRITEMASK,previousColor);
    glDepthFunc(GL_LEQUAL);
    auto mesh=[&](){
        glBegin(GL_TRIANGLES);
        for(const auto& v:clip.frames[index]){
            const float horizontal=mirror?-v.x:v.x;
            glColor4f(v.r,v.g,v.b,v.a);glTexCoord2f(v.u,v.v);
            glVertex3f(x+.745f*horizontal,y+v.y+.025f,z-.667f*horizontal);
        }
        glEnd();
    };
    if(shadow){
        // The shadow pass only needs the alpha-tested silhouette.
        mesh();
    }else{
        glDepthMask(GL_FALSE);
        mesh();
        if(previousWrite){
            glColorMask(GL_FALSE,GL_FALSE,GL_FALSE,GL_FALSE);
            glDepthMask(GL_TRUE);
            mesh();
        }
    }
    glColorMask(previousColor[0],previousColor[1],previousColor[2],previousColor[3]);
    glDepthMask(previousWrite);glDepthFunc(previousDepth);
    glDisable(GL_ALPHA_TEST);glDisable(GL_TEXTURE_2D);glColor4f(1,1,1,1);
    return true;
}
