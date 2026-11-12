#include "stdafx.h"
#include "Renderer.h"
#include "RpgGame.h"
#include "GameMonitor.h"
#include "Platform.h"
#include "OpenGL.h"
#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdio>
#include <memory>
#include <random>

namespace
{
std::unique_ptr<Renderer> renderer;
RpgGame game;
std::unique_ptr<GameMonitor> monitor;
GLFWwindow *window = nullptr;
int mouseX = 0;
int mouseY = 0;
int windowedX = 30;
int windowedY = 30;
int windowedWidth = 1280;
int windowedHeight = 800;
bool fullscreen = false;

void Error(int, const char *description)
{
    std::fprintf(stderr, "GLFW: %s\n", description ? description : "알 수 없는 오류");
}

int CharacterKey(int key)
{
    if (key >= GLFW_KEY_A && key <= GLFW_KEY_Z)
        return std::tolower('A' + key - GLFW_KEY_A);
    if (key >= GLFW_KEY_0 && key <= GLFW_KEY_9)
        return '0' + key - GLFW_KEY_0;
    if (key == GLFW_KEY_ENTER || key == GLFW_KEY_KP_ENTER)
        return 13;
    if (key == GLFW_KEY_SPACE)
        return ' ';
    return -1;
}

void UpdateMousePosition(double x, double y)
{
    if (!window || !renderer)
        return;
    int logicalWidth = 0, logicalHeight = 0;
    int framebufferWidth = 0, framebufferHeight = 0;
    glfwGetWindowSize(window, &logicalWidth, &logicalHeight);
    glfwGetFramebufferSize(window, &framebufferWidth, &framebufferHeight);
    if (logicalWidth <= 0 || logicalHeight <= 0)
        return;
    mouseX = int(std::round(x * framebufferWidth / logicalWidth));
    mouseY = int(std::round(y * framebufferHeight / logicalHeight));
}

void UpdateMousePosition()
{
    double x = 0, y = 0;
    glfwGetCursorPos(window, &x, &y);
    UpdateMousePosition(x, y);
}

void Resize(GLFWwindow *, int width, int height)
{
    if (renderer)
        renderer->Resize(width, height);
}

void Close(GLFWwindow *)
{
    game.dollTraining.Stop();
    renderer.reset();
}

void Motion(GLFWwindow *, double x, double y)
{
    UpdateMousePosition(x, y);
}

void CursorEnter(GLFWwindow *, int entered)
{
    if (!entered)
        game.ClearInput();
}

void Focus(GLFWwindow *, int focused)
{
    if (!focused)
    {
        game.ClearInput();
        game.paused = true;
    }
}

void Mouse(GLFWwindow *target, int button, int action, int)
{
    double x = 0, y = 0;
    glfwGetCursorPos(target, &x, &y);
    UpdateMousePosition(x, y);
    if (!renderer || game.dollTraining.active)
        return;
    if (button == GLFW_MOUSE_BUTTON_LEFT)
        game.firing = action == GLFW_PRESS && !game.paused && game.hp > 0 &&
                      !renderer->IsUi(mouseX, mouseY, game);
}

void Scroll(GLFWwindow *, double, double yOffset)
{
    if (!renderer || yOffset == 0)
        return;
    renderer->Zoom(yOffset > 0 ? 1 : -1);
    UpdateMousePosition();
}

void ToggleFullscreen()
{
    if (!window)
        return;
    if (!fullscreen)
    {
        glfwGetWindowPos(window, &windowedX, &windowedY);
        glfwGetWindowSize(window, &windowedWidth, &windowedHeight);
        GLFWmonitor *monitorTarget = glfwGetPrimaryMonitor();
        const GLFWvidmode *mode = monitorTarget ? glfwGetVideoMode(monitorTarget) : nullptr;
        if (!mode)
            return;
        glfwSetWindowMonitor(window,
                             monitorTarget,
                             0,
                             0,
                             mode->width,
                             mode->height,
                             mode->refreshRate);
        fullscreen = true;
    }
    else
    {
        glfwSetWindowMonitor(window,
                             nullptr,
                             windowedX,
                             windowedY,
                             windowedWidth,
                             windowedHeight,
                             GLFW_DONT_CARE);
        fullscreen = false;
    }
    game.ClearInput();
}

void Key(GLFWwindow *, int key, int, int action, int)
{
    if (action == GLFW_REPEAT)
        return;

    const int character = CharacterKey(key);
    if (action == GLFW_RELEASE)
    {
        if (character >= 0 && character < int(game.keys.size()))
            game.keys[character] = false;
        return;
    }
    if (action != GLFW_PRESS)
        return;

    if (key == GLFW_KEY_F11)
    {
        ToggleFullscreen();
        return;
    }
    if (key == GLFW_KEY_ESCAPE)
    {
        game.paused = !game.paused;
        game.ClearInput();
        return;
    }

    if (game.paused || game.hp <= 0)
    {
        if (character == 13)
            game.Reset(game.seed);
        else if (character == 'n')
            game.Reset(std::random_device{}());
        return;
    }
    if (character < 0 || character >= int(game.keys.size()))
        return;

    const unsigned char pressedKey = static_cast<unsigned char>(std::tolower(character));
    if (pressedKey == 'v' || pressedKey == 'm')
    {
        game.worldDebug = !game.worldDebug;
        return;
    }
    if (game.talkingNpc >= 0 && pressedKey >= '1' && pressedKey <= '5')
    {
        game.NpcChoice(pressedKey - '0');
        return;
    }
    if (pressedKey == 'h' && game.talkingNpc >= 0)
    {
        game.HelpSelectedNpc();
        return;
    }
    if (pressedKey == 'j')
    {
        game.npcDebug = !game.npcDebug;
        return;
    }
    if (pressedKey == 'l')
    {
        if (game.talkingNpc >= 0)
            return;
        game.inventory = false;
        game.storyJournalOpen = !game.storyJournalOpen;
        game.ClearInput();
        return;
    }
    if (pressedKey == 'f')
    {
        if (game.talkingActor)
            game.talkingActor = 0;
        else if (game.talkingNpc >= 0)
            game.talkingNpc = -1;
        else if (game.InteractWorldActor())
            return;
        else
            game.InteractNpc();
        return;
    }

    game.keys[pressedKey] = true;
    switch (pressedKey)
    {
    case 'e':
        if (!game.WorldRescue())
            game.Pickup();
        break;
    case 'q':
        game.Heal();
        break;
    case 'r':
        game.ToggleRest();
        break;
    case 'b':
        game.EatRation();
        break;
    case 't':
        game.DrinkWater();
        break;
    case 'i':
        if (game.talkingNpc >= 0)
            break;
        game.storyJournalOpen = false;
        game.inventory = !game.inventory;
        game.ClearInput();
        break;
    default:
        break;
    }
}
}

int main()
{
    glfwSetErrorCallback(Error);
    if (!glfwInit())
    {
        std::fprintf(stderr, "GLFW 초기화에 실패했습니다.\n");
        return 1;
    }

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 0);
    glfwWindowHint(GLFW_DOUBLEBUFFER, GLFW_TRUE);
    glfwWindowHint(GLFW_DEPTH_BITS, 24);
    glfwWindowHint(GLFW_SAMPLES, 0);
    glfwWindowHint(GLFW_SCALE_TO_MONITOR, GLFW_TRUE);

    GLFWmonitor *primaryMonitor = glfwGetPrimaryMonitor();
    int workX = 0, workY = 0, workWidth = 1920, workHeight = 1080;
    if (primaryMonitor)
        glfwGetMonitorWorkarea(primaryMonitor, &workX, &workY, &workWidth, &workHeight);
    windowedWidth = std::max(640, std::min(1920, workWidth - 80));
    windowedHeight = std::max(480, std::min(1080, workHeight - 100));
    windowedX = workX + 30;
    windowedY = workY + 30;

    window = glfwCreateWindow(
        windowedWidth, windowedHeight, "왕국 교역로 | 실종자 수색과 귀환", nullptr, nullptr);
    if (!window)
    {
        std::fprintf(stderr, "OpenGL 3.0 창을 만들 수 없습니다.\n");
        glfwTerminate();
        return 1;
    }
    glfwSetWindowPos(window, windowedX, windowedY);
    glfwMakeContextCurrent(window);
    glfwSwapInterval(1);

    glewExperimental = GL_TRUE;
    const GLenum status = glewInit();
    if (status != GLEW_OK || !GLEW_VERSION_3_0)
    {
        std::fprintf(stderr,
                     "OpenGL 3.0 호환 초기화에 실패했습니다: %s\n",
                     glewGetErrorString(status));
        glfwDestroyWindow(window);
        glfwTerminate();
        return 1;
    }
    glGetError();

    const auto executableDirectory = Platform::ExecutableDirectory();
    game.dollTraining.SetLogDirectory(executableDirectory / "Profiles");
    monitor = std::make_unique<GameMonitor>(executableDirectory / "Profiles" / "monitor.json");
    int framebufferWidth = 0, framebufferHeight = 0;
    glfwGetFramebufferSize(window, &framebufferWidth, &framebufferHeight);
    renderer = std::make_unique<Renderer>(framebufferWidth, framebufferHeight);
    glfwSetFramebufferSizeCallback(window, Resize);
    glfwSetWindowCloseCallback(window, Close);
    glfwSetCursorPosCallback(window, Motion);
    glfwSetCursorEnterCallback(window, CursorEnter);
    glfwSetWindowFocusCallback(window, Focus);
    glfwSetMouseButtonCallback(window, Mouse);
    glfwSetScrollCallback(window, Scroll);
    glfwSetKeyCallback(window, Key);

    double previousTick = glfwGetTime();
    double fpsWindowStart = previousTick;
    unsigned fpsWindowFrames = 0;
    float currentFps = 0;
    while (!glfwWindowShouldClose(window))
    {
        glfwPollEvents();
        if (glfwWindowShouldClose(window) || !renderer)
            break;
        const double now = glfwGetTime();
        const float deltaTime = float(std::max(0.0, now - previousTick));
        previousTick = now;

        if (glfwGetWindowAttrib(window, GLFW_FOCUSED) != GLFW_TRUE)
        {
            game.ClearInput();
            game.paused = true;
        }
        UpdateMousePosition();
        if (!renderer->IsUi(mouseX, mouseY, game))
            game.aim = renderer->Pick(mouseX, mouseY);
        else
            game.firing = false;
        game.Update(deltaTime);
        if (monitor)
            monitor->Publish(game);
        ++fpsWindowFrames;
        const double fpsWindowElapsed = now - fpsWindowStart;
        if (fpsWindowElapsed >= 1.0)
        {
            currentFps = float(fpsWindowFrames / fpsWindowElapsed);
            fpsWindowFrames = 0;
            fpsWindowStart = now;
        }
        renderer->SetFrameRate(currentFps);
        renderer->Draw(game, mouseX, mouseY);
        glfwSwapBuffers(window);
    }

    Close(window);
    monitor.reset();
    glfwDestroyWindow(window);
    window = nullptr;
    glfwTerminate();
    return 0;
}
