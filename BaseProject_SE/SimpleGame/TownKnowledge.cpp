#include "stdafx.h"
#include "RpgGame.h"
#include <algorithm>
#include <cmath>

namespace
{
float Distance(DollPoint a, DollPoint b)
{
    return std::hypot(a.x - b.x, a.z - b.z);
}

bool CanCommunicate(const TownNpc &a, const TownNpc &b, bool radio)
{
    return (radio && a.radio && b.radio) || Distance(a.position, b.position) <= 6;
}
}

void RpgGame::PerceiveNpc(TownNpc &npc)
{
    for (int i = 0; i < int(enemies.size()); ++i)
    {
        const auto &e = enemies[i];
        // Observers do not treat a distant, non-hostile patrol as an active combat target.
        if (e.action == BanditAction::Attack &&
            Distance(npc.position, {e.position.x, e.position.z}) <= 10 &&
            LineOfSight({npc.position.x, npc.position.z}, e.position))
            npc.mind.Observe({NpcFact::Enemy,
                              i,
                              npc.id,
                              {e.position.x, e.position.z},
                              float(e.hp),
                              1,
                              time,
                              true,
                              true});
    }
    for (const auto &other : npcs)
        if (other.id != npc.id && Distance(npc.position, other.position) <= 8 &&
            LineOfSight({npc.position.x, npc.position.z}, {other.position.x, other.position.z}))
            npc.mind.Observe({NpcFact::Ally,
                              other.id,
                              npc.id,
                              other.position,
                              other.hp,
                              1,
                              time,
                              true,
                              other.hostile,
                              other.scrap});
    for (const auto &other : npcs)
        if (other.criminalUntil > time && npc.mind.Find(NpcFact::Crime, other.id, time) &&
            Distance(npc.position, other.position) < 8 &&
            LineOfSight({npc.position.x, npc.position.z}, {other.position.x, other.position.z}))
            npc.mind.Observe(
                {NpcFact::Crime, other.id, npc.id, other.position, other.hp, 1, time, true, true});
    if (Distance(npc.position, {player.x, player.z}) <= 8 &&
        LineOfSight({npc.position.x, npc.position.z}, player))
        npc.mind.Observe({NpcFact::Commander,
                          -1,
                          npc.id,
                          {player.x, player.z},
                          float(hp),
                          1,
                          time,
                          true,
                          false});
    npc.mind.Expire(time);
}

void RpgGame::CoordinateTown()
{
    // Snapshot before communication: second-hand facts cannot cascade across town this tick.
    std::vector<std::vector<NpcBelief>> reports(npcs.size());
    for (const auto &source : npcs)
        if (source.hp > 0 && !source.hostile)
            for (const auto &entry : source.mind.knowledge)
                if (entry.second.direct && entry.second.Confidence(time) > .6f)
                    reports[source.id].push_back(entry.second);
    for (auto &observer : npcs)
    {
        observer.mind.Expire(time);
        observer.mind.assignedPatient = -1;
        observer.mind.rescueBid = 0;
        if (observer.hp <= 0 || observer.hostile)
            continue;
        for (const auto &source : npcs)
        {
            if (source.id == observer.id || !CanCommunicate(observer, source, townRadioConnected))
                continue;
            float trust = observer.mind.relationships[source.id].trust;
            for (auto fact : reports[source.id])
            {
                fact.source = source.id;
                fact.direct = false;
                // Original observation timestamp is retained; retelling never refreshes evidence.
                fact.confidence *= .55f + .3f * trust;
                observer.mind.Observe(fact);
            }
        }
    }

    // Greedy deterministic role auction. A receiver cannot win two jobs in the same auction.
    // Disconnected groups can independently attempt the same rescue, as they cannot coordinate.
    struct Bid
    {
        int actor, patient;
        float score;
    };

    std::vector<Bid> bids;
    for (auto &npc : npcs)
    {
        if (npc.hp < 25 || npc.hostile || npc.traits.autonomy <= .5f)
            continue;
        for (const auto &entry : npc.mind.knowledge)
        {
            const auto &fact = entry.second;
            if (fact.kind != NpcFact::Ally || fact.subject == npc.id || fact.hp >= 55 ||
                fact.hostile || fact.Confidence(time) <= .25f)
                continue;
            float score = npc.traits.rescuePriority * .5f + npc.traits.teamSupport * .2f +
                          npc.mind.relationships[fact.subject].debt * .2f + npc.hp * .001f -
                          Distance(npc.position, fact.position) * .015f;
            if (score > .35f)
                bids.push_back({npc.id, fact.subject, score});
        }
    }
    std::sort(bids.begin(),
              bids.end(),
              [](const Bid &a, const Bid &b)
              {
                  if (a.score != b.score)
                      return a.score > b.score;
                  if (a.actor != b.actor)
                      return a.actor < b.actor;
                  return a.patient < b.patient;
              });
    std::vector<Bid> awarded;
    for (const auto &bid : bids)
    {
        if (npcs[bid.actor].mind.assignedPatient >= 0)
            continue;
        bool claimed = false;
        for (const auto &owner : awarded)
            if (owner.patient == bid.patient &&
                CanCommunicate(npcs[owner.actor], npcs[bid.actor], townRadioConnected))
                claimed = true;
        if (claimed)
            continue;
        npcs[bid.actor].mind.assignedPatient = bid.patient;
        npcs[bid.actor].mind.rescueBid = bid.score;
        awarded.push_back(bid);
    }
}

void RpgGame::HelpSelectedNpc()
{
    if (paused || hp <= 0 || dollTraining.active || talkingNpc < 0)
        return;
    auto &npc = npcs[talkingNpc];
    if (Distance(npc.position, {player.x, player.z}) > 3 ||
        !LineOfSight(player, {npc.position.x, npc.position.z}))
        return;
    if (npc.hp >= 100 || potions <= 0)
    {
        Notify("부상 주민과 회복약이 있어야 지원할 수 있습니다.");
        return;
    }
    --potions;
    npc.hp = std::min(100.0f, npc.hp + 50);
    npc.recovery = 0;
    npc.mind.RescuedBy(-1, npc.id, time);
    npc.trust = npc.mind.relationships[-1].trust;
    npc.respect = npc.mind.relationships[-1].respect;
    npc.Remember("commander_saved_me");
    LogNpc(npc, "commander_saved_npc");
    Notify(npc.name + ": 도와주신 일을 기억하겠습니다.");
}
