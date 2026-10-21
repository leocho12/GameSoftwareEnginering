// Link RPG sources including RpgEconomy.cpp and RpgBandits.cpp; no renderer.
#include "../BaseProject_SE/SimpleGame/RpgGame.h"
#include <cassert>
#include <limits>

namespace
{
void Isolate(RpgGame &game)
{
    game.Reset(42);
    game.enemies.clear();
    game.banditGroups.clear();
    game.npcs.clear();
    game.world = narrative::WorldSimulation{};
    game.worldBodies.clear();
}

TownNpc Resident(int id, NpcRole role, float x, float z, int scrap)
{
    TownNpc npc;
    npc.id = id;
    npc.role = role;
    npc.position = npc.home = {x, z};
    npc.scrap = scrap;
    return npc;
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
    int from = 4, to = 1;
    assert(game.TransferScrap(from, to, 3) && from == 1 && to == 4);
    assert(!game.TransferScrap(from, to, 2));
    assert(!game.TransferScrap(from, to, -1));
    assert(!game.TransferScrap(from, from, 1));
    to = std::numeric_limits<int>::max();
    assert(!game.TransferScrap(from, to, 1));

    Isolate(game);
    auto merchant = Resident(0, NpcRole::Merchant, 3, 3, 5);
    merchant.potionStock = 1;
    game.npcs.push_back(merchant);
    game.talkingNpc = 0;
    game.NpcChoice(1);
    assert(game.scrap == 7 && game.npcs[0].scrap == 8);
    assert(game.potions == 4 && game.npcs[0].potionStock == 0);
    game.NpcChoice(1);
    assert(game.scrap == 7 && game.potions == 4);
    game.NpcChoice(2);
    assert(game.scrap + game.npcs[0].scrap == 15);
    assert(game.potions == 3 && game.npcs[0].potionStock == 1);
    game.npcs[0].scrap = 0;
    game.NpcChoice(2);
    assert(game.potions == 3); // An insolvent merchant cannot create sale proceeds.

    Isolate(game);
    game.npcs = {Resident(0, NpcRole::Outlaw, 3, 3, 2),
                 Resident(1, NpcRole::Merchant, 4, 3, 8),
                 Resident(2, NpcRole::Guard, 3, 4, 8)};
    assert(game.ResolveRobbery(0, 1));
    assert(game.npcs[0].scrap == 5 && game.npcs[1].scrap == 5);
    assert(!game.ResolveRobbery(0, 1)); // Cooldown prevents repeated transfers.
    Advance(game, 1);
    assert(game.npcs[2].criminalTarget == 0);
    assert(game.npcs[2].action == NpcAction::Attack);
    game.npcs[0].hp = 0;
    Advance(game, 1);
    assert(game.npcs[0].scrap == 2 && game.npcs[1].scrap == 8);
    game.RecoverStolenGoods(game.npcs[0]);
    assert(game.npcs[1].scrap == 8); // Recovery is idempotent.

    Isolate(game);
    game.npcs = {Resident(0, NpcRole::Outlaw, 3, 3, 2), Resident(1, NpcRole::Merchant, 4, 3, 8)};
    game.npcs[0].traits.aggression = .9f;
    Advance(game, 145);
    assert(game.npcs[1].scrap == 5); // Autonomous selection, approach and robbery channel.

    Isolate(game);
    game.npcs.push_back(Resident(0, NpcRole::Guard, 3, 3, 8));
    game.InteractNpc();
    Advance(game, 120);
    assert(game.npcs[0].position.x == 3 && game.npcs[0].position.z == 3);
    assert(!game.npcs[0].moving && game.time > 1); // Conversation does not freeze the world.
    game.talkingNpc = -1;
    Advance(game, 120);
    assert(game.npcs[0].position.x != 3 || game.npcs[0].position.z != 3);

    Isolate(game);
    game.npcs = {Resident(0, NpcRole::Outlaw, 3, 3, 2), Resident(1, NpcRole::Merchant, 4, 3, 8)};
    game.talkingNpc = 1;
    assert(!game.ResolveRobbery(0, 1));
}
