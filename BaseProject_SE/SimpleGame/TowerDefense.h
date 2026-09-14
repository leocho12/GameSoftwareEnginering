#pragma once
#include "GridTypes.h"
#include <array>
#include <string>
#include <vector>

struct Defender
{
    std::string name, role, skill;
    int cost = 0, block = 0, facing = 0;
    bool elevated = false, deployed = false;
    Cell cell;
    float hp = 100, maxHp = 100, attack = 10, interval = 1, timer = 0;
    float cooldown = 0, sp = 0, skillTime = 0;
    float attackStarted = -1000, deployedAt = 0, defeatedAt = -1000;
};

struct Invader
{
    int lane = 0, type = 0, blockedBy = -1;
    float progress = 0, x = 0, z = 0, hp = 40, maxHp = 40, timer = 0;
    bool alive = true;
};

struct CombatTrace
{
    float ax, az, bx, bz, time;
    bool heal;
};

// Self-contained real-time encounter. World traversal never uses these combat rules.
class TowerDefense
{
  public:
    void Reset(bool wild = false);
    void Update(float dt);
    void Select(int index);
    void Click(Cell cell);
    void Rotate();
    void Confirm();
    void Cancel();
    void Skill();
    void Retreat();
    bool Board(Cell c) const;
    bool High(Cell c) const;
    bool Road(Cell c) const;
    bool CanDeploy(int index, Cell c) const;
    bool InRange(int index, Cell origin, int facing, float x, float z) const;
    int At(Cell c) const;
    int Deployed() const;
    int Cost(int index) const;
    std::array<Defender, 4> team;
    std::array<std::vector<Cell>, 2> lanes;
    std::vector<Invader> enemies;
    std::vector<CombatTrace> traces;
    int selected = -1, facing = 0, lives = 5, spawned = 0, resolved = 0, kills = 0, total = 12,
        speed = 1;
    float dp = 24, time = 0;
    bool tacticalPause = false, won = false, lost = false, wild = false;
    Cell pending;
    std::string message;

  private:
    void Step(float dt);
    void Remove(int index, bool refund);
};
