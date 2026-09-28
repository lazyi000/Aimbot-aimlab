#include "color_sample.h"

#include <algorithm>
#include <cstdint>

namespace aimlab {

RGB SampleAtCenter(const ScreenFrame& frame, int centerX, int centerY, int radius) {
    RGB out{0, 0, 0};
    if (!frame.valid() || radius < 0) return out;

    const int x0 = std::max(0, centerX - radius);
    const int y0 = std::max(0, centerY - radius);
    const int x1 = std::min(frame.width  - 1, centerX + radius);
    const int y1 = std::min(frame.height - 1, centerY + radius);
    if (x1 < x0 || y1 < y0) return out;

    long long sr = 0, sg = 0, sb = 0, n = 0;
    for (int y = y0; y <= y1; ++y) {
        const std::uint8_t* row = frame.pixels.data()
                                + static_cast<std::size_t>(y) * frame.width * 4;
        for (int x = x0; x <= x1; ++x) {
            sr += row[x * 4 + 2];  // BGRA: r is at offset 2
            sg += row[x * 4 + 1];
            sb += row[x * 4 + 0];
            ++n;
        }
    }
    if (n == 0) return out;
    out.r = static_cast<int>(sr / n);
    out.g = static_cast<int>(sg / n);
    out.b = static_cast<int>(sb / n);
    return out;
}

}  // namespace aimlab
