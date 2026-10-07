# NPC 자율 행동 시스템 — 적용 기준

최신 사용자 제공 Advanced Autonomous NPC AI System 명세를 아래에 원문으로 반영한다.
이전 명세는 [이력](docs/history/NPC_AI_Behavior_System_Design_legacy.md)에 보존했다.
충돌 시 아래 최신 명세와 최신 사용자 요청을 우선한다. 원문 전체가 구현 완료되었다는 뜻은 아니다.

## 현재 코드 반영 범위 (2026-09-28)

- 기존 마을 20명/8직업, 실시간 RPG를 유지한다. 2명 최소 프로토타입은 기존 F6 구조 훈련으로 유지한다.
- 마을 NPC: 개인 지식·정보 출처·신뢰도·관측 시각, 5Hz 직접 인지, 2Hz 정보 교환/역할 입찰/Utility 판단.
- 관측 정보만으로 목표 위치를 정하고, 실제 치료·명중 순간에는 거리와 시야를 재검증한다. 5초 지난 정보는 폐기한다.
- 무전 가능한 경비·사냥꾼·의무관·구호 주민 또는 거리 6 이내 주민끼리 정보를 교환한다. 근거리 통신은 현재 거리만 검사한다.
- 직접 관측 정보만 1회 전달하며 원래 관측 시각을 보존한다. 연결된 후보 간에는 구조 담당을 중복 배정하지 않는다. 단절된 집단은 중복 구조 가능하다.
- 방향성 관계 3개(trust/respect/debt), 중요도·시간 기반 최대 24개 사건 기억, 구조받은 상대에 대한 기억/관계의 구조 점수 반영.
- 4개 Needs 노출, 기존 12개 성향 유지. 새 명세의 핵심 5개 중 riskTolerance도 이미 포함된다. F6 훈련의 기존 coverPreference는 유지한다.
- CommanderSavedMe / AllySavedMe / SuccessfulMission / FailedRescue 이벤트를 연결했다. CommanderIgnoredMe는 유형만 정의했으며 무시 판정 정책은 후속이다.
- 임계 체력 피해 시 즉시 재판단. 명령 수락·부상 시 수정·신뢰 부족 시 재질문. 단순 접근→치료/사격 실행이며 GOAP 탐색은 아니다.
- H로 회복약을 써서 대화 중 부상 주민 지원, J로 대화 패널의 AI 요약 전환, K로 무전 연결/단절 시험.
- JSONL v2: 후보 점수, 선택 이유, 작업, 구조 입찰·가산/감점, Needs, 지식 출처/신뢰도, 방향성 관계, 기억 기록.

## 후속 구현 (현재 미구현)

GOAP 플래너, 범용 Task/Event Bus/Blackboard 모듈, 감정·Mood 동역학, 부대 문화, 미션 종료 GA,
Storylet/Director, 영속 저장·불러오기 및 재현 리플레이는 후속 단계다.
현재 통신/입찰은 20명 대상 중앙 조정 함수이며 범용 분산 블랙보드가 아니다.
구조 역할 선정과 실제 행동은 별개여서 생존·명령이 우선되면 선정된 NPC도 구조를 보류할 수 있다.

상세 조작·제약은 [마을 프로토타입](docs/TOWN_NPC_PROTOTYPE.md)을 참조한다.
코드 정적 검토만 수행하며 C++ 빌드·게임 실행·테스트 실행은 사용자 확인 전이다.

---

# 사용자 제공 최신 명세 원문

# Advanced Autonomous NPC AI System
## Codex Implementation Specification

---

# 0. 시스템 목표

이 게임의 NPC, 특히 전술인형은 고정 스크립트대로만 움직이는 NPC가 아니다.

NPC는 다음 능력을 가져야 한다.

1. 현재 상황을 인식한다.
2. 자신이 알고 있는 정보와 모르는 정보를 구분한다.
3. 과거 사건을 기억한다.
4. 플레이어와 다른 NPC에 대한 관계를 형성한다.
5. 자신의 성격과 현재 감정 상태를 가진다.
6. 현재 가장 중요한 목표를 선택한다.
7. 목표를 달성하기 위한 행동 계획을 세운다.
8. 다른 NPC의 행동을 관찰하고 학습한다.
9. 성공/실패 경험에 따라 행동 전략이 장기적으로 변화한다.
10. 소대 전체에 고유한 행동 문화가 형성된다.
11. 플레이어가 명령할 수 없는 상황에서도 독립적으로 행동한다.

최종적으로 NPC는 다음과 같이 느껴져야 한다.

```text
초반:
"플레이어의 명령을 기다리는 NPC"

중반:
"플레이어와 동료에게서 배우는 NPC"

후반:
"스스로 상황을 판단하고 계획하는 NPC"

최종:
"플레이어가 없어도 자기 선택으로 행동하는 NPC"
```

---

# 1. 전체 AI Architecture

권장 구조:

```text
                   ┌────────────────────┐
                   │ Narrative Director │
                   └─────────┬──────────┘
                             │
                             ↓
              ┌─────────────────────────┐
              │       NPC Mind          │
              │                         │
              │ Personality             │
              │ Emotion / Mood          │
              │ Needs / Drives          │
              │ Memory                  │
              │ Knowledge / Belief      │
              │ Relationships           │
              └──────────┬──────────────┘
                         ↓
                    Utility AI
                    Goal Selection
                         ↓
                       GOAP
                    Plan Creation
                         ↓
               Behavior Tree / Tasks
                    Action Execution
                         ↓
                      World
                         ↓
                   Result / Event
                         ↓
           ┌─────────────┴─────────────┐
           ↓                           ↓
    Social Imitation            Strategy Evolution
    Short-term learning         Long-term learning
           ↓                           ↓
           └─────────────┬─────────────┘
                         ↓
                  Behavior Update
                         ↓
                   Squad Culture
```

소대 단위 공유 정보:

```text
                  Squad Blackboard
                         ↕
                 모든 Squad NPC
```

---

# 2. 구현 시스템 목록

전체 시스템을 다음 모듈로 분리한다.

```text
NPC AI
├── Perception System
├── Knowledge / Belief System
├── Memory System
├── Personality System
├── Emotion / Mood System
├── Relationship System
├── Needs / Drives System
├── Utility AI
├── GOAP Planner
├── Behavior / Task System
├── Command System
├── Autonomy System
├── Squad Blackboard
├── Role Bidding System
├── Social Imitation
├── Evolutionary Strategy
├── Squad Culture
├── Storylet System
├── Narrative Director
├── Save / Load
└── AI Debugger
```

각 모듈은 가능한 한 독립적으로 구현한다.

---

# 3. NPC 기본 구조

예시:

```cpp
class NPC
{
public:
    NPCStats stats;

    Personality personality;
    EmotionState emotion;
    Needs needs;

    BehaviorTraits traits;
    LearningProfile learning;

    KnowledgeBase knowledge;
    MemorySystem memory;

    RelationshipGraph relationships;

    Goal currentGoal;
    Plan currentPlan;

    BehaviorStrategy activeStrategy;

    CombatRole role;

    AIState currentState;
};
```

---

# 4. Behavior Traits

NPC의 장기 행동 성향.

```cpp
struct BehaviorTraits
{
    float aggression;

    float coverPreference;
    float rescuePriority;

    float commandObedience;
    float autonomy;

    float riskTolerance;

    float ammoConservation;

    float civilianPriority;

    float retreatPreference;

    float teamSupport;

    float objectivePriority;

    float selfPreservation;
};
```

범위:

```text
0.0 ~ 1.0
```

이 값은 고정되지 않는다.

다음 시스템에 의해 장기적으로 변한다.

```text
Experience
Social Imitation
Commander Feedback
Genetic Evolution
Story Event
```

---

# 5. Personality System

BehaviorTraits와 Personality는 분리한다.

BehaviorTraits:

```text
행동 습관
변할 수 있음
```

Personality:

```text
캐릭터의 근본적인 성격
쉽게 변하지 않음
```

예:

```cpp
struct Personality
{
    float empathy;
    float discipline;
    float impulsiveness;

    float curiosity;

    float courage;

    float independence;

    float loyalty;

    float personalityResistance;
};
```

PersonalityResistance가 높으면 외부 학습 영향이 작다.

---

# 6. Emotion / Mood System

Emotion과 Mood를 구분한다.

## Emotion

짧게 지속.

```cpp
struct EmotionState
{
    float fear;
    float anger;

    float relief;
    float grief;

    float confidence;

    float stress;
};
```

---

## Mood

Mission 여러 개 동안 지속 가능.

```cpp
struct MoodState
{
    float anxiety;

    float exhaustion;

    float optimism;

    float cynicism;

    float stability;
};
```

예:

```text
Mira Personality:
Empathy HIGH

현재 Mood:
Exhaustion HIGH

현재 Emotion:
Fear HIGH
```

평소라면 구조 행동을 선택하지만

현재 상태에서는 구조를 포기할 수도 있다.

NPC가 항상 동일한 선택을 하지 않게 한다.

---

# 7. Needs / Drives

NPC에게 현재 순간의 동기를 부여한다.

```cpp
struct Needs
{
    float survive;
    float protectAlly;

    float completeMission;

    float obeyCommander;

    float protectCivilian;

    float acquireAmmo;

    float reduceThreat;

    float maintainRelationship;
};
```

Needs는 상황에 따라 빠르게 변한다.

예:

```text
Bell Down

Mira:

protectAlly      = 0.95
completeMission  = 0.60
survive          = 0.55
```

다른 상황:

```text
Mira HP 10%
No Ammo

survive          = 0.95
protectAlly      = 0.50
```

---

# 8. Perception System

NPC는 게임 전체 World State를 직접 알면 안 된다.

NPC 자신이 인식한 정보만 사용할 수 있다.

```cpp
struct PerceptionData
{
    vector<EnemyInfo> enemies;

    vector<AllyInfo> allies;

    vector<CoverInfo> covers;

    vector<ObjectiveInfo> objectives;

    bool commanderConnected;

    bool underFire;

    bool suppressed;
};
```

갱신 권장:

```text
5 ~ 10 Hz
```

---

# 9. Knowledge / Belief System

중요.

실제 세계 상태와

NPC가 알고 있다고 믿는 상태를 분리한다.

```text
World Truth
≠
NPC Belief
```

예:

실제:

```text
Noah Alive
Location Sector B
```

Mira Knowledge:

```text
Noah Status = Unknown
```

Bell:

```text
Noah likely retreated
```

Leon:

```text
Noah probably dead
```

---

구조 예:

```cpp
struct Belief
{
    FactType type;

    EntityID subject;

    Variant value;

    float confidence;

    float timestamp;

    EntityID source;
};
```

---

# 10. Information Source

NPC는 정보 출처도 기억한다.

예:

```text
Saw Directly
Commander Told
Mira Told
Radio
Unknown Source
```

신뢰도가 다르다.

```cpp
float CalculateBeliefConfidence(
    InformationSource source,
    Relationship sourceRelationship);
```

이 때문에 NPC가 잘못 판단할 수 있다.

의도적인 오판 가능성이 있어야 한다.

---

# 11. Memory System

NPC는 중요한 사건을 기억한다.

단순 Relationship +/-로 끝내지 않는다.

```cpp
struct MemoryEvent
{
    MemoryType type;

    EntityID actor;
    EntityID target;

    MissionID mission;

    float timestamp;

    float tacticalValue;

    float emotionalImpact;

    EmotionTags emotion;

    vector<EntityID> participants;
};
```

예:

```text
Memory:

CommanderChoseServer

Location:
S09

Result:
Mira injured
Server saved

Emotion:
Abandonment 0.7
Fear 0.4
Respect 0.2
```

---

# 12. Memory Importance

모든 사건을 영원히 저장하지 않는다.

중요도 계산:

```text
Importance =
    EmotionalImpact
  × TacticalValue
  × RelationshipImpact
```

최근 기억 가중치:

```text
EffectiveMemory =
    Importance
  × RecencyWeight
```

---

# 13. Memory Recall

현재 상황과 비슷한 기억을 검색한다.

예:

현재 상황:

```text
Commander chooses mission asset over injured Doll
```

Mira Memory Search:

```text
S09 Server Incident
```

그러면 대사:

```text
"지난번에도 서버를 먼저 선택했죠."
```

행동에도 영향:

```text
Trust ↓
Rescue Self-Initiative ↑
```

---

# 14. Relationship Graph

플레이어-NPC 관계뿐 아니라 NPC-NPC 관계도 존재한다.

```cpp
struct Relationship
{
    float trust;

    float respect;

    float affection;

    float rivalry;

    float fear;

    float debt;

    float loyalty;
};
```

---

예:

```text
Mira → Bell

Trust     0.80
Respect   0.70
Affection 0.60
Debt      0.90
```

Bell이 과거 Mira를 구조했다면 Debt가 높을 수 있다.

---

# 15. Relationship Graph

```cpp
unordered_map<EntityID, Relationship>
relationships;
```

NPC마다 별도 Relationship Graph를 가진다.

A가 B를 좋아한다고 B도 A를 좋아할 필요는 없다.

```text
A → B
≠
B → A
```

---

# 16. Rumor / Information Spread

NPC가 직접 보지 않은 사건도 다른 NPC에게 전달받을 수 있다.

흐름:

```text
Event
↓
Mira observes
↓
Mira interprets
↓
Mira tells Bell
↓
Bell stores belief
```

중요:

같은 사건도 NPC마다 해석이 다르다.

예:

실제 사건:

```text
Commander ignored objective to save civilians.
```

Mira Interpretation:

```text
Commander protects people.
```

Bell Interpretation:

```text
Commander may ignore orders.
```

---

# 17. Utility AI

Utility AI 역할:

```text
"지금 무엇을 하고 싶은가?"
```

Goal을 선택한다.

---

Goal 종류:

```cpp
enum GoalType
{
    Survive,

    AttackEnemy,

    ProtectAlly,

    RescueAlly,

    CompleteObjective,

    ProtectCivilian,

    Retreat,

    AcquireAmmo,

    SupportSquad
};
```

---

# 18. Utility Score

예:

```cpp
float RescueGoalScore(NPC& npc, Context& ctx)
{
    float score = 0;

    score += npc.needs.protectAlly;

    score += npc.traits.rescuePriority;

    score += npc.traits.teamSupport;

    score += RelationshipTo(ctx.downedAlly).affection;

    score += RelationshipTo(ctx.downedAlly).debt;

    score -= ctx.dangerLevel *
             (1.0f - npc.traits.riskTolerance);

    return Normalize(score);
}
```

---

# 19. Utility AI Update Rate

매 Frame 계산 금지.

추천:

```text
Normal:
2 ~ 5 Hz

Emergency:
Event Driven
```

---

# 20. GOAP

Utility AI가 목표를 결정한다.

예:

```text
Goal:
Rescue Bell
```

GOAP은 방법을 결정한다.

---

Actions:

```text
SuppressEnemy
MoveToCover
ThrowSmoke
ApproachAlly
HealAlly
CarryAlly
MoveToExtraction
```

Planner 결과:

```text
Suppress Enemy
→ Throw Smoke
→ Move to Bell
→ Rescue Bell
→ Move to Cover
```

---

# 21. GOAP Action 구조

```cpp
struct GOAPAction
{
    vector<Condition> preconditions;

    vector<Effect> effects;

    float cost;

    ActionType action;
};
```

---

예:

```text
Action: RescueAlly

Preconditions:
NearDownedAlly = true
ThreatLevel < 0.7

Effects:
AllyRescued = true

Cost:
2.0
```

---

# 22. GOAP 사용 범위

GOAP은 모든 작은 행동에 사용하지 않는다.

사용:

```text
Rescue
Retreat
Flank
Hack
Secure Objective
Escape
```

단순 행동:

```text
Reload
Shoot
TakeCover
```

Behavior Tree 또는 Task System에서 처리.

---

# 23. Behavior Tree / Task Execution

GOAP:

```text
무엇을 어떤 순서로 할지 결정
```

Behavior Tree:

```text
실제로 어떻게 실행할지 관리
```

예:

```text
Task: MoveToCover

Check Path
↓
Reserve Cover
↓
Move
↓
Handle Obstacle
↓
Confirm Arrival
```

---

# 24. Autonomy System

기존 Relationship의 Autonomy와 연결.

```text
Autonomy Level 0
명령 대기

Level 1
생존 행동 자동

Level 2
지원 행동 자동

Level 3
전술적 판단 가능

Level 4
다른 NPC에게 행동 제안 가능
```

---

# 25. Player Command

플레이어 명령은 Utility Goal보다 높은 우선도를 가진다.

하지만 상황에 따라 NPC가 제안/수정/거부 가능.

명령 상태:

```text
Accepted
AcceptedWithModification
Questioned
Rejected
```

---

예:

```text
Commander:
"Bell, 위치 유지."

Bell detects flank.

High Autonomy Bell:

"측면 접근 확인.
위치 변경을 제안합니다."
```

---

# 26. Squad Blackboard

모든 NPC가 공유할 수 있는 Tactical Data.

```cpp
struct SquadBlackboard
{
    vector<EntityID> knownEnemies;

    vector<Vector3> threatZones;

    vector<EntityID> downedAllies;

    vector<Vector3> availableCovers;

    Objective currentObjective;

    Vector3 extractionPoint;

    float squadAmmo;

    float squadThreat;
};
```

---

# 27. Blackboard Information Ownership

모든 정보가 즉시 공유되면 안 된다.

공유 조건:

```text
Radio Connected
NPC Nearby
Direct Communication
Data Link Available
```

통신 두절 상황에서는 Blackboard 일부만 공유.

Great Retreat에서 중요.

---

# 28. Role Bidding System

소대 전체가 필요한 Tactical Role을 Blackboard에 등록한다.

예:

```text
NeedRescue = 0.9
NeedSuppress = 0.8
NeedScout = 0.5
```

각 NPC가 역할 점수를 계산한다.

---

예:

```text
Mira Rescue Bid 0.94
Bell Rescue Bid 0.30
Leon Rescue Bid 0.51
```

→ Mira 담당.

---

Suppress:

```text
Leon 0.91
Mira 0.22
Bell 0.45
```

→ Leon 담당.

---

# 29. Role Bid 계산

```cpp
float CalculateRoleBid(
    NPC& npc,
    TacticalRole role)
{
    return
        PersonalityMatch(role)
      + TraitMatch(role)
      + CurrentCondition(role)
      + RelationshipFactor(role)
      + EquipmentSuitability(role);
}
```

---

# 30. Social Imitation

NPC는 주변 NPC 행동을 관찰한다.

단기 학습 시스템.

```text
Observe
↓
Evaluate
↓
Imitation Score
↓
Trait Adjustment
```

---

Imitation Score:

```text
Success
× ObserverImitationRate
× SourceInfluence
× TacticalValue
× CommanderApproval
```

---

# 31. Source Influence

```text
Trust
Respect
Experience
Rank
RecentSuccess
```

---

# 32. Steady-State Genetic Algorithm

장기 행동 전략 진화.

유전 알고리즘 대상은 NPC가 아니다.

```text
Behavior Strategy
```

이다.

---

```cpp
struct BehaviorStrategy
{
    BehaviorTraits traits;

    float fitness;

    int usageCount;

    int successCount;
};
```

---

Population:

```text
10 ~ 20 Strategies
```

---

Mission 종료:

```text
Worst Strategies 제거
↓
Parent Selection
↓
Crossover
↓
Mutation
↓
Population 유지
```

---

# 33. GA 실행 시점

전투 중 실행 금지.

```text
Mission End only
```

---

# 34. Squad Culture

개별 NPC 행동이 누적되면서 소대 문화 생성.

예:

```text
CommandDependent
Autonomous
MutualProtection
MissionFirst
SurvivalFirst
Aggressive
Defensive
Balanced
```

---

Culture는 Stat Buff가 아니다.

Utility Weight에 영향을 준다.

예:

```text
MutualProtection:

Rescue Goal +25%
Support Goal +20%
```

---

# 35. 일상 AI

전투 밖에서도 NPC가 스스로 행동해야 한다.

가능 행동:

```text
Rest
Maintenance
Train
Talk
ReadMissionLog
CheckEquipment
VisitAnotherNPC
VisitCommander
StayAlone
```

---

# 36. Daily Schedule

NPC마다 일정이 있다.

```cpp
struct ScheduleBlock
{
    GameTime start;

    GameTime end;

    ActivityType preferredActivity;
};
```

단, 일정은 강제 스크립트가 아니라 Preference.

상황에 따라 변경 가능.

---

# 37. 기지에서 NPC-NPC 상호작용

조건 예:

```text
Mira Stress HIGH
Bell nearby
Mira-Bell Trust HIGH
```

Bell이 Mira에게 말을 걸 수 있음.

플레이어가 없어도 관계가 변한다.

---

# 38. Storylet System

작은 캐릭터 이벤트를 조건 기반으로 생성한다.

```cpp
struct Storylet
{
    vector<Condition> conditions;

    EventID event;

    float weight;

    bool repeatable;
};
```

---

예:

```text
Condition:

MiraBellTrust < 0.3
Mira recently saved Bell
BellRespectMira > 0.7

→ Event:
"말하지 못한 감사"
```

---

# 39. 다른 Storylet 예

```text
Noah Autonomy > 0.8
Commander ObediencePreference > 0.75

→ Event:
"명령과 판단"
```

---

# 40. Storylet은 Main Story를 변경하지 않는다

중요.

```text
Main Canon Event
고정

Storylet
플레이어 개인 서사
가변
```

---

# 41. Narrative Operations Director

Meta AI.

NPC를 직접 조종하지 않는다.

다음 정보를 관찰한다.

```text
Recent Difficulty
NPC Stress
NPC Relationship Conflict
Resource State
Recent Death
Storylet Frequency
Player Success Rate
```

---

# 42. Director 역할

예:

최근 3 Mission 매우 어려움.

```text
Enemy Density ↓
Recovery Event Probability ↑
```

Mira/Bell 갈등 높음.

```text
Mira-Bell Storylet Probability ↑
```

탄약 부족.

```text
Ammo Recovery Opportunity ↑
```

---

# 43. Director 제한

Director는 다음을 변경하면 안 된다.

```text
Main Story Outcome
Canon Character Fate
Fixed Historical Event
```

조정 가능:

```text
Encounter
Optional Event
Storylet
Minor Resource
NPC Interaction
```

---

# 44. LLM 사용 정책

핵심 행동 결정에 LLM 사용하지 않는다.

금지:

```text
Combat Decision
Pathfinding
Goal Selection
Mission Logic
```

선택적으로 사용 가능:

```text
Small Talk Variation
Memory Summary
Dynamic Flavor Text
Debrief Dialogue
```

Core AI는 항상 결정론적 시스템 기반.

---

# 45. NPC Decision Pipeline

NPC Update:

```text
1. Update Perception

2. Update Knowledge / Belief

3. Recall Relevant Memory

4. Update Emotion / Mood

5. Update Needs

6. Read Squad Blackboard

7. Process Player Command

8. Calculate Utility Goals

9. Select Goal

10. GOAP Planning

11. Execute Plan via Behavior Tree

12. Record Result

13. Generate Memory Event

14. Update Relationship

15. Broadcast Relevant Observation
```

---

# 46. Mission End Pipeline

```text
1. Collect Mission Reports

2. Update Important Memories

3. Update NPC Relationships

4. Update Commander Profile

5. Process Social Imitation

6. Calculate Strategy Fitness

7. Run Steady-State Evolution

8. Update BehaviorTraits

9. Calculate Squad Culture

10. Generate Storylet Candidates

11. Update Narrative Director State

12. Save AI State
```

---

# 47. 추천 Component 구조

```text
NPCEntity

├── NPCStatsComponent
├── PerceptionComponent
├── KnowledgeComponent
├── MemoryComponent
├── PersonalityComponent
├── EmotionComponent
├── NeedsComponent
├── RelationshipComponent
├── BehaviorTraitsComponent
├── LearningComponent
├── UtilityAIComponent
├── GOAPComponent
├── CommandComponent
├── AutonomyComponent
├── TaskExecutorComponent
└── AIDebugComponent
```

Squad:

```text
SquadEntity

├── BlackboardComponent
├── CultureComponent
├── RoleCoordinatorComponent
└── SquadMemoryComponent
```

Global:

```text
AIWorldSystem

├── StoryletManager
├── NarrativeDirector
└── AIEventBus
```

---

# 48. Event Bus

시스템 간 직접 의존성을 줄인다.

예:

```cpp
struct GameEvent
{
    EventType type;

    EntityID actor;

    EntityID target;

    Vector3 location;

    float timestamp;
};
```

Events:

```text
NPCDowned
NPCRescued
CivilianKilled
CommanderOrder
CommanderPraise
CommanderReprimand
ObjectiveCompleted
SquadMemberLost
EnemyDetected
CommunicationLost
```

Memory / Relationship / Imitation은 Event를 구독한다.

---

# 49. Save Data

반드시 저장:

## NPC

```text
Personality
Emotion/Mood
BehaviorTraits
LearningProfile
RelationshipGraph
ImportantMemory
Knowledge/Beliefs
StrategyPopulation
ActiveStrategy
Experience
```

## Squad

```text
Culture
SharedKnowledge
Doctrine
SharedHistory
```

## Player

```text
CommanderProfile
ImportantDecisionHistory
```

## Director

```text
RecentDifficulty
RecentStorylets
PacingState
```

---

# 50. Debug UI

NPC 선택 시 반드시 표시.

```text
NPC NAME

Current State
Current Goal
Current Plan
Current Action

Needs:
Survive
Protect Ally
Objective
Obey

Personality

Emotion
Mood

Behavior Traits

Knowledge

Important Memories

Relationships

Utility Scores

Current GOAP Plan

Active Strategy

Strategy Fitness

Observed Behaviors

Squad Culture

Role Bid Scores
```

---

# 51. AI Decision Log

반드시 구현.

예:

```text
[BELL]

Goal Candidates:

Survive        0.41
Attack         0.52
Rescue Mira    0.83
Objective      0.58

Selected:
Rescue Mira

Reason:

RescuePriority      +0.20
Relationship Debt   +0.18
Trust Mira          +0.10
Risk                -0.09
SquadCulture        +0.12

GOAP:

Suppress Enemy
→ Move Cover
→ Reach Mira
→ Rescue
```

이 로그가 없으면 AI 디버깅이 매우 어렵다.

---

# 52. 첫 프로토타입 범위

처음부터 전체 시스템 구현 금지.

## NPC

2명.

```text
Mira
Bell
```

---

## Traits

5개만.

```text
Aggression
RescuePriority
CommandObedience
Autonomy
RiskTolerance
```

---

## Needs

4개.

```text
Survive
ProtectAlly
CompleteObjective
ObeyCommander
```

---

## Relationship

3개.

```text
Trust
Respect
Debt
```

---

## Memory

5 Event Types.

```text
CommanderSavedMe
CommanderIgnoredMe
AllySavedMe
SuccessfulMission
FailedRescue
```

---

## Actions

```text
Attack
TakeCover
Rescue
Retreat
Objective
```

---

# 53. Prototype Phase 1

구현:

```text
Perception
Utility AI
Command
```

목표:

NPC가 상황에 따라 다른 행동 선택.

---

# 54. Prototype Phase 2

추가:

```text
Memory
Relationship
```

테스트:

Mira가 Bell에게 구조받은 후

다음 Mission에서 Bell 구조 우선도가 증가하는지 확인.

---

# 55. Prototype Phase 3

추가:

```text
Squad Blackboard
Role Bidding
```

테스트:

부상 NPC 발생.

가장 적합한 NPC 하나만 구조에 나서는지 확인.

---

# 56. Prototype Phase 4

추가:

```text
Social Imitation
```

Bell이 Mira의 성공 구조 행동을 관찰.

여러 Mission 후 구조 성향 변화 확인.

---

# 57. Prototype Phase 5

추가:

```text
Steady-State GA
```

10 Strategy Population.

Mission 종료 후:

```text
Worst 2 제거
2 Child 생성
```

---

# 58. Prototype Phase 6

추가:

```text
GOAP
```

Goal:

```text
Rescue Ally
```

가능 Planning:

```text
Suppress
→ Move
→ Rescue
```

---

# 59. Prototype Phase 7

추가:

```text
Emotion
Mood
Storylet
```

전투 스트레스가 행동과 이벤트에 영향을 주는지 확인.

---

# 60. Prototype Phase 8

추가:

```text
Narrative Director
```

단, Director는 마지막에 구현.

Core AI가 안정된 이후 작업.

---

# 61. 성공 기준

같은 초기 NPC로 플레이한다.

## Player A

반복적으로:

```text
Rescue Praise
Autonomy Support
```

20 Mission 후:

```text
High Rescue
High Team Support
Independent Action
```

---

## Player B

반복적으로:

```text
Objective Praise
Unauthorized Action Reprimand
```

20 Mission 후:

```text
High Command Obedience
High Objective Priority
Low Unauthorized Rescue
```

두 AI의 행동 차이가 **숫자가 아니라 실제 화면에서 보일 정도로 달라야 한다.**

---

# 62. 추가 성공 기준

NPC가 다음 행동을 보여야 한다.

```text
과거 사건 언급

NPC-NPC 관계 변화

잘못된 정보 기반 오판

스스로 Tactical Suggestion

타 NPC 행동 모방

플레이어 없이 역할 분담

기지에서 독립적 활동

조건 기반 개인 Storylet 발생
```

---

# 63. 구현 우선순위

실제 중요도 기준:

```text
1. Utility AI

2. Memory

3. Relationship Graph

4. Knowledge / Belief

5. Squad Blackboard

6. Role Bidding

7. Social Imitation

8. GOAP

9. Emotion / Mood

10. Genetic Evolution

11. Storylet

12. Narrative Director
```

중요:

처음부터 Genetic Algorithm을 우선 구현하지 않는다.

플레이어 입장에서 NPC가 살아 있는 것처럼 느껴지는 데 가장 중요한 것은 먼저:

```text
Memory
Relationship
Knowledge
```

이다.

---

# 64. AI 설계 최종 원칙

NPC는 다음 식으로 행동해야 한다.

```text
Personality
+
Current Emotion
+
Current Needs
+
Known Information
+
Past Memory
+
Relationship
+
Player Command
+
Squad Culture
+
Learned Strategy
```

↓

```text
Goal
```

↓

```text
Plan
```

↓

```text
Action
```

↓

```text
Result
```

↓

```text
Memory / Learning
```

---

# 65. 게임의 핵심 AI 경험

이 시스템의 목적은 NPC를 완벽하게 만드는 것이 아니다.

플레이어가 다음 경험을 하게 만드는 것이다.

```text
"얘가 내 명령을 따랐다."

↓

"얘가 내가 어떻게 행동하는지 기억한다."

↓

"얘가 다른 인형과 관계를 만든다."

↓

"얘가 내가 없는 곳에서도 행동한다."

↓

"얘가 나와 다른 판단을 한다."

↓

"얘가 스스로 선택한다."
```

최종적으로 NPC는

```text
Player-owned Unit
```

이 아니라

```text
Autonomous Character
```

처럼 느껴져야 한다.