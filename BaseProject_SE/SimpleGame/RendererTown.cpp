#include "stdafx.h"
#include "Renderer.h"
#include <cmath>
#include <algorithm>

void Renderer::TownBuildings(const RpgGame &game)
{
    if (!shadowPass)
    {
        effects.Material(2);
        float first = novel::Villages().front().x, last = novel::Villages().back().x;
        float left = std::max(first - 11, game.player.x - ViewRadius() - 2.0f);
        float right = std::min(last + 11, game.player.x + ViewRadius() + 2.0f);
        if (right > left)
            Cube((left + right) * .5f, .012f, 3, right - left, .018f, 4, .57f, .46f, .29f);
        for (const auto &v : novel::Villages())
            if (std::abs(v.x - game.player.x) < ViewRadius() + 12)
            {
                Cube(v.x, .014f, 3, 2, .018f, 24, .57f, .46f, .29f);
                Cube(v.x, .016f, 3, 6, .018f, 6, .63f, .54f, .36f);
            }
    }
    for (const auto &building : TownNpc::Buildings())
    {
        if (std::abs(building.x - game.player.x) > ViewRadius() + 5 ||
            std::abs(building.z - game.player.z) > ViewRadius() + 5)
            continue;
        if (!shadowPass)
            effects.Material(7);
        Cube(building.x, .9f, building.z, building.width, 1.8f, building.depth, .77f, .69f, .49f);
        if (!shadowPass)
            effects.Material(4);
        for (int tier = 0; tier < 5; ++tier)
            Cube(building.x,
                 1.85f + tier * .16f,
                 building.z,
                 building.width + .4f,
                 .17f,
                 building.depth + .5f - tier * .58f,
                 .34f,
                 .18f,
                 .11f);
        for (float side : {-1.0f, 1.0f})
            Cube(building.x + side * (building.width * .5f - .1f),
                 .9f,
                 building.z + building.depth * .5f + .03f,
                 .16f,
                 1.8f,
                 .1f,
                 .24f,
                 .15f,
                 .08f);
        Cube(building.x,
             .55f,
             building.z + building.depth * .5f + .02f,
             .8f,
             1.1f,
             .06f,
             .12f,
             .18f,
             .17f);
        Cube(building.x - 1,
             1.15f,
             building.z + building.depth * .5f + .04f,
             .6f,
             .5f,
             .05f,
             .76f,
             .63f,
             .33f);
    }
}

void Renderer::TownHud(const RpgGame &game)
{
    for (const auto &enemy : game.enemies)
    {
        if (enemy.hp <= 0 ||
            std::hypot(enemy.position.x - game.player.x, enemy.position.z - game.player.z) > 12)
            continue;
        GLdouble x, y, z;
        if (gluProject(
                enemy.position.x, 2.2, enemy.position.z, model, projection, viewport, &x, &y, &z) &&
            z >= 0 && z <= 1)
        {
            float sx = float(x) * 1280 / width, sy = 800 - float(y) * 800 / height;
            if (sx > 0 && sx < 1280 && sy > 235 && sy < 690)
                Text(sx - 40,
                     sy,
                     "마수 · " + std::string(+RpgEnemy::ActionLabel(enemy.action)),
                     12,
                     1,
                     enemy.action == BanditAction::Attack ? .4f : .8f,
                     .35f);
        }
    }
    // Static names reuse the existing text texture cache.
    for (const auto &npc : game.npcs)
    {
        if (std::hypot(npc.position.x - game.player.x, npc.position.z - game.player.z) > 12)
            continue;
        GLdouble x, y, z;
        if (gluProject(
                npc.position.x, 2.2, npc.position.z, model, projection, viewport, &x, &y, &z) &&
            z >= 0 && z <= 1)
        {
            float sx = float(x) * 1280 / width, sy = 800 - float(y) * 800 / height;
            if (sx < 0 || sx > 1280 || sy < 105 || sy > 690)
                continue;
            Text(sx - 38,
                 sy,
                 npc.name + " · " +
                     (game.IsRescueActor(npc.id) ? "실종 사냥꾼" : TownNpc::RoleLabel(npc.role)) +
                     (npc.action == NpcAction::Rob    ? " · 약탈 중"
                      : npc.criminalUntil > game.time ? " · 범죄"
                                                      : ""),
                 12,
                 .9f,
                 .85f,
                 .65f);
        }
    }
    WorldPoint trail{novel::Villages()[1].x + 20, 3};
    if (std::hypot(trail.x - game.player.x, trail.z - game.player.z) < 12 &&
        game.LineOfSight(game.player, trail))
    {
        GLdouble x, y, z;
        if (gluProject(trail.x, .6, trail.z, model, projection, viewport, &x, &y, &z) && z >= 0 &&
            z <= 1)
            Text(float(x) * 1280 / width - 50,
                 800 - float(y) * 800 / height,
                 "흩어진 짐 · 가까이에서 E 조사",
                 13,
                 .95f,
                 .85f,
                 .5f);
    }
    int nearestNpc = game.NearestNpc();
    if (nearestNpc >= 0 && game.talkingNpc < 0)
        Text(455,
             658,
             "F: " + game.npcs[nearestNpc].name + " 대화",
             18,
             .95f,
             .85f,
             .5f);
    if (game.talkingNpc < 0)
        return;
    const auto &npc = game.npcs[game.talkingNpc];
    Panel(750, 185, 510, 470, .025f, .055f, .07f, .96f);
    Text(773, 207, npc.name + " / " + TownNpc::RoleLabel(npc.role), 23);
    Text(773,
         245,
         npc.routine == NpcRoutine::Sleep  ? "수면 일과 · 영업 중단"
         : npc.routine == NpcRoutine::Meal ? "식사 일과 · 영업 중단"
                                           : "근무 일과 · 대화 중",
         16);
    Text(773,
         266,
         "내 은화 " + std::to_string(game.silver) + " / 상대 은화 " + std::to_string(npc.silver) +
             " / 상품 " + std::to_string(npc.potionStock) + " / 의약 " +
             std::to_string(npc.medicalSupplies),
         12);
    if (game.npcDebug)
    {
        const auto &mind = npc.mind;
        auto percent = [](float value)
        {
            return std::to_string(int(value * 100));
        };
        Text(773,
             285,
             "생존 " + percent(mind.needs.survive) + " / 동료 보호 " +
                 percent(mind.needs.protectAlly),
             15);
        Text(773,
             315,
             "임무 " + percent(mind.needs.completeObjective) + " / 명령 " +
                 percent(mind.needs.obeyCommander),
             15);
        Text(773,
             350,
             "기억 " + std::to_string(mind.memories.size()) + " / 알고 있는 정보 " +
                 std::to_string(mind.knowledge.size()),
             15);
        Text(773,
             385,
             "구조 담당 대상 " + std::to_string(mind.assignedPatient) + " / 입찰 점수 " +
                 percent(mind.rescueBid),
             15);
        Text(773,
             420,
             "구조 성향 " + percent(npc.helpTraitScore) + " / 관계 " +
                 percent(npc.helpRelationshipScore),
             15);
        Text(773,
             450,
             "회상 가산 " + percent(npc.helpMemoryScore) + " / 위험 감점 " +
                 percent(npc.helpRiskScore),
             15);
        Text(773, 490, "판단·정보 출처·관계 상세: npc_ai.jsonl", 14);
        Text(773, 532, "전술 관측 5초 · 소문은 별도 보관 · 대면 전달", 13);
        Text(773, 585, "J 대화 화면 · H 회복약 지원 · F 닫기", 15);
        return;
    }
    const char *primary = "1: 순찰 거점 지정", *secondary = "2: 사냥터 정보";
    switch (npc.role)
    {
    case NpcRole::Merchant:
        primary = "1: 회복약 구매 (은화 3)";
        secondary = "2: 회복약 판매 (은화 +2)";
        break;
    case NpcRole::Medic:
        primary = "1: 체력 전체 치료 (은화 2)";
        secondary = "2: 치료 안내";
        break;
    case NpcRole::Helper:
        primary = "1: 무료 응급 처치 (+25, 재사용 45초)";
        secondary = "2: 마을 안내";
        break;
    case NpcRole::Swindler:
        primary = "1: 보증 없는 특가 상자 (은화 4)";
        secondary = "2: 수상한 거래 거절";
        break;
    case NpcRole::Outlaw:
    case NpcRole::Bully:
        primary = "1: 통행료 지불 (은화 2)";
        secondary = "2: 요구 거절 (외부에서 적대 가능)";
        break;
    default:
        break;
    }
    if (game.IsRescueActor(npc.id))
    {
        primary = "1: 아플리아 외곽까지 호위 시작/재개";
        secondary = "2: 귀환 목적지 안내";
    }
    Text(773, 295, primary, 17, .95f, .8f, .4f);
    Text(773, 326, secondary, 16);
    if (game.IsRescueActor(npc.id))
        Text(773, 348, "1: 호위 시작/재개 · 목적지: 아플리아", 14);
    else
        Text(773,
             348,
             npc.role == NpcRole::Merchant ? "3: 단서 / 4: 식량 / 5: 식수 (각 은화 1)"
                                           : "3: 실종자 단서 문의 / 구조 보고",
             13);
    Text(773, 378, "이 주민에게 들은 소식", 13, .9f, .8f, .5f);
    float y = 405;
    int lines = 0;
    for (auto k = npc.rumors.rbegin(); k != npc.rumors.rend() && lines < 2; ++k)
        for (const auto &r : game.rumors)
            if (r.id == k->rumor)
            {
                Text(773, y, r.text, 11);
                y += 24;
                ++lines;
            }
    if (!lines)
        Text(773, y, "아직 전해 들은 소식이 없습니다.", 13);
    Text(773,
         472,
         "체력 " + std::to_string(int(npc.hp)) + " · 집중력 " + std::to_string(npc.ammo) +
             " · 신뢰 " + std::to_string(int(npc.trust * 100)),
         14);
    Text(773,
         505,
         npc.courier ? "상단 전령 · 마을을 걸어서 오가며 소식 전달"
                     : "현지 주민 · 가까운 상대와만 소식 교환",
         13);
    Text(773, 535, "주민의 이름·생활 사건은 게임용 창작입니다.", 12);
    Text(773,
         565,
         game.QuestReady() ? "귀환 완료: 출발지 경비에게 3번 보고" : "L: 현재 의뢰와 귀환 안내",
         13);
    Text(773, 607, "F 닫기 · H 회복약 지원 · J AI 정보", 15);
}
