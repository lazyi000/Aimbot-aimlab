#pragma once

namespace aimlab {

enum class InputMethod {
    MouseEvent = 0,
    SendInput  = 1,
};

void MoveRelative(int dx, int dy, InputMethod method);
void ClickLeft(InputMethod method);

}  // namespace aimlab
