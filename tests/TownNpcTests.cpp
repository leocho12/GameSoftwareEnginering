// Link RpgGame, RpgWorld, WorldSimulation, TownNpc, TownKnowledge, DollBehavior (no renderer).
#include "../BaseProject_SE/SimpleGame/RpgGame.h"
#include <cassert>
#include <set>

int main()
{
    RpgGame game;
    game.Reset(42);
    assert(game.npcs.size() == 20 && game.enemies.size() == 32);
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
    assert(game.hp == 100); // The village is a safe zone.
    for (const auto &npc : game.npcs)
        assert(npc.memory.size() <= 16);
}
