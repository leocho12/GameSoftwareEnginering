#pragma once
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
};
