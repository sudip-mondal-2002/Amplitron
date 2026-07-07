#include "midi/midi_manager.h"
#ifndef __EMSCRIPTEN__
#include <rtmidi/RtMidi.h>
#endif

#include <iostream>

#ifdef AMPLITRON_TESTS
bool g_mock_rtmidi_should_fail_enumerate = false;
bool g_mock_rtmidi_should_fail_open = false;
bool g_mock_rtmidi_should_fail_constructor = false;
bool g_mock_rtmidi_should_fail_close = false;
int g_mock_rtmidi_port_count = 1;
void (*g_mock_rtmidi_callback)(double, std::vector<unsigned char>*, void*) = nullptr;
void* g_mock_rtmidi_user_data = nullptr;
bool g_mock_rtmidi_port_open = false;

class MockRtMidiIn {
   public:
    MockRtMidiIn(int = 0, const std::string& = "") {
        if (g_mock_rtmidi_should_fail_constructor) throw RtMidiError("Mock constructor failure");
    }
    void ignoreTypes(bool, bool, bool) {}
    unsigned int getPortCount() {
        if (g_mock_rtmidi_should_fail_enumerate) throw RtMidiError("Mock port count failure");
        return g_mock_rtmidi_port_count;
    }
    std::string getPortName(unsigned int index) {
        if (index == 0) return "Mock MIDI Port";
        throw RtMidiError("Invalid port");
    }
    void setCallback(void (*cb)(double, std::vector<unsigned char>*, void*), void* data) {
        g_mock_rtmidi_callback = cb;
        g_mock_rtmidi_user_data = data;
    }
    void openPort(unsigned int index, const std::string&) {
        if (index != 0) throw RtMidiError("Invalid port");
        if (g_mock_rtmidi_should_fail_open) throw RtMidiError("Mock open failure");
        g_mock_rtmidi_port_open = true;
    }
    void closePort() {
        if (g_mock_rtmidi_should_fail_close) throw RtMidiError("Mock close failure");
        g_mock_rtmidi_port_open = false;
    }
    void cancelCallback() {
        g_mock_rtmidi_callback = nullptr;
        g_mock_rtmidi_user_data = nullptr;
    }
};

#define RtMidiIn MockRtMidiIn
#endif

namespace Amplitron {

// ---------------------------------------------------------------------------
// Construction / destruction
// ---------------------------------------------------------------------------

MidiManager::MidiManager() = default;

MidiManager::~MidiManager() { shutdown(); }

// ---------------------------------------------------------------------------
// RtMidi callback — runs on RtMidi's internal thread, must be lock-free
// ---------------------------------------------------------------------------

#ifdef __EMSCRIPTEN__
void MidiManager::midi_callback(double /*timestamp*/, std::vector<unsigned char>* /*message*/,
                                void* /*user_data*/) {}

bool MidiManager::initialize() { return true; }
void MidiManager::shutdown() { close_port(); }
std::vector<std::string> MidiManager::get_available_ports() const { return {}; }
bool MidiManager::open_port(int /*port_index*/) { return false; }
void MidiManager::close_port() {
    current_port_ = -1;
    current_port_name_.clear();
}
#else
void MidiManager::midi_callback(double /*timestamp*/, std::vector<unsigned char>* message,
                                void* user_data) {
    if (!message || message->size() < 3) return;

    auto* self = static_cast<MidiManager*>(user_data);
    uint8_t status = (*message)[0];

    // Only handle Control Change messages (0xB0 .. 0xBF)
    if ((status & 0xF0) != 0xB0) return;

    MidiEvent event{};
    event.status = status;
    event.data1 = (*message)[1];        // CC number
    event.data2 = (*message)[2];        // CC value
    self->midi_queue_.try_push(event);  // Drop if full — acceptable for CC
}

// ---------------------------------------------------------------------------
// Port management
// ---------------------------------------------------------------------------

bool MidiManager::initialize() {
    if (midi_in_) return true;  // Already initialized

    try {
        auto* rt = new RtMidiIn(RtMidi::UNSPECIFIED, "Amplitron MIDI");
        rt->ignoreTypes(true, true, true);  // Ignore SysEx, timing, active sensing
        midi_in_ = rt;
    } catch (const RtMidiError& e) {
        std::cerr << "[MidiManager] RtMidi init failed: " << e.getMessage() << "\n";
        return false;
    }

    // Auto-open the first available port (if any)
    auto ports = get_available_ports();
    if (!ports.empty()) {
        open_port(0);
    }
    return true;
}

void MidiManager::shutdown() {
    close_port();
    if (midi_in_) {
        delete static_cast<RtMidiIn*>(midi_in_);
        midi_in_ = nullptr;
    }
}

std::vector<std::string> MidiManager::get_available_ports() const {
    std::vector<std::string> result;
    if (!midi_in_) return result;

    auto* rt = static_cast<RtMidiIn*>(midi_in_);
    try {
        unsigned int count = rt->getPortCount();
        result.reserve(count);
        for (unsigned int i = 0; i < count; ++i) {
            result.push_back(rt->getPortName(i));
        }
    } catch (const RtMidiError& e) {
        std::cerr << "[MidiManager] Failed to enumerate MIDI ports: " << e.getMessage() << "\n";
    }
    return result;
}

bool MidiManager::open_port(int port_index) {
    if (!midi_in_) return false;

    close_port();

    auto* rt = static_cast<RtMidiIn*>(midi_in_);

    try {
        unsigned int count = rt->getPortCount();
        if (port_index < 0 || static_cast<unsigned int>(port_index) >= count) return false;

        rt->setCallback(&MidiManager::midi_callback, this);
        rt->openPort(static_cast<unsigned int>(port_index), "Amplitron In");
        current_port_ = port_index;
        current_port_name_ = rt->getPortName(static_cast<unsigned int>(port_index));
        return true;
    } catch (const RtMidiError& e) {
        std::cerr << "[MidiManager] Failed to open port " << port_index << ": " << e.getMessage()
                  << "\n";
        // Ensure port is closed on error
        try {
            rt->closePort();
        } catch (...) {
        }
        current_port_ = -1;
        current_port_name_.clear();
        return false;
    }
}

void MidiManager::close_port() {
    if (!midi_in_ || current_port_ < 0) return;

    auto* rt = static_cast<RtMidiIn*>(midi_in_);
    try {
        rt->cancelCallback();
        rt->closePort();
    } catch (...) {
    }
    current_port_ = -1;
    current_port_name_.clear();
}
#endif

}  // namespace Amplitron
