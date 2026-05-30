#ifdef WITH_JACK
#include "jack_mock.h"

namespace Amplitron {
    bool g_mock_jack_client_open_fail = false;
    bool g_mock_jack_port_register_fail = false;
    bool g_mock_jack_activate_fail = false;
    
    int (*g_mock_jack_process_callback)(jack_nframes_t, void*) = nullptr;
    void *g_mock_jack_process_arg = nullptr;
}

extern "C" {
jack_client_t *MOCK_jack_client_open(const char *client_name, jack_options_t options, jack_status_t *status, ...) {
    (void)client_name;
    (void)options;
    if (status) {
        *status = static_cast<jack_status_t>(0);
    }
    if (Amplitron::g_mock_jack_client_open_fail) {
        return nullptr;
    }
    // Return a dummy non-null pointer for the client
    return reinterpret_cast<jack_client_t*>(0x12345678);
}

int MOCK_jack_client_close(jack_client_t *client) {
    (void)client;
    return 0;
}

jack_port_t *MOCK_jack_port_register(jack_client_t *client, const char *port_name, const char *port_type, unsigned long flags, unsigned long buffer_size) {
    (void)client;
    (void)port_name;
    (void)port_type;
    (void)flags;
    (void)buffer_size;
    if (Amplitron::g_mock_jack_port_register_fail) {
        return nullptr;
    }
    // Return dummy non-null port pointers
    if (port_name && std::string(port_name) == "in_1") {
        return reinterpret_cast<jack_port_t*>(0x87654321);
    }
    return reinterpret_cast<jack_port_t*>(0x87654322);
}

int MOCK_jack_set_process_callback(jack_client_t *client, int (*process_callback)(jack_nframes_t, void*), void *arg) {
    (void)client;
    Amplitron::g_mock_jack_process_callback = process_callback;
    Amplitron::g_mock_jack_process_arg = arg;
    return 0;
}

int MOCK_jack_activate(jack_client_t *client) {
    (void)client;
    if (Amplitron::g_mock_jack_activate_fail) {
        return -1;
    }
    return 0;
}

int MOCK_jack_deactivate(jack_client_t *client) {
    (void)client;
    return 0;
}

// Dummy buffers for input/output
static float s_dummy_in[4096] = {0.0f};
static float s_dummy_out[4096] = {0.0f};

void *MOCK_jack_port_get_buffer(jack_port_t *port, jack_nframes_t nframes) {
    (void)nframes;
    if (port == reinterpret_cast<jack_port_t*>(0x87654321)) {
        return s_dummy_in;
    }
    if (port == reinterpret_cast<jack_port_t*>(0x87654322)) {
        return s_dummy_out;
    }
    return nullptr;
}
}
#endif
