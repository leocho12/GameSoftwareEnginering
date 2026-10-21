#include "stdafx.h"
#include "RpgGame.h"
#include <Windows.h>
#include <algorithm>
#include <cmath>
#include <fstream>
#include <locale>

namespace
{
std::filesystem::path RuntimeDirectory()
{
    wchar_t path[MAX_PATH]{};
    GetModuleFileNameW(nullptr, path, MAX_PATH);
    return std::filesystem::path(path).parent_path();
}
}

std::string RpgGame::StoryStatus() const
{
    std::string status =
        patrolReported ? "현장 순찰: 복귀 보고 완료" : "현장 순찰: 성장·물자 회수 중";
    if (world.Ready() && world.ResultReported())
        status += " / 구조 보고: " +
                  std::string(narrative::WorldSimulation::OutcomeLabel(world.Event().outcome));
    else if (world.Ready() && world.PlayerAware())
        status += " / 실종 인형의 구조 요청 확인";
    return status;
}

std::string RpgGame::StoryNextStep() const
{
    if (!world.Ready())
        return "구조 사건 자료를 불러오지 못했습니다. 현장 순찰 목표는 계속할 수 있습니다.";
    if (world.ResultReported())
        return patrolReported
                   ? "현장 시험 임무 종료. 중계소·수송차 본편 퀘스트는 아직 미구현입니다."
                   : "구조 보고는 일지에 보관됩니다. 순찰 목표를 마치고 경비에게 F로 보고하세요.";
    bool canTalk = false;
    for (const auto &entry : world.Actors())
        canTalk |= world.CanTalk(entry.first);
    if (!world.PlayerAware())
        return canTalk
                   ? "기지의 미라 또는 벨에게 F로 대화해 지역 소식을 확인하세요."
                   : "동료가 현장 임무로 기지를 비웠습니다. 순찰을 진행하며 귀환 보고를 기다리세요.";
    if (world.Event().playerAccepted && world.Event().state == narrative::EventState::Active &&
        world.Event().participants.empty())
        return "직접 수색 수락: 남쪽 길 초록색 구조 표식으로 이동 → E 구조.";
    if (!world.Event().participants.empty())
        return "동료가 수색에 출발했습니다. 기지 귀환 후 F로 보고를 확인하세요.";
    if (world.Event().state != narrative::EventState::Active)
        return "구조 요청의 최종 보고를 아직 듣지 못했습니다. 동료에게 F로 확인하세요.";
    return canTalk ? "동료에게 F 대화 → 1 직접 수색 / 2 자율 판단. 귀환 후 F로 보고 확인."
                   : "동료가 구조 임무를 진행 중입니다. 기지 귀환 후 F로 결과를 확인하세요.";
}

void RpgGame::InitializeWorld()
{
    worldBodies.clear();
    talkingActor = selectedWorldActor = 0;
    worldDebug = false;
    worldLogCursor = 0;
    if (!world.Load(RuntimeDirectory() / "Assets" / "AI"))
        return;
    for (const auto &entry : world.Actors())
    {
        TownNpc body;
        body.position = {entry.second.position.x, entry.second.position.z};
        body.home = body.position;
        worldBodies.emplace(entry.first, body);
        if (!selectedWorldActor)
            selectedWorldActor = entry.first;
    }
    LogWorld();
}

void RpgGame::UpdateWorld(float dt)
{
    if (!world.Ready())
        return;
    world.SetConversation(talkingActor);
    world.Advance(dt, townRadioConnected);
    for (const auto &entry : world.Actors())
    {
        auto id = entry.first;
        const auto &actor = entry.second;
        if (!actor.available)
            continue;
        auto &body = worldBodies.at(id);
        body.position = {actor.position.x, actor.position.z};
        body.moving = false;
        body.pathTimer -= dt;
        if (id == talkingActor)
        {
            body.path.clear();
            world.SetPosition(id, actor.position, false);
            continue;
        }
        if (actor.task == narrative::Task::Departing || actor.task == narrative::Task::WalkingHome)
        {
            auto target =
                actor.task == narrative::Task::Departing ? world.Settings().exit : actor.home;
            MoveNpc(body, {target.x, target.z}, dt);
            world.SetPosition(id, {body.position.x, body.position.z}, body.moving);
            if (std::hypot(body.position.x - target.x, body.position.z - target.z) < .35f)
            {
                body.path.clear();
                if (actor.task == narrative::Task::Departing)
                    world.ArrivedAtExit(id);
                else
                    world.ArrivedHome(id);
            }
            else if (actor.task == narrative::Task::Departing &&
                     world.Minute() - actor.taskSince > world.Settings().departureTimeout)
            {
                world.AbortDeparture(id);
                body.path.clear();
            }
        }
        else
            world.SetPosition(id, actor.position, false);
    }
    if (talkingActor)
    {
        const auto *a = world.Find(talkingActor);
        if (!a || !world.CanTalk(talkingActor) ||
            std::hypot(a->position.x - player.x, a->position.z - player.z) > 3 ||
            !LineOfSight(player, {a->position.x, a->position.z}))
        {
            Notify(world.Text("busy"));
            talkingActor = 0;
        }
    }
    LogWorld();
}

bool RpgGame::InteractWorldActor()
{
    if (!world.Ready())
        return false;
    narrative::ActorId nearest = 0;
    float distance = 2.4f;
    int town = NearestNpc();
    if (town >= 0)
        distance = std::hypot(npcs[town].position.x - player.x, npcs[town].position.z - player.z);
    for (const auto &entry : world.Actors())
    {
        const auto &a = entry.second;
        float d = std::hypot(a.position.x - player.x, a.position.z - player.z);
        if (a.available && d < distance && LineOfSight(player, {a.position.x, a.position.z}))
        {
            nearest = a.id;
            distance = d;
        }
    }
    if (!nearest)
        return false;
    if (!world.Talk(nearest))
    {
        Notify(world.Text("busy"));
        return true;
    }
    talkingNpc = -1;
    talkingActor = selectedWorldActor = nearest;
    world.SetConversation(nearest);
    world.SetPosition(nearest, world.Find(nearest)->position, false);
    firing = false;
    Notify(world.Dialogue(nearest));
    return true;
}

void RpgGame::WorldChoice(int choice)
{
    if (paused || hp <= 0 || dollTraining.active || !talkingActor)
        return;
    const auto *a = world.Find(talkingActor);
    if (!a || std::hypot(a->position.x - player.x, a->position.z - player.z) > 3 ||
        !LineOfSight(player, {a->position.x, a->position.z}))
        return;
    if (world.Choose(talkingActor, choice))
        Notify(choice == 1 ? world.Text("accepted") : "각자의 판단으로 움직이도록 했습니다.");
    else
        Notify("현재 선택할 수 없습니다. 이미 출발했거나 사건이 종료되었습니다.");
    LogWorld();
}

bool RpgGame::WorldRescue()
{
    if (paused || hp <= 0 || dollTraining.active)
        return false;
    auto target = world.Settings().search;
    if (!LineOfSight(player, {target.x, target.z}))
        return false;
    if (!world.PlayerRescue({player.x, player.z}))
        return false;
    Notify(world.Text("player_rescue"));
    LogWorld();
    return true;
}

void RpgGame::FastForwardWorld()
{
    if (paused || hp <= 0 || dollTraining.active || !worldDebug)
        return;
    // Remote clock only: local navigation still requires actual travel and arrival.
    world.SetConversation(talkingActor);
    world.Advance(10.0 * world.Settings().minuteSeconds, townRadioConnected);
    Notify("진단: 월드 시간 +10분 (로컬 이동은 실제 도착 필요)");
    LogWorld();
}

void RpgGame::LogWorld()
{
    const auto &entries = world.Journal();
    if (worldLogCursor >= entries.size())
        return;
    auto directory = RuntimeDirectory() / "Profiles";
    std::error_code error;
    std::filesystem::create_directories(directory, error);
    auto path = directory / "world_ai.jsonl";
    if (std::filesystem::exists(path, error) &&
        std::filesystem::file_size(path, error) > 5 * 1024 * 1024)
    {
        auto previous = directory / "world_ai.previous.jsonl";
        std::filesystem::remove(previous, error);
        error.clear();
        std::filesystem::rename(path, previous, error);
        if (error)
            return;
    }
    std::ofstream output(path, std::ios::app);
    output.imbue(std::locale::classic());
    if (!output)
        return;
    for (; worldLogCursor < entries.size(); ++worldLogCursor)
    {
        const auto &e = entries[worldLogCursor];
        output << "{\"schema_version\":1,\"seed\":" << seed << ",\"event_id\":" << e.eventId
               << ",\"sequence\":" << e.sequence << ",\"game_minute\":" << e.minute
               << ",\"actor_id\":" << e.actor << ",\"target_id\":" << e.target << ",\"kind\":\""
               << e.kind << "\",\"supplies_delta\":" << e.suppliesDelta
               << ",\"rescued_delta\":" << e.rescuedDelta << ",\"injury\":" << e.injury
               << ",\"trust_delta\":" << e.trustDelta << ",\"outcome\":" << int(e.outcome)
               << ",\"cpu_world_update_ms\":" << world.updateMs
               << ",\"cpu_goal_eval_ms\":" << world.decisionMs
               << ",\"queue_depth\":" << world.QueueSize()
               << ",\"events_processed_total\":" << world.processedEvents;
        if (e.kind == "decision")
            output << ",\"belief_confidence\":" << e.confidence << ",\"goal_scores\":["
                   << e.scores[0] << ',' << e.scores[1] << ',' << e.scores[2]
                   << "],\"selected_goal\":" << e.intention << ",\"reason\":\"" << e.reason
                   << "\",\"rescue_contributions\":[" << e.contributions[0] << ','
                   << e.contributions[1] << ',' << e.contributions[2] << ',' << e.contributions[3]
                   << ',' << e.contributions[4] << ']';
        output << "}\n";
    }
}
