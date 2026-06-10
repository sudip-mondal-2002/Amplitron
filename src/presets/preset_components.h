#pragma once

#include "presets/i_preset_migrator.h"
#include "presets/i_preset_serializer.h"
#include "presets/i_preset_storage.h"

namespace Amplitron {

class PresetSerializer : public IPresetSerializer {
   public:
    PresetSerializer() = default;
    ~PresetSerializer() override = default;
    std::string serialize(const PresetData& preset) override;
    bool deserialize(const std::string& json_str, PresetData& preset) override;
};

class PresetStorage : public IPresetStorage {
   public:
    PresetStorage() = default;
    ~PresetStorage() override = default;
    bool save(const std::string& filepath, const std::string& data) override;
    std::string load(const std::string& filepath) override;
    std::vector<std::string> list() override;
    bool remove(const std::string& filepath) override;
};

class PresetMigrator : public IPresetMigrator {
   public:
    PresetMigrator() = default;
    ~PresetMigrator() override = default;
    std::string migrate(const std::string& raw_json) override;
};

}  // namespace Amplitron
