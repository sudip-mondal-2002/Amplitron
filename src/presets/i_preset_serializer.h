#pragma once

#include <string>

namespace Amplitron {

struct PresetData;

/**
 * @brief Interface for serializing and deserializing preset configurations.
 * Satisfies Single Responsibility Principle (SRP) and Dependency Inversion Principle (DIP).
 */
class IPresetSerializer {
public:
    virtual ~IPresetSerializer() = default;
    virtual std::string serialize(const PresetData& preset) = 0;
    virtual bool deserialize(const std::string& json_str, PresetData& preset) = 0;
};

} // namespace Amplitron
