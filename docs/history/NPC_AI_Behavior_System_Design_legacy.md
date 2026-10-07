# NPC AI / Behavior System Design Specification

## 0. 문서 목적

이 문서는 게임 내 전술인형(NPC)의 행동 패턴, 전투 AI, 자율 판단, 사회적 모방, 장기 행동 진화 시스템을 구현하기 위한 기술 설계 명세다.

핵심 목표:

- NPC가 고정된 스크립트만 반복하지 않는다.
- 플레이어의 지휘 방식에 따라 NPC 행동이 변한다.
- NPC가 다른 NPC의 행동을 관찰하고 일부를 모방한다.
- 장기적으로 소대 전체에 고유한 행동 문화(Squad Culture)가 형성된다.
- 플레이어가 직접 명령할 수 없는 상황에서도 NPC가 자신의 경험과 학습 결과를 이용해 행동한다.
- 공격적, 방어적, 구조 중심, 임무 우선 등 서로 다른 행동 문화가 모두 장단점을 가진다.

---

# 1. 전체 AI 구조

```text
Perception
    ↓
World State / Tactical Context
    ↓
Decision
    ↓
Action Selection
    ↓
Action Execution
    ↓
Result Evaluation
    ↓
Memory / Experience
    ↓
Social Imitation
    ↓
Strategy Evolution
    ↓
Squad Culture Update
```

권장 구현 계층:

```text
Layer 1 : Perception
Layer 2 : Tactical Context
Layer 3 : Utility Decision
Layer 4 : Action / Task
Layer 5 : Relationship & Personality
Layer 6 : Learning / Memory
Layer 7 : Social Imitation
Layer 8 : Evolutionary Strategy
Layer 9 : Squad Culture
```

---

# 2. 핵심 설계 철학

## 2.1 NPC는 완벽하게 행동하지 않는다

NPC는 최적해를 계산하는 봇이 아니다. 같은 상황에서도 다음 요소에 따라 다른 판단을 내려야 한다.

- 성격
- 경험
- 플레이어와의 관계
- 현재 전술 상황
- 소대 문화
- 학습된 행동 전략
- 주변 NPC의 행동
- 최근 성공/실패 경험

## 2.2 플레이어 명령과 NPC 자율 행동을 분리한다

### Commanded Action

플레이어가 직접 내린 명령.

```text
Move
Attack
Defend
Hold
Rescue
Retreat
Hack
```

### Autonomous Action

NPC가 상황을 보고 스스로 선택하는 행동.

```text
TakeCover
Reload
HelpAlly
ChangePosition
SuppressEnemy
RetreatFromDanger
ProtectCivilian
```

자율성이 높을수록 더 중요한 행동까지 스스로 선택할 수 있다.

---

# 3. NPC 기본 데이터

```cpp
struct NPCStats
{
    float hp;
    float maxHp;
    int ammo;
    int maxAmmo;
    float accuracy;
    float movementSpeed;
    float suppression;
    float morale;
};
```

---

# 4. 행동 성향 BehaviorTraits

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

권장 범위:

```text
0.0 ~ 1.0
```

Trait 의미:

- `aggression`: 공격 적극성
- `coverPreference`: 엄폐 선호
- `rescuePriority`: 부상 아군 구조 우선도
- `commandObedience`: 명령 순응도
- `autonomy`: 독립 판단 성향
- `riskTolerance`: 위험 감수 성향
- `ammoConservation`: 탄약 절약 성향
- `civilianPriority`: 민간인 보호 우선도
- `retreatPreference`: 후퇴 선호도
- `teamSupport`: 동료 지원 성향
- `objectivePriority`: 임무 목표 우선도
- `selfPreservation`: 자기 생존 우선도

---

# 5. Personality Base

BehaviorTraits는 변하지만 캐릭터의 원래 성격이 완전히 사라지면 안 된다.

```cpp
struct PersonalityBase
{
    BehaviorTraits defaultTraits;
    float personalityResistance;
};
```

`personalityResistance`가 높을수록 외부 학습에 덜 흔들린다.

---

# 6. Relationship System

```cpp
struct Relationship
{
    float trust;
    float respect;
    float bond;
    float autonomySupport;
};
```

- `trust`: 플레이어 판단을 믿는 정도
- `respect`: 지휘관으로 인정하는 정도
- `bond`: 개인적 애착
- `autonomySupport`: 자신의 자율성을 존중받는다고 느끼는 정도

관계는 행동 점수와 명령 반응에 영향을 준다.

---

# 7. Learning Profile

NPC마다 배우는 방식이 다르다.

```cpp
struct LearningProfile
{
    float imitationRate;
    float selfLearningRate;
    float mutationRate;
    float authorityInfluence;
    float peerInfluence;
    float successInfluence;
    float personalityResistance;
};
```

예:

```text
Mira  : imitationRate HIGH / peerInfluence HIGH
Bell  : authorityInfluence HIGH / successInfluence HIGH
Noah  : imitationRate LOW / selfLearningRate HIGH
Leon  : successInfluence VERY HIGH
```

---

# 8. Perception System

NPC는 실제로 감지한 정보만 사용한다.

```cpp
struct PerceptionData
{
    std::vector<EnemyInfo> visibleEnemies;
    std::vector<AllyInfo> visibleAllies;
    std::vector<CoverInfo> nearbyCovers;
    std::vector<ObjectiveInfo> knownObjectives;

    bool commanderConnected;
    bool underFire;
    bool suppressed;
};
```

Perception 항목 예:

```text
EnemyDistance
EnemyCount
AllyHP
CivilianNearby
NearestCover
EscapeRoute
ObjectiveDistance
AmmoState
```

---

# 9. Tactical Context

Perception을 행동 결정용 상황 정보로 변환한다.

```cpp
struct TacticalContext
{
    float dangerLevel;
    float allyDanger;
    float selfDanger;

    bool allyDown;
    bool objectiveCritical;
    bool civilianThreatened;
    bool safeCoverAvailable;
    bool retreatRouteAvailable;
    bool commanderConnected;
};
```

---

# 10. 상위 AI 상태와 Utility AI

권장 구조:

```text
FSM
 ↓
Utility AI
 ↓
Action
```

상위 상태:

```cpp
enum class AIState
{
    Idle,
    Alert,
    Combat,
    Retreat,
    Disabled,
    FollowingCommand
};
```

State 내부에서 Utility AI가 실제 행동을 선택한다.

---

# 11. Action 목록

```cpp
enum class ActionType
{
    Attack,
    Move,
    TakeCover,
    Reload,
    Suppress,
    RescueAlly,
    ProtectCivilian,
    AdvanceObjective,
    Retreat,
    HoldPosition,
    Flank,
    Hack,
    Heal,
    Wait
};
```

---

# 12. Utility AI

각 후보 행동의 점수를 계산하고 가장 높은 행동을 선택한다.

예:

```text
Attack Score
Cover Score
Rescue Score
Retreat Score
Objective Score
Support Score
```

## Rescue 예시

```cpp
float CalculateRescueScore(const NPC& npc, const TacticalContext& ctx)
{
    if (!ctx.allyDown)
        return 0.0f;

    float score = 0.0f;
    score += npc.traits.rescuePriority * 0.4f;
    score += npc.traits.teamSupport * 0.2f;
    score += npc.relationship.autonomySupport * 0.1f;
    score -= ctx.selfDanger * 0.2f;
    score += npc.traits.riskTolerance * 0.1f;
    return score;
}
```

## Attack 예시

```cpp
float CalculateAttackScore(const NPC& npc, const TacticalContext& ctx)
{
    float score = 0.0f;
    score += npc.traits.aggression * 0.35f;
    score += npc.traits.objectivePriority * 0.15f;
    score -= ctx.selfDanger * 0.15f;
    score += npc.stats.ammo > 0 ? 0.1f : -1.0f;
    return score;
}
```

## Retreat 예시

```cpp
float CalculateRetreatScore(const NPC& npc, const TacticalContext& ctx)
{
    float score = 0.0f;
    score += npc.traits.retreatPreference * 0.35f;
    score += npc.traits.selfPreservation * 0.25f;
    score += ctx.selfDanger * 0.3f;

    if (!ctx.retreatRouteAvailable)
        score *= 0.2f;

    return score;
}
```

---

# 13. Command System

```cpp
struct Command
{
    CommandType type;
    Vector3 targetPosition;
    EntityID targetEntity;
    float priority;
};
```

플레이어 명령은 기본적으로 Utility AI보다 높은 우선순위를 가진다.

단, 관계와 자율성에 따라 수행 방식이 달라진다.

```text
CommandCompliance ≈ commandObedience × trust × respect
```

랜덤으로 명령을 무시하게 만드는 것은 피한다.

권장 동작:

```text
높은 순응도    → 명령을 정확히 수행
중간 순응도    → 명령 수행 + 상황 판단
낮은 순응도    → 위험 시 다른 행동 제안
매우 낮은 순응 → 특정 상황에서 명령 거부 가능
```

---

# 14. Autonomy Level

```text
LEVEL 0 : Direct Command Only
LEVEL 1 : Basic Survival
LEVEL 2 : Tactical Support
LEVEL 3 : Independent Tactical Decision
LEVEL 4 : Squad Leadership
```

### Level 0

중요 행동은 직접 명령 필요.

### Level 1

자동 가능:

```text
Reload
TakeCover
AvoidGrenade
```

### Level 2

```text
SupportAlly
SuppressEnemy
ChangeCover
```

### Level 3

```text
RescueAlly
Flank
Retreat
Hack
ChangeObjectiveRoute
```

### Level 4

다른 NPC나 플레이어에게 Tactical Suggestion 가능.

---

# 15. Tactical Suggestion

자율성이 높은 NPC는 플레이어에게 제안한다.

예:

```text
"좌측으로 적 증원이 접근합니다. 제가 저격 위치를 변경하겠습니다."
```

플레이어 선택:

```text
Approve
Modify
Reject
```

이 결과는 Relationship과 Learning에 반영한다.

---

# 16. NPC Memory

모든 과거를 저장하지 않고 중요한 사건만 저장한다.

```cpp
struct MemoryEvent
{
    EventType type;
    EntityID actor;
    EntityID target;
    ActionType action;
    bool success;
    float emotionalImpact;
    float tacticalValue;
    int missionID;
};
```

Memory 예:

```text
CommanderSavedMe
CommanderAbandonedAlly
AllyRescuedMe
SuccessfulFlank
FailedRescue
CommanderPraisedAction
CommanderRejectedSuggestion
CivilianDeath
SquadMemberLost
```

최근 경험에 더 높은 가중치를 주는 것을 권장한다.

```text
MemoryWeight = BaseImportance × Recency × EmotionalImpact
```

---

# 17. Social Imitation

NPC는 다른 NPC의 성공/실패 행동을 관찰하고 자신의 행동 성향에 일부 반영한다.

```cpp
struct ObservedBehavior
{
    EntityID source;
    ActionType action;
    bool success;
    float risk;
    float tacticalResult;
};
```

예:

```text
Mira가 Bell 구조
↓
Bell 또는 Noah가 관찰
↓
구조 성공
↓
Imitation Score 계산
↓
rescuePriority / teamSupport 변화
```

---

# 18. Imitation Score

```text
ImitationScore =
    Success
  × ObserverImitationRate
  × SourceInfluence
  × CommanderApproval
  × TacticalValue
```

SourceInfluence 예:

```text
Respect
Trust
Experience
Rank
RecentSuccess
```

Threshold 이상이면 Trait가 작게 변한다.

예:

```text
Rescue 성공 관찰
→ rescuePriority +0.02
→ teamSupport +0.01
```

실패한 행동은 Negative Learning이 가능하다.

```text
무리한 구조 실패
→ riskTolerance -0.03
→ rescuePriority -0.01
```

---

# 19. Commander Feedback

Mission 종료 후 특정 행동에 피드백 가능.

```text
Praise
Warning
Reprimand
Discussion
```

### Praise

- 해당 행동 Fitness 상승
- 사회적 모방 확률 상승

### Warning

- 행동 의도는 인정
- 위험 감수 성향 감소 가능

### Reprimand

- CommandObedience 상승
- Autonomy 감소
- 해당 행동 Fitness 감소

### Discussion

- 행동 이유를 대화
- Relationship / Personality / Memory에 영향

---

# 20. Behavior Strategy Population

NPC마다 여러 행동 전략 후보를 유지한다.

```cpp
struct BehaviorStrategy
{
    BehaviorTraits traits;
    float fitness;
    int usageCount;
    int successCount;
};
```

권장 Population:

```text
Prototype : 10
Final     : 10 ~ 20
```

---

# 21. Steady-State Genetic Algorithm

전체 Population을 한 번에 교체하지 않는다.

Mission 종료 후 일부 Strategy만 교체한다.

```text
Population = 10
↓
Worst 2 제거
↓
Parent A / B 선택
↓
Crossover
↓
Mutation
↓
2개 신규 Strategy 생성
↓
Population = 10
```

---

# 22. Parent Selection

초기 구현은 Tournament Selection 권장.

```cpp
BehaviorStrategy SelectParent(std::vector<BehaviorStrategy>& population)
{
    auto a = RandomStrategy();
    auto b = RandomStrategy();
    return a.fitness > b.fitness ? a : b;
}
```

---

# 23. Crossover

```cpp
BehaviorStrategy Crossover(
    const BehaviorStrategy& a,
    const BehaviorStrategy& b)
{
    BehaviorStrategy child;
    float alpha = Random01();

    child.traits.aggression =
        a.traits.aggression * alpha +
        b.traits.aggression * (1.0f - alpha);

    // 나머지 Trait 동일 방식
    return child;
}
```

---

# 24. Mutation

```cpp
float MutateTrait(float value, float mutationRate)
{
    if (Random01() > mutationRate)
        return value;

    value += RandomRange(-0.05f, 0.05f);
    return Clamp(value, 0.0f, 1.0f);
}
```

권장 변화량:

```text
±0.01 ~ ±0.05
```

한 Mission으로 성격이 급격히 바뀌지 않게 한다.

---

# 25. Personality Resistance

```text
EffectiveMutation = Mutation × (1 - PersonalityResistance)
```

기본 성격이 강한 캐릭터는 변화 속도가 느리다.

---

# 26. Fitness Model

Kill Count만으로 평가하지 않는다.

```cpp
struct FitnessWeights
{
    float missionSuccess;
    float survival;
    float allySurvival;
    float civilianSurvival;
    float objectiveContribution;
    float commandCompliance;
    float rescueSuccess;
    float damageEfficiency;
    float ammoEfficiency;
    float autonomousSuccess;
};
```

Fitness Weight는 플레이어의 지휘 성향과 Story Modifier의 영향을 받을 수 있다.

---

# 27. Commander Profile

플레이어에게 직접 성향을 고르게 하지 않고 실제 행동으로 추정한다.

```cpp
struct CommanderProfile
{
    float missionFirst;
    float survivalFirst;
    float rescueFirst;
    float autonomySupport;
    float obediencePreference;
    float civilianProtection;
};
```

이 값은 숨겨진 내부 데이터로 사용하는 것을 권장한다.

---

# 28. Mission Behavior Report

```cpp
struct MissionBehaviorReport
{
    int kills;
    int shotsFired;

    float damageDealt;
    float damageTaken;

    int alliesRescued;
    int civiliansSaved;

    int autonomousActions;
    int commandsFollowed;
    int commandsRejected;

    int objectiveActions;

    float ammoUsed;
    float dangerExposure;

    bool survived;
    bool missionSuccess;
};
```

Mission 종료 후 학습 및 Evolution의 입력 데이터로 사용한다.

---

# 29. Squad Culture

소대 전체의 Trait와 행동 기록을 분석해 현재 소대 문화를 계산한다.

```cpp
enum class SquadCulture
{
    CommandDependent,
    Autonomous,
    MutualProtection,
    MissionFirst,
    SurvivalFirst,
    Aggressive,
    Defensive,
    Balanced
};
```

예:

```text
Average rescuePriority > 0.7
AND Average teamSupport > 0.7
→ MutualProtection
```

```text
Average commandObedience > 0.8
AND Average autonomy < 0.3
→ CommandDependent
```

```text
Average objectivePriority > 0.75
→ MissionFirst
```

---

# 30. Squad Culture의 효과

단순 Stat Buff보다 Utility Weight에 영향을 주는 방식을 권장한다.

### MutualProtection

```text
Rescue Score × 1.25
Support Score × 1.20
```

### MissionFirst

```text
Objective Score × 1.30
Rescue Score × 0.85
```

### SurvivalFirst

```text
Retreat Score × 1.25
RiskTolerance Modifier 감소
```

정답 Culture는 존재하지 않는다.

---

# 31. Combat Role

```cpp
enum class CombatRole
{
    Frontline,
    Assault,
    Sniper,
    Support,
    MachineGunner,
    ElectronicWarfare,
    Medic
};
```

Role은 Utility Weight의 초기값에만 영향을 주고, 학습을 막지 않는다.

예:

```text
Sniper
- CoverPreference HIGH
- DistancePreference HIGH

Frontline
- Aggression HIGH
- TeamSupport HIGH

ElectronicWarfare
- HackPriority HIGH
- DirectCombatPriority LOW
```

---

# 32. Story Event Modifier

스토리 챕터가 AI 환경에 직접 영향을 줄 수 있다.

### Great Retreat

```text
CommunicationReliability ↓
AutonomyImportance ↑
SurvivalPressure ↑
```

### Marduk

```text
SelectionPressure ↑↑
ResourceScarcity ↑
CommanderConnectivity ↓
```

### Muromets

```text
SquadExperience HIGH
TacticalSuggestionFrequency ↑
```

### Nidhogg

```text
CommanderCommandEnabled = false
```

---

# 33. Group Separation

스토리상 소대가 갈라질 경우 그룹별로 별도의 Culture가 형성될 수 있다.

예:

```text
Group A
Commander + Mira + Bell

Group B
Noah + Leon
```

Group B:

```text
CommanderInfluence = 0
SocialImitationInfluence ↑
SelfLearning ↑
```

재회 시 두 그룹의 행동 문화 차이를 보여줄 수 있다.

---

# 34. Backup / Memory Reset

NPC가 과거 Backup에서 복원될 경우 데이터 계층을 분리한다.

복원:

```text
PersonalityBase
Backup 시점 BehaviorTraits
Backup 시점 일부 Relationship
```

복원되지 않음:

```text
Recent Memory
Recent Social Learning
Recent Personal Experience
```

다른 NPC들이 과거 행동을 기억하고 있으므로 Social Imitation을 통해 일부 행동 특성이 다시 나타날 수 있다.

---

# 35. Nidhogg Final Test

명령 링크 차단:

```text
CommanderCommandEnabled = false
```

이때 NPC 행동은 다음만으로 결정된다.

```text
BehaviorTraits
+
ActiveStrategy
+
Memory
+
Relationship
+
SocialLearning
+
SquadCulture
```

예상 결과:

### CommandDependent

- 명령 대기
- 행동 지연

### Autonomous

- 독립적으로 임무 수행
- 위치 변경/엄폐/지원 자율 처리

### MutualProtection

- 동료 구조 적극적
- Objective 지연 위험

### MissionFirst

- 목표 달성 우선
- 동료 손실 위험 증가

---

# 36. NPC Decision Pipeline

실제 Tick/Turn 처리 순서:

```text
1. Update Perception
2. Build Tactical Context
3. Check Critical Survival Action
4. Check Player Command
5. Generate Autonomous Candidate Actions
6. Apply Personality Modifier
7. Apply Relationship Modifier
8. Apply Squad Culture Modifier
9. Calculate Utility Scores
10. Select Action
11. Execute Action
12. Record Result
```

---

# 37. Decision Example

상황:

```text
HP = 60%
Ally Down
Enemy Nearby
Cover Available
Commander Connected
```

NPC 성향:

```text
rescuePriority = 0.8
riskTolerance = 0.6
teamSupport = 0.9
```

Utility:

```text
Attack  = 0.42
Cover   = 0.51
Rescue  = 0.78
Retreat = 0.23
```

결과:

```text
RescueAlly
```

---

# 38. Cooldown / Hysteresis

AI가 매 순간 행동을 바꾸는 것을 방지한다.

```text
DecisionCooldown
CurrentActionCommitment
MinimumUtilityDifference
```

예:

새로운 행동 Utility가 현재 행동보다 최소 `0.15` 이상 높을 때만 행동 변경.

---

# 39. Emergency Override

다음 행동은 일반 Utility보다 우선할 수 있다.

```text
GrenadeNearby
ImmediateDeathThreat
OutOfAmmo
CriticalHP
```

---

# 40. Pathfinding 분리

AI는 다음만 결정한다.

```text
"저 엄폐물로 이동한다."
```

Navigation System이 실제 경로를 계산한다.

AI Decision 코드에서 Pathfinding을 직접 구현하지 않는다.

---

# 41. 코드 책임 분리

```text
AI Decision
≠ Movement
≠ Animation
≠ Combat Resolution
```

AI:

```text
무엇을 할 것인가?
```

Movement:

```text
어떻게 이동하는가?
```

Combat:

```text
명중 / 피해 / 탄약
```

Animation:

```text
어떻게 보이는가?
```

---

# 42. 추천 Component 구조

```text
NPCController

├── PerceptionComponent
├── MemoryComponent
├── RelationshipComponent
├── BehaviorTraitComponent
├── UtilityDecisionComponent
├── CommandComponent
├── SocialLearningComponent
├── EvolutionComponent
├── SquadCultureComponent
└── DebugComponent
```

---

# 43. Save Data

NPC별:

```text
PersonalityBase
BehaviorTraits
StrategyPopulation
ActiveStrategy
LearningProfile
Relationship
ImportantMemories
MissionExperience
SocialInfluenceHistory
```

Squad:

```text
SquadCulture
SharedHistory
Doctrine
```

Commander:

```text
CommanderProfile
DecisionHistory
```

---

# 44. Debug UI

NPC 선택 시 최소 다음 정보를 볼 수 있어야 한다.

```text
Current State
Current Action

Behavior Traits
- Aggression
- Cover
- Rescue
- Obedience
- Autonomy
- Risk
- Support
- Objective

Utility Scores
- Attack
- Cover
- Rescue
- Retreat
- Objective

Current Strategy
Strategy Fitness

Relationship
- Trust
- Respect
- Bond
- Autonomy Support

Observed Behaviors
Recent Memories
Squad Culture
```

---

# 45. Debug Visualization

전투 화면에서 토글 가능하게 한다.

```text
NPC 시야 범위
위험도
엄폐 점수
목표 우선순위
Utility Score
현재 명령
현재 자율 행동
```

---

# 46. AI Logging

행동 선택 이유를 로그로 남긴다.

예:

```text
[BELL]

Context:
AllyDown = true
SelfDanger = 0.45

Scores:
Attack = 0.31
Cover = 0.46
Rescue = 0.78
Retreat = 0.25

Selected:
RescueAlly

Reason:
High RescuePriority
High TeamSupport
Moderate RiskTolerance
```

AI 디버깅에 필수.

---

# 47. Performance

AI 전체 계산을 매 Frame마다 하지 않는다.

권장:

```text
Perception Update : 5 ~ 10 Hz
Utility Decision  : 2 ~ 5 Hz
Critical Emergency: Event Based
Social Learning   : Action End Event
Evolution         : Mission End
```

---

# 48. Prototype Phase 1

처음에는 NPC 2명만 구현한다.

Traits 5개:

```text
Aggression
CoverPreference
RescuePriority
CommandObedience
Autonomy
```

Actions 5개:

```text
Attack
TakeCover
Rescue
Retreat
Objective
```

---

# 49. Prototype Test Map

```text
NPC A
NPC B
Enemy x3~5
Civilian x1
Objective x1
Extraction Zone
```

NPC B가 부상했을 때 NPC A의 후보 행동:

```text
Attack
TakeCover
Rescue NPC B
Continue Objective
Retreat
```

---

# 50. 학습 테스트

## Player A

항상 구조 행동을 Praise.

예상:

```text
RescuePriority ↑
TeamSupport ↑
Autonomous Rescue 증가
```

## Player B

Objective 행동을 Praise하고 Rescue를 Reprimand.

예상:

```text
ObjectivePriority ↑
CommandObedience ↑
RescuePriority ↓
```

---

# 51. Prototype 성공 조건

동일 초기 NPC로 20회 Mission 진행 후 두 플레이 스타일의 행동 차이가 눈으로 확인되어야 한다.

### Player A Squad

```text
부상자 자동 구조
서로 엄호
높은 자율 판단
```

### Player B Squad

```text
Objective 우선
명령 중심
안정적 진형 유지
```

특정 결과가 더 강한 정답이 되어서는 안 된다.

---

# 52. 구현 순서

```text
STEP 1  NPC Data Model
STEP 2  Perception System
STEP 3  Tactical Context
STEP 4  FSM + Utility AI
STEP 5  Player Command
STEP 6  Relationship + Autonomy
STEP 7  Memory System
STEP 8  Mission Behavior Report
STEP 9  Social Imitation
STEP 10 Steady-State Genetic Algorithm
STEP 11 Squad Culture
STEP 12 Story Event Modifier
STEP 13 Save / Load
STEP 14 Debug UI / AI Logging
```

---

# 53. 피해야 할 구현

## 완전 랜덤 AI

```text
Random(Attack, Cover, Rescue)
```

금지.

행동에는 설명 가능한 이유가 있어야 한다.

## 거대한 if 중첩

장기적으로 유지보수가 어렵다. FSM + Utility AI 조합을 사용한다.

## 전투 중 GA 실행

금지.

Evolution은 Mission 종료 시에만 실행한다.

## 너무 빠른 성격 변화

한 Mission으로 NPC 정체성이 변하지 않도록 한다.

## 특정 Squad Culture를 정답으로 만들기

모든 문화에는 장단점이 있어야 한다.

---

# 54. 최종 목표

NPC AI의 목적은 단순히 강한 AI를 만드는 것이 아니다.

플레이어가 게임을 진행하면서 다음 변화를 느끼게 해야 한다.

```text
초반
"내가 명령해야 움직인다."

중반
"내 방식을 배우고 있다."

후반
"내가 말하지 않아도 서로 판단한다."

최종
"이제 이들은 내가 없어도 자기 선택을 할 수 있다."
```

NPC AI는 게임의 핵심 주제인

```text
명령
→ 학습
→ 관계
→ 자율성
→ 자유
```

를 실제 Gameplay로 표현하기 위한 시스템이다.

---

# 55. Codex 첫 구현 요청 범위

처음부터 전체 시스템을 구현하지 않는다.

첫 프로토타입 범위:

```text
NPC 2명
Enemy 3~5명

Traits 5개
- Aggression
- CoverPreference
- RescuePriority
- CommandObedience
- Autonomy

Actions 5개
- Attack
- TakeCover
- Rescue
- Retreat
- Objective
```

먼저 Utility AI를 완성한다.

그 다음 순서:

```text
Mission Report
→ Social Imitation
→ Simple Evolution
→ Squad Culture
```

이 순서로 확장한다.
