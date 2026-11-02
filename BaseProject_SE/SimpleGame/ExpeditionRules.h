#pragma once
#include <algorithm>
#include <array>
#include <cmath>
#include <string>

// Original, non-adult gameplay rules. Time is world minutes, except explicit Seconds fields.
namespace expedition
{
constexpr float MinutesPerSecond = 2.0f;

enum class Phase
{
    DeepNight,
    Dawn,
    Morning,
    HighHeat,
    Afternoon,
    Dusk,
    Evening
};

struct Clock
{
    double minutes = 480;

    void Advance(float seconds)
    {
        if (std::isfinite(seconds) && seconds > 0)
            minutes += double(seconds) * MinutesPerSecond;
    }

    int Day() const
    {
        return 1 + int(minutes / 1440);
    }

    int MinuteOfDay() const
    {
        return int(std::fmod(minutes, 1440.0));
    }

    Phase CurrentPhase() const
    {
        int minute = MinuteOfDay();
        return minute < 240    ? Phase::DeepNight
               : minute < 420  ? Phase::Dawn
               : minute < 660  ? Phase::Morning
               : minute < 900  ? Phase::HighHeat
               : minute < 1080 ? Phase::Afternoon
               : minute < 1200 ? Phase::Dusk
                               : Phase::Evening;
    }

    std::string Label() const
    {
        const char *names[] = {"깊은 밤", "새벽", "아침", "한낮", "오후", "해질녘", "저녁"};
        int minute = MinuteOfDay();
        return std::to_string(Day()) + "일 " + (minute / 60 < 10 ? "0" : "") +
               std::to_string(minute / 60) + ":" + (minute % 60 < 10 ? "0" : "") +
               std::to_string(minute % 60) + " · " + names[int(CurrentPhase())];
    }

    float SecondsToDawn() const
    {
        double next = (std::floor(minutes / 1440) + 1) * 1440 + 240;
        if (MinuteOfDay() < 240)
            next -= 1440;
        return float((next - minutes) / MinutesPerSecond);
    }
};

struct Needs
{
    float hunger = 12, thirst = 14, fatigue = 8, stamina = 100;
    float healthDebt = 0, recoveryCredit = 0;
    bool sprintExhausted = false;

    static float Penalty(float value)
    {
        return value < 60 ? 0 : value < 85 ? (value - 60) * .4f : 10 + (value - 85) * (20.0f / 15);
    }

    float Condition(int hp) const
    {
        return std::clamp(
            float(hp) - Penalty(hunger) - Penalty(thirst) - Penalty(fatigue), 0.0f, 100.0f);
    }

    float MaxStamina(int hp) const
    {
        return 70 + Condition(hp) * .3f;
    }

    bool SpendStamina(float amount)
    {
        if (amount < 0 || stamina < amount)
            return false;
        stamina -= amount;
        return true;
    }

    bool CanSprint()
    {
        if (stamina < 12)
            sprintExhausted = true;
        else if (stamina >= 30)
            sprintExhausted = false;
        return !sprintExhausted;
    }

    void Advance(float seconds, Phase phase, bool resting, bool sprinting, int &hp)
    {
        if (!std::isfinite(seconds) || seconds <= 0 || hp <= 0)
            return;
        float minutes = seconds * MinutesPerSecond;
        hunger = std::clamp(hunger + .020f * minutes, 0.0f, 100.0f);
        thirst = std::clamp(
            thirst + .030f * minutes * (phase == Phase::HighHeat ? 2.2f : 1), 0.0f, 100.0f);
        fatigue = std::clamp(fatigue + (resting     ? -.8f
                                        : sprinting ? .10f
                                                    : .022f) *
                                           minutes,
                             0.0f,
                             100.0f);
        stamina = std::clamp(stamina + (sprinting ? -18
                                        : resting ? 18
                                                  : 7) *
                                           seconds,
                             0.0f,
                             MaxStamina(hp));
        float damage = (thirst >= 85   ? 2
                        : thirst >= 60 ? 1
                                       : 0) +
                       (hunger >= 85   ? 1
                        : hunger >= 60 ? .5f
                                       : 0) +
                       (fatigue >= 85 ? .5f : 0);
        healthDebt += damage * minutes / 60;
        int loss = int(healthDebt);
        hp = std::max(0, hp - loss);
        healthDebt -= loss;
        if (hp > 0 && resting && hunger < 60 && thirst < 60)
        {
            recoveryCredit += seconds;
            int gain = int(recoveryCredit);
            hp = std::min(100, hp + gain);
            recoveryCredit -= gain;
        }
        else
            recoveryCredit = 0;
    }
};

struct Alert
{
    float value = 0, quietSeconds = 0;

    void Raise(float amount)
    {
        if (amount <= 0)
            return;
        value = std::clamp(value + amount, 0.0f, 100.0f);
        quietSeconds = 0;
    }

    void Advance(float seconds)
    {
        float previous = quietSeconds;
        quietSeconds += seconds;
        float decaySeconds = std::max(0.0f, quietSeconds - 10) - std::max(0.0f, previous - 10);
        value = std::max(0.0f, value - decaySeconds);
    }

    int Level() const
    {
        return value >= 80 ? 4 : value >= 55 ? 3 : value >= 30 ? 2 : value >= 10 ? 1 : 0;
    }
};

enum class RescueStage
{
    Investigating,
    Located,
    Escorting,
    Returned,
    Completed,
    Failed
};

struct RescueQuest
{
    RescueStage stage = RescueStage::Investigating;
    std::array<bool, 3> clues{};
    int actorId = -1;
    bool contacted = false;

    int ClueCount() const
    {
        return int(std::count(clues.begin(), clues.end(), true));
    }

    bool Learn(int clue)
    {
        if (clue < 0 || clue >= int(clues.size()) || clues[clue] ||
            stage == RescueStage::Completed || stage == RescueStage::Failed)
            return false;
        clues[clue] = true;
        if (ClueCount() >= 2 && stage == RescueStage::Investigating)
            stage = RescueStage::Located;
        return true;
    }

    bool Report()
    {
        if (stage != RescueStage::Returned)
            return false;
        stage = RescueStage::Completed;
        return true;
    }
};

struct Progression
{
    int combatWins = 0, rescues = 0;

    int WeaponSkill() const
    {
        return std::min(100, combatWins * 3);
    }

    bool Veteran() const
    {
        return combatWins >= 3;
    }
};

struct State
{
    Clock clock;
    Needs needs;
    Progression progress;
    RescueQuest rescue;
    std::array<Alert, 4> alerts;
    int rations = 3, water = 3;
    bool resting = false;
};
}
