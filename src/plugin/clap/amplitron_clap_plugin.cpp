#include "plugin/clap/amplitron_clap_plugin.h"

#include <clap/clap.h>

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <string>
#include <vector>

#include "plugin/amplitron_plugin_processor.h"

namespace Amplitron {
namespace {

constexpr const char* kPluginId = "org.amplitron.clap";

struct AmplitronClapPlugin {
    clap_plugin_t plugin{};
    const clap_host_t* host{};
    const clap_host_params_t* host_params{};
    AmplitronPluginProcessor processor;
    double sample_rate{48000.0};
    uint32_t max_block_size{512};
    bool active{false};
    bool processing{false};
};

AmplitronClapPlugin* from_plugin(const clap_plugin_t* plugin) {
    if (plugin == nullptr) {
        return nullptr;
    }

    return static_cast<AmplitronClapPlugin*>(plugin->plugin_data);
}

void copy_string(char* destination, uint32_t capacity, const std::string& value) {
    if (destination == nullptr || capacity == 0) {
        return;
    }

    std::snprintf(destination, capacity, "%s", value.c_str());
}

void apply_param_event(AmplitronClapPlugin* self, const clap_event_header_t* header) {
    if (self == nullptr || header == nullptr) {
        return;
    }

    if (header->space_id != CLAP_CORE_EVENT_SPACE_ID || header->type != CLAP_EVENT_PARAM_VALUE ||
        header->size < sizeof(clap_event_param_value_t)) {
        return;
    }

    const auto* event = reinterpret_cast<const clap_event_param_value_t*>(header);
    const int32_t index = self->processor.parameter_index_for_id(event->param_id);

    if (index >= 0) {
        self->processor.set_parameter_plain(static_cast<uint32_t>(index),
                                            static_cast<float>(event->value));
    }
}

void apply_input_events(AmplitronClapPlugin* self, const clap_input_events_t* events) {
    if (self == nullptr || events == nullptr || events->size == nullptr || events->get == nullptr) {
        return;
    }

    const uint32_t event_count = events->size(events);

    for (uint32_t index = 0; index < event_count; ++index) {
        apply_param_event(self, events->get(events, index));
    }
}

void emit_current_parameter_values(AmplitronClapPlugin* self, const clap_output_events_t* events) {
    if (self == nullptr || events == nullptr || events->try_push == nullptr) {
        return;
    }

    const uint32_t parameter_count = self->processor.parameter_count();

    for (uint32_t index = 0; index < parameter_count; ++index) {
        const auto info = self->processor.parameter_info(index);

        clap_event_param_value_t event{};
        event.header.size = sizeof(event);
        event.header.time = 0;
        event.header.space_id = CLAP_CORE_EVENT_SPACE_ID;
        event.header.type = CLAP_EVENT_PARAM_VALUE;
        event.header.flags = 0;
        event.param_id = info.id;
        event.cookie = nullptr;
        event.note_id = -1;
        event.port_index = -1;
        event.channel = -1;
        event.key = -1;
        event.value = self->processor.get_parameter_plain(index);

        events->try_push(events, &event.header);
    }
}

bool CLAP_ABI plugin_init(const clap_plugin_t* plugin) {
    auto* self = from_plugin(plugin);

    if (self == nullptr) {
        return false;
    }

    if (self->host != nullptr && self->host->get_extension != nullptr) {
        self->host_params = static_cast<const clap_host_params_t*>(
            self->host->get_extension(self->host, CLAP_EXT_PARAMS));
    }

    self->processor.prepare(self->sample_rate, self->max_block_size);
    return true;
}

void CLAP_ABI plugin_destroy(const clap_plugin_t* plugin) { delete from_plugin(plugin); }

bool CLAP_ABI plugin_activate(const clap_plugin_t* plugin, double sample_rate, uint32_t,
                              uint32_t max_frames_count) {
    auto* self = from_plugin(plugin);

    if (self == nullptr) {
        return false;
    }

    self->sample_rate = sample_rate > 0.0 ? sample_rate : 48000.0;
    self->max_block_size = max_frames_count > 0 ? max_frames_count : 512;
    self->processor.prepare(self->sample_rate, self->max_block_size);
    self->active = true;

    return true;
}

void CLAP_ABI plugin_deactivate(const clap_plugin_t* plugin) {
    auto* self = from_plugin(plugin);

    if (self == nullptr) {
        return;
    }

    self->processing = false;
    self->active = false;
}

bool CLAP_ABI plugin_start_processing(const clap_plugin_t* plugin) {
    auto* self = from_plugin(plugin);

    if (self == nullptr || !self->active) {
        return false;
    }

    self->processing = true;
    return true;
}

void CLAP_ABI plugin_stop_processing(const clap_plugin_t* plugin) {
    auto* self = from_plugin(plugin);

    if (self == nullptr) {
        return;
    }

    self->processing = false;
}

void CLAP_ABI plugin_reset(const clap_plugin_t* plugin) {
    auto* self = from_plugin(plugin);

    if (self == nullptr) {
        return;
    }

    self->processor.reset();
}

clap_process_status CLAP_ABI plugin_process(const clap_plugin_t* plugin,
                                            const clap_process_t* process) {
    auto* self = from_plugin(plugin);

    if (self == nullptr || process == nullptr) {
        return CLAP_PROCESS_ERROR;
    }

    apply_input_events(self, process->in_events);

    if (process->frames_count == 0 || process->audio_outputs == nullptr ||
        process->audio_outputs_count == 0) {
        return CLAP_PROCESS_CONTINUE;
    }

    const clap_audio_buffer_t* input_buffer =
        process->audio_inputs_count > 0 ? &process->audio_inputs[0] : nullptr;

    clap_audio_buffer_t* output_buffer = &process->audio_outputs[0];

    if (output_buffer->data32 == nullptr || output_buffer->channel_count == 0) {
        return CLAP_PROCESS_CONTINUE;
    }

    const uint32_t input_channels = input_buffer != nullptr && input_buffer->data32 != nullptr
                                        ? std::min<uint32_t>(input_buffer->channel_count, 2)
                                        : 0;

    const uint32_t output_channels = std::min<uint32_t>(output_buffer->channel_count, 2);

    const float* input_channel_data[2] = {nullptr, nullptr};
    float* output_channel_data[2] = {nullptr, nullptr};

    for (uint32_t channel = 0; channel < input_channels; ++channel) {
        input_channel_data[channel] = input_buffer->data32[channel];
    }

    for (uint32_t channel = 0; channel < output_channels; ++channel) {
        output_channel_data[channel] = output_buffer->data32[channel];
    }

    self->processor.process(input_channels > 0 ? input_channel_data : nullptr, output_channel_data,
                            process->frames_count, input_channels, output_channels);

    return CLAP_PROCESS_CONTINUE;
}

uint32_t CLAP_ABI params_count(const clap_plugin_t* plugin) {
    auto* self = from_plugin(plugin);

    if (self == nullptr) {
        return 0;
    }

    return self->processor.parameter_count();
}

bool CLAP_ABI params_get_info(const clap_plugin_t* plugin, uint32_t param_index,
                              clap_param_info_t* param_info) {
    auto* self = from_plugin(plugin);

    if (self == nullptr || param_info == nullptr) {
        return false;
    }

    if (param_index >= self->processor.parameter_count()) {
        return false;
    }

    const auto info = self->processor.parameter_info(param_index);

    param_info->id = info.id;
    param_info->flags = CLAP_PARAM_IS_AUTOMATABLE | CLAP_PARAM_REQUIRES_PROCESS;

    if (info.name == "Bypass") {
        param_info->flags |= CLAP_PARAM_IS_BYPASS | CLAP_PARAM_IS_STEPPED;
    }

    param_info->cookie = nullptr;
    copy_string(param_info->name, sizeof(param_info->name), info.name);
    copy_string(param_info->module, sizeof(param_info->module), info.module);
    param_info->min_value = info.min_value;
    param_info->max_value = info.max_value;
    param_info->default_value = info.default_value;

    return true;
}

bool CLAP_ABI params_get_value(const clap_plugin_t* plugin, clap_id param_id, double* out_value) {
    auto* self = from_plugin(plugin);

    if (self == nullptr || out_value == nullptr) {
        return false;
    }

    const int32_t index = self->processor.parameter_index_for_id(param_id);

    if (index < 0) {
        return false;
    }

    *out_value = self->processor.get_parameter_plain(static_cast<uint32_t>(index));
    return true;
}

bool CLAP_ABI params_value_to_text(const clap_plugin_t*, clap_id, double value, char* out_buffer,
                                   uint32_t out_buffer_capacity) {
    if (out_buffer == nullptr || out_buffer_capacity == 0) {
        return false;
    }

    std::snprintf(out_buffer, out_buffer_capacity, "%.3f", value);
    return true;
}

bool CLAP_ABI params_text_to_value(const clap_plugin_t*, clap_id, const char* param_value_text,
                                   double* out_value) {
    if (param_value_text == nullptr || out_value == nullptr) {
        return false;
    }

    char* end = nullptr;
    const double value = std::strtod(param_value_text, &end);

    if (end == param_value_text) {
        return false;
    }

    *out_value = value;
    return true;
}

void CLAP_ABI params_flush(const clap_plugin_t* plugin, const clap_input_events_t* in,
                           const clap_output_events_t* out) {
    auto* self = from_plugin(plugin);

    apply_input_events(self, in);
    emit_current_parameter_values(self, out);
}

uint32_t CLAP_ABI audio_ports_count(const clap_plugin_t*, bool) { return 1; }

bool CLAP_ABI audio_ports_get(const clap_plugin_t*, uint32_t index, bool is_input,
                              clap_audio_port_info_t* info) {
    if (info == nullptr || index != 0) {
        return false;
    }

    info->id = is_input ? 0 : 1;
    std::snprintf(info->name, sizeof(info->name), "%s", is_input ? "Input" : "Output");
    info->flags = CLAP_AUDIO_PORT_IS_MAIN;
    info->channel_count = 2;
    info->port_type = CLAP_PORT_STEREO;
    info->in_place_pair = CLAP_INVALID_ID;

    return true;
}

bool CLAP_ABI state_save(const clap_plugin_t* plugin, const clap_ostream_t* stream) {
    try {
        auto* self = from_plugin(plugin);

        if (self == nullptr || stream == nullptr || stream->write == nullptr) {
            return false;
        }

        const std::string state = self->processor.save_state_json();
        uint64_t written_total = 0;

        while (written_total < state.size()) {
            const int64_t written =
                stream->write(stream, state.data() + written_total, state.size() - written_total);

            if (written <= 0) {
                return false;
            }

            written_total += static_cast<uint64_t>(written);
        }

        return true;
    } catch (...) {
        return false;
    }
}

bool CLAP_ABI state_load(const clap_plugin_t* plugin, const clap_istream_t* stream) {
    auto* self = from_plugin(plugin);

    if (self == nullptr || stream == nullptr || stream->read == nullptr) {
        return false;
    }

    std::string state;
    char buffer[4096];

    while (true) {
        const int64_t bytes_read = stream->read(stream, buffer, sizeof(buffer));

        if (bytes_read < 0) {
            return false;
        }

        if (bytes_read == 0) {
            break;
        }

        state.append(buffer, static_cast<size_t>(bytes_read));
    }

    const bool loaded = self->processor.load_state_json(state);

    if (loaded && self->host_params != nullptr && self->host_params->request_flush != nullptr) {
        self->host_params->request_flush(self->host);
    }

    return loaded;
}

const clap_plugin_params_t kParamsExtension = {params_count,         params_get_info,
                                               params_get_value,     params_value_to_text,
                                               params_text_to_value, params_flush};

const clap_plugin_audio_ports_t kAudioPortsExtension = {audio_ports_count, audio_ports_get};

const clap_plugin_state_t kStateExtension = {state_save, state_load};

const void* CLAP_ABI plugin_get_extension(const clap_plugin_t*, const char* id) {
    if (id == nullptr) {
        return nullptr;
    }

    if (std::strcmp(id, CLAP_EXT_PARAMS) == 0) {
        return &kParamsExtension;
    }

    if (std::strcmp(id, CLAP_EXT_AUDIO_PORTS) == 0) {
        return &kAudioPortsExtension;
    }

    if (std::strcmp(id, CLAP_EXT_STATE) == 0) {
        return &kStateExtension;
    }

    return nullptr;
}

void CLAP_ABI plugin_on_main_thread(const clap_plugin_t*) {}

const char* const kFeatures[] = {CLAP_PLUGIN_FEATURE_AUDIO_EFFECT,
                                 CLAP_PLUGIN_FEATURE_STEREO,
                                 CLAP_PLUGIN_FEATURE_DISTORTION,
                                 CLAP_PLUGIN_FEATURE_DELAY,
                                 CLAP_PLUGIN_FEATURE_REVERB,
                                 CLAP_PLUGIN_FEATURE_MULTI_EFFECTS,
                                 nullptr};

const clap_plugin_descriptor_t kDescriptor = {CLAP_VERSION,
                                              kPluginId,
                                              "Amplitron",
                                              "Amplitron",
                                              "",
                                              "",
                                              "",
                                              AMPLITRON_VERSION,
                                              "Guitar amp and effects processor",
                                              kFeatures};

}  // namespace

const clap_plugin_descriptor_t* get_amplitron_clap_descriptor() { return &kDescriptor; }

const clap_plugin_t* create_amplitron_clap_plugin(const clap_host_t* host) {
    try {
        auto instance = std::make_unique<AmplitronClapPlugin>();

        instance->host = host;
        instance->plugin.desc = &kDescriptor;
        instance->plugin.plugin_data = instance.get();
        instance->plugin.init = plugin_init;
        instance->plugin.destroy = plugin_destroy;
        instance->plugin.activate = plugin_activate;
        instance->plugin.deactivate = plugin_deactivate;
        instance->plugin.start_processing = plugin_start_processing;
        instance->plugin.stop_processing = plugin_stop_processing;
        instance->plugin.reset = plugin_reset;
        instance->plugin.process = plugin_process;
        instance->plugin.get_extension = plugin_get_extension;
        instance->plugin.on_main_thread = plugin_on_main_thread;

        return &instance.release()->plugin;
    } catch (...) {
        return nullptr;
    }
}

}  // namespace Amplitron
