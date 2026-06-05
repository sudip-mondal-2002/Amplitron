#pragma once

#include <string>
#include <vector>

#include "audio/effects/core/effect_param.h"

namespace Amplitron {

class IParameterizable {
   public:
    virtual ~IParameterizable() = default;
    virtual std::vector<EffectParam>& params() = 0;
    virtual const std::vector<EffectParam>& params() const = 0;
    virtual std::vector<std::string> get_param_names() = 0;
    virtual float get_param_value(const std::string& name) = 0;
    virtual void set_param_by_name(const std::string& name, float value) = 0;
};

}  // namespace Amplitron
