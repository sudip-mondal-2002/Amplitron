#include "midi/midi_manager.h"
#ifndef __EMSCRIPTEN__
#include <rtmidi/RtMidi.h>
#endif

#include <iostream>

namespace Amplitron {

// ---------------------------------------------------------------------------
// Construction / destruction
// ---------------------------------------------------------------------------

MidiManager::MidiManager() = default;

MidiManager::~MidiManager() {
    shutdown();
}

// ---------------------------------------------------------------------------
// RtMidi callback — runs on RtMidi's internal thread, must be lock-free
// ---------------------------------------------------------------------------

#ifdef __EMSCRIPTEN__
void MidiManager::midi_callback(double /*timestamp*/,
                                std::vector<unsigned char>* /*message*/,
                                void* /*user_data*/) {
}

bool MidiManager::initialize() { return true; }
void MidiManager::shutdown() { close_port(); }
std::vector<std::string> MidiManager::get_available_ports() const { return {}; }
bool MidiManager::open_port(int /*port_index*/) { return false; }
void MidiManager::close_port() { current_port_ = -1; current_port_name_.clear(); }
#else
void MidiManager::midi_callback(double /*timestamp*/,
                                std::vector<unsigned char>* message,
                                void* user_data) {
    if (!message || message->size() < 3) return;

    auto* self = static_cast<MidiManager*>(user_data);
    uint8_t status = (*message)[0];

    // Only handle Control Change messages (0xB0 .. 0xBF)
    if ((status & 0xF0) != 0xB0) return;

    MidiEvent event{};
    event.status = status;
    event.data1  = (*message)[1];  // CC number
    event.data2  = (*message)[2];  // CC value
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
        std::cerr << "[MidiManager] Failed to enumerate MIDI ports: "
                  << e.getMessage() << "\n";
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
        std::cerr << "[MidiManager] Failed to open port " << port_index
                  << ": " << e.getMessage() << "\n";
        // Ensure port is closed on error
        try {
            rt->closePort();
        } catch (...) {}
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
    } catch (...) {}
    current_port_ = -1;
    current_port_name_.clear();
}
#endif

} // namespace Amplitron
