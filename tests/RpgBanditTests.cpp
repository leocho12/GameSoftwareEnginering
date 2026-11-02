// Link RpgGame, RpgEconomy, RpgBandits, RpgWorld, RpgExpedition, WorldSimulation, TownNpc, TownKnowledge, DollBehavior.
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
    game.Reset(42);
    assert(game.enemies.size() == 200 && game.banditGroups.size() == 40);
    for (int corridor = 0; corridor < 4; ++corridor)
    {
        int count = 0;
        for (const auto &enemy : game.enemies)
            if (enemy.band / 10 == corridor)
            {
                ++count;
                assert(enemy.hp > 0 && game.Fits(enemy.position));
            }
        assert(count == 50);
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

    Isolate(game);
    assert(!game.QuestReady());
    game.visitedVillages.fill(true);
    game.kills = 12;
    assert(!game.QuestReady()); // Old kill/visit counters cannot complete a rescue.
    game.expeditionState.rescue.stage = expedition::RescueStage::Returned;
    assert(game.QuestReady());
    TownNpc guard;
    guard.role = NpcRole::Guard;
    guard.position = {3, 3};
    game.npcs.push_back(guard);
    int silver = game.silver;
    game.InteractNpc();
    assert(game.silver == silver); // Opening the dialogue never awards a quest.
    game.NpcChoice(3);
    assert(game.expeditionState.rescue.stage == expedition::RescueStage::Completed);
    assert(game.silver == silver + 12);
    game.NpcChoice(3);
    assert(game.silver == silver + 12);
    game.Reset(42);
    assert(game.itemsCollected == 0 && !game.QuestReady());
}
