#include "stdafx.h"
#include "Renderer.h"
#include "RpgGame.h"
#include "Dependencies/freeglut.h"
#include <Windows.h>
#include <algorithm>
#include <cctype>
#include <cstdio>
#include <memory>
#include <random>

namespace
{
std::unique_ptr<Renderer> renderer;
RpgGame game;
int mouseX = 0, mouseY = 0, lastTick = 0;

void Display()
{
    if (renderer)
    {
        renderer->Draw(game, mouseX, mouseY);
        glutSwapBuffers();
    }
}

void Resize(int w, int h)
{
    if (renderer)
        renderer->Resize(w, h);
}

void Close()
{
    renderer.reset();
}

void Motion(int x, int y)
{
    mouseX = x;
    mouseY = y;
}

void Entry(int state)
{
    if (state == GLUT_LEFT)
        game.ClearInput();
}

void Mouse(int button, int state, int x, int y)
{
    mouseX = x;
    mouseY = y;
    if (!renderer)
        return;
    if (state == GLUT_DOWN && (button == 3 || button == 4))
    {
        renderer->Zoom(button == 3 ? 1 : -1);
        return;
    }
    if (button == GLUT_LEFT_BUTTON)
        game.firing =
            state == GLUT_DOWN && !game.paused && game.hp > 0 && !renderer->IsUi(x, y, game);
}

void KeyUp(unsigned char key, int, int)
{
    game.keys[std::tolower(key)] = false;
}

void Key(unsigned char key, int, int)
{
    key = static_cast<unsigned char>(std::tolower(key));
    if (key == 27)
    {
        game.paused = !game.paused;
        game.ClearInput();
        return;
    }
    if (game.paused || game.hp <= 0)
    {
        if (key == 13)
            game.Reset(game.seed);
        else if (key == 'n')
            game.Reset(std::random_device{}());
        return;
    }
    game.keys[key] = true;
    switch (key)
    {
    case 'e':
        game.Pickup();
        break;
    case 'q':
        game.Heal();
        break;
    case 'i':
        game.inventory = !game.inventory;
        game.firing = false;
        break;
    default:
        break;
    }
}

void Special(int key, int, int)
{
    if (key == GLUT_KEY_F11)
    {
        static bool full = false;
        full = !full;
        if (full)
            glutFullScreen();
        else
            glutLeaveFullScreen();
        game.ClearInput();
    }
}

void Tick(int)
{
    if (!renderer)
        return;
    int now = glutGet(GLUT_ELAPSED_TIME);
    float dt = std::max(0.0f, float(now - lastTick) / 1000);
    lastTick = now;
    // Focus loss must not leave movement or automatic fire held down.
    HWND window = WindowFromDC(wglGetCurrentDC());
    if (GetForegroundWindow() != window)
    {
        game.ClearInput();
        game.paused = true;
    }
    if (!renderer->IsUi(mouseX, mouseY, game))
        game.aim = renderer->Pick(mouseX, mouseY);
    else
        game.firing = false;
    game.Update(dt);
    glutPostRedisplay();
    glutTimerFunc(16, Tick, 0);
}
}

int main(int argc, char **argv)
{
    // Avoid Windows bitmap-stretching the entire OpenGL window on high-DPI monitors.
    using SetDpiContext = BOOL(WINAPI *)(HANDLE);
    auto setDpi = reinterpret_cast<SetDpiContext>(
        GetProcAddress(GetModuleHandleW(L"user32.dll"), "SetProcessDpiAwarenessContext"));
    if (!setDpi || !setDpi(reinterpret_cast<HANDLE>(-4)))
        SetProcessDPIAware();
    glutInit(&argc, argv);
    // Immediate-mode 3D and stroke text require a compatibility context, deliberately not core.
    glutInitContextVersion(3, 0);
    glutInitDisplayMode(GLUT_DOUBLE | GLUT_RGBA | GLUT_DEPTH | GLUT_MULTISAMPLE);
    RECT work{};
    SystemParametersInfoW(SPI_GETWORKAREA, 0, &work, 0);
    int initialW = std::max(640, std::min(1920, int(work.right - work.left) - 80));
    int initialH = std::max(480, std::min(1080, int(work.bottom - work.top) - 100));
    glutInitWindowSize(initialW, initialH);
    glutInitWindowPosition(work.left + 30, work.top + 30);
    int window = glutCreateWindow("G&K RPG Level One");
    if (window <= 0)
        return 1;
    SetWindowTextW(WindowFromDC(wglGetCurrentDC()), L"외곽 사냥터 | RPG 첫 레벨");
    glutSetOption(GLUT_ACTION_ON_WINDOW_CLOSE, GLUT_ACTION_GLUTMAINLOOP_RETURNS);
    GLenum status = glewInit();
    if (status != GLEW_OK || !GLEW_VERSION_3_0)
    {
        std::fprintf(stderr,
                     "OpenGL 3.0 compatibility initialization failed: %s\n",
                     glewGetErrorString(status));
        glutDestroyWindow(window);
        return 1;
    }
    renderer = std::make_unique<Renderer>(initialW, initialH);
    glutDisplayFunc(Display);
    glutReshapeFunc(Resize);
    glutMouseFunc(Mouse);
    glutPassiveMotionFunc(Motion);
    glutMotionFunc(Motion);
    glutKeyboardFunc(Key);
    glutKeyboardUpFunc(KeyUp);
    glutEntryFunc(Entry);
    glutSpecialFunc(Special);
    glutCloseFunc(Close);
    glutIgnoreKeyRepeat(1);
    lastTick = glutGet(GLUT_ELAPSED_TIME);
    glutTimerFunc(16, Tick, 0);
    glutMainLoop();
    renderer.reset();
    return 0;
}
