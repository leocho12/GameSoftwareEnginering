#include "stdafx.h"
#include "RpgGame.h"
#include <Windows.h>
#include <algorithm>
#include <chrono>
#include <cmath>
#include <fstream>
#include <locale>

namespace
{
float Distance(WorldPoint a, WorldPoint b)
{
    return std::hypot(a.x - b.x, a.z - b.z);
}
}

const char *RpgEnemy::ActionName(BanditAction value)
{
    const char *names[] = {"patrol", "observe", "attack", "retreat", "return_to_camp"};
    return names[int(value)];
}

const char *RpgEnemy::ActionLabel(BanditAction value)
{
    const char *names[] = {"순찰", "경계", "교전", "후퇴", "서식지 복귀"};
    return names[int(value)];
}

void RpgGame::InitializeBandits()
{
    constexpr int groupsPerCorridor = 10;
    banditGroups.assign(novel::CorridorCount * groupsPerCorridor, {});
    enemies.assign(novel::CorridorCount * novel::MonstersPerCorridor, {});
    banditLogTimer = banditUpdateCpuMs = banditPathCpuMs = 0;
    banditDecisions = banditPathReplans = 0;
    for (int route = 0; route < novel::CorridorCount; ++route)
    {
        float left = novel::Villages()[route].x + 14;
        for (int group = 0; group < groupsPerCorridor; ++group)
        {
            auto &g = banditGroups[route * groupsPerCorridor + group];
            g.camp = {left + 5 + (group % 5) * 8.0f, 3 + (group < 5 ? -12.0f : 12.0f)};
            g.waypoint = g.camp;
        }
        for (int member = 0; member < novel::MonstersPerCorridor; ++member)
        {
            auto &enemy = enemies[route * novel::MonstersPerCorridor + member];
            enemy.band = route * groupsPerCorridor + member / 5;
            enemy.hp = 0;
            enemy.appearance = member % 4;
            enemy.traits.aggression = member % 4 == 0 ? .85f : member % 4 == 1 ? .65f : .05f;
            enemy.traits.riskTolerance = .4f;
            enemy.traits.selfPreservation = .8f;
            enemy.traits.teamSupport = .5f;
            RespawnMonster(enemy, true);
        }
    }
}

bool RpgGame::RespawnMonster(RpgEnemy &enemy, bool initial)
{
    if (enemy.band < 0 || enemy.band / 10 >= novel::CorridorCount ||
        banditGroups.size() != novel::CorridorCount * 10)
    {
        enemy.respawn = .25f;
        return false;
    }
    const int route = enemy.band / 10;
    const float left = novel::Villages()[route].x + 13;
    const float right = novel::Villages()[route + 1].x - 13;
    const WorldPoint death = enemy.position;
    for (int attempt = 0; attempt < 256; ++attempt)
    {
        WorldPoint point{left + float(random() % 10001) / 10000 * (right - left),
                         3 + float(int(random() % 4301) - 2150) / 100};
        // Keep the main road usable and avoid visible pop-in after a kill.
        if (std::abs(point.z - 3) < 4 || !Fits(point) || TownNpc::InTown(point.x, point.z) ||
            (!initial && (Distance(point, death) < 8 || Distance(point, player) < 10)))
            continue;
        bool occupied = false;
        for (const auto &other : enemies)
            if (&other != &enemy && other.hp > 0 && Distance(point, other.position) < 1)
                occupied = true;
        if (occupied)
            continue;
        int nearest = route * 10;
        for (int g = route * 10; g < (route + 1) * 10; ++g)
            if (Distance(point, banditGroups[g].camp) < Distance(point, banditGroups[nearest].camp))
                nearest = g;
        auto traits = enemy.traits;
        int appearance = enemy.appearance;
        enemy = RpgEnemy{};
        enemy.traits = traits;
        enemy.band = nearest;
        enemy.appearance = appearance;
        enemy.position = point;
        enemy.maxHp = enemy.hp = 40;
        enemy.decisionTimer = float(random() % 50) * .01f;
        if (!initial)
            ++monsterRespawns;
        return true;
    }
    enemy.respawn = .25f; // Retry later, never force a blocked or occupied spawn.
    return false;
}

bool RpgGame::PlaceBand(int band)
{
    bool placed = false;
    for (auto &enemy : enemies)
        if (enemy.band == band && enemy.hp <= 0)
            placed |= RespawnMonster(enemy);
    return placed;
}

void RpgGame::ProvokeBand(RpgEnemy &victim)
{
    // Only witnesses in the same nearby group share the provocation.
    for (auto &enemy : enemies)
        if (&enemy == &victim ||
            (victim.band >= 0 && enemy.band == victim.band &&
             Distance(enemy.position, victim.position) <= 8 && LineOfSight(enemy.position, player)))
        {
            enemy.provokedUntil = time + 12;
            enemy.lastSeen = player;
            enemy.memoryUntil = time + 6;
            enemy.decisionTimer = 0;
        }
}

bool RpgGame::DamageMonster(RpgEnemy &enemy, int damage, bool playerCredit)
{
    if (enemy.hp <= 0 || damage <= 0)
        return false;
    enemy.hp = std::max(0, enemy.hp - damage);
    if (enemy.hp > 0)
        return false;
    enemy.respawn = expeditionState.clock.SecondsToDawn();
    enemy.moving = false;
    if (playerCredit)
    {
        ++kills;
        ++expeditionState.progress.combatWins;
        AddLocalRumor(enemy.position, "교역로의 마수가 모험가에게 토벌되었다고 한다.", 1);
        drops.push_back({enemy.position, random() % 3 == 0});
        if (drops.size() > 128)
            drops.erase(drops.begin());
    }
    return true;
}

void RpgGame::UpdateBandits(float dt)
{
    auto started = std::chrono::steady_clock::now();
    for (int band = 0; band < int(banditGroups.size()); ++band)
    {
        auto &group = banditGroups[band];
        group.patrolTimer -= dt;
        if (group.patrolTimer <= 0)
        {
            group.patrolTimer = 6;
            for (int attempt = 0; attempt < 12; ++attempt)
            {
                WorldPoint p{group.camp.x + int(random() % 7) - 3,
                             group.camp.z + int(random() % 7) - 3};
                if (Fits(p) && !TownNpc::InTown(p.x, p.z))
                {
                    group.waypoint = p;
                    break;
                }
            }
        }
    }
    for (auto &enemy : enemies)
    {
        enemy.moving = false;
        if (enemy.hp <= 0)
        {
            enemy.respawn -= dt;
            if (enemy.respawn <= 0)
                RespawnMonster(enemy);
            continue;
        }
        bool nearby = Distance(enemy.position, player) <= 38;
        if (!nearby)
            for (const auto &npc : npcs)
                if (npc.hp > 0 && Distance(enemy.position, {npc.position.x, npc.position.z}) < 12)
                {
                    nearby = true;
                    break;
                }
        if (!nearby)
        {
            enemy.seesPlayer = false;
            continue;
        }
        enemy.cooldown = std::max(0.0f, enemy.cooldown - dt);
        enemy.decisionTimer -= dt;
        enemy.locomotion.pathTimer -= dt;
        WorldPoint camp = enemy.band >= 0 && enemy.band < int(banditGroups.size())
                              ? banditGroups[enemy.band].camp
                              : enemy.position;
        float range = Distance(enemy.position, player);
        if (enemy.decisionTimer <= 0)
        {
            ++banditDecisions;
            enemy.decisionTimer = .5f;
            auto &alert = expeditionState.alerts[RegionAt(enemy.position)];
            auto phase = expeditionState.clock.CurrentPhase();
            bool night =
                phase == expedition::Phase::DeepNight || phase == expedition::Phase::Evening;
            float sightRange = (night ? 5.0f : 7.0f) + alert.Level() * .5f;
            if (keys['c'])
                sightRange *= .55f;
            bool seesPlayer = range <= sightRange && LineOfSight(enemy.position, player);
            WorldPoint heard;
            bool hearsNoise = HeardNoise(enemy.position, heard);
            if (hearsNoise && !seesPlayer)
            {
                enemy.lastSeen = heard;
                enemy.memoryUntil = time + 8;
            }
            if ((seesPlayer || hearsNoise) && time - enemy.alertReportedAt >= 8)
            {
                // Only a local observer raises this route's alert; no remote broadcast.
                alert.Raise(seesPlayer && enemy.traits.aggression > .5f ? 8.0f : 3.0f);
                enemy.alertReportedAt = time;
            }
            enemy.seesPlayer = seesPlayer;
            if (seesPlayer)
            {
                enemy.lastSeen = player;
                enemy.memoryUntil = time + 5;
            }
            int allies = 0, guards = 0;
            for (const auto &other : enemies)
                if (other.hp > 0 && other.band == enemy.band &&
                    Distance(other.position, enemy.position) <= 8)
                    ++allies;
            for (const auto &npc : npcs)
                if (npc.hp > 0 && (npc.role == NpcRole::Guard || npc.role == NpcRole::Hunter) &&
                    Distance(enemy.position, {npc.position.x, npc.position.z}) < 8 &&
                    LineOfSight(enemy.position, {npc.position.x, npc.position.z}))
                    ++guards;
            float health = float(enemy.hp) / std::max(1, enemy.maxHp);
            float danger = (1 - health) * .65f + guards * .3f +
                           std::max(0.0f, (AttackPower() / 14.0f - allies) * .25f);
            enemy.nearbyAllies = allies;
            enemy.nearbyGuards = guards;
            enemy.perceivedDanger = danger;
            bool provoked = enemy.provokedUntil > time;
            bool remembers = enemy.memoryUntil > time;
            bool leash = Distance(enemy.position, camp) > 14 + alert.Level() * 3;
            auto &scores = enemy.scores;
            scores.fill(-1);
            scores[0] = .35f;
            if (seesPlayer || hearsNoise)
                scores[1] = .65f - enemy.traits.aggression * .20f;
            if (remembers && !leash)
                scores[2] = enemy.traits.aggression * .65f + enemy.traits.riskTolerance * .20f +
                            std::min(allies - 1, 2) * .07f - danger * .5f + (provoked ? .65f : 0);
            if (remembers && danger > .25f)
                scores[3] = danger * .8f + enemy.traits.selfPreservation * .35f;
            if (leash || (!remembers && Distance(enemy.position, camp) > 5))
                scores[4] = 1.5f;
            auto next = BanditAction(
                std::distance(scores.begin(), std::max_element(scores.begin(), scores.end())));
            enemy.reason = "highest_utility";
            if (scores[int(enemy.action)] >= 0 &&
                scores[int(next)] - scores[int(enemy.action)] < .08f)
            {
                next = enemy.action;
                enemy.reason = "hysteresis";
            }
            if (health < .25f && remembers)
            {
                next = BanditAction::Retreat;
                enemy.reason = "critical_health";
            }
            enemy.action = next;
        }
        bool knowsPlayer = enemy.memoryUntil > time;
        WorldPoint goal = camp;
        switch (enemy.action)
        {
        case BanditAction::Patrol:
            if (enemy.band >= 0 && enemy.band < int(banditGroups.size()))
            {
                goal = banditGroups[enemy.band].waypoint;
                // Loose formation around a shared route, with collision-checked offsets.
                int slot = int(&enemy - enemies.data()) % 5;
                WorldPoint offset{goal.x + slot - 1, goal.z + (slot == 1 ? 1 : 0)};
                if (Fits(offset))
                    goal = offset;
            }
            break;
        case BanditAction::Observe:
            enemy.mirror = enemy.lastSeen.x < enemy.position.x;
            goal = enemy.memoryUntil > time ? enemy.lastSeen : camp;
            break;
        case BanditAction::Attack:
            if (!knowsPlayer)
                continue;
            goal = enemy.lastSeen;
            if (enemy.seesPlayer && range <= .85f && LineOfSight(enemy.position, player))
            {
                if (enemy.cooldown == 0)
                {
                    hp = std::max(0, hp - std::max(1, 8 - Defense()));
                    enemy.cooldown = 1.2f;
                    enemy.attackStarted = time;
                    hurt = .2f;
                    if (hp == 0)
                    {
                        ClearInput();
                        Notify("전투 불능 · Enter로 같은 맵에서 다시 시작");
                        break;
                    }
                }
                continue;
            }
            break;
        case BanditAction::Retreat:
        {
            float dx = enemy.position.x - enemy.lastSeen.x;
            float dz = enemy.position.z - enemy.lastSeen.z;
            float length = std::max(.1f, std::hypot(dx, dz));
            WorldPoint away{enemy.position.x + dx / length * 3, enemy.position.z + dz / length * 3};
            if (Fits(away))
                goal = away;
            break;
        }
        case BanditAction::Return:
            break;
        }
        if (hp <= 0)
            break;
        if (Distance(enemy.position, goal) > .3f)
        {
            auto before = enemy.position;
            enemy.locomotion.position = {before.x, before.z};
            MoveNpc(enemy.locomotion,
                    goal,
                    enemy.action == BanditAction::Patrol ? dt * .6f : dt,
                    false);
            enemy.position = {enemy.locomotion.position.x, enemy.locomotion.position.z};
            enemy.moving = Distance(before, enemy.position) > .0001f;
            if (enemy.moving)
                enemy.mirror = enemy.position.x < before.x;
        }
    }
    banditUpdateCpuMs +=
        std::chrono::duration<float, std::milli>(std::chrono::steady_clock::now() - started)
            .count();
    banditLogTimer += dt;
    if (banditLogTimer >= 1)
    {
        LogBandits();
        banditLogTimer = banditUpdateCpuMs = banditPathCpuMs = 0;
        banditDecisions = banditPathReplans = 0;
    }
}

void RpgGame::LogBandits()
{
    wchar_t executable[MAX_PATH]{};
    GetModuleFileNameW(nullptr, executable, MAX_PATH);
    auto directory = std::filesystem::path(executable).parent_path() / "Profiles";
    std::error_code error;
    std::filesystem::create_directories(directory, error);
    auto path = directory / "monster_ai.jsonl";
    if (std::filesystem::exists(path, error) &&
        std::filesystem::file_size(path, error) > 5 * 1024 * 1024)
    {
        auto previous = directory / "monster_ai.previous.jsonl";
        std::filesystem::remove(previous, error);
        error.clear();
        std::filesystem::rename(path, previous, error);
        if (error)
            return;
    }
    std::ofstream out(path, std::ios::app);
    out.imbue(std::locale::classic());
    out << "{\"event\":\"monster_performance_window\",\"game_seconds\":" << time
        << ",\"window_seconds\":" << banditLogTimer << ",\"cpu_update_ms\":" << banditUpdateCpuMs
        << ",\"cpu_path_ms\":" << banditPathCpuMs << ",\"decisions\":" << banditDecisions
        << ",\"path_replans\":" << banditPathReplans << ",\"enemy_slots\":" << enemies.size()
        << ",\"group_count\":" << banditGroups.size() << "}\n";
    for (std::size_t i = 0; i < enemies.size(); ++i)
    {
        const auto &enemy = enemies[i];
        if (Distance(enemy.position, player) > 38 && enemy.hp > 0)
            continue;
        out << "{\"event\":\"monster_ai_snapshot\",\"seed\":" << seed
            << ",\"game_seconds\":" << time << ",\"enemy_id\":" << i
            << ",\"band_id\":" << enemy.band << ",\"hp\":" << enemy.hp << ",\"action\":\""
            << RpgEnemy::ActionName(enemy.action) << "\",\"reason\":\"" << enemy.reason
            << "\",\"aggression\":" << enemy.traits.aggression
            << ",\"risk_tolerance\":" << enemy.traits.riskTolerance
            << ",\"self_preservation\":" << enemy.traits.selfPreservation
            << ",\"sees_player\":" << (enemy.seesPlayer ? "true" : "false")
            << ",\"nearby_allies_including_self\":" << enemy.nearbyAllies
            << ",\"nearby_guards\":" << enemy.nearbyGuards
            << ",\"perceived_danger\":" << enemy.perceivedDanger
            << ",\"provoked\":" << (enemy.provokedUntil > time ? "true" : "false")
            << ",\"player_memory_seconds\":" << std::max(0.0f, enemy.memoryUntil - time)
            << ",\"utility_scores\":[";
        for (int score = 0; score < 5; ++score)
            out << (score ? "," : "") << enemy.scores[score];
        out << "]}\n";
    }
}
