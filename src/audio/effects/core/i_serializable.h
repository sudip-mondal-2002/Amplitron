#pragma once

#include <nlohmann/json_fwd.hpp>

namespace Amplitron {

class ISerializable {
   public:
    virtual ~ISerializable() = default;
    virtual nlohmann::json get_params() const = 0;
    virtual void set_params(const nlohmann::json& j) = 0;
};

}  // namespace Amplitron
