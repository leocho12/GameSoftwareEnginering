#pragma once
#include "DollBehavior.h"
#include "TownNpc.h"
#include "WorldSimulation.h"
#include <array>
#include <cstdint>
#include <random>
#include <string>
#include <vector>

struct WorldPoint
{
    float x = 0, z = 0;
};

struct RpgEnemy
{
    WorldPoint position;
    int hp = 40, maxHp = 40;
    float cooldown = 0, respawn = 0;
};

struct RpgDrop
{
    WorldPoint position;
    bool potion = false;
};

struct RpgShot
{
    WorldPoint from, to;
    float life = .12f;
};

class RpgGame
{
  public:
    // Bounded enemy navigation window; this is not the world size.
    static constexpr int Size = 32;
    enum class Ground
    {
        Dirt,
        Tree,
        Rock
    };
    RpgGame();
    void Reset(uint32_t newSeed);
    void Update(float dt);
    void ClearInput();
    void Attack();
    void Pickup();
    void Heal();
    void InteractNpc();
    void NpcChoice(int choice);
    int NearestNpc() const;
    std::vector<TownNpc> npcs;
    int talkingNpc = -1;
    narrative::WorldSimulation world;
    std::map<narrative::ActorId, TownNpc> worldBodies;
    narrative::ActorId talkingActor = 0;
    bool worldDebug = false;
    narrative::ActorId selectedWorldActor = 0;
    bool InteractWorldActor();
    void WorldChoice(int choice);
    bool WorldRescue();
    void FastForwardWorld();
    bool npcDebug = false;
    bool townRadioConnected = true;
    void HelpSelectedNpc();
    void AddExperience(int amount);
    bool Walkable(int x, int z) const;
    bool Fits(WorldPoint p) const;
    bool Connected() const;
    bool LineOfSight(WorldPoint a, WorldPoint b) const;
    int NextExperience() const;
    int MaxHp() const;
    int AttackPower() const;
    int Defense() const;
    Ground Terrain(int x, int z) const;

    DollBehaviorPrototype dollTraining;
    uint32_t seed = 0;
    WorldPoint player{3, 3}, aim{4, 3};
    std::array<bool, 256> keys{};
    bool firing = false, paused = false, inventory = false, moving = false;
    int level = 1, experience = 0, totalExperience = 0, hp = 100;
    int potions = 3, scrap = 0, kills = 0;
    float time = 0, attackCooldown = 0, attackStarted = -100, hurt = 0;
    std::string message;
    std::vector<RpgEnemy> enemies;
    std::vector<RpgDrop> drops;
    std::vector<RpgShot> shots;
    std::vector<WorldPoint> fires;

  private:
    std::mt19937 random;
    float messageTime = 0;
    void Move(WorldPoint &p, float x, float z);
    WorldPoint Spawn();
    void Notify(const std::string &text);
    void Step(float dt);
    float npcUpdateCpuMs = 0, npcDecisionCpuMs = 0, npcPathCpuMs = 0, npcMetricsTimer = 0;
    int npcDecisions = 0, npcPathReplans = 0;
    void InitializeWorld();
    void UpdateWorld(float dt);
    void LogWorld();
    std::size_t worldLogCursor = 0;
    void InitializeTown();
    void UpdateTown(float dt);
    void PerceiveNpc(TownNpc &npc);
    void CoordinateTown();
    void DecideNpc(TownNpc &npc);
    float townCoordinationTimer = 0;
    void MoveNpc(TownNpc &npc, WorldPoint target, float dt);
    void LogNpc(const TownNpc &npc, const char *event) const;
};
