#pragma once

#include <string>

namespace Amplitron {

/**
 * @brief Interface for migrating preset configurations across versions.
 * Satisfies Single Responsibility Principle (SRP) and Dependency Inversion Principle (DIP).
 */
class IPresetMigrator {
   public:
    virtual ~IPresetMigrator() = default;
    virtual std::string migrate(const std::string& raw_json) = 0;
};

}  // namespace Amplitron
