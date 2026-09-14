#pragma once
#include <array>
#include <deque>
#include <string>
#include <vector>

#include "GridTypes.h"
#include "TowerDefense.h"

struct Unit
{
    std::string name;
    Cell cell;
    float x = 0, z = 0;
    int hp = 16, maxHp = 16;
    bool ally = true, skillUsed = false;
};

struct Npc
{
    std::string name, dialogue;
    Cell cell;
};

struct Wildlife
{
    Cell home, cell;
    float x = 0, z = 0;
    int hp = 12;
};
enum class Phase
{
    Briefing,
    Supply,
    Approach,
    Battle,
    Restore,
    Complete,
    Defeat,
    Explore
};

class TutorialGame
{
  public:
    static constexpr int Width = 28, Depth = 24;
    static constexpr Cell SupplyCell{3, 8};
    static constexpr Cell TerminalCell{11, 1};
    TowerDefense defense;
    TutorialGame();
    void Reset();
    void Start();
    void StartDefenseDemo();
    void RetryBattle();
    void ContinueExploring();
    void Update(float dt);
    void Click(Cell cell);
    void Interact();

    void Confirm();
    void Cancel();
    void Wait();
    void TogglePause();

    bool Inside(Cell c) const;
    bool Wall(Cell c) const;
    bool Water(Cell c) const;
    int NpcAt(Cell c) const;
    float Height(Cell c) const;
    bool CanStep(Cell from, Cell to) const;
    int UnitAt(Cell c, int except = -1) const;
    std::vector<Cell> Path(Cell from, Cell to, int mover, int limit = 999) const;
    std::string Objective() const;

    bool Busy() const
    {
        return !route.empty() || effectTime > 0;
    }

    int Distance(Cell a, Cell b) const;

    Phase phase = Phase::Briefing;

    std::array<Unit, 5> units;
    int active = 0;
    bool paused = false, hasParts = false;
    bool defenseDemo = false;
    float elapsed = 0, phaseTime = 0;
    std::string message;
    Cell preview;
    std::vector<Cell> previewPath;
    int target = -1;
    std::deque<Cell> route;
    float effectTime = 0;
    int effectFrom = -1, effectTo = -1;
    bool healingEffect = false;
    int shots = 0;
    bool yardCleared = false;
    std::array<Npc, 8> npcs;
    std::array<Wildlife, 4> wildlife;
    int wildEncounter = -1;
    float animationTime = 0;

  private:
    Cell interaction;

    std::array<Unit, 5> fieldCheckpoint;
    float wildlifeTick = 0;
    int patrolStep = 0;
    void BeginWildlife(int index);
    void BeginBattle();

    void TryInteraction();
    void MoveToObject(Cell object);
    bool Walkable(Cell c, int mover) const;
};
