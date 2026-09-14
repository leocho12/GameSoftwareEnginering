#include "stdafx.h"
#include "TutorialGame.h"
#include <algorithm>
#include <cmath>
#include <queue>

TutorialGame::TutorialGame() { Reset(); }

void TutorialGame::Reset() {
    phase = Phase::Briefing; defense.Reset();
    defenseDemo=false;
    yardCleared=false;wildEncounter=-1;animationTime=wildlifeTick=0;patrolStep=0;
    npcs = {{
        {"보급 담당", "보급 담당: 이 중계소가 끊기면 주민과 수송대가 고립됩니다. 상자의 복구 부품을 가져가 주세요.",{1,9}},
        {"정비 인형", "정비 인형: 소체를 고치는 것과 잃은 기억을 되찾는 건 달라요. 동료들이 무사히 돌아오게 해 주세요.",{5,10}},
        {"순찰대원", "순찰대원: 오염 위험 때문에 동쪽 순찰을 줄였습니다. 수로 너머 야생 개체를 조심하세요.",{14,10}},
        {"통신병", "통신병: 다른 그리폰 부대도 각자 담당 구역에 묶여 있습니다. 우선 우리 구역의 연락망을 복구해야 합니다.",{17,6}},
        {"피난민", "피난민: 오염을 피해 여기까지 왔어요. 중계소가 살아나면 가족에게 소식을 전할 수 있겠죠?",{23,6}},
        {"수송 기사", "수송 기사: 계약서에는 물자 운송이라고만 적혀 있지만, 저 상자들을 기다리는 사람들이 있습니다.",{24,16}},
        {"민간 기술자", "민간 기술자: 전쟁이 끝나도 무너진 길은 저절로 돌아오지 않습니다. 사람도 인형도 이 다리를 써야 해요.",{10,19}},
        {"초소 경비", "초소 경비: 회사 명령으로 지키는 초소지만, 이곳 사람들의 얼굴은 우리가 매일 봅니다. 잘 부탁드립니다.",{16,22}}
    }};
    wildlife = {{{{16,15},{16,15},16,15,12},{{24,11},{24,11},24,11,12},{{25,21},{25,21},25,21,12},{{13,20},{13,20},13,20,12}}};
    units = {{
        {"알파", {2,9}, 2,9,16,16,true,false},
        {"브라보", {3,10}, 3,10,16,16,true,false},
        {"경비 01", {7,4}, 7,4,8,8,false,false},
        {"경비 02", {9,4}, 9,4,8,8,false,false},
        {"경비 03", {10,2},10,2,8,8,false,false}
    }};
    for(int i=2;i<5;++i)units[i].hp=0;
    active = 0; paused = hasParts = false;
    elapsed = phaseTime = effectTime = 0;
    shots = 0; effectFrom = effectTo = -1; healingEffect = false;

    interaction = Cell{};
    route.clear(); Cancel();
    message = "지휘부: 담당 구역의 주민과 보급 수송대가 연락 두절입니다. 인형들과 중계소 통신을 복구하세요.";
}

void TutorialGame::Start() {
    if (phase != Phase::Briefing) return;
    phase = Phase::Supply; phaseTime = 0;
    message = "노란 보급함을 클릭하세요. 자동으로 접근해 조사합니다. 가까이에서는 E 키도 사용할 수 있습니다.";
}

bool TutorialGame::Inside(Cell c) const { return c.x >= 0 && c.z >= 0 && c.x < Width && c.z < Depth; }
int TutorialGame::Distance(Cell a, Cell b) const { return std::abs(a.x-b.x) + std::abs(a.z-b.z); }
float TutorialGame::Height(Cell c) const { return (phase==Phase::Battle||phase==Phase::Defeat) ? (defense.High(c)?.35f:0.0f) : (c.x >= 8 && c.x<=13 && c.z <= 3 ? 0.65f : 0.0f); }
bool TutorialGame::Water(Cell c) const { return c.x>=19 && c.x<=21 && c.z>=8 && !(c.z==13 || c.z==14); }
int TutorialGame::NpcAt(Cell c) const { for(int i=0;i<int(npcs.size());++i)if(npcs[i].cell==c)return i;return -1; }
bool TutorialGame::Wall(Cell c) const {
    return !Inside(c) || Water(c) || NpcAt(c)>=0 || c == SupplyCell || c == TerminalCell ||
        (c.x>=16&&c.x<=18&&c.z>=2&&c.z<=4) || (c.x>=24&&c.x<=26&&c.z>=17&&c.z<=19) ||
        (c.x == 6 && (c.z == 4 || c.z == 5)) || (c.x == 9 && c.z == 6) ||
        (c.x <= 1 && c.z <= 3) || (c.x == 12 && c.z >= 7 && c.z <= 9);
}
bool TutorialGame::CanStep(Cell a, Cell b) const {
    if (Wall(b) || Distance(a,b) != 1) return false;
    if (Height(a) == Height(b)) return true;
    // The elevated relay apron is reachable only by the marked two-tile stair.
    return a.x == b.x && (a.x == 8 || a.x == 9) &&
        ((a.z == 3 && b.z == 4) || (a.z == 4 && b.z == 3));
}
int TutorialGame::UnitAt(Cell c, int except) const {
    for (int i=0; i<5; ++i) {
        if (i != except && units[i].hp > 0 && units[i].cell == c) return i;
    }
    return -1;
}
bool TutorialGame::Walkable(Cell c, int mover) const {
    if (Wall(c) || UnitAt(c,mover) >= 0) return false;
    if (phase == Phase::Supply && (c.z < 6 || c.x>13 || c.z>11)) return false;
    return true;
}
std::vector<Cell> TutorialGame::Path(Cell from, Cell to, int mover, int limit) const {
    if (!Inside(from) || !Walkable(to,mover) || from == to) return {};
    std::array<int, Width*Depth> previous, distance;
    previous.fill(-1); distance.fill(-1);
    const auto index = [](Cell p) { return p.z*Width+p.x; };
    std::queue<Cell> q; q.push(from); distance[index(from)] = 0;
    const Cell directions[] = {{1,0},{-1,0},{0,1},{0,-1}};
    while (!q.empty()) {
        Cell c=q.front(); q.pop();
        if (c == to) break;
        for (Cell d: directions) {
            Cell n{c.x+d.x,c.z+d.z};
            if (!Walkable(n,mover) || !CanStep(c,n)) continue;
            int ni=index(n), ci=index(c);
            if (distance[ni] >= 0 || distance[ci] >= limit) continue;
            previous[ni]=ci; distance[ni]=distance[ci]+1; q.push(n);
        }
    }
    if (distance[index(to)] < 0) return {};
    std::vector<Cell> result;
    for (int i=index(to); i != index(from); i=previous[i]) result.push_back({i%Width,i/Width});
    std::reverse(result.begin(),result.end());
    return result;
}

void TutorialGame::MoveToObject(Cell object) {
    if (Distance(units[0].cell,object)<=1) { interaction=object; TryInteraction(); return; }
    std::vector<Cell> best;
    const Cell ds[]={{1,0},{-1,0},{0,1},{0,-1}};
    for (Cell d:ds) {
        auto p=Path(units[0].cell,{object.x+d.x,object.z+d.z},0);
        if (!p.empty() && (best.empty() || p.size()<best.size())) best=p;
    }
    if (best.empty()) { message="해당 대상까지 이동할 수 있는 경로가 없습니다."; return; }
    interaction=object; route.assign(best.begin(),best.end());
}
void TutorialGame::TryInteraction() {
    Cell c=interaction; interaction=Cell{};
    if (Distance(units[0].cell,c)>1) return;
    int npc=NpcAt(c);if(npc>=0){message=npcs[npc].dialogue;return;}
    if (c==SupplyCell && phase==Phase::Supply) {
        hasParts=true; phase=Phase::Approach;
        message="부품 확보. 소대가 방어 준비를 마쳤습니다. 중계소 마당으로 이동해 주민의 연락망을 지켜 주세요.";
    } else if (c==TerminalCell && phase==Phase::Restore && hasParts) {
        phase=Phase::Complete; phaseTime=0;
        message="통신병: 주민 연락망 복구 확인. 돌아오지 않은 수송차의 수색 요청도 들어왔습니다. 다음 보고에 첨부하겠습니다.";
    }
}
void TutorialGame::Interact() {
    if (paused || Busy()) return;
    if(phase!=Phase::Battle){for(const auto& npc:npcs)if(Distance(units[0].cell,npc.cell)<=1){interaction=npc.cell;TryInteraction();return;}}
    if (phase==Phase::Supply && Distance(units[0].cell,SupplyCell)<=1) { interaction=SupplyCell; TryInteraction(); }
    else if (phase==Phase::Restore && Distance(units[0].cell,TerminalCell)<=1) { interaction=TerminalCell; TryInteraction(); }
    else message="표시된 대상 가까이 이동하거나 대상을 클릭해 자동 접근하세요.";
}
void TutorialGame::BeginBattle() {
    fieldCheckpoint=units;route.clear();interaction={};Cancel();
    phase=Phase::Battle;phaseTime=0;defense.Reset(wildEncounter>=0);
    message=defense.message;
}

void TutorialGame::StartDefenseDemo() {
    if(phase!=Phase::Briefing)return;
    Reset();defenseDemo=true;hasParts=true;
    // A valid field checkpoint lets the demo continue through relay restoration.
    units[0].cell={8,5};units[0].x=8;units[0].z=5;
    units[1].cell={9,5};units[1].x=9;units[1].z=5;
    BeginBattle();defense.dp=50;defense.tacticalPause=true;
    defense.message="방어전 체험: 카드 → 타일 → 방향을 선택해 배치하세요. 준비 후 Space 또는 전투 재개.";
    message=defense.message;
}

void TutorialGame::RetryBattle() {
    if(phase!=Phase::Defeat)return;
    phase=Phase::Battle;paused=false;defense.Reset(wildEncounter>=0);message=defense.message;
    if(defenseDemo&&wildEncounter<0){defense.dp=50;defense.tacticalPause=true;}
}

void TutorialGame::ContinueExploring(){
    if(phase!=Phase::Complete)return;phase=Phase::Explore;
    message="자유 탐험: 지도를 확대·축소하며 주민과 대화하세요. 야생 개체를 클릭하거나 접근하면 선택 전투가 시작됩니다.";
}
void TutorialGame::BeginWildlife(int index){
    if(phase!=Phase::Explore||wildlife[index].hp<=0)return;
    wildEncounter=index;BeginBattle();
}

void TutorialGame::Click(Cell c) {
    if(paused)return;
    if(phase==Phase::Battle){defense.Click(c);message=defense.message;return;}
    if(Busy()||!Inside(c))return;
    if (phase==Phase::Supply || phase==Phase::Approach || phase==Phase::Restore || phase==Phase::Explore) {
        if(NpcAt(c)>=0){MoveToObject(c);return;}
        if(phase==Phase::Explore){for(int i=0;i<int(wildlife.size());++i)if(wildlife[i].hp>0&&wildlife[i].cell==c){
            if(Distance(units[0].cell,c)<=5)BeginWildlife(i);else MoveToObject(c);return;}}
        if (c==SupplyCell) {
            if (hasParts) message="이미 복구 부품을 확보했습니다.";
            else MoveToObject(c);
            return;
        }
        if (c==TerminalCell) {
            if(phase==Phase::Explore){message="통신 단말: 주민 연락망 정상. 연락 두절 수송차의 수색 요청 접수. 후속 임무 배정 대기 중.";return;}
            if (phase!=Phase::Restore) message="중계소를 복구하기 전에 방어 작전을 완료해야 합니다.";
            else MoveToObject(c);
            return;
        }
        auto p=Path(units[0].cell,c,0);
        if (p.empty()) { message="이동할 수 없습니다. 장애물을 피해 이동하세요. 마당 진입 전에는 부품이 필요합니다."; return; }
        interaction=Cell{}; route.assign(p.begin(),p.end()); return;
    }
}
void TutorialGame::Confirm(){if(phase==Phase::Battle&&!paused){defense.Confirm();message=defense.message;}}

void TutorialGame::Cancel(){preview={};previewPath.clear();target=-1;if(phase==Phase::Battle)defense.Cancel();}

void TutorialGame::Wait(){if(phase==Phase::Battle&&!paused)defense.tacticalPause=!defense.tacticalPause;}

void TutorialGame::TogglePause() { if (phase!=Phase::Briefing && phase!=Phase::Complete && phase!=Phase::Defeat) paused=!paused; }

void TutorialGame::Update(float dt) {
    if(!paused)animationTime+=std::min(dt,.1f);
    if (phase==Phase::Complete) { phaseTime+=dt; return; }
    if (paused || phase==Phase::Briefing || phase==Phase::Defeat) return;
    if(phase!=Phase::Explore && wildEncounter<0 && !(phase==Phase::Battle&&defense.tacticalPause))elapsed+=dt;
    phaseTime+=dt;
    if(phase==Phase::Battle){
        defense.Update(dt);message=defense.message;
        if(defense.lost){phase=Phase::Defeat;return;}
        if(defense.won){
            units=fieldCheckpoint;active=0;route.clear();
            if(wildEncounter>=0){wildlife[wildEncounter].hp=0;wildEncounter=-1;phase=Phase::Explore;message="방어 성공. 필드 탐험을 계속합니다.";}
            else{yardCleared=true;phase=Phase::Restore;message="소대: 진입로 확보. 단말을 복구하면 주민과 수송대에 연락할 수 있습니다.";}
        }
        return;
    }
    // Track real active time, but bound animation steps after a debugger stop or slow frame.
    dt=std::min(dt,.1f);
    if(phase==Phase::Explore){
        wildlifeTick+=dt;
        for(int i=0;i<int(wildlife.size());++i)if(wildlife[i].hp>0&&Distance(units[0].cell,wildlife[i].cell)<=3){BeginWildlife(i);return;}
        if(wildlifeTick>1.8f){
            wildlifeTick=0;++patrolStep;const Cell ds[]={{1,0},{0,1},{-1,0},{0,-1}};
            for(int i=0;i<int(wildlife.size());++i){auto& w=wildlife[i];if(w.hp<=0)continue;Cell d=ds[(patrolStep+i)%4],c{w.cell.x+d.x,w.cell.z+d.z};
                bool occupied=false;for(int j=0;j<int(wildlife.size());++j)if(i!=j&&wildlife[j].hp>0&&wildlife[j].cell==c)occupied=true;
                if(!occupied&&!Wall(c)&&UnitAt(c)<0&&CanStep(w.cell,c)&&Distance(c,w.home)<=2)w.cell=c;}
        }
        for(auto& w:wildlife){w.x+=(w.cell.x-w.x)*std::min(1.0f,dt*5);w.z+=(w.cell.z-w.z)*std::min(1.0f,dt*5);}
    }
    if (!route.empty()) {
        auto& u=units[phase==Phase::Battle ? active : 0]; Cell next=route.front();
        float dx=next.x-u.x, dz=next.z-u.z, d=std::sqrt(dx*dx+dz*dz), step=dt*4.5f;
        if (d<=step) { u.x=float(next.x); u.z=float(next.z); u.cell=next; route.pop_front(); }
        else { u.x+=dx/d*step; u.z+=dz/d*step; }
        if (phase==Phase::Approach && u.cell.z<=6 && u.cell.x<=13) { BeginBattle(); return; }
        if (route.empty()) TryInteraction();
        return;
    }

}
std::string TutorialGame::Objective() const {
    switch (phase) {
    case Phase::Explore: return "자유 탐험 / 주민과 대화 · 야생 개체 조사";
    case Phase::Briefing: return "끊어진 중계소의 통신을 복구하세요";
    case Phase::Supply: return "01 / 복구 부품 확보";
    case Phase::Approach: return "02 / 중계소 마당 진입";
    case Phase::Battle: return wildEncounter>=0 ? "선택 전투 / 외곽 방어" : "02 / 중계소 방어";
    case Phase::Restore: return "03 / 통신 단말 복구";
    case Phase::Complete: return "임무 완료 / 통신 복구";
    default: return "소대 전투 불능 / 재도전 가능";
    }
}
