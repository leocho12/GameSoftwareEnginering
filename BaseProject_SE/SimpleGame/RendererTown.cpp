#include "stdafx.h"
#include "Renderer.h"
#include <cmath>
#include <algorithm>

void Renderer::TownBuildings(const RpgGame &game)
{
    if (!shadowPass)
    {
        effects.Material(2);
        Cube(3, .012f, 3, 24, .018f, 2, .4f, .38f, .31f);
        Cube(3, .013f, 3, 2, .018f, 24, .4f, .38f, .31f);
        Cube(3, .015f, 3, 5, .018f, 5, .47f, .45f, .37f);
    }
    for (const auto &building : TownNpc::Buildings())
    {
        if (std::abs(building.x - game.player.x) > ViewRadius() + 5 ||
            std::abs(building.z - game.player.z) > ViewRadius() + 5)
            continue;
        if (!shadowPass)
            effects.Material(4);
        Cube(building.x, .9f, building.z, building.width, 1.8f, building.depth, .36f, .4f, .39f);
        Cube(building.x,
             1.95f,
             building.z,
             building.width + .25f,
             .3f,
             building.depth + .25f,
             .18f,
             .24f,
             .26f);
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
                     "야적 " + std::to_string(enemy.band + 1) + "조 · " +
                         RpgEnemy::ActionLabel(enemy.action),
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
                 npc.name + " · " + TownNpc::RoleLabel(npc.role) +
                     (npc.action == NpcAction::Rob    ? " · 약탈 중"
                      : npc.criminalUntil > game.time ? " · 범죄"
                                                      : ""),
                 12,
                 .9f,
                 .85f,
                 .65f);
        }
    }
    int alive = int(std::count_if(game.enemies.begin(),
                                  game.enemies.end(),
                                  [](const RpgEnemy &enemy)
                                  {
                                      return enemy.hp > 0;
                                  }));
    Text(850,
         80,
         "주민 " + std::to_string(game.npcs.size()) + "명 / 야적 " + std::to_string(alive) +
             "명 / " + std::to_string(game.banditGroups.size()) + "무리",
         12);
    Text(850,
         103,
         "내 부품 " + std::to_string(game.scrap) +
             (game.townRadioConnected ? " · K: 무전 끊기 · J: AI" : " · K: 무전 복구"),
         12);
    int near = game.NearestNpc();
    if (near >= 0 && game.talkingNpc < 0)
        Text(455, 658, "F: " + game.npcs[near].name + " 대화", 18, .95f, .85f, .5f);
    if (game.talkingNpc < 0)
        return;
    const auto &npc = game.npcs[game.talkingNpc];
    Panel(750, 185, 510, 470, .025f, .055f, .07f, .96f);
    Text(773, 207, npc.name + " / " + TownNpc::RoleLabel(npc.role), 23);
    Text(773, 245, "대화 중 · 이동 정지", 16);
    Text(773,
         266,
         "내 부품 " + std::to_string(game.scrap) + " / 상대 부품 " + std::to_string(npc.scrap) +
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
        Text(773, 532, "정보는 5초 후 소멸 · 연결된 구조자 1명 선정", 13);
        Text(773, 585, "J 대화 화면 · H 회복약 지원 · F 닫기", 15);
        return;
    }
    const char *primary = "1: 순찰 거점 지정", *secondary = "2: 사냥터 정보";
    switch (npc.role)
    {
    case NpcRole::Merchant:
        primary = "1: 회복약 구매 (부품 3)";
        secondary = "2: 회복약 판매 (부품 +2)";
        break;
    case NpcRole::Medic:
        primary = "1: 체력 전체 치료 (부품 2)";
        secondary = "2: 치료 안내";
        break;
    case NpcRole::Helper:
        primary = "1: 무료 응급 처치 (+25, 재사용 45초)";
        secondary = "2: 마을 안내";
        break;
    case NpcRole::Swindler:
        primary = "1: 보증 없는 특가 상자 (부품 4)";
        secondary = "2: 수상한 거래 거절";
        break;
    case NpcRole::Outlaw:
    case NpcRole::Bully:
        primary = "1: 통행료 지불 (부품 2)";
        secondary = "2: 요구 거절 (외부에서 적대 가능)";
        break;
    default:
        break;
    }
    if (npc.role == NpcRole::Guard)
        Text(773,
             555,
             game.patrolReported  ? "첫 순찰: 보고 완료 (보상 수령)"
             : game.PatrolReady() ? "첫 순찰: F 대화로 보고 완료"
                                  : "첫 순찰: 레벨 3 · 전리품 2개 후 복귀",
             13);
    Text(773, 291, primary, 17, .95f, .8f, .4f);
    Text(773, 326, secondary, 16);
    Text(773,
         377,
         "공격 " + std::to_string(int(npc.traits.aggression * 100)) + " / 구조 " +
             std::to_string(int(npc.traits.rescuePriority * 100)) + " / 자율 " +
             std::to_string(int(npc.traits.autonomy * 100)),
         15);
    Text(773,
         410,
         "신뢰 " + std::to_string(int(npc.trust * 100)) + " / 처치 " + std::to_string(npc.kills) +
             " / 동료 지원 " + std::to_string(npc.helped),
         15);
    Text(773,
         448,
         "체력 " + std::to_string(int(npc.hp)) + " / 탄약 " + std::to_string(npc.ammo),
         15);
    Text(773,
         490,
         npc.serviceCooldown > 0
             ? "응대 대기: " + std::to_string(int(npc.serviceCooldown) + 1) + "초"
             : "선택 시 거래/상호작용이 실행됩니다.",
         14);
    Text(773, 532, "창작 주민 · 원작 SD 시험 외형 · 원작 인물과 별개", 13);
    Text(773, 585, "F 닫기 · H 회복약 지원 · J AI 정보", 16);
}
