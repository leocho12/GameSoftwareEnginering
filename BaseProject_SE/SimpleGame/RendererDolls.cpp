#include "stdafx.h"
#include "Renderer.h"
#include <algorithm>
#include <iomanip>
#include <sstream>

namespace
{
std::string Decimal(float value)
{
    std::ostringstream text;
    text << std::fixed << std::setprecision(2) << value;
    return text.str();
}
}

void Renderer::DollActors(const DollBehaviorPrototype &training)
{
    glUseProgram(0);
    glDisable(GL_LIGHTING);
    glDisable(GL_FOG);
    glActiveTexture(GL_TEXTURE0);
    std::array<int, 2> order{0, 1};
    std::sort(order.begin(),
              order.end(),
              [&](int a, int b)
              {
                  auto p = training.dolls[a].position, q = training.dolls[b].position;
                  return p.x * 17 + p.z * 19 < q.x * 17 + q.z * 19;
              });
    for (int i : order)
    {
        const auto &doll = training.dolls[i];
        auto p = doll.position;
        bool moving = !doll.down && training.result == RescueResult::Running &&
                      (doll.action == DollAction::Rescue || doll.action == DollAction::Cover ||
                       doll.action == DollAction::Retreat);
        const bool acting = training.result == RescueResult::Running && !doll.down;
        const char *clip = acting && doll.action == DollAction::Attack ? "attack"
                           : moving                                    ? "move"
                                                                       : "wait";
        // Use a lowered idle pose for injured dolls; death clips can end fully transparent.
        // Existing assets are presentation proxies, not canonical character personalities.
        float y = doll.down ? -.35f : 0;
        if (!Character(
                i + 1, clip, doll.down ? 0 : training.report.elapsed, p.x, y, p.z, false, true))
            Sprite(p.x, y, p.z, i, moving ? int(training.report.elapsed * 10) % 8 : 0);
        if (!shadowPass)
        {
            Ring(p.x, .04f, p.z, .4f, i == 0 ? .25f : .85f, .55f, 1);
            Line(p.x - .3f, 1.95f, p.z + .27f, p.x + .3f, 1.95f, p.z - .27f, .15f, .15f, .15f);
            float ratio = doll.hp / 100;
            Line(p.x - .3f,
                 1.96f,
                 p.z + .27f,
                 p.x - .3f + .6f * ratio,
                 1.96f,
                 p.z + .27f - .54f * ratio,
                 doll.down ? 1 : .3f,
                 .7f,
                 .3f);
        }
    }
    if (shadowPass)
        return;
    Ring(1, .03f, 3, .7f, .25f, 1, .5f);
    Ring(1, .03f, 1, .5f, .3f, .7f, 1);
    Ring(5, .03f, 5, .6f, 1, .25f, .2f);
    if (training.dolls[training.rescuer].action == DollAction::Attack &&
        training.result == RescueResult::Running)
    {
        auto p = training.dolls[training.rescuer].position;
        Line(p.x, .8f, p.z, 5, .6f, 5, 1, .75f, .3f);
    }
}

void Renderer::DollHud(const DollBehaviorPrototype &training, bool paused)
{
    Panel(0, 0, 1280, 103, .035f, .065f, .08f, .97f);
    Text(24, 14, "Doll AI / 동료 구조 훈련", 24, .95f, .87f, .67f);
    Text(24, 52, "Doll A·B는 가상 시험 인형 / M4A1·UMP45 SD는 시험 외형", 14);
    Text(640, 18, training.StatusLabel(), 17, .9f, .8f, .5f);
    Text(640,
         54,
         "경과 " + Decimal(training.report.elapsed) + "초 / 위협 " + Decimal(training.threat),
         15);
    const char *commands[] = {"자율 판단", "구조 명령", "대기 명령"};
    Panel(12, 118, 300, 510, .04f, .07f, .09f, .92f);
    Text(27, 135, "구조 담당: Doll " + std::string(training.rescuer == 0 ? "A" : "B"), 21);
    Text(27, 173, "현재 지시: " + std::string(commands[int(training.command)]), 17);
    Text(27,
         207,
         "행동: " + std::string(DollBehaviorPrototype::ActionLabel(
                        training.dolls[training.rescuer].action)),
         18);
    Text(27, 250, "행동 점수 (-1: 선택 불가)", 15);
    for (int i = 0; i < 5; ++i)
        Text(27,
             280.0f + i * 28,
             DollBehaviorPrototype::ActionLabel(DollAction(i)) + std::string(": ") +
                 Decimal(training.dolls[training.rescuer].utilities[i]),
             16);
    Text(27, 439, "자율성 > 0.70: 자율 구조 가능", 14);
    Text(27, 470, "녹색 원: 이송 / 파란 원: 엄폐", 14);
    Text(27, 500, "빨간 원: 모의 위협 (별도 적 없음)", 13);
    Text(27, 539, "구조 횟수 " + std::to_string(training.report.alliesRescued), 16);
    Text(27,
         570,
         "피해 " + Decimal(training.report.damageTaken) + " / 판단 " +
             std::to_string(training.report.decisions),
         15);
    Panel(970, 118, 298, 510, .04f, .07f, .09f, .92f);
    for (int doll = 0; doll < 2; ++doll)
    {
        float top = 135.0f + doll * 238;
        Text(984,
             top,
             std::string(doll == 0 ? "Doll A" : "Doll B") + " / 체력 " +
                 Decimal(training.dolls[doll].hp),
             19);
        for (int i = 0; i < 5; ++i)
        {
            bool selected = doll == training.rescuer && i == training.selectedTrait;
            Text(984,
                 top + 38 + i * 30,
                 std::string(selected ? "> " : "  ") + DollBehaviorPrototype::TraitLabel(i) + " " +
                     Decimal(training.dolls[doll].traits.Get(i)),
                 15,
                 selected ? 1.0f : .8f,
                 selected ? .8f : .85f,
                 .65f);
        }
    }
    // Project the two names through the same camera used to draw their sprites.
    for (int i = 0; i < 2; ++i)
    {
        const auto &doll = training.dolls[i];
        GLdouble x, y, z;
        if (gluProject(
                doll.position.x, 2.2, doll.position.z, model, projection, viewport, &x, &y, &z))
            Text(float(x) * 1280 / width - 34,
                 800 - float(y) * 800 / height,
                 std::string(i == 0 ? "Doll A" : "Doll B") + (doll.down ? " [부상]" : ""),
                 14);
    }
    Panel(0, 690, 1280, 110, .035f, .065f, .08f, .98f);
    Text(24,
         705,
         "1 구조 · 2 대기 · 3 명령 해제 · R 재시도 · Tab 역할 교환 · F6 사냥터 복귀 · Esc 정지",
         16);
    Text(24,
         738,
         "[ / ] Trait 선택 · - / + 값 조절 · 0 기본 Trait 복원 | 유전·모방·영속 학습은 아직 없음",
         15);
    Text(
        24, 769, "AI 판단 로그: 실행 파일 옆 Profiles/adbs.jsonl · 훈련 중 사냥터 진행은 정지", 13);
    if (paused)
    {
        Panel(350, 320, 580, 100, .03f, .06f, .08f, .98f);
        Text(380, 350, "훈련 일시정지 · Esc로 계속", 24);
    }
}
