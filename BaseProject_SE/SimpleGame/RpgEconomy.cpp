#include "stdafx.h"
#include "RpgGame.h"
#include <Windows.h>
#include <algorithm>
#include <cmath>
#include <fstream>
#include <limits>
#include <locale>

namespace
{
TownNpc *FindNpc(std::vector<TownNpc> &npcs, int id)
{
    auto found = std::find_if(npcs.begin(),
                              npcs.end(),
                              [id](const TownNpc &npc)
                              {
                                  return npc.id == id;
                              });
    return found == npcs.end() ? nullptr : &*found;
}

float Distance(DollPoint a, DollPoint b)
{
    return std::hypot(a.x - b.x, a.z - b.z);
}
}

bool RpgGame::TransferScrap(int &from, int &to, int amount)
{
    if (&from == &to || amount <= 0 || from < amount ||
        to > std::numeric_limits<int>::max() - amount)
        return false;
    from -= amount;
    to += amount;
    return true;
}

void RpgGame::DecideRobbery(TownNpc &npc)
{
    npc.robberyTarget = -1;
    if ((npc.role != NpcRole::Outlaw && npc.role != NpcRole::Bully) || npc.robberyCooldown > 0 ||
        npc.hp < 35)
        return;
    float best = -1;
    int guards = 0;
    for (const auto &entry : npc.mind.knowledge)
    {
        const auto &fact = entry.second;
        if (fact.kind != NpcFact::Ally || fact.Confidence(time) <= .25f || fact.hp <= 0)
            continue;
        auto *other = FindNpc(npcs, fact.subject);
        if (other && other->role == NpcRole::Guard && Distance(npc.position, fact.position) < 6)
            ++guards;
    }
    for (const auto &entry : npc.mind.knowledge)
    {
        const auto &fact = entry.second;
        auto *victim = FindNpc(npcs, fact.subject);
        if (fact.kind != NpcFact::Ally || fact.Confidence(time) <= .25f || fact.hp <= 0 ||
            fact.carriedScrap <= 0 || !victim || victim->id == npc.id ||
            victim->role == NpcRole::Guard || victim->role == NpcRole::Outlaw ||
            victim->role == NpcRole::Bully ||
            (talkingNpc >= 0 && npcs[talkingNpc].id == victim->id))
            continue;
        float score = .45f + npc.traits.aggression * .4f + (npc.scrap < 4 ? .3f : 0) +
                      std::min(fact.carriedScrap, 10) * .015f - guards * .4f -
                      Distance(npc.position, fact.position) * .015f;
        if (score > best)
        {
            best = score;
            npc.robberyTarget = victim->id;
            npc.goal = fact.position;
        }
    }
    if (npc.robberyTarget >= 0)
        npc.scores[int(NpcAction::Rob)] = best;
}

void RpgGame::StepRobbery(TownNpc &npc, float dt)
{
    auto *victim = FindNpc(npcs, npc.robberyTarget);
    if (!victim || victim->hp <= 0 || victim->scrap <= 0 ||
        (talkingNpc >= 0 && npcs[talkingNpc].id == victim->id))
    {
        npc.robberyProgress = 0;
        npc.decisionTimer = 0;
        return;
    }
    const auto *known = npc.mind.Find(NpcFact::Ally, victim->id, time);
    if (!known)
    {
        npc.robberyProgress = 0;
        npc.decisionTimer = 0;
        return;
    }
    if (Distance(npc.position, victim->position) > 1.25f ||
        !LineOfSight({npc.position.x, npc.position.z}, {victim->position.x, victim->position.z}))
    {
        npc.robberyProgress = 0;
        MoveNpc(npc, {known->position.x, known->position.z}, dt);
        return;
    }
    npc.robberyProgress += dt;
    if (npc.robberyProgress >= 2)
    {
        ResolveRobbery(npc.id, victim->id);
        npc.robberyProgress = 0;
        npc.decisionTimer = 0;
    }
}

bool RpgGame::ResolveRobbery(int robberId, int victimId)
{
    auto *robber = FindNpc(npcs, robberId), *victim = FindNpc(npcs, victimId);
    if (!robber || !victim || robber == victim || robber->hp <= 0 || victim->hp <= 0 ||
        victim->role == NpcRole::Guard || victim->role == NpcRole::Outlaw ||
        victim->role == NpcRole::Bully ||
        (robber->role != NpcRole::Outlaw && robber->role != NpcRole::Bully) ||
        robber->robberyCooldown > 0 || Distance(robber->position, victim->position) > 1.25f ||
        (talkingNpc >= 0 && (npcs[talkingNpc].id == victimId || npcs[talkingNpc].id == robberId)) ||
        !LineOfSight({robber->position.x, robber->position.z},
                     {victim->position.x, victim->position.z}))
        return false;
    int amount = std::min(3, victim->scrap);
    if (!TransferScrap(victim->scrap, robber->scrap, amount))
        return false;
    robber->stolenGoods[victimId] += amount;
    robber->robberyCooldown = 30;
    robber->criminalUntil = time + 30;
    robber->hostile = true;
    victim->robbedUntil = time + 12;
    victim->decisionTimer = 0;
    victim->Remember("robbed_by_" + std::to_string(robberId));
    robber->Remember("robbery_success");
    for (auto &observer : npcs)
        if (observer.id != robberId && observer.hp > 0 &&
            Distance(observer.position, robber->position) < 8 &&
            LineOfSight({observer.position.x, observer.position.z},
                        {robber->position.x, robber->position.z}))
        {
            observer.mind.Observe({NpcFact::Crime,
                                   robberId,
                                   victimId,
                                   robber->position,
                                   robber->hp,
                                   1,
                                   time,
                                   true,
                                   true});
            observer.decisionTimer = 0;
        }
    RecordEconomy("robbery", victimId, robberId, amount);
    if (std::hypot(player.x - robber->position.x, player.z - robber->position.z) < 12 &&
        LineOfSight(player, {robber->position.x, robber->position.z}))
        Notify(robber->name + "이(가) " + victim->name + "의 부품 " + std::to_string(amount) +
               "개를 약탈했습니다. 범죄자를 제압하면 회수됩니다.");
    LogNpc(*robber, "robbery_success");
    LogNpc(*victim, "robbery_victim");
    return true;
}

void RpgGame::RecoverStolenGoods(TownNpc &robber)
{
    for (auto &entry : robber.stolenGoods)
    {
        auto *victim = FindNpc(npcs, entry.first);
        int amount = std::min(entry.second, robber.scrap);
        if (victim && TransferScrap(robber.scrap, victim->scrap, amount))
        {
            entry.second -= amount;
            RecordEconomy("stolen_goods_returned", robber.id, victim->id, amount);
            victim->Remember("stolen_goods_returned");
        }
    }
    for (auto it = robber.stolenGoods.begin(); it != robber.stolenGoods.end();)
        if (it->second == 0)
            it = robber.stolenGoods.erase(it);
        else
            ++it;
    robber.criminalUntil = 0;
}

void RpgGame::RecordEconomy(const char *kind, int from, int to, int amount)
{
    economyEvents.push_back({time, kind, from, to, amount});
    if (economyEvents.size() > 128)
        economyEvents.erase(economyEvents.begin());
    wchar_t executable[MAX_PATH]{};
    GetModuleFileNameW(nullptr, executable, MAX_PATH);
    auto directory = std::filesystem::path(executable).parent_path() / "Profiles";
    std::error_code error;
    std::filesystem::create_directories(directory, error);
    auto path = directory / "economy.jsonl";
    if (std::filesystem::exists(path, error) &&
        std::filesystem::file_size(path, error) > 5 * 1024 * 1024)
    {
        auto previous = directory / "economy.previous.jsonl";
        std::filesystem::remove(previous, error);
        error.clear();
        std::filesystem::rename(path, previous, error);
        if (error)
            return;
    }
    std::ofstream out(path, std::ios::app);
    out.imbue(std::locale::classic());
    auto balance = [&](int id)
    {
        if (id == -1)
            return scrap;
        auto *npc = FindNpc(npcs, id);
        return npc ? npc->scrap : -1;
    };
    out << "{\"event\":\"" << kind << "\",\"game_seconds\":" << time << ",\"from_npc_id\":" << from
        << ",\"to_npc_id\":" << to << ",\"amount\":" << amount
        << ",\"currency\":\"barter_scrap\",\"from_balance\":" << balance(from)
        << ",\"to_balance\":" << balance(to) << ",\"player_scrap\":" << scrap << "}\n";
}
