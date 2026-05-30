#undef Pa_GetDeviceCount
#undef Pa_GetDeviceInfo
#undef Pa_GetHostApiInfo
#undef Pa_GetHostApiCount
#undef Pa_HostApiDeviceIndexToDeviceIndex
#undef Pa_GetDefaultInputDevice
#undef Pa_GetDefaultOutputDevice
#undef Pa_OpenStream
#undef Pa_StartStream
#undef Pa_StopStream
#undef Pa_CloseStream
#undef Pa_GetStreamInfo
#undef Pa_Initialize

#include "portaudio_mock.h"

namespace Amplitron {
int (*g_mock_pa_get_device_count)() = nullptr;
const PaDeviceInfo *(*g_mock_pa_get_device_info)(int) = nullptr;
const PaHostApiInfo *(*g_mock_pa_get_host_api_info)(int) = nullptr;
int (*g_mock_pa_get_host_api_count)() = nullptr;
int (*g_mock_pa_host_api_device_index_to_device_index)(int, int) = nullptr;
int (*g_mock_pa_get_default_input_device)() = nullptr;
int (*g_mock_pa_get_default_output_device)() = nullptr;
PaError (*g_mock_pa_open_stream)(PaStream **, const PaStreamParameters *,
                                 const PaStreamParameters *, double,
                                 unsigned long, PaStreamFlags,
                                 PaStreamCallback *, void *) = nullptr;
PaError (*g_mock_pa_start_stream)(PaStream *) = nullptr;
PaError (*g_mock_pa_stop_stream)(PaStream *) = nullptr;
PaError (*g_mock_pa_close_stream)(PaStream *) = nullptr;
const PaStreamInfo *(*g_mock_pa_get_stream_info)(PaStream *) = nullptr;
PaError (*g_mock_pa_initialize)() = nullptr;
} // namespace Amplitron

extern "C" {
int MOCK_Pa_GetDeviceCount() {
  if (Amplitron::g_mock_pa_get_device_count)
    return Amplitron::g_mock_pa_get_device_count();
  return Pa_GetDeviceCount();
}
const PaDeviceInfo *MOCK_Pa_GetDeviceInfo(PaDeviceIndex device) {
  if (Amplitron::g_mock_pa_get_device_info)
    return Amplitron::g_mock_pa_get_device_info(device);
  return Pa_GetDeviceInfo(device);
}
const PaHostApiInfo *MOCK_Pa_GetHostApiInfo(PaHostApiIndex hostApi) {
  if (Amplitron::g_mock_pa_get_host_api_info)
    return Amplitron::g_mock_pa_get_host_api_info(hostApi);
  return Pa_GetHostApiInfo(hostApi);
}
PaHostApiIndex MOCK_Pa_GetHostApiCount() {
  if (Amplitron::g_mock_pa_get_host_api_count)
    return Amplitron::g_mock_pa_get_host_api_count();
  return Pa_GetHostApiCount();
}
PaDeviceIndex MOCK_Pa_HostApiDeviceIndexToDeviceIndex(PaHostApiIndex hostApi,
                                                      int hostApiDeviceIndex) {
  if (Amplitron::g_mock_pa_host_api_device_index_to_device_index)
    return Amplitron::g_mock_pa_host_api_device_index_to_device_index(
        hostApi, hostApiDeviceIndex);
  return Pa_HostApiDeviceIndexToDeviceIndex(hostApi, hostApiDeviceIndex);
}
PaDeviceIndex MOCK_Pa_GetDefaultInputDevice() {
  if (Amplitron::g_mock_pa_get_default_input_device)
    return Amplitron::g_mock_pa_get_default_input_device();
  return Pa_GetDefaultInputDevice();
}
PaDeviceIndex MOCK_Pa_GetDefaultOutputDevice() {
  if (Amplitron::g_mock_pa_get_default_output_device)
    return Amplitron::g_mock_pa_get_default_output_device();
  return Pa_GetDefaultOutputDevice();
}
PaError MOCK_Pa_OpenStream(PaStream **stream,
                           const PaStreamParameters *inputParameters,
                           const PaStreamParameters *outputParameters,
                           double sampleRate, unsigned long framesPerBuffer,
                           PaStreamFlags streamFlags,
                           PaStreamCallback *streamCallback, void *userData) {
  if (Amplitron::g_mock_pa_open_stream)
    return Amplitron::g_mock_pa_open_stream(
        stream, inputParameters, outputParameters, sampleRate, framesPerBuffer,
        streamFlags, streamCallback, userData);
  return Pa_OpenStream(stream, inputParameters, outputParameters, sampleRate,
                       framesPerBuffer, streamFlags, streamCallback, userData);
}
PaError MOCK_Pa_StartStream(PaStream *stream) {
  if (Amplitron::g_mock_pa_start_stream)
    return Amplitron::g_mock_pa_start_stream(stream);
  return Pa_StartStream(stream);
}
PaError MOCK_Pa_StopStream(PaStream *stream) {
  if (Amplitron::g_mock_pa_stop_stream)
    return Amplitron::g_mock_pa_stop_stream(stream);
  return Pa_StopStream(stream);
}
PaError MOCK_Pa_CloseStream(PaStream *stream) {
  if (Amplitron::g_mock_pa_close_stream)
    return Amplitron::g_mock_pa_close_stream(stream);
  return Pa_CloseStream(stream);
}
const PaStreamInfo *MOCK_Pa_GetStreamInfo(PaStream *stream) {
  if (Amplitron::g_mock_pa_get_stream_info)
    return Amplitron::g_mock_pa_get_stream_info(stream);
  return Pa_GetStreamInfo(stream);
}
PaError MOCK_Pa_Initialize() {
  if (Amplitron::g_mock_pa_initialize)
    return Amplitron::g_mock_pa_initialize();
  return Pa_Initialize();
}
} // extern "C"
