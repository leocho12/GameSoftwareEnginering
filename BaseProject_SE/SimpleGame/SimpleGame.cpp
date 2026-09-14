/*
Copyright 2022 Lee Taek Hee (Tech University of Korea)
This program is free software: you can redistribute it and/or modify
it under the terms of the What The Hell License. Do it plz.
This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY.
Tutorial prototype built on the supplied FreeGLUT/OpenGL project.
*/
#include "stdafx.h"
#include "Renderer.h"
#include "TutorialGame.h"
#include "Dependencies/freeglut.h"
#include <algorithm>
#include <cstdio>
#include <memory>
#include <Windows.h>

namespace {
std::unique_ptr<Renderer> renderer;
TutorialGame game;
int mouseX=0,mouseY=0,lastTick=0;
void Display(){if(renderer){renderer->Draw(game,mouseX,mouseY);glutSwapBuffers();}}
void Resize(int w,int h){if(renderer)renderer->Resize(w,h);}
void Close(){renderer.reset();}
void Command(UiCommand c){
    switch(c){
    case UiCommand::Unit0:game.defense.Select(0);break;
    case UiCommand::Unit1:game.defense.Select(1);break;
    case UiCommand::Unit2:game.defense.Select(2);break;
    case UiCommand::Unit3:game.defense.Select(3);break;
    case UiCommand::Rotate:game.defense.Rotate();break;
    case UiCommand::Retreat:game.defense.Retreat();break;
    case UiCommand::Speed:game.defense.speed=3-game.defense.speed;break;
    case UiCommand::Start:game.Start();break;
    case UiCommand::DefenseDemo:game.StartDefenseDemo();renderer->Recenter();break;
    case UiCommand::Skill:game.defense.Skill();break;
    case UiCommand::Confirm:game.Confirm();break;
    case UiCommand::Wait:game.Wait();break;
    case UiCommand::Pause:game.TogglePause();break;
    case UiCommand::Restart:game.Reset();break;
    case UiCommand::Retry:game.RetryBattle();break;
    case UiCommand::Explore:game.ContinueExploring();renderer->Recenter();break;
    default:break;
    }
}
void Mouse(int button,int state,int x,int y){
    mouseX=x;mouseY=y;if(state!=GLUT_DOWN||!renderer)return;
    if(button==3||button==4){renderer->Zoom(button==3?1:-1);return;}
    if(button==GLUT_RIGHT_BUTTON){
        if(game.paused)return;game.Cancel();
        if(game.phase!=Phase::Battle&&game.route.size()>1)game.route.erase(game.route.begin()+1,game.route.end());
        return;
    }
    if(button!=GLUT_LEFT_BUTTON)return;
    UiCommand c=renderer->HitUi(x,y,game);if(c!=UiCommand::None){Command(c);return;}
    if(game.paused||game.phase==Phase::Briefing||game.phase==Phase::Complete||game.phase==Phase::Defeat)return;
    if(!renderer->IsUi(x,y))game.Click(renderer->Pick(x,y,game));
}
void Motion(int x,int y){mouseX=x;mouseY=y;}
void Key(unsigned char key,int,int){
    if(key==27){if(game.preview.x>=0||game.target>=0||(game.phase==Phase::Battle&&game.defense.selected>=0))game.Cancel();else game.TogglePause();return;}
    if(game.paused)return;
    if(game.phase==Phase::Briefing){if(key==' '||key==13)game.Start();else if(key=='d'||key=='D'){game.StartDefenseDemo();renderer->Recenter();}return;}
    if(game.phase==Phase::Complete||game.phase==Phase::Defeat)return;
    if(game.phase==Phase::Battle){
        if(key>='1'&&key<='4')game.defense.Select(key-'1');
        else if(key=='q'||key=='Q')game.defense.Skill();
        else if(key=='r'||key=='R')game.defense.Retreat();
        else if(key=='f'||key=='F')game.defense.Rotate();
        else if(key==13)game.defense.Confirm();
        else if(key==' ')game.Wait();
        else if(key=='\t')game.defense.speed=3-game.defense.speed;
        return;
    }
    switch(key){
    case 'e':case 'E':game.Interact();break;
    case 'c':case 'C':renderer->Recenter();break;
    case 13:game.Confirm();break;
    case ' ':game.Wait();break;
    }
}
void Special(int key,int,int){
    if(key==GLUT_KEY_F11){static bool full=false;full=!full;if(full)glutFullScreen();else glutLeaveFullScreen();return;}
    if(game.paused||!renderer||game.phase==Phase::Battle||game.phase==Phase::Defeat)return;
    if(key==GLUT_KEY_LEFT)renderer->Pan(-1,1);
    if(key==GLUT_KEY_RIGHT)renderer->Pan(1,-1);
    if(key==GLUT_KEY_UP)renderer->Pan(-1,-1);
    if(key==GLUT_KEY_DOWN)renderer->Pan(1,1);
}
void Tick(int){
    if(!renderer)return;int now=glutGet(GLUT_ELAPSED_TIME);
    float dt=std::max(0.0f,(now-lastTick)/1000.0f);lastTick=now;
    Phase previous=game.phase;game.Update(dt);if(game.phase!=previous)renderer->Recenter();glutPostRedisplay();glutTimerFunc(16,Tick,0);
}
}
int main(int argc,char** argv){
    // Avoid Windows bitmap-stretching the entire OpenGL window on high-DPI monitors.
    using SetDpiContext=BOOL(WINAPI*)(HANDLE);
    auto setDpi=reinterpret_cast<SetDpiContext>(GetProcAddress(GetModuleHandleW(L"user32.dll"),"SetProcessDpiAwarenessContext"));
    if(!setDpi||!setDpi(reinterpret_cast<HANDLE>(-4)))SetProcessDPIAware();
    glutInit(&argc,argv);
    // Immediate-mode 3D and stroke text require a compatibility context, deliberately not core.
    glutInitContextVersion(3,0);glutInitDisplayMode(GLUT_DOUBLE|GLUT_RGBA|GLUT_DEPTH|GLUT_MULTISAMPLE);
    RECT work{};SystemParametersInfoW(SPI_GETWORKAREA,0,&work,0);
    int initialW=std::max(640,std::min(1920,int(work.right-work.left)-80));
    int initialH=std::max(480,std::min(1080,int(work.bottom-work.top)-100));
    glutInitWindowSize(initialW,initialH);glutInitWindowPosition(work.left+30,work.top+30);
    int window=glutCreateWindow("The Silent Relay | G&K Tutorial Prototype");if(window<=0)return 1;
    SetWindowTextW(WindowFromDC(wglGetCurrentDC()),L"침묵한 중계소 | 그리폰 튜토리얼 프로토타입");
    glutSetOption(GLUT_ACTION_ON_WINDOW_CLOSE,GLUT_ACTION_GLUTMAINLOOP_RETURNS);
    GLenum status=glewInit();
    if(status!=GLEW_OK||!GLEW_VERSION_3_0){
        std::fprintf(stderr,"OpenGL 3.0 compatibility initialization failed: %s\n",glewGetErrorString(status));
        glutDestroyWindow(window);return 1;
    }
    renderer=std::make_unique<Renderer>(initialW,initialH);
    glutDisplayFunc(Display);glutReshapeFunc(Resize);glutMouseFunc(Mouse);
    glutPassiveMotionFunc(Motion);glutMotionFunc(Motion);glutKeyboardFunc(Key);glutSpecialFunc(Special);glutCloseFunc(Close);
    glutIgnoreKeyRepeat(1);lastTick=glutGet(GLUT_ELAPSED_TIME);glutTimerFunc(16,Tick,0);
    glutMainLoop();renderer.reset();return 0;
}
