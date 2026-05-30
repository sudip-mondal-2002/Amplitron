#pragma once
#ifdef WITH_JACK
#include <jack/jack.h>
#include <string>

namespace Amplitron {
    // Configurable behavior for JACK mock
    extern bool g_mock_jack_client_open_fail;
    extern bool g_mock_jack_port_register_fail;
    extern bool g_mock_jack_activate_fail;
    
    // Store process callback and argument so we can trigger it in tests
    extern int (*g_mock_jack_process_callback)(jack_nframes_t, void*);
    extern void *g_mock_jack_process_arg;
}

extern "C" {
jack_client_t *MOCK_jack_client_open(const char *client_name, jack_options_t options, jack_status_t *status, ...);
int MOCK_jack_client_close(jack_client_t *client);
jack_port_t *MOCK_jack_port_register(jack_client_t *client, const char *port_name, const char *port_type, unsigned long flags, unsigned long buffer_size);
int MOCK_jack_set_process_callback(jack_client_t *client, int (*process_callback)(jack_nframes_t, void*), void *arg);
int MOCK_jack_activate(jack_client_t *client);
int MOCK_jack_deactivate(jack_client_t *client);
void *MOCK_jack_port_get_buffer(jack_port_t *port, jack_nframes_t nframes);
}
#endif
