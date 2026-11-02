# Blender 리소스 제작 연결

2026-10-21 · 설치 경로 및 실제 제작 스크립트 실행 검증 완료

## 확인한 설치

- Blender **5.2.2 LTS**, Steam 설치본.
- 실행 파일: `C:/Program Files (x86)/Steam/steamapps/common/Blender/blender.exe`.
- PC별 경로: `tools/blender/local.settings.json`. 이 파일은 Git에서 제외한다.
- 경로를 임시로 바꾸려면 `BLENDER_EXECUTABLE` 환경 변수를 사용한다. 우선순위는 환경 변수 → 로컬 설정 → PATH다.

프로젝트에서 설치된 Blender를 별도 백그라운드 프로세스로 실행하고 Python `bpy`로 메시·재질·장면을 제작한다. 열린 Blender 창이나 작업 파일에 연결해 조작하는 MCP 서버 방식은 아니다. 기존 GUI 장면을 저장·종료하거나 사용자 설정을 수정하지 않는다. 연결된 전용 Blender 플러그인은 이번 검색에서 발견되지 않았다.

## 연결 확인

저장소 루트의 PowerShell에서:

```powershell
.\tools\blender\Blender.ps1 -Action Check
```

`tools/blender/probe_blender.py`가 임시 삼각형 메시를 메모리에서 생성·검증하고 제거한다. 결과는 `tools/blender/connection.local.json`에 기록하며 Git에서 제외한다.

이번 실행에서 확인한 결과:

- `ready: true`, `mesh_creation_verified: true`
- `background: true`, `saved_scene: false`
- `export_gltf_available: true`, `export_obj_available: true`

메시를 실제로 만들 수 있음을 확인한 것이며, 새 게임 리소스나 `.blend` 작업 파일을 생성한 것은 아니다. GLB·OBJ는 내보내기 기능의 등록 여부를 확인했으며 실제 파일 출력 검사는 아직 하지 않았다.

## 리소스 제작 스크립트 실행

```powershell
.\tools\blender\Blender.ps1 -Action Run -Script tools/blender/create_asset.py
```

위 `create_asset.py`는 이후 리소스 요청 때 작성할 스크립트 이름의 예시이며 현재 제공된 파일이 아니다. 인자가 필요하면:

```powershell
.\tools\blender\Blender.ps1 -Action Run -Script tools/blender/create_asset.py -ScriptArguments @('--output', 'AssetsSource/Blender/supply_crate.blend')
```

실행 작업 디렉터리는 저장소 루트다. `--background --factory-startup`으로 별도 장면에서 실행하며, Python 예외는 종료 코드 1로 전달한다. 스크립트에서 저장·내보내기를 명시해야 파일이 만들어진다. 생성 스크립트는 기존 파일을 덮어쓸 때 사용자 요청 범위를 확인한다.

앞으로 Blender 리소스 제작 요청에서는 이 실행기를 사용한다. 외부 Python에 `bpy`를 설치할 필요 없이 Blender에 포함된 Python을 사용한다. 예를 들어 군용 보급 상자·마을 건물·바위 등의 제작 스크립트를 작성해 이 경로로 실행할 수 있다.

## 게임 적용 범위

원본 작업 파일은 `AssetsSource/Blender`, 게임용 출력은 `BaseProject_SE/SimpleGame/Assets` 아래에 자산별로 관리하는 것을 기본으로 한다. 구체적인 파일은 리소스 요청 때 만든다. 원작 캐릭터 SD 원본은 보존한다.

Blender 연결과 게임 엔진의 모델 로딩은 별개다. 현재 OpenGL 렌더러에는 범용 GLB·OBJ 모델 로딩 경로가 없으므로, 제작한 모델을 실제 게임에 넣을 때는 변환 형식·로더·재질 연결을 추가로 구현해야 한다. 이번에는 렌더러·게임 플레이·빌드 설정을 변경하지 않았다. 게임 빌드·실행 확인은 기존처럼 사용자 담당이다.


## 2026-10-22 게임 연결 추가

Resource의 전달 GLB를 tools/resources/compile_pack.py로 RPK1으로 변환하여 게임에 연결했다. 범용 GLB 직접 로더나 열린 Blender GUI 접속을 추가한 것은 아니다. 현재 배치·스키닝·검증은 [RESOURCE_WORLD_INTEGRATION.md](RESOURCE_WORLD_INTEGRATION.md)를 따른다.
