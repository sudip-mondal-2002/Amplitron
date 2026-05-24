## Summary

On Linux, PortAudio with ALSA/PulseAudio can still introduce system-level latency due to audio server mixing. [JACK](https://jackaudio.org/) is the professional-grade low-latency audio server used by studios and live performers on Linux. Adding a JACK backend gives Linux users best-in-class latency (often < 5ms round-trip).

## Current Behaviour

- Linux uses PortAudio with ALSA or PulseAudio
- Achievable latency: ~5-20ms depending on system configuration
- PulseAudio adds significant mixing overhead

## Proposed Behaviour

- Amplitron detects if JACK is running and offers it as a backend option in **File → Settings → Audio Backend**
- When JACK is selected, the app registers as a JACK client with `in_1` and `out_1` ports
- Buffer size and sample rate are inherited from the JACK server configuration
- Falls back to PortAudio if JACK is not available

## Implementation Notes

- The audio backend abstraction already exists at `src/audio/audio_backend.h`
- Create `src/audio/audio_backend_jack.cpp` implementing the same interface
- Use `libjack-jackd2-dev` (conditionally compiled with `-DWITH_JACK=ON`)
- Add build instructions for JACK to README and `scripts/setup_dependencies.sh`
- JACK callback: `int jack_process(jack_nframes_t nframes, void* arg)`

## Dependencies

- `libjack-jackd2-dev` (Linux only, optional)
- Conditional CMake flag: `-DWITH_JACK=ON`

## Acceptance Criteria

- [ ] `audio_backend_jack.cpp` implements the `AudioBackend` interface
- [ ] JACK backend selectable in Settings when JACK server is running
- [ ] Correct audio I/O with JACK (guitar in → processed out)
- [ ] Falls back to PortAudio gracefully when JACK is unavailable
- [ ] Build instructions updated in README and setup script
- [ ] CI does not break for platforms that don't have JACK (conditional compile)
- [ ] Measured latency documented in PR description, void* arg)`
## Dependencies

- `libjack-jackd2-dev` (Linux only, optional)
- - Conditional CMake flag: `-DWITH_JACK=ON`
## Acceptance Criteria

- [ ] `audio_backend_jack.cpp` implements the `AudioBackend` interface
- [ ] - [ ] JACK backend selectable in Settings when JACK server is running
- [ ] - [ ] Correct audio I/O with JACK (guitar in → processed out)
- [ ] - [ ] Falls back to PortAudio gracefully when JACK is unavailable
- [ ] - [ ] Build instructions updated in README and setup script
- [ ] - [ ] CI does not break for platforms that don't have JACK (conditional compile)
- [ ] - [ ] Measured latency documented in PR description