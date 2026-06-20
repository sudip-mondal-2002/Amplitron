#pragma once

#include <string>

namespace Amplitron {

// Runtime-editable effect parameter exposed to the UI and preset system.
struct EffectParam {
    std::string name;
    float value = 0.0f;
    float min_val = 0.0f;
    float max_val = 0.0f;
    float default_val = 0.0f;
    std::string unit;
    std::string tooltip;
};

}  // namespace Amplitron
