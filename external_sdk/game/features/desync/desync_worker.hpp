#pragma once

#include <atomic>

namespace desync_worker {
    void start();
    void stop();
    bool running();
}
