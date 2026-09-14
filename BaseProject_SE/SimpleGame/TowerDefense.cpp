#include "stdafx.h"
#include "TowerDefense.h"
#include <algorithm>
#include <cmath>

bool TowerDefense::Board(Cell c) const { return c.x>=3&&c.x<=13&&c.z>=1&&c.z<=7; }
bool TowerDefense::Road(Cell c) const {
    for(const auto& lane:lanes)for(Cell p:lane)if(c==p)return true;
    return false;
}
bool TowerDefense::High(Cell c) const { return Board(c)&&!Road(c)&&(c.z==2||c.z==4||c.z==6); }
int TowerDefense::At(Cell c) const { for(int i=0;i<4;++i)if(team[i].deployed&&team[i].cell==c)return i;return -1; }
int TowerDefense::Deployed() const { int n=0;for(const auto& u:team)if(u.deployed)++n;return n; }
int TowerDefense::Cost(int i) const { return i>=0&&i<4?team[i].cost:999; }
bool TowerDefense::CanDeploy(int i,Cell c) const {
    return i>=0&&i<4&&Board(c)&&c.x>3&&c.x<13&&At(c)<0&&
        !team[i].deployed&&team[i].cooldown<=0&&dp>=Cost(i)&&Deployed()<4&&High(c)==team[i].elevated;
}
bool TowerDefense::InRange(int i,Cell origin,int dir,float x,float z) const {
    float dx=x-origin.x,dz=z-origin.z;
    const float fx[]={1,0,-1,0},fz[]={0,1,0,-1};
    float forward=dx*fx[dir]+dz*fz[dir],side=std::abs(dx*fz[dir]-dz*fx[dir]);
    return forward>=-.5f&&forward<=(team[i].elevated?3.5f:1.25f)&&side<=(team[i].elevated?1.5f:.6f);
}
void TowerDefense::Reset(bool isWild) {
    *this=TowerDefense{};wild=isWild;total=wild?8:12;
    team={{{"선봉 인형","선봉 · 저지 1","보급 요청 +10",8,1,0,false,false,{},100,100,14,.8f},
           {"방패 인형","방어 · 저지 3","방어 강화 8초",12,3,0,false,false,{},240,240,10,1.2f},
           {"소총 인형","고지 · 원거리","집중 사격 8초",14,0,0,true,false,{},90,90,22,.8f},
           {"수리 인형","고지 · 회복","긴급 수리 8초",10,0,0,true,false,{},90,90,18,1.1f}}};
    for(int l=0;l<2;++l){for(int x=3;x<=10;++x)lanes[l].push_back({x,l==0?3:5});
        for(int x=10;x<=13;++x)lanes[l].push_back({x,4});}
    message="1~4 인형 선택 → 타일 클릭 → 바라볼 방향의 타일 클릭. 10초 후 적 진입!";
}
void TowerDefense::Select(int i) {
    if(i<0||i>=4||won||lost)return;selected=i;pending={};facing=2;
    message=team[i].name+": "+team[i].role+" / "+team[i].skill+
        (team[i].deployed?" · Q 기술 / R 퇴각":" · 타일 선택 후 방향 지정");
}
void TowerDefense::Cancel(){pending={};selected=-1;}
void TowerDefense::Click(Cell c){
    if(won||lost||!Board(c))return;
    if(pending.x>=0&&selected>=0){
        int dx=c.x-pending.x,dz=c.z-pending.z;
        if(dx==0&&dz==0){message="인접한 방향 타일을 누르거나 회전 후 배치 확정을 누르세요.";return;}
        facing=std::abs(dx)>=std::abs(dz)?(dx>0?0:2):(dz>0?1:3);Confirm();return;
    }
    int i=At(c);if(i>=0){Select(i);return;}
    if(!CanDeploy(selected,c)){message="배치 조건 확인: 비용 · 재배치 대기 · 지상/고지 · 빈 타일 (양끝 배치 불가)";return;}
    pending=c;message="바라볼 방향의 타일을 클릭하세요. F 회전 / Enter 확정 / 우클릭 취소";
}
void TowerDefense::Rotate(){if(pending.x>=0)facing=(facing+1)%4;}
void TowerDefense::Confirm(){
    if(won||lost||!CanDeploy(selected,pending))return;
    auto& u=team[selected];dp-=Cost(selected);u.deployed=true;u.cell=pending;u.facing=facing;
    u.deployedAt=time;u.attackStarted=u.defeatedAt=-1000;u.hp=u.maxHp;u.timer=0;u.sp=10;u.skillTime=0;pending={};
    message=u.name+" 배치 완료. 자동 공격하며 Q로 충전된 기술을 사용합니다.";
}
void TowerDefense::Remove(int i,bool refund){
    auto& u=team[i];u.defeatedAt=refund?-1000:time;u.deployed=false;u.cooldown=20;u.skillTime=u.sp=0;
    if(refund)dp=std::min(99.0f,dp+u.cost*.5f);
    for(auto& e:enemies)if(e.blockedBy==i)e.blockedBy=-1;
}
void TowerDefense::Retreat(){
    if(selected<0||!team[selected].deployed||won||lost)return;
    Remove(selected,true);message="퇴각: 비용 50% 반환. 20초 후 재배치 가능합니다.";Cancel();
}
void TowerDefense::Skill(){
    if(selected<0||won||lost)return;auto& u=team[selected];
    if(!u.deployed||u.sp<20||u.skillTime>0){message="배치된 인형의 기술이 20까지 충전되어야 합니다.";return;}
    u.sp=0;if(selected==0)dp=std::min(99.0f,dp+10);else u.skillTime=8;
    message=u.name+": "+u.skill;
}
void TowerDefense::Update(float dt){
    if(tacticalPause||won||lost)return;
    // Bound debugger stalls and integrate movement/block acquisition in small steps, including 2x speed.
    float remaining=std::clamp(dt,0.0f,.1f)*speed;
    while(remaining>0&&!won&&!lost){float step=std::min(remaining,1.0f/60);Step(step);remaining-=step;}
}
void TowerDefense::Step(float dt){
    time+=dt;dp=std::min(99.0f,dp+dt);
    for(auto& t:traces)t.time-=dt;
    traces.erase(std::remove_if(traces.begin(),traces.end(),[](const CombatTrace& t){return t.time<=0;}),traces.end());
    if(spawned<total&&time>=10+spawned*4.0f){
        Invader e;e.lane=spawned%2;e.type=(spawned%4==3)?1:0;
        e.hp=e.maxHp=e.type?100.0f:45.0f;e.x=float(lanes[e.lane][0].x);e.z=float(lanes[e.lane][0].z);
        enemies.push_back(e);++spawned;
    }
    std::array<int,4> blocked{};
    for(auto& e:enemies)if(e.alive&&e.blockedBy>=0){
        if(!team[e.blockedBy].deployed)e.blockedBy=-1;else ++blocked[e.blockedBy];
    }
    for(auto& u:team){u.cooldown=std::max(0.0f,u.cooldown-dt);if(u.deployed){
        u.timer-=dt;u.skillTime=std::max(0.0f,u.skillTime-dt);if(u.skillTime<=0)u.sp=std::min(20.0f,u.sp+dt);}}
    for(auto& e:enemies){
        if(!e.alive)continue;
        if(e.blockedBy<0)for(int i=0;i<4;++i){const auto& u=team[i];
            if(u.deployed&&blocked[i]<u.block&&std::abs(e.x-u.cell.x)+std::abs(e.z-u.cell.z)<.58f){e.blockedBy=i;++blocked[i];break;}}
        if(e.blockedBy>=0){
            e.timer-=dt;if(e.timer<=0){auto& u=team[e.blockedBy];e.timer=1.1f;
                float damage=e.type?18.0f:9.0f;if(e.blockedBy==1&&u.skillTime>0)damage*=.3f;
                u.hp=std::max(0.0f,u.hp-damage);traces.push_back({e.x,e.z,float(u.cell.x),float(u.cell.z),.15f,false});
                if(u.hp<=0)Remove(e.blockedBy,false);
            }continue;
        }
        e.progress+=dt*(e.type?.52f:.72f);const auto& lane=lanes[e.lane];
        if(e.progress>=float(lane.size()-1)){e.alive=false;++resolved;--lives;if(lives<=0){lost=true;return;}continue;}
        int n=int(e.progress);float f=e.progress-n;
        e.x=lane[n].x+(lane[n+1].x-lane[n].x)*f;e.z=lane[n].z+(lane[n+1].z-lane[n].z)*f;
    }
    for(int i=0;i<4;++i){auto& u=team[i];if(!u.deployed||u.timer>0)continue;
        if(i==3){int target=-1;float ratio=1;
            for(int j=0;j<4;++j){const auto& a=team[j];if(a.deployed&&a.hp/a.maxHp<ratio&&InRange(i,u.cell,u.facing,float(a.cell.x),float(a.cell.z))){target=j;ratio=a.hp/a.maxHp;}}
            if(target>=0){auto& a=team[target];a.hp=std::min(a.maxHp,a.hp+u.attack*(u.skillTime>0?2:1));u.timer=u.interval;u.attackStarted=time;
                traces.push_back({float(u.cell.x),float(u.cell.z),float(a.cell.x),float(a.cell.z),.25f,true});}
        }else{int target=-1;float progress=-1;
            for(int j=0;j<int(enemies.size());++j){const auto& e=enemies[j];
                if(e.alive&&(e.blockedBy==i||InRange(i,u.cell,u.facing,e.x,e.z))&&e.progress>progress){progress=e.progress;target=j;}}
            if(target>=0){auto& e=enemies[target];float damage=std::max(2.0f,u.attack*(u.skillTime>0&&i==2?1.8f:1)-(e.type?5:0));
                e.hp=std::max(0.0f,e.hp-damage);u.timer=u.interval;u.attackStarted=time;traces.push_back({float(u.cell.x),float(u.cell.z),e.x,e.z,.18f,false});
                if(e.hp<=0){e.alive=false;e.blockedBy=-1;++resolved;++kills;}}
        }
    }
    if(resolved==total&&lives>0)won=true;
}
