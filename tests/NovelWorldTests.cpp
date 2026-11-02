// Standalone regression entry point. Link the active RPG sources, excluding SimpleGame.cpp.
#include "../BaseProject_SE/SimpleGame/RpgGame.h"
#include <cassert>
#include <cmath>
#include <set>

namespace
{
void Advance(RpgGame &game, float seconds)
{
    for (int i = 0; i < int(seconds * 60); ++i)
        game.Update(1.0f / 60);
}
}

int main()
{
    RpgGame game;
    for (unsigned seed = 0; seed < 8; ++seed)
    {
        game.Reset(seed);
        assert(game.npcs.size() == 100);
        assert(game.enemies.size() == 200);
        assert(!game.world.Ready() && game.worldBodies.empty());
        assert(game.fires.size() == 5);
        for (int v = 0; v < 5; ++v)
        {
            int count = 0;
            for (const auto &n : game.npcs)
                if (n.homeVillage == v)
                {
                    ++count;
                    assert(game.Fits({n.position.x, n.position.z}));
                    if (!game.IsRescueActor(n.id))
                        assert(novel::VillageAt(n.position.x, n.position.z) == v);
                    else
                        assert(novel::VillageAt(n.position.x, n.position.z) < 0);
                    for (const auto &k : n.rumors)
                        assert(k.rumor == v);
                }
            assert(count == 20);
        }
        for (int route = 0; route < 4; ++route)
        {
            int count = 0;
            for (const auto &enemy : game.enemies)
                if (enemy.band / 10 == route)
                {
                    ++count;
                    if (enemy.hp > 0)
                    {
                        assert(game.Fits(enemy.position));
                        assert(!TownNpc::InTown(enemy.position.x, enemy.position.z));
                    }
                }
            assert(count == 50);
            float distance = novel::Villages()[route + 1].x - novel::Villages()[route].x;
            assert(std::abs(distance / novel::PlayerSpeed - 20) < .001f);
        }
        for (int x = -8; x <= 302; ++x)
            assert(game.Fits({float(x), 3}));
    }
    game.Reset(42);
    // Even forcing the obsolete radio flag cannot transmit a rumor across villages.
    game.townRadioConnected = true;
    for (auto &n : game.npcs)
        n.courier = false;
    Advance(game, 2);
    for (const auto &n : game.npcs)
        for (const auto &k : n.rumors)
            for (const auto &rumor : game.rumors)
                if (rumor.id == k.rumor)
                    assert(rumor.originVillage == n.homeVillage);
    auto &victim = game.enemies.front();
    auto death = victim.position;
    victim.hp = 0;
    victim.respawn = game.expeditionState.clock.SecondsToDawn();
    game.player = {3, 3};
    Advance(game, 3.2f);
    assert(victim.hp == 0); // Death no longer replenishes a route after three seconds.
    game.expeditionState.clock.minutes = 239;
    victim.respawn = game.expeditionState.clock.SecondsToDawn();
    Advance(game, 1);
    assert(victim.hp > 0);
    assert(std::hypot(victim.position.x - death.x, victim.position.z - death.z) >= 7);
    assert(game.monsterRespawns > 0);
    game.Reset(42);
    game.enemies.clear();
    for (auto &n : game.npcs)
    {
        n.courier = false;
        n.role = NpcRole::Merchant;
        n.home = n.position;
        n.decisionTimer = 0;
    }
    auto &courier = game.npcs[1];
    courier.courier = true;
    courier.position = {novel::Villages()[0].x, 3};
    courier.home = courier.position;
    courier.travelWait = 0;
    auto before = courier.position;
    Advance(game, 1);
    assert(courier.position.x > before.x && courier.position.x - before.x < 1.8f);
    Advance(game, 65);
    assert(courier.arrivalCount > 0);
    bool delivered = false;
    for (const auto &n : game.npcs)
        if (n.id != courier.id && n.homeVillage == 1)
            for (const auto &k : n.rumors)
                delivered |= k.rumor == 0;
    assert(delivered);
    game.visitedVillages.fill(true);
    game.kills = 12;
    assert(!game.QuestReady());
    assert(game.expeditionState.rescue.stage != expedition::RescueStage::Completed);
}
