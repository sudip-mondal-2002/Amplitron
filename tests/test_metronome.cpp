#include "audio/audio_engine.h"
#include <cassert>
#include <iostream>

void test_metronome_features() {
    // 1. Set up a dummy audio engine
    Amplitron::AudioEngine engine;
    engine.initialize();

    std::cout << "[Test] Verifying sample rate synchronization..." << std::endl;
    // 2. Change the sample rate
    engine.set_sample_rate(48000);
    engine.start(); 
    // 3. Assert (verify) that the engine actually updated it
    assert(engine.get_sample_rate() == 48000);

    std::cout << "[Test] Verifying volume parameter targeting..." << std::endl;
    // 4. Change the volume
    engine.set_metronome_volume(0.9f);
    // 5. Assert (verify) that the target volume updated correctly
    assert(engine.get_metronome_volume() == 0.9f);

    std::cout << ">> All metronome regression tests PASSED!" << std::endl;
    engine.shutdown();
}

// This main function runs when the testing suite is triggered
int main() {
    test_metronome_features();
    return 0;
}