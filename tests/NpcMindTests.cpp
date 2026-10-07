// Standalone CPU model assertions. Build and execution are left to the user.
#include "../BaseProject_SE/SimpleGame/NpcMind.h"
#include <cassert>

int main()
{
    NpcMind observer;
    NpcBelief direct{NpcFact::Enemy, 7, 1, {2, 3}, 60, 1, 10, true, true};
    observer.Observe(direct);
    auto rumor = direct;
    rumor.direct = false;
    rumor.source = 2;
    rumor.confidence = .6f;
    rumor.position = {9, 9};
    observer.Observe(rumor);
    assert(observer.Find(NpcFact::Enemy, 7, 10)->position.x == 2);
    assert(!observer.Find(NpcFact::Enemy, 8, 10));
    assert(!observer.Find(NpcFact::Enemy, 7, 16));
    observer.Expire(16);
    assert(observer.knowledge.empty());
    observer.Observe(rumor);
    assert(!observer.Find(NpcFact::Enemy, 7, 16)); // Retelling does not refresh time.

    NpcMind saved, rescuer;
    saved.RescuedBy(4, 3, 20);
    assert(saved.relationships.at(4).debt > 0);
    assert(rescuer.relationships.empty()); // Relationship is directed.
    assert(saved.RecallRescue(4, 20) > saved.RecallRescue(5, 20));
    assert(saved.RecallRescue(4, 100) < saved.RecallRescue(4, 20));
    saved.RescuedBy(-1, 3, 21);
    assert(saved.memories.back().type == NpcMemoryType::CommanderSavedMe);
    for (int i = 0; i < 50; ++i)
        saved.Remember(NpcMemoryType::SuccessfulMission, 3, -1, 22 + float(i), .1f);
    assert(saved.memories.size() == 24);
    bool retainedRescue = false;
    for (const auto &memory : saved.memories)
        retainedRescue |= memory.type == NpcMemoryType::CommanderSavedMe;
    assert(retainedRescue);
}
