#pragma once

#include <cstdint>
#include "screen_capture.h"

namespace aimlab {

struct RGB { int r = 0, g = 0, b = 0; };

// Averages the BGRA pixels in a square box of (2*radius+1) px centered on
// (centerX, centerY). Returns {0,0,0} if the frame is empty or the box is
// entirely off-screen.
RGB SampleAtCenter(const ScreenFrame& frame, int centerX, int centerY, int radius);

}  // namespace aimlab
