#include "stdafx.h"
#include "GameMonitor.h"
#include "RpgGame.h"
#include <Windows.h>
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <fstream>
#include <iomanip>
#include <locale>
#include <sstream>

namespace
{
std::string Quote(const std::string &value)
{
    std::ostringstream out;
    out << '"';
    for (unsigned char c : value)
    {
        switch (c)
        {
        case '"':
            out << "\\\"";
            break;
        case '\\':
            out << "\\\\";
            break;
        case '\n':
            out << "\\n";
            break;
        case '\r':
            out << "\\r";
            break;
        case '\t':
            out << "\\t";
            break;
        default:
            if (c < 32)
                out << "\\u" << std::hex << std::setw(4) << std::setfill('0') << int(c) << std::dec;
            else
                out << char(c);
        }
    }
    out << '"';
    return out.str();
}

float Finite(float n)
{
    return std::isfinite(n) ? n : 0;
}

void Position(std::ostream &out, float x, float z)
{
    out << "{\"x\":" << Finite(x) << ",\"z\":" << Finite(z) << '}';
}

// Monitoring summaries describe existing state; they never change the simulation.
void VillageOverview(std::ostream &out, const RpgGame &game)
{
    int population = 0, hurt = 0, combat = 0, threats = 0;
    float rescue = 0, obedience = 0, autonomy = 0;
    for (const auto &npc : game.npcs)
    {
        if (npc.hp <= 0)
        {
            ++hurt;
            continue;
        }
        ++population;
        hurt += npc.hp < 100;
        combat += npc.state == NpcState::Combat;
        threats += npc.hostile;
        rescue += Finite(npc.traits.rescuePriority);
        obedience += Finite(npc.traits.commandObedience);
        autonomy += Finite(npc.traits.autonomy);
    }

    struct Rumor
    {
        int holders = 0;
        float confidence = 0;
        std::int64_t observed = 0;
        std::vector<std::string> names;
    };

    std::array<Rumor, 4> rumors{};
    for (const auto &entry : game.world.Actors())
    {
        const auto &npc = entry.second;
        if (npc.hp <= 0)
        {
            ++hurt;
            continue;
        }
        ++population;
        hurt += npc.hp < 100;
        rescue += Finite(npc.rescue);
        obedience += Finite(npc.obedience);
        autonomy += Finite(npc.autonomy);
        // Remote personal knowledge is not yet a rumor heard inside the village.
        if (npc.available && TownNpc::InTown(npc.position.x, npc.position.z) &&
            npc.brain.belief.aware)
        {
            auto &rumor = rumors[int(npc.brain.belief.outcome)];
            ++rumor.holders;
            rumor.confidence += Finite(npc.brain.belief.confidence);
            rumor.observed = std::max(rumor.observed, npc.brain.belief.observed);
            rumor.names.push_back(npc.name);
        }
    }
    for (const auto &enemy : game.enemies)
        threats += enemy.hp > 0 && TownNpc::InTown(enemy.position.x, enemy.position.z);
    const char *mood = "평온";
    const char *reason = "교전·적대·부상 인원이 없습니다.";
    if (!population)
    {
        mood = "인원 없음";
        reason = "활동 가능한 등록 인원이 없습니다.";
    }
    else if (combat || threats)
    {
        mood = "긴장";
        reason = "등록 인원의 교전·적대 또는 마을 내 몹이 감지됐습니다.";
    }
    else if (hurt)
    {
        mood = "걱정";
        reason = "부상 인원이 있어 치료가 필요합니다.";
    }
    else if (rumors[int(narrative::Outcome::Failed)].holders)
    {
        mood = "침울";
        reason = "기지 인원이 구조 실패 소식을 알고 있습니다.";
    }
    else if (rumors[int(narrative::Outcome::Partial)].holders)
    {
        mood = "안도와 걱정";
        reason = "일부 구조와 손실 소식이 알려졌습니다.";
    }
    else if (rumors[int(narrative::Outcome::Success)].holders)
    {
        mood = "안도";
        reason = "기지 인원이 구조 성공 소식을 알고 있습니다.";
    }
    else if (rumors[0].holders)
    {
        mood = "불안";
        reason = "실종 소식을 알고 있으나 결과는 아직 확인하지 못했습니다.";
    }
    out << ",\"population\":" << population << ",\"atmosphere\":{\"label\":" << Quote(mood)
        << ",\"reason\":" << Quote(reason) << ",\"basis\":\"derived_registered_npc_state\"}"
        << ",\"tendencies\":";
    if (population)
        out << "{\"rescue_priority\":" << rescue / population
            << ",\"command_obedience\":" << obedience / population
            << ",\"autonomy\":" << autonomy / population
            << ",\"basis\":\"living_registered_npc_mean\"}";
    else
        out << "null";
    out << ",\"rumors\":[";
    const char *messages[] = {"남쪽에서 인형 2명의 연락이 끊겼다는 소식",
                              "실종 인형 2명을 구조했다는 소식",
                              "실종 인형 중 1명만 구조했다는 소식",
                              "실종 인형 수색에 실패했다는 소식"};
    bool first = true;
    for (int i = 0; i < 4; ++i)
    {
        const auto &rumor = rumors[i];
        if (!rumor.holders)
            continue;
        if (!first)
            out << ',';
        first = false;
        out << "{\"id\":" << Quote("missing_doll:" + std::to_string(i))
            << ",\"text\":" << Quote(messages[i])
            << ",\"confidence\":" << rumor.confidence / rumor.holders
            << ",\"holders\":" << rumor.holders << ",\"observed_minute\":" << rumor.observed
            << ",\"basis\":\"local_npc_beliefs\",\"sources\":[";
        for (std::size_t n = 0; n < rumor.names.size(); ++n)
        {
            if (n)
                out << ',';
            out << Quote(rumor.names[n]);
        }
        out << "]}";
    }
    out << ']';
}

}

GameMonitor::GameMonitor(std::filesystem::path output) : destination(std::move(output))
{
}

void GameMonitor::Publish(const RpgGame &game)
{
    const auto now = std::chrono::steady_clock::now();
    if (now < next)
        return;
    next = now + std::chrono::milliseconds(500);
    const auto timestamp = std::chrono::duration_cast<std::chrono::milliseconds>(
                               std::chrono::system_clock::now().time_since_epoch())
                               .count();
    int aliveMobs = 0, townMobs = 0, aliveNpcs = 0, present = 0, injured = 0, hostile = 0, away = 0;
    for (const auto &e : game.enemies)
        if (e.hp > 0)
        {
            ++aliveMobs;
            if (TownNpc::InTown(e.position.x, e.position.z))
                ++townMobs;
        }
    for (const auto &n : game.npcs)
    {
        aliveNpcs += n.hp > 0;
        injured += n.hp > 0 && n.hp < 100;
        hostile += n.hostile;
        present += TownNpc::InTown(n.position.x, n.position.z);
    }
    for (const auto &entry : game.world.Actors())
    {
        const auto &n = entry.second;
        aliveNpcs += n.hp > 0;
        injured += n.hp > 0 && n.hp < 100;
        away += !n.available;
        present += n.available && TownNpc::InTown(n.position.x, n.position.z);
    }
    const auto total = game.npcs.size() + game.world.Actors().size();
    std::ostringstream out;
    out.imbue(std::locale::classic());
    out << std::setprecision(7)
        << "{\"schema_version\":1,\"type\":\"snapshot\",\"session_id\":" << GetCurrentProcessId()
        << ",\"sequence\":" << ++sequence << ",\"sent_at_ms\":" << timestamp
        << ",\"game\":{\"seed\":" << game.seed << ",\"paused\":" << (game.paused ? "true" : "false")
        << ",\"player_down\":" << (game.hp <= 0 ? "true" : "false")
        << ",\"training\":" << (game.dollTraining.active ? "true" : "false")
        << ",\"simulation_seconds\":" << Finite(game.time)
        << ",\"world_clock\":" << Quote(game.world.ClockLabel())
        << ",\"world_ready\":" << (game.world.Ready() ? "true" : "false")
        << ",\"world_error\":" << Quote(game.world.error)
        << ",\"radio_connected\":" << (game.townRadioConnected ? "true" : "false")
        << "},\"counts\":{\"npcs_total\":" << total << ",\"npcs_active\":" << aliveNpcs
        << ",\"npcs_incapacitated\":" << total - aliveNpcs << ",\"npcs_remote\":" << away
        << ",\"mobs_alive\":" << aliveMobs << ",\"mob_slots\":" << game.enemies.size()
        << ",\"mobs_respawning\":" << game.enemies.size() - aliveMobs
        << ",\"bandit_groups\":" << game.banditGroups.size()
        << "},\"villages\":[{\"id\":\"frontier\",\"name\":\"변경 마을 / 그리폰 기지\",\"npcs_registered\":"
        << total << ",\"npcs_present\":" << present
        << ",\"npcs_outside\":" << total - present - away << ",\"npcs_remote\":" << away
        << ",\"npcs_incapacitated\":" << total - aliveNpcs << ",\"npcs_injured\":" << injured
        << ",\"npcs_hostile\":" << hostile << ",\"mobs_inside\":" << townMobs
        << ",\"buildings\":" << TownNpc::Buildings().size() << ",\"supplies\":";
    if (game.world.Ready())
        out << game.world.Region().supplies;
    else
        out << "null";
    out << ",\"rescued\":";
    if (game.world.Ready())
        out << game.world.Region().rescued;
    else
        out << "null";
    VillageOverview(out, game);
    out << "}],\"npcs\":[";
    bool first = true;
    for (const auto &n : game.npcs)
    {
        if (!first)
            out << ',';
        first = false;
        const char *states[] = {"대기", "경계", "교전", "후퇴", "전투 불능"};
        out << "{\"id\":" << Quote("town:" + std::to_string(n.id))
            << ",\"village_id\":\"frontier\",\"kind\":\"resident\",\"name\":" << Quote(n.name)
            << ",\"role\":" << Quote(TownNpc::RoleLabel(n.role))
            << ",\"state\":" << Quote(n.hp <= 0 ? "전투 불능" : states[int(n.state)])
            << ",\"action\":"
            << Quote(game.talkingNpc >= 0 && &n == &game.npcs[game.talkingNpc]
                         ? "대화 중"
                         : TownNpc::ActionLabel(n.action))
            << ",\"hp\":" << Finite(n.hp) << ",\"max_hp\":100,\"ammo\":" << n.ammo
            << ",\"location\":"
            << Quote(TownNpc::InTown(n.position.x, n.position.z) ? "마을 내부" : "마을 외곽")
            << ",\"position\":";
        Position(out, n.position.x, n.position.z);
        out << ",\"reason\":" << Quote(n.mind.selectionReason) << ",\"task\":" << Quote(n.mind.task)
            << ",\"memories\":" << n.mind.memories.size()
            << ",\"beliefs\":" << n.mind.knowledge.size() << ",\"trust\":" << Finite(n.trust)
            << ",\"respect\":" << Finite(n.respect)
            << ",\"hostile\":" << (n.hostile ? "true" : "false") << ",\"wallet_scrap\":" << n.scrap
            << ",\"potion_stock\":" << n.potionStock
            << ",\"medical_supplies\":" << n.medicalSupplies
            << ",\"robbery_target\":" << n.robberyTarget
            << ",\"criminal_until_s\":" << n.criminalUntil << ",\"available\":true,\"scores\":{";
        for (int i = 0; i < int(n.scores.size()); ++i)
        {
            if (i)
                out << ',';
            out << Quote(TownNpc::ActionLabel(NpcAction(i))) << ':' << Finite(n.scores[i]);
        }
        out << "}}";
    }
    for (const auto &entry : game.world.Actors())
    {
        const auto &n = entry.second;
        if (!first)
            out << ',';
        first = false;
        out << "{\"id\":" << Quote("world:" + std::to_string(n.id))
            << ",\"village_id\":\"frontier\",\"kind\":\"companion\",\"name\":" << Quote(n.name)
            << ",\"role\":\"서사 동료\",\"state\":"
            << Quote(n.hp <= 0     ? "전투 불능"
                     : n.available ? "기지 활동"
                                   : "원격 임무")
            << ",\"action\":" << Quote(narrative::WorldSimulation::GoalLabel(n.brain.intention))
            << ",\"hp\":" << Finite(n.hp) << ",\"max_hp\":100,\"ammo\":null,\"location\":"
            << Quote(n.available
                         ? (TownNpc::InTown(n.position.x, n.position.z) ? "마을 내부" : "마을 외곽")
                         : "원격 지역")
            << ",\"position\":";
        if (n.available)
            Position(out, n.position.x, n.position.z);
        else
            out << "null";
        auto r = n.relations.find(narrative::PlayerId);
        out << ",\"reason\":" << Quote(n.brain.reason)
            << ",\"task\":" << Quote(narrative::WorldSimulation::TaskLabel(n.task))
            << ",\"memories\":" << n.memories.size()
            << ",\"beliefs\":" << (n.brain.belief.aware ? 1 : 0)
            << ",\"trust\":" << (r == n.relations.end() ? 0 : Finite(r->second.trust))
            << ",\"respect\":" << (r == n.relations.end() ? 0 : Finite(r->second.respect))
            << ",\"hostile\":false,\"available\":" << (n.available ? "true" : "false")
            << ",\"scores\":{";
        for (int i = 0; i < 3; ++i)
        {
            if (i)
                out << ',';
            out << Quote(narrative::WorldSimulation::GoalLabel(narrative::Goal(i))) << ':'
                << Finite(n.brain.desires[i]);
        }
        out << "}}";
    }
    out << "],\"bandits\":[";
    for (std::size_t i = 0; i < game.enemies.size(); ++i)
    {
        const auto &enemy = game.enemies[i];
        if (i)
            out << ',';
        out << "{\"id\":" << i << ",\"band_id\":" << enemy.band << ",\"hp\":" << enemy.hp
            << ",\"max_hp\":" << enemy.maxHp
            << ",\"action\":" << Quote(RpgEnemy::ActionName(enemy.action))
            << ",\"action_label\":" << Quote(RpgEnemy::ActionLabel(enemy.action))
            << ",\"reason\":" << Quote(enemy.reason)
            << ",\"aggression\":" << enemy.traits.aggression
            << ",\"risk_tolerance\":" << enemy.traits.riskTolerance
            << ",\"provoked\":" << (enemy.provokedUntil > game.time ? "true" : "false")
            << ",\"position\":";
        Position(out, enemy.position.x, enemy.position.z);
        out << ",\"scores\":[";
        for (int score = 0; score < 5; ++score)
            out << (score ? "," : "") << Finite(enemy.scores[score]);
        out << "]}";
    }
    out << "],\"objective\":{\"title\":" << Quote(game.PatrolObjective())
        << ",\"level\":" << game.level
        << ",\"target_level\":3,\"items_collected\":" << game.itemsCollected
        << ",\"reported\":" << (game.patrolReported ? "true" : "false")
        << "},\"story\":{\"status\":" << Quote(game.StoryStatus())
        << ",\"next_step\":" << Quote(game.StoryNextStep())
        << "},\"economy\":{\"currency\":\"barter_scrap\",\"player_scrap\":" << game.scrap
        << ",\"recent_events\":[";
    for (std::size_t i = 0; i < game.economyEvents.size(); ++i)
    {
        const auto &event = game.economyEvents[i];
        if (i)
            out << ',';
        out << "{\"kind\":" << Quote(event.kind) << ",\"time_s\":" << event.time
            << ",\"from\":" << event.from << ",\"to\":" << event.to
            << ",\"amount\":" << event.amount << '}';
    }
    out << "]},\"world_event\":{\"state\":" << int(game.world.Event().state) << ",\"outcome\":"
        << Quote(narrative::WorldSimulation::OutcomeLabel(game.world.Event().outcome))
        << ",\"queue_depth\":" << game.world.QueueSize() << "}}";
    std::error_code error;
    std::filesystem::create_directories(destination.parent_path(), error);
    auto temporary = destination;
    temporary += L".tmp";
    bool ok = false;
    {
        std::ofstream file(temporary, std::ios::binary | std::ios::trunc);
        file << out.str();
        file.close();
        ok = bool(file);
    }
    if (ok)
        ok = MoveFileExW(temporary.c_str(), destination.c_str(), MOVEFILE_REPLACE_EXISTING) != 0;
    if (!ok && !failureReported)
        std::fprintf(stderr, "Monitor snapshot write failed; check Profiles directory.\n");
    failureReported = !ok;
}
