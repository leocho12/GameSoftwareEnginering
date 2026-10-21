#pragma once
#include "DollBehavior.h"
#include <algorithm>
#include <cmath>
#include <deque>
#include <map>
#include <vector>

enum class NpcFact
{
    Enemy,
    Ally,
    Commander,
    Crime
};
enum class NpcMemoryType
{
    CommanderSavedMe,
    CommanderIgnoredMe,
    AllySavedMe,
    SuccessfulMission,
    FailedRescue
};

struct NpcBelief
{
    NpcFact kind = NpcFact::Enemy;
    int subject = -1, source = -1;
    DollPoint position;
    float hp = 0, confidence = 1, timestamp = 0;
    bool direct = true, hostile = false;
    int carriedScrap = 0;

    float Confidence(float now) const
    {
        float age = std::max(0.0f, now - timestamp);
        return age <= 5 ? std::max(0.0f, confidence - age * .12f) : 0;
    }
};

struct NpcRelationship
{
    float trust = .5f, respect = .5f, debt = 0;
};

struct NpcMemoryEvent
{
    NpcMemoryType type = NpcMemoryType::SuccessfulMission;
    int actor = -1, target = -1;
    float timestamp = 0, importance = .5f;
};

struct NpcNeeds
{
    float survive = 0, protectAlly = 0, completeObjective = .5f, obeyCommander = 0;
};

// CPU-only mind model. World truth enters only through observation/events.
struct NpcMind
{
    std::map<std::pair<NpcFact, int>, NpcBelief> knowledge;
    std::map<int, NpcRelationship> relationships;
    std::deque<NpcMemoryEvent> memories;
    NpcNeeds needs;
    int assignedPatient = -1;
    float rescueBid = 0;
    const char *commandStatus = "none";
    const char *selectionReason = "initial";
    const char *task = "idle";
    float commandUntil = 0;

    void Observe(const NpcBelief &fact)
    {
        auto key = std::make_pair(fact.kind, fact.subject);
        auto found = knowledge.find(key);
        if (found == knowledge.end() || fact.timestamp > found->second.timestamp ||
            (fact.timestamp == found->second.timestamp && fact.direct && !found->second.direct))
            knowledge[key] = fact;
    }

    void Expire(float now)
    {
        for (auto it = knowledge.begin(); it != knowledge.end();)
            if (it->second.Confidence(now) <= 0)
                it = knowledge.erase(it);
            else
                ++it;
    }

    const NpcBelief *Find(NpcFact kind, int id, float now) const
    {
        auto it = knowledge.find({kind, id});
        return it != knowledge.end() && it->second.Confidence(now) > .25f ? &it->second : nullptr;
    }

    void Remember(NpcMemoryType type, int actor, int target, float now, float importance)
    {
        memories.push_back({type, actor, target, now, std::clamp(importance, 0.0f, 1.0f)});
        if (memories.size() > 24)
        {
            auto weakest =
                std::min_element(memories.begin(),
                                 memories.end(),
                                 [now](const auto &a, const auto &b)
                                 {
                                     return a.importance / (1 + (now - a.timestamp) * .02f) <
                                            b.importance / (1 + (now - b.timestamp) * .02f);
                                 });
            memories.erase(weakest);
        }
    }

    float RecallRescue(int person, float now) const
    {
        float weight = 0;
        for (const auto &event : memories)
            if (event.type == NpcMemoryType::AllySavedMe && event.actor == person)
                weight += event.importance / (1 + std::max(0.0f, now - event.timestamp) * .01f);
        return std::min(.2f, weight * .1f);
    }

    void RescuedBy(int actor, int self, float now)
    {
        auto &r = relationships[actor];
        r.trust = std::min(1.0f, r.trust + .05f);
        r.respect = std::min(1.0f, r.respect + .04f);
        r.debt = std::min(1.0f, r.debt + .15f);
        Remember(actor == -1 ? NpcMemoryType::CommanderSavedMe : NpcMemoryType::AllySavedMe,
                 actor,
                 self,
                 now,
                 .9f);
    }
};
