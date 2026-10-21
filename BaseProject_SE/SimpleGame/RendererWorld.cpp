#include "stdafx.h"
#include "Renderer.h"
#include <cmath>

void Renderer::WorldHud(const RpgGame &game)
{
    const auto &world = game.world;
    if (!world.Ready())
        return;
    for (const auto &entry : world.Actors())
    {
        const auto &a = entry.second;
        if (!a.available)
            continue;
        GLdouble x, y, z;
        if (gluProject(a.position.x, 2.3, a.position.z, model, projection, viewport, &x, &y, &z) &&
            z >= 0 && z <= 1)
        {
            float sx = float(x) * 1280 / width, sy = 800 - float(y) * 800 / height;
            if (sx > 0 && sx < 1280 && sy > 155 && sy < 680)
                Text(sx - 25,
                     sy,
                     a.name + " · " + narrative::WorldSimulation::TaskLabel(a.task),
                     13,
                     .5f,
                     .9f,
                     1);
        }
    }
    if (game.talkingActor)
    {
        const auto *a = world.Find(game.talkingActor);
        if (a)
        {
            Panel(745, 190, 520, 450, .025f, .06f, .09f, .97f);
            Text(765, 215, a->name + " · 그리폰 시험 동료", 21);
            Text(765, 265, world.Dialogue(a->id), 14);
            if (a->brain.belief.aware && world.Event().state == narrative::EventState::Active)
            {
                if (world.Event().participants.empty())
                    Text(765, 315, "1: 내가 직접 수색한다", 18, .95f, .8f, .4f);
                Text(765, 355, "2: 각자의 판단에 맡긴다", 18);
            }
            Text(765,
                 420,
                 "현재 목표: " +
                     std::string(narrative::WorldSimulation::GoalLabel(a->brain.intention)),
                 16);
            Text(765,
                 465,
                 "체력 " + std::to_string(int(a->hp)) + " / 기억 " +
                     std::to_string(a->memories.size()),
                 15);
            Text(765, 570, "F 종료 · V 판단 근거 · 출발 시 대화 중단", 14);
        }
    }
    if (!game.worldDebug)
    {
        return;
    }
    Panel(15, 180, 710, 495, .02f, .035f, .06f, .96f);
    const auto *a = world.Find(game.selectedWorldActor);
    Text(35, 200, "월드 AI 진단 (세계 진실 포함) · T 시간 +10분", 18, .5f, .9f, 1);
    Text(35,
         235,
         "사건 #1 / 기한 " + std::to_string(world.Event().deadline) + "분 / 결과 " +
             narrative::WorldSimulation::OutcomeLabel(world.Event().outcome),
         14);
    Text(35,
         265,
         "지역 물자 " + std::to_string(world.Region().supplies) + " / 구조 " +
             std::to_string(world.Region().rescued) + " / 예약 " +
             std::to_string(world.QueueSize()),
         14);
    if (!a)
        return;
    auto number = [](float n)
    {
        return std::to_string(int(n * 100));
    };
    Text(35,
         300,
         a->name + " / " + narrative::WorldSimulation::TaskLabel(a->task) + " / 체력 " +
             std::to_string(int(a->hp)),
         18);
    Text(35,
         333,
         "Belief: " + std::string(a->brain.belief.aware ? "실종 신호 인지" : "정보 없음") +
             " / 신뢰도 " + number(a->brain.belief.confidence) + " / 출처 " +
             std::to_string(a->brain.belief.source),
         14);
    Text(35,
         365,
         "Desire: 기지 " + number(a->brain.desires[0]) + " / 구조 " + number(a->brain.desires[1]) +
             " / 회복 " + number(a->brain.desires[2]),
         14);
    Text(35,
         395,
         "Intention: " + std::string(narrative::WorldSimulation::GoalLabel(a->brain.intention)) +
             " / " + a->brain.reason,
         14);
    Text(35,
         425,
         "구조 기여: 성향 " + number(a->brain.personalityScore) + " 관계 " +
             number(a->brain.relationshipScore) + " 능력 " + number(a->brain.skillScore) +
             " 위험 " + number(a->brain.riskScore),
         14);
    std::string relations = "관계 ";
    for (const auto &r : a->relations)
        if (r.first != narrative::PlayerId)
            relations += std::to_string(r.first) + " 신뢰 " + number(r.second.trust) + " 빚 " +
                         number(r.second.debt);
    Text(35, 455, relations, 14);
    Text(35,
         485,
         "기억 " + std::to_string(a->memories.size()) +
             (a->memories.empty() ? "" : " / " + a->memories.back().tag),
         14);
    Text(35, 515, "GOAP / HTN: 미구현 · 고정 TaskSequence 실행", 14);
    Text(35,
         545,
         "CPU 갱신 " + std::to_string(world.updateMs) + "ms · 판단 " +
             std::to_string(world.decisionMs) + "ms",
         13);
    const auto &log = world.PlayerLog();
    if (!log.empty())
        Text(35, 580, "수신 기록: " + log.back(), 13);
    Text(35, 625, "V 닫기 · B 인물 전환 · world_ai.jsonl 상세 기록", 14);
}

void Renderer::StoryHud(const RpgGame &game)
{
    if (game.worldDebug && !game.storyJournalOpen)
        return;
    Panel(24, 240, 710, 125, .025f, .06f, .08f, .92f);
    Text(38, 250, "이야기 | 1장 끊어진 보급선 · 지역 창작 프롤로그", 16, .6f, .9f, 1);
    Text(38, 279, game.StoryStatus(), 13);
    Text(38, 306, game.StoryNextStep(), 12);
    Text(38, 337, "L: 이야기 일지 · 원작 지휘관과 별개인 그리폰 현장 지휘관", 12, .7f, .8f, .85f);
    if (!game.storyJournalOpen)
        return;
    Panel(120, 110, 1040, 575, .02f, .045f, .065f, .98f);
    Text(148, 133, "이야기 일지 | L로 닫기", 24, .65f, .9f, 1);
    Text(148, 180, "당신은 그리폰 & 크루거의 인간 지휘관으로 변경 마을을 담당합니다.", 16);
    Text(148, 211, "원작 주인공과 별개로 주민·현장 동료의 안전과 보급을 맡습니다.", 16);
    Text(148, 247, "현장 임무: " + game.PatrolObjective(), 16, .95f, .84f, .55f);
    Text(148, 280, "지역 소식: " + game.StoryStatus(), 14);
    Text(148, 313, game.StoryNextStep(), 14);
    Text(148, 352, "받은 무전·직접 들은 보고 (지역 사건은 기다리지 않고 진행됩니다)", 16);
    float y = 386;
    const auto &lines = game.world.PlayerLog();
    if (lines.empty())
        Text(148, y, "아직 확인한 소식이 없습니다. 기지의 미라·벨에게 F로 대화하세요.", 14);
    for (std::size_t i = lines.size() > 5 ? lines.size() - 5 : 0; i < lines.size(); ++i)
    {
        Text(148, y, lines[i], 13);
        y += 28;
    }
    Text(148, 557, "구현 범위: 현장 순찰과 실종 인형 구조 시뮬레이션", 15, .8f, .85f, .8f);
    Text(148, 588, "중계소 복구·수송차 수색·회사 잔류/독립 분기는 아직 기획 단계입니다.", 14);
    Text(
        148, 626, "일지를 열어도 세계 시간과 전투는 계속됩니다. Esc: 일시정지", 13, .95f, .8f, .5f);
}
