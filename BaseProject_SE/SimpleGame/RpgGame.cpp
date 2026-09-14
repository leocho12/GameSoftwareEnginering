#include "stdafx.h"
#include "RpgGame.h"
#include <algorithm>
#include <cmath>
#include <queue>

namespace
{
float Distance(WorldPoint a, WorldPoint b)
{
    return std::hypot(a.x - b.x, a.z - b.z);
}

}

RpgGame::RpgGame()
{
    Reset(std::random_device{}());
}

int RpgGame::MaxHp() const
{
    return 100 + 20 * (level - 1);
}

int RpgGame::AttackPower() const
{
    return 14 + 3 * (level - 1);
}

int RpgGame::Defense() const
{
    return 2 + (level - 1);
}

int RpgGame::NextExperience() const
{
    return 40 + 20 * (level - 1);
}

RpgGame::Ground RpgGame::Terrain(int x, int z) const
{
    // Coordinate hashing generates the same world without storing visited ground.
    // Isolated obstacle cells leave two-cell-wide connected corridors everywhere.
    auto mod3 = [](int value)
    {
        int remainder = value % 3;
        return remainder < 0 ? remainder + 3 : remainder;
    };
    if ((x >= 0 && x <= 6 && z >= 0 && z <= 6) || mod3(x) != 1 || mod3(z) != 1)
        return Ground::Dirt;
    uint32_t hash = seed ^ (uint32_t(x) * 0x9e3779b9u) ^ (uint32_t(z) * 0x85ebca6bu);
    hash ^= hash >> 16;
    hash *= 0x7feb352du;
    hash ^= hash >> 15;
    hash *= 0x846ca68bu;
    hash ^= hash >> 16;
    if (hash % 10 < 3)
        return Ground::Dirt;
    return hash % 4 == 0 ? Ground::Rock : Ground::Tree;
}

bool RpgGame::Walkable(int x, int z) const
{
    return Terrain(x, z) == Ground::Dirt;
}

bool RpgGame::Fits(WorldPoint p) const
{
    // Four corners provide clearance for a continuous .44-unit wide collision box.
    for (float dx : {-.22f, .22f})
        for (float dz : {-.22f, .22f})
            if (!Walkable(int(std::floor(p.x + dx + .5f)), int(std::floor(p.z + dz + .5f))))
                return false;
    return true;
}

bool RpgGame::Connected() const
{
    // Audit a player-centered window, including negative world coordinates.
    int originX = int(std::floor(player.x + .5f)) - Size / 2;
    int originZ = int(std::floor(player.z + .5f)) - Size / 2;
    std::array<bool, Size * Size> seen{};
    std::queue<int> pending;
    int start = (Size / 2) * Size + Size / 2;
    if (!Walkable(originX + Size / 2, originZ + Size / 2))
        return false;
    pending.push(start);
    seen[start] = true;
    int count = 0, total = 0;
    for (int z = 0; z < Size; ++z)
        for (int x = 0; x < Size; ++x)
            total += Walkable(originX + x, originZ + z) ? 1 : 0;
    while (!pending.empty())
    {
        int i = pending.front();
        pending.pop();
        ++count;
        for (auto d : {WorldPoint{1, 0}, WorldPoint{-1, 0}, WorldPoint{0, 1}, WorldPoint{0, -1}})
        {
            int x = i % Size + int(d.x), z = i / Size + int(d.z);
            if (x < 0 || z < 0 || x >= Size || z >= Size)
                continue;
            int j = z * Size + x;
            if (!seen[j] && Walkable(originX + x, originZ + z))
            {
                seen[j] = true;
                pending.push(j);
            }
        }
    }
    return count == total;
}

void RpgGame::Reset(uint32_t newSeed)
{
    seed = newSeed;
    random.seed(seed);
    player = {3, 3};
    aim = {4, 3};
    level = 1;
    experience = totalExperience = 0;
    hp = 100;
    potions = 3;
    scrap = kills = 0;
    time = attackCooldown = hurt = 0;
    attackStarted = -100;
    paused = inventory = moving = false;
    ClearInput();
    enemies.clear();
    drops.clear();
    shots.clear();
    fires.clear();
    for (int i = 0; i < 16; ++i)
    {
        RpgEnemy e;
        e.position = Spawn();
        enemies.push_back(e);
    }
    fires = {{2, 5}, {5, 2}};
    drops.push_back({{4, 3}, true});
    Notify("레벨 1 / 외곽 사냥터: 적을 처치하고 경험치를 모아 레벨 5에 도달하세요.");
}

void RpgGame::ClearInput()
{
    keys.fill(false);
    firing = false;
    moving = false;
}

WorldPoint RpgGame::Spawn()
{
    std::vector<WorldPoint> candidates;
    int centerX = int(std::floor(player.x)), centerZ = int(std::floor(player.z));
    for (int z = centerZ - 14; z <= centerZ + 14; ++z)
        for (int x = centerX - 14; x <= centerX + 14; ++x)
        {
            WorldPoint p{float(x), float(z)};
            if (!Fits(p) || Distance(p, player) < 7)
                continue;
            bool occupied = false;
            for (const auto &e : enemies)
                if (e.hp > 0 && Distance(e.position, p) < 1)
                    occupied = true;
            if (!occupied)
                candidates.push_back(p);
        }
    return candidates.empty() ? player : candidates[random() % candidates.size()];
}

void RpgGame::Move(WorldPoint &p, float x, float z)
{
    WorldPoint next{p.x + x, p.z};
    if (Fits(next))
        p = next;
    next = {p.x, p.z + z};
    if (Fits(next))
        p = next;
}

bool RpgGame::LineOfSight(WorldPoint a, WorldPoint b) const
{
    int steps = std::max(1, int(std::ceil(Distance(a, b) / .1f)));
    for (int i = 0; i <= steps; ++i)
    {
        float t = float(i) / steps;
        if (!Walkable(int(std::floor(a.x + (b.x - a.x) * t + .5f)),
                      int(std::floor(a.z + (b.z - a.z) * t + .5f))))
            return false;
    }
    return true;
}

void RpgGame::Notify(const std::string &text)
{
    message = text;
    messageTime = 4;
}

void RpgGame::AddExperience(int amount)
{
    if (amount <= 0)
        return;
    totalExperience += amount;
    if (level >= 100)
        return;
    experience += amount;
    bool gained = false;
    while (level < 100 && experience >= NextExperience())
    {
        experience -= NextExperience();
        ++level;
        hp = std::min(MaxHp(), hp + 20);
        gained = true;
    }
    if (level == 100)
        experience = 0;
    if (gained)
        Notify("레벨 " + std::to_string(level) + "! 최대 체력 +20 / 공격력 +3 / 방어력 +1");
}

void RpgGame::Attack()
{
    if (paused || hp <= 0 || attackCooldown > 0)
        return;
    attackCooldown = .4f;
    attackStarted = time;
    float dx = aim.x - player.x, dz = aim.z - player.z, length = std::hypot(dx, dz);
    if (length < .01f)
    {
        dx = 1;
        dz = 0;
        length = 1;
    }
    dx /= length;
    dz /= length;
    int target = -1;
    float nearest = 6;
    for (int i = 0; i < int(enemies.size()); ++i)
    {
        const auto &e = enemies[i];
        if (e.hp <= 0)
            continue;
        float ex = e.position.x - player.x, ez = e.position.z - player.z;
        float forward = ex * dx + ez * dz, side = std::abs(ex * dz - ez * dx);
        if (forward >= 0 && forward < nearest && side < .5f && LineOfSight(player, e.position))
        {
            target = i;
            nearest = forward;
        }
    }
    WorldPoint end{player.x + dx * 6, player.z + dz * 6};
    if (target >= 0)
    {
        auto &e = enemies[target];
        end = e.position;
        e.hp = std::max(0, e.hp - AttackPower());
        if (e.hp == 0)
        {
            ++kills;
            e.respawn = 18;
            drops.push_back({e.position, random() % 3 == 0});
            // Keep the floor bounded during prolonged grinding.
            if (drops.size() > 128)
                drops.erase(drops.begin());
            AddExperience(20);
        }
    }
    else
    {
        for (float d = .1f; d <= 6; d += .1f)
        {
            WorldPoint p{player.x + dx * d, player.z + dz * d};
            if (!Walkable(int(std::floor(p.x + .5f)), int(std::floor(p.z + .5f))))
            {
                end = p;
                break;
            }
        }
    }
    shots.push_back({player, end, .12f});
}

void RpgGame::Pickup()
{
    if (paused || hp <= 0)
        return;
    int count = 0;
    drops.erase(std::remove_if(drops.begin(),
                               drops.end(),
                               [&](const RpgDrop &d)
                               {
                                   if (Distance(d.position, player) > 1.5f ||
                                       !LineOfSight(player, d.position))
                                       return false;
                                   if (d.potion)
                                       ++potions;
                                   else
                                       ++scrap;
                                   ++count;
                                   return true;
                               }),
                drops.end());
    Notify(count ? std::to_string(count) + "개 아이템 획득"
                 : "주변에 획득 가능한 아이템이 없습니다.");
}

void RpgGame::Heal()
{
    if (paused || hp <= 0)
        return;
    if (potions <= 0)
    {
        Notify("회복약이 없습니다.");
        return;
    }
    if (hp == MaxHp())
    {
        Notify("체력이 가득 찼습니다.");
        return;
    }
    --potions;
    hp = std::min(MaxHp(), hp + MaxHp() / 2);
    Notify("회복약 사용: 최대 체력의 50% 회복");
}

void RpgGame::Update(float dt)
{
    if (paused || hp <= 0)
        return;
    float remaining = std::clamp(dt, 0.0f, .1f);
    while (remaining > 0 && hp > 0)
    {
        float step = std::min(remaining, 1.0f / 60);
        Step(step);
        remaining -= step;
    }
}

void RpgGame::Step(float dt)
{
    time += dt;
    attackCooldown = std::max(0.0f, attackCooldown - dt);
    hurt = std::max(0.0f, hurt - dt);
    messageTime = std::max(0.0f, messageTime - dt);
    if (messageTime == 0)
        message.clear();
    for (auto &s : shots)
        s.life -= dt;
    shots.erase(std::remove_if(shots.begin(),
                               shots.end(),
                               [](const RpgShot &s)
                               {
                                   return s.life <= 0;
                               }),
                shots.end());
    // Screen-relative WASD mapped to the fixed quarter-view camera axes.
    float right = float(keys['d']) - float(keys['a']), up = float(keys['w']) - float(keys['s']);
    float length = std::hypot(right, up);
    WorldPoint before = player;
    if (length > 0)
        Move(player,
             (.745f * right - .667f * up) / length * 3.6f * dt,
             (-.667f * right - .745f * up) / length * 3.6f * dt);
    moving = Distance(before, player) > .00001f;
    if (firing || keys[' '])
        Attack();
    // A bounded local navigation window follows the player through the infinite world.
    int originX = int(std::floor(player.x + .5f)) - Size / 2;
    int originZ = int(std::floor(player.z + .5f)) - Size / 2;
    auto indexOf = [&](WorldPoint p)
    {
        int x = int(std::floor(p.x + .5f)) - originX, z = int(std::floor(p.z + .5f)) - originZ;
        return x < 0 || z < 0 || x >= Size || z >= Size ? -1 : z * Size + x;
    };
    auto pointOf = [&](int i)
    {
        return WorldPoint{float(originX + i % Size), float(originZ + i / Size)};
    };
    auto adjacent = [](int i, int j)
    {
        return j >= 0 && j < Size * Size &&
               std::abs(i % Size - j % Size) + std::abs(i / Size - j / Size) == 1;
    };
    std::array<int, Size * Size> distance;
    distance.fill(-1);
    std::queue<int> pending;
    int start = indexOf(player);
    distance[start] = 0;
    pending.push(start);
    while (!pending.empty())
    {
        int i = pending.front();
        pending.pop();
        for (int offset : {1, -1, Size, -Size})
        {
            int j = i + offset;
            if (!adjacent(i, j) || distance[j] >= 0 ||
                !Walkable(originX + j % Size, originZ + j / Size))
                continue;
            distance[j] = distance[i] + 1;
            pending.push(j);
        }
    }
    for (auto &e : enemies)
    {
        // Recycle off-screen population so exploration stays populated and memory stays bounded.
        if (e.hp > 0 && Distance(e.position, player) > 24)
        {
            e.position = Spawn();
            e.maxHp = 40 + std::min(level - 1, 10) * 5;
            e.hp = e.maxHp;
            e.cooldown = 1;
        }
        if (e.hp <= 0)
        {
            e.respawn -= dt;
            if (e.respawn <= 0)
            {
                e.position = Spawn();
                e.maxHp = 40 + std::min(level - 1, 10) * 5;
                e.hp = e.maxHp;
                e.cooldown = 1;
            }
            continue;
        }
        e.cooldown = std::max(0.0f, e.cooldown - dt);
        float range = Distance(player, e.position);
        if (range > 10)
            continue;
        if (range > .85f)
        {
            WorldPoint goal = player;
            auto clearRoute = [&](WorldPoint target)
            {
                int samples = std::max(1, int(std::ceil(Distance(e.position, target) / .1f)));
                for (int n = 1; n <= samples; ++n)
                {
                    float t = float(n) / samples;
                    if (!Fits({e.position.x + (target.x - e.position.x) * t,
                               e.position.z + (target.z - e.position.z) * t}))
                        return false;
                }
                return true;
            };
            if (!clearRoute(player))
            {
                int here = indexOf(e.position);
                if (here < 0)
                    continue;
                int best = here;
                for (int offset : {1, -1, Size, -Size})
                {
                    int j = here + offset;
                    if (adjacent(here, j) && distance[j] >= 0 &&
                        (distance[best] < 0 || distance[j] < distance[best]))
                        best = j;
                }
                goal = pointOf(best);
                if (!clearRoute(goal))
                    goal = pointOf(here);
            }
            float dx = goal.x - e.position.x, dz = goal.z - e.position.z, len = std::hypot(dx, dz);
            if (len > .01f)
                Move(e.position,
                     dx / len * std::min(len, 1.45f * dt),
                     dz / len * std::min(len, 1.45f * dt));
        }
        else if (e.cooldown == 0 && LineOfSight(player, e.position))
        {
            hp = std::max(0, hp - std::max(1, 10 - Defense()));
            e.cooldown = 1;
            hurt = .2f;
            if (hp == 0)
            {
                ClearInput();
                Notify("전투 불능 · Enter로 같은 맵에서 다시 시작");
                break;
            }
        }
    }
}
