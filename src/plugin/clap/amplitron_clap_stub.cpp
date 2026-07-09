#include "plugin/amplitron_plugin_processor.h"

extern "C" {

#if defined(_WIN32)
__declspec(dllexport)
#endif
const char* amplitron_clap_plugin_name() {
    return "Amplitron";
}

#if defined(_WIN32)
__declspec(dllexport)
#endif
const char* amplitron_clap_plugin_version() {
    return AMPLITRON_VERSION;
}

#if defined(_WIN32)
__declspec(dllexport)
#endif
int amplitron_clap_smoke_test() {
    Amplitron::AmplitronPluginProcessor processor;
    processor.prepare(48000.0, 512);
    return processor.parameter_count() > 0 ? 0 : 1;
}

}
