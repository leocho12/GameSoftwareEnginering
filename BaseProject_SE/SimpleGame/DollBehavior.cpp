#include "stdafx.h"
#include "DollBehavior.h"
#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <locale>

namespace
{
using Clock = std::chrono::steady_clock;

float CpuMs(Clock::time_point start)
{
    return std::chrono::duration<float, std::milli>(Clock::now() - start).count();
}

float Distance(DollPoint a, DollPoint b)
{
    return std::hypot(a.x - b.x, a.z - b.z);
}
}

float BehaviorTraits::Get(int index) const
{
    switch (index)
    {
    case 0:
        return aggression;
    case 1:
        return coverPreference;
    case 2:
        return rescuePriority;
    case 3:
        return commandObedience;
    default:
        return autonomy;
    }
}

void BehaviorTraits::Adjust(int index, float delta)
{
    float *values[] = {
        &aggression, &coverPreference, &rescuePriority, &commandObedience, &autonomy};
    if (index >= 0 && index < 5 && std::isfinite(delta))
        *values[index] = std::clamp(*values[index] + delta, 0.0f, 1.0f);
}

DollBehaviorPrototype::DollBehaviorPrototype()
{
    session =
        static_cast<unsigned long long>(std::chrono::duration_cast<std::chrono::milliseconds>(
                                            std::chrono::system_clock::now().time_since_epoch())
                                            .count());
    RestoreTraits();
}

void DollBehaviorPrototype::RestoreTraits()
{
    dolls[0].traits = {.25f, .55f, .9f, .55f, .85f};
    dolls[1].traits = {.55f, .7f, .45f, .9f, .2f};
    decisionRemaining = 0;
    if (active)
        WriteEvent("traits_restored");
}

void DollBehaviorPrototype::SetLogDirectory(const std::filesystem::path &directory)
{
    logDirectory = directory;
}

void DollBehaviorPrototype::Start(bool switchRoles)
{
    if (active && result == RescueResult::Running)
        Finish(RescueResult::Aborted);
    if (switchRoles)
        rescuer = 1 - rescuer;
    ++round;
    active = true;
    command = DollCommand::None;
    phase = RescuePhase::Approach;
    result = RescueResult::Running;
    report = {};
    selectionReason = "not_evaluated";
    threat = .8f;
    stabilize = decisionRemaining = 0;
    for (int i = 0; i < 2; ++i)
    {
        dolls[i].position = i == rescuer ? Extraction : DollPoint{4, 3};
        dolls[i].hp = i == rescuer ? 100.0f : 50.0f;
        dolls[i].down = i != rescuer;
        dolls[i].action = DollAction::Wait;
        dolls[i].utilities.fill(-1);
    }
    WriteEvent("round_started");
}

void DollBehaviorPrototype::Stop()
{
    if (active && result == RescueResult::Running)
        Finish(RescueResult::Aborted);
    active = false;
    if (log)
        log.flush();
}

bool DollBehaviorPrototype::Move(DollPoint &p, DollPoint target, float distance)
{
    float length = Distance(p, target);
    if (length <= distance || length < .001f)
    {
        p = target;
        return true;
    }
    p.x += (target.x - p.x) / length * distance;
    p.z += (target.z - p.z) / length * distance;
    return false;
}

void DollBehaviorPrototype::SetCommand(DollCommand value)
{
    if (!active || result != RescueResult::Running)
        return;
    command = value;
    decisionRemaining = 0;
    WriteEvent("command_changed");
}

void DollBehaviorPrototype::AdjustTrait(float delta)
{
    dolls[rescuer].traits.Adjust(selectedTrait, delta);
    decisionRemaining = 0;
    if (active)
        WriteEvent("trait_changed");
}

void DollBehaviorPrototype::Decide()
{
    auto start = Clock::now();
    auto &actor = dolls[rescuer];
    const auto &t = actor.traits;
    const float health = actor.hp / 100, urgency = 1 - dolls[1 - rescuer].hp / 50;
    auto &s = actor.utilities;
    s.fill(-1);
    s[int(DollAction::Wait)] =
        .05f + (command == DollCommand::Hold ? t.commandObedience * .9f : (1 - t.autonomy) * .15f);
    s[int(DollAction::Attack)] =
        threat > .01f ? t.aggression * .75f + threat * .35f - (1 - health) * .3f : -1;
    s[int(DollAction::Cover)] =
        health < .9f ? t.coverPreference * .65f + (1 - health) * .7f + threat * .15f : -1;
    // High autonomy unlocks independent rescue. An explicit rescue command unlocks it for anyone.
    if (t.autonomy > .7f || command == DollCommand::Rescue)
        s[int(DollAction::Rescue)] =
            t.rescuePriority * .85f + t.autonomy * .35f + urgency * .4f -
            threat * t.coverPreference * .4f +
            (command == DollCommand::Rescue ? t.commandObedience * .8f : 0) -
            (command == DollCommand::Hold ? t.commandObedience * 1.1f : 0);
    s[int(DollAction::Retreat)] =
        health < .35f ? (1 - health) * .8f + t.coverPreference * .2f - t.rescuePriority * .15f : -1;
    auto previous = actor.action;
    auto selected = DollAction(std::distance(s.begin(), std::max_element(s.begin(), s.end())));
    // Avoid rapid switching for nearly equal utilities; carrying a casualty is committed.
    selectionReason = "max_utility";
    if (phase == RescuePhase::Evacuate)
    {
        selected = DollAction::Rescue;
        selectionReason = "evacuation_commitment";
    }
    else if (s[int(previous)] >= 0 && s[int(selected)] - s[int(previous)] < .08f)
    {
        selected = previous;
        selectionReason = "switch_hysteresis";
    }
    if (selected != previous)
    {
        if (selected == DollAction::Rescue && command != DollCommand::Rescue)
            ++report.autonomousActions;
        if ((command == DollCommand::Hold && selected != DollAction::Wait) ||
            (command == DollCommand::Rescue && selected != DollAction::Rescue))
            ++report.ignoredCommands;
    }
    actor.action = selected;
    ++report.decisions;
    report.decisionCpuMs += CpuMs(start);
    WriteEvent("decision");
}

void DollBehaviorPrototype::Update(float dt)
{
    if (!active || result != RescueResult::Running || !std::isfinite(dt) || dt <= 0)
        return;
    auto start = Clock::now();
    updating = true;
    float remaining = std::min(dt, .1f);
    while (remaining > 0 && result == RescueResult::Running)
    {
        float step = std::min(remaining, 1.0f / 60);
        Step(step);
        remaining -= step;
    }
    report.updateCpuMs += CpuMs(start);
    updating = false;
    if (result != RescueResult::Running)
    {
        WriteEvent("round_finished");
        if (log)
            log.flush();
    }
}

void DollBehaviorPrototype::Step(float dt)
{
    report.elapsed += dt;
    decisionRemaining -= dt;
    if (decisionRemaining <= 0)
    {
        Decide();
        decisionRemaining = .25f;
    }
    auto &actor = dolls[rescuer];
    auto &victim = dolls[1 - rescuer];
    const bool sheltered =
        Distance(actor.position, Cover) < .15f && actor.action == DollAction::Cover;
    float damage = threat * (sheltered ? .2f : 3.0f) * dt;
    actor.hp = std::max(0.0f, actor.hp - damage);
    report.damageTaken += damage;
    report.riskExposure += threat * (sheltered ? .1f : 1.0f) * dt;
    if (phase != RescuePhase::Evacuate)
        victim.hp = std::max(0.0f, victim.hp - 2 * dt);
    if (actor.hp <= 0)
    {
        Finish(RescueResult::RescuerDown);
        return;
    }
    if (victim.hp <= 0)
    {
        Finish(RescueResult::VictimLost);
        return;
    }
    switch (actor.action)
    {
    case DollAction::Attack:
        threat = std::max(0.0f, threat - .18f * dt);
        report.attackTime += dt;
        break;
    case DollAction::Cover:
        if (Move(actor.position, Cover, 1.8f * dt))
            actor.hp = std::min(100.0f, actor.hp + 7 * dt);
        break;
    case DollAction::Retreat:
        Move(actor.position, Extraction, 2.2f * dt);
        break;
    case DollAction::Rescue:
        if (phase == RescuePhase::Approach && Move(actor.position, victim.position, 1.8f * dt))
        {
            phase = RescuePhase::Stabilize;
            WriteEvent("rescue_contact");
        }
        else if (phase == RescuePhase::Stabilize)
        {
            // A previous cover/retreat action may have moved the rescuer away.
            if (Distance(actor.position, victim.position) > .2f)
            {
                phase = RescuePhase::Approach;
                stabilize = 0;
            }
            else
            {
                stabilize += dt;
                if (stabilize >= 1.5f)
                {
                    phase = RescuePhase::Evacuate;
                    WriteEvent("victim_stabilized");
                }
            }
        }
        else if (phase == RescuePhase::Evacuate)
        {
            bool reached = Move(actor.position, Extraction, 1.2f * dt);
            victim.position = {actor.position.x + .3f, actor.position.z};
            if (reached)
            {
                victim.down = false;
                victim.hp = std::max(victim.hp, 25.0f);
                Finish(RescueResult::Success);
            }
        }
        break;
    case DollAction::Wait:
        break;
    }
    if (result == RescueResult::Running && report.elapsed >= 35)
        Finish(RescueResult::Timeout);
}

void DollBehaviorPrototype::Finish(RescueResult value)
{
    if (result != RescueResult::Running)
        return;
    result = value;
    report.missionSuccess = value == RescueResult::Success;
    report.alliesRescued = report.missionSuccess ? 1 : 0;
    report.survived = dolls[rescuer].hp > 0;
    if (!report.survived)
        dolls[rescuer].down = true;
    if (!updating)
    {
        WriteEvent("round_finished");
        if (log)
            log.flush();
    }
}

const char *DollBehaviorPrototype::ActionName(DollAction value)
{
    const char *names[] = {"wait", "attack", "cover", "rescue", "retreat"};
    return names[int(value)];
}

const char *DollBehaviorPrototype::ActionLabel(DollAction value)
{
    const char *names[] = {"대기", "위협 제압", "엄폐·회복", "동료 구조", "후퇴"};
    return names[int(value)];
}

const char *DollBehaviorPrototype::TraitName(int index)
{
    const char *names[] = {
        "aggression", "cover_preference", "rescue_priority", "command_obedience", "autonomy"};
    return names[std::clamp(index, 0, 4)];
}

const char *DollBehaviorPrototype::TraitLabel(int index)
{
    const char *names[] = {"공격 적극성", "엄폐 선호", "구조 우선도", "명령 순응도", "자율성"};
    return names[std::clamp(index, 0, 4)];
}

const char *DollBehaviorPrototype::ResultName(RescueResult value)
{
    const char *names[] = {
        "running", "success", "victim_lost", "rescuer_down", "timeout", "aborted"};
    return names[int(value)];
}

const char *DollBehaviorPrototype::StatusLabel() const
{
    if (result == RescueResult::Success)
        return "구조 성공: 부상자를 안전 구역으로 이송했습니다.";
    if (result != RescueResult::Running)
        return "구조 실패/중단: 명령이나 Trait를 바꾸고 R로 재시도하세요.";
    if (phase == RescuePhase::Evacuate)
        return "부상자 이송 중 · 안전한 인계를 위해 구조 행동 유지";
    if (phase == RescuePhase::Stabilize && dolls[rescuer].action == DollAction::Rescue)
        return "응급 처치 중 (1.5초)";
    return "부상자 출혈 중 · 구조 명령 또는 자율 판단을 관찰하세요.";
}

void DollBehaviorPrototype::WriteEvent(const char *event)
{
    if (logDirectory.empty())
        return;
    if (!log.is_open())
    {
        std::error_code error;
        std::filesystem::create_directories(logDirectory, error);
        log.open(logDirectory / "adbs.jsonl", std::ios::app);
        log.imbue(std::locale::classic());
        if (!log)
        {
            std::fprintf(stderr, "ADBS: cannot open adbs.jsonl.\n");
            logDirectory.clear();
            return;
        }
    }
    if (log.tellp() > std::streamoff(5 * 1024 * 1024))
    {
        log.close();
        std::error_code error;
        auto previous = logDirectory / "adbs.previous.jsonl";
        std::filesystem::remove(previous, error);
        std::filesystem::rename(logDirectory / "adbs.jsonl", previous, error);
        log.open(logDirectory / "adbs.jsonl", std::ios::trunc);
    }
    const auto &actor = dolls[rescuer];
    const char *commands[] = {"none", "rescue", "hold"};
    log << "{\"schema_version\":1,\"system\":\"adbs_phase1\",\"event\":\"" << event
        << "\",\"session_id\":" << session << ",\"sequence\":" << ++logSequence
        << ",\"round\":" << round << ",\"elapsed_s\":" << report.elapsed
        << ",\"rescuer_id\":" << rescuer << ",\"command\":\"" << commands[int(command)]
        << "\",\"action\":\"" << ActionName(actor.action) << "\",\"result\":\""
        << ResultName(result) << "\",\"selection_reason\":\"" << selectionReason
        << "\",\"rescue_phase\":" << int(phase) << ",\"rescue_unlocked\":"
        << ((actor.traits.autonomy > .7f || command == DollCommand::Rescue) ? "true" : "false")
        << ",\"threat\":" << threat << ",\"rescuer_hp\":" << actor.hp
        << ",\"victim_hp\":" << dolls[1 - rescuer].hp << ",\"traits\":{";
    for (int i = 0; i < 5; ++i)
    {
        if (i)
            log << ',';
        log << '"' << TraitName(i) << "\":" << actor.traits.Get(i);
    }
    log << "},\"utility_scores\":{";
    for (int i = 0; i < 5; ++i)
    {
        if (i)
            log << ',';
        log << '"' << ActionName(DollAction(i)) << "\":" << actor.utilities[i];
    }
    log << "},\"dolls\":[";
    for (int d = 0; d < 2; ++d)
    {
        if (d)
            log << ',';
        log << "{\"id\":" << d << ",\"x\":" << dolls[d].position.x
            << ",\"z\":" << dolls[d].position.z << ",\"hp\":" << dolls[d].hp << ",\"traits\":{";
        for (int i = 0; i < 5; ++i)
        {
            if (i)
                log << ',';
            log << '"' << TraitName(i) << "\":" << dolls[d].traits.Get(i);
        }
        log << "}}";
    }
    log << "],\"report\":{\"rescued_count\":" << report.alliesRescued
        << ",\"autonomous_rescue_count\":" << report.autonomousActions
        << ",\"ignored_command_transitions_count\":" << report.ignoredCommands
        << ",\"damage_taken\":" << report.damageTaken
        << ",\"risk_exposure_s\":" << report.riskExposure
        << ",\"attack_time_s\":" << report.attackTime << ",\"decisions_count\":" << report.decisions
        << ",\"decision_cpu_ms\":" << report.decisionCpuMs
        << ",\"update_cpu_ms\":" << report.updateCpuMs << "}}\n";
}
