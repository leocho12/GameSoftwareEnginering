set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_DOCS OFF CACHE BOOL "" FORCE)
set(GLFW_INSTALL OFF CACHE BOOL "" FORCE)
set(BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)

if(UNIX AND NOT APPLE)
    set(GLFW_BUILD_X11 ON CACHE BOOL "" FORCE)
    set(GLFW_BUILD_WAYLAND OFF CACHE BOOL "" FORCE)
endif()

find_package(OpenGL REQUIRED)
if(NOT TARGET OpenGL::GLU)
    message(FATAL_ERROR "OpenGL GLU development files are required by the current renderer.")
endif()

add_subdirectory(Dependencies/glfw)

add_library(glew_static STATIC Dependencies/glew/src/glew.c)
target_include_directories(glew_static PUBLIC Dependencies/glew/include)
target_compile_definitions(glew_static PUBLIC GLEW_STATIC GLEW_NO_GLU)
target_link_libraries(glew_static PUBLIC OpenGL::GL)

if(UNIX AND NOT APPLE)
    find_package(X11 REQUIRED)
    target_link_libraries(glew_static PRIVATE X11::X11)
    if(TARGET OpenGL::GLX)
        target_link_libraries(glew_static PRIVATE OpenGL::GLX)
    endif()
endif()

set(SIMPLE_GAME_SOURCES
    CharacterAnimation.cpp
    DollBehavior.cpp
    GameMonitor.cpp
    Platform.cpp
    RenderEffects.cpp
    RenderGeometry.cpp
    Renderer.cpp
    RendererDolls.cpp
    RendererTown.cpp
    RendererWorld.cpp
    RpgBandits.cpp
    RpgEconomy.cpp
    RpgExpedition.cpp
    RpgGame.cpp
    RpgWorld.cpp
    SimpleGame.cpp
    SpriteRasterizer.cpp
    ThirdPartyStb.cpp
    TownKnowledge.cpp
    TownNpc.cpp
    WorldResources.cpp
    WorldSimulation.cpp
    stdafx.cpp
)

add_executable(SimpleGame ${SIMPLE_GAME_SOURCES})
target_include_directories(SimpleGame PRIVATE . Dependencies/stb)
target_compile_definitions(SimpleGame PRIVATE NOMINMAX)
target_link_libraries(SimpleGame PRIVATE glfw glew_static OpenGL::GL OpenGL::GLU)

if(WIN32)
    target_link_libraries(SimpleGame PRIVATE user32 gdi32 shell32)
endif()

if(MSVC)
    target_compile_options(SimpleGame PRIVATE /W3 /utf-8 /permissive- /sdl)
endif()

add_custom_command(TARGET SimpleGame POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory
            "$<TARGET_FILE_DIR:SimpleGame>/Assets/World"
    COMMAND ${CMAKE_COMMAND} -E make_directory
            "$<TARGET_FILE_DIR:SimpleGame>/Assets/Fonts"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${CMAKE_CURRENT_SOURCE_DIR}/Assets/World/kingdom.rpack"
            "${CMAKE_CURRENT_SOURCE_DIR}/Assets/World/manifest.json"
            "$<TARGET_FILE_DIR:SimpleGame>/Assets/World"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${CMAKE_CURRENT_SOURCE_DIR}/Assets/Fonts/NotoSansKR.ttf"
            "${CMAKE_CURRENT_SOURCE_DIR}/Assets/Fonts/OFL.txt"
            "$<TARGET_FILE_DIR:SimpleGame>/Assets/Fonts"
    VERBATIM
)
