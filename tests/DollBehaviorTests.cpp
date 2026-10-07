// CPU-only: compile with DollBehavior.cpp. No OpenGL context or assets required.
#include "../BaseProject_SE/SimpleGame/DollBehavior.h"
#include <cassert>
#include <cmath>
#include <limits>

namespace
{
void Run(DollBehaviorPrototype &training, int frames = 2400)
{
    for (int i = 0; i < frames && training.result == RescueResult::Running; ++i)
        training.Update(1.0f / 60);
}
}

int main()
{
    DollBehaviorPrototype training;
    assert(!training.active && training.dolls.size() == 2);
    training.Start();
    Run(training);
    assert(training.result == RescueResult::Success);
    assert(training.report.alliesRescued == 1 && training.report.autonomousActions == 1);
    assert(!training.dolls[1].down);
    auto report = training.report;
    training.Update(1);
    assert(training.report.decisions == report.decisions);
    assert(training.report.alliesRescued == 1);

    training.Start(true);
    assert(training.rescuer == 1);
    Run(training);
    assert(training.result == RescueResult::VictimLost);
    assert(training.report.autonomousActions == 0);

    training.Start();
    training.SetCommand(DollCommand::Rescue);
    Run(training);
    assert(training.result == RescueResult::Success);
    assert(training.report.autonomousActions == 0);

    training.Start();
    training.SetCommand(DollCommand::Hold);
    training.Update(.01f);
    assert(training.dolls[1].action == DollAction::Wait);
    training.SetCommand(DollCommand::Rescue);
    Run(training);
    assert(training.result == RescueResult::Success);

    training.Start();
    training.selectedTrait = 4;
    training.AdjustTrait(100);
    assert(training.dolls[1].traits.autonomy == 1);
    training.AdjustTrait(-100);
    assert(training.dolls[1].traits.autonomy == 0);
    training.Update(std::numeric_limits<float>::quiet_NaN());
    assert(training.report.elapsed == 0);
    training.Stop();
    assert(!training.active && training.result == RescueResult::Aborted);
    training.RestoreTraits();
    assert(std::abs(training.dolls[1].traits.autonomy - .2f) < .001f);

    // Equivalent fixed-step inputs are deterministic when no external command changes occur.
    DollBehaviorPrototype first, second;
    first.Start();
    second.Start();
    Run(first);
    Run(second);
    assert(first.result == second.result);
    assert(first.report.elapsed == second.report.elapsed);
    assert(first.dolls[0].position.x == second.dolls[0].position.x);
}
