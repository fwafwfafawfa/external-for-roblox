#pragma once

#include <stdint.h>

namespace hooks {
    void desync();
}

namespace desync_feature {
    void enable(bool on);
    void start_worker();
    void stop_worker();
}
