#include "audio/effects/nam_loader.h"
#include "test_framework.h"

using namespace Amplitron;

TEST(nam_loader_default_state) {
    NamLoader nl;
    ASSERT_TRUE(nl.is_enabled());
    ASSERT_TRUE(nl.model_path().empty());
}

TEST(nam_loader_name_and_type_id) {
    NamLoader nl;
    ASSERT_TRUE(std::strcmp(nl.name(), "NAM Loader") == 0);
    ASSERT_TRUE(std::strcmp(nl.type_id(), "NamLoader") == 0);
}

TEST(nam_loader_params_valid) {
    NamLoader nl;
    auto& params = nl.params();
    ASSERT_FALSE(params.empty());
    for (auto& p : params) {
        ASSERT_TRUE(p.min_val <= p.max_val);
        ASSERT_TRUE(p.value >= p.min_val && p.value <= p.max_val);
        ASSERT_FALSE(p.name.empty());
    }
}

TEST(nam_loader_load_invalid_path_returns_false) {
    NamLoader nl;
    bool result = nl.load_model("/nonexistent/path/model.nam");
    ASSERT_FALSE(result);
    ASSERT_TRUE(nl.model_path().empty());
}

TEST(nam_loader_process_without_model_leaves_buffer_unchanged) {
    NamLoader nl;
    nl.set_sample_rate(48000);
    nl.reset();

    float buf[64];
    for (int i = 0; i < 64; ++i) buf[i] = 0.5f;

    nl.process(buf, 64);

    for (int i = 0; i < 64; ++i) ASSERT_NEAR(buf[i], 0.5f, 1e-6f);
}

TEST(nam_loader_bypass_passes_signal_unchanged) {
    NamLoader nl;
    nl.set_sample_rate(48000);
    nl.reset();
    nl.set_enabled(false);

    float buf[64], orig[64];
    for (int i = 0; i < 64; ++i) buf[i] = orig[i] = 0.5f;

    nl.process(buf, 64);

    for (int i = 0; i < 64; ++i) ASSERT_NEAR(buf[i], orig[i], 1e-6f);
}

TEST(nam_loader_clear_clears_state) {
    NamLoader nl;
    nl.set_sample_rate(48000);
    nl.load_model("../tests/assets/test_model.nam");
    nl.clear_model();
    ASSERT_TRUE(nl.model_path().empty());
}

TEST(nam_loader_load_real_nam_file_validates_existence) {
    // Uses a minimal RTNeural-format JSON fixture (in_shape + layers).
    // This is the format parseJson expects; load must succeed.
    NamLoader nl;
    nl.set_sample_rate(48000);

    bool result = nl.load_model("../tests/assets/rtneural_test_model.json");
    ASSERT_TRUE(result);
    ASSERT_EQ(nl.model_path(), "../tests/assets/rtneural_test_model.json");
}

TEST(nam_loader_load_incompatible_format_fails_gracefully) {
    // test_model.nam uses the NAM WaveNet schema (version/architecture/config)
    // which is incompatible with RTNeural's parseJson (expects in_shape/layers).
    // load_model must catch the parse exception and return false.
    NamLoader nl;
    nl.set_sample_rate(48000);

    bool result = nl.load_model("../tests/assets/test_model.nam");
    ASSERT_FALSE(result);
    ASSERT_TRUE(nl.model_path().empty());
}

TEST(nam_loader_process_is_safe_regardless_of_model_state) {
    NamLoader nl;
    nl.set_sample_rate(48000);

    // Attempt loading .nam file
    nl.load_model("../tests/assets/test_model.nam");

    // Regardless of load result, process should never crash
    float buf[64];
    for (int i = 0; i < 64; ++i) buf[i] = 0.1f;
    nl.process(buf, 64);

    // Buffer should be finite and not NaN
    bool is_safe = true;
    for (int i = 0; i < 64; ++i) {
        if (!std::isfinite(buf[i])) {  // NaN and Infinity check
            is_safe = false;
            break;
        }
    }
    ASSERT_TRUE(is_safe);
}
