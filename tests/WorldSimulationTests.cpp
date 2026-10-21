// CPU-only tests: compile this file with WorldSimulation.cpp, C++17, no OpenGL/PCH.
// Optional argument: path to Assets/AI. Execution remains the user's responsibility.
#include "../BaseProject_SE/SimpleGame/WorldSimulation.h"
#include <cassert>
#include <filesystem>
#include <iostream>

using namespace narrative;

WorldSimulation Create(const std::filesystem::path &assets)
{
    WorldSimulation world;
    if (!world.Load(assets))
        std::cerr << world.error << '\n';
    assert(world.Ready());
    return world;
}

void Drain(WorldSimulation &world, bool radio)
{
    for (int i = 0; i < 20; ++i)
        world.Advance(0, radio);
}

int main(int argc, char **argv)
{
    const auto assets = argc > 1 ? std::filesystem::path(argv[1])
                                 : std::filesystem::path("BaseProject_SE/SimpleGame/Assets/AI");
    auto conversation = Create(assets);
    conversation.SetConversation(1001);
    conversation.Advance(30, true);
    assert(conversation.Find(1001)->task == Task::Idle);
    assert(conversation.Find(1002)->task == Task::Departing);
    conversation.SetConversation(0);
    conversation.Advance(10, true);
    assert(conversation.Find(1001)->task == Task::Departing);
    auto unattended = Create(assets);
    unattended.Advance(30, true);
    assert(unattended.Find(1001)->brain.intention == Goal::Rescue);
    assert(unattended.Find(1002)->brain.intention == Goal::Rescue);
    assert(unattended.Region().supplies == 8);
    assert(!unattended.PlayerAware());
    unattended.ArrivedAtExit(1001);
    unattended.ArrivedAtExit(1001); // Arrival acknowledgement is idempotent.
    unattended.ArrivedAtExit(1002);
    assert(unattended.Event().participants.size() == 2);
    assert(!unattended.CanTalk(1001));
    unattended.Advance(150, true);
    assert(unattended.Event().outcome == Outcome::Success);
    assert(unattended.Region().rescued == 2);
    assert(!unattended.ResultReported());
    assert(unattended.Find(1001)->relations.at(1002).trust > .55f);
    assert(!unattended.Find(1001)->memories.empty());
    unattended.Advance(60, true);
    assert(unattended.Find(1001)->available);
    unattended.ArrivedHome(1001);
    assert(unattended.Talk(1001) && unattended.ResultReported());
    assert(unattended.Dialogue(1001) == unattended.Text("success"));
    unattended.Advance(4000, true);
    Drain(unattended, true);
    assert(unattended.Region().rescued == 2);
    auto history = unattended.Journal();
    history.insert(history.end(), unattended.Journal().begin(), unattended.Journal().end());
    auto replay = WorldSimulation::ReplayRegion(history);
    assert(replay.supplies == unattended.Region().supplies && replay.rescued == 2);

    auto disconnected = Create(assets);
    disconnected.Advance(30, false);
    assert(!disconnected.Find(1001)->brain.belief.aware);
    assert(disconnected.Find(1001)->task == Task::Idle);
    disconnected.Advance(10, true);
    assert(disconnected.Find(1001)->brain.belief.aware);

    auto expires = Create(assets);
    expires.Advance(3000, false);
    Drain(expires, false);
    assert(expires.Event().state == EventState::Expired);
    assert(expires.Region().rescued == 0);
    assert(!expires.Find(1001)->brain.belief.aware);

    auto player = Create(assets);
    player.Advance(5, true);
    assert(player.Talk(1001));
    assert(player.Choose(1001, 1));
    assert(!player.Choose(1001, 1));
    assert(!player.PlayerRescue({0, 0}));
    assert(player.PlayerRescue(player.Settings().search));
    assert(!player.PlayerRescue(player.Settings().search));
    assert(player.Region().rescued == 2 && player.ResultReported());
    assert(player.Talk(1001));
    assert(player.Find(1001)->brain.belief.source == PlayerId);
    assert(player.Find(1001)->relations.at(PlayerId).trust > .5f);
    player.Advance(3000, true);
    Drain(player, true);
    assert(player.Region().rescued == 2);

    auto relation = Create(assets);
    relation.Find(1002)->relations[1001].trust = 0;
    relation.Advance(30, true);
    assert(relation.Find(1001)->brain.intention == Goal::Rescue);
    assert(relation.Find(1002)->brain.intention == Goal::Stay);
    relation.ArrivedAtExit(1001);
    relation.Advance(150, true);
    assert(relation.Event().outcome == Outcome::Partial);
    assert(relation.Find(1001)->hp == 75);
    assert(relation.Find(1002)->relations.at(1001).trust == 0);

    auto blocked = Create(assets);
    blocked.Advance(30, true);
    blocked.AbortDeparture(1001);
    blocked.AbortDeparture(1001);
    assert(blocked.Region().supplies == 10); // One refund, other actor's reservation retained.
    assert(blocked.Find(1001)->brain.reason == "departure_failed");

    auto chunked = Create(assets), single = Create(assets);
    for (int i = 0; i < 300; ++i)
        chunked.Advance(.1, true);
    single.Advance(30, true);
    // Tolerance around fractional representation: cross the same time boundary explicitly.
    chunked.Advance(.001, true);
    single.Advance(.001, true);
    assert(chunked.Find(1001)->brain.intention == single.Find(1001)->brain.intention);
    assert(chunked.Region().supplies == single.Region().supplies);
    auto invalid = Create(assets);
    assert(!invalid.Load(assets / "missing_directory"));
    assert(!invalid.Ready() && invalid.Actors().empty() && !invalid.error.empty());
}
