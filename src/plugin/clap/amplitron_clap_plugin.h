#pragma once

#include <clap/clap.h>

namespace Amplitron {

const clap_plugin_descriptor_t* get_amplitron_clap_descriptor();
const clap_plugin_t* create_amplitron_clap_plugin(const clap_host_t* host);

}  // namespace Amplitron
