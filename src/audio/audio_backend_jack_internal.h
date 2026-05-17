#pragma once

#ifdef WITH_JACK
#include <jack/jack.h>

namespace Amplitron
{

    struct AudioBackendState
    {
        jack_client_t *client = nullptr;
        jack_port_t *input_port = nullptr;
        jack_port_t *output_port = nullptr;
        int measured_latency_ms = 0;
    };

} // namespace Amplitron
#endif // WITH_JACK
