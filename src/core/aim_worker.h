#pragma once

#include <atomic>
#include <thread>

#include "../config.h"

namespace aimlab {

// Background loop: capture -> detect -> aim -> click. Reads config from
// ConfigStore each frame (cheap shared copy). Exposes live stats for the
// GUI to display.
class AimWorker {
public:
    explicit AimWorker(ConfigStore& cfg);
    ~AimWorker();

    void start();
    void stop();

    std::atomic<bool>  running{false};
    std::atomic<int>   fps{0};
    std::atomic<long long> frames{0};
    std::atomic<long long> clicks{0};
    std::atomic<long long> captureErrors{0};

    std::atomic<bool>  hasLastTarget{false};
    std::atomic<int>   lastTargetX{0};
    std::atomic<int>   lastTargetY{0};
    std::atomic<int>   lastTargetSize{0};
    std::atomic<int>   lastTargetDistPx{0};

    std::atomic<bool>  lastCenterMatched{false};

    // Specific-window mode: whether the target HWND was resolved this frame.
    std::atomic<bool>  targetWindowFound{false};
    std::atomic<int>   targetWindowW{0};
    std::atomic<int>   targetWindowH{0};

    // Sticky target state. These are only touched by the worker thread
    // but exposed as atomics so the UI can read them for diagnostics.
    std::atomic<bool>  hasLock{false};
    std::atomic<int>   lockX{0};
    std::atomic<int>   lockY{0};

private:
    void loop();

    ConfigStore&         cfg_;
    std::thread          thread_;
    std::atomic<bool>    stop_{false};
};

}  // namespace aimlab
