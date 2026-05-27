#include "test_framework.h"
#include "test_fixtures.h"
#include "audio/effects/noise_gate.h"
#include "audio/effects/compressor.h"
#include "audio/effects/multiband_compressor.h"
#include <cstring>
#include <cmath>

using namespace Amplitron;

TEST_F(EffectsTest, noise_gate_silences_quiet_signal) {
    NoiseGate ng;
    ng.set_sample_rate(SR);
    ng.reset();

    // Very quiet signal
    for (int i = 0; i < BUFFER_SIZE; ++i) {
        input_buffer[i] = 0.0001f * std::sin(2.0f * 3.14159f * 440.0f * i / SR);
    }

    // Process several times to let the gate close
    for (int rep = 0; rep < 20; ++rep) {
        ng.process(input_buffer, 256);
    }

    float out_rms = rms(input_buffer, 256);
    ASSERT_LT(out_rms, 0.001f);
}

TEST_F(EffectsTest, noise_gate_passes_loud_signal) {
    NoiseGate ng;
    ng.set_sample_rate(SR);
    ng.reset();

    fill_sine(440.0f);
    float in_rms = rms(input_buffer, 256);

    ng.process(input_buffer, 256);
    float out_rms = rms(input_buffer, 256);

    // Loud signal should pass through mostly unchanged
    ASSERT_GT(out_rms, in_rms * 0.5f);
}

TEST_F(EffectsTest, compressor_reduces_dynamic_range) {
    Compressor comp;
    comp.set_sample_rate(SR);
    comp.reset();

    // Loud signal
    float buf[2048];
    for (int i = 0; i < 2048; ++i) {
        buf[i] = 0.9f * std::sin(2.0f * 3.14159f * 440.0f * i / SR);
    }

    // Process multiple times to let compressor engage
    for (int rep = 0; rep < 5; ++rep) {
        for (int i = 0; i < 2048; ++i) {
            buf[i] = 0.9f * std::sin(2.0f * 3.14159f * 440.0f * i / SR);
        }
        comp.process(buf, 2048);
    }

    ASSERT_TRUE(is_finite(buf, 2048));
    ASSERT_GT(rms(buf, 2048), 0.01f);
}

TEST_F(EffectsTest, multiband_compressor_unity_gain_passthrough) {
    MultiBandCompressor mbc;
    mbc.set_sample_rate(SR);
    mbc.reset();

    // Set ratios of all bands to 1:1, makeup to 0 dB, and Out Gain to 0 dB
    mbc.params()[0].value = 200.0f;   // Low XOver
    mbc.params()[1].value = 4000.0f;  // High XOver

    // Low Band
    mbc.params()[2].value = -20.0f;   // Thresh
    mbc.params()[3].value = 1.0f;     // Ratio = 1.0 (1:1)
    mbc.params()[4].value = 5.0f;     // Attack
    mbc.params()[5].value = 100.0f;   // Release
    mbc.params()[6].value = 0.0f;     // Makeup = 0.0 dB

    // Mid Band
    mbc.params()[7].value = -20.0f;   // Thresh
    mbc.params()[8].value = 1.0f;     // Ratio = 1.0
    mbc.params()[9].value = 5.0f;     // Attack
    mbc.params()[10].value = 100.0f;  // Release
    mbc.params()[11].value = 0.0f;    // Makeup = 0.0 dB

    // High Band
    mbc.params()[12].value = -20.0f;  // Thresh
    mbc.params()[13].value = 1.0f;    // Ratio = 1.0
    mbc.params()[14].value = 5.0f;    // Attack
    mbc.params()[15].value = 100.0f;  // Release
    mbc.params()[16].value = 0.0f;    // Makeup = 0.0 dB

    // Global
    mbc.params()[17].value = 0.0f;    // Out Gain = 0.0 dB

    fill_sine(1000.0f);
    std::memcpy(output_buffer, input_buffer, sizeof(input_buffer));

    mbc.process(input_buffer, BUFFER_SIZE);

    for (int i = 0; i < BUFFER_SIZE; ++i) {
        ASSERT_NEAR(input_buffer[i], output_buffer[i], 1e-4f);
    }
}

TEST_F(EffectsTest, multiband_compressor_independent_band_compression) {
    MultiBandCompressor mbc;
    mbc.set_sample_rate(SR);
    mbc.reset();

    // Crossovers
    mbc.params()[0].value = 200.0f;   // Low XOver = 200 Hz
    mbc.params()[1].value = 4000.0f;  // High XOver = 4000 Hz

    // Set Low Band ratio to 10:1 and Mid/High ratios to 1:1
    for (int b = 0; b < 3; ++b) {
        int offset = 2 + b * 5;
        mbc.params()[offset + 0].value = -40.0f; // Threshold = -40 dB
        mbc.params()[offset + 1].value = (b == 0) ? 10.0f : 1.0f; // Low Ratio = 10:1, Mid/High = 1:1
        mbc.params()[offset + 2].value = 2.0f;   // Fast attack
        mbc.params()[offset + 3].value = 50.0f;  // Fast release
        mbc.params()[offset + 4].value = 0.0f;   // Makeup = 0 dB
    }
    mbc.params()[17].value = 0.0f; // Out Gain = 0 dB

    // Feed a 100 Hz sine wave (Low band)
    float low_buf[1024];
    for (int i = 0; i < 1024; ++i) {
        low_buf[i] = 0.8f * std::sin(2.0f * 3.14159f * 100.0f * i / SR);
    }

    // Process a few times to let envelope followers charge up
    for (int rep = 0; rep < 10; ++rep) {
        for (int i = 0; i < 1024; ++i) {
            low_buf[i] = 0.8f * std::sin(2.0f * 3.14159f * 100.0f * i / SR);
        }
        mbc.process(low_buf, 1024);
    }

    // Low band compression should be active, Mid/High should be inactive
    ASSERT_GT(mbc.get_gain_reduction_db(0), 1.0f);
    ASSERT_NEAR(mbc.get_gain_reduction_db(1), 0.0f, 1e-4f);
    ASSERT_NEAR(mbc.get_gain_reduction_db(2), 0.0f, 1e-4f);

    // Now reset and do the opposite: compress only the High Band
    mbc.reset();
    for (int b = 0; b < 3; ++b) {
        int offset = 2 + b * 5;
        mbc.params()[offset + 0].value = -40.0f;
        mbc.params()[offset + 1].value = (b == 2) ? 10.0f : 1.0f; // High Ratio = 10:1, Low/Mid = 1:1
        mbc.params()[offset + 2].value = 2.0f;
        mbc.params()[offset + 3].value = 50.0f;
        mbc.params()[offset + 4].value = 0.0f;
    }

    // Feed a 6000 Hz sine wave (High band)
    float high_buf[1024];
    for (int i = 0; i < 1024; ++i) {
        high_buf[i] = 0.8f * std::sin(2.0f * 3.14159f * 6000.0f * i / SR);
    }

    for (int rep = 0; rep < 10; ++rep) {
        for (int i = 0; i < 1024; ++i) {
            high_buf[i] = 0.8f * std::sin(2.0f * 3.14159f * 6000.0f * i / SR);
        }
        mbc.process(high_buf, 1024);
    }

    // High band compression should be active, Low/Mid should be inactive
    ASSERT_GT(mbc.get_gain_reduction_db(2), 1.0f);
    ASSERT_NEAR(mbc.get_gain_reduction_db(0), 0.0f, 1e-4f);
    ASSERT_NEAR(mbc.get_gain_reduction_db(1), 0.0f, 1e-4f);
}
