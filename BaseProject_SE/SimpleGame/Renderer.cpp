#include "stdafx.h"
#include "Renderer.h"
#include "Dependencies/freeglut.h"
#include <Windows.h>
#include <wincodec.h>
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <vector>
#pragma comment(lib,"windowscodecs.lib")
#pragma comment(lib,"ole32.lib")

namespace {
constexpr float Pi=3.14159265f;
float UIX(int x,int w) { return x*1280.0f/w; }
float UIY(int y,int h) { return y*800.0f/h; }
bool In(float x,float y,float l,float t,float w,float h) { return x>=l && x<=l+w && y>=t && y<=t+h; }
template<class T> void Release(T*& p) { if (p) { p->Release(); p=nullptr; } }
}

Renderer::Renderer(int w,int h) {
    effects.Initialize();Resize(w,h);
    wchar_t path[MAX_PATH]{}; GetModuleFileNameW(nullptr,path,MAX_PATH);
    std::wstring base(path); base=base.substr(0,base.find_last_of(L"\\/")+1)+L"Assets\\";
    HRESULT hr=CoInitializeEx(nullptr,COINIT_APARTMENTTHREADED);
    portraits[0]=LoadPng(base+L"alpha.png",portraitAspects[0]);
    portraits[1]=LoadPng(base+L"bravo.png",portraitAspects[1]);
    portraits[2]=LoadPng(base+L"enemy.png",portraitAspects[2]);
    const wchar_t* names[]={L"4type",L"m4a1",L"ump45",L"hk416",L"g11"};
    for(int i=0;i<5;++i){
        std::wstring stem=base+L"GFL\\"+names[i];float aspect=1;
        if(characters[i].Load(stem+L".gfa"))characterTextures[i]=LoadPng(stem+L".png",aspect);
        if(!characterTextures[i]){characters[i].Clear();std::fprintf(stderr,"GFL character %d unavailable; using placeholder.\n",i);}
    }
    if (SUCCEEDED(hr)) CoUninitialize();
}
Renderer::~Renderer() { Shutdown(); }
void Renderer::Shutdown() { for(auto& t:characterTextures){if(t)glDeleteTextures(1,&t);t=0;}for(auto& c:characters)c.Clear(); for (auto& t:portraits) if(t) { glDeleteTextures(1,&t); t=0; }effects.Shutdown(); }
void Renderer::Resize(int w,int h) { width=std::max(1,w); height=std::max(1,h); cameraReady=false;effects.Resize(width,height); }
void Renderer::Zoom(int direction){zoom=std::clamp(zoom-direction*.7f,5.0f,19.0f);cameraReady=false;}
void Renderer::Pan(float x,float z){manualCamera=true;cameraX=std::clamp(cameraX+x,0.0f,27.0f);cameraZ=std::clamp(cameraZ+z,0.0f,23.0f);cameraReady=false;}
GLuint Renderer::LoadPng(const std::wstring& path,float& aspect) {
    if (GetFileAttributesW(path.c_str())==INVALID_FILE_ATTRIBUTES) return 0;
    IWICImagingFactory* factory=nullptr; IWICBitmapDecoder* decoder=nullptr;
    IWICBitmapFrameDecode* frame=nullptr; IWICFormatConverter* converter=nullptr;
    GLuint texture=0;
    HRESULT hr=CoCreateInstance(CLSID_WICImagingFactory,nullptr,CLSCTX_INPROC_SERVER,IID_PPV_ARGS(&factory));
    if (SUCCEEDED(hr)) hr=factory->CreateDecoderFromFilename(path.c_str(),nullptr,GENERIC_READ,WICDecodeMetadataCacheOnLoad,&decoder);
    if (SUCCEEDED(hr)) hr=decoder->GetFrame(0,&frame);
    if (SUCCEEDED(hr)) hr=factory->CreateFormatConverter(&converter);
    if (SUCCEEDED(hr)) hr=converter->Initialize(frame,GUID_WICPixelFormat32bppRGBA,WICBitmapDitherTypeNone,nullptr,0,WICBitmapPaletteTypeCustom);
    UINT w=0,h=0;
    if (SUCCEEDED(hr)) hr=converter->GetSize(&w,&h);
    if (SUCCEEDED(hr) && w>0 && h>0 && w<=4096 && h<=4096) {
        std::vector<unsigned char> pixels(size_t(w)*h*4);
        hr=converter->CopyPixels(nullptr,w*4,UINT(pixels.size()),pixels.data());
        if (SUCCEEDED(hr)) {
            aspect=float(w)/h;glGenTextures(1,&texture);glBindTexture(GL_TEXTURE_2D,texture);
            glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_LINEAR);
            glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_LINEAR);
            glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_S,GL_CLAMP_TO_EDGE);
            glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_T,GL_CLAMP_TO_EDGE);
            glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA,GLsizei(w),GLsizei(h),0,GL_RGBA,GL_UNSIGNED_BYTE,pixels.data());
        }
    }
    if (FAILED(hr)) std::fprintf(stderr,"Could not decode optional PNG (HRESULT %lx).\n",static_cast<unsigned long>(hr));
    Release(converter);Release(frame);Release(decoder);Release(factory);return texture;
}
void Renderer::Camera(const TutorialGame& game) {
    if(game.phase==Phase::Battle||game.phase==Phase::Defeat){cameraX=8;cameraZ=4;}
    else if(!manualCamera){
        float tx=game.units[0].x+2,tz=game.units[0].z-1;
        if(game.phase==Phase::Briefing){tx=6.5f;tz=5.5f;}
        else if(game.phase==Phase::Battle&&game.wildEncounter<0){tx=7.5f;tz=4.0f;}
        else if(game.phase==Phase::Battle){tx=game.wildlife[game.wildEncounter].home.x;tz=game.wildlife[game.wildEncounter].home.z;}
        cameraX+=(tx-cameraX)*.12f;cameraZ+=(tz-cameraZ)*.12f;
    }
    int bottom=int(height*.20f),top=int(height*.12f);
    glViewport(0,bottom,width,std::max(1,height-bottom-top));
    glMatrixMode(GL_PROJECTION);glLoadIdentity();
    float aspect=float(width)/std::max(1,height-bottom-top),span=std::max(zoom,zoom*1.35f/aspect);
    glOrtho(-span*aspect,span*aspect,-span,span,-60,80);
    glMatrixMode(GL_MODELVIEW);glLoadIdentity();gluLookAt(cameraX+17,20,cameraZ+19,cameraX,0,cameraZ,0,1,0);
    glGetDoublev(GL_MODELVIEW_MATRIX,model);glGetDoublev(GL_PROJECTION_MATRIX,projection);glGetIntegerv(GL_VIEWPORT,viewport);
    cameraReady=true;combatView=game.phase==Phase::Battle||game.phase==Phase::Defeat;
}
void Renderer::Cube(float x,float y,float z,float sx,float sy,float sz,float r,float g,float b) {
    glColor3f(r,g,b);glPushMatrix();glTranslatef(x,y,z);glScalef(sx,sy,sz);glutSolidCube(1);glPopMatrix();
}
void Renderer::Line(float ax,float ay,float az,float bx,float by,float bz,float r,float g,float b,float a) {
    glColor4f(r,g,b,a);glBegin(GL_LINES);glVertex3f(ax,ay,az);glVertex3f(bx,by,bz);glEnd();
}
void Renderer::Tile(Cell c,float y,float r,float g,float b,float a,bool outline) {
    glColor4f(r,g,b,a);glBegin(outline?GL_LINE_LOOP:GL_QUADS);
    glVertex3f(c.x-.46f,y,c.z-.46f);glVertex3f(c.x-.46f,y,c.z+.46f);
    glVertex3f(c.x+.46f,y,c.z+.46f);glVertex3f(c.x+.46f,y,c.z-.46f);glEnd();
}
void Renderer::Ring(float x,float y,float z,float radius,float r,float g,float b,float a) {
    glColor4f(r,g,b,a);glBegin(GL_LINE_LOOP);
    for(int i=0;i<40;++i){float t=2*Pi*i/40;glVertex3f(x+std::cos(t)*radius,y,z+std::sin(t)*radius);}glEnd();
}
void Renderer::Environment(const TutorialGame& game) {
    if(game.phase==Phase::Battle||game.phase==Phase::Defeat){DefenseScene(game);return;}
    glEnable(GL_LIGHTING);glEnable(GL_LIGHT0);glEnable(GL_COLOR_MATERIAL);glEnable(GL_NORMALIZE);
    glColorMaterial(GL_FRONT_AND_BACK,GL_AMBIENT_AND_DIFFUSE);
    GLfloat sun[]={-28,37,-22,0},warm[]={1.0f,.76f,.51f,1},ambient[]={.27f,.32f,.38f,1};
    glLightfv(GL_LIGHT0,GL_POSITION,sun);glLightfv(GL_LIGHT0,GL_DIFFUSE,warm);glLightModelfv(GL_LIGHT_MODEL_AMBIENT,ambient);
    GLfloat fog[]={.19f,.23f,.25f,1};glEnable(GL_FOG);glFogi(GL_FOG_MODE,GL_LINEAR);
    glFogfv(GL_FOG_COLOR,fog);glFogf(GL_FOG_START,27);glFogf(GL_FOG_END,56);
    if(!shadowPass)effects.Lit(model,game.animationTime);
    Cube(13.5f,-.8f,11.5f,28.9f,1.4f,24.9f,.12f,.16f,.17f);
    for(int z=0;z<TutorialGame::Depth;++z)for(int x=0;x<TutorialGame::Width;++x){
        float shade=((x*17+z*31)%7)*.006f,h=game.Height({x,z});bool road=(x>=3&&x<=10&&z<12)||(z>=13&&z<=14)||(x>=16&&x<=17);
        if(!shadowPass)effects.Material(game.Water({x,z})?3:road?2:5);
        Cube(float(x),h-.13f,float(z),.985f,.25f,.985f,road?.30f+shade:.22f+shade,road?.32f+shade:.27f+shade,.27f+shade);
        if(h>0)Cube(float(x),.18f,float(z),1,.6f,1,.27f,.29f,.27f);
    }
    if(!shadowPass)effects.Material(4);
    for(int i=0;i<5;++i){
        float x=-2.5f+i*4.3f;
        Cube(x,1.4f,-3.3f,3.5f,3.0f+(i%2),2.8f,.19f,.23f,.24f);
        Cube(x,3.1f+(i%2)*.5f,-3.3f,3.7f,.17f,3.0f,.13f,.16f,.17f);
        for(int j=0;j<3;++j)Cube(x-1+j,1.8f,-1.87f,.55f,.65f,.03f,.35f,.28f,.19f);
    }
    Cube(.5f,.9f,1.5f,2,1.8f,4,.28f,.32f,.31f);Cube(.5f,1.85f,1.5f,2.2f,.18f,4.2f,.18f,.23f,.23f);
    for(Cell c:{Cell{6,4},Cell{6,5},Cell{9,6}}){
        Cube(float(c.x),.48f,float(c.z),.9f,.95f,.8f,.31f,.36f,.32f);
        Cube(float(c.x),.94f,float(c.z),.95f,.10f,.86f,.47f,.43f,.31f);
    }
    for(int z=7;z<=9;++z){Cube(12,.55f,float(z),.95f,1.1f,.96f,.23f,.29f,.28f);Cube(12,.92f,float(z),.98f,.08f,.98f,.44f,.37f,.24f);}
    for(int x=8;x<=9;++x)for(int s=0;s<4;++s)Cube(float(x),.08f*(s+1),4.42f-s*.23f,.94f,.16f*(s+1),.24f,.41f,.43f,.39f);
    Cube(3,.33f,8,.85f,.64f,.7f,.42f,.39f,.22f);Cube(3,.68f,8,.90f,.09f,.74f,.62f,.52f,.28f);
    Cube(3,.35f,8.36f,.35f,.24f,.02f,.90f,.66f,.23f);
    Cube(11,1.2f,1,.65f,1.1f,.55f,.16f,.23f,.24f);Cube(11,1.87f,1,.9f,.3f,.7f,.24f,.30f,.3f);
    Cube(11,1.52f,1.29f,.44f,.35f,.025f,.10f,.55f,.48f);
    Cube(12.4f,2.7f,.3f,.16f,5.4f,.16f,.32f,.34f,.32f);Cube(12.4f,4.5f,.3f,1.8f,.10f,.12f,.39f,.39f,.33f);
    Cube(12.4f,3.8f,.3f,1.1f,.10f,.12f,.39f,.39f,.33f);Cube(12.4f,3.3f,.3f,.13f,.12f,1.5f,.39f,.39f,.33f);
    for(int i=0;i<7;++i)Cube(3+i*1.5f,.62f,-.5f,.07f,1.3f,.07f,.31f,.31f,.28f);
    for(int i=0;i<3;++i)Cube(7.5f,.25f+i*.35f,-.5f,9,.025f,.025f,.36f,.33f,.28f);
    for(int i=0;i<24;++i)Cube(i%2==0?-.3f:13.25f,.13f,.3f+(i/2)*.91f,.22f,.30f,.11f,.23f,.30f,.21f);
    // New eastern settlement and southern wetland connect to the original tutorial yard.
    Cube(17,1.05f,3,3,2.1f,3,.35f,.37f,.33f);Cube(17,2.15f,3,3.3f,.18f,3.3f,.22f,.29f,.30f);
    Cube(25,1.25f,18,3,2.5f,3,.31f,.36f,.34f);Cube(25,2.53f,18,3.3f,.18f,3.3f,.26f,.31f,.31f);
    for(int i=0;i<3;++i){Cube(16+i,1.35f,4.51f,.6f,.6f,.03f,.66f,.53f,.29f);Cube(24+i,1.45f,19.51f,.6f,.65f,.03f,.59f,.48f,.26f);}
    if(!shadowPass)effects.Material(1);
    for(int z=8;z<24;++z){Cube(18.5f,.05f,float(z),.15f,.28f,1,.4f,.42f,.38f);Cube(21.5f,.05f,float(z),.15f,.28f,1,.4f,.42f,.38f);}
    for(int x=19;x<=21;++x)for(int z=13;z<=14;++z)Cube(float(x),.02f,float(z),1,.1f,1,.42f,.39f,.29f);
    if(!shadowPass)effects.Material(4);
    for(int x=18;x<=22;++x){Cube(float(x),.55f,12.5f,.05f,1.1f,.05f,.33f,.36f,.31f);Cube(float(x),.55f,14.5f,.05f,1.1f,.05f,.33f,.36f,.31f);}
    Cube(20,1,12.5f,4,.035f,.035f,.37f,.4f,.35f);Cube(20,1,14.5f,4,.035f,.035f,.37f,.4f,.35f);
    if(!shadowPass)effects.Material(5);
    for(int i=0;i<52;++i){float x=14+(i*7%13),z=9+(i*11%14);Cell c{int(x),int(z)};
        if(game.Water(c)||game.Wall(c)||(z>=13&&z<=14))continue;
        float sway=shadowPass?0:std::sin(game.animationTime*1.3f+i)*.04f;
        Cube(x+sway,.17f,z,.09f,.34f,.10f,.26f,.34f,.20f);
    }
    if(shadowPass)return;
    glUseProgram(0);glDisable(GL_LIGHTING);glLineWidth(2);
    Line(11,.69f,1,10,.69f,2,.08f,.10f,.09f);Line(10,.69f,2,8,.69f,2,.08f,.10f,.09f);
    for(Cell c:{Cell{6,4},Cell{6,5},Cell{9,6}}){
        glColor4f(.05f,.08f,.08f,.24f);glBegin(GL_QUADS);
        glVertex3f(c.x-.4f,.012f,c.z-.3f);glVertex3f(c.x+.4f,.012f,c.z-.3f);
        glVertex3f(c.x+1.3f,.012f,c.z+1.3f);glVertex3f(c.x+.5f,.012f,c.z+1.3f);glEnd();
    }
    bool restored=game.phase==Phase::Complete||game.phase==Phase::Explore||game.wildEncounter>=0;
    Cube(12.4f,5.45f,.3f,.19f,.15f,.19f,restored?.35f:1,restored?.95f:.23f,.2f);
    Ring(11,.68f,1,.7f,restored?.3f:1,.75f,.32f,.9f);glDisable(GL_FOG);
}
void Renderer::Actors(const TutorialGame& game) {
    if(game.phase==Phase::Battle||game.phase==Phase::Defeat){DefenseActors(game);return;}
    glUseProgram(0);
    glDisable(GL_LIGHTING);
    for(int i=0;i<5;++i){
        const auto& u=game.units[i];if(u.hp<=0)continue;
        float y=game.Height(u.cell),x=u.x,z=u.z,r=u.ally?.36f:.96f,g=u.ally?.83f:.34f,b=u.ally?.78f:.27f;
        glColor4f(.035f,.055f,.06f,.4f);glBegin(GL_TRIANGLE_FAN);glVertex3f(x,y+.02f,z);
        for(int n=0;n<=32;++n){float a=n*Pi/16;glVertex3f(x+std::cos(a)*.40f,y+.02f,z+std::sin(a)*.30f);}glEnd();
        glLineWidth(i==game.active?3.0f:1.5f);Ring(x,y+.04f,z,.41f,r,g,b);
        int imageIndex=u.ally?i:2;bool walking=!game.route.empty()&&i==(game.phase==Phase::Battle?game.active:0);
        int frame=walking?int(game.animationTime*10)%8:0;
        float bob=walking?std::abs(std::sin(game.animationTime*10))*.035f:std::sin(game.animationTime*2+i)*.015f;
        bool wild=game.wildEncounter>=0&&i==2;
        std::string clip=walking?"move":game.phase==Phase::Complete?"victory":"wait";
        float clock=game.phase==Phase::Complete?game.phaseTime:game.animationTime;
        bool mirror=walking&&game.route.front().x<u.x;
        if(!u.ally||!Character(i,clip,clock,x,y,z,mirror,game.phase!=Phase::Complete))
            Sprite(x,y+bob,z,wild?3:imageIndex,frame,wild?0:portraits[imageIndex],portraitAspects[imageIndex]);
        glPushMatrix();glTranslatef(x,y+1.85f,z);glRotatef(42,0,1,0);
        glColor3f(.10f,.14f,.15f);glBegin(GL_QUADS);glVertex3f(-.42f,0,0);glVertex3f(.42f,0,0);glVertex3f(.42f,.07f,0);glVertex3f(-.42f,.07f,0);glEnd();
        float end=-.40f+.80f*u.hp/u.maxHp;glColor3f(r,g,b);glBegin(GL_QUADS);
        glVertex3f(-.40f,.01f,.005f);glVertex3f(end,.01f,.005f);glVertex3f(end,.06f,.005f);glVertex3f(-.40f,.06f,.005f);glEnd();glPopMatrix();
    }
    if(game.effectTime>0&&game.effectFrom>=0&&game.effectTo>=0){
        const auto& a=game.units[game.effectFrom];const auto& b=game.units[game.effectTo];
        glLineWidth(4);Line(a.x,game.Height(a.cell)+.65f,a.z,b.x,game.Height(b.cell)+.65f,b.z,game.healingEffect?.3f:1,game.healingEffect?1:.76f,.35f,game.effectTime/.7f);
        Ring(b.x,game.Height(b.cell)+.04f,b.z,1-game.effectTime*.6f,1,.7f,.3f,game.effectTime);
    }
    glLineWidth(1);
}
void Renderer::Sprite(float x,float y,float z,int kind,int frame,GLuint texture,float aspect,bool mirror){
    if(!texture){texture=effects.Sprite(kind,frame);aspect=.8f;}
    if(!texture)return;
    glUseProgram(0);glDisable(GL_LIGHTING);glEnable(GL_TEXTURE_2D);glBindTexture(GL_TEXTURE_2D,texture);
    glEnable(GL_ALPHA_TEST);glAlphaFunc(GL_GREATER,shadowPass?.35f:.01f);
    float h=1.8f,w=h*aspect;if(w>1.5f){h*=1.5f/w;w=1.5f;}
    float rx=.745f*w*.5f,rz=-.667f*w*.5f;
    glColor4f(1,1,1,1);glBegin(GL_QUADS);
    glTexCoord2f(mirror?1.0f:0.0f,1);glVertex3f(x-rx,y+.025f,z-rz);
    glTexCoord2f(mirror?0.0f:1.0f,1);glVertex3f(x+rx,y+.025f,z+rz);
    glTexCoord2f(mirror?0.0f:1.0f,0);glVertex3f(x+rx,y+h,z+rz);
    glTexCoord2f(mirror?1.0f:0.0f,0);glVertex3f(x-rx,y+h,z-rz);glEnd();
    glDisable(GL_ALPHA_TEST);glDisable(GL_TEXTURE_2D);
}
void Renderer::Extras(const TutorialGame& game){
    if(game.phase==Phase::Battle||game.phase==Phase::Defeat)return;
    for(int i=0;i<int(game.npcs.size());++i){const auto& n=game.npcs[i];float y=game.Height(n.cell);
        Sprite(float(n.cell.x),y+std::sin(game.animationTime*1.8f+i)*.018f,float(n.cell.z),i%2,0);
        if(!shadowPass){Ring(float(n.cell.x),y+.03f,float(n.cell.z),.38f,.85f,.71f,.37f);}
    }
    for(int i=0;i<int(game.wildlife.size());++i){const auto& w=game.wildlife[i];if(w.hp<=0||i==game.wildEncounter)continue;
        bool moving=std::abs(w.cell.x-w.x)+std::abs(w.cell.z-w.z)>.02f;
        Sprite(w.x,0,w.z,3,moving?int(game.animationTime*10)%8:0);
        if(!shadowPass)Ring(w.x,.03f,w.z,.42f,.88f,.32f,.21f);
    }
}
void Renderer::Panel(float x,float y,float w,float h,float r,float g,float b,float a){
    glColor4f(r,g,b,a);glBegin(GL_QUADS);glVertex2f(x,y);glVertex2f(x+w,y);glVertex2f(x+w,y+h);glVertex2f(x,y+h);glEnd();
}
void Renderer::Text(float x,float y,const std::string& s,float size,float r,float g,float b){
    effects.Text(x,y,s,size,r,g,b);
}
void Renderer::Button(float x,float y,float w,const std::string& label,bool enabled,bool selected,int mx,int my){
    bool hover=In(UIX(mx,width),UIY(my,height),x,y,w,40);
    Panel(x,y,w,40,selected?.22f:.105f,selected?.37f:.16f,selected?.34f:.18f);
    if(enabled&&(hover||selected))Panel(x,y,w,2,.86f,.69f,.37f);
    Text(x+12,y+11,label,14,enabled?.94f:.34f,enabled?.91f:.40f,enabled?.82f:.41f);
}
void Renderer::Hud(const TutorialGame& g,int mx,int my){
    if(g.phase==Phase::Battle||g.phase==Phase::Defeat){DefenseHud(g,mx,my);Overlay(g);return;}
    for(const auto& n:g.npcs){
        GLdouble sx,sy,sz;gluProject(n.cell.x,g.Height(n.cell)+2.05f,n.cell.z,model,projection,viewport,&sx,&sy,&sz);
        float px=float(sx)*1280/width,py=float(height-sy)*800/height;
        if(px>30&&px<1040&&py>230&&py<595){Panel(px-42,py-1,90,20,.045f,.09f,.10f,.82f);Text(px-36,py,n.name,12,.89f,.78f,.52f);}
    }
    Panel(0,0,1280,96,.045f,.075f,.09f);Panel(0,95,1280,1,.34f,.43f,.42f);
    Text(30,17,"그리폰 · 크루거 / 현장 작전",12,.68f,.74f,.73f);Text(30,39,"침묵한 중계소",27,.96f,.91f,.78f);
    Text(390,25,g.Objective(),18,.87f,.78f,.55f);Text(390,58,"외곽 계약 구역 / 주민 연락망 복구",11,.52f,.64f,.64f);
    int seconds=int(g.elapsed);char timer[64];std::snprintf(timer,sizeof(timer),"%02d:%02d / 목표 05:00",seconds/60,seconds%60);
    Text(1010,26,timer,14,.80f,.85f,.80f);Button(1100,51,150,"Esc / 일시정지",g.phase!=Phase::Briefing&&g.phase!=Phase::Complete,false,mx,my);
    Panel(22,114,298,109,.055f,.10f,.12f,.90f);Text(38,127,"작전 목표",12,.79f,.69f,.46f);
    Text(38,151,std::string(g.hasParts?"[x]":"[ ]")+" 복구 부품 확보",13);
    Text(38,174,std::string(g.yardCleared?"[x]":"[ ]")+" 중계소 방어",13);
    Text(38,197,std::string(g.phase==Phase::Complete||g.phase==Phase::Explore||g.wildEncounter>=0?"[x]":"[ ]")+" 중계소 복구",13);
    Text(30,610,characters[0].Ready()?"원작 SD 애니메이션 적용 · NPC와 적은 임시 외형":"캐릭터 데이터 로드 실패 · 임시 외형 사용",12,.68f,.73f,.7f);
    Panel(1078,411,190,191,.035f,.075f,.085f,.92f);Text(1090,417,"주변 지도 · 클릭 이동",12,.8f,.74f,.5f);
    for(int z=0;z<TutorialGame::Depth;++z)for(int x=0;x<TutorialGame::Width;++x){
        Cell c{x,z};bool water=g.Water(c),wall=g.Wall(c);
        Panel(1090+x*6.0f,440+z*6.0f,5,5,water?.10f:wall?.27f:.19f,water?.34f:wall?.27f:.27f,water?.4f:.22f);
    }
    for(const auto& n:g.npcs)Panel(1090+n.cell.x*6.0f,440+n.cell.z*6.0f,5,5,.9f,.72f,.35f);
    for(const auto& w:g.wildlife)if(w.hp>0)Panel(1090+w.cell.x*6.0f,440+w.cell.z*6.0f,5,5,.9f,.3f,.23f);
    Panel(1089+g.units[0].x*6,439+g.units[0].z*6,7,7,.38f,.96f,.83f);
    Text(1090,587,"휠 확대 · 방향키 시점 · C 추적",10,.6f,.74f,.7f);
    Panel(0,640,1280,160,.045f,.075f,.09f);Panel(0,640,1280,2,.40f,.45f,.39f);Text(28,655,"지휘 통신",11,.76f,.64f,.40f);
    // Fit a complete UTF-8 line rather than splitting multi-byte Korean characters.
    float messageSize=16;float measured=effects.TextWidth(g.message,messageSize);
    if(measured>1210)messageSize*=1210/measured;
    Text(28,680,g.message,messageSize);
    Text(30,735,"좌클릭 이동·상호작용    E 주변 대화·조사    Esc 일시정지    F11 전체화면",14);
    Text(30,765,"보급함 조사 → 중계소 방어 → 단말 복구 · 휠 확대/축소 · C 카메라 추적",13,.60f,.72f,.68f);
    Overlay(g);
}
void Renderer::Overlay(const TutorialGame& g){
    if(g.phase==Phase::Complete&&g.phaseTime<2.0f)return;
    if(g.phase!=Phase::Briefing&&g.phase!=Phase::Complete&&g.phase!=Phase::Defeat&&!g.paused)return;
    Panel(0,0,1280,800,.025f,.045f,.055f,.72f);Panel(300,215,680,352,.065f,.105f,.12f,.98f);Panel(300,215,680,3,.82f,.66f,.35f);
    std::string tag="첫 현장 임무 / 01",title="침묵한 중계소";
    std::string a="당신은 이 외곽 구역을 맡은 그리폰의 인간 지휘관입니다.",b="주민과 보급 수송대의 연락망이 끊겼습니다. 인형들과 복구하세요.",c="인형 4기를 배치해 두 진입로를 방어합니다. 목표 시간은 5분 이내입니다.";
    std::string button="전체 튜토리얼 시작";
    if(g.paused){tag="작전 일시정지";title="지시를 기다리고 있습니다";a="작전 시간은 일시정지 상태입니다.";b="계속 진행하거나 튜토리얼을 처음부터 다시 시작할 수 있습니다.";c="";button="계속하기";}
    else if(g.phase==Phase::Complete){
        tag="작전 성공";title="통신 연결을 복구했습니다";a="주민 연락망 복구 완료. 다음 보고: 돌아오지 않은 수송차.";
        b="작전 시간: "+std::to_string(int(g.elapsed)/60)+"분 "+std::to_string(int(g.elapsed)%60)+"초 / 방어 완료";
        c=g.defenseDemo?"방어전 체험 완료 · 전체 튜토리얼 시간 측정에서는 제외됩니다.":g.elapsed<=300?"이번 플레이는 5분 목표를 달성했습니다.":"임무 완료. 이번 플레이는 5분 목표를 넘겼습니다.";button="자유 탐험 계속";
    }else if(g.phase==Phase::Defeat){tag="거점 방어 실패";title="재정비 후 다시 도전하세요";a="지상 인형으로 저지하고 고지의 소총·수리 인형으로 지원하세요.";b="재도전하면 양측이 회복됩니다. 확보한 복구 부품은 유지됩니다.";c="훈련에서는 동료를 영구적으로 잃지 않습니다.";button="전투 재도전";}
    Text(340,245,tag,13,.78f,.66f,.41f);Text(340,278,title,27,.96f,.91f,.8f);
    Text(340,341,a,14);Text(340,375,b,13);Text(340,407,c,12,.65f,.77f,.72f);
    Panel(340,473,280,47,.23f,.37f,.34f);Text(361,487,button,17,.96f,.91f,.77f);
    if(g.paused||g.phase==Phase::Complete){Panel(650,473,250,47,.20f,.22f,.23f);Text(670,487,"레벨 다시 시작",16);}
    if(!g.paused&&g.phase==Phase::Briefing){Panel(650,473,250,47,.20f,.30f,.34f);Text(670,487,"방어전 바로 체험 [D]",16);}
    Text(340,537,"버튼을 클릭하세요 · 일시정지 중에는 작전 시간이 흐르지 않습니다",10,.51f,.63f,.63f);
}
void Renderer::Draw(const TutorialGame& game,int mx,int my){
    glUseProgram(0);glActiveTexture(GL_TEXTURE0);glDisable(GL_TEXTURE_2D);
    if(effects.BeginShadow()){
        shadowPass=true;Environment(game);
        glDisable(GL_LIGHTING);glDisable(GL_FOG);
        // Silhouette casters use the same billboard alpha as the visible sprites.
        if(game.phase==Phase::Battle||game.phase==Phase::Defeat)DefenseActors(game);else{
        for(int i=0;i<5;++i){
            const auto& u=game.units[i];if(u.hp<=0)continue;
            bool wild=game.wildEncounter>=0&&i==2;
            bool moving=!game.route.empty()&&i==(game.phase==Phase::Battle?game.active:0);
            int image=u.ally?i:2;
            std::string clip=moving?"move":game.phase==Phase::Complete?"victory":"wait";
            float clock=game.phase==Phase::Complete?game.phaseTime:game.animationTime;
            bool mirror=moving&&game.route.front().x<u.x;
            if(!u.ally||!Character(i,clip,clock,u.x,game.Height(u.cell),u.z,mirror,game.phase!=Phase::Complete))
                Sprite(u.x,game.Height(u.cell),u.z,wild?3:image,moving?int(game.animationTime*10)%8:0,wild?0:portraits[image],portraitAspects[image]);
        }
        }
        Extras(game);shadowPass=false;
    }
    effects.BeginScene();
    glEnable(GL_DEPTH_TEST);glEnable(GL_BLEND);glBlendFunc(GL_SRC_ALPHA,GL_ONE_MINUS_SRC_ALPHA);
    Camera(game);Environment(game);glDisable(GL_LIGHTING);glDisable(GL_FOG);glDepthMask(GL_FALSE);
    Cell hover=Pick(mx,my,game);if(game.Inside(hover)&&!IsUi(mx,my))Tile(hover,game.Height(hover)+.05f,.96f,.85f,.54f,.85f,true);
    Cell object=game.phase==Phase::Supply?TutorialGame::SupplyCell:game.phase==Phase::Restore?TutorialGame::TerminalCell:Cell{};
    if(game.Inside(object)){float h=game.Height(object);Ring(float(object.x),h+.05f,float(object.z),.64f,1,.75f,.30f);Line(float(object.x),h+1.1f,float(object.z),float(object.x),h+1.75f,float(object.z),1,.78f,.35f);}
    if(!game.route.empty())for(Cell c:game.route)Ring(float(c.x),game.Height(c)+.03f,float(c.z),.08f,.83f,.81f,.52f,.75f);
    glDepthMask(GL_TRUE);Actors(game);Extras(game);effects.Composite();glDisable(GL_DEPTH_TEST);glDisable(GL_LIGHTING);glDisable(GL_TEXTURE_2D);glDisable(GL_FOG);
    glViewport(0,0,width,height);glMatrixMode(GL_PROJECTION);glLoadIdentity();glOrtho(0,1280,800,0,-1,1);
    glMatrixMode(GL_MODELVIEW);glLoadIdentity();Hud(game,mx,my);
}
Cell Renderer::Pick(int x,int y,const TutorialGame& game) const {
    float ux=UIX(x,width),uy=UIY(y,height);
    if(game.phase!=Phase::Battle&&game.phase!=Phase::Defeat&&In(ux,uy,1090,440,168,144))return {std::clamp(int((ux-1090)/6),0,27),std::clamp(int((uy-440)/6),0,23)};
    if(!cameraReady||IsUi(x,y))return {};
    GLdouble nx,ny,nz,fx,fy,fz;
    if(!gluUnProject(x,height-y,0,model,projection,viewport,&nx,&ny,&nz)||!gluUnProject(x,height-y,1,model,projection,viewport,&fx,&fy,&fz))return {};
    if(std::abs(fy-ny)<.0001)return {};
    Cell best;double distance=1e9;
    for(int z=0;z<TutorialGame::Depth;++z)for(int xx=0;xx<TutorialGame::Width;++xx){
        double t=(game.Height({xx,z})-ny)/(fy-ny);if(t<0||t>1)continue;
        double wx=nx+(fx-nx)*t,wz=nz+(fz-nz)*t;
        if(wx>=xx-.5&&wx<xx+.5&&wz>=z-.5&&wz<z+.5&&t<distance){best={xx,z};distance=t;}
    }
    if(game.phase==Phase::Battle||game.phase==Phase::Defeat)return best;
    double nearest=1e9;
    auto pickActor=[&](Cell cell,float px,float pz,float elevation){
        GLdouble sx,sy,sz;gluProject(px,elevation,pz,model,projection,viewport,&sx,&sy,&sz);
        double dx=x-sx,dy=height-y-sy,d=dx*dx+dy*dy;
        if(d<324.0*height/800.0*height/800.0&&d<nearest){best=cell;nearest=d;}
    };
    for(int i=0;i<5;++i){
        const auto& u=game.units[i];if(u.hp<=0)continue;
        pickActor(u.cell,u.x,u.z,game.Height(u.cell)+.8f);
    }
    if(game.phase!=Phase::Battle){
        for(const auto& n:game.npcs)pickActor(n.cell,float(n.cell.x),float(n.cell.z),game.Height(n.cell)+.8f);
        for(const auto& w:game.wildlife)if(w.hp>0)pickActor(w.cell,w.x,w.z,.7f);
        pickActor(TutorialGame::SupplyCell,3,8,.45f);pickActor(TutorialGame::TerminalCell,11,1,1.35f);
    }
    return best;
}
bool Renderer::IsUi(int x,int y) const {
    float ux=UIX(x,width),uy=UIY(y,height);
    if(combatView)return uy<160||uy>=640;
    return uy<96||uy>=640||In(ux,uy,22,114,298,109)||In(ux,uy,918,114,340,55)||
        (In(ux,uy,1078,411,190,191)&&!In(ux,uy,1090,440,168,144));
}
UiCommand Renderer::HitUi(int x,int y,const TutorialGame& g) const {
    float ux=UIX(x,width),uy=UIY(y,height);
    if(g.phase==Phase::Complete&&g.phaseTime<2.0f)return UiCommand::None;
    if(g.phase==Phase::Briefing||g.phase==Phase::Complete||g.phase==Phase::Defeat||g.paused){
        if(!g.paused&&g.phase==Phase::Briefing&&In(ux,uy,650,473,250,47))return UiCommand::DefenseDemo;
        if(In(ux,uy,340,473,280,47))return g.paused?UiCommand::Pause:g.phase==Phase::Briefing?UiCommand::Start:g.phase==Phase::Defeat?UiCommand::Retry:UiCommand::Explore;
        if((g.paused||g.phase==Phase::Complete)&&In(ux,uy,650,473,250,47))return UiCommand::Restart;return UiCommand::None;
    }
    if(In(ux,uy,1100,51,150,40))return UiCommand::Pause;
    if(g.phase==Phase::Battle){
        for(int i=0;i<4;++i)if(In(ux,uy,24+i*200.0f,658,188,40))return static_cast<UiCommand>(int(UiCommand::Unit0)+i);
        if(In(ux,uy,24,748,140,40))return UiCommand::Rotate;
        if(In(ux,uy,174,748,140,40))return UiCommand::Confirm;
        if(In(ux,uy,324,748,140,40))return UiCommand::Skill;
        if(In(ux,uy,474,748,140,40))return UiCommand::Retreat;
        if(In(ux,uy,824,658,190,40))return UiCommand::Wait;
        if(In(ux,uy,1024,658,220,40))return UiCommand::Speed;
    }
    return UiCommand::None;
}


void Renderer::DefenseScene(const TutorialGame& game){
    const auto& d=game.defense;
    if(!shadowPass)effects.Lit(model,game.animationTime,1);
    Cube(8,-.3f,4,14,.4f,10,.19f,.24f,.23f);
    for(int z=1;z<=7;++z)for(int x=3;x<=13;++x){Cell c{x,z};bool road=d.Road(c),high=d.High(c);
        if(!shadowPass)effects.Material(road?2:high?4:1);
        Cube(float(x),high?.125f:-.05f,float(z),.96f,high?.45f:.1f,.96f,road?.30f:.39f,road?.29f:.44f,road?.25f:.42f);
    }
    if(shadowPass)return;
    glUseProgram(0);glDisable(GL_LIGHTING);glDisable(GL_FOG);
    glDepthMask(GL_FALSE);
    for(const auto& lane:d.lanes){
        for(size_t i=1;i<lane.size();++i){const Cell a=lane[i-1],b=lane[i];
            Line(float(a.x),.025f,float(a.z),float(b.x),.025f,float(b.z),.96f,.52f,.25f,.8f);
            float mx=(a.x+b.x)*.5f,mz=(a.z+b.z)*.5f,dx=float(b.x-a.x),dz=float(b.z-a.z);
            Line(mx,.03f,mz,mx-dx*.17f+dz*.12f,.03f,mz-dz*.17f-dx*.12f,1,.7f,.3f);
            Line(mx,.03f,mz,mx-dx*.17f-dz*.12f,.03f,mz-dz*.17f+dx*.12f,1,.7f,.3f);
        }
        Tile(lane.front(),.04f,1,.20f,.16f,.72f);Tile(lane.back(),.04f,.2f,.66f,1,.85f);
    }
    for(int z=1;z<=7;++z)for(int x=3;x<=13;++x){Cell c{x,z};float y=game.Height(c)+.03f;
        Tile(c,y,.66f,.78f,.73f,.38f,true);
        if(d.CanDeploy(d.selected,c))Tile(c,y+.01f,.2f,.86f,.6f,.30f);
        if(d.selected>=0){const auto& u=d.team[d.selected];Cell origin=d.pending.x>=0?d.pending:u.cell;
            if((u.deployed||d.pending.x>=0)&&d.InRange(d.selected,origin,d.pending.x>=0?d.facing:u.facing,float(x),float(z)))
                Tile(c,y+.02f,1,.73f,.24f,.25f);
        }
    }
    if(d.pending.x>=0){Tile(d.pending,game.Height(d.pending)+.06f,.3f,1,.75f,.9f,true);
        const int dx[]={1,0,-1,0},dz[]={0,1,0,-1};
        Line(float(d.pending.x),.65f,float(d.pending.z),float(d.pending.x+dx[d.facing]),.65f,float(d.pending.z+dz[d.facing]),1,.85f,.25f);
    }
    glDepthMask(GL_TRUE);
}
void Renderer::DefenseActors(const TutorialGame& game){
    const auto& d=game.defense;
    auto health=[&](float x,float y,float z,float ratio,bool ally){
        if(shadowPass)return;
        glLineWidth(5);Line(x-.32f,y,z+.29f,x+.32f,y,z-.29f,.08f,.1f,.1f);
        Line(x-.32f,y+.01f,z+.29f,x-.32f+.64f*ratio,y+.01f,z+.29f-.58f*ratio,ally?.3f:1,ally?.95f:.35f,.4f);glLineWidth(1);
    };
    for(int i=0;i<4;++i){const auto& u=d.team[i];if(!u.deployed){
        if(d.time-u.defeatedAt>=0&&d.time-u.defeatedAt<2){const int models[]={2,0,3,4};
            Character(models[i],"die",d.time-u.defeatedAt,float(u.cell.x),game.Height(u.cell),float(u.cell.z),u.facing==2,false);}
        continue;
    }
        float y=game.Height(u.cell);const int models[]={2,0,3,4};
        float sinceAttack=d.time-u.attackStarted;
        bool attacking=sinceAttack>=0&&sinceAttack<std::min(u.interval,.85f);
        if(!Character(models[i],attacking?"attack":"wait",attacking?sinceAttack:d.time-u.deployedAt,float(u.cell.x),y,float(u.cell.z),u.facing==2,!attacking))
            Sprite(float(u.cell.x),y,float(u.cell.z),i%2,0,portraits[i%2],portraitAspects[i%2],u.facing==2);
        if(!shadowPass){Ring(float(u.cell.x),y+.05f,float(u.cell.z),.42f,i==d.selected?1:.25f,.85f,.6f);
            const int dx[]={1,0,-1,0},dz[]={0,1,0,-1};
            Line(float(u.cell.x),y+.1f,float(u.cell.z),u.cell.x+dx[u.facing]*.65f,y+.1f,u.cell.z+dz[u.facing]*.65f,1,.8f,.3f);
            if(u.skillTime>0)Ring(float(u.cell.x),y+.12f,float(u.cell.z),.55f,.9f,.75f,.25f);
        }
        health(float(u.cell.x),y+1.9f,float(u.cell.z),u.hp/u.maxHp,true);
    }
    if(d.pending.x>=0&&d.selected>=0&&d.CanDeploy(d.selected,d.pending)){
        const int models[]={2,0,3,4};
        Character(models[d.selected],"wait",d.time,float(d.pending.x),game.Height(d.pending),float(d.pending.z),d.facing==2);
    }
    for(const auto& e:d.enemies)if(e.alive){
        Sprite(e.x,0,e.z,d.wild?3:2,e.blockedBy>=0?0:int(d.time*10)%8,d.wild?0:portraits[2],portraitAspects[2]);
        health(e.x,1.9f,e.z,e.hp/e.maxHp,false);
    }
    if(!shadowPass)for(const auto& t:d.traces){glLineWidth(3);Line(t.ax,.9f,t.az,t.bx,.9f,t.bz,t.heal?.3f:1,t.heal?1:.65f,.35f);glLineWidth(1);}
}
void Renderer::DefenseHud(const TutorialGame& game,int mx,int my){
    const auto& d=game.defense;
    for(const auto& u:d.team)if(u.deployed){
        GLdouble sx,sy,sz;gluProject(u.cell.x,game.Height(u.cell)+2.05f,u.cell.z,model,projection,viewport,&sx,&sy,&sz);
        float x=float(sx)*1280/width,y=float(height-sy)*800/height;
        if(y>162&&y<615){Panel(x-38,y,85,22,.03f,.08f,.1f,.85f);Text(x-34,y+1,u.name,13,.72f,.96f,.86f);}
    }
    Panel(0,0,1280,96,.04f,.07f,.09f);Text(24,16,"그리폰 / 실시간 방어 작전",16,.63f,.84f,.79f);
    Text(24,46,d.wild?"외곽 위험 개체 방어":"침묵한 중계소 방어",26,.98f,.89f,.7f);
    Text(430,22,"거점 "+std::to_string(d.lives)+" / 5     처리 "+std::to_string(d.resolved)+" / "+std::to_string(d.total),21);
    Text(430,57,"배치 비용 "+std::to_string(int(d.dp))+" / 99    배치 "+std::to_string(d.Deployed())+" / 4",18,.5f,.94f,.8f);
    Button(1100,51,150,"Esc / 메뉴",true,false,mx,my);
    Text(900,20,"작전 "+std::to_string(int(d.time))+"초",17);
    Panel(0,96,1280,64,.08f,.13f,.15f,.96f);
    Text(24,103,d.time<10?"준비 시간 "+std::to_string(int(std::ceil(10-d.time)))+"초 · 두 붉은 입구에서 파란 거점으로 적이 이동합니다.":
        "붉은 입구 → 주황 경로 → 파란 거점 · 지상: 선봉/방패 · 고지: 소총/수리",16,.95f,.78f,.48f);
    float textSize=16;float measured=effects.TextWidth(d.message,textSize);if(measured>1230)textSize*=1230/measured;
    Text(24,133,d.message,textSize);
    Panel(0,640,1280,160,.04f,.07f,.09f);
    for(int i=0;i<4;++i){const auto& u=d.team[i];float x=24+i*200.0f;
        const char* appearances[]={"UMP45", "4식", "HK416", "G11"};
        Button(x,658,188,std::to_string(i+1)+" / "+appearances[i],true,d.selected==i,mx,my);
        Text(x+5,701,u.deployed?"배치 중 · 체력 "+std::to_string(int(u.hp)):u.cooldown>0?"재배치 "+std::to_string(int(std::ceil(u.cooldown)))+"초":"비용 "+std::to_string(d.Cost(i))+" · "+(u.elevated?"고지":"지상"),14);
        Text(x+5,722,u.deployed?"기술 "+std::to_string(int(u.sp))+" / 20":u.role,13,.6f,.78f,.75f);
    }
    Button(824,658,190,d.tacticalPause?"Space / 전투 재개":"Space / 전술 정지",true,d.tacticalPause,mx,my);
    Button(1024,658,220,d.speed==1?"Tab / 1배속":"Tab / 2배속",true,d.speed==2,mx,my);
    Button(24,748,140,"F / 회전",d.pending.x>=0,false,mx,my);
    Button(174,748,140,"Enter / 배치",d.CanDeploy(d.selected,d.pending),false,mx,my);
    bool deployed=d.selected>=0&&d.team[d.selected].deployed;
    Button(324,748,140,"Q / 기술",deployed&&d.team[d.selected].sp>=20,false,mx,my);
    Button(474,748,140,"R / 퇴각",deployed,false,mx,my);
    Text(650,753,"우클릭 취소 · F11 전체화면 · 휠 확대",15);
    Text(824,708,d.tacticalPause?"시간 정지 중 · 배치/기술 지시 가능":"자동 공격 · 저지 초과 시 적 통과",14,.91f,.78f,.5f);
}

bool Renderer::Character(int index,const std::string& clip,float time,float x,float y,float z,bool mirror,bool loop){
    if(index<0||index>=int(characters.size()))return false;
    return characters[index].Draw(characterTextures[index],clip,time,loop,x,y,z,mirror,shadowPass);
}
