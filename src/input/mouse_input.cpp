#include "mouse_input.h"

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

namespace aimlab {

namespace {

void MoveViaMouseEvent(int dx, int dy) {
    mouse_event(MOUSEEVENTF_MOVE,
                static_cast<DWORD>(dx),
                static_cast<DWORD>(dy),
                0, 0);
}

void ClickViaMouseEvent() {
    mouse_event(MOUSEEVENTF_LEFTDOWN, 0, 0, 0, 0);
    mouse_event(MOUSEEVENTF_LEFTUP,   0, 0, 0, 0);
}

void MoveViaSendInput(int dx, int dy) {
    INPUT in{};
    in.type        = INPUT_MOUSE;
    in.mi.dx       = dx;
    in.mi.dy       = dy;
    in.mi.mouseData = 0;
    in.mi.dwFlags   = MOUSEEVENTF_MOVE;
    in.mi.time      = 0;
    in.mi.dwExtraInfo = 0;
    SendInput(1, &in, sizeof(in));
}

void ClickViaSendInput() {
    INPUT down{};
    down.type          = INPUT_MOUSE;
    down.mi.dwFlags    = MOUSEEVENTF_LEFTDOWN;
    INPUT up{};
    up.type            = INPUT_MOUSE;
    up.mi.dwFlags      = MOUSEEVENTF_LEFTUP;
    INPUT both[2] = { down, up };
    SendInput(2, both, sizeof(INPUT));
}

}  // namespace

void MoveRelative(int dx, int dy, InputMethod method) {
    if (method == InputMethod::SendInput) MoveViaSendInput(dx, dy);
    else                                   MoveViaMouseEvent(dx, dy);
}

void ClickLeft(InputMethod method) {
    if (method == InputMethod::SendInput) ClickViaSendInput();
    else                                  ClickViaMouseEvent();
}

}  // namespace aimlab
