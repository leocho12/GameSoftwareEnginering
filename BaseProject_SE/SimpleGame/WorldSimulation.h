#pragma once
#include <array>
#include <cstdint>
#include <filesystem>
#include <map>
#include <string>
#include <vector>

namespace narrative
{
using ActorId = std::uint64_t;
constexpr ActorId PlayerId = 1;

struct Point
{
    float x = 0, z = 0;
};
enum class Goal
{
    Stay,
    Rescue,
    Recover
};
enum class Task
{
    Idle,
    Departing,
    Travelling,
    Searching,
    Returning,
    WalkingHome,
    Resting
};
enum class EventState
{
    Pending,
    Active,
    Resolved,
    Expired
};
enum class Outcome
{
    Unknown,
    Success,
    Partial,
    Failed
};

struct Relationship
{
    float trust = .5f, respect = .5f, affection = 0, fear = 0, rivalry = 0, debt = 0, loyalty = .5f;
};

struct Belief
{
    bool aware = false;
    Outcome outcome = Outcome::Unknown;
    ActorId source = 0;
    float confidence = 0;
    std::int64_t observed = 0;
};

struct Memory
{
    std::uint64_t sourceEvent = 1;
    ActorId actor = 0;
    std::int64_t time = 0;
    std::string tag;
    float importance = .5f;
    bool permanent = false;
};

struct Brain
{
    Belief belief;
    Goal intention = Goal::Stay;
    std::array<float, 3> desires{};
    float personalityScore = 0, relationshipScore = 0, skillScore = 0, riskScore = 0,
          memoryScore = 0;
    std::int64_t committedUntil = 0;
    std::string reason = "not_informed";
};

struct Actor
{
    ActorId id = 0;
    std::string name;
    Point home, position;
    float rescue = .5f, obedience = .5f, autonomy = .5f, combat = .5f, tactics = .5f;
    float hp = 100, stayPriority = .5f;
    Brain brain;
    std::map<ActorId, Relationship> relations;
    std::vector<Memory> memories;
    Task task = Task::Idle;
    std::int64_t taskSince = 0;
    bool available = true, moving = false, attempted = false;
};

struct Config
{
    int minuteSeconds = 1, signalMinute = 5, decisionMinute = 30, deadlineMinutes = 2880;
    int travelMinutes = 60, searchMinutes = 90, returnMinutes = 60;
    int departureTimeout = 90, supplies = 12;
    float difficulty = .65f;
    Point exit{3, 14}, search{3, 18};
    std::array<float, 6> goalWeights{.55f, .15f, .2f, .1f, .25f, .15f};
};

struct WorldEvent
{
    std::uint64_t id = 1;
    EventState state = EventState::Pending;
    Outcome outcome = Outcome::Unknown;
    std::int64_t deadline = 0;
    std::vector<ActorId> participants;
    bool playerAccepted = false;
};

struct RegionState
{
    int supplies = 0, rescued = 0;
};

// Committed facts, not executable commands. Apply never invokes AI or re-rolls outcomes.
struct JournalEntry
{
    std::uint64_t sequence = 0, eventId = 1;
    std::int64_t minute = 0;
    ActorId actor = 0, target = 0;
    std::string kind;
    int suppliesDelta = 0, rescuedDelta = 0;
    float injury = 0, trustDelta = 0;
    Outcome outcome = Outcome::Unknown;
    std::array<float, 3> scores{};
    std::array<float, 5> contributions{};
    float confidence = 0;
    int intention = 0;
    std::string reason;
};

struct Scheduled
{
    std::int64_t minute = 0;
    std::uint64_t order = 0;
    ActorId actor = 0;
    enum class Kind
    {
        Signal,
        Decide,
        Arrive,
        Resolve,
        Return,
        Deadline
    } kind = Kind::Signal;
};

class WorldSimulation
{
  public:
    bool Load(const std::filesystem::path &directory);
    void Reset();
    void Advance(double seconds, bool radio);
    void ArrivedAtExit(ActorId id);
    void ArrivedHome(ActorId id);
    void AbortDeparture(ActorId id);
    bool Talk(ActorId id);
    bool Choose(ActorId id, int choice);
    bool PlayerRescue(Point position);
    const Actor *Find(ActorId id) const;
    Actor *Find(ActorId id);
    void SetPosition(ActorId id, Point point, bool moving);
    bool CanTalk(ActorId id) const;
    std::string Dialogue(ActorId id) const;
    std::string Text(const std::string &key) const;
    std::string ClockLabel() const;
    static const char *GoalLabel(Goal goal);
    static const char *TaskLabel(Task task);
    static const char *OutcomeLabel(Outcome outcome);
    static RegionState ReplayRegion(const std::vector<JournalEntry> &entries);

    const std::map<ActorId, Actor> &Actors() const
    {
        return actors;
    }

    const std::vector<JournalEntry> &Journal() const
    {
        return journal;
    }

    const std::vector<std::string> &PlayerLog() const
    {
        return playerLog;
    }

    const WorldEvent &Event() const
    {
        return event;
    }

    const RegionState &Region() const
    {
        return region;
    }

    const Config &Settings() const
    {
        return config;
    }

    std::int64_t Minute() const
    {
        return minute;
    }

    std::size_t QueueSize() const
    {
        return queue.size();
    }

    bool Ready() const
    {
        return ready;
    }

    bool PlayerAware() const
    {
        return playerAware;
    }

    bool ResultReported() const
    {
        return reported;
    }

    void SetConversation(ActorId id)
    {
        conversationActor = id;
    }

    std::string error;
    std::int64_t nextRecovery = 0;
    double updateMs = 0, decisionMs = 0;
    unsigned processedEvents = 0;

  private:
    Config config;
    std::map<ActorId, Actor> profiles, actors;
    std::map<std::string, std::string> dialogue;
    std::vector<Scheduled> queue;
    std::vector<JournalEntry> journal;
    std::vector<std::string> playerLog;
    WorldEvent event;
    RegionState region;
    std::int64_t minute = 0;
    double targetMinute = 0;
    std::uint64_t nextOrder = 1;
    bool ready = false, playerAware = false, reported = false, radioConnected = true;
    ActorId conversationActor = 0;
    void Schedule(std::int64_t at, Scheduled::Kind kind, ActorId actor = 0);
    void Process(const Scheduled &item);
    void Evaluate();
    void Resolve(bool byPlayer);
    void Commit(JournalEntry entry);
    void Remember(Actor &actor,
                  ActorId other,
                  const std::string &tag,
                  float importance,
                  bool permanent = false);
    float Recall(const Actor &actor, const std::string &tag) const;
    void Report(const std::string &line);
};
}
