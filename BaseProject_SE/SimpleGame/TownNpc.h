#pragma once
#include "DollBehavior.h"
#include "NpcMind.h"
#include <array>
#include <deque>
#include <string>
#include <vector>

struct NpcTraits : BehaviorTraits
{
    float riskTolerance = .5f, ammoConservation = .5f, civilianPriority = .5f;
    float retreatPreference = .5f, teamSupport = .5f, objectivePriority = .6f,
          selfPreservation = .5f;
};
enum class NpcRole
{
    Merchant,
    Medic,
    Guard,
    Hunter,
    Outlaw,
    Bully,
    Swindler,
    Helper
};
enum class NpcState
{
    Idle,
    Alert,
    Combat,
    Retreat,
    Disabled
};
enum class NpcAction
{
    Work,
    Patrol,
    Attack,
    Retreat,
    Help,
    Rest
};

struct NpcBuilding
{
    float x, z, width, depth;
    const char *name;
};

struct TownNpc
{
    int id = 0;
    std::string name;
    NpcRole role = NpcRole::Helper;
    NpcTraits traits, personality;
    NpcMind mind;
    float perceptionTimer = 0;
    bool radio = false;
    float helpTraitScore = 0, helpRelationshipScore = 0, helpMemoryScore = 0, helpRiskScore = 0;
    float resistance = .75f, trust = .5f, respect = .5f;
    DollPoint position, home, goal;
    float hp = 100, cooldown = 0, decisionTimer = 0, recovery = 0, serviceCooldown = 0;
    int ammo = 8, enemyTarget = -1, allyTarget = -1, patrol = 0;
    bool hostile = false, moving = false, playerTarget = false;
    NpcState state = NpcState::Idle;
    NpcAction action = NpcAction::Work;
    std::array<float, 6> scores{};
    std::deque<std::string> memory;
    std::vector<DollPoint> path;
    float pathTimer = 0;
    int pathX = 9999, pathZ = 9999;
    int kills = 0, helped = 0;
    static bool InTown(float x, float z);
    static const std::array<NpcBuilding, 6> &Buildings();
    static const char *RoleName(NpcRole role);
    static const char *ActionName(NpcAction action);
    static const char *RoleLabel(NpcRole role);
    static const char *ActionLabel(NpcAction action);
    void Remember(const std::string &event);
};
