#include "audio/backend/jack_backend.h"

#include <iostream>
#include <string>

#include "audio/engine/i_audio_engine.h"

namespace Amplitron {

#ifdef WITH_JACK
static int jack_process(jack_nframes_t nframes, void *arg) {
    auto *backend = static_cast<JackBackend *>(arg);
    if (!backend || !backend->get_engine()) return 0;

    auto *in_port = backend->get_in_port();
    auto *out_port = backend->get_out_port();
    if (!in_port || !out_port) return 0;

    float *in = static_cast<float *>(jack_port_get_buffer(in_port, nframes));
    float *out = static_cast<float *>(jack_port_get_buffer(out_port, nframes));
    if (in && out) {
        backend->get_engine()->process_audio(in, out, static_cast<int>(nframes));
    }

    return 0;
}
#endif

JackBackend::JackBackend() = default;
JackBackend::~JackBackend() { shutdown(); }

bool JackBackend::initialize(IAudioEngine *engine) {
    if (initialized_) return true;
    engine_ = engine;

#ifdef WITH_JACK
    jack_status_t status = static_cast<jack_status_t>(0);
    client_ = jack_client_open("Amplitron", JackNoStartServer, &status);
    if (!client_) {
        std::string status_details = "";
        if (status & JackFailure) status_details += "JackFailure ";
        if (status & JackInvalidOption) status_details += "JackInvalidOption ";
        if (status & JackNameNotUnique) status_details += "JackNameNotUnique ";
        if (status & JackServerStarted) status_details += "JackServerStarted ";
        if (status & JackServerFailed) status_details += "JackServerFailed ";
        if (status & JackServerError) status_details += "JackServerError ";
        if (status & JackNoSuchClient) status_details += "JackNoSuchClient ";
        if (status & JackLoadFailure) status_details += "JackLoadFailure ";
        if (status & JackInitFailure) status_details += "JackInitFailure ";
        if (status & JackShmFailure) status_details += "JackShmFailure ";
        if (status & JackVersionError) status_details += "JackVersionError ";
        if (status & JackBackendError) status_details += "JackBackendError ";
        if (status & JackClientZombie) status_details += "JackClientZombie ";

        std::cerr << "[Amplitron] JACK: could not open JACK server (is jackd running?). Status: 0x"
                  << std::hex << static_cast<int>(status) << std::dec;
        if (!status_details.empty()) {
            std::cerr << " (" << status_details << ")";
        }
        std::cerr << std::endl;
    }
#endif

    initialized_ = true;
    return true;
}

void JackBackend::shutdown() {
    stop();
#ifdef WITH_JACK
    if (client_) {
        jack_client_close(client_);
        client_ = nullptr;
        in_port_ = nullptr;
        out_port_ = nullptr;
        ports_registered_ = false;
    }
#endif
    initialized_ = false;
}

bool JackBackend::start() {
    if (!initialized_ || running_) return false;

#ifdef WITH_JACK
    if (!client_) return false;

    if (!ports_registered_) {
        in_port_ = jack_port_register(client_, "in_1", JACK_DEFAULT_AUDIO_TYPE, JackPortIsInput, 0);
        out_port_ =
            jack_port_register(client_, "out_1", JACK_DEFAULT_AUDIO_TYPE, JackPortIsOutput, 0);
        if (!in_port_ || !out_port_) {
            std::cerr << "[Amplitron] JACK: failed to register in/out ports." << std::endl;
            return false;
        }
        jack_set_process_callback(client_, jack_process, this);
        ports_registered_ = true;
    }

    if (jack_activate(client_)) {
        std::cerr << "[Amplitron] JACK: failed to activate JACK client." << std::endl;
        return false;
    }

    running_ = true;
    return true;
#else
    return false;
#endif
}

void JackBackend::stop() {
#ifdef WITH_JACK
    if (client_ && running_) {
        jack_deactivate(client_);
    }
#endif
    running_ = false;
}

std::vector<AudioDeviceInfo> JackBackend::get_input_devices() const {
    return {{0, "JACK in_1", 1, 0, static_cast<double>(get_sample_rate()), false}};
}

std::vector<AudioDeviceInfo> JackBackend::get_output_devices() const {
    return {{0, "JACK out_1", 0, 1, static_cast<double>(get_sample_rate()), false}};
}

bool JackBackend::set_input_device(int device_index) { return device_index == 0; }

bool JackBackend::set_output_device(int device_index) { return device_index == 0; }

std::string JackBackend::get_input_device_name() const { return "JACK in_1"; }

std::string JackBackend::get_output_device_name() const { return "JACK out_1"; }

int JackBackend::get_sample_rate() const {
#ifdef WITH_JACK
    if (client_) {
        return static_cast<int>(jack_get_sample_rate(client_));
    }
#endif
    return engine_ ? engine_->get_sample_rate() : 48000;
}

int JackBackend::get_buffer_size() const {
#ifdef WITH_JACK
    if (client_) {
        return static_cast<int>(jack_get_buffer_size(client_));
    }
#endif
    return engine_ ? engine_->get_buffer_size() : 512;
}

}  // namespace Amplitron
