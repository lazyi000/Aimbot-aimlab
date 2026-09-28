#include "aim_worker.h"

#include <chrono>
#include <cmath>
#include <cstdio>

#include "../capture/color_match.h"
#include "../input/mouse_input.h"
#include "../capture/screen_capture.h"
#include "../ui/window_picker.h"

namespace aimlab {

namespace {

inline int sqr(int v) { return v * v; }
inline int dist2(int x1, int y1, int x2, int y2) {
    int dx = x1 - x2, dy = y1 - y2;
    return dx * dx + dy * dy;
}

}  // namespace

AimWorker::AimWorker(ConfigStore& cfg) : cfg_(cfg) {}

AimWorker::~AimWorker() { stop(); }

void AimWorker::start() {
    if (running.load()) return;
    stop_.store(false);
    running.store(true);
    thread_ = std::thread([this] { loop(); });
}

void AimWorker::stop() {
    if (!running.load()) return;
    stop_.store(true);
    if (thread_.joinable()) thread_.join();
    running.store(false);
}

void AimWorker::loop() {
    using clock = std::chrono::steady_clock;

    auto     lastFpsTime  = clock::now();
    long long fpsFrames   = 0;

    const auto screenSize = GetPrimaryScreenSize();

    // Cached identity of the target window (re-resolved each frame
    // because HWNDs become stale when windows close).
    DWORD   lastTgtPid    = 0;
    wchar_t lastTgtTitle[256]{};
    wchar_t lastTgtClass[128]{};
    HWND    cachedHwnd    = nullptr;

    bool       lastInside  = false;  // debounce for auto-click
    auto       lastClickAt = clock::time_point{};
    constexpr int clickCooldownMs = 150;

    // Sticky-target lock state (frame-local coords). -1, -1 = no lock.
    int        lockLocalX  = -1;
    int        lockLocalY  = -1;
    int        lockMisses  = 0;

    // Persistent lock position in SCREEN coords - used to compute the
    // capture region (adaptive) so we don't have to scan the whole screen
    // every frame. -1 = no lock, do full-screen capture.
    int        lockScreenX = -1;
    int        lockScreenY = -1;

    while (!stop_.load()) {
        ++frames;
        ++fpsFrames;

        Settings s;
        cfg_.copyInto(s);

        // Resolve the per-frame anchor and capture source.
        //  anchorCx/anchorCy = the crosshair position in SCREEN coords
        //  (either the screen center, or the target window's center).
        int  anchorCx = screenSize.width  / 2;
        int  anchorCy = screenSize.height / 2;
        int  origX = 0, origY = 0;
        ScreenFrame frame;

        if (s.scope == WindowScope::SpecificWindow) {
            // Re-resolve the HWND if identity changed.
            bool identityChanged =
                s.targetWindowPid != static_cast<int>(lastTgtPid) ||
                wcscmp(s.targetWindowTitle, lastTgtTitle) != 0 ||
                wcscmp(s.targetWindowClass, lastTgtClass) != 0;

            if (identityChanged) {
                lastTgtPid   = static_cast<DWORD>(s.targetWindowPid);
                wcsncpy(lastTgtTitle, s.targetWindowTitle, 255);
                lastTgtTitle[255] = L'\0';
                wcsncpy(lastTgtClass, s.targetWindowClass, 127);
                lastTgtClass[127] = L'\0';
                cachedHwnd   = ResolveWindowHandle(lastTgtPid, lastTgtTitle, lastTgtClass);
            }

            if (cachedHwnd && IsWindow(cachedHwnd)) {
                // Refresh the rect each frame (windows move / resize).
                RECT rc{};
                if (GetWindowRect(cachedHwnd, &rc)) {
                    const int w = rc.right  - rc.left;
                    const int h = rc.bottom - rc.top;
                    if (w > 0 && h > 0) {
                        anchorCx = rc.left + w / 2;
                        anchorCy = rc.top  + h / 2;
                        targetWindowW.store(w);
                        targetWindowH.store(h);
                        targetWindowFound.store(true);

                        frame = CaptureWindow(cachedHwnd, origX, origY);
                    }
                }
            }

            if (!targetWindowFound.load()) {
                hasLastTarget.store(false);
                lastCenterMatched.store(false);
                std::this_thread::sleep_for(std::chrono::milliseconds(s.frameDelayMs));
                goto fpsTick;
            }
        } else {
            targetWindowFound.store(false);

            // Adaptive capture: when we have a lock, capture a (searchR x
            // searchR) box around it instead of the full screen. This is
            // the biggest single perf win - at 1920x1080 we go from ~2M
            // pixels/frame to ~640K (for searchR=800).
            if (lockScreenX >= 0) {
                int sr = s.searchRegionPx;
                if (sr < 64) sr = 64;
                int rx = lockScreenX - sr / 2;
                int ry = lockScreenY - sr / 2;
                int rw = sr, rh = sr;

                // Clip to the primary monitor.
                if (rx < 0)               { rw += rx; rx = 0; }
                if (ry < 0)               { rh += ry; ry = 0; }
                if (rx + rw > screenSize.width)  rw = screenSize.width  - rx;
                if (ry + rh > screenSize.height) rh = screenSize.height - ry;
                if (rw >= 32 && rh >= 32) {
                    frame = CaptureRegion(rx, ry, rw, rh);
                    // origX / origY are set inside CaptureRegion.
                } else {
                    frame = CapturePrimaryScreen();
                }
            } else {
                frame = CapturePrimaryScreen();
            }
        }

        if (!frame.valid()) {
            ++captureErrors;
            std::this_thread::sleep_for(std::chrono::milliseconds(s.frameDelayMs));
        } else {
            // Detect target, if any.  FindTarget works in frame-local
            // coordinates - we feed it the frame's center (which for the
            // window scope is the window's center inside the frame).
            const int localCx = anchorCx - origX;
            const int localCy = anchorCy - origY;

            // Pass the current lock as a sticky hint when sticky aim is on.
            const int hintX = (s.stickyTarget && lockLocalX >= 0) ? lockLocalX : -1;
            const int hintY = (s.stickyTarget && lockLocalY >= 0) ? lockLocalY : -1;
            TargetMatch m = FindTarget(frame, localCx, localCy,
                                       s.targetR, s.targetG, s.targetB,
                                       s.colorTolerance, s.minPixelNeighbors,
                                       s.clusterRadiusPx, s.minClusterSize,
                                       hintX, hintY, s.stickRadiusPx);

            // ---- Sticky-target state machine ----
            // m is the FRESH detection (closest-to-anchor blob, or the
            // sticky hint if it was in range). The lock is the target the
            // worker is COMMITTED to. The rest of the loop uses the lock.
            if (m.found) {
                if (lockLocalX < 0) {
                    // No lock yet -> adopt the fresh hit.
                    lockLocalX = m.x;
                    lockLocalY = m.y;
                } else {
                    int dx = m.x - lockLocalX;
                    int dy = m.y - lockLocalY;
                    int d2 = dx * dx + dy * dy;
                    int stickR2 = s.stickRadiusPx * s.stickRadiusPx;

                    if (d2 <= stickR2) {
                        // m is the same blob (slightly moved) -> update lock.
                        lockLocalX = m.x;
                        lockLocalY = m.y;
                    } else if (s.stickyTarget) {
                        // m is a different blob and we are sticky. Only
                        // switch if the new candidate is dramatically
                        // closer to the anchor than the current lock.
                        int lockDx = lockLocalX - localCx;
                        int lockDy = lockLocalY - localCy;
                        int lockD2 = lockDx * lockDx + lockDy * lockDy;
                        int newDx  = m.x - localCx;
                        int newDy  = m.y - localCy;
                        int newD2  = newDx * newDx + newDy * newDy;

                        // ratio in [1, 100]: 100 = always switch,
                        // 50 = new must be 2x closer, 10 = 10x closer.
                        int ratio = s.stickSwitchRatio;
                        if (ratio < 1)   ratio = 1;
                        if (ratio > 100) ratio = 100;
                        // Acceptable new distance = lockD2 * (100 / ratio)
                        long long allowedNewD2 =
                            (long long)lockD2 * (long long)(100 - ratio);
                        // allowedNewD2 is in the same units as newD2.

                        if (lockD2 == 0 ||
                            (long long)newD2 <= allowedNewD2) {
                            lockLocalX = m.x;
                            lockLocalY = m.y;
                        }
                        // else: keep the current lock.
                    } else {
                        // Sticky disabled -> always follow the fresh hit.
                        lockLocalX = m.x;
                        lockLocalY = m.y;
                    }
                }
                lockMisses = 0;
                hasLock.store(true);
                lockScreenX = lockLocalX + origX;
                lockScreenY = lockLocalY + origY;
                lockX.store(lockScreenX);
                lockY.store(lockScreenY);
            } else {
                // Fresh detection missed. Drop the lock after a few frames
                // so the aimbot doesn't keep chasing a dead target forever.
                ++lockMisses;
                if (lockMisses > s.stickLockMaxMisses) {
                    lockLocalX  = lockLocalY  = -1;
                    lockScreenX = lockScreenY = -1;
                    hasLock.store(false);
                }
            }

            // The "effective" target the rest of this frame uses.
            const bool haveEffective =
                (lockLocalX >= 0) ||
                (m.found && (!s.stickyTarget || lockLocalX >= 0));
            if (haveEffective && lockLocalX >= 0) {
                hasLastTarget.store(true);
                lastTargetX.store(lockLocalX + origX);
                lastTargetY.store(lockLocalY + origY);
                // "size" for display = m.size if we have a fresh hit, else 0
                lastTargetSize.store(m.size);
                lastTargetDistPx.store(static_cast<int>(
                    std::sqrt(static_cast<double>(dist2(lockLocalX + origX,
                                                        lockLocalY + origY,
                                                        anchorCx, anchorCy)))));
            } else if (m.found) {
                hasLastTarget.store(true);
                lastTargetX.store(m.x + origX);
                lastTargetY.store(m.y + origY);
                lastTargetSize.store(m.size);
                lastTargetDistPx.store(static_cast<int>(
                    std::sqrt(static_cast<double>(dist2(m.x + origX, m.y + origY,
                                                        anchorCx, anchorCy)))));
            } else {
                hasLastTarget.store(false);
            }

            // Center-pixel check (purely informational: shows in the GUI).
            if (localCx >= 0 && localCx < frame.width &&
                localCy >= 0 && localCy < frame.height) {
                const std::uint8_t* cp = frame.pixels.data()
                                       + (static_cast<std::size_t>(localCy) * frame.width + localCx) * 4;
                int dr = static_cast<int>(cp[2]) - s.targetR;
                int dg = static_cast<int>(cp[1]) - s.targetG;
                int db = static_cast<int>(cp[0]) - s.targetB;
                int tol2 = sqr(s.colorTolerance) * 3;
                bool centerMatches = (dr * dr + dg * dg + db * db) <= tol2;
                lastCenterMatched.store(centerMatches);
            } else {
                lastCenterMatched.store(false);
            }

            const bool active = cfg_.aimEnabled();

            // Auto-click: trigger when the FRESH detection is close enough
            // to the anchor (= the in-game crosshair). Uses the SCREEN
            // coords of both the target and the anchor.
            if (active && s.autoClickEnabled && m.found) {
                int tx = m.x + origX;
                int ty = m.y + origY;
                int dxT = tx - anchorCx;
                int dyT = ty - anchorCy;
                int d2T = dxT * dxT + dyT * dyT;
                int reach = s.autoClickDistancePx;
                bool closeEnough = (d2T <= reach * reach);

                if (closeEnough) {
                    auto now = clock::now();
                    if (!lastInside ||
                        std::chrono::duration_cast<std::chrono::milliseconds>(
                            now - lastClickAt).count() >= clickCooldownMs) {
                        ClickLeft(s.inputMethod == 1
                                  ? InputMethod::SendInput
                                  : InputMethod::MouseEvent);
                        ++clicks;
                        lastClickAt = now;
                    }
                    lastInside = true;
                } else {
                    lastInside = false;
                }
            } else {
                lastInside = false;
            }

            // Aim: only if enabled and a fresh detection exists THIS frame.
            // We aim at the fresh hit (m), NOT at the sticky lock - the lock
            // is just a hint to FindTarget so it picks the same blob. Aiming
            // at a stale lock would push the crosshair toward a dead target.
            if (active && m.found) {
                int ddx = (m.x + origX) - anchorCx;
                int ddy = (m.y + origY) - anchorCy;
                double dist = std::sqrt(static_cast<double>(ddx * ddx + ddy * ddy));

                if (dist > static_cast<double>(s.aimDeadzonePx)) {
                    double power = static_cast<double>(s.aimPower) / 100.0;
                    int    cap   = s.aimMaxSpeed;

                    int moveX = ddx, moveY = ddy;

                    // Snap close: when within ~2x cap, move the full delta
                    // (no slow-down at the end of the approach).
                    bool close = s.aimSnapClose && (dist <= 2.0 * cap);
                    if (!close && dist > cap) {
                        double r = cap / dist;
                        moveX = static_cast<int>(ddx * r);
                        moveY = static_cast<int>(ddy * r);
                    }

                    if (power != 1.0) {
                        moveX = static_cast<int>(moveX * power);
                        moveY = static_cast<int>(moveY * power);
                    }

                    if (moveX != 0 || moveY != 0) {
                        MoveRelative(moveX, moveY,
                                     s.inputMethod == 1
                                     ? InputMethod::SendInput
                                     : InputMethod::MouseEvent);
                    }
                }
            }

            std::this_thread::sleep_for(std::chrono::milliseconds(s.frameDelayMs));
        }

    fpsTick:
        // FPS update
        auto now = clock::now();
        double elapsed = std::chrono::duration<double>(now - lastFpsTime).count();
        if (elapsed >= 1.0) {
            fps.store(static_cast<int>(fpsFrames / elapsed));
            fpsFrames = 0;
            lastFpsTime = now;
        }
    }
}

}  // namespace aimlab
