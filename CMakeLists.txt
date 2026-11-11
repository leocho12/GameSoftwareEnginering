cmake_minimum_required(VERSION 3.24)

if(POLICY CMP0091)
    cmake_policy(SET CMP0091 NEW)
endif()

project(GameSoftwareEnginering LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(MSVC)
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
endif()

add_subdirectory(BaseProject_SE/SimpleGame)
