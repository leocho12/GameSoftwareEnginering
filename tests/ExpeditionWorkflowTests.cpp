// Link the active RPG sources including RpgExpedition.cpp; exclude SimpleGame/Renderer.
// These are unexecuted integration regressions until built by the project owner.
#include "../BaseProject_SE/SimpleGame/RpgGame.h"
#include <algorithm>
#include <cassert>
#include <cmath>

namespace
{
void Advance(RpgGame &game, float seconds)
{
    for (int i = 0; i < int(seconds * 60); ++i)
        game.Update(1.0f / 60);
}

TownNpc &Find(RpgGame &game, int id)
{
    auto found = std::find_if(game.npcs.begin(),
                              game.npcs.end(),
                              [id](const TownNpc &npc)
                              {
                                  return npc.id == id;
                              });
    assert(found != game.npcs.end());
    return *found;
}
}

int main()
{
    using expedition::RescueStage;
    RpgGame game;
    game.Reset(42);
    game.enemies.clear();
    for (auto &npc : game.npcs)
    {
        npc.courier = false;
        npc.traits.aggression = 0;
    }
    double initial = game.expeditionState.clock.minutes;
    Advance(game, 1);
    assert(game.expeditionState.clock.minutes > initial); // Real-time, no input needed.
    game.paused = true;
    initial = game.expeditionState.clock.minutes;
    Advance(game, 1);
    assert(game.expeditionState.clock.minutes == initial);
    game.paused = false;

    game.player = {3, 3};
    game.talkingNpc = 5; // Actual Aplia guard, not a fabricated quest actor.
    game.npcs[5].position = {3, 3};
    game.NpcChoice(3);
    assert(game.expeditionState.rescue.ClueCount() == 1);
    game.NpcChoice(3);
    assert(game.expeditionState.rescue.ClueCount() == 1);
    game.talkingNpc = 20; // Kadinak merchant with an independent clue.
    game.player = {75, 3};
    game.npcs[20].position = {75, 3};
    game.NpcChoice(3);
    assert(game.expeditionState.rescue.stage == RescueStage::Located);
    game.talkingNpc = -1;

    int actorId = game.expeditionState.rescue.actorId;
    auto &actor = Find(game, actorId);
    game.player = {actor.position.x + 1, actor.position.z};
    if (!game.Fits(game.player))
        game.player = {actor.position.x, actor.position.z};
    assert(game.WorldRescue());
    assert(game.expeditionState.rescue.stage == RescueStage::Escorting);
    auto before = actor.position;
    game.player = {actor.position.x + 10, 3};
    Advance(game, .5f);
    assert(std::hypot(actor.position.x - before.x, actor.position.z - before.z) < 3);
    assert(actor.id == actorId); // The same physical person, never replaced by a spawned double.
    game.player = {3, 3};
    Advance(game, .1f);
    assert(game.expeditionState.rescue.stage == RescueStage::Located); // Lost, not teleported.
    assert(actor.position.x > 80);

    // Set up the final arrival boundary, then exercise normal update + dialogue reporting.
    actor.position = {4, 3};
    actor.path.clear();
    game.expeditionState.rescue.stage = RescueStage::Escorting;
    Advance(game, .1f);
    assert(game.QuestReady());
    game.talkingNpc = 5;
    game.npcs[5].position = {3, 3};
    int money = game.silver;
    game.NpcChoice(3);
    assert(game.silver == money + 12 && game.expeditionState.progress.rescues == 1);
    game.NpcChoice(3);
    assert(game.silver == money + 12 && game.expeditionState.progress.rescues == 1);

    game.talkingNpc = -1;
    game.expeditionState.needs.fatigue = 50;
    game.ToggleRest();
    assert(game.expeditionState.resting);
    Advance(game, 1);
    assert(game.expeditionState.needs.fatigue < 50);
    game.ToggleRest();
    assert(!game.expeditionState.resting);
    game.Reset(42);
    assert(game.expeditionState.rescue.stage == RescueStage::Investigating);
    assert(game.expeditionState.progress.combatWins == 0);
}
