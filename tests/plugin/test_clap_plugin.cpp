#include <clap/clap.h>
#include <gtest/gtest.h>

#include <algorithm>
#include <cstring>
#include <string>
#include <vector>

#include "plugin/clap/amplitron_clap_plugin.h"

TEST(ClapPlugin, DescriptorIsValid) {
    const clap_plugin_descriptor_t* descriptor = Amplitron::get_amplitron_clap_descriptor();

    ASSERT_NE(descriptor, nullptr);
    EXPECT_STREQ(descriptor->id, "org.amplitron.clap");
    EXPECT_STREQ(descriptor->name, "Amplitron");
    ASSERT_NE(descriptor->features, nullptr);
}

TEST(ClapPlugin, CreatesAndDestroysPlugin) {
    const clap_plugin_t* plugin = Amplitron::create_amplitron_clap_plugin(nullptr);

    ASSERT_NE(plugin, nullptr);
    ASSERT_NE(plugin->init, nullptr);
    ASSERT_NE(plugin->destroy, nullptr);

    EXPECT_TRUE(plugin->init(plugin));
    plugin->destroy(plugin);
}

TEST(ClapPlugin, ExposesParamsAudioPortsAndStateExtensions) {
    const clap_plugin_t* plugin = Amplitron::create_amplitron_clap_plugin(nullptr);

    ASSERT_NE(plugin, nullptr);
    ASSERT_TRUE(plugin->init(plugin));

    EXPECT_NE(plugin->get_extension(plugin, CLAP_EXT_PARAMS), nullptr);
    EXPECT_NE(plugin->get_extension(plugin, CLAP_EXT_AUDIO_PORTS), nullptr);
    EXPECT_NE(plugin->get_extension(plugin, CLAP_EXT_STATE), nullptr);

    plugin->destroy(plugin);
}

TEST(ClapPlugin, ExposesAutomatableParameters) {
    const clap_plugin_t* plugin = Amplitron::create_amplitron_clap_plugin(nullptr);

    ASSERT_NE(plugin, nullptr);
    ASSERT_TRUE(plugin->init(plugin));

    const auto* params =
        static_cast<const clap_plugin_params_t*>(plugin->get_extension(plugin, CLAP_EXT_PARAMS));

    ASSERT_NE(params, nullptr);
    ASSERT_GT(params->count(plugin), 0u);

    clap_param_info_t info{};
    ASSERT_TRUE(params->get_info(plugin, 0, &info));
    EXPECT_NE(info.flags & CLAP_PARAM_IS_AUTOMATABLE, 0u);

    double value = 0.0;
    EXPECT_TRUE(params->get_value(plugin, info.id, &value));

    plugin->destroy(plugin);
}

TEST(ClapPlugin, ProcessesStereoAudio) {
    const clap_plugin_t* plugin = Amplitron::create_amplitron_clap_plugin(nullptr);

    ASSERT_NE(plugin, nullptr);
    ASSERT_TRUE(plugin->init(plugin));
    ASSERT_TRUE(plugin->activate(plugin, 48000.0, 1, 128));
    ASSERT_TRUE(plugin->start_processing(plugin));

    std::vector<float> input_left(128, 0.25f);
    std::vector<float> input_right(128, -0.25f);
    std::vector<float> output_left(128, 0.0f);
    std::vector<float> output_right(128, 0.0f);

    float* input_channels[] = {input_left.data(), input_right.data()};
    float* output_channels[] = {output_left.data(), output_right.data()};

    clap_audio_buffer_t input_buffer{};
    input_buffer.data32 = input_channels;
    input_buffer.channel_count = 2;

    clap_audio_buffer_t output_buffer{};
    output_buffer.data32 = output_channels;
    output_buffer.channel_count = 2;

    clap_process_t process{};
    process.frames_count = 128;
    process.audio_inputs = &input_buffer;
    process.audio_inputs_count = 1;
    process.audio_outputs = &output_buffer;
    process.audio_outputs_count = 1;

    EXPECT_EQ(plugin->process(plugin, &process), CLAP_PROCESS_CONTINUE);
    EXPECT_FLOAT_EQ(output_left[0], 0.25f);
    EXPECT_FLOAT_EQ(output_right[0], -0.25f);

    plugin->stop_processing(plugin);
    plugin->deactivate(plugin);
    plugin->destroy(plugin);
}

extern "C" {
extern const clap_plugin_entry_t clap_entry;
}

namespace {

struct EventHarness {
    std::vector<clap_event_param_value_t> input_events;
    std::vector<clap_event_param_value_t> output_events;
    bool request_flush_called{false};
};

struct MemoryOutputStream {
    std::string data;
};

struct MemoryInputStream {
    const std::string* data{};
    uint64_t offset{};
};

uint32_t CLAP_ABI input_event_size(const clap_input_events_t* events) {
    const auto* harness = static_cast<const EventHarness*>(events->ctx);
    return static_cast<uint32_t>(harness->input_events.size());
}

const clap_event_header_t* CLAP_ABI input_event_get(const clap_input_events_t* events,
                                                    uint32_t index) {
    const auto* harness = static_cast<const EventHarness*>(events->ctx);

    if (index >= harness->input_events.size()) {
        return nullptr;
    }

    return &harness->input_events[index].header;
}

bool CLAP_ABI output_event_try_push(const clap_output_events_t* events,
                                    const clap_event_header_t* event) {
    if (events == nullptr || event == nullptr || event->type != CLAP_EVENT_PARAM_VALUE ||
        event->size < sizeof(clap_event_param_value_t)) {
        return false;
    }

    auto* harness = static_cast<EventHarness*>(events->ctx);
    harness->output_events.push_back(*reinterpret_cast<const clap_event_param_value_t*>(event));
    return true;
}

int64_t CLAP_ABI memory_stream_write(const clap_ostream_t* stream, const void* buffer,
                                     uint64_t size) {
    if (stream == nullptr || stream->ctx == nullptr || buffer == nullptr) {
        return -1;
    }

    auto* output = static_cast<MemoryOutputStream*>(stream->ctx);
    output->data.append(static_cast<const char*>(buffer), static_cast<size_t>(size));
    return static_cast<int64_t>(size);
}

int64_t CLAP_ABI memory_stream_read(const clap_istream_t* stream, void* buffer, uint64_t size) {
    if (stream == nullptr || stream->ctx == nullptr || buffer == nullptr) {
        return -1;
    }

    auto* input = static_cast<MemoryInputStream*>(stream->ctx);

    if (input->data == nullptr || input->offset >= input->data->size()) {
        return 0;
    }

    const uint64_t remaining = static_cast<uint64_t>(input->data->size()) - input->offset;
    const uint64_t to_read = std::min(size, remaining);

    std::memcpy(buffer, input->data->data() + static_cast<size_t>(input->offset),
                static_cast<size_t>(to_read));
    input->offset += to_read;

    return static_cast<int64_t>(to_read);
}

void CLAP_ABI host_params_rescan(const clap_host_t*, clap_param_rescan_flags) {}

void CLAP_ABI host_params_clear(const clap_host_t*, clap_id, clap_param_clear_flags) {}

void CLAP_ABI host_params_request_flush(const clap_host_t* host) {
    if (host == nullptr || host->host_data == nullptr) {
        return;
    }

    auto* harness = static_cast<EventHarness*>(host->host_data);
    harness->request_flush_called = true;
}

const void* CLAP_ABI host_get_extension(const clap_host_t*, const char* extension_id) {
    static const clap_host_params_t host_params = {
        host_params_rescan,
        host_params_clear,
        host_params_request_flush,
    };

    if (extension_id != nullptr && std::strcmp(extension_id, CLAP_EXT_PARAMS) == 0) {
        return &host_params;
    }

    return nullptr;
}

void CLAP_ABI host_request_restart(const clap_host_t*) {}
void CLAP_ABI host_request_process(const clap_host_t*) {}
void CLAP_ABI host_request_callback(const clap_host_t*) {}

clap_host_t make_test_host(EventHarness* harness) {
    clap_host_t host{};
    host.clap_version = CLAP_VERSION;
    host.host_data = harness;
    host.name = "Amplitron Test Host";
    host.vendor = "Amplitron";
    host.url = "";
    host.version = "1.0";
    host.get_extension = host_get_extension;
    host.request_restart = host_request_restart;
    host.request_process = host_request_process;
    host.request_callback = host_request_callback;
    return host;
}

clap_event_param_value_t make_param_event(clap_id id, double value) {
    clap_event_param_value_t event{};
    event.header.size = sizeof(event);
    event.header.time = 0;
    event.header.space_id = CLAP_CORE_EVENT_SPACE_ID;
    event.header.type = CLAP_EVENT_PARAM_VALUE;
    event.header.flags = 0;
    event.param_id = id;
    event.cookie = nullptr;
    event.note_id = -1;
    event.port_index = -1;
    event.channel = -1;
    event.key = -1;
    event.value = value;
    return event;
}

}  // namespace

TEST(ClapPlugin, EntryFactoryCreatesPlugin) {
    ASSERT_TRUE(clap_entry.init(""));

    const auto* factory =
        static_cast<const clap_plugin_factory_t*>(clap_entry.get_factory(CLAP_PLUGIN_FACTORY_ID));

    ASSERT_NE(factory, nullptr);
    ASSERT_EQ(factory->get_plugin_count(factory), 1u);

    const clap_plugin_descriptor_t* descriptor = factory->get_plugin_descriptor(factory, 0);

    ASSERT_NE(descriptor, nullptr);
    EXPECT_STREQ(descriptor->id, "org.amplitron.clap");
    EXPECT_EQ(factory->get_plugin_descriptor(factory, 1), nullptr);
    EXPECT_EQ(factory->create_plugin(factory, nullptr, "wrong.id"), nullptr);

    const clap_plugin_t* plugin = factory->create_plugin(factory, nullptr, descriptor->id);

    ASSERT_NE(plugin, nullptr);
    ASSERT_TRUE(plugin->init(plugin));
    plugin->destroy(plugin);

    clap_entry.deinit();
}

TEST(ClapPlugin, AudioPortsExposeStereoMainInputAndOutput) {
    const clap_plugin_t* plugin = Amplitron::create_amplitron_clap_plugin(nullptr);

    ASSERT_NE(plugin, nullptr);
    ASSERT_TRUE(plugin->init(plugin));

    const auto* audio_ports = static_cast<const clap_plugin_audio_ports_t*>(
        plugin->get_extension(plugin, CLAP_EXT_AUDIO_PORTS));

    ASSERT_NE(audio_ports, nullptr);
    EXPECT_EQ(audio_ports->count(plugin, true), 1u);
    EXPECT_EQ(audio_ports->count(plugin, false), 1u);

    clap_audio_port_info_t input_info{};
    clap_audio_port_info_t output_info{};

    ASSERT_TRUE(audio_ports->get(plugin, 0, true, &input_info));
    ASSERT_TRUE(audio_ports->get(plugin, 0, false, &output_info));
    EXPECT_EQ(input_info.channel_count, 2u);
    EXPECT_EQ(output_info.channel_count, 2u);
    EXPECT_NE(input_info.flags & CLAP_AUDIO_PORT_IS_MAIN, 0u);
    EXPECT_NE(output_info.flags & CLAP_AUDIO_PORT_IS_MAIN, 0u);
    EXPECT_FALSE(audio_ports->get(plugin, 1, true, &input_info));

    plugin->destroy(plugin);
}

TEST(ClapPlugin, ParamsFlushAppliesInputAndEmitsCurrentValues) {
    const clap_plugin_t* plugin = Amplitron::create_amplitron_clap_plugin(nullptr);

    ASSERT_NE(plugin, nullptr);
    ASSERT_TRUE(plugin->init(plugin));

    const auto* params =
        static_cast<const clap_plugin_params_t*>(plugin->get_extension(plugin, CLAP_EXT_PARAMS));

    ASSERT_NE(params, nullptr);

    clap_param_info_t info{};
    ASSERT_TRUE(params->get_info(plugin, 0, &info));

    EventHarness harness;
    harness.input_events.push_back(make_param_event(info.id, 12.0));

    clap_input_events_t input_events{};
    input_events.ctx = &harness;
    input_events.size = input_event_size;
    input_events.get = input_event_get;

    clap_output_events_t output_events{};
    output_events.ctx = &harness;
    output_events.try_push = output_event_try_push;

    params->flush(plugin, &input_events, &output_events);

    double value = 0.0;
    ASSERT_TRUE(params->get_value(plugin, info.id, &value));
    EXPECT_DOUBLE_EQ(value, 12.0);
    EXPECT_GE(harness.output_events.size(), params->count(plugin));

    plugin->destroy(plugin);
}

TEST(ClapPlugin, StateSaveLoadRoundTripsAndRequestsHostFlush) {
    EventHarness harness;
    clap_host_t host = make_test_host(&harness);

    const clap_plugin_t* plugin = Amplitron::create_amplitron_clap_plugin(&host);

    ASSERT_NE(plugin, nullptr);
    ASSERT_TRUE(plugin->init(plugin));

    const auto* params =
        static_cast<const clap_plugin_params_t*>(plugin->get_extension(plugin, CLAP_EXT_PARAMS));
    const auto* state =
        static_cast<const clap_plugin_state_t*>(plugin->get_extension(plugin, CLAP_EXT_STATE));

    ASSERT_NE(params, nullptr);
    ASSERT_NE(state, nullptr);

    clap_param_info_t info{};
    ASSERT_TRUE(params->get_info(plugin, 0, &info));

    EventHarness event_harness;
    event_harness.input_events.push_back(make_param_event(info.id, 8.0));

    clap_input_events_t input_events{};
    input_events.ctx = &event_harness;
    input_events.size = input_event_size;
    input_events.get = input_event_get;

    clap_output_events_t output_events{};
    output_events.ctx = &event_harness;
    output_events.try_push = output_event_try_push;

    params->flush(plugin, &input_events, &output_events);

    MemoryOutputStream output_memory;
    clap_ostream_t output_stream{};
    output_stream.ctx = &output_memory;
    output_stream.write = memory_stream_write;

    ASSERT_TRUE(state->save(plugin, &output_stream));
    ASSERT_FALSE(output_memory.data.empty());

    plugin->destroy(plugin);

    harness.request_flush_called = false;

    const clap_plugin_t* restored = Amplitron::create_amplitron_clap_plugin(&host);

    ASSERT_NE(restored, nullptr);
    ASSERT_TRUE(restored->init(restored));

    const auto* restored_params = static_cast<const clap_plugin_params_t*>(
        restored->get_extension(restored, CLAP_EXT_PARAMS));
    const auto* restored_state =
        static_cast<const clap_plugin_state_t*>(restored->get_extension(restored, CLAP_EXT_STATE));

    ASSERT_NE(restored_params, nullptr);
    ASSERT_NE(restored_state, nullptr);

    MemoryInputStream input_memory{&output_memory.data, 0};
    clap_istream_t input_stream{};
    input_stream.ctx = &input_memory;
    input_stream.read = memory_stream_read;

    ASSERT_TRUE(restored_state->load(restored, &input_stream));
    EXPECT_TRUE(harness.request_flush_called);

    double restored_value = 0.0;
    ASSERT_TRUE(restored_params->get_value(restored, info.id, &restored_value));
    EXPECT_DOUBLE_EQ(restored_value, 8.0);

    restored->destroy(restored);
}

TEST(ClapPlugin, StateLoadRejectsInvalidJson) {
    const clap_plugin_t* plugin = Amplitron::create_amplitron_clap_plugin(nullptr);

    ASSERT_NE(plugin, nullptr);
    ASSERT_TRUE(plugin->init(plugin));

    const auto* state =
        static_cast<const clap_plugin_state_t*>(plugin->get_extension(plugin, CLAP_EXT_STATE));

    ASSERT_NE(state, nullptr);

    const std::string invalid_state = "not json";
    MemoryInputStream input_memory{&invalid_state, 0};
    clap_istream_t input_stream{};
    input_stream.ctx = &input_memory;
    input_stream.read = memory_stream_read;

    EXPECT_FALSE(state->load(plugin, &input_stream));

    plugin->destroy(plugin);
}

// Extra coverage for CLAP guard paths
namespace {

struct NullInputEventList {
    clap_input_events_t events{};

    NullInputEventList() {
        events.ctx = this;
        events.size = size;
        events.get = get;
    }

    static uint32_t CLAP_ABI size(const clap_input_events_t*) { return 1; }

    static const clap_event_header_t* CLAP_ABI get(const clap_input_events_t*, uint32_t) {
        return nullptr;
    }
};

struct InvalidInputEventList {
    clap_input_events_t events{};
    clap_event_header_t header{};

    InvalidInputEventList() {
        header.size = sizeof(header);
        header.space_id = 0;
        header.type = 0;

        events.ctx = this;
        events.size = size;
        events.get = get;
    }

    static uint32_t CLAP_ABI size(const clap_input_events_t*) { return 1; }

    static const clap_event_header_t* CLAP_ABI get(const clap_input_events_t* list, uint32_t) {
        return &static_cast<const InvalidInputEventList*>(list->ctx)->header;
    }
};

struct ZeroWriteStream {
    clap_ostream_t stream{};

    ZeroWriteStream() {
        stream.ctx = this;
        stream.write = write;
    }

    static int64_t CLAP_ABI write(const clap_ostream_t*, const void*, uint64_t) { return 0; }
};

struct NegativeReadStream {
    clap_istream_t stream{};

    NegativeReadStream() {
        stream.ctx = this;
        stream.read = read;
    }

    static int64_t CLAP_ABI read(const clap_istream_t*, void*, uint64_t) { return -1; }
};

}  // namespace

TEST(ClapPlugin, EntryFactoryRejectsInvalidFactoryIds) {
    EXPECT_EQ(clap_entry.get_factory(nullptr), nullptr);
    EXPECT_EQ(clap_entry.get_factory("unknown.factory"), nullptr);
}

TEST(ClapPlugin, LifecycleRejectsNullPluginCalls) {
    const clap_plugin_t* plugin = Amplitron::create_amplitron_clap_plugin(nullptr);
    ASSERT_NE(plugin, nullptr);

    EXPECT_FALSE(plugin->init(nullptr));
    EXPECT_FALSE(plugin->activate(nullptr, 48000.0, 64, 512));

    plugin->deactivate(nullptr);

    EXPECT_FALSE(plugin->start_processing(nullptr));

    plugin->stop_processing(nullptr);
    plugin->reset(nullptr);

    clap_process_t process{};
    EXPECT_EQ(plugin->process(nullptr, &process), CLAP_PROCESS_ERROR);
    EXPECT_EQ(plugin->process(plugin, nullptr), CLAP_PROCESS_ERROR);

    EXPECT_FALSE(plugin->start_processing(plugin));

    plugin->destroy(plugin);
}

TEST(ClapPlugin, LifecycleHandlesResetAndEmptyProcess) {
    const clap_plugin_t* plugin = Amplitron::create_amplitron_clap_plugin(nullptr);
    ASSERT_NE(plugin, nullptr);

    ASSERT_TRUE(plugin->init(plugin));
    ASSERT_TRUE(plugin->activate(plugin, 48000.0, 64, 512));
    ASSERT_TRUE(plugin->start_processing(plugin));

    plugin->reset(plugin);

    clap_process_t process{};
    process.frames_count = 0;
    process.audio_outputs = nullptr;
    process.audio_outputs_count = 0;

    EXPECT_EQ(plugin->process(plugin, &process), CLAP_PROCESS_CONTINUE);

    plugin->stop_processing(plugin);
    plugin->deactivate(plugin);
    plugin->destroy(plugin);
}

TEST(ClapPlugin, ExtensionRejectsNullAndUnknownIds) {
    const clap_plugin_t* plugin = Amplitron::create_amplitron_clap_plugin(nullptr);
    ASSERT_NE(plugin, nullptr);

    EXPECT_EQ(plugin->get_extension(plugin, nullptr), nullptr);
    EXPECT_EQ(plugin->get_extension(plugin, "unknown.extension"), nullptr);

    plugin->on_main_thread(plugin);

    plugin->destroy(plugin);
}

TEST(ClapPlugin, ParamsHandleGuardPathsAndTextConversion) {
    const clap_plugin_t* plugin = Amplitron::create_amplitron_clap_plugin(nullptr);
    ASSERT_NE(plugin, nullptr);

    const auto* params =
        static_cast<const clap_plugin_params_t*>(plugin->get_extension(plugin, CLAP_EXT_PARAMS));
    ASSERT_NE(params, nullptr);

    NullInputEventList null_events;
    InvalidInputEventList invalid_events;

    params->flush(plugin, &null_events.events, nullptr);
    params->flush(plugin, &invalid_events.events, nullptr);

    double value = 123.0;
    EXPECT_FALSE(params->get_value(plugin, 999999u, &value));

    char buffer[32] = {};
    EXPECT_FALSE(params->value_to_text(plugin, 0, 1.25, nullptr, sizeof(buffer)));
    EXPECT_FALSE(params->value_to_text(plugin, 0, 1.25, buffer, 0));
    EXPECT_TRUE(params->value_to_text(plugin, 0, 1.25, buffer, sizeof(buffer)));

    double parsed = 0.0;
    EXPECT_FALSE(params->text_to_value(plugin, 0, nullptr, &parsed));
    EXPECT_FALSE(params->text_to_value(plugin, 0, "1.25", nullptr));
    EXPECT_FALSE(params->text_to_value(plugin, 0, "not-a-number", &parsed));
    EXPECT_TRUE(params->text_to_value(plugin, 0, "3.5", &parsed));
    EXPECT_DOUBLE_EQ(parsed, 3.5);

    plugin->destroy(plugin);
}

TEST(ClapPlugin, StateExtensionRejectsInvalidStreams) {
    const clap_plugin_t* plugin = Amplitron::create_amplitron_clap_plugin(nullptr);
    ASSERT_NE(plugin, nullptr);

    const auto* state =
        static_cast<const clap_plugin_state_t*>(plugin->get_extension(plugin, CLAP_EXT_STATE));
    ASSERT_NE(state, nullptr);

    clap_ostream_t output_without_writer{};
    EXPECT_FALSE(state->save(plugin, nullptr));
    EXPECT_FALSE(state->save(plugin, &output_without_writer));

    ZeroWriteStream zero_writer;
    EXPECT_FALSE(state->save(plugin, &zero_writer.stream));

    clap_istream_t input_without_reader{};
    EXPECT_FALSE(state->load(plugin, nullptr));
    EXPECT_FALSE(state->load(plugin, &input_without_reader));

    NegativeReadStream negative_reader;
    EXPECT_FALSE(state->load(plugin, &negative_reader.stream));

    plugin->destroy(plugin);
}
