// CPU-only checks. Link the RPG sources including RpgEconomy.cpp and RpgBandits.cpp; no renderer.
#include "../BaseProject_SE/SimpleGame/RpgGame.h"
#include <cassert>
#include <cmath>

int main()
{
    RpgGame game;
    for (unsigned seed = 0; seed < 64; ++seed)
    {
        game.Reset(seed);
        assert(game.Connected());
        assert(game.Fits(game.player));
        for (const auto &e : game.enemies)
            assert(game.Fits(e.position));
        for (int z = 1; z < RpgGame::Size - 1; ++z)
            for (int x = 1; x < RpgGame::Size - 1; ++x)
            {
                if (!game.Walkable(x, z))
                    continue;
                for (auto direction : {WorldPoint{1, 0}, WorldPoint{0, 1}})
                {
                    if (!game.Walkable(x + int(direction.x), z + int(direction.z)))
                        continue;
                    for (int n = 0; n <= 10; ++n)
                        assert(game.Fits({x + direction.x * n / 10, z + direction.z * n / 10}));
                }
            }
    }
    game.Reset(42);
    auto sample = game.Terrain(15, 15);
    game.Reset(42);
    assert(sample == game.Terrain(15, 15));
    game.AddExperience(39);
    assert(game.level == 1 && game.experience == 39);
    game.AddExperience(1);
    assert(game.level == 2 && game.experience == 0);
    assert(game.MaxHp() == 120 && game.AttackPower() == 17 && game.Defense() == 3);
    game.AddExperience(140);
    assert(game.level == 4 && game.experience == 0);
    assert(game.totalExperience == 180);
    game.Reset(42);
    game.enemies.clear();
    game.enemies.push_back({{4, 3}, 14, 14});
    game.aim = {4, 3};
    game.Attack();
    assert(game.kills == 1 && game.experience == 20 && game.enemies[0].hp == 0);
    game.Attack();
    assert(game.kills == 1 && game.experience == 20);
    game.Pickup();
    assert(game.drops.empty());
    game.hp = 30;
    game.potions = 1;
    game.Heal();
    assert(game.hp == 80 && game.potions == 0);
    game.paused = true;
    auto before = game.player;
    game.keys['w'] = true;
    game.Update(.1f);
    assert(game.player.x == before.x && game.player.z == before.z);
    game.Reset(42);
    game.enemies.clear();
    game.keys['d'] = true;
    for (int i = 0; i < 1800; ++i)
        game.Update(1.0f / 60);
    assert(game.Fits(game.player));
    game.hp = 0;
    int experience = game.experience;
    game.Attack();
    game.Update(.1f);
    assert(game.experience == experience);
    game.ClearInput();
    assert(!game.keys['d'] && !game.firing);
}
