#include "stdafx.h"
#include "Renderer.h"
#include <cmath>

void Renderer::WorldHud(const RpgGame &game)
{
    const auto &world = game.world;
    if (!world.Ready())
    {
        Text(25, 130, "월드 AI 데이터 오류: " + world.error, 14, 1, .4f, .3f);
        return;
    }
    Text(25,
         130,
         "기지 시간 " + world.ClockLabel() + " · V 월드 AI · B 인물 전환 · F 대화",
         14,
         .55f,
         .9f,
         1);
    if (world.Event().playerAccepted && world.Event().state == narrative::EventState::Active)
        Text(25, 155, "남쪽 길의 초록색 구조 표식 · E 구조 · 동료 출발 전 도착", 14);
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
        if (!world.PlayerLog().empty())
            Text(25, 178, world.PlayerLog().back(), 13, .7f, .85f, .9f);
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
