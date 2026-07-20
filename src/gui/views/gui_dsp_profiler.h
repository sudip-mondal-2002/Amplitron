#pragma once

#include "audio/engine/dsp_performance_profiler.h"

namespace Amplitron {

class GuiDspProfiler {
   public:
    void render(bool* open, const DspProfilerSnapshot& snapshot);
};

}  // namespace Amplitron