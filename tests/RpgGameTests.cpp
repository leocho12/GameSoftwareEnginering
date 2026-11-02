// CPU-only checks. Link the RPG sources including RpgEconomy.cpp, RpgBandits.cpp and RpgExpedition.cpp; no renderer.
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
    game.expeditionState.progress.combatWins = 3;
    assert(game.MaxHp() == 100 && game.Defense() == 3);
    game.expeditionState.progress.combatWins = 10;
    assert(game.expeditionState.progress.WeaponSkill() == 30 && game.AttackPower() == 17);
    game.Reset(42);
    game.enemies.clear();
    game.enemies.push_back({{4, 3}, 14, 14});
    game.aim = {4, 3};
    game.Attack();
    assert(game.kills == 1 && game.expeditionState.progress.combatWins == 1 &&
           game.enemies[0].hp == 0);
    game.Attack();
    assert(game.kills == 1 && game.expeditionState.progress.combatWins == 1);
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
    int victories = game.expeditionState.progress.combatWins;
    game.Attack();
    game.Update(.1f);
    assert(game.expeditionState.progress.combatWins == victories);
    game.ClearInput();
    assert(!game.keys['d'] && !game.firing);
}
