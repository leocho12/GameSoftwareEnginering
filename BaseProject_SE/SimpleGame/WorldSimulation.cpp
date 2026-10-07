#include "WorldSimulation.h"
#include <algorithm>
#include <chrono>
#include <cmath>
#include <fstream>
#include <set>
#include <sstream>
#include <stdexcept>

namespace narrative
{
namespace
{
std::vector<std::string> Fields(std::string line)
{
    if (line.size() >= 3 && static_cast<unsigned char>(line[0]) == 0xef)
        line.erase(0, 3);
    if (!line.empty() && line.back() == '\r')
        line.pop_back();
    std::vector<std::string> result;
    std::istringstream input(line);
    std::string field;
    while (std::getline(input, field, '\t'))
        result.push_back(field);
    return result;
}

float Number(const std::string &value)
{
    std::size_t used = 0;
    float result = std::stof(value, &used);
    if (used != value.size() || !std::isfinite(result))
        throw std::runtime_error("invalid number");
    return result;
}

float Unit(const std::string &value)
{
    float n = Number(value);
    if (n < 0 || n > 1)
        throw std::runtime_error("expected value in [0,1]");
    return n;
}

int Integer(const std::string &value)
{
    float n = Number(value);
    if (n < 1 || n > 100000 || n != std::floor(n))
        throw std::runtime_error("expected positive integer");
    return int(n);
}
}

bool WorldSimulation::Load(const std::filesystem::path &directory)
{
    ready = false;
    profiles.clear();
    dialogue.clear();
    config = Config{};
    std::string file;
    int row = 0;
    try
    {
        file = "actors.tsv";
        std::ifstream input(directory / file);
        if (!input)
            throw std::runtime_error("cannot open file");
        std::string line;
        while (std::getline(input, line))
        {
            ++row;
            auto f = Fields(line);
            if (f.empty() || f[0].empty() || f[0][0] == '#')
                continue;
            if (f.size() != 12)
                throw std::runtime_error("expected 12 columns");
            Actor a;
            a.id = Integer(f[0]);
            a.name = f[1];
            a.home = {Number(f[2]), Number(f[3])};
            a.position = a.home;
            a.rescue = Unit(f[4]);
            a.obedience = Unit(f[5]);
            a.autonomy = Unit(f[6]);
            a.combat = Unit(f[7]);
            a.tactics = Unit(f[8]);
            a.stayPriority = Unit(f[9]);
            if (a.id == PlayerId || a.name.empty() || profiles.count(a.id))
                throw std::runtime_error("duplicate/reserved actor id or empty name");
            ActorId partner = Integer(f[10]);
            a.relations[partner].trust = Unit(f[11]);
            a.relations[PlayerId] = Relationship{};
            profiles.emplace(a.id, a);
        }
        if (profiles.size() != 2)
            throw std::runtime_error("prototype requires two actors");
        for (const auto &p : profiles)
            for (const auto &r : p.second.relations)
                if (r.first != PlayerId && (r.first == p.first || !profiles.count(r.first)))
                    throw std::runtime_error("invalid relationship target");
        file = "event.tsv";
        row = 0;
        std::ifstream settings(directory / file);
        if (!settings)
            throw std::runtime_error("cannot open file");
        std::set<std::string> keys;
        while (std::getline(settings, line))
        {
            ++row;
            auto f = Fields(line);
            if (f.empty() || f[0].empty() || f[0][0] == '#')
                continue;
            if (f.size() != 2 || !keys.insert(f[0]).second)
                throw std::runtime_error("duplicate key or expected two columns");
            const auto &k = f[0];
            if (k == "difficulty")
                config.difficulty = Unit(f[1]);
            else if (k == "exit_x")
                config.exit.x = Number(f[1]);
            else if (k == "exit_z")
                config.exit.z = Number(f[1]);
            else if (k == "search_x")
                config.search.x = Number(f[1]);
            else if (k == "search_z")
                config.search.z = Number(f[1]);
            else
            {
                int n = Integer(f[1]);
                if (k == "minute_seconds")
                    config.minuteSeconds = n;
                else if (k == "signal_minute")
                    config.signalMinute = n;
                else if (k == "decision_minute")
                    config.decisionMinute = n;
                else if (k == "deadline_minutes")
                    config.deadlineMinutes = n;
                else if (k == "travel_minutes")
                    config.travelMinutes = n;
                else if (k == "search_minutes")
                    config.searchMinutes = n;
                else if (k == "return_minutes")
                    config.returnMinutes = n;
                else if (k == "departure_timeout")
                    config.departureTimeout = n;
                else if (k == "supplies")
                    config.supplies = n;
                else
                    throw std::runtime_error("unknown setting");
            }
        }
        if (keys.size() != 14 || config.signalMinute >= config.decisionMinute ||
            config.decisionMinute >= config.deadlineMinutes)
            throw std::runtime_error("missing settings or invalid chronology");
        file = "goals.tsv";
        row = 0;
        std::ifstream goals(directory / file);
        if (!goals)
            throw std::runtime_error("cannot open file");
        const std::array<std::string, 6> terms = {
            "rescue", "autonomy", "relationship", "skill", "risk", "obedience"};
        std::set<std::string> weightKeys;
        while (std::getline(goals, line))
        {
            ++row;
            auto f = Fields(line);
            if (f.empty() || f[0].empty() || f[0][0] == '#')
                continue;
            if (f.size() != 2 || !weightKeys.insert(f[0]).second)
                throw std::runtime_error("invalid goal weight");
            auto found = std::find(terms.begin(), terms.end(), f[0]);
            if (found == terms.end())
                throw std::runtime_error("unknown goal term");
            config.goalWeights[std::distance(terms.begin(), found)] = Unit(f[1]);
        }
        if (weightKeys.size() != terms.size())
            throw std::runtime_error("missing goal weights");
        file = "dialogue.tsv";
        row = 0;
        std::ifstream text(directory / file);
        if (!text)
            throw std::runtime_error("cannot open file");
        while (std::getline(text, line))
        {
            ++row;
            auto f = Fields(line);
            if (f.empty() || f[0].empty() || f[0][0] == '#')
                continue;
            if (f.size() != 2 || f[1].empty() || !dialogue.emplace(f[0], f[1]).second)
                throw std::runtime_error("invalid dialogue row");
        }
        for (const char *key : {"unknown",
                                "request",
                                "accepted",
                                "busy",
                                "success",
                                "partial",
                                "failed",
                                "support",
                                "support_denied",
                                "stay",
                                "report",
                                "player_rescue",
                                "heard_report"})
            if (!dialogue.count(key))
                throw std::runtime_error(std::string("missing dialogue: ") + key);
        ready = true;
        error.clear();
        Reset();
        return true;
    }
    catch (const std::exception &e)
    {
        error = file + ":" + std::to_string(row) + ": " + e.what();
        actors.clear();
        queue.clear();
        journal.clear();
        return false;
    }
}

void WorldSimulation::Reset()
{
    actors = profiles;
    queue.clear();
    journal.clear();
    playerLog.clear();
    event = WorldEvent{};
    region = {};
    minute = 0;
    targetMinute = 0;
    nextOrder = 1;
    playerAware = reported = false;
    nextRecovery = 10;
    updateMs = decisionMs = 0;
    processedEvents = 0;
    if (!ready)
        return;
    event.deadline = config.deadlineMinutes;
    JournalEntry initial;
    initial.kind = "region_initialized";
    initial.suppliesDelta = config.supplies;
    Commit(initial);
    Schedule(config.signalMinute, Scheduled::Kind::Signal);
    Schedule(config.decisionMinute, Scheduled::Kind::Decide);
    Schedule(event.deadline, Scheduled::Kind::Deadline);
}

void WorldSimulation::Schedule(std::int64_t at, Scheduled::Kind kind, ActorId actor)
{
    queue.push_back({at, nextOrder++, actor, kind});
    std::sort(queue.begin(),
              queue.end(),
              [](const auto &a, const auto &b)
              {
                  return a.minute == b.minute ? a.order < b.order : a.minute < b.minute;
              });
}

void WorldSimulation::Advance(double seconds, bool radio)
{
    if (!ready || !std::isfinite(seconds) || seconds < 0)
        return;
    auto start = std::chrono::steady_clock::now();
    radioConnected = radio;
    targetMinute += seconds / config.minuteSeconds;
    int budget = 64;
    while (!queue.empty() && queue.front().minute <= targetMinute && budget-- > 0)
    {
        auto item = queue.front();
        queue.erase(queue.begin());
        minute = item.minute;
        Process(item);
        ++processedEvents;
    }
    if (queue.empty() || queue.front().minute > targetMinute)
        minute = std::int64_t(targetMinute);
    if (minute >= nextRecovery)
    {
        const auto elapsed = minute - nextRecovery + 10;
        nextRecovery = minute + 10;
        for (auto &p : actors)
        {
            auto &a = p.second;
            if (a.task == Task::Resting)
            {
                a.hp = std::min(100.0f, a.hp + float(elapsed) * .2f);
                if (a.hp >= 100)
                {
                    a.task = Task::Idle;
                    a.brain.intention = Goal::Stay;
                }
            }
            for (const auto &source : actors)
                if (source.first != a.id && a.available && source.second.available &&
                    source.second.brain.belief.outcome != Outcome::Unknown &&
                    a.brain.belief.outcome == Outcome::Unknown &&
                    (radioConnected || std::hypot(a.position.x - source.second.position.x,
                                                  a.position.z - source.second.position.z) < 6))
                {
                    a.brain.belief = {
                        true, source.second.brain.belief.outcome, source.first, .7f, minute};
                    Remember(a, source.first, "received_mission_report", .6f);
                }
        }
    }
    updateMs =
        std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - start).count();
}

void WorldSimulation::Process(const Scheduled &item)
{
    auto *a = Find(item.actor);
    switch (item.kind)
    {
    case Scheduled::Kind::Signal:
        if (event.state == EventState::Pending)
        {
            event.state = EventState::Active;
            JournalEntry e;
            e.kind = "event_created";
            Commit(e);
        }
        if (event.state != EventState::Active)
            break;
        if (radioConnected)
        {
            for (auto &p : actors)
            {
                if (!p.second.brain.belief.aware)
                {
                    p.second.brain.belief = {true, Outcome::Unknown, 0, .85f, minute};
                    JournalEntry heard;
                    heard.kind = "signal_received";
                    heard.actor = p.first;
                    Commit(heard);
                }
            }
        }
        else
        {
            Schedule(minute + 10, Scheduled::Kind::Signal);
        }
        break;
    case Scheduled::Kind::Decide:
        Evaluate();
        if (event.state == EventState::Active)
            Schedule(minute + 10, Scheduled::Kind::Decide);
        break;
    case Scheduled::Kind::Arrive:
        if (a && a->task == Task::Travelling)
        {
            a->task = Task::Searching;
            a->taskSince = minute;
            JournalEntry e;
            e.kind = "search_started";
            e.actor = a->id;
            Commit(e);
            Schedule(minute + config.searchMinutes, Scheduled::Kind::Resolve, a->id);
        }
        break;
    case Scheduled::Kind::Resolve:
        if (a && a->task == Task::Searching && event.state == EventState::Active)
            Resolve(false);
        break;
    case Scheduled::Kind::Return:
        if (a && a->task == Task::Returning)
        {
            a->available = true;
            a->position = config.exit;
            a->task = Task::WalkingHome;
            a->taskSince = minute;
            JournalEntry e;
            e.kind = "returned_to_base";
            e.actor = a->id;
            Commit(e);
        }
        break;
    case Scheduled::Kind::Deadline:
        if (event.state == EventState::Active)
        {
            Resolve(false);
            if (radioConnected)
                for (auto &p : actors)
                    p.second.brain.belief = {true, Outcome::Failed, 0, 1, minute};
        }
        break;
    }
}

void WorldSimulation::Evaluate()
{
    auto start = std::chrono::steady_clock::now();
    std::vector<ActorId> candidates;
    for (auto &p : actors)
    {
        auto &a = p.second;
        auto &b = a.brain;
        if (a.task != Task::Idle || a.attempted || !b.belief.aware ||
            event.state != EventState::Active)
            continue;
        b.personalityScore = a.rescue * config.goalWeights[0] + a.autonomy * config.goalWeights[1];
        b.relationshipScore = 0;
        for (const auto &r : a.relations)
            if (r.first != PlayerId)
                b.relationshipScore =
                    std::max(b.relationshipScore,
                             r.second.trust * config.goalWeights[2] + r.second.debt * .1f);
        b.skillScore = (a.combat + a.tactics) * config.goalWeights[3];
        b.riskScore = -config.difficulty * (1 - a.autonomy) * config.goalWeights[4] -
                      (1 - b.belief.confidence) * .02f;
        b.memoryScore = Recall(a, "rescue_success") * .1f;
        b.desires = {a.stayPriority + a.obedience * config.goalWeights[5],
                     b.personalityScore + b.relationshipScore + b.skillScore + b.riskScore +
                         b.memoryScore,
                     a.hp < 40 ? 1.5f : 0};
        Goal next = Goal(
            std::distance(b.desires.begin(), std::max_element(b.desires.begin(), b.desires.end())));
        b.reason = "highest_utility";
        if (minute < b.committedUntil && b.desires[int(next)] - b.desires[int(b.intention)] < .08f)
        {
            next = b.intention;
            b.reason = "commitment";
        }
        b.intention = next;
        b.committedUntil = minute + 15;
        JournalEntry e;
        e.kind = "decision";
        e.actor = a.id;
        Commit(e);
        if (next == Goal::Rescue)
            candidates.push_back(a.id);
    }
    if (!candidates.empty())
    {
        const auto leader = *std::max_element(candidates.begin(),
                                              candidates.end(),
                                              [this](auto a, auto b)
                                              {
                                                  return actors.at(a).brain.desires[1] <
                                                         actors.at(b).brain.desires[1];
                                              });
        for (auto &entry : actors)
        {
            auto &other = entry.second;
            const auto &source = actors.at(leader);
            if (other.id == leader || !other.available || other.attempted ||
                !other.brain.belief.aware ||
                (!radioConnected && std::hypot(other.position.x - source.position.x,
                                               other.position.z - source.position.z) > 6))
                continue;
            if (std::any_of(journal.begin(),
                            journal.end(),
                            [&other](const auto &e)
                            {
                                return e.kind == "cooperation_requested" && e.target == other.id;
                            }))
                continue;
            JournalEntry request;
            request.kind = "cooperation_requested";
            request.actor = leader;
            request.target = other.id;
            Commit(request);
            JournalEntry reply;
            reply.kind = other.brain.intention == Goal::Rescue ? "cooperation_accepted"
                                                               : "cooperation_declined";
            reply.actor = other.id;
            reply.target = leader;
            Commit(reply);
            Remember(other, leader, reply.kind, .5f);
        }
    }
    for (auto id : candidates)
    {
        auto &a = actors.at(id);
        if (region.supplies < 2)
        {
            a.brain.reason = "insufficient_supplies";
            a.brain.intention = Goal::Stay;
            a.attempted = true;
            JournalEntry blocked;
            blocked.kind = "decision";
            blocked.actor = id;
            Commit(blocked);
            continue;
        }
        a.attempted = true;
        a.task = Task::Departing;
        a.taskSince = minute;
        JournalEntry e;
        e.kind = "mission_prepared";
        e.actor = id;
        e.suppliesDelta = -2;
        Commit(e);
    }
    decisionMs =
        std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - start).count();
}

void WorldSimulation::ArrivedAtExit(ActorId id)
{
    auto *a = Find(id);
    if (!a || a->task != Task::Departing)
        return;
    if (event.state != EventState::Active)
    {
        AbortDeparture(id);
        return;
    }
    a->task = Task::Travelling;
    a->taskSince = minute;
    a->available = false;
    event.participants.push_back(id);
    JournalEntry e;
    e.kind = "departed_base";
    e.actor = id;
    Commit(e);
    Schedule(minute + config.travelMinutes, Scheduled::Kind::Arrive, id);
}

void WorldSimulation::AbortDeparture(ActorId id)
{
    auto *a = Find(id);
    if (!a || a->task != Task::Departing)
        return;
    a->task = Task::WalkingHome;
    a->brain.intention = Goal::Stay;
    a->brain.reason = "departure_failed";
    Remember(*a, id, "departure_failed", .7f);
    JournalEntry e;
    e.kind = "departure_failed";
    e.actor = id;
    e.suppliesDelta = 2;
    Commit(e);
}

void WorldSimulation::ArrivedHome(ActorId id)
{
    auto *a = Find(id);
    if (!a || a->task != Task::WalkingHome)
        return;
    a->position = a->home;
    a->task = a->hp < 100 ? Task::Resting : Task::Idle;
    a->brain.intention = a->hp < 100 ? Goal::Recover : Goal::Stay;
}

void WorldSimulation::Resolve(bool byPlayer)
{
    if (event.state != EventState::Active)
        return;
    float power = 0;
    std::vector<ActorId> present;
    for (auto id : event.participants)
    {
        auto &a = actors.at(id);
        if (a.task == Task::Searching)
        {
            power += a.combat * .5f + a.tactics * .35f;
            present.push_back(id);
        }
    }
    if (byPlayer)
        power += 1;
    bool deadline = minute >= event.deadline;
    Outcome result = (!byPlayer && (present.empty() || deadline)) ? Outcome::Failed
                     : power >= config.difficulty                 ? Outcome::Success
                     : power >= config.difficulty * .55f          ? Outcome::Partial
                                                                  : Outcome::Failed;
    JournalEntry e;
    e.kind = "event_resolved";
    e.actor = byPlayer ? PlayerId : (present.empty() ? 0 : present.front());
    e.outcome = result;
    e.rescuedDelta = result == Outcome::Success ? 2 : result == Outcome::Partial ? 1 : 0;
    Commit(e);
    event.outcome = result;
    event.state = deadline ? EventState::Expired : EventState::Resolved;
    for (auto &p : actors)
    {
        auto &a = p.second;
        if (a.task == Task::Departing)
        {
            AbortDeparture(a.id);
            continue;
        }
        if (a.task != Task::Travelling && a.task != Task::Searching)
            continue;
        bool witnessed = std::find(present.begin(), present.end(), a.id) != present.end();
        if (witnessed)
        {
            a.brain.belief = {true, result, a.id, 1, minute};
            Remember(
                a, a.id, result == Outcome::Failed ? "rescue_failed" : "rescue_success", .9f, true);
            JournalEntry hurt;
            hurt.kind = "mission_injury";
            hurt.actor = a.id;
            hurt.injury = result == Outcome::Success ? 0 : 25;
            Commit(hurt);
            for (auto other : present)
                if (other != a.id)
                {
                    JournalEntry relation;
                    relation.kind = "cooperation_result";
                    relation.actor = a.id;
                    relation.target = other;
                    relation.trustDelta = result == Outcome::Failed ? -.05f : .1f;
                    Commit(relation);
                    Remember(a, other, "cooperated", .7f);
                }
        }
        a.task = Task::Returning;
        a.taskSince = minute;
        Schedule(minute + config.returnMinutes, Scheduled::Kind::Return, a.id);
    }
    if (byPlayer)
    {
        playerAware = reported = true;
        Report(Text("player_rescue"));
    }
}

void WorldSimulation::Commit(JournalEntry e)
{
    if (const auto *a = Find(e.actor))
    {
        e.scores = a->brain.desires;
        e.confidence = a->brain.belief.confidence;
        e.intention = int(a->brain.intention);
        e.reason = a->brain.reason;
        e.contributions = {a->brain.personalityScore,
                           a->brain.relationshipScore,
                           a->brain.skillScore,
                           a->brain.riskScore,
                           a->brain.memoryScore};
    }
    e.sequence = journal.size() + 1;
    e.minute = minute;
    journal.push_back(e);
    region.supplies += e.suppliesDelta;
    region.rescued += e.rescuedDelta;
    if (auto *a = Find(e.actor))
    {
        a->hp = std::max(0.0f, a->hp - e.injury);
        if (e.target)
        {
            auto &r = a->relations[e.target];
            r.trust = std::clamp(r.trust + e.trustDelta, 0.0f, 1.0f);
            r.respect = std::clamp(r.respect + e.trustDelta * .5f, 0.0f, 1.0f);
            r.debt = std::clamp(r.debt + std::max(0.0f, e.trustDelta), 0.0f, 1.0f);
        }
    }
}

RegionState WorldSimulation::ReplayRegion(const std::vector<JournalEntry> &entries)
{
    RegionState state;
    std::set<std::uint64_t> applied;
    for (const auto &e : entries)
        if (applied.insert(e.sequence).second)
        {
            state.supplies += e.suppliesDelta;
            state.rescued += e.rescuedDelta;
        }
    return state;
}

void WorldSimulation::Remember(
    Actor &a, ActorId other, const std::string &tag, float importance, bool permanent)
{
    a.memories.push_back({event.id, other, minute, tag, importance, permanent});
    if (a.memories.size() > 24)
    {
        auto it = std::min_element(
            a.memories.begin(),
            a.memories.end(),
            [this](const auto &x, const auto &y)
            {
                return (x.permanent ? 100 : x.importance / (1 + float(minute - x.time) * .001f)) <
                       (y.permanent ? 100 : y.importance / (1 + float(minute - y.time) * .001f));
            });
        if (!it->permanent)
            a.memories.erase(it);
    }
}

float WorldSimulation::Recall(const Actor &a, const std::string &tag) const
{
    float best = 0;
    for (const auto &m : a.memories)
        if (m.tag == tag)
            best = std::max(best, m.importance / (1 + float(minute - m.time) * .001f));
    return best;
}

const Actor *WorldSimulation::Find(ActorId id) const
{
    auto it = actors.find(id);
    return it == actors.end() ? nullptr : &it->second;
}

Actor *WorldSimulation::Find(ActorId id)
{
    auto it = actors.find(id);
    return it == actors.end() ? nullptr : &it->second;
}

void WorldSimulation::SetPosition(ActorId id, Point p, bool moving)
{
    if (auto *a = Find(id))
    {
        a->position = p;
        a->moving = moving;
    }
}

bool WorldSimulation::CanTalk(ActorId id) const
{
    const auto *a = Find(id);
    return a && a->available && a->hp > 0 && a->task != Task::Departing &&
           a->task != Task::WalkingHome;
}

bool WorldSimulation::Talk(ActorId id)
{
    auto *a = Find(id);
    if (!CanTalk(id))
        return false;
    if (reported && event.playerAccepted && a->brain.belief.outcome == Outcome::Unknown)
    {
        a->brain.belief = {true, event.outcome, PlayerId, 1, minute};
        Remember(*a, PlayerId, "commander_rescued_dolls", .9f, true);
        JournalEntry thanks;
        thanks.kind = "commander_report_received";
        thanks.actor = id;
        thanks.target = PlayerId;
        thanks.trustDelta = .1f;
        Commit(thanks);
    }
    if (a->brain.belief.aware && !playerAware)
    {
        playerAware = true;
        Report(Text("request"));
    }
    if (a->brain.belief.outcome != Outcome::Unknown && !reported)
    {
        reported = true;
        Report(Text("report") + OutcomeLabel(a->brain.belief.outcome));
    }
    return true;
}

bool WorldSimulation::Choose(ActorId id, int choice)
{
    auto *a = Find(id);
    if (!CanTalk(id) || !a->brain.belief.aware || event.state != EventState::Active)
        return false;
    if (choice == 1)
    {
        if (!event.participants.empty())
            return false;
        if (event.playerAccepted)
            return false;
        event.playerAccepted = true;
        playerAware = true;
        Report(Text("accepted"));
        return true;
    }
    if (choice == 2)
    {
        if (std::any_of(journal.begin(),
                        journal.end(),
                        [id](const auto &e)
                        {
                            return e.kind == "player_authorized_autonomy" && e.actor == id;
                        }))
            return false;
        JournalEntry e;
        e.kind = "player_authorized_autonomy";
        e.actor = id;
        e.target = PlayerId;
        e.trustDelta = .02f;
        Commit(e);
        a->brain.committedUntil = 0;
        return true;
    }
    return false;
}

bool WorldSimulation::PlayerRescue(Point p)
{
    if (!event.playerAccepted || event.state != EventState::Active || !event.participants.empty() ||
        std::hypot(p.x - config.search.x, p.z - config.search.z) > 1.6f)
        return false;
    Resolve(true);
    return true;
}

std::string WorldSimulation::Dialogue(ActorId id) const
{
    const auto *a = Find(id);
    if (!a)
        return {};
    if (!CanTalk(id))
        return Text("busy");
    const auto &b = a->brain.belief;
    if (b.outcome != Outcome::Unknown && b.source != a->id)
        return Text("heard_report") + OutcomeLabel(b.outcome);
    if (b.outcome == Outcome::Success)
        return Text("success");
    if (b.outcome == Outcome::Partial)
        return Text("partial");
    if (b.outcome == Outcome::Failed)
        return Text("failed");
    if (!b.aware)
        return Text("unknown");
    if (a->brain.intention == Goal::Stay && a->brain.reason != "not_informed")
        return Text("stay");
    return Text("request");
}

std::string WorldSimulation::Text(const std::string &key) const
{
    auto it = dialogue.find(key);
    return it == dialogue.end() ? key : it->second;
}

void WorldSimulation::Report(const std::string &line)
{
    playerLog.push_back(ClockLabel() + " " + line);
    if (playerLog.size() > 12)
        playerLog.erase(playerLog.begin());
}

std::string WorldSimulation::ClockLabel() const
{
    auto absolute = minute + 480;
    return std::to_string(absolute / 1440 + 1) + "일 " + std::to_string(absolute % 1440 / 60) +
           ":" + (absolute % 60 < 10 ? "0" : "") + std::to_string(absolute % 60);
}

const char *WorldSimulation::GoalLabel(Goal g)
{
    return g == Goal::Rescue ? "실종 인형 수색" : g == Goal::Recover ? "부상 회복" : "기지 유지";
}

const char *WorldSimulation::TaskLabel(Task t)
{
    const char *labels[] = {"기지 대기",
                            "출구로 이동",
                            "원격 이동",
                            "원격 수색",
                            "귀환 중",
                            "숙소로 이동",
                            "부상 휴식"};
    return labels[int(t)];
}

const char *WorldSimulation::OutcomeLabel(Outcome o)
{
    const char *labels[] = {"미확인", "2명 구조", "1명 구조 / 일부 실패", "구조 실패"};
    return labels[int(o)];
}
}
