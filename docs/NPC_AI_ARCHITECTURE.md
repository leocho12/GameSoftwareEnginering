# NPC AI Architecture

작성: 2026-10-01 · 상태: 검토용 설계 / 구현 전

## 적용 기준

최신 사용자 첨부 `NPC AI / World Simulation 구현 지시`를 기준으로 작성했다. 이번 산출물은 구조 분석과 설계 문서다. 코드·프로젝트 파일·기존 사용자 변경은 수정하지 않는다. 이전 [월드 구현 계획](AUTONOMOUS_NPC_WORLD_IMPLEMENTATION_PLAN.md)은 배경 자료이며, BDI/HTN 추가와 확장 순서는 본 문서 및 [실행 계획](NPC_AI_IMPLEMENTATION_PLAN.md)이 우선한다.

## 1. 현재 저장소와 실행 구조

- `BaseProject_SE/SimpleGame.sln`: Visual Studio C++17 / OpenGL / GLEW / FreeGLUT 프로젝트.
- `BaseProject_SE/SimpleGame/`: 게임·렌더링·NPC 소스와 프로젝트 설정. 소스 파일은 vcxproj에 개별 등록한다.
- `BaseProject_SE/SimpleGame/Assets/`: 런타임 리소스. `AssetsSource/`와 `tools/`는 추출·가공 원본과 도구다.
- `docs/`: 설계·세계관·리소스 문서. `tests/`: 독립 C++ 검사 소스. `.vs` 및 빌드 디렉터리는 구현 대상으로 다루지 않는다.
- `SimpleGame.cpp`의 입력/타이머 → `RpgGame::Update/Step` → 이동·전투·NPC 갱신 → `Renderer` 표시 구조다.
- `TutorialGame`, `TowerDefense`는 과거 구조이며 현재 등록된 실행 소스로 재사용하지 않는다.
- 기존 미커밋 NPC 작업까지 포함한 현재 작업 트리를 분석했다. 구현 존재와 빌드·실행 검증 여부는 별개다.

## 2. 현재 NPC 시스템

| 구성 | 현재 책임 | 한계 |
|---|---|---|
| `TownNpc.h` | 직업 8종, 성향 12개, 위치·체력·탄약·경로·행동과 NpcMind | 개인 영속 상태와 화면 근처 실행 상태 혼합 |
| `TownNpc.cpp` | 주민 20명 생성, 업무/순찰/공격/후퇴/도움/휴식 점수, 이동·전투·거래 | RpgGame 멤버 함수에 집중. Utility가 목표보다 즉시 행동을 선택 |
| `NpcMind.h` | Needs 4개, 출처·신뢰도 있는 믿음, 방향성 관계 3축, 중요 기억 최대 24개 | 모든 믿음 최대 5초, 장기 Desire/Intention·사건 ID 없음 |
| `TownKnowledge.cpp` | 인지 5Hz, 통신 및 구조 입찰 2Hz | 중앙 함수의 제한적 전달/담당 선정. 범용 Blackboard/Contract-Net 아님 |
| `DollBehavior.*` | F6 독립 2명 구조 실험, 고정 성향·점수·단계 실행 | 훈련 중 본 게임 갱신 중단. 영속 세계와 분리 |
| `RendererTown.cpp` | 주민 이름, 고정 거래 선택, 일부 AI 점수 요약 | BDI·계획·원격 사건 전체 디버거 아님 |

소규모 구조 성공 관찰에 따른 성향 변화는 존재한다. 완성된 Social Imitation, BDI, GOAP, HTN, 감정/Mood, 유전 진화, 부대 문화는 구현되어 있다고 가정하지 않는다.

## 3. 현재 Combat 시스템

- `RpgGame::Attack`은 마우스 방향, 사거리·횡방향 폭·시야선으로 대상을 찾아 즉시 피해를 준다. `shots`는 수명이 짧은 표시 데이터이며 탄도 시뮬레이션이 아니다.
- 플레이어가 몹을 처치하면 경험치·드롭·8초 재생성을 처리한다. 적대 NPC는 마을 밖에서 공격할 수 있다.
- `RpgGame::Step`의 적은 플레이어를 추적하고 근거리에서 재사용 대기시간과 시야를 검사해 피해를 준다. 멀어진 적 슬롯은 플레이어 주변으로 재배치된다.
- 플레이어 주변 32×32 BFS 거리장과 직접 이동 가능 여부 검사가 있다. `MoveNpc`는 별도 40×40 로컬 BFS와 연속 좌표 이동을 사용한다.
- 주민은 탄약을 사용해 사격하고 부상 동료를 치료한다. 외곽에서 몹과 밀착하면 반격 피해를 받는다. NPC 체력 0은 현재 30초 뒤 거점 회복 규칙이다.
- 피해·치료·드롭·관계 변화가 여러 함수에서 직접 적용된다. 통합 CombatSystem이나 공통 결과 이벤트 경로는 없다.

재사용: Fits/Walkable/LineOfSight, 연속 이동, BFS, 사격·피해 규칙. 경로 계산은 NavigationAdapter 뒤로 감싸며 첫 버전에서 A*로 교체하지 않는다. 새 AI의 목표 결정과 경로 탐색을 분리하는 것만으로 현재 요구에 충분하다.

## 4. 현재 Event / Save 시스템

**Event:** `Notify`는 일시적 화면 메시지다. `LogNpc`, `DollBehaviorPrototype::WriteEvent`는 JSONL 진단 로그다. NpcMemoryEvent는 개인 기억이다. 이들은 수명·기한·참여자·효과를 관리하는 WorldEvent 또는 구독형 EventBus가 아니다. 현재 일반 RPG 실행 경로에는 범용 WorldEvent/Quest/Storylet 시스템이 없다.

**Save:** 현재 게임/NPC 상태의 저장·불러오기 경로는 없다. Reset으로 다시 생성하며 JSONL은 회전·삭제될 수 있는 진단 기록이다. 캐릭터 JSON/manifest는 리소스 메타데이터이고 세이브가 아니다. seed만으로 관계·기억·선택의 결과를 복원할 수 없다.

## 5. 재사용과 충돌 해결

| 재사용 대상 | 충돌/위험 | 변경 제안 |
|---|---|---|
| NpcMind의 믿음 | 단기 좌표와 장기 사건 정보를 같은 5초 수명으로 취급 | TacticalFact/EventBelief 분리, 명제별 만료/신뢰 규칙 |
| 성향·기억·방향성 관계 | trust/respect 중복, 문자열 기억과 구조화 기억 병존 | 관계의 단일 소유자, 표현 이력과 판단용 기억 분리 |
| NPC 벡터 | ID를 배열 인덱스로 사용 | 안정 ActorId 등록소, 슬롯 재사용 시 새 ID/세대 |
| RpgGame::time | float 초가 전투·애니메이션에 사용됨 | 정수 GameClock 별도, 전투 속도와 달력 배율 분리 |
| 기존 행동 Utility | 장기 목표와 즉시 동작을 같은 선택기로 확장할 위험 | 상위 BDI/GoalEvaluator와 하위 Task 실행 분리 |
| 치료/피해 규칙 | 체력 0 자동 회복이 실종·사망 결과를 취소 | Alive/Injured/Missing/Dead 상태 및 회복 정책 구분 |
| 로컬 적 풀 | 화면 밖 원정 전력을 플레이어 중심 재생성 풀에 의존 | 원정 위험·참여자·자원은 사건 데이터로 관리 |
| 렌더러의 직접 벡터 참조 | 화면 밖 부재를 객체 삭제로 취급 | 영속 Actor와 로컬 표현 분리, 원정 중 비표시 |
| 직접 거래·관계 변경 | 해결 중복·원인 추적 누락 | 사건 효과의 단일 커밋 경로로 점진 이관 |

현재 소스에서 공통 Actor/SceneGraph 클래스는 확인되지 않는다. `Renderer::Actors`는 렌더 함수다. 향후 화면 Actor는 영속 ActorId를 참조하며 SceneGraph의 컬링이 세계 인물 삭제나 월드 시간 정지를 유발하지 않게 한다. 이번 AI 설계는 SceneGraph 전면 구현에 의존하지 않는다.

## 6. 목표 Architecture와 책임

기본 흐름: WorldEvent → Perception/Belief → BDI Desire → Utility → Intention → Planner → Task → ActionResult → WorldResult → Memory/Relationship → 학습.

| 컴포넌트 / 제안 파일 | 책임 | 금지할 책임 | 단계 |
|---|---|---|---|
| `WorldActorRegistry`, `WorldTypes.h` | 안정 ID, 인물·위치·소속·생명 상태 조회 | 렌더 객체가 영속 정체성 소유 | 최초 |
| `NpcProfile`, `NpcProfile.h` | 성격·가치·스킬·행동 성향 | 이름 검사로 행동 강제 | 최초 |
| `NpcBrain`, `NpcBrain.h/.cpp` | Belief 참조, Desire 목록, 현재 Intention와 commitment 보관 | 경로/피해 직접 실행 | 최초 |
| `BeliefStore`, `NpcKnowledge.h/.cpp` | 출처·시각·신뢰·모순·Unknown 관리 | 월드 진실 자동 동기화 | 최초 |
| `GoalEvaluator`, `NpcGoals.h/.cpp` | 후보 목표의 기여점수 산출 및 선택 제안 | 이동 순서/계획 생성 | 최초 |
| `EpisodicMemory`, `NpcMemory.h/.cpp` | 구조화 기억, 중요도/감정/시간 감쇠, 영구 기억 | 일반 로그 전체 무제한 보존 | 최초 최소형 |
| `MemoryRecall` | 태그 가중 유사도 검색, 상위 K개 회상과 판단 기여 | 기억에 없는 월드 진실 검색 | 최초 최소형 |
| `RelationshipRules` | 관계 7축의 방향성 갱신 | 반대 관계 자동 복사 | 최초 |
| `TaskRunner`, `NpcTasks.h/.cpp` | Running/Success/Failure/Cancelled, 도착/재시도/중단 처리 | 목표 선택·장기 전략 | 최초 |
| `NavigationAdapter`, `CombatAdapter` | 기존 이동/전투 호출과 결과 반환 | 독립 목표 선택 | 최초 |
| `GameClock`, `WorldSimulation` | 달력, 전체 갱신 순서·시간 예산 | 거대한 단일 판단 함수 | 최초 |
| `WorldEventSystem`, `EventResolver` | 사건 수명·참여·기한, 결과 및 효과의 단일 적용 | NPC에게 모든 사건 정보 공개 | 최초 |
| `DiscreteEventScheduler` | 다음 시각 순 예약, 원격 출발/도착/탐색/귀환 | 화면 밖 매 프레임 전투 | 최초 |
| `SimulationEventBus` | 타입 있는 커밋 결과 알림, 지연 큐 | 전역 mutable 상태 전달 | 최초 |
| `EventJournal`, `WorldState` | 중요한 확정 사건 이력과 그 결과 상태 | 회전 진단 파일을 세이브로 사용 | 최초 최소형 |
| `DialogueSystem`, `ConditionEvaluator` | 직접 접근 대화·조건·거절/중단·선택 재검사 | NPC가 모르는 결과 발설 | 최초 |
| `QuestView`, `PlayerWorldLog` | 알려진 사건의 참여/보고 UI | 플레이어 수락 전 사건 정지 | 최초 |
| `AiDebugSnapshot`, `EventDebugSnapshot` | 판단/계획/기억/출처의 읽기 전용 진단 | 디버그 진실을 실제 AI에 주입 | 최초 |
| `SquadBlackboard`, `RoleCoordinator` | 통신 가능한 공유 정보, 입찰·예약·계약 만료 | 모든 NPC 자동 공유 | 후속 1·2 |
| `GoapPlanner` | 전제조건/효과/비용으로 단기 전술 계획 | 재장전 같은 단일 동작에 검색 남용 | 후속 3 |
| `HtnPlanner` | 조건별 방법으로 장기 임무 분해 | Utility와 독립적으로 최상위 목표 변경 | 후속 4 |
| `InfluenceMap` | 알려진 위협·지원·엄폐·탈출 안전 평가 | 숨은 적 진실로 AI 맵 생성 | 후속 5 |
| `StoryletSystem`, `NpcSchedule`, `PersonalStory` | 조건부 개인 사건·일과 후보·서사 상태 | 스케줄이 긴급 목표 강제 덮어쓰기 | 후속 6 |
| `SocialImitationSystem` | 관측 성공/관계/저항 기반 작은 성향 변화 | 보지 못한 행동 학습 | 후속 7 |
| `StrategyEvolutionSystem` | 임무 종료 후 전략 population 교체 | NPC 정체성 진화, 전투 중 GA | 후속 8 |
| `AiLodScheduler`, 지역/세력 정책 | 계층별 예산·지역/세력 선택 | 렌더 가시성만으로 모든 사고 중단 | 후속 9~11 |

Emotion/Mood와 SquadCulture는 NpcMind 또는 소대 상태의 별도 입력 컴포넌트로 예약한다. 첫 프로토타입에서는 미구현 항의 점수를 0으로 명시하고 임의 값을 구현된 감정처럼 표시하지 않는다.

BDI는 세 데이터 영역과 의도 유지 규칙이다. Utility가 Desire 점수를 산출하면 Brain이 Intention을 확정한다. hysteresis·최소 유지 시간은 목표 흔들림을 줄이되 사망·대상 부재·긴급 피해·계획 불가능 시 즉시 재평가한다. 전환 이유와 점수 기여를 기록한다.

GOAP과 HTN은 매번 병렬 실행하는 두 AI가 아니다. 장기 Intention은 HTN으로 분해하고 필요할 때 특정 전술 하위 작업만 GOAP에 맡긴다. TaskRunner가 실행과 실패를 보고한다. 첫 프로토타입에는 검색 없는 데이터 정의 TaskSequence를 사용하며 이를 HTN/GOAP으로 표시하지 않는다. 별도 Behavior Tree 런타임은 TaskRunner만으로 부족할 때 도입한다.

## 7. Component Dependency와 상태 소유권

```text
SimpleGame (입력) → RpgGame (조립/로컬 플레이)
                         ↓
                   WorldSimulation
  ┌──────────────────────┼──────────────────────┐
GameClock/Scheduler   WorldActorRegistry     WorldEventSystem
                         │                       │
                  NpcProfile + NpcMind       EventResolver
                         ↓                       ↓
BeliefView → NpcBrain ↔ GoalEvaluator       EventJournal → WorldState
                         ↓                       ↓
                PlanProvider/TaskRunner ← 커밋 알림(EventBus)
                         ↓                       ↓
             Navigation/Combat Adapter   Memory/Relationship/Dialogue

Renderer ← Dialogue/Debug/PlayerKnowledge의 읽기 전용 Snapshot
```

- WorldSimulation이 시스템 수명을 소유한다. 각 시스템은 필요한 인터페이스만 참조한다. 코어는 OpenGL/Windows/Renderer를 포함하지 않는다.
- ActorRegistry 안의 NPC 데이터가 개인 상태를 단독 소유한다. RelationshipRules/MemoryRecall은 규칙 서비스이며 별도 관계 복사본을 만들지 않는다.
- Brain/Planner는 ActorKnowledgeView와 자신의 자원·스킬을 읽는다. WorldState 전체 접근은 Resolver와 감지 경계로 제한한다. InfluenceMap도 NPC/소대가 알게 된 사실을 입력받는다.
- 요청(Command/Intent), 진행 사건(WorldEvent), 커밋된 사실(DomainEvent), 진단 로그를 다른 타입으로 구분한다.
- 입력 수집→인지→목표 선택→작업 실행→결과 검증/커밋→알림 순서. 알림 처리 중 생성된 새 요청은 다음 단계에서 처리하여 재귀 폭주를 막는다.
- 같은 구조/피해가 근거리와 원격 양쪽에서 적용되지 않게 실행 권한을 한 곳에 둔다. 초기 원정은 출구~귀환까지 원격 구간으로 고정한다.

## 8. 데이터·기억·Event Sourcing

제안 데이터 위치: `Assets/AI/npc_profiles.json`, `goals.json`, `task_sequences.json`, `world_events.json`, `dialogues.json`. 후속으로 goap_actions/htn_methods/storylets를 추가한다. 이는 예정 파일이며 이번에 만들지 않는다.

프로젝트에 일반 AI 데이터 로더가 있다고 가정하지 않는다. JSON 파서는 구현 단계에서 의존성·라이선스·x64 빌드 연결을 확인해 하나를 선택한다. 수작업 문자열 검색으로 JSON을 해석하지 않는다. 로더는 버전·필수 필드·ID 중복·참조·범위·허용 조건/효과를 검증하고 파일/항목 위치를 오류로 표시한다. 렌더/작업 디렉터리에 의존하지 않게 실행 파일 기준 경로와 프로젝트 복사 규칙을 정한다.

데이터는 성향·목표 가중치·대사·조건·단계 순서를 정의한다. 코드에는 Move/Heal/ReserveResource 같은 검증 가능한 기본 동작만 둔다. 임의 코드 실행 스크립트를 초기부터 도입하지 않는다.

CBR 첫 버전은 명시적 상황 태그의 가중 일치도와 기억 강도로 상위 K개를 찾는다. 누락 정보는 불일치와 구분하고 기여값을 제한한다. 회상한 MemoryId·매칭 태그·유사도·점수 변화를 기록한다. 감정 값이 0이라는 이유만으로 유의미한 비감정 기억이 전부 사라지지 않게 기본 강도와 감정 가중치를 분리한다. 영구 서사 기억은 일반 기억 용량에서 별도 관리하고 무제한 중복 생성하지 않는다.

Event Sourcing은 중요한 월드 변화에 한정한다. 각 커밋에는 sequence/eventId/causationId/actorId/gameTime/schemaVersion/확정 effects를 저장한다. 먼저 검증된 사건을 순서대로 기록하고 그 기록을 reducer가 상태에 반영한다. `(eventId, effectId)` 적용 이력으로 중복을 막는다. 재생 중에는 이미 확정된 난수 결과를 적용하며 AI·대사·알림을 다시 실행하지 않는다. 스냅샷에는 마지막 적용 sequence와 시계·ID·RNG 상태를 포함한다.

첫 프로토타입은 메모리 Journal과 결정적 replay 검사까지, 디스크 저장·원자적 스냅샷·버전 이관·손상 복구는 별도 저장 단계로 구분한다. 저장 전에는 종료 후 기록이 유지된다고 표시하지 않는다. NPC Memory/대사는 전체 Journal을 직접 읽지 않고 자신이 경험·수신한 사실만 사용한다.

## 9. World State와 안전한 확장 경계

Canon은 읽기 전용, Regional/Personal은 허용 효과로 변경한다. 첫 사건은 미라·벨이라는 시험 인물과 지역 실종 사건이며 원작 사건의 시점·결과를 확정하지 않는다. 원작 세계관 적용 기준은 `WORLD_AND_STORY.md`를 유지한다.

중요한 NPC 선택은 점수·제약·관계·기억으로 결정한다. 결과의 불확실성이 필요하면 별도 seed의 제한적 판정을 사용하고 모든 입력/추출값을 남긴다. 선택 tie는 안정 ID 등 결정적 규칙으로 해결한다.

디버그에는 Belief/Desire/Intention, 후보와 기여점수, 현재 Task, 사건/기억/관계/정보 출처/최근 결정이 필요하다. 후속 GOAP/HTN/Emotion/전략/Blackboard 항목은 미구현이라고 표시한다. 한국어 요약과 안정된 영문 로그 키를 함께 사용한다.
