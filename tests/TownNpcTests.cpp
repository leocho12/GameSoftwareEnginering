// Link RpgGame, RpgEconomy, RpgBandits, RpgWorld, WorldSimulation, TownNpc, TownKnowledge, DollBehavior.
#include "../BaseProject_SE/SimpleGame/RpgGame.h"
#include <cassert>
#include <set>

namespace
{
void CheckMonsterPursuit(WorldPoint player, WorldPoint outside)
{
    RpgGame game;
    game.Reset(42);
    // Isolate monster pursuit from guards, healing services, and narrative events.
    game.npcs.clear();
    game.world = narrative::WorldSimulation{};
    game.worldBodies.clear();
    game.enemies.clear();
    game.banditGroups.clear();
    game.player = player;
    game.enemies.push_back({outside, 40, 40});
    game.enemies.front().provokedUntil = 100;
    assert(TownNpc::InTown(player.x, player.z));
    assert(!TownNpc::InTown(outside.x, outside.z));
    assert(game.Fits(player) && game.Fits(outside));

    for (int i = 0; i < 180; ++i)
    {
        game.Update(1.0f / 60);
        assert(game.Fits(game.enemies.front().position));
    }
    const auto &enemy = game.enemies.front();
    assert(TownNpc::InTown(enemy.position.x, enemy.position.z));
    assert(game.hp < game.MaxHp()); // Monsters can cross the boundary and attack inside.

    // An enemy already inside must stay in place when it is within attack range.
    game.hp = game.MaxHp();
    game.player = {3, 3};
    game.enemies.front() = {{3.5f, 3}, 40, 40};
    game.enemies.front().provokedUntil = 100;
    game.Update(1.0f / 60);
    assert(game.enemies.front().position.x == 3.5f);
    assert(game.enemies.front().position.z == 3);
    assert(game.hp < game.MaxHp());
}
}

int main()
{
    RpgGame game;
    game.Reset(42);
    assert(game.npcs.size() == 20 && game.enemies.size() == 9);
    std::set<int> roles;
    for (const auto &npc : game.npcs)
    {
        roles.insert(int(npc.role));
        assert(game.Fits({npc.position.x, npc.position.z}));
    }
    assert(roles.size() == 8);
    for (const auto &enemy : game.enemies)
    {
        assert(!TownNpc::InTown(enemy.position.x, enemy.position.z));
        assert(game.Fits(enemy.position));
    }
    game.player = {game.npcs[0].position.x, game.npcs[0].position.z};
    game.talkingNpc = 0;
    game.npcs[0].hp = 40;
    int beforeSupport = game.potions;
    game.HelpSelectedNpc();
    assert(game.npcs[0].hp == 90 && game.potions == beforeSupport - 1);
    assert(game.npcs[0].mind.relationships.at(-1).debt > 0);
    assert(game.npcs[0].mind.memories.back().type == NpcMemoryType::CommanderSavedMe);
    game.Reset(42);
    assert(game.Connected());
    for (const auto &building : TownNpc::Buildings())
        assert(!game.Walkable(int(building.x), int(building.z)));
    assert(game.Walkable(3, 3));

    game.player = {game.npcs[0].position.x, game.npcs[0].position.z};
    game.talkingNpc = 0;
    int potions = game.potions, money = game.scrap;
    game.NpcChoice(1);
    assert(game.potions == potions + 1 && game.scrap == money - 3);
    game.NpcChoice(2);
    assert(game.potions == potions && game.scrap == money - 1);
    game.scrap = 0;
    game.NpcChoice(1);
    assert(game.potions == potions && game.scrap == 0);

    game.player = {game.npcs[15].position.x, game.npcs[15].position.z};
    game.talkingNpc = 15;
    game.scrap = 10;
    game.NpcChoice(2);
    assert(game.scrap == 10);
    game.NpcChoice(1);
    assert(game.scrap == 6 && game.potions == potions);

    game.player = {game.npcs[17].position.x, game.npcs[17].position.z};
    game.talkingNpc = 17;
    game.hp = 10;
    game.NpcChoice(1);
    assert(game.hp == 35);
    game.NpcChoice(1);
    assert(game.hp == 35); // Service cooldown prevents infinite healing.
    game.talkingNpc = -1;
    game.player = {3, 3};
    game.hp = 100;
    for (int i = 0; i < 600; ++i)
        game.Update(1.0f / 60);
    for (const auto &npc : game.npcs)
        assert(npc.memory.size() <= 16);

    CheckMonsterPursuit({3, 13}, {3, 15});
    CheckMonsterPursuit({3, -7}, {3, -9});
    CheckMonsterPursuit({13, 3}, {15, 3});
    CheckMonsterPursuit({-7, 3}, {-9, 3});
}
