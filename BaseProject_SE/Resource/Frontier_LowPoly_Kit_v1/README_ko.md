# Frontier LowPoly Kit v1
Blender 5.2.2 LTS에서 제작한 전쟁 이후 숲·전초기지·민간 거주지용 로우폴리 리소스 초안입니다.

## 바로 열기
- Characters/Human_Modular_Library.blend: 7등신 인간 기본 골격과 교체 가능한 파츠 전체.
- Characters/Presets/*.glb: 주인공 지휘관, 정찰병, 주민, 작업자 조합 예시.
- Animals/*_Modular.blend 및 *.glb: 호랑이, 늑대, 여우, 사슴. 종별 골격과 분리된 신체 파츠.
- Settlement/Settlement_Modular_Library.blend: 건축과 집기 모듈.
- Settlement/Home_Prefab.blend: 같은 부품으로 조립한 주거 예시.
- Nature/Nature_Modular_Library.blend: 나무, 덤불, 풀, 바위, 지면 타일.
- Previews: Blender 렌더 미리보기.
- Materials/Textures: 타일 반복 가능한 256×256 BaseColor/Roughness PNG. 색상은 sRGB, Roughness는 Non-Color.
- Documentation/animation_transitions.json: 클립 조합, 전환 시간, 회전 기준 포즈, 이벤트와 루트 이동 처리.
- Documentation/asset_manifest.json: 전체 파일·메시 수·삼각형 수·골격·클립 목록.

## 조합 규약
1 Blender 단위 = 1m. Blender는 Z 위 / -Y 전방. GLB는 Y 위 / +Z 전방. GLB 내보내기에서 축을 변환합니다.
인간은 신장 1.82m, 머리 기준 0.26m인 7등신입니다. HEAD/FACE/HAIR/TORSO/ARM_L/ARM_R/PELVIS/LEG_L/LEG_R/FOOT/HAND 슬롯으로 조합합니다.
모든 인간 파츠는 HUM_7H_v1의 동일한 바인드 자세와 본 이름을 사용합니다. 하나의 골격에 같은 슬롯의 메시 하나씩 연결하고 웨이트를 보존하세요. Face와 Hair는 Head 위에 올리는 메시입니다.
의상 몸통과 좌우 소매는 같은 의상 종류를 사용합니다. TacticalVest는 FieldJacket용 겹옷입니다. Cap과 머리카락이 겹치면 머리카락을 숨기세요.
개별 모듈 GLB는 바인드 골격과 해당 파츠를 포함하며 애니메이션은 없습니다. 완성 조합 GLB와 각 원본 blend에 애니메이션이 있습니다.
동물은 BODY/NECK/HEAD/JAW/EARS/앞·뒷다리 네 개/TAIL과 사슴 ANTLERS를 분리했습니다.
서로 다른 종은 바인드 위치가 달라 직접 교환할 수 없습니다. 같은 종의 파츠만 직접 교환하고, 종을 넘는 변형은 리타기팅·리깅이 필요합니다.
앞다리는 팔꿈치·손목, 뒷다리는 앞으로 나온 무릎과 뒤로 꺾인 비절·발목을 별도 본으로 표현했습니다. 사슴은 갈라진 발굽을 사용합니다.
위험/중립/순함은 asset_manifest의 기본 분류이며, AI 행동은 포함하지 않습니다.

## 애니메이션
30fps. 모든 골격은 20개 Action을 포함합니다.
Pose_Neutral / Pose_Turn_L45 / Pose_Turn_R45 / Pose_Crouch
Idle / Walk / Run / Turn_L90 / Turn_R90 / Attack
Jump / Jump_Start / Jump_Air / Jump_Land
Crouch / Crouch_Walk / Stand_To_Crouch / Crouch_To_Stand / Walk_Start / Walk_Stop
걷기·뛰기는 제자리 순환입니다. root의 수평 이동은 없습니다. 게임 이동 속도는 스트라이드에 맞게 조정해야 합니다.
회전은 root 본의 회전을 포함합니다. 캐릭터 전체 회전과 중복 적용하지 마세요.
전체 Jump에는 root 본의 수직 이동이 포함됩니다. 물리 점프를 사용한다면 전체 Jump 대신 분할 클립을 연결하고 root Z를 추출·제거하세요.
Pose_*는 짧은 고정 자세입니다. 기본 중립 자세를 기준으로 좌우 45도 자세를 보간하거나 가산 레이어 차이를 계산합니다.
일반 인간은 오른손 공격입니다. 무기 메시·무기별 공격과 전투 판정은 포함하지 않았습니다.
블렌더 Action 편집기에서 골격을 선택하고 Action을 바꾸어 확인합니다. 원본은 A포즈/중립으로 저장되어 있습니다.
애니메이션은 절차적으로 키를 만든 1차 블로킹입니다. 실제 카메라와 게임 속도에서 접지·관절·클립 전환에 대한 최종 조정이 필요합니다.

## 건축과 자연
건축: 벽 폭 2m, 높이 2.6m, 바닥 2×2m. 기본 모듈 중심 X/Y, 바닥 Z=0. 벽은 X방향 길이이고 Y축 회전이 아니라 Z축 90도로 측벽을 만듭니다.
문은 힌지 로컬 원점. 창 프레임은 창 중심 원점. Roof_Slope는 낮은 처마 로컬 X=0에서 ±X 2m 방향으로 1.05m 상승합니다.
가구는 실제 사람 크기에 맞춘 독립 배치용. 콜라이더·문 스크립트·대화·아이템 기능은 포함하지 않습니다.
지면은 2×2m이고 외곽 높이는 0이므로 나란히 연결할 수 있습니다. 잔디는 양면 재질의 삼각형 잎입니다.
나무는 침엽수/활엽수/고사목, 그루터기와 통나무를 포함합니다. 텍스처는 GLB 안에 포함하고 blend에도 패킹했습니다.

## 범위와 검증
제작·저장은 Resource/Frontier_LowPoly_Kit_v1 안에서만 진행했습니다. 게임 코드·빌드·렌더러는 변경하지 않았습니다.
GLB는 표준 전달용이며 프로젝트 로더와 연결하는 작업은 별도입니다.
블렌더 메시·웨이트·클립, GLB의 skin/animation 채널 및 파일 재열기 결과는 Documentation/validation_report.json을 참고하세요.
_Source/author_kit.py는 Blender 내부 제작을 재현하는 원본이며 게임용 코드가 아닙니다.

## 제작 완료 목록

- Blender 원본 9개, GLB 132개.
- 7등신 인간 파츠 26종과 주인공·NPC 조합 예시 4종.
- 호랑이·늑대·여우·사슴 4종과 교체 가능한 동물 파츠 41종.
- 건축·생활 모듈 32종과 조립식 주거 예시 1종.
- 자연·지면 모듈 24종, 재질 텍스처 PNG 17개.
- 각 인간·동물 골격에 걷기·뛰기·회전·공격·점프·웅크리기 및 기준 자세·전환을 포함한 20개 클립.

읽기 쉬운 동작 조합은 Documentation/애니메이션_연결표.md를 참고하세요.
273개 검사 항목을 통과했으며 오류와 경고는 0개입니다. 게임 내 연결·재생 검증은 별도입니다.
호랑이 줄무늬는 Materials/Textures/Tiger_Stripes_BaseColor.png를 사용하며 해당 GLB와 blend에 포함되어 있습니다.
보행 재생 속도와 혼합 시간은 Documentation/animation_transitions.json에서 확인할 수 있습니다.

