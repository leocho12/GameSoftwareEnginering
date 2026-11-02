#pragma once
#include "DollBehavior.h"
#include "TownNpc.h"
#include "WorldSimulation.h"
#include "ExpeditionRules.h"
#include <array>
#include <cstdint>
#include <random>
#include <string>
#include <vector>

struct WorldPoint
{
    float x = 0, z = 0;
};

enum class BanditAction
{
    Patrol,
    Observe,
    Attack,
    Retreat,
    Return
};

struct RpgEnemy
{
    WorldPoint position;
    int hp = 40, maxHp = 40;
    float cooldown = 0, respawn = 0;
    int band = -1, appearance = 5;
    NpcTraits traits;
    BanditAction action = BanditAction::Patrol;
    std::array<float, 5> scores{};
    WorldPoint lastSeen;
    float decisionTimer = 0, memoryUntil = -1, provokedUntil = -1, attackStarted = -100;
    float alertReportedAt = -100;
    bool moving = false, mirror = false;
    bool seesPlayer = false;
    int nearbyAllies = 0, nearbyGuards = 0;
    float perceivedDanger = 0;
    const char *reason = "routine_patrol";
    // Reuse the NPC's cached locomotion component, not its social AI or personality.
    TownNpc locomotion;
    static const char *ActionName(BanditAction action);
    static const char *ActionLabel(BanditAction action);
};

struct BanditGroup
{
    WorldPoint camp, waypoint;
    float patrolTimer = 0, respawnTimer = 45;
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

struct EconomyEvent
{
    float time = 0;
    std::string kind;
    int from = -1, to = -1, amount = 0;
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
    bool QuestReady() const;
    std::string QuestObjective() const;
    std::string QuestHint() const;
    expedition::State expeditionState;
    WorldPoint rescueSite;
    int CurrentAlert() const;
    bool IsRescueActor(int id) const;
    void ToggleRest();
    void EatRation();
    void DrinkWater();
    std::string StoryStatus() const;
    std::string StoryNextStep() const;
    bool storyJournalOpen = false;
    std::vector<EconomyEvent> economyEvents;
    bool TransferSilver(int &from, int &to, int amount);
    bool ResolveRobbery(int robberId, int victimId);
    void RecoverStolenGoods(TownNpc &robber);
    bool Walkable(int x, int z) const;
    bool Fits(WorldPoint p) const;
    bool Connected() const;
    bool LineOfSight(WorldPoint a, WorldPoint b) const;
    int MaxHp() const;
    int AttackPower() const;
    int Defense() const;
    Ground Terrain(int x, int z) const;

    struct Rumor
    {
        int id = 0, originVillage = 0;
        std::string text;
        float createdAt = 0, expiresAt = 0;
        int sentiment = 0;
    };

    struct VillageStats
    {
        int present = 0, registered = 0, injured = 0, disabled = 0, hostile = 0;
        int combat = 0, travelers = 0, monsters = 0, silver = 0, rumorHolders = 0;
        float safety = 100, morale = 70, prosperity = 50, trust = 0;
        std::array<int, 4> moods{}; // calm, anxious, frightened, hopeful
        const char *label = "평온";
    };

    struct MoodSample
    {
        float time = 0, morale = 0, safety = 0, prosperity = 0;
        int population = 0;
    };

    std::array<std::deque<MoodSample>, novel::VillageCount> moodHistory;
    std::vector<Rumor> rumors;
    std::array<VillageStats, novel::VillageCount> villageStats{};
    std::array<bool, novel::VillageCount> visitedVillages{};
    int rumorTransfers = 0, courierArrivals = 0, monsterRespawns = 0;
    float rumorUpdateMs = 0, worldUpdateMs = 0, villageStatsMs = 0;
    int VisitedCount() const;
    void AddLocalRumor(WorldPoint origin, const std::string &text, int sentiment);
    void TravelNpc(TownNpc &npc, float dt);
    bool RespawnMonster(RpgEnemy &enemy, bool initial = false);
    DollBehaviorPrototype dollTraining;
    uint32_t seed = 0;
    WorldPoint player{3, 3}, aim{4, 3};
    float playerMoveSpeed = novel::PlayerSpeed;
    std::array<bool, 256> keys{};
    bool firing = false, paused = false, inventory = false, moving = false;
    int hp = 100;
    int potions = 3, silver = 0, kills = 0;
    int itemsCollected = 0;
    std::vector<BanditGroup> banditGroups;
    float time = 0, attackCooldown = 0, attackStarted = -100, hurt = 0;
    std::string message;
    std::vector<RpgEnemy> enemies;
    std::vector<RpgDrop> drops;
    std::vector<RpgShot> shots;
    std::vector<WorldPoint> fires;

  private:
    struct Noise
    {
        WorldPoint position;
        float radius = 0, expiresAt = 0;
    };

    std::vector<Noise> noises;
    float footstepTimer = 0;
    void InitializeExpedition();
    void UpdateExpedition(float dt, float movedDistance, bool sprinting);
    void UpdateRescue(float dt);
    void UpdateNpcRoutine(TownNpc &npc);
    bool RescueDialogue(TownNpc &npc, int choice);
    bool BeginEscort();
    void LearnRescueClue(int clue);
    bool DangerNearby() const;
    int RegionAt(WorldPoint position) const;
    void EmitNoise(WorldPoint position, float radius);
    bool HeardNoise(WorldPoint observer, WorldPoint &source) const;
    std::mt19937 random;
    float messageTime = 0;
    void Move(WorldPoint &p, float x, float z);
    void Notify(const std::string &text);
    void Step(float dt);
    void InitializeBandits();
    bool PlaceBand(int band);
    void UpdateBandits(float dt);
    void ProvokeBand(RpgEnemy &enemy);
    bool DamageMonster(RpgEnemy &enemy, int damage, bool playerCredit);
    float banditLogTimer = 0, banditUpdateCpuMs = 0;
    float banditPathCpuMs = 0;
    int banditDecisions = 0;
    int banditPathReplans = 0;
    void LogBandits();
    float npcUpdateCpuMs = 0, npcDecisionCpuMs = 0, npcPathCpuMs = 0, npcMetricsTimer = 0;
    int npcDecisions = 0, npcPathReplans = 0;
    mutable float npcLogCpuMs = 0;
    void InitializeWorld();
    void UpdateWorld(float dt);
    void LogWorld();
    std::size_t worldLogCursor = 0;
    float rumorTimer = 0, statsTimer = 0, worldWindowMs = 0;
    int nextRumorId = 0;
    void UpdateRumors();
    void UpdateVillageStats();
    void InitializeTown();
    void UpdateTown(float dt);
    void PerceiveNpc(TownNpc &npc);
    void CoordinateTown();
    void DecideNpc(TownNpc &npc);
    float townCoordinationTimer = 0;
    void MoveNpc(TownNpc &npc, WorldPoint target, float dt, bool profileNpc = true);
    void LogNpc(const TownNpc &npc, const char *event) const;
    void DecideRobbery(TownNpc &npc);
    void StepRobbery(TownNpc &npc, float dt);
    void RecordEconomy(const char *kind, int from, int to, int amount);
};
