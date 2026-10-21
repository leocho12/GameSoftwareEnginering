// Link RpgGame, RpgEconomy, RpgBandits, RpgWorld, WorldSimulation, TownNpc, TownKnowledge, DollBehavior.
#include "../BaseProject_SE/SimpleGame/RpgGame.h"
#include <cassert>
#include <cmath>

namespace
{
void Isolate(RpgGame &game)
{
    game.Reset(42);
    game.npcs.clear();
    game.world = narrative::WorldSimulation{};
    game.worldBodies.clear();
    game.banditGroups.clear();
    game.enemies.clear();
    game.player = {3, 3};
}

void Advance(RpgGame &game, int frames)
{
    for (int i = 0; i < frames; ++i)
        game.Update(1.0f / 60);
}
}

int main()
{
    RpgGame game;
    for (unsigned seed = 0; seed < 8; ++seed)
    {
        game.Reset(seed);
        assert(game.enemies.size() == 9 && game.banditGroups.size() == 3);
        for (int band = 0; band < 3; ++band)
        {
            int count = 0;
            const auto &camp = game.banditGroups[band].camp;
            for (const auto &enemy : game.enemies)
                if (enemy.band == band)
                {
                    ++count;
                    assert(enemy.hp > 0 && game.Fits(enemy.position));
                    assert(!TownNpc::InTown(enemy.position.x, enemy.position.z));
                    assert(std::hypot(enemy.position.x - camp.x, enemy.position.z - camp.z) < 4);
                    assert(std::hypot(enemy.position.x - game.player.x,
                                      enemy.position.z - game.player.z) > 14);
                }
            assert(count == 3);
        }
    }
    Isolate(game);
    game.enemies.push_back({{3.5f, 3}, 40, 40});
    Advance(game, 60);
    assert(game.hp == 100);
    assert(game.enemies[0].action == BanditAction::Observe);
    game.aim = game.enemies[0].position;
    game.Attack();
    Advance(game, 1);
    assert(game.enemies[0].action == BanditAction::Attack);
    assert(game.hp < 100);

    Isolate(game);
    game.enemies.push_back({{4, 3}, 5, 40});
    Advance(game, 1);
    assert(game.enemies[0].action == BanditAction::Retreat);
    assert(game.enemies[0].reason == std::string("critical_health"));

    Isolate(game);
    game.enemies = {{{4, 3}, 40, 40}, {{5, 3}, 40, 40}, {{6, 3}, 40, 40}};
    game.enemies[0].band = game.enemies[1].band = 0;
    game.enemies[2].band = 1;
    game.aim = {4, 3};
    game.Attack();
    assert(game.enemies[1].provokedUntil > game.time);
    assert(game.enemies[2].provokedUntil < game.time);

    // A wiped camp cannot refill immediately, even when the player has left it behind.
    Isolate(game);
    game.banditGroups.push_back({{100, 100}, {100, 100}});
    game.enemies.push_back({{100, 100}, 0, 40});
    game.enemies[0].band = 0;
    Advance(game, 120);
    assert(game.enemies[0].hp == 0);
    assert(game.banditGroups[0].respawnTimer > 40);

    Isolate(game);
    assert(!game.PatrolReady());
    game.AddExperience(100);
    assert(game.level == 3 && !game.PatrolReady());
    game.drops = {{{3, 3}, false}, {{3, 3}, true}};
    game.Pickup();
    assert(game.itemsCollected == 2 && game.PatrolReady());
    TownNpc guard;
    guard.role = NpcRole::Guard;
    guard.position = {3, 3};
    game.npcs.push_back(guard);
    int scrap = game.scrap;
    game.InteractNpc();
    assert(game.patrolReported && game.scrap == scrap + 5);
    game.InteractNpc();
    assert(game.scrap == scrap + 5);
    game.Reset(42);
    assert(game.itemsCollected == 0 && !game.patrolReported);
}
