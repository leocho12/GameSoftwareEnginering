#include "stdafx.h"
#include "RpgGame.h"
#include "Platform.h"
#include <algorithm>
#include <cmath>
#include <chrono>
#include <fstream>
#include <locale>
#include <queue>

namespace
{
struct CpuTimer
{
    float &total;
    std::chrono::steady_clock::time_point start = std::chrono::steady_clock::now();

    ~CpuTimer()
    {
        total += std::chrono::duration<float, std::milli>(std::chrono::steady_clock::now() - start)
                     .count();
    }
};

float Distance(DollPoint a, DollPoint b)
{
    return std::hypot(a.x - b.x, a.z - b.z);
}

WorldPoint Point(DollPoint p)
{
    return {p.x, p.z};
}
}

bool TownNpc::InTown(float x, float z)
{
    return novel::VillageAt(x, z) >= 0;
}

const std::vector<NpcBuilding> &TownNpc::Buildings()
{
    static const std::vector<NpcBuilding> buildings = []
    {
        std::vector<NpcBuilding> result;
        const NpcBuilding local[] = {{-4, -4, 4, 3, "연금술 상점"},
                                     {4, -4, 4, 3, "치유소"},
                                     {11, -4, 3, 3, "경비 초소"},
                                     {-4, 10, 4, 3, "여관"},
                                     {4, 10, 4, 3, "공동 식당"},
                                     {11, 10, 3, 3, "교역 창고"}};
        for (const auto &v : novel::Villages())
            for (auto building : local)
            {
                building.x += v.x - 3;
                result.push_back(building);
            }
        return result;
    }();
    return buildings;
}

const char *TownNpc::RoleName(NpcRole role)
{
    const char *names[] = {
        "merchant", "medic", "guard", "hunter", "outlaw", "bully", "swindler", "helper"};
    return names[int(role)];
}

const char *TownNpc::ActionName(NpcAction action)
{
    const char *names[] = {"work", "patrol", "attack", "retreat", "help", "rest", "rob", "travel"};
    return names[int(action)];
}

const char *TownNpc::RoleLabel(NpcRole role)
{
    const char *names[] = {"상인", "치유사", "경비", "사냥꾼", "무법자", "악당", "사기꾼", "주민"};
    return names[int(role)];
}

const char *TownNpc::ActionLabel(NpcAction action)
{
    const char *names[] = {
        "업무", "순찰", "교전", "위험 회피", "동료 지원", "휴식·집중", "약탈", "마을 간 이동"};
    return names[int(action)];
}

void TownNpc::Remember(const std::string &event)
{
    memory.push_back(event);
    if (memory.size() > 16)
        memory.pop_front();
}

void RpgGame::InitializeTown()
{
    npcUpdateCpuMs = npcDecisionCpuMs = npcPathCpuMs = npcMetricsTimer = 0;
    npcLogCpuMs = 0;
    npcDecisions = npcPathReplans = 0;
    npcDebug = false;
    townRadioConnected = false;
    townCoordinationTimer = 0;
    npcs.clear();
    talkingNpc = -1;

    struct Profile
    {
        const char *name;
        NpcRole role;
        float x, z;
    };

    const Profile profiles[] = {
        {"엘린", NpcRole::Merchant, -5, -1}, {"마르코", NpcRole::Merchant, -2, -1},
        {"테오", NpcRole::Merchant, 11, 7},  {"세라", NpcRole::Medic, 3, 0},
        {"이리스", NpcRole::Medic, 5, -1},   {"에른", NpcRole::Guard, 11, -1},
        {"루카", NpcRole::Guard, 14, 3},     {"오웬", NpcRole::Guard, 3, 14},
        {"핀", NpcRole::Hunter, 12, 5},      {"레온", NpcRole::Hunter, 6, 12},
        {"미아", NpcRole::Hunter, -7, 4},    {"까마귀", NpcRole::Outlaw, 13, 8},
        {"브룬", NpcRole::Outlaw, 8, 13},    {"볼트", NpcRole::Bully, 13, 6},
        {"크로", NpcRole::Bully, 6, 13},     {"로웰", NpcRole::Swindler, -5, 5},
        {"실비", NpcRole::Swindler, 8, 6},   {"니아", NpcRole::Helper, 3, 6},
        {"로제", NpcRole::Helper, 6, 7},     {"베른", NpcRole::Helper, -5, 7}};
    for (int slot = 0; slot < novel::VillageCount * novel::ResidentsPerVillage; ++slot)
    {
        const int i = slot % novel::ResidentsPerVillage;
        const int village = slot / novel::ResidentsPerVillage;
        const auto &settlement = novel::Villages()[village];
        TownNpc npc;
        npc.id = slot;
        npc.homeVillage = village;
        npc.courier = i == 1 || i == 2;
        npc.travelWait = 5 + i * 4;
        npc.name = profiles[i].name + std::string(" ") + std::to_string(village + 1);
        npc.role = profiles[i].role;
        npc.silver = npc.role == NpcRole::Merchant                               ? 20
                     : npc.role == NpcRole::Outlaw || npc.role == NpcRole::Bully ? 2
                                                                                 : 8;
        npc.potionStock = npc.role == NpcRole::Merchant ? 5 : 0;
        npc.medicalSupplies = npc.role == NpcRole::Medic ? 5 : 0;
        npc.rationStock = npc.role == NpcRole::Merchant ? 6 : 0;
        npc.waterStock = npc.role == NpcRole::Merchant ? 8 : 0;
        npc.home = npc.position = {profiles[i].x + settlement.x - 3, profiles[i].z};
        // Spawn on a verified walkable point near the requested workplace.
        for (int d = 0; d < 5 && !Fits(Point(npc.position)); ++d)
            npc.position.x += 1;
        npc.home = npc.position;
        auto &t = npc.traits;
        t.aggression = .15f;
        t.coverPreference = .7f;
        t.rescuePriority = .35f;
        t.commandObedience = .45f;
        t.autonomy = .6f;
        if (npc.role == NpcRole::Medic || npc.role == NpcRole::Helper)
        {
            t.rescuePriority = .95f;
            t.teamSupport = .9f;
            t.civilianPriority = .95f;
            t.autonomy = .85f;
        }
        if (npc.role == NpcRole::Guard || npc.role == NpcRole::Hunter)
        {
            t.aggression = .85f;
            t.riskTolerance = .8f;
            t.commandObedience = .8f;
            t.autonomy = .85f;
            t.selfPreservation = .4f;
        }
        if (npc.role == NpcRole::Outlaw || npc.role == NpcRole::Bully)
        {
            t.aggression = .9f;
            t.commandObedience = .1f;
            t.rescuePriority = .05f;
            t.teamSupport = .1f;
            t.civilianPriority = .05f;
            t.autonomy = .9f;
        }
        if (npc.role == NpcRole::Swindler)
        {
            t.selfPreservation = .95f;
            t.retreatPreference = .95f;
            t.commandObedience = .2f;
        }
        t.aggression = std::clamp(t.aggression + (i % 3 - 1) * .035f, 0.0f, 1.0f);
        t.objectivePriority = .5f + .08f * (i % 5);
        t.ammoConservation = .35f + .1f * (i % 4);
        if (village == 2)
            t.teamSupport = std::min(1.0f, t.teamSupport + .12f);
        if (village == 3)
            t.objectivePriority = std::min(1.0f, t.objectivePriority + .1f);
        if (village == 4)
            t.selfPreservation = std::min(1.0f, t.selfPreservation + .1f);
        npc.personality = t;
        npc.resistance = .65f + .04f * (i % 5);
        npc.decisionTimer = .025f * i;
        npc.perceptionTimer = .01f * i;
        npc.radio = false;
        npc.mind.relationships[-1] = {npc.trust, npc.respect, 0};
        npcs.push_back(npc);
    }
}

void RpgGame::DecideNpc(TownNpc &npc)
{
    const auto decisionStarted = std::chrono::steady_clock::now();
    const auto previousAction = npc.action;
    ++npcDecisions;
    npc.enemyTarget = npc.allyTarget = npc.criminalTarget = -1;
    npc.playerTarget = false;
    float enemyDistance = 12;
    for (const auto &entry : npc.mind.knowledge)
    {
        const auto &belief = entry.second;
        if (belief.kind != NpcFact::Enemy || belief.hp <= 0 || belief.Confidence(time) <= .25f)
            continue;
        float d = Distance(npc.position, belief.position);
        if (d < enemyDistance)
        {
            npc.enemyTarget = belief.subject;
            enemyDistance = d;
        }
    }
    if (npc.mind.assignedPatient >= 0 &&
        npc.mind.Find(NpcFact::Ally, npc.mind.assignedPatient, time))
        npc.allyTarget = npc.mind.assignedPatient;
    const auto *commander = npc.mind.Find(NpcFact::Commander, -1, time);
    npc.playerTarget = npc.serviceCooldown <= 0 && (npc.role == NpcRole::Outlaw || npc.hostile) &&
                       commander &&
                       !TownNpc::InTown(commander->position.x, commander->position.z) &&
                       Distance(npc.position, commander->position) < 6;
    float danger = (npc.enemyTarget >= 0 ? 1 - enemyDistance / 12 : 0) + (1 - npc.hp / 100);
    const auto &t = npc.traits;
    auto &needs = npc.mind.needs;
    needs.survive = std::clamp(danger * .5f + (1 - npc.hp / 100), 0.0f, 1.0f);
    needs.protectAlly = npc.allyTarget >= 0 ? 1.0f : 0.0f;
    needs.completeObjective = t.objectivePriority;
    needs.obeyCommander =
        time < npc.mind.commandUntil ? t.commandObedience * npc.trust * npc.respect : 0;
    npc.helpTraitScore = npc.helpRelationshipScore = npc.helpMemoryScore = npc.helpRiskScore = 0;
    auto &s = npc.scores;
    s.fill(-1);
    s[int(NpcAction::Work)] = .2f + t.objectivePriority * .3f;
    s[int(NpcAction::Patrol)] =
        (npc.role == NpcRole::Guard || npc.role == NpcRole::Hunter || npc.role == NpcRole::Outlaw)
            ? .55f
            : .1f;
    if (npc.enemyTarget >= 0 || npc.playerTarget)
        s[int(NpcAction::Attack)] =
            npc.ammo > 0 ? t.aggression * .7f + t.riskTolerance * .2f - danger * .1f : -1;
    if (danger > .25f)
        s[int(NpcAction::Retreat)] = t.retreatPreference * .3f + t.selfPreservation * .3f +
                                     danger * .35f + (npc.hp < 25 ? 1 : 0);
    if (npc.allyTarget >= 0 && t.autonomy > .5f)
    {
        auto &relationship = npc.mind.relationships[npc.allyTarget];
        npc.helpTraitScore =
            t.rescuePriority * .55f + t.teamSupport * .35f + t.civilianPriority * .1f;
        npc.helpRelationshipScore = relationship.debt * .2f + relationship.trust * .05f;
        npc.helpMemoryScore = npc.mind.RecallRescue(npc.allyTarget, time);
        npc.helpRiskScore = -danger * (1 - t.riskTolerance) * .2f;
        s[int(NpcAction::Help)] = npc.helpTraitScore + npc.helpRelationshipScore +
                                  npc.helpMemoryScore + npc.helpRiskScore;
    }
    if (npc.ammo == 0 || npc.hp < 60)
        s[int(NpcAction::Rest)] =
            .4f + t.coverPreference * .3f + (npc.ammo == 0 ? .65f * t.ammoConservation : 0);
    // Routine priorities only override peaceful work, never a known threat or injury.
    if (npc.enemyTarget < 0 && !npc.playerTarget && npc.allyTarget < 0 && npc.hp >= 60 &&
        npc.routine != NpcRoutine::Work)
    {
        s[int(NpcAction::Patrol)] = -1;
        s[int(NpcAction::Work)] = npc.routine == NpcRoutine::Meal ? 1.0f : -1;
        s[int(NpcAction::Rest)] = npc.routine == NpcRoutine::Sleep ? 1.0f : -1;
    }
    int previousRobberyTarget = npc.robberyTarget;
    if (npc.routine == NpcRoutine::Work)
        DecideRobbery(npc);
    else
        npc.robberyTarget = -1;
    if (previousRobberyTarget != npc.robberyTarget)
        npc.robberyProgress = 0;
    if (npc.role == NpcRole::Guard)
        for (const auto &entry : npc.mind.knowledge)
        {
            const auto &fact = entry.second;
            if (fact.kind == NpcFact::Crime && fact.hp > 0 && fact.Confidence(time) > .25f)
            {
                npc.criminalTarget = fact.subject;
                npc.goal = fact.position;
                s[int(NpcAction::Attack)] = 1.1f;
                break;
            }
        }
    if (npc.robbedUntil > time)
        s[int(NpcAction::Retreat)] = 1.3f;
    s[int(NpcAction::Patrol)] += needs.obeyCommander;
    auto next = NpcAction(std::distance(s.begin(), std::max_element(s.begin(), s.end())));
    npc.mind.selectionReason = "highest_utility";
    if (s[int(npc.action)] >= 0 && s[int(next)] - s[int(npc.action)] < .08f)
    {
        next = npc.action;
        npc.mind.selectionReason = "hysteresis";
    }
    if (npc.hp < 20)
    {
        next = NpcAction::Retreat;
        npc.mind.selectionReason = "critical_health_override";
    }
    if (next == NpcAction::Help)
    {
        const auto *belief = npc.mind.Find(NpcFact::Ally, npc.allyTarget, time);
        if (belief)
            npc.goal = belief->position;
    }
    if (next == NpcAction::Attack && npc.criminalTarget < 0)
    {
        const auto *belief =
            npc.playerTarget ? commander : npc.mind.Find(NpcFact::Enemy, npc.enemyTarget, time);
        if (belief)
            npc.goal = belief->position;
    }
    npc.mind.task = next == NpcAction::Rob       ? "approach_then_rob"
                    : next == NpcAction::Help    ? "approach_then_heal"
                    : next == NpcAction::Attack  ? "approach_then_shoot"
                    : next == NpcAction::Retreat ? "return_to_safety"
                                                 : "routine_task";
    npc.state = next == NpcAction::Attack                        ? NpcState::Combat
                : next == NpcAction::Retreat                     ? NpcState::Retreat
                : next == NpcAction::Rob || npc.enemyTarget >= 0 ? NpcState::Alert
                                                                 : NpcState::Idle;
    npc.action = next;
    if (next != NpcAction::Rob)
        npc.robberyProgress = 0;
    npcDecisionCpuMs +=
        std::chrono::duration<float, std::milli>(std::chrono::steady_clock::now() - decisionStarted)
            .count();
    if (next != previousAction || time >= npc.decisionLogAt)
    {
        npc.decisionLogAt = time + 5;
        LogNpc(npc, "decision");
    }
}

void RpgGame::MoveNpc(TownNpc &npc, WorldPoint target, float dt, bool profileNpc)
{
    npc.goal = {target.x, target.z};
    int tx = int(std::round(target.x)), tz = int(std::round(target.z));
    if ((tx != npc.pathX || tz != npc.pathZ || npc.path.empty()) && npc.pathTimer <= 0)
    {
        CpuTimer pathTimer{profileNpc ? npcPathCpuMs : banditPathCpuMs};
        if (profileNpc)
            ++npcPathReplans;
        else
            ++banditPathReplans;
        npc.pathTimer = .75f;
        npc.pathX = tx;
        npc.pathZ = tz;
        npc.path.clear();
        // Bounded local BFS, separated from utility choice. Replans at most ~1.3 Hz.
        constexpr int side = 40;
        int ox = int(std::round(npc.position.x)) - side / 2,
            oz = int(std::round(npc.position.z)) - side / 2;
        tx = std::clamp(tx, ox, ox + side - 1);
        tz = std::clamp(tz, oz, oz + side - 1);
        if (!Fits({float(tx), float(tz)}))
            return;
        std::array<int, side * side> parent;
        parent.fill(-1);
        int start = side / 2 * side + side / 2, goal = (tz - oz) * side + tx - ox;
        std::queue<int> queue;
        queue.push(start);
        parent[start] = start;
        while (!queue.empty() && parent[goal] < 0)
        {
            int at = queue.front();
            queue.pop();
            for (int off : {1, -1, side, -side})
            {
                int n = at + off;
                if (n < 0 || n >= side * side ||
                    std::abs(n % side - at % side) + std::abs(n / side - at / side) != 1 ||
                    parent[n] >= 0)
                    continue;
                if (!Fits({float(ox + n % side), float(oz + n / side)}))
                    continue;
                parent[n] = at;
                queue.push(n);
            }
        }
        if (parent[goal] >= 0)
        {
            for (int at = goal; at != start; at = parent[at])
                npc.path.push_back({float(ox + at % side), float(oz + at / side)});
            std::reverse(npc.path.begin(), npc.path.end());
        }
    }
    if (npc.path.empty())
        return;
    auto next = npc.path.front();
    float d = Distance(npc.position, next);
    if (d < .12f)
    {
        npc.path.erase(npc.path.begin());
        return;
    }
    auto p = Point(npc.position);
    auto before = p;
    float step = std::min(d, npc.walkSpeed * dt);
    Move(p, (next.x - p.x) / d * step, (next.z - p.z) / d * step);
    npc.position = {p.x, p.z};
    npc.moving = p.x != before.x || p.z != before.z;
    if (npc.moving)
        npc.mirror = p.x < before.x;
}

void RpgGame::UpdateTown(float dt)
{
    auto updateStart = std::chrono::steady_clock::now();
    for (auto &npc : npcs)
    {
        npc.perceptionTimer -= dt;
        if (npc.hp > 0 && npc.perceptionTimer <= 0)
        {
            PerceiveNpc(npc);
            npc.perceptionTimer = .5f;
        }
    }
    townCoordinationTimer -= dt;
    if (townCoordinationTimer <= 0)
    {
        CoordinateTown();
        townCoordinationTimer = .5f;
    }
    for (auto &npc : npcs)
    {
        npc.moving = false;
        npc.cooldown = std::max(0.0f, npc.cooldown - dt);
        npc.serviceCooldown = std::max(0.0f, npc.serviceCooldown - dt);
        npc.robberyCooldown = std::max(0.0f, npc.robberyCooldown - dt);
        npc.pathTimer -= dt;
        UpdateNpcRoutine(npc);
        if (IsRescueActor(npc.id))
        {
            npc.state = npc.hp <= 0 ? NpcState::Disabled : NpcState::Idle;
            npc.action = NpcAction::Rest;
            continue;
        }
        if (npc.hp <= 0)
        {
            if (!npc.stolenGoods.empty())
                RecoverStolenGoods(npc);
            npc.state = NpcState::Disabled;
            npc.recovery += dt;
            if (npc.recovery >= 30)
            {
                npc.hp = 60;
                npc.position = npc.home;
                npc.hostile = false;
                npc.recovery = 0;
                npc.path.clear();
                // Recovery is not physical travel; do not teleport remote news home.
                npc.rumors.clear();
                npc.mind.knowledge.clear();
                npc.destinationVillage = -1;
                npc.travelWait = 12;
                npc.Remember("recovered_at_home");
            }
            continue;
        }
        if (talkingNpc >= 0 && &npc == &npcs[talkingNpc])
        {
            npc.robberyProgress = 0;
            npc.path.clear();
        }
        else
        {
            npc.decisionTimer -= dt;
            if (npc.decisionTimer <= 0)
            {
                DecideNpc(npc);
                npc.decisionTimer = .5f;
            }
            if (npc.courier && npc.hp >= 35 && npc.enemyTarget < 0 && npc.criminalTarget < 0 &&
                npc.robbedUntil <= time &&
                (npc.destinationVillage >= 0 || npc.routine == NpcRoutine::Work))
            {
                npc.travelWait -= dt;
                if (npc.destinationVillage >= 0 || npc.travelWait <= 0)
                    npc.action = NpcAction::Travel;
            }
            switch (npc.action)
            {
            case NpcAction::Travel:
                TravelNpc(npc, dt);
                break;
            case NpcAction::Work:
                if (!npc.courier || npc.travelWait <= 0 ||
                    novel::VillageAt(npc.position.x, npc.position.z) == npc.homeVillage)
                    MoveNpc(npc, Point(npc.routineGoal), dt);
                break;
            case NpcAction::Patrol:
            {
                const DollPoint offsets[] = {{2, 0}, {0, 2}, {-2, 0}, {0, -2}};
                auto o = offsets[npc.patrol % 4];
                WorldPoint goal{npc.home.x + o.x, npc.home.z + o.z};
                if (!Fits(goal))
                {
                    ++npc.patrol;
                    npc.path.clear();
                    break;
                }
                if (Distance(npc.position, {goal.x, goal.z}) < .3f)
                {
                    ++npc.patrol;
                    npc.path.clear();
                }
                MoveNpc(npc, goal, dt);
                break;
            }
            case NpcAction::Retreat:
                MoveNpc(npc, {novel::Villages()[npc.homeVillage].x, 3}, dt);
                break;
            case NpcAction::Rest:
                if (Distance(npc.position, npc.routineGoal) > 1)
                    MoveNpc(npc, Point(npc.routineGoal), dt);
                else
                {
                    npc.hp = std::min(100.0f, npc.hp + 5 * dt);
                    if (npc.cooldown == 0)
                    {
                        npc.ammo = 8;
                        npc.cooldown = 2;
                    }
                }
                break;
            case NpcAction::Help:
                if (npc.allyTarget >= 0 && npc.mind.assignedPatient == npc.allyTarget)
                {
                    const auto *belief = npc.mind.Find(NpcFact::Ally, npc.allyTarget, time);
                    if (!belief)
                    {
                        npc.decisionTimer = 0;
                        break;
                    }
                    auto &ally = npcs[npc.allyTarget];
                    if (Distance(npc.position, npc.goal) > 1.2f)
                        MoveNpc(npc, Point(npc.goal), dt);
                    else if (npc.cooldown == 0 && ally.hp < 80 &&
                             Distance(npc.position, ally.position) <= 1.2f &&
                             LineOfSight(Point(npc.position), Point(ally.position)))
                    {
                        ally.hp = std::min(100.0f, ally.hp + 35);
                        ally.recovery = 0;
                        npc.cooldown = 4;
                        ++npc.helped;
                        npc.Remember("help_ally_success");
                        ally.Remember("received_ally_help");
                        ally.mind.RescuedBy(npc.id, ally.id, time);
                        npc.mind.Remember(
                            NpcMemoryType::SuccessfulMission, npc.id, ally.id, time, .6f);
                        npc.mind.relationships[ally.id].trust =
                            std::min(1.0f, npc.mind.relationships[ally.id].trust + .02f);
                        npc.mind.Observe({NpcFact::Ally,
                                          ally.id,
                                          npc.id,
                                          ally.position,
                                          ally.hp,
                                          1,
                                          time,
                                          true,
                                          ally.hostile});
                        // Small observation-driven adaptation, constrained around personality.
                        for (auto &observer : npcs)
                            if (observer.hp > 0 && observer.id != npc.id &&
                                Distance(observer.position, npc.position) < 8 &&
                                LineOfSight(Point(observer.position), Point(npc.position)))
                            {
                                observer.traits.rescuePriority =
                                    std::min(observer.personality.rescuePriority + .1f,
                                             std::min(1.0f,
                                                      observer.traits.rescuePriority +
                                                          .02f * (1 - observer.resistance)));
                                observer.Remember("observed_help_success");
                            }
                        LogNpc(npc, "help_success");
                    }
                }
                break;
            case NpcAction::Rob:
                StepRobbery(npc, dt);
                break;
            case NpcAction::Attack:
            {
                if (npc.criminalTarget >= 0)
                {
                    auto found = std::find_if(npcs.begin(),
                                              npcs.end(),
                                              [&](const TownNpc &suspect)
                                              {
                                                  return suspect.id == npc.criminalTarget;
                                              });
                    if (found == npcs.end() || found->hp <= 0 || found->criminalUntil <= time)
                    {
                        npc.decisionTimer = 0;
                        break;
                    }
                    WorldPoint target{found->position.x, found->position.z};
                    if (Distance(npc.position, found->position) > 4 ||
                        !LineOfSight(Point(npc.position), target))
                        MoveNpc(npc, Point(npc.goal), dt);
                    else if (npc.cooldown == 0 && npc.ammo > 0)
                    {
                        --npc.ammo;
                        npc.cooldown = .9f;
                        npc.attackStarted = time;
                        shots.push_back({Point(npc.position), target, .12f});
                        found->hp = std::max(0.0f, found->hp - 12);
                        found->robberyProgress = 0;
                        found->decisionTimer = 0;
                        if (found->hp == 0)
                        {
                            RecoverStolenGoods(*found);
                            LogNpc(npc, "criminal_subdued");
                        }
                    }
                    break;
                }
                const auto *known = npc.playerTarget
                                        ? npc.mind.Find(NpcFact::Commander, -1, time)
                                        : npc.mind.Find(NpcFact::Enemy, npc.enemyTarget, time);
                if (!known)
                {
                    npc.decisionTimer = 0;
                    break;
                }
                WorldPoint target = player;
                if (!npc.playerTarget)
                {
                    if (npc.enemyTarget < 0 || enemies[npc.enemyTarget].hp <= 0)
                        break;
                    target = enemies[npc.enemyTarget].position;
                }
                if (npc.playerTarget &&
                    (TownNpc::InTown(player.x, player.z) || npc.serviceCooldown > 0))
                    break;
                if (Distance(npc.position, npc.goal) > 5)
                    MoveNpc(npc, Point(npc.goal), dt);
                else if (npc.cooldown == 0 && npc.ammo > 0 &&
                         Distance(npc.position, {target.x, target.z}) <= 5 &&
                         LineOfSight(Point(npc.position), target))
                {
                    --npc.ammo;
                    npc.attackStarted = time;
                    npc.cooldown = .9f;
                    shots.push_back({Point(npc.position), target, .12f});
                    if (npc.playerTarget)
                    {
                        npc.hostile = true;
                        hp = std::max(0, hp - 4);
                        hurt = .2f;
                    }
                    else
                    {
                        auto &e = enemies[npc.enemyTarget];
                        if (DamageMonster(e, 12, false))
                        {
                            ++npc.kills;
                            npc.Remember("hunt_success");
                            npc.mind.Remember(
                                NpcMemoryType::SuccessfulMission, npc.id, -1, time, .3f);
                            LogNpc(npc, "hunt_success");
                        }
                    }
                }
                break;
            }
            }
        }
        float previousHp = npc.hp;
        // Nearby hostile creatures retaliate; citizens inside the town remain protected.
        if (!TownNpc::InTown(npc.position.x, npc.position.z))
            for (const auto &e : enemies)
                if (e.hp > 0 && e.action == BanditAction::Attack &&
                    Distance(npc.position, {e.position.x, e.position.z}) < 1.5f)
                    npc.hp = std::max(0.0f, npc.hp - 5 * dt);
        if (previousHp >= 20 && npc.hp < 20)
        {
            if (npc.action == NpcAction::Help)
                npc.mind.Remember(NpcMemoryType::FailedRescue, npc.id, npc.allyTarget, time, .8f);
            npc.decisionTimer = 0;
            DecideNpc(npc);
        }
    }
    if (talkingNpc >= 0 &&
        (npcs[talkingNpc].hp <= 0 || Distance(npcs[talkingNpc].position, {player.x, player.z}) > 3))
        talkingNpc = -1;
    if (hp == 0)
        ClearInput();
    npcUpdateCpuMs +=
        std::chrono::duration<float, std::milli>(std::chrono::steady_clock::now() - updateStart)
            .count();
    npcMetricsTimer += dt;
    if (npcMetricsTimer >= 1 && !npcs.empty())
    {
        LogNpc(npcs.front(), "performance_window");
        npcUpdateCpuMs = npcDecisionCpuMs = npcPathCpuMs = npcMetricsTimer = 0;
        npcLogCpuMs = 0;
        npcDecisions = npcPathReplans = 0;
    }
}

int RpgGame::NearestNpc() const
{
    int selected = -1;
    float best = 2.4f;
    for (int i = 0; i < int(npcs.size()); ++i)
    {
        float distance = Distance(npcs[i].position, {player.x, player.z});
        if (npcs[i].hp > 0 && distance < best && LineOfSight(player, Point(npcs[i].position)))
        {
            best = distance;
            selected = i;
        }
    }
    return selected;
}

void RpgGame::InteractNpc()
{
    if (paused || hp <= 0 || dollTraining.active || inventory || storyJournalOpen)
        return;
    talkingNpc = NearestNpc();
    if (talkingNpc < 0)
    {
        Notify("가까운 주민에게 다가가 F로 대화하세요.");
        return;
    }
    firing = false;
    talkingActor = 0;
    auto &selected = npcs[talkingNpc];
    selected.moving = false;
    selected.path.clear();
    if (IsRescueActor(selected.id))
    {
        Notify(selected.name + ": 1번으로 호위를 시작하세요. 목적지는 아플리아 외곽입니다.");
        return;
    }
    if (selected.robbedUntil > time)
    {
        Notify(selected.name +
               ": 은화를 약탈당했습니다. 범죄자를 제압하면 도난 물자를 회수할 수 있습니다.");
        return;
    }
    bool grateful = false;
    for (const auto &event : selected.mind.memories)
        if (event.type == NpcMemoryType::CommanderSavedMe)
            grateful = true;
    Notify(selected.name + (grateful ? ": 지난번에 도와주신 일은 잊지 않았습니다."
                                     : ": 아래 대화창에서 1 또는 2를 선택하세요."));
}

void RpgGame::NpcChoice(int choice)
{
    if (paused || hp <= 0 || dollTraining.active || talkingNpc < 0)
        return;
    auto &npc = npcs[talkingNpc];
    if (npc.hp <= 0 || Distance(npc.position, {player.x, player.z}) > 3)
    {
        talkingNpc = -1;
        return;
    }
    if (RescueDialogue(npc, choice))
        return;
    bool localService =
        npc.serviceOpen && Distance(npc.position, npc.home) <= 2 && npc.destinationVillage < 0;
    if ((npc.role == NpcRole::Merchant || npc.role == NpcRole::Medic) && !localService)
    {
        Notify("식사·수면·이동 중에는 영업하지 않습니다. 근무 시간에 일터에서 만나세요.");
        return;
    }
    if (npc.serviceCooldown > 0)
    {
        Notify("잠시 기다려 주세요.");
        return;
    }
    if (npc.role == NpcRole::Merchant && (choice == 4 || choice == 5))
    {
        int &stock = choice == 4 ? npc.rationStock : npc.waterStock;
        int &supply = choice == 4 ? expeditionState.rations : expeditionState.water;
        if (stock > 0 && supply < 6 && TransferSilver(silver, npc.silver, 1))
        {
            --stock;
            ++supply;
            RecordEconomy(choice == 4 ? "ration_purchase" : "water_purchase", -1, npc.id, 1);
            Notify(choice == 4 ? "은화 1개로 식량 1개 구매." : "은화 1개로 식수 1개 구매.");
        }
        else
            Notify("은화·상인 재고가 필요합니다. 보급품은 각각 6개까지 휴대합니다.");
        return;
    }
    switch (npc.role)
    {
    case NpcRole::Merchant:
        if (choice == 1 && npc.potionStock > 0 && TransferSilver(silver, npc.silver, 3))
        {
            --npc.potionStock;
            RecordEconomy("potion_purchase", -1, npc.id, 3);
            ++potions;
            Notify("은화 3개로 회복약 1개 구매.");
        }
        else if (choice == 2 && potions > 0 && TransferSilver(npc.silver, silver, 2))
        {
            --potions;
            ++npc.potionStock;
            RecordEconomy("potion_sale", npc.id, -1, 2);
            Notify("회복약 1개 판매: 은화 2개 획득.");
        }
        else
            Notify("구매: 은화 3개와 상인 재고 필요 / 판매: 회복약과 상인 은화 2개 필요.");
        break;
    case NpcRole::Medic:
        if (choice == 1 && hp < MaxHp() && npc.medicalSupplies > 0 &&
            TransferSilver(silver, npc.silver, 2))
        {
            --npc.medicalSupplies;
            RecordEconomy("medical_service", -1, npc.id, 2);
            hp = MaxHp();
            Notify("은화 2개로 치료 완료.");
        }
        else
            Notify("치료: 은화 2개와 의약 재고가 필요합니다. 상처가 없으면 치료하지 않습니다.");
        break;
    case NpcRole::Helper:
        if (choice == 1 && hp < MaxHp())
        {
            hp = std::min(MaxHp(), hp + 25);
            npc.serviceCooldown = 45;
            npc.trust = std::min(1.0f, npc.trust + .05f);
            Notify("구호 주민의 응급 처치: 체력 +25.");
        }
        else
            Notify("광장에서도 적의 추격에 주의하세요. 사냥터에는 사냥꾼이 있습니다.");
        break;
    case NpcRole::Swindler:
        if (choice == 1 && TransferSilver(silver, npc.silver, 4))
        {
            RecordEconomy("fraud", -1, npc.id, 4);
            npc.trust = std::max(0.0f, npc.trust - .1f);
            npc.serviceCooldown = 20;
            npc.Remember("fake_goods_sold");
            Notify("보증 없는 특가 상자를 샀지만 안은 비어 있었습니다. 은화 -4.");
        }
        else
        {
            npc.Remember("offer_rejected");
            Notify("수상한 거래를 거절했습니다. 비용은 들지 않습니다.");
        }
        break;
    case NpcRole::Outlaw:
    case NpcRole::Bully:
        if (choice == 1 && TransferSilver(silver, npc.silver, 2))
        {
            RecordEconomy("extortion", -1, npc.id, 2);
            npc.serviceCooldown = 20;
            npc.hostile = false;
            Notify("통행료로 은화 2개를 냈습니다.");
        }
        else
        {
            npc.hostile = true;
            npc.Remember("payment_refused");
            Notify("요구를 거절했습니다. 이 NPC는 마을 밖에서 적대할 수 있습니다.");
        }
        break;
    case NpcRole::Hunter:
    case NpcRole::Guard:
        if (choice == 1 && npc.traits.commandObedience * npc.trust * npc.respect >= .15f)
        {
            npc.mind.commandStatus = npc.hp < 35 ? "accepted_with_modification" : "accepted";
            npc.mind.commandUntil = time + 20;
            npc.home = npc.hp < 35 ? DollPoint{novel::Villages()[npc.homeVillage].x, 3}
                                   : DollPoint{player.x, player.z};
            npc.path.clear();
            npc.routineKey = -1;
            npc.decisionTimer = 0;
            npc.trust = std::min(1.0f, npc.trust + .02f);
            Notify(npc.hp < 35 ? "부상으로 광장을 순찰 거점으로 변경하겠습니다."
                               : "현재 위치를 새 순찰 거점으로 지정했습니다.");
        }
        else
        {
            if (choice == 1)
                npc.mind.commandStatus = "questioned";
            Notify(choice == 1
                       ? "현재 신뢰로는 명령을 바로 수행하기 어렵습니다."
                       : "마수는 교역로 주변에 서식합니다. 도로를 따라가면 다음 마을이 나옵니다.");
        }
        break;
    }
    npc.mind.relationships[-1].trust = npc.trust;
    npc.mind.relationships[-1].respect = npc.respect;
    npc.Remember(choice == 1 ? "player_primary_choice" : "player_secondary_choice");
    LogNpc(npc, "interaction");
}

void RpgGame::LogNpc(const TownNpc &npc, const char *event) const
{
    CpuTimer logTimer{npcLogCpuMs};
    static std::ofstream stream;
    static std::filesystem::path directory;
    static bool attempted = false;
    if (!attempted)
    {
        attempted = true;
        directory = Platform::ExecutableDirectory() / "Profiles";
        std::error_code error;
        std::filesystem::create_directories(directory, error);
        stream.open(directory / "npc_ai.jsonl", std::ios::app);
        stream.imbue(std::locale::classic());
    }
    if (!stream)
        return;
    if (stream.tellp() > std::streamoff(5 * 1024 * 1024))
    {
        stream.close();
        std::error_code error;
        auto old = directory / "npc_ai.previous.jsonl";
        std::filesystem::remove(old, error);
        std::filesystem::rename(directory / "npc_ai.jsonl", old, error);
        stream.open(directory / "npc_ai.jsonl", std::ios::trunc);
    }
    stream << "{\"schema_version\":2,\"event\":\"" << event << "\",\"seed\":" << seed
           << ",\"time_s\":" << time << ",\"npc_id\":" << npc.id << ",\"role\":\""
           << TownNpc::RoleName(npc.role) << "\",\"action\":\"" << TownNpc::ActionName(npc.action)
           << "\",\"state\":" << int(npc.state) << ",\"hp\":" << npc.hp
           << ",\"wallet_silver\":" << npc.silver << ",\"robbery_target\":" << npc.robberyTarget
           << ",\"criminal_target\":" << npc.criminalTarget
           << ",\"enemy_target\":" << npc.enemyTarget << ",\"ally_target\":" << npc.allyTarget
           << ",\"trust\":" << npc.trust << ",\"memory_count\":" << npc.memory.size()
           << ",\"cpu_update_ms\":" << npcUpdateCpuMs << ",\"cpu_decision_ms\":" << npcDecisionCpuMs
           << ",\"cpu_log_ms\":" << npcLogCpuMs << ",\"cpu_pathfinding_ms\":" << npcPathCpuMs
           << ",\"window_simulation_s\":" << npcMetricsTimer
           << ",\"decisions_count\":" << npcDecisions
           << ",\"path_replans_count\":" << npcPathReplans << ",\"scores\":{";
    for (int i = 0; i < int(npc.scores.size()); ++i)
    {
        if (i)
            stream << ',';
        stream << '"' << TownNpc::ActionName(NpcAction(i)) << "\":" << npc.scores[i];
    }
    stream << "},\"selection_reason\":\"" << npc.mind.selectionReason << "\",\"task\":\""
           << npc.mind.task << "\",\"command_status\":\"" << npc.mind.commandStatus
           << "\",\"radio_enabled\":" << (townRadioConnected && npc.radio ? "true" : "false")
           << ",\"assigned_patient\":" << npc.mind.assignedPatient
           << ",\"rescue_bid\":" << npc.mind.rescueBid
           << ",\"help_contributions\":{\"traits\":" << npc.helpTraitScore
           << ",\"relationship\":" << npc.helpRelationshipScore
           << ",\"memory\":" << npc.helpMemoryScore << ",\"risk\":" << npc.helpRiskScore
           << "},\"needs\":{\"survive\":" << npc.mind.needs.survive
           << ",\"protect_ally\":" << npc.mind.needs.protectAlly
           << ",\"complete_objective\":" << npc.mind.needs.completeObjective
           << ",\"obey_commander\":" << npc.mind.needs.obeyCommander << "},\"beliefs\":[";
    bool first = true;
    for (const auto &entry : npc.mind.knowledge)
    {
        if (!first)
            stream << ',';
        first = false;
        const auto &fact = entry.second;
        stream << "{\"kind\":" << int(fact.kind) << ",\"subject\":" << fact.subject
               << ",\"source\":" << fact.source
               << ",\"direct\":" << (fact.direct ? "true" : "false")
               << ",\"observed_at_s\":" << fact.timestamp
               << ",\"confidence\":" << fact.Confidence(time) << ",\"x\":" << fact.position.x
               << ",\"z\":" << fact.position.z << ",\"observed_scrap\":" << fact.carriedSilver
               << ",\"hp\":" << fact.hp << '}';
    }
    stream << "],\"relationships\":[";
    first = true;
    for (const auto &entry : npc.mind.relationships)
    {
        if (!first)
            stream << ',';
        first = false;
        stream << "{\"target\":" << entry.first << ",\"trust\":" << entry.second.trust
               << ",\"respect\":" << entry.second.respect << ",\"debt\":" << entry.second.debt
               << '}';
    }
    stream << "],\"memories\":[";
    first = true;
    for (const auto &memory : npc.mind.memories)
    {
        if (!first)
            stream << ',';
        first = false;
        stream << "{\"type\":" << int(memory.type) << ",\"actor\":" << memory.actor
               << ",\"target\":" << memory.target << ",\"time_s\":" << memory.timestamp
               << ",\"importance\":" << memory.importance << '}';
    }
    stream << "],\"traits\":{\"aggression\":" << npc.traits.aggression
           << ",\"cover_preference\":" << npc.traits.coverPreference
           << ",\"rescue_priority\":" << npc.traits.rescuePriority
           << ",\"command_obedience\":" << npc.traits.commandObedience
           << ",\"autonomy\":" << npc.traits.autonomy
           << ",\"risk_tolerance\":" << npc.traits.riskTolerance
           << ",\"ammo_conservation\":" << npc.traits.ammoConservation
           << ",\"civilian_priority\":" << npc.traits.civilianPriority
           << ",\"retreat_preference\":" << npc.traits.retreatPreference
           << ",\"team_support\":" << npc.traits.teamSupport
           << ",\"objective_priority\":" << npc.traits.objectivePriority
           << ",\"self_preservation\":" << npc.traits.selfPreservation << "}}\n";
    if (event == std::string("interaction") || event == std::string("performance_window"))
        stream.flush();
}
