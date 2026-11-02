// CPU-only rules, no renderer or game process. C++17 standalone entry point.
#include "../BaseProject_SE/SimpleGame/ExpeditionRules.h"
#include <cassert>
#include <limits>

int main()
{
    using namespace expedition;
    Clock clock;
    assert(clock.MinuteOfDay() == 480 && clock.Day() == 1);
    clock.Advance(30);
    assert(clock.MinuteOfDay() == 540);
    clock.Advance(std::numeric_limits<float>::infinity());
    assert(clock.MinuteOfDay() == 540);
    clock.minutes = 1439;
    clock.Advance(1);
    assert(clock.Day() == 2 && clock.MinuteOfDay() == 1);
    assert(clock.CurrentPhase() == Phase::DeepNight);
    clock.minutes = 239;
    assert(clock.SecondsToDawn() == .5f);
    clock.Advance(.5f);
    assert(clock.CurrentPhase() == Phase::Dawn && clock.SecondsToDawn() == 720);

    Needs needs;
    needs.hunger = needs.thirst = needs.fatigue = 100;
    assert(needs.Condition(100) < 11 && needs.MaxStamina(100) < 74);
    int hp = 100;
    needs.Advance(30, Phase::HighHeat, false, false, hp);
    assert(hp < 100);
    needs = Needs{};
    needs.fatigue = 70;
    needs.stamina = 20;
    hp = 50;
    needs.Advance(5, Phase::Morning, true, false, hp);
    assert(needs.fatigue < 70 && needs.stamina > 20 && hp > 50);
    needs.stamina = 9;
    assert(!needs.SpendStamina(10) && needs.stamina == 9);
    assert(needs.SpendStamina(9) && needs.stamina == 0);
    assert(!needs.CanSprint());
    needs.stamina = 20;
    assert(!needs.CanSprint()); // Holding sprint must not oscillate around the low threshold.
    needs.stamina = 30;
    assert(needs.CanSprint());

    Alert one, sliced;
    one.Raise(60);
    sliced.Raise(60);
    assert(one.Level() == 3);
    one.Advance(15);
    for (int i = 0; i < 30; ++i)
        sliced.Advance(.5f);
    assert(one.value == sliced.value && one.value == 55);
    one.Raise(100);
    assert(one.value == 100 && one.Level() == 4);

    RescueQuest quest;
    assert(quest.Learn(0) && !quest.Learn(0) && !quest.Learn(3));
    assert(quest.ClueCount() == 1 && !quest.Report());
    assert(quest.Learn(1) && quest.stage == RescueStage::Located);
    assert(!quest.Report());
    quest.stage = RescueStage::Returned;
    assert(quest.Report() && !quest.Report());
    assert(!quest.Learn(2));
}
