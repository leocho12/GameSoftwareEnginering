# GLFW 전환 및 빌드 안내

## 포함된 의존성

- GLFW 3.5.1: `BaseProject_SE/SimpleGame/Dependencies/glfw`
- GLEW 2.3.1: `BaseProject_SE/SimpleGame/Dependencies/glew`
- stb_image, stb_truetype: `BaseProject_SE/SimpleGame/Dependencies/stb`
- Noto Sans KR: `BaseProject_SE/SimpleGame/Assets/Fonts`

GLFW와 GLEW는 소스에서 정적 라이브러리로 빌드합니다. 별도 GLFW/GLEW DLL은 필요하지 않습니다. PNG 디코딩과 한글 글꼴도 프로젝트에 포함했으며, Windows GDI/GDI+/WIC 런타임에 의존하지 않습니다. 의존성별 사용 허가 전문은 각 리소스 폴더에 있습니다.

## Visual Studio

Windows에서는 `BaseProject_SE/SimpleGame.sln`의 `SimpleGame` 프로젝트를 빌드합니다. GLFW와 GLEW의 필요한 C 소스가 프로젝트 항목에 포함되어 있으므로, 저장소를 새 PC에 복제한 뒤 추가 라이브러리 설치나 DLL 복사 없이 빌드할 수 있습니다. 현재 솔루션은 Win32와 x64 구성을 유지합니다.

## CMake

저장소 루트에서 기본 CMake 생성기를 사용해 구성합니다.

```powershell
cmake -S . -B BaseProject_SE/SimpleGame/.build/cmake
cmake --build BaseProject_SE/SimpleGame/.build/cmake --config Debug
```

Visual Studio 외의 Windows 컴파일러나 Linux에서는 해당 환경의 C/C++ 컴파일러와 CMake를 지정해 구성할 수 있습니다. Linux에서는 같은 경로에 `-DCMAKE_BUILD_TYPE=Debug` 또는 `Release`를 지정합니다. Linux는 X11, XRandR, Xinerama, Xcursor, Xi, Xext, OpenGL 및 GLU 개발 파일이 필요합니다. Wayland 전용 개발 환경에서는 XWayland와 이 X11 개발 파일을 설치해야 합니다.

실행 파일 옆에 `Assets` 폴더가 복사됩니다. 프로필 로그도 실행 파일 옆 `Profiles`에 생성됩니다. Windows CMake 빌드는 정적 MSVC 런타임을 사용해 Visual C++ 재배포 DLL 의존성을 줄입니다. 운영체제의 OpenGL 드라이버는 별도로 필요합니다.

## 렌더링 호환성

현재 렌더러는 OpenGL 3.0 호환 프로필과 GLU의 쿼터뷰 투영, 고정 기능 파이프라인 호출을 사용합니다. Windows와 Linux의 호환 프로필 드라이버를 대상으로 합니다. macOS는 OpenGL 3.0 호환 프로필을 제공하지 않으므로 컴파일 대상과 실제 실행 가능 플랫폼은 구분해야 합니다. Core Profile로 이행하는 렌더러 교체는 별도 작업입니다.
