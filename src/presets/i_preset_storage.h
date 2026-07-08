#pragma once

#include <string>
#include <vector>

namespace Amplitron {

/**
 * @brief Interface for preset persistence storage.
 * Satisfies Single Responsibility Principle (SRP) and Dependency Inversion Principle (DIP).
 */
class IPresetStorage {
   public:
    virtual ~IPresetStorage() = default;
    virtual bool save(const std::string& filepath, const std::string& data) = 0;
    virtual std::string load(const std::string& filepath) = 0;
    virtual std::vector<std::string> list() = 0;
    virtual bool remove(const std::string& filepath) = 0;
};

}  // namespace Amplitron
