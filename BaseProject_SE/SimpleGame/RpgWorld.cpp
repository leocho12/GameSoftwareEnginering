#include "stdafx.h"
#include "RpgGame.h"
#include "Platform.h"
#include <algorithm>
#include <chrono>
#include <cmath>
#include <fstream>
#include <locale>

namespace
{
float Distance(DollPoint a, DollPoint b)
{
    return std::hypot(a.x - b.x, a.z - b.z);
}

float Milliseconds(std::chrono::steady_clock::time_point start)
{
    return std::chrono::duration<float, std::milli>(std::chrono::steady_clock::now() - start)
        .count();
}
}

int RpgGame::VisitedCount() const
{
    return int(std::count(visitedVillages.begin(), visitedVillages.end(), true));
}

std::string RpgGame::StoryStatus() const
{
    return QuestObjective();
}

std::string RpgGame::StoryNextStep() const
{
    return QuestHint();
}

void RpgGame::InitializeWorld()
{
    // Former story assets are deliberately not loaded. Keep their source as history only.
    world = narrative::WorldSimulation{};
    worldBodies.clear();
    talkingActor = selectedWorldActor = 0;
    worldDebug = false;
    visitedVillages.fill(false);
    visitedVillages[0] = true;
    villageStats = {};
    for (auto &history : moodHistory)
        history.clear();
    rumors.clear();
    rumorTransfers = courierArrivals = monsterRespawns = nextRumorId = 0;
    rumorTimer = statsTimer = worldWindowMs = 0;
    worldUpdateMs = rumorUpdateMs = villageStatsMs = 0;
    worldLogCursor = 0;
    const char *news[] = {"아플리아로 향하는 상단이 호위할 모험가를 찾는다고 한다.",
                          "카디낙 길목의 여관에 여행객이 늘었다고 한다.",
                          "하멜른 주민들이 산길의 마수 흔적을 경계하고 있다고 한다.",
                          "키르멜트 후방으로 보낼 물자가 부족하다고 한다.",
                          "쿠락트 산록의 사냥꾼들이 위험한 발자국을 보았다고 한다."};
    for (int i = 0; i < novel::VillageCount; ++i)
    {
        rumors.push_back({nextRumorId++, i, news[i], 0, 600, i == 1 ? 1 : -1});
        for (int j : {0, 1, 2, 5})
            npcs[i * novel::ResidentsPerVillage + j].rumors.push_back({i, -1, 0, 0});
    }
    fires.clear();
    for (const auto &v : novel::Villages())
        fires.push_back({v.x + 2, v.z + 3});
    UpdateVillageStats();
}

void RpgGame::TravelNpc(TownNpc &npc, float dt)
{
    if (npc.destinationVillage < 0)
    {
        int current = novel::VillageAt(npc.position.x, npc.position.z);
        if (current < 0)
            current = npc.homeVillage;
        if (current != npc.homeVillage)
            npc.destinationVillage = npc.homeVillage;
        else if (current == 0)
            npc.destinationVillage = 1;
        else if (current == novel::VillageCount - 1)
            npc.destinationVillage = current - 1;
        else
            npc.destinationVillage = current + ((npc.id % 2) ? 1 : -1);
        npc.path.clear();
        npc.pathTimer = 0;
    }
    const auto &destination = novel::Villages()[npc.destinationVillage];
    // First reach the clear road, then travel along it. No simulated teleporting.
    WorldPoint goal{destination.x, destination.z};
    if (std::abs(npc.position.z - 3) > .3f)
        goal = {npc.position.x, 3};
    MoveNpc(npc, goal, dt);
    if (Distance(npc.position, {destination.x, destination.z}) < .45f)
    {
        npc.routineKey = -1;
        ++courierArrivals;
        ++npc.arrivalCount;
        npc.travelWait = 12;
        npc.destinationVillage = -1;
        npc.action = NpcAction::Work;
        npc.decisionTimer = 1;
        npc.path.clear();
        npc.Remember("physical_village_arrival");
    }
}

void RpgGame::AddLocalRumor(WorldPoint origin, const std::string &text, int sentiment)
{
    int village = novel::VillageAt(origin.x, origin.z);
    if (village < 0)
        village = std::clamp(int(std::round((origin.x - 3) / novel::VillageSpacing)), 0, 4);
    // Bound repeated incidents and retained knowledge during long play sessions.
    for (const auto &r : rumors)
        if (r.originVillage == village && r.text == text && time - r.createdAt < 20)
            return;
    if (rumors.size() >= 64)
        return;
    Rumor rumor{nextRumorId++, village, text, time, time + 600, sentiment};
    bool witnessed = false;
    for (auto &n : npcs)
        if (n.hp > 0 && Distance(n.position, {origin.x, origin.z}) <= 6 &&
            LineOfSight(origin, {n.position.x, n.position.z}))
        {
            n.rumors.push_back({rumor.id, -1, 0, time});
            witnessed = true;
        }
    if (witnessed)
        rumors.push_back(rumor);
}

void RpgGame::UpdateRumors()
{
    auto start = std::chrono::steady_clock::now();
    rumors.erase(std::remove_if(rumors.begin(),
                                rumors.end(),
                                [&](const Rumor &r)
                                {
                                    return r.expiresAt <= time;
                                }),
                 rumors.end());
    for (auto &n : npcs)
        n.rumors.erase(std::remove_if(n.rumors.begin(),
                                      n.rumors.end(),
                                      [&](const auto &k)
                                      {
                                          return std::none_of(rumors.begin(),
                                                              rumors.end(),
                                                              [&](const Rumor &r)
                                                              {
                                                                  return r.id == k.rumor;
                                                              });
                                      }),
                       n.rumors.end());
    std::map<int, std::vector<RumorKnowledge>> before;
    for (const auto &n : npcs)
        before[n.id] = n.rumors;
    // Snapshot knowledge prevents same-tick chains across the world.
    for (auto &receiver : npcs)
    {
        if (receiver.hp <= 0)
            continue;
        for (const auto &sender : npcs)
        {
            if (sender.id == receiver.id || sender.hp <= 0 ||
                Distance(sender.position, receiver.position) > 4 ||
                !LineOfSight({sender.position.x, sender.position.z},
                             {receiver.position.x, receiver.position.z}))
                continue;
            for (const auto &k : before[sender.id])
            {
                if (std::any_of(receiver.rumors.begin(),
                                receiver.rumors.end(),
                                [&](const auto &known)
                                {
                                    return known.rumor == k.rumor;
                                }))
                    continue;
                receiver.rumors.push_back({k.rumor, sender.id, k.hops + 1, time});
                ++rumorTransfers;
            }
        }
    }
    rumorUpdateMs = Milliseconds(start);
}

void RpgGame::UpdateVillageStats()
{
    auto start = std::chrono::steady_clock::now();
    villageStats = {};
    for (const auto &n : npcs)
    {
        ++villageStats[n.homeVillage].registered;
        villageStats[n.homeVillage].disabled += n.hp <= 0;
        int location = novel::VillageAt(n.position.x, n.position.z);
        if (n.hp <= 0 || location < 0)
            continue;
        auto &s = villageStats[location];
        ++s.present;
        s.injured += n.hp < 80;
        s.hostile += n.criminalUntil > time || n.hostile;
        s.combat += n.state == NpcState::Combat;
        s.travelers += n.homeVillage != location;
        s.silver += n.silver;
        s.trust += n.trust * 100;
        bool anxious = false, hopeful = false;
        for (const auto &k : n.rumors)
            for (const auto &r : rumors)
                if (r.id == k.rumor)
                {
                    anxious |= r.sentiment < 0;
                    hopeful |= r.sentiment > 0;
                }
        s.rumorHolders += !n.rumors.empty();
        int mood = n.hp < 35 || n.robbedUntil > time ? 2 : hopeful ? 3 : anxious ? 1 : 0;
        ++s.moods[mood];
    }
    for (const auto &e : enemies)
    {
        int v = novel::VillageAt(e.position.x, e.position.z);
        if (v >= 0 && e.hp > 0)
            ++villageStats[v].monsters;
    }
    for (int i = 0; i < novel::VillageCount; ++i)
    {
        auto &s = villageStats[i];
        s.trust = s.present ? s.trust / s.present : 0;
        s.safety =
            std::clamp(100.0f - s.monsters * 15 - s.combat * 5 - s.hostile * 8, 0.0f, 100.0f);
        s.morale = s.present ? std::clamp((s.moods[0] * 75.0f + s.moods[1] * 45.0f +
                                           s.moods[2] * 15.0f + s.moods[3] * 90.0f) /
                                              s.present,
                                          0.0f,
                                          100.0f)
                             : 0;
        s.prosperity =
            s.present ? std::clamp(float(s.silver) / (s.present * 12) * 100, 0.0f, 100.0f) : 0;
        s.label = !s.present      ? "인원 없음"
                  : s.safety < 60 ? "위험"
                  : s.morale < 50 ? "불안"
                                  : "평온";
        auto &history = moodHistory[i];
        history.push_back({time, s.morale, s.safety, s.prosperity, s.present});
        if (history.size() > 120)
            history.pop_front();
    }
    villageStatsMs = Milliseconds(start);
}

void RpgGame::UpdateWorld(float dt)
{
    auto start = std::chrono::steady_clock::now();
    int location = novel::VillageAt(player.x, player.z);
    if (location >= 0 && !visitedVillages[location])
    {
        visitedVillages[location] = true;
        Notify(std::string(novel::Villages()[location].name) + " 도착 · 방문 " +
               std::to_string(VisitedCount()) + "/5");
    }
    rumorTimer -= dt;
    statsTimer -= dt;
    if (rumorTimer <= 0)
    {
        UpdateRumors();
        rumorTimer = 1;
    }
    const bool publish = statsTimer <= 0;
    if (publish)
    {
        UpdateVillageStats();
        statsTimer = 1;
    }
    worldWindowMs += Milliseconds(start);
    if (publish)
    {
        worldUpdateMs = worldWindowMs;
        worldWindowMs = 0;
        LogWorld();
    }
}

// Archived narrative adapters stay inert; WorldRescue belongs to the active expedition.
bool RpgGame::InteractWorldActor()
{
    return false;
}

void RpgGame::WorldChoice(int)
{
}

bool RpgGame::WorldRescue()
{
    if (paused || hp <= 0 || talkingNpc >= 0 || inventory || storyJournalOpen)
        return false;
    if (BeginEscort())
        return true;
    WorldPoint trail{novel::Villages()[1].x + 20, 3};
    if (std::hypot(player.x - trail.x, player.z - trail.z) <= 3 && LineOfSight(player, trail))
    {
        LearnRescueClue(2);
        return true;
    }
    return false;
}

void RpgGame::FastForwardWorld()
{
}

void RpgGame::LogWorld()
{
    auto directory = Platform::ExecutableDirectory() / "Profiles";
    std::error_code error;
    std::filesystem::create_directories(directory, error);
    auto path = directory / "novel_world.jsonl";
    if (std::filesystem::exists(path, error) &&
        std::filesystem::file_size(path, error) > 5 * 1024 * 1024)
    {
        auto previous = directory / "novel_world.previous.jsonl";
        std::filesystem::remove(previous, error);
        error.clear();
        std::filesystem::rename(path, previous, error);
        if (error)
            return;
    }
    std::ofstream out(path, std::ios::app);
    out.imbue(std::locale::classic());
    out << "{\"schema_version\":1,\"event\":\"physical_world_simulation\",\"game_seconds\":" << time
        << ",\"npc_count\":" << npcs.size() << ",\"mob_slots\":" << enemies.size()
        << ",\"rumor_contact_transfers_total\":" << rumorTransfers
        << ",\"courier_physical_arrivals_total\":" << courierArrivals
        << ",\"monster_respawns_total\":" << monsterRespawns
        << ",\"cpu_rumor_contact_ms\":" << rumorUpdateMs
        << ",\"cpu_village_stats_ms\":" << villageStatsMs
        << ",\"cpu_world_compute_window_ms\":" << worldUpdateMs << ",\"villages\":[";
    for (int i = 0; i < novel::VillageCount; ++i)
    {
        const auto &s = villageStats[i];
        out << (i ? "," : "") << "{\"id\":\"" << novel::Villages()[i].id
            << "\",\"population\":" << s.present << ",\"safety\":" << s.safety
            << ",\"morale\":" << s.morale << ",\"prosperity\":" << s.prosperity << '}';
    }
    out << "]}\n";
}
