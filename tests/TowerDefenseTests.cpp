// Optional CPU-only regression checks. Build manually; not part of the game executable.
#include "../BaseProject_SE/SimpleGame/TowerDefense.h"
#include <cassert>
#include <cmath>

static void Advance(TowerDefense &d, float seconds)
{
    for (int i = 0; i < int(seconds * 60); ++i)
        d.Update(1.0f / 60);
}

static void Deploy(TowerDefense &d, int i, Cell c, int direction = 2)
{
    d.Select(i);
    d.Click(c);
    d.facing = direction;
    d.Confirm();
    assert(d.team[i].deployed);
}

int main()
{
    TowerDefense d;
    d.Reset();
    assert(!d.CanDeploy(0, {3, 3})); // Ingress is reserved.
    assert(!d.CanDeploy(0, {8, 4})); // Ground unit cannot use a high tile.
    assert(!d.CanDeploy(2, {8, 3})); // Ranged unit cannot use the road.
    Deploy(d, 0, {8, 3});
    assert(std::abs(d.dp - 16) < .01f);
    assert(!d.CanDeploy(1, {8, 3}));
    d.Select(1);
    d.Click({8, 5});
    float before = d.dp;
    d.Cancel();
    assert(d.dp == before);
    d.Select(0);
    d.Retreat();
    assert(std::abs(d.dp - 20) < .01f);
    assert(!d.CanDeploy(0, {8, 3}));
    d.tacticalPause = true;
    Advance(d, 2);
    assert(d.time == 0 && d.team[0].cooldown == 20);
    d.tacticalPause = false;
    Advance(d, 21);
    assert(d.team[0].cooldown == 0);

    d.Reset();
    d.dp = 0;
    assert(!d.CanDeploy(1, {8, 3}));
    d.Reset();
    Deploy(d, 0, {8, 3});
    d.team[0].sp = 20;
    d.Skill();
    assert(std::abs(d.dp - 26) < .01f);
    d.Skill();
    assert(std::abs(d.dp - 26) < .01f); // Cannot consume the same charge twice.
    assert(d.InRange(0, {8, 3}, 2, 7, 3));
    assert(!d.InRange(0, {8, 3}, 2, 9, 3));

    d.Reset();
    Deploy(d, 0, {8, 3});
    d.team[0].timer = 100;
    Invader a;
    a.x = 8;
    a.z = 3;
    a.progress = 5;
    d.enemies = {a, a};
    d.Update(1.0f / 60);
    assert(d.enemies[0].blockedBy == 0 && d.enemies[1].blockedBy == -1);
    d.Select(0);
    d.Retreat();
    assert(d.enemies[0].blockedBy == -1);

    d.Reset();
    Deploy(d, 1, {11, 4});
    d.dp = 99;
    Deploy(d, 3, {11, 6}, 3);
    d.team[1].hp = 100;
    d.Update(1.0f / 60);
    assert(d.team[1].hp > 100);
    d.Reset();
    Advance(d, 100);
    assert(d.lost && d.lives == 0 && !d.won);

    // A resource-legal, two-lane defense should finish well within the tutorial budget.
    d.Reset();
    Deploy(d, 0, {8, 3});
    Deploy(d, 2, {9, 4});
    Advance(d, 10.1f);
    Deploy(d, 1, {11, 4});
    Advance(d, 10.1f);
    Deploy(d, 3, {11, 6}, 3);
    Advance(d, 100);
    assert(d.won && !d.lost && d.resolved == 12 && d.time < 150);
    d.Reset(true);
    assert(d.total == 8 && d.lives == 5 && d.enemies.empty());
}
