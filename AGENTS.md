# GameSoftwareEnginering — Codex 게임 개발 지침

사용자 최신 지시가 우선한다. 2026-10-28 실시간 조작·현재 그래픽·소설 세계관 유지 및 Gobboree식 생존·일과·경보·구조 의뢰 전환을 반영한다.

## 협업·컨벤션

- 한국어로 보고한다. 실제 플레이 가능한 RPG 제작이 목표다.
- 특별한 상황이 아니면 C++ 빌드·게임 실행은 사용자가 한다. 정적 검토·리소스/브라우저 검사와 실제 게임 검증을 구분한다.
- 기존 사용자 변경·IDE 캐시·빌드 산출물을 임의로 삭제하거나 되돌리지 않는다.
- 루트 .clang-format: 4칸, 다음 줄 중괄호, 함수 사이 빈 줄, 한 줄 명령문 분리, 100열.
- 단순 포맷에서 include·using 순서·문자열·셰이더를 바꾸지 않는다. 외부 의존성·생성 리소스는 일괄 포맷하지 않는다.
- 계획·미정·구현·실행 검증을 구분한다. 요청 없이 로드맵 전체를 구현하지 않는다.

## 최신 방향

- 《용사 파티 때려치웁니다》 사용자 원문이 설정 기준이다. docs/NOVEL_WORLD_REFERENCE.md와 docs/WORLD_AND_STORY.md를 먼저 읽는다.
- 소녀전선 지휘관·전술인형·PMC·회사 분기·무전 구조 사건은 현행 설정이 아니다. 원문·추출 자료는 이력으로 보존하되 활성 실행에 재연결하지 않는다.
- 원작 근거와 각색을 구분한다. 미확인 대륙명·국가·환율·원작 지리를 임의로 확정하지 않는다.
- 플레이어는 왕국 시기의 별도 신참 모험가다. 라니엘/라니아·카일·레미아·사라의 정체·능력·관계를 복사하지 않는다.
- WASD 연속 이동·마우스 조준·실시간 공격을 유지한다. 생존·일과·지역 경보·단서/호위/귀환 의뢰를 연결하며, 경험치·레벨 자동 성장 대신 숙련·실적을 사용한다. 타워디펜스·타일 이동과 성인 콘텐츠는 도입하지 않는다.
- 고정2.5D 쿼터뷰와 실제3D 배경을 유지한다. 현재 인간·동물도 Resource의3D 스켈레탈 메시다. 옛 SD 유지 규칙은 대체되었다.
- 가능한 환경 효과는 셰이더를 쓴다. 바닥 물은 재추가하지 않는다.

## 월드·주민·의뢰

- 마을5개: 아플리아 외곽·카디낙 길목·하멜른·키르멜트 후방·쿠락트 산록. 원작 지도와 동일한 지리가 아닌 압축 교역로다.
- 인접 광장72단위 / 플레이어 속도3.6 = 직선 약20초. 실측과 구분한다.
- 마을당 등록20명, 총100명. 현장 인구는 여행으로 변한다.
- 교역로4개·각50마리 슬롯·총200마리. 마수 사망 처리는 DamageMonster가 소유하며 다음 새벽까지 재생성을 지연한다. 기존 안전 배치·후보 없을 때 재시도는 유지한다. 3초 반복 생성으로 되돌리지 않는다.
- 소문은 실제로 이동한 NPC가 가까운 상대에게만 전달한다. 전역 목록·무전·즉시 원격 지식으로 방송하지 않는다.
- 마을별 인구·범죄·부상·안전·사기·경제·신뢰·분위기 분포·최근120초 통계와 소문을 표시한다.
- 첫 의뢰는 실종 사냥꾼의 단서·현장 접촉·호위·아플리아 귀환·경비 F→3 보고다. 단서 중복·보상 중복을 막고 기존 주민 ID와 실제 이동을 유지한다. 옛 방문5/토벌12 보고는 활성 목표가 아니다.
- 은화는 지갑·거래·약탈·반환과 실제로 이전한다. 대화 상대만 이동을 멈춘다.
- 이동 가능 지역을 연결하며 중앙도로를 건물·나무·집기로 막지 않는다.
- 시스템 최신 기준: docs/REALTIME_EXPEDITION.md. 지도·원작 각색 이력은 docs/NOVEL_WORLD_PROTOTYPE.md.

## 그래픽·도구

- 원본: BaseProject_SE/Resource/Frontier_LowPoly_Kit_v1. 별도 리소스 세션이 만든 Blender·GLB·재질·애니메이션을 임의 덮어쓰지 않는다.
- tools/resources/compile_pack.py → BaseProject_SE/SimpleGame/Assets/World/kingdom.rpack.
- tools/resources/verify_pack.py로 형식·웨이트·표본 스키닝을 검사한다.
- WorldResources.h/.cpp는 정적 청크·마을 캐시와 동적 메시 배치를 담당한다. 개별 GLUT 호출로 되돌리지 않는다.
- 이동 속도와 클립 재생 속도를 함께 조정한다. 자세 전환은 리소스 조합 규칙을 참고한다.
- 연결 범위·제한: docs/RESOURCE_WORLD_INTEGRATION.md.
- Blender는 tools/blender/Blender.ps1과 docs/BLENDER_WORKFLOW.md를 따른다. Steam Blender5.2.2 LTS 백그라운드 bpy를 확인했다.
- PC 경로는 Git 제외된 tools/blender/local.settings.json. GUI MCP·범용 GLB 직접 로더와 현재 변환 연결을 구분한다.

## 기술·검증

- BaseProject_SE/SimpleGame.sln, C++17/OpenGL/GLEW/GLFW. GLFW 3.5.1, GLEW 2.3.1, stb와 Noto Sans KR을 저장소에 포함하며, GLFW/GLEW는 정적으로 빌드한다.
- Visual Studio 솔루션은 Windows Win32/x64용이다. 루트 CMake 프로젝트는 Windows와 X11 기반 Linux용 소스 빌드 경로를 제공한다.
- 렌더러는 OpenGL 3.0 호환 프로필과 GLU, 고정 기능 호출을 사용한다. macOS OpenGL Core Profile 지원은 별도 렌더러 이행 전까지 대상이 아니다.
- Windows GDI/GDI+/WIC 의존은 제거했다. 한글 UI 글꼴과 PNG 디코더는 포함된 Noto Sans KR/stb를 사용한다.
- 활성: SimpleGame/RpgGame/RpgWorld/RpgBandits/RpgEconomy/RpgExpedition/ExpeditionRules/TownNpc/TownKnowledge/Renderer/RenderEffects/WorldResources/GameMonitor.
- 옛 TutorialGame·TowerDefense·WorldSimulation/DollBehavior 스토리는 현행 기준이 아니다. 호환 소스가 남아도 활성 진입·옛 자료 로드는 막아 둔다.
- NPC·마수·소문·월드·스키닝·정적 캐시·메시 draw call은 Profiles JSONL로 기록하고 파일을 회전한다.
- 모니터: tools/monitor/OpenMonitor.cmd, docs/LOCAL_MONITOR.md. JSON을 로컬 HTTP로 수신한다.
- C++ 빌드·게임 실행은 미검증이다. 리소스·DOM·서버 검사는 별도로 기록한다.
- 영속 저장·본편 전체 재현·추가 대륙·런타임 신체 파츠 편집기는 후속 범위다.
- 증분 링크는 끄고 중간 산출물은 .build/플랫폼/구성에 둔다. 보안을 끄지 않는다.

과거 지침: docs/history/AGENTS_GF_2026-10-21.md. 과거 숫자·설정을 최신 요구보다 우선하지 않는다.
