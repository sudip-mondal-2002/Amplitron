#pragma once

#include <portaudio.h>

namespace Amplitron {
// Test injection seams for PortAudio
extern int (*g_mock_pa_get_device_count)();
extern const PaDeviceInfo* (*g_mock_pa_get_device_info)(int);
extern const PaHostApiInfo* (*g_mock_pa_get_host_api_info)(int);
extern int (*g_mock_pa_get_host_api_count)();
extern int (*g_mock_pa_host_api_device_index_to_device_index)(int, int);
extern int (*g_mock_pa_get_default_input_device)();
extern int (*g_mock_pa_get_default_output_device)();

extern PaError (*g_mock_pa_open_stream)(PaStream**, const PaStreamParameters*, const PaStreamParameters*, double, unsigned long, PaStreamFlags, PaStreamCallback*, void*);
extern PaError (*g_mock_pa_start_stream)(PaStream*);
extern PaError (*g_mock_pa_stop_stream)(PaStream*);
extern PaError (*g_mock_pa_close_stream)(PaStream*);
extern const PaStreamInfo* (*g_mock_pa_get_stream_info)(PaStream*);
extern PaError (*g_mock_pa_initialize)();
} // namespace Amplitron

extern "C" {
int MOCK_Pa_GetDeviceCount();
const PaDeviceInfo *MOCK_Pa_GetDeviceInfo(PaDeviceIndex device);
const PaHostApiInfo *MOCK_Pa_GetHostApiInfo(PaHostApiIndex hostApi);
PaHostApiIndex MOCK_Pa_GetHostApiCount();
PaDeviceIndex MOCK_Pa_HostApiDeviceIndexToDeviceIndex(PaHostApiIndex hostApi, int hostApiDeviceIndex);
PaDeviceIndex MOCK_Pa_GetDefaultInputDevice();
PaDeviceIndex MOCK_Pa_GetDefaultOutputDevice();
PaError MOCK_Pa_OpenStream(PaStream **stream, const PaStreamParameters *inputParameters, const PaStreamParameters *outputParameters, double sampleRate, unsigned long framesPerBuffer, PaStreamFlags streamFlags, PaStreamCallback *streamCallback, void *userData);
PaError MOCK_Pa_StartStream(PaStream *stream);
PaError MOCK_Pa_StopStream(PaStream *stream);
PaError MOCK_Pa_CloseStream(PaStream *stream);
const PaStreamInfo *MOCK_Pa_GetStreamInfo(PaStream *stream);
PaError MOCK_Pa_Initialize();
}
