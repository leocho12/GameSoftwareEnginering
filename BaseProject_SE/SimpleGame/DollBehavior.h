#pragma once
#include <array>
#include <filesystem>
#include <fstream>
#include <string>

// Phase 1 only: fixed traits drive real actions. No population or learning yet.
struct BehaviorTraits
{
    float aggression = .25f;
    float coverPreference = .55f;
    float rescuePriority = .9f;
    float commandObedience = .55f;
    float autonomy = .85f;
    float Get(int index) const;
    void Adjust(int index, float delta);
};

enum class DollAction
{
    Wait,
    Attack,
    Cover,
    Rescue,
    Retreat
};
enum class DollCommand
{
    None,
    Rescue,
    Hold
};
enum class RescuePhase
{
    Approach,
    Stabilize,
    Evacuate
};
enum class RescueResult
{
    Running,
    Success,
    VictimLost,
    RescuerDown,
    Timeout,
    Aborted
};

struct DollPoint
{
    float x = 0, z = 0;
};

struct DollAI
{
    BehaviorTraits traits;
    DollPoint position;
    float hp = 100;
    bool down = false;
    DollAction action = DollAction::Wait;
    std::array<float, 5> utilities{};
};

struct MissionBehaviorReport
{
    float elapsed = 0, damageTaken = 0, riskExposure = 0, attackTime = 0;
    float decisionCpuMs = 0, updateCpuMs = 0;
    int decisions = 0, autonomousActions = 0, ignoredCommands = 0, alliesRescued = 0;
    bool survived = true, missionSuccess = false;
};

class DollBehaviorPrototype
{
  public:
    DollBehaviorPrototype();
    bool active = false;
    std::array<DollAI, 2> dolls;
    int rescuer = 0, selectedTrait = 0;
    DollCommand command = DollCommand::None;
    RescuePhase phase = RescuePhase::Approach;
    RescueResult result = RescueResult::Running;
    MissionBehaviorReport report;
    float threat = .8f, stabilize = 0;
    unsigned long long round = 0;
    static constexpr DollPoint Extraction{1, 3}, Cover{1, 1}, Threat{5, 5};
    void SetLogDirectory(const std::filesystem::path &directory);
    void Start(bool switchRoles = false);
    void Stop();
    void Update(float dt);
    void SetCommand(DollCommand value);
    void AdjustTrait(float delta);
    void RestoreTraits();
    static const char *ActionName(DollAction action);
    static const char *ActionLabel(DollAction action);
    static const char *TraitName(int index);
    static const char *TraitLabel(int index);
    static const char *ResultName(RescueResult value);
    const char *StatusLabel() const;

  private:
    float decisionRemaining = 0;
    bool updating = false;
    unsigned long long session = 0, logSequence = 0;
    const char *selectionReason = "not_evaluated";
    std::filesystem::path logDirectory;
    std::ofstream log;
    void Step(float dt);
    void Decide();
    void Finish(RescueResult value);
    void WriteEvent(const char *event);
    static bool Move(DollPoint &position, DollPoint goal, float distance);
};
