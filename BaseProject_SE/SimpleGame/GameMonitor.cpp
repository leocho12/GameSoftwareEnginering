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

void VillageOverview(std::ostream &out, const RpgGame &game, int index)
{
    const auto &v = novel::Villages()[index];
    const auto &s = game.villageStats[index];
    float rescue = 0, obedience = 0, autonomy = 0;
    int away = 0;
    for (const auto &n : game.npcs)
        if (n.homeVillage == index)
        {
            rescue += n.traits.rescuePriority;
            obedience += n.traits.commandObedience;
            autonomy += n.traits.autonomy;
            away += n.hp > 0 && novel::VillageAt(n.position.x, n.position.z) != index;
        }
    out << "{\"id\":" << Quote(v.id) << ",\"name\":" << Quote(v.name)
        << ",\"description\":" << Quote(v.description) << ",\"population\":" << s.present
        << ",\"npcs_registered\":" << s.registered << ",\"npcs_present\":" << s.present
        << ",\"npcs_outside\":" << away
        << ",\"npcs_remote\":0,\"npcs_incapacitated\":" << s.disabled
        << ",\"npcs_injured\":" << s.injured << ",\"npcs_hostile\":" << s.hostile
        << ",\"mobs_inside\":" << s.monsters
        << ",\"buildings\":6,\"visiting_travelers\":" << s.travelers
        << ",\"total_silver\":" << s.silver << ",\"atmosphere\":{\"label\":" << Quote(s.label)
        << ",\"reason\":" << Quote("현재 현장 인원·부상·교전·지갑·개인 소문 인지에서 계산")
        << ",\"safety\":" << s.safety << ",\"morale\":" << s.morale
        << ",\"prosperity\":" << s.prosperity << ",\"trust\":" << s.trust
        << ",\"calm\":" << s.moods[0] << ",\"anxious\":" << s.moods[1]
        << ",\"frightened\":" << s.moods[2] << ",\"hopeful\":" << s.moods[3]
        << ",\"basis\":\"physical_presence_and_local_knowledge\"}"
        << ",\"tendencies\":{\"rescue_priority\":" << rescue / novel::ResidentsPerVillage
        << ",\"command_obedience\":" << obedience / novel::ResidentsPerVillage
        << ",\"autonomy\":" << autonomy / novel::ResidentsPerVillage << "},\"rumors\":[";
    bool first = true;
    for (const auto &rumor : game.rumors)
    {
        std::vector<const TownNpc *> holders;
        for (const auto &n : game.npcs)
            if (n.hp > 0 && novel::VillageAt(n.position.x, n.position.z) == index &&
                std::any_of(n.rumors.begin(),
                            n.rumors.end(),
                            [&](const auto &k)
                            {
                                return k.rumor == rumor.id;
                            }))
                holders.push_back(&n);
        if (holders.empty())
            continue;
        out << (first ? "" : ",") << "{\"id\":" << rumor.id << ",\"text\":" << Quote(rumor.text)
            << ",\"origin_village_id\":" << Quote(novel::Villages()[rumor.originVillage].id)
            << ",\"holders\":" << holders.size()
            << ",\"confidence\":1,\"basis\":\"physical_contact\",\"sources\":[";
        first = false;
        for (std::size_t i = 0; i < holders.size(); ++i)
            out << (i ? "," : "") << Quote(holders[i]->name);
        out << "]}";
    }
    out << "],\"mood_history\":[";
    bool firstSample = true;
    for (const auto &sample : game.moodHistory[index])
    {
        out << (firstSample ? "" : ",") << "{\"time_s\":" << sample.time
            << ",\"morale\":" << sample.morale << ",\"safety\":" << sample.safety
            << ",\"prosperity\":" << sample.prosperity << ",\"population\":" << sample.population
            << '}';
        firstSample = false;
    }
    out << "]}";
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
    int aliveMobs = 0, aliveNpcs = 0, traveling = 0;
    for (const auto &e : game.enemies)
        aliveMobs += e.hp > 0;
    for (const auto &n : game.npcs)
    {
        aliveNpcs += n.hp > 0;
        traveling += n.destinationVillage >= 0;
    }
    std::ostringstream out;
    out.imbue(std::locale::classic());
    out << std::setprecision(7)
        << "{\"schema_version\":1,\"world_schema_version\":2,\"type\":\"snapshot\",\"session_id\":"
        << GetCurrentProcessId() << ",\"sequence\":" << ++sequence
        << ",\"sent_at_ms\":" << timestamp << ",\"game\":{\"seed\":" << game.seed
        << ",\"paused\":" << (game.paused ? "true" : "false")
        << ",\"player_down\":" << (game.hp <= 0 ? "true" : "false")
        << ",\"training\":false,\"simulation_seconds\":" << Finite(game.time)
        << ",\"world_clock\":" << Quote(game.expeditionState.clock.Label())
        << ",\"world_ready\":true,\"world_error\":\"\",\"radio_connected\":false,\"setting\":\"novel_kingdom\"}"
        << ",\"counts\":{\"npcs_total\":" << game.npcs.size() << ",\"npcs_active\":" << aliveNpcs
        << ",\"npcs_incapacitated\":" << game.npcs.size() - aliveNpcs << ",\"npcs_remote\":0"
        << ",\"npcs_traveling\":" << traveling << ",\"mobs_alive\":" << aliveMobs
        << ",\"mob_slots\":" << game.enemies.size()
        << ",\"mobs_respawning\":" << game.enemies.size() - aliveMobs
        << ",\"bandit_groups\":" << game.banditGroups.size() << "},\"villages\":[";
    for (int i = 0; i < novel::VillageCount; ++i)
    {
        if (i)
            out << ',';
        VillageOverview(out, game, i);
    }
    out << "],\"corridors\":[";
    for (int i = 0; i < novel::CorridorCount; ++i)
    {
        int alive = 0, slots = 0;
        for (const auto &e : game.enemies)
            if (e.band / 10 == i)
            {
                ++slots;
                alive += e.hp > 0;
            }
        out << (i ? "," : "") << "{\"from\":" << Quote(novel::Villages()[i].name)
            << ",\"to\":" << Quote(novel::Villages()[i + 1].name) << ",\"mobs_alive\":" << alive
            << ",\"mob_slots\":" << slots << ",\"respawning\":" << slots - alive
            << ",\"distance_units\":" << novel::VillageSpacing << ",\"nominal_player_seconds\":20}";
    }
    out << "],\"npcs\":[";
    bool first = true;
    for (const auto &n : game.npcs)
    {
        out << (first ? "" : ",");
        first = false;
        int current = novel::VillageAt(n.position.x, n.position.z);
        out << "{\"id\":" << Quote("town:" + std::to_string(n.id))
            << ",\"village_id\":" << Quote(novel::Villages()[n.homeVillage].id)
            << ",\"current_village_id\":"
            << (current >= 0 ? Quote(novel::Villages()[current].id) : "null")
            << ",\"destination_village_id\":"
            << (n.destinationVillage >= 0 ? Quote(novel::Villages()[n.destinationVillage].id)
                                          : "null")
            << ",\"kind\":\"resident\",\"name\":" << Quote(n.name)
            << ",\"role\":" << Quote(TownNpc::RoleLabel(n.role)) << ",\"state\":"
            << Quote(n.hp <= 0                       ? "전투 불능"
                     : n.state == NpcState::Combat   ? "교전"
                     : n.state == NpcState::Retreat  ? "후퇴"
                     : n.state == NpcState::Alert    ? "경계"
                     : n.action == NpcAction::Travel ? "여행"
                     : n.action == NpcAction::Rest   ? "휴식"
                                                     : "대기")
            << ",\"routine\":"
            << Quote(n.routine == NpcRoutine::Sleep  ? "수면"
                     : n.routine == NpcRoutine::Meal ? "식사"
                                                     : "근무")
            << ",\"action\":"
            << Quote(game.talkingNpc == n.id ? "대화 중" : TownNpc::ActionLabel(n.action))
            << ",\"hp\":" << Finite(n.hp) << ",\"max_hp\":100,\"ammo\":" << n.ammo
            << ",\"location\":"
            << Quote(current >= 0 ? novel::Villages()[current].name : "마을 사이 교역로")
            << ",\"position\":";
        Position(out, n.position.x, n.position.z);
        out << ",\"reason\":" << Quote(n.mind.selectionReason) << ",\"task\":" << Quote(n.mind.task)
            << ",\"memories\":" << n.mind.memories.size()
            << ",\"beliefs\":" << n.mind.knowledge.size() << ",\"trust\":" << Finite(n.trust)
            << ",\"respect\":" << Finite(n.respect)
            << ",\"hostile\":" << (n.hostile ? "true" : "false")
            << ",\"wallet_silver\":" << n.silver << ",\"potion_stock\":" << n.potionStock
            << ",\"medical_supplies\":" << n.medicalSupplies
            << ",\"criminal_until_s\":" << n.criminalUntil
            << ",\"courier\":" << (n.courier ? "true" : "false")
            << ",\"physical_arrivals\":" << n.arrivalCount
            << ",\"available\":true,\"known_rumors\":[";
        for (std::size_t i = 0; i < n.rumors.size(); ++i)
        {
            const auto &k = n.rumors[i];
            out << (i ? "," : "") << "{\"rumor_id\":" << k.rumor
                << ",\"source_npc_id\":" << k.sourceNpc << ",\"learned_at_s\":" << k.learnedAt
                << ",\"hops\":" << k.hops << '}';
        }
        out << "],\"scores\":{";
        for (int i = 0; i < int(n.scores.size()); ++i)
            out << (i ? "," : "") << Quote(TownNpc::ActionLabel(NpcAction(i))) << ':'
                << Finite(n.scores[i]);
        out << "}}";
    }
    out << "],\"bandits\":["; // Existing browser transport key; entries are now monsters.
    for (std::size_t i = 0; i < game.enemies.size(); ++i)
    {
        const auto &e = game.enemies[i];
        out << (i ? "," : "") << "{\"id\":" << i << ",\"band_id\":" << e.band
            << ",\"corridor_id\":" << e.band / 10 << ",\"hp\":" << e.hp << ",\"max_hp\":" << e.maxHp
            << ",\"action\":" << Quote(RpgEnemy::ActionName(e.action))
            << ",\"action_label\":" << Quote(RpgEnemy::ActionLabel(e.action))
            << ",\"reason\":" << Quote(e.reason) << ",\"aggression\":" << e.traits.aggression
            << ",\"risk_tolerance\":" << e.traits.riskTolerance
            << ",\"respawn_remaining_s\":" << std::max(0.0f, e.respawn)
            << ",\"provoked\":" << (e.provokedUntil > game.time ? "true" : "false")
            << ",\"position\":";
        Position(out, e.position.x, e.position.z);
        out << ",\"scores\":[";
        for (int score = 0; score < 5; ++score)
            out << (score ? "," : "") << Finite(e.scores[score]);
        out << "]}";
    }
    out << "],\"objective\":{\"title\":" << Quote(game.QuestObjective())
        << ",\"visited_villages\":" << game.VisitedCount() << ",\"kills\":" << game.kills
        << ",\"reported\":"
        << (game.expeditionState.rescue.stage == expedition::RescueStage::Completed ? "true"
                                                                                    : "false")
        << ",\"clues\":" << game.expeditionState.rescue.ClueCount()
        << ",\"stage\":" << int(game.expeditionState.rescue.stage)
        << ",\"actor_id\":" << game.expeditionState.rescue.actorId
        << "},\"survival\":{\"world_minutes\":" << game.expeditionState.clock.minutes
        << ",\"hunger\":" << game.expeditionState.needs.hunger
        << ",\"thirst\":" << game.expeditionState.needs.thirst
        << ",\"fatigue\":" << game.expeditionState.needs.fatigue
        << ",\"stamina\":" << game.expeditionState.needs.stamina
        << ",\"alert_level\":" << game.CurrentAlert()
        << ",\"resting\":" << (game.expeditionState.resting ? "true" : "false")
        << "},\"story\":{\"status\":" << Quote(game.StoryStatus())
        << ",\"next_step\":" << Quote(game.StoryNextStep())
        << "},\"economy\":{\"currency\":\"silver_units\",\"player_silver\":" << game.silver
        << ",\"recent_events\":[";
    for (std::size_t i = 0; i < game.economyEvents.size(); ++i)
    {
        const auto &e = game.economyEvents[i];
        out << (i ? "," : "") << "{\"kind\":" << Quote(e.kind) << ",\"time_s\":" << e.time
            << ",\"from\":" << e.from << ",\"to\":" << e.to << ",\"amount\":" << e.amount << '}';
    }
    out << "]},\"profiling\":{\"cpu_rumor_contact_ms\":" << game.rumorUpdateMs
        << ",\"cpu_village_stats_ms\":" << game.villageStatsMs
        << ",\"cpu_world_update_ms\":" << game.worldUpdateMs
        << ",\"rumor_contact_transfers_total\":" << game.rumorTransfers
        << ",\"courier_physical_arrivals_total\":" << game.courierArrivals
        << ",\"monster_respawns_total\":" << game.monsterRespawns << "}}";
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
