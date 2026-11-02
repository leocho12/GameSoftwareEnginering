#include "stdafx.h"
#include "RpgGame.h"
#include <algorithm>
#include <cmath>

namespace
{
float Distance(WorldPoint a, WorldPoint b)
{
    return std::hypot(a.x - b.x, a.z - b.z);
}
}

int RpgGame::RegionAt(WorldPoint position) const
{
    return std::clamp(
        int(std::floor((position.x - 3) / novel::VillageSpacing)), 0, novel::CorridorCount - 1);
}

int RpgGame::CurrentAlert() const
{
    return expeditionState.alerts[RegionAt(player)].Level();
}

bool RpgGame::DangerNearby() const
{
    for (const auto &enemy : enemies)
        if (enemy.hp > 0 && enemy.action == BanditAction::Attack &&
            Distance(enemy.position, player) < 12 && LineOfSight(enemy.position, player))
            return true;
    for (const auto &npc : npcs)
        if (npc.hp > 0 && npc.playerTarget && npc.action == NpcAction::Attack &&
            Distance({npc.position.x, npc.position.z}, player) < 10 &&
            LineOfSight({npc.position.x, npc.position.z}, player))
            return true;
    return false;
}

void RpgGame::EmitNoise(WorldPoint position, float radius)
{
    if (radius <= 0)
        return;
    noises.push_back({position, radius, time + 2});
    if (noises.size() > 24)
        noises.erase(noises.begin());
}

bool RpgGame::HeardNoise(WorldPoint observer, WorldPoint &source) const
{
    float best = -1;
    for (const auto &noise : noises)
    {
        float strength = noise.radius - Distance(observer, noise.position);
        if (noise.expiresAt > time && strength >= 0 && strength > best)
        {
            best = strength;
            source = noise.position;
        }
    }
    return best >= 0;
}

void RpgGame::InitializeExpedition()
{
    expeditionState = expedition::State{};
    rescueSite = {};
    noises.clear();
    footstepTimer = 0;
    // This is an existing resident, moved once by the new expedition's starting event.
    // Quest references use identity, never a vector element pointer or array index.
    auto missing = std::find_if(npcs.begin(),
                                npcs.end(),
                                [](const TownNpc &npc)
                                {
                                    return npc.homeVillage == 2 && npc.role == NpcRole::Hunter;
                                });
    if (missing == npcs.end())
    {
        expeditionState.rescue.stage = expedition::RescueStage::Failed;
        return;
    }
    WorldPoint site{(novel::Villages()[1].x + novel::Villages()[2].x) * .5f, 8};
    bool placed = false;
    for (int radius = 0; radius <= 8 && !placed; ++radius)
        for (int dx = -radius; dx <= radius && !placed; ++dx)
            for (int dz = -radius; dz <= radius && !placed; ++dz)
            {
                WorldPoint candidate{site.x + dx, site.z + dz};
                if (Fits(candidate))
                {
                    rescueSite = candidate;
                    placed = true;
                }
            }
    if (!placed)
    {
        expeditionState.rescue.stage = expedition::RescueStage::Failed;
        return;
    }
    expeditionState.rescue.actorId = missing->id;
    missing->position = {rescueSite.x, rescueSite.z};
    missing->hp = 35;
    missing->action = NpcAction::Rest;
    missing->path.clear();
    missing->rumors.clear();
    missing->mind.knowledge.clear();
    missing->Remember("missing_on_trade_road");
}

bool RpgGame::IsRescueActor(int id) const
{
    return id >= 0 && id == expeditionState.rescue.actorId &&
           expeditionState.rescue.stage != expedition::RescueStage::Completed &&
           expeditionState.rescue.stage != expedition::RescueStage::Failed;
}

void RpgGame::UpdateExpedition(float dt, float movedDistance, bool sprinting)
{
    auto &state = expeditionState;
    state.clock.Advance(dt);
    if (state.resting && (movedDistance > .00001f || DangerNearby()))
    {
        state.resting = false;
        Notify("움직이거나 위험이 다가와 휴식을 중단했습니다.");
    }
    bool sprint = sprinting && movedDistance > .00001f && !state.resting;
    state.needs.Advance(dt, state.clock.CurrentPhase(), state.resting, sprint, hp);
    for (auto &alert : state.alerts)
        alert.Advance(dt);
    noises.erase(std::remove_if(noises.begin(),
                                noises.end(),
                                [&](const Noise &noise)
                                {
                                    return noise.expiresAt <= time;
                                }),
                 noises.end());
    footstepTimer -= dt;
    if (movedDistance > .00001f && footstepTimer <= 0)
    {
        EmitNoise(player, keys['c'] ? 1.5f : sprint ? 8.0f : 3.0f);
        footstepTimer = .6f;
    }
    if (hp <= 0)
    {
        state.resting = false;
        state.rescue.stage = expedition::RescueStage::Failed;
        ClearInput();
    }
}

void RpgGame::ToggleRest()
{
    if (paused || hp <= 0 || talkingNpc >= 0 || inventory || storyJournalOpen)
        return;
    if (expeditionState.resting)
    {
        expeditionState.resting = false;
        Notify("휴식을 마쳤습니다.");
        return;
    }
    if (novel::VillageAt(player.x, player.z) < 0 || DangerNearby())
    {
        Notify("휴식은 추격을 벗어난 마을에서 가능합니다.");
        return;
    }
    ClearInput();
    expeditionState.resting = true;
    Notify("휴식 중: 피로·기력 회복. 허기·갈증 60 미만이면 체력도 회복합니다. R: 중단");
}

void RpgGame::EatRation()
{
    if (paused || hp <= 0 || talkingNpc >= 0 || storyJournalOpen)
        return;
    auto &state = expeditionState;
    if (state.rations <= 0 || state.needs.hunger < 5)
    {
        Notify(state.rations <= 0 ? "식량이 없습니다. 상인 대화 4번으로 구매하세요."
                                  : "아직 배가 고프지 않습니다.");
        return;
    }
    --state.rations;
    state.needs.hunger = std::max(0.0f, state.needs.hunger - 45);
    Notify("식량 사용: 허기 -45");
}

void RpgGame::DrinkWater()
{
    if (paused || hp <= 0 || talkingNpc >= 0 || storyJournalOpen)
        return;
    auto &state = expeditionState;
    if (state.water <= 0 || state.needs.thirst < 5)
    {
        Notify(state.water <= 0 ? "물이 없습니다. 상인 대화 5번으로 구매하세요."
                                : "아직 목이 마르지 않습니다.");
        return;
    }
    --state.water;
    state.needs.thirst = std::max(0.0f, state.needs.thirst - 55);
    Notify("식수 사용: 갈증 -55");
}

void RpgGame::LearnRescueClue(int clue)
{
    if (!expeditionState.rescue.Learn(clue))
    {
        Notify("이미 기록한 단서입니다. L에서 현재 목표를 확인하세요.");
        return;
    }
    const char *messages[] = {
        "출발지 경비: 실종자는 하멜른의 사냥꾼이며 서쪽 교역로로 나갔습니다.",
        "카디낙 상인: 카디낙과 하멜른 사이 도로 북쪽에서 부상자를 봤습니다.",
        "현장 흔적: 카디낙 동쪽 길가에 하멜른 사냥꾼의 짐과 발자국이 남았습니다."};
    Notify(messages[clue]);
}

bool RpgGame::RescueDialogue(TownNpc &npc, int choice)
{
    using expedition::RescueStage;
    auto &quest = expeditionState.rescue;
    if (IsRescueActor(npc.id))
    {
        if (choice == 1)
        {
            talkingNpc = -1;
            if (!BeginEscort())
                Notify("호위를 시작하려면 부상자에게 더 가까이 다가가세요.");
        }
        else
            Notify("1: 호위 시작/재개 · 부상자를 아플리아 외곽 광장까지 데려가세요.");
        return true;
    }
    if (choice != 3)
        return false;
    if (npc.role == NpcRole::Guard && npc.homeVillage == 0)
    {
        if (quest.stage == RescueStage::Returned &&
            novel::VillageAt(npc.position.x, npc.position.z) == 0 && quest.Report())
        {
            silver += 12;
            ++expeditionState.progress.rescues;
            auto rescued = std::find_if(npcs.begin(),
                                        npcs.end(),
                                        [&](const TownNpc &person)
                                        {
                                            return person.id == quest.actorId;
                                        });
            if (rescued != npcs.end())
                rescued->walkSpeed = 1.65f;
            RecordEconomy("rescue_reward", -2, -1, 12);
            Notify("구조 보고 완료 · 은화 +12 · 구조자 실적 획득. 보상은 한 번만 지급됩니다.");
        }
        else if (quest.stage == RescueStage::Completed)
            Notify("이미 구조 보상을 받았습니다. 다음 탐사를 준비하세요.");
        else
            LearnRescueClue(0);
    }
    else if (npc.role == NpcRole::Merchant && npc.homeVillage == 1)
        LearnRescueClue(1);
    else if (npc.role == NpcRole::Hunter && npc.homeVillage == 2)
        LearnRescueClue(2);
    else
        Notify("새 단서는 없습니다. 출발지 경비나 카디낙 상인에게 문의하세요.");
    return true;
}

bool RpgGame::BeginEscort()
{
    using expedition::RescueStage;
    auto &quest = expeditionState.rescue;
    auto actor = std::find_if(npcs.begin(),
                              npcs.end(),
                              [&](const TownNpc &npc)
                              {
                                  return npc.id == quest.actorId;
                              });
    if (actor == npcs.end() || quest.stage == RescueStage::Completed ||
        quest.stage == RescueStage::Failed || quest.stage == RescueStage::Returned ||
        Distance(player, {actor->position.x, actor->position.z}) > 2.4f ||
        !LineOfSight(player, {actor->position.x, actor->position.z}))
        return false;
    if (actor->hp <= 0)
    {
        if (potions <= 0)
        {
            Notify("호위 대상을 일으키려면 회복약 1개가 필요합니다.");
            return true;
        }
        --potions;
        actor->hp = 35;
    }
    quest.stage = RescueStage::Escorting;
    quest.contacted = true;
    rescueSite = {actor->position.x, actor->position.z};
    actor->walkSpeed = novel::PlayerSpeed;
    actor->path.clear();
    actor->pathTimer = 0;
    expeditionState.resting = false;
    Notify("호위 시작: 14거리 이상 떨어지면 기다립니다. 가까이에서 E로 재개하세요.");
    return true;
}

void RpgGame::UpdateRescue(float dt)
{
    using expedition::RescueStage;
    auto &quest = expeditionState.rescue;
    if (hp <= 0)
    {
        quest.stage = RescueStage::Failed;
        return;
    }
    if (quest.stage != RescueStage::Escorting)
        return;
    auto actor = std::find_if(npcs.begin(),
                              npcs.end(),
                              [&](const TownNpc &npc)
                              {
                                  return npc.id == quest.actorId;
                              });
    if (actor == npcs.end())
    {
        quest.stage = RescueStage::Failed;
        Notify("호위 대상이 없어 의뢰를 진행할 수 없습니다.");
        return;
    }
    WorldPoint position{actor->position.x, actor->position.z};
    if (Distance(position, player) <= 14 && LineOfSight(player, position))
        rescueSite = position;
    if (actor->hp <= 0 || Distance(position, player) > 14)
    {
        quest.stage = RescueStage::Located;
        actor->path.clear();
        Notify("호위 대상이 멈췄습니다. 마지막 위치로 돌아가 E로 호위를 재개하세요.");
        return;
    }
    if (talkingNpc >= 0 && npcs[talkingNpc].id == actor->id)
        return;
    if (Distance(position, player) > 1.5f)
        MoveNpc(*actor, player, dt);
    const auto &home = novel::Villages()[0];
    if (Distance(player, {home.x, home.z}) <= 5 &&
        Distance({actor->position.x, actor->position.z}, {home.x, home.z}) <= 5)
    {
        quest.stage = RescueStage::Returned;
        actor->path.clear();
        actor->moving = false;
        Notify("함께 안전하게 귀환했습니다. 출발지 경비에게 F → 3으로 보고하세요.");
    }
}

bool RpgGame::QuestReady() const
{
    return expeditionState.rescue.stage == expedition::RescueStage::Returned;
}

std::string RpgGame::QuestObjective() const
{
    using expedition::RescueStage;
    switch (expeditionState.rescue.stage)
    {
    case RescueStage::Investigating:
        return "실종 사냥꾼: 서로 다른 단서 " + std::to_string(expeditionState.rescue.ClueCount()) +
               "/2";
    case RescueStage::Located:
        return "실종 사냥꾼: 현장 접촉 · 가까이에서 E로 호위";
    case RescueStage::Escorting:
        return "호위 중: 함께 아플리아 외곽 광장으로 귀환";
    case RescueStage::Returned:
        return "귀환 완료: 출발지 경비에게 F → 3 보고";
    case RescueStage::Completed:
        return "구조 의뢰 완료 · 생존과 숙련을 이어가는 자유 탐사";
    case RescueStage::Failed:
        return "탐사 실패 · Enter로 같은 시드 재시작";
    }
    return {};
}

std::string RpgGame::QuestHint() const
{
    using expedition::RescueStage;
    auto stage = expeditionState.rescue.stage;
    if (stage == RescueStage::Located)
    {
        if (expeditionState.rescue.contacted)
            return "마지막 목격: (" + std::to_string(int(rescueSite.x)) + ", " +
                   std::to_string(int(rescueSite.z)) + ") · 가까이에서 E로 호위 재개";
        return "카디낙~하멜른 사이 도로 북쪽. 가까이에서 E로 호위를 시작하세요.";
    }
    if (stage == RescueStage::Escorting)
        return "달리면 동행자가 뒤처집니다. 위험을 피해 천천히 귀환하세요.";
    return "F 대화 → 3 단서/보고 · E 현장 조사/호위 · L 일지 · R 마을 휴식";
}

void RpgGame::UpdateNpcRoutine(TownNpc &npc)
{
    auto phase = expeditionState.clock.CurrentPhase();
    bool night = phase == expedition::Phase::DeepNight || phase == expedition::Phase::Evening;
    int minute = expeditionState.clock.MinuteOfDay();
    bool meal = (minute >= 420 && minute < 450) || (minute >= 660 && minute < 690) ||
                (minute >= 1080 && minute < 1110);
    int key = int(phase) * 2 + (meal ? 1 : 0);
    if (npc.routineKey == key)
        return;
    npc.routineKey = key;
    bool nightGuard = npc.role == NpcRole::Guard && npc.id % 2 == 0;
    npc.routine = night && !nightGuard ? NpcRoutine::Sleep
                  : meal               ? NpcRoutine::Meal
                                       : NpcRoutine::Work;
    npc.serviceOpen = npc.routine == NpcRoutine::Work;
    int currentVillage = novel::VillageAt(npc.position.x, npc.position.z);
    int routineVillage =
        npc.routine != NpcRoutine::Work && currentVillage >= 0 ? currentVillage : npc.homeVillage;
    const auto &village = novel::Villages()[routineVillage];
    WorldPoint desired = npc.routine == NpcRoutine::Sleep  ? WorldPoint{village.x - 5, 7}
                         : npc.routine == NpcRoutine::Meal ? WorldPoint{village.x + 1, 7}
                                                           : WorldPoint{npc.home.x, npc.home.z};
    // One stable, walkable activity slot per resident. Movement still uses the existing path owner.
    desired.x += float(npc.id % 3) - 1;
    desired.z += float((npc.id / 3) % 2);
    npc.routineGoal = npc.home;
    for (int radius = 0; radius <= 3; ++radius)
        for (int dx = -radius; dx <= radius; ++dx)
            for (int dz = -radius; dz <= radius; ++dz)
            {
                WorldPoint candidate{desired.x + dx, desired.z + dz};
                if (Fits(candidate))
                {
                    npc.routineGoal = {candidate.x, candidate.z};
                    return;
                }
            }
}
