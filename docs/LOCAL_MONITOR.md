# 로컬 월드 모니터

2026-10-22 · 다섯 마을/소설 세계관 버전.

## 여는 방법

1. Visual Studio에서 게임을 빌드·실행한다.
2. tools/monitor/OpenMonitor.cmd를 실행하거나, 저장소 루트에서 Python으로 tools/monitor/server.py --open을 실행한다.
3. 별도 브라우저의 http://127.0.0.1:8765/ 에서 확인한다. 화면의 「별도 창으로 열기」도 사용할 수 있다.

HTML 파일을 직접 더블클릭하는 방식이 아니다. 게임 실행 파일 옆 Profiles/monitor.json을 로컬 서버가 읽는다. 서버 창은 유지하고 종료 시 Ctrl+C를 누른다. 다른 출력 경로라면 --snapshot으로 파일을 지정한다. 포트 충돌은 --port 8766처럼 변경한다. 보안을 끄지 않는다.

## 표시 내용

- 마을5개, 등록 주민100명, 마수200개 슬롯과 살아 있음/재생성 대기 구분.
- 교역로4개 각각의 몹 수·목표 슬롯50·광장 간 거리·명목상20초.
- 마을별 등록/현장/외출/방문 주민·부상·전투 불능·적대·진입 몹·지갑 총액.
- 안전·사기·경제·신뢰의0~100 통계, 평온·불안·공포·희망 인원, 최근120초 표본과 최근20개 사기 추이 표시.
- 해당 마을에 **실제로 있는 주민이 알고 있는** 소문·발원지·보유자.
- NPC의 소속 마을·현재 위치·목적지·여행·대화·행동·체력·집중력·지갑·상품 재고·개인 소문 출처·전령 도착 수.
- 의뢰·거래·약탈·도난품 반환, 소문 대면 전달 수·실제 전령 도착 수·마수 재생성 수.
- 이름·직업·행동 검색, 소속 마을 필터, 행 선택 JSON 상세, 수신 JSON 다운로드.

등록 마을과 현재 마을은 다르다. 마을 필터는 소속 기준이다. 현장 인구는 방문객을 포함하고 여행 중·전투 불능 주민을 제외한다. 등록20명이어도 현장 숫자는 변한다. 개발용 전체 상태는 플레이어가 자동으로 알게 된 사실이 아니다.

## JSON 계약

게임 → 원자적 JSON 파일 교체 → 로컬 HTTP → HTML fetch. WebSocket이 아닌 폴링이다.

- 게임은0.5초마다 스냅샷을 쓰고 브라우저는1초마다 /api/snapshot을 요청한다.
- 운송 호환 schema_version=1, 세계 데이터 확장 world_schema_version=2, type=snapshot.
- game/counts/villages/npcs/corridors/objective/story/economy/profiling.
- 마수 배열은 기존 화면 호환 키 bandits를 유지하지만 내용은 새 동물/마수다.
- NPC ID town:0~town:99, village_id는 등록 소속, current_village_id/destination_village_id는 현장·목적지다.
- atmosphere는 계산값과 개인 분위기 분포, mood_history는1초 표본이다.
- known_rumors는 개인별 rumor_id/source_npc_id/learned_at_s/hops다.
- rumor_contact_transfers_total/courier_physical_arrivals_total/monster_respawns_total을 누적 계수한다.
- 서버가 transport의 나이·stale·원본 경로를 추가한다. 스냅샷 없거나 손상이면503 JSON 오류, 복구 시 재시도한다.
- 127.0.0.1만 사용하며 외부 CDN·인터넷·게임 제어 API가 없다.

게임이 포커스를 잃으면 기존 규칙에 따라 일시정지하므로, 브라우저 전환 중 숫자가 정지할 수 있다. 이전 빌드 스냅샷을 읽으면 예전 수치가 나오므로 새 빌드 실행 후 확인한다.

## 확인 상태

브라우저 DOM 회귀 검사와 로컬 서버5개 검사는 통과했다. 게임에서 실제로 생성한 새 스냅샷과 최종 브라우저 표시는 C++ 빌드·실행 후 사용자 확인이 필요하다.
