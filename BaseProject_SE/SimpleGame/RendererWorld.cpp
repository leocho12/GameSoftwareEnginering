#include "stdafx.h"
#include "Renderer.h"
#include <cmath>

void Renderer::WorldHud(const RpgGame &game)
{
    if (!game.worldDebug)
        return;
    Panel(15, 180, 710, 495, .055f, .065f, .045f, .97f);
    Text(35, 198, "왕국 교역로 · 마을 통계 [M / V]", 21, .94f, .85f, .6f);
    Text(35, 236, "광장 간 72 거리 / 기본 속도 3.6 / 직선 이동 약 20초", 14);
    float y = 280;
    for (int i = 0; i < novel::VillageCount; ++i)
    {
        const auto &v = novel::Villages()[i];
        const auto &s = game.villageStats[i];
        Text(35,
             y,
             std::string(game.visitedVillages[i] ? "● " : "○ ") + v.name + " · " + s.label +
                 " · 현장 " + std::to_string(s.present) + " / 등록 20",
             16);
        Text(55,
             y + 27,
             "안전 " + std::to_string(int(s.safety)) + " · 사기 " + std::to_string(int(s.morale)) +
                 " · 경제 " + std::to_string(int(s.prosperity)) + " · 소문 인지 " +
                 std::to_string(s.rumorHolders),
             13);
        y += 61;
    }
    Text(35, 613, "소문: 대면 4거리 · 마을 간 전령의 실제 이동 필요", 14);
    Text(35, 644, "세부 통계·최근 120초 추이: 별도 브라우저 월드 모니터", 13);
}

void Renderer::StoryHud(const RpgGame &game)
{
    if (!game.storyJournalOpen)
        return;
    Panel(120, 110, 1040, 575, .055f, .055f, .035f, .98f);
    Text(148, 133, "모험가의 일지 | L로 닫기", 24, .95f, .85f, .6f);
    Text(148, 181, "용사 파티 때려치웁니다 · 왕국 시기의 별도 모험가", 18);
    Text(148, 217, "용사 카일과 회색 마법사 라니엘이 살아가는 세계의 교역로입니다.", 16);
    Text(148, 249, "아플리아 아카데미·잿빛 마탑·델로힘 신앙이 존재하는 마법과 검의 세계.", 15);
    Text(148, 290, "의뢰: 실종 사냥꾼의 단서를 찾아 구조하고 아플리아 외곽으로 호위.", 16);
    Text(148, 331, game.StoryStatus(), 16, .95f, .83f, .55f);
    Text(148, 366, game.StoryNextStep(), 13);
    Text(148, 410, "F 대화 → 3: 단서/보고 · 카디낙 동쪽 길가에서 E: 현장 조사", 15);
    Text(148, 446, "허기·갈증·피로는 실시간 진행. G 달리기 / C 은신 / R 마을 휴식", 15);
    Text(148,
         494,
         "5개 정착지의 연결·거리·의뢰·주민은 게임용 각색이며 원작 공식 지도가 아닙니다.",
         14);
    Text(148, 535, "원작 주인공의 능력과 정체를 플레이어에게 복사하지 않습니다.", 14);
    Text(148, 581, "부상자는 14거리 이상 멀어지면 대기. 돌아가 E로 호위를 재개하세요.", 14);
    Text(148, 626, "일지와 통계창을 열어도 전투는 계속됩니다. Esc: 일시정지", 13, .95f, .8f, .5f);
}
