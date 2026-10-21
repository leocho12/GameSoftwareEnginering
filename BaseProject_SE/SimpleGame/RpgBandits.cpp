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
    const char *names[] = {"순찰", "경계", "교전", "후퇴", "야영지 복귀"};
    return names[int(value)];
}

void RpgGame::InitializeBandits()
{
    banditGroups.assign(3, {});
    enemies.resize(9);
    banditLogTimer = banditUpdateCpuMs = banditPathCpuMs = 0;
    banditDecisions = banditPathReplans = 0;
    for (int band = 0; band < 3; ++band)
    {
        for (int member = 0; member < 3; ++member)
        {
            auto &enemy = enemies[band * 3 + member];
            enemy.hp = 0;
            enemy.band = band;
            enemy.appearance = 5 + member;
            enemy.traits.aggression = .20f + band * .28f + member * .035f;
            enemy.traits.riskTolerance = .25f + band * .25f;
            enemy.traits.selfPreservation = .85f - band * .15f;
            enemy.traits.teamSupport = .8f;
        }
        PlaceBand(band);
    }
}

bool RpgGame::PlaceBand(int band)
{
    std::vector<WorldPoint> candidates;
    int cx = int(std::floor(player.x)), cz = int(std::floor(player.z));
    for (int z = cz - 32; z <= cz + 32; ++z)
        for (int x = cx - 32; x <= cx + 32; ++x)
        {
            WorldPoint p{float(x), float(z)};
            if (Distance(p, player) < 18 || Distance(p, player) > 32 || TownNpc::InTown(p.x, p.z) ||
                !Fits(p))
                continue;
            bool clear = true;
            // A compact formation needs room; keep camps away from village exits.
            for (int dz = -2; dz <= 2; ++dz)
                for (int dx = -2; dx <= 2; ++dx)
                    if (TownNpc::InTown(p.x + dx, p.z + dz))
                        clear = false;
            for (int other = 0; other < int(banditGroups.size()); ++other)
                if (other != band && Distance(p, banditGroups[other].camp) < 13)
                    clear = false;
            if (clear)
                candidates.push_back(p);
        }
    if (candidates.empty())
        return false;
    auto &group = banditGroups[band];
    group.camp = group.waypoint = candidates[random() % candidates.size()];
    group.patrolTimer = 0;
    group.respawnTimer = 45;
    int member = 0;
    for (auto &enemy : enemies)
    {
        if (enemy.band != band)
            continue;
        auto traits = enemy.traits;
        int appearance = enemy.appearance;
        enemy = RpgEnemy{};
        enemy.band = band;
        enemy.traits = traits;
        enemy.appearance = appearance;
        enemy.maxHp = enemy.hp = 40 + std::min(level - 1, 4) * 3;
        enemy.position = group.camp;
        for (int dz = -2; dz <= 2; ++dz)
            for (int dx = -2; dx <= 2; ++dx)
            {
                WorldPoint p{group.camp.x + dx, group.camp.z + dz};
                if (!Fits(p) || TownNpc::InTown(p.x, p.z))
                    continue;
                bool occupied = false;
                for (const auto &other : enemies)
                    if (&other != &enemy && other.band == band && other.hp > 0 &&
                        Distance(other.position, p) < .75f)
                        occupied = true;
                if (!occupied)
                    enemy.position = p;
            }
        enemy.decisionTimer = .08f * member++;
    }
    return true;
}

void RpgGame::ProvokeBand(RpgEnemy &victim)
{
    // Only witnesses in the same nearby group share the provocation.
    for (auto &enemy : enemies)
        if (&enemy == &victim || (victim.band >= 0 && enemy.band == victim.band &&
                                  Distance(enemy.position, victim.position) <= 8))
        {
            enemy.provokedUntil = time + 12;
            enemy.lastSeen = player;
            enemy.memoryUntil = time + 6;
            enemy.decisionTimer = 0;
        }
}

void RpgGame::UpdateBandits(float dt)
{
    auto started = std::chrono::steady_clock::now();
    for (int band = 0; band < int(banditGroups.size()); ++band)
    {
        auto &group = banditGroups[band];
        bool any = false, allFar = true;
        for (const auto &enemy : enemies)
            if (enemy.band == band)
            {
                any |= enemy.hp > 0;
                allFar &= Distance(enemy.position, player) > 42;
            }
        // Never refill individual members next to a fight or recycle visible pursuers.
        if (!any)
            group.respawnTimer -= dt;
        if (((any && allFar) || (!any && group.respawnTimer <= 0)) &&
            Distance(group.camp, player) > 36)
        {
            PlaceBand(band);
            continue;
        }
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
            continue;
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
            bool seesPlayer = range <= 7 && LineOfSight(enemy.position, player);
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
            bool leash = Distance(enemy.position, camp) > 14;
            auto &scores = enemy.scores;
            scores.fill(-1);
            scores[0] = .35f;
            if (seesPlayer)
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
                int slot = int(&enemy - enemies.data()) % 3;
                WorldPoint offset{goal.x + slot - 1, goal.z + (slot == 1 ? 1 : 0)};
                if (Fits(offset))
                    goal = offset;
            }
            break;
        case BanditAction::Observe:
            enemy.mirror = enemy.lastSeen.x < enemy.position.x;
            continue;
        case BanditAction::Attack:
            if (!knowsPlayer)
                continue;
            goal = enemy.lastSeen;
            if (range <= .85f && LineOfSight(enemy.position, player))
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
    auto path = directory / "bandit_ai.jsonl";
    if (std::filesystem::exists(path, error) &&
        std::filesystem::file_size(path, error) > 5 * 1024 * 1024)
    {
        auto previous = directory / "bandit_ai.previous.jsonl";
        std::filesystem::remove(previous, error);
        error.clear();
        std::filesystem::rename(path, previous, error);
        if (error)
            return;
    }
    std::ofstream out(path, std::ios::app);
    out.imbue(std::locale::classic());
    out << "{\"event\":\"bandit_performance_window\",\"game_seconds\":" << time
        << ",\"window_seconds\":" << banditLogTimer << ",\"cpu_update_ms\":" << banditUpdateCpuMs
        << ",\"cpu_path_ms\":" << banditPathCpuMs << ",\"decisions\":" << banditDecisions
        << ",\"path_replans\":" << banditPathReplans << ",\"enemy_slots\":" << enemies.size()
        << ",\"group_count\":" << banditGroups.size() << "}\n";
    for (std::size_t i = 0; i < enemies.size(); ++i)
    {
        const auto &enemy = enemies[i];
        out << "{\"event\":\"bandit_ai_snapshot\",\"seed\":" << seed << ",\"game_seconds\":" << time
            << ",\"enemy_id\":" << i << ",\"band_id\":" << enemy.band << ",\"hp\":" << enemy.hp
            << ",\"action\":\"" << RpgEnemy::ActionName(enemy.action) << "\",\"reason\":\""
            << enemy.reason << "\",\"aggression\":" << enemy.traits.aggression
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

bool RpgGame::PatrolReady() const
{
    return level >= 3 && itemsCollected >= 2;
}

std::string RpgGame::PatrolObjective() const
{
    if (patrolReported)
        return "첫 순찰 완료! 자유 탐험을 계속할 수 있습니다.";
    if (level < 3)
        return "1/3 야적과 교전해 레벨 3 달성 (현재 " + std::to_string(level) + ")";
    if (itemsCollected < 2)
        return "2/3 전리품 2개 회수 (현재 " + std::to_string(itemsCollected) + ")";
    return "3/3 마을 경비에게 F로 복귀 보고 · 보상 부품 5개";
}

std::string RpgGame::PatrolHint() const
{
    if (patrolReported)
        return "무리를 우회하거나 교전하세요. I: 성장 능력치 확인";
    if (level >= 3)
        return itemsCollected < 2 ? "적을 제압한 자리에서 E로 전리품을 회수하세요."
                                  : "마을 초소나 출입로의 경비에게 다가가 F를 누르세요.";
    const RpgEnemy *nearest = nullptr;
    for (const auto &enemy : enemies)
        if (enemy.hp > 0 &&
            (!nearest || Distance(enemy.position, player) < Distance(nearest->position, player)))
            nearest = &enemy;
    if (!nearest)
        return "지역 무리를 모두 제압했습니다. 다른 구역을 탐험하세요.";
    float dx = nearest->position.x - player.x, dz = nearest->position.z - player.z;
    float right = .745f * dx - .667f * dz, up = -.667f * dx - .745f * dz;
    const char *direction = std::abs(right) > std::abs(up) ? (right > 0 ? "오른쪽" : "왼쪽")
                                                           : (up > 0 ? "위쪽" : "아래쪽");
    return "야적 무리: 화면 " + std::string(direction) + " 약 " +
           std::to_string(int(std::round(Distance(nearest->position, player)))) +
           " 거리 · 먼저 공격하면 주변 동료가 반응합니다.";
}
