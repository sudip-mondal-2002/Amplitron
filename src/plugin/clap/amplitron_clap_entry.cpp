#include <clap/clap.h>

#include <cstring>

#include "plugin/clap/amplitron_clap_plugin.h"

namespace {

bool CLAP_ABI entry_init(const char*) { return true; }

void CLAP_ABI entry_deinit() {}

uint32_t CLAP_ABI factory_get_plugin_count(const clap_plugin_factory_t*) { return 1; }

const clap_plugin_descriptor_t* CLAP_ABI factory_get_plugin_descriptor(const clap_plugin_factory_t*,
                                                                       uint32_t index) {
    if (index != 0) {
        return nullptr;
    }

    return Amplitron::get_amplitron_clap_descriptor();
}

const clap_plugin_t* CLAP_ABI factory_create_plugin(const clap_plugin_factory_t*,
                                                    const clap_host_t* host,
                                                    const char* plugin_id) {
    const clap_plugin_descriptor_t* descriptor = Amplitron::get_amplitron_clap_descriptor();

    if (plugin_id == nullptr || std::strcmp(plugin_id, descriptor->id) != 0) {
        return nullptr;
    }

    return Amplitron::create_amplitron_clap_plugin(host);
}

const clap_plugin_factory_t kPluginFactory = {factory_get_plugin_count,
                                              factory_get_plugin_descriptor, factory_create_plugin};

const void* CLAP_ABI entry_get_factory(const char* factory_id) {
    if (factory_id == nullptr) {
        return nullptr;
    }

    if (std::strcmp(factory_id, CLAP_PLUGIN_FACTORY_ID) == 0) {
        return &kPluginFactory;
    }

    return nullptr;
}

}  // namespace

extern "C" {

CLAP_EXPORT extern const clap_plugin_entry_t clap_entry = {CLAP_VERSION, entry_init, entry_deinit,
                                                           entry_get_factory};
}
