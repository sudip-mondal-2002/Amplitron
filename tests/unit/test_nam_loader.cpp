#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <string>
#include <thread>

#include "audio/effects/nam_loader.h"
#include "test_framework.h"

using namespace Amplitron;

// ---- Basic state ----

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

// ---- Params ----

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

TEST(nam_loader_const_params_valid) {
  const NamLoader nl;
  const auto& params = nl.params();
  ASSERT_FALSE(params.empty());
  ASSERT_EQ(params[0].name, std::string("Level"));
  ASSERT_NEAR(params[0].value, 1.0f, 1e-6f);
  ASSERT_NEAR(params[0].min_val, 0.0f, 1e-6f);
  ASSERT_NEAR(params[0].max_val, 1.0f, 1e-6f);
}

// ---- Load: invalid paths ----

TEST(nam_loader_load_invalid_path_returns_false) {
  NamLoader nl;
  bool result = nl.load_model("/nonexistent/path/model.nam");
  ASSERT_FALSE(result);
  ASSERT_TRUE(nl.model_path().empty());
}

TEST(nam_loader_load_empty_path_returns_false) {
  NamLoader nl;
  bool result = nl.load_model("");
  ASSERT_FALSE(result);
  ASSERT_TRUE(nl.model_path().empty());
}

// ---- Load: valid RTNeural fixture ----

TEST(nam_loader_load_real_nam_file_validates_existence) {
  // Uses a minimal RTNeural-format JSON fixture (in_shape + layers).
  // This is the format parseJson expects; load must succeed.
  NamLoader nl;
  nl.set_sample_rate(48000);

  bool result = nl.load_model("../tests/assets/rtneural_test_model.json");
  ASSERT_TRUE(result);
  ASSERT_EQ(nl.model_path(),
            std::string("../tests/assets/rtneural_test_model.json"));
}

TEST(nam_loader_load_success_sets_model_path) {
  NamLoader nl;
  nl.set_sample_rate(48000);
  bool ok = nl.load_model("../tests/assets/rtneural_test_model.json");
  ASSERT_TRUE(ok);
  ASSERT_FALSE(nl.model_path().empty());
}

TEST(nam_loader_load_success_model_loaded_flag) {
  // After a successful load + process tick (which consumes the pending model),
  // the loader should remain in a loaded state.
  NamLoader nl;
  nl.set_sample_rate(48000);
  ASSERT_TRUE(nl.load_model("../tests/assets/rtneural_test_model.json"));

  // Tick process so the audio thread ingests the pending model.
  float buf[8] = {};
  nl.process(buf, 8);

  // Path is still set; loader did not crash.
  ASSERT_FALSE(nl.model_path().empty());
}

// ---- Load: incompatible format ----

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

// ---- Load: reload and replace ----

TEST(nam_loader_reload_replaces_previous_model) {
  // Loading a second valid model after the first should succeed and update
  // the path (exercises the old_pending delete path in load_model).
  NamLoader nl;
  nl.set_sample_rate(48000);

  ASSERT_TRUE(nl.load_model("../tests/assets/rtneural_test_model.json"));
  // Load again — should replace and delete the first pending model.
  ASSERT_TRUE(nl.load_model("../tests/assets/rtneural_test_model.json"));
  ASSERT_EQ(nl.model_path(),
            std::string("../tests/assets/rtneural_test_model.json"));
}

TEST(nam_loader_reload_after_process_replaces_active_model) {
  // Load → process (promotes pending to active) → load again.
  // This exercises the old-active-model deferred-delete path inside
  // check_pending_model when a second model is swapped in.
  NamLoader nl;
  nl.set_sample_rate(48000);

  ASSERT_TRUE(nl.load_model("../tests/assets/rtneural_test_model.json"));
  float buf[8] = {};
  nl.process(buf, 8);  // pending → active

  ASSERT_TRUE(nl.load_model("../tests/assets/rtneural_test_model.json"));
  nl.process(buf, 8);  // second pending → active, old active → deferred delete

  ASSERT_FALSE(nl.model_path().empty());
}

// ---- Clear model ----

TEST(nam_loader_clear_after_load_empties_path) {
  NamLoader nl;
  nl.set_sample_rate(48000);
  ASSERT_TRUE(nl.load_model("../tests/assets/rtneural_test_model.json"));
  nl.clear_model();
  ASSERT_TRUE(nl.model_path().empty());
}

TEST(nam_loader_clear_clears_state) {
  NamLoader nl;
  nl.set_sample_rate(48000);
  nl.load_model("../tests/assets/test_model.nam");
  nl.clear_model();
  ASSERT_TRUE(nl.model_path().empty());
}

TEST(nam_loader_clear_then_process_is_safe) {
  // clear_model sets clear_pending_; the next process() call must handle
  // that flag (branch in check_pending_model) without crashing.
  NamLoader nl;
  nl.set_sample_rate(48000);

  ASSERT_TRUE(nl.load_model("../tests/assets/rtneural_test_model.json"));
  float buf[8] = {};
  nl.process(buf, 8);  // pending → active

  nl.clear_model();    // sets clear_pending_ = true
  nl.process(buf, 8);  // clear_pending_ path: active → deferred delete

  for (int i = 0; i < 8; ++i) ASSERT_NEAR(buf[i], 0.0f, 1e-6f);
}

TEST(nam_loader_clear_on_active_with_stale_old_model) {
  // First replace (active ← first model, first becomes old_to_delete).
  // Then clear, exercising the prev_old != nullptr branch in the clear path.
  NamLoader nl;
  nl.set_sample_rate(48000);

  ASSERT_TRUE(nl.load_model("../tests/assets/rtneural_test_model.json"));
  float buf[8] = {};
  nl.process(buf, 8);  // active = model_1

  ASSERT_TRUE(nl.load_model("../tests/assets/rtneural_test_model.json"));
  nl.process(buf, 8);  // active = model_2, old_to_delete = model_1

  // Load a third but do NOT process — pending holds model_3.
  ASSERT_TRUE(nl.load_model("../tests/assets/rtneural_test_model.json"));
  // clear_model replaces pending with nullptr (deletes model_3) and schedules
  // clear.
  nl.clear_model();
  nl.process(buf, 8);  // clears active (model_2) → old_to_delete

  ASSERT_TRUE(nl.model_path().empty());
}

TEST(nam_loader_double_clear_is_safe) {
  NamLoader nl;
  nl.clear_model();
  nl.clear_model();
  ASSERT_TRUE(nl.model_path().empty());
}

// ---- Process: bypass / no-model / with-model ----

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

TEST(nam_loader_process_with_loaded_model_runs) {
  // Load a valid model, tick process so it becomes active, then run a full
  // block to exercise the active_model_->forward() path.
  NamLoader nl;
  nl.set_sample_rate(48000);

  ASSERT_TRUE(nl.load_model("../tests/assets/rtneural_test_model.json"));
  float buf[64];
  for (int i = 0; i < 64; ++i) buf[i] = 0.1f;

  nl.process(buf, 64);  // 1st call: pending → active, then process
  nl.process(buf, 64);  // 2nd call: purely process with active model

  // Output must be finite (identity dense: output ≈ input * level=1).
  for (int i = 0; i < 64; ++i) ASSERT_TRUE(std::isfinite(buf[i]));
}

TEST(nam_loader_process_level_param_scales_output) {
  // Set level=0 and verify output is zero (exercises params_[0].value path).
  NamLoader nl;
  nl.set_sample_rate(48000);
  ASSERT_TRUE(nl.load_model("../tests/assets/rtneural_test_model.json"));

  // Tick once to promote pending → active.
  float buf[8] = {0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f};
  nl.process(buf, 8);

  // Now set level to 0.
  nl.params()[0].value = 0.0f;
  for (int i = 0; i < 8; ++i) buf[i] = 0.5f;
  nl.process(buf, 8);
  for (int i = 0; i < 8; ++i) ASSERT_NEAR(buf[i], 0.0f, 1e-6f);
}

TEST(nam_loader_process_is_safe_regardless_of_model_state) {
  NamLoader nl;
  nl.set_sample_rate(48000);

  // Use the NAM-format file which fails to load — process must not crash.
  nl.load_model("../tests/assets/test_model.nam");

  float buf[64];
  for (int i = 0; i < 64; ++i) buf[i] = 0.1f;
  nl.process(buf, 64);

  bool is_safe = true;
  for (int i = 0; i < 64; ++i) {
    if (!std::isfinite(buf[i])) {
      is_safe = false;
      break;
    }
  }
  ASSERT_TRUE(is_safe);
}

// ---- reset() ----

TEST(nam_loader_reset_without_model_is_safe) {
  NamLoader nl;
  nl.reset();  // must not crash with active_model_ == nullptr
}

TEST(nam_loader_reset_with_active_model) {
  // Load + process to make model active, then reset() (exercises the
  // model->reset() branch).
  NamLoader nl;
  nl.set_sample_rate(48000);
  ASSERT_TRUE(nl.load_model("../tests/assets/rtneural_test_model.json"));
  float buf[8] = {};
  nl.process(buf, 8);  // pending → active
  nl.reset();          // model->reset() called
}

// ---- Destructor paths ----

TEST(nam_loader_destructor_with_pending_model) {
  // The destructor must safely delete a model that was stored as pending
  // but never consumed by process().
  {
    NamLoader nl;
    nl.set_sample_rate(48000);
    ASSERT_TRUE(nl.load_model("../tests/assets/rtneural_test_model.json"));
    // nl goes out of scope: ~NamLoader deletes pending model.
  }
}

TEST(nam_loader_destructor_with_active_model) {
  // The destructor deletes active_model_ directly.
  {
    NamLoader nl;
    nl.set_sample_rate(48000);
    ASSERT_TRUE(nl.load_model("../tests/assets/rtneural_test_model.json"));
    float buf[8] = {};
    nl.process(buf, 8);  // pending → active
                         // nl destroyed: ~NamLoader deletes active_model_.
  }
}

TEST(nam_loader_destructor_with_old_model_to_delete) {
  // The destructor must also clean up old_model_to_delete_.
  {
    NamLoader nl;
    nl.set_sample_rate(48000);

    // First model → active.
    ASSERT_TRUE(nl.load_model("../tests/assets/rtneural_test_model.json"));
    float buf[8] = {};
    nl.process(buf, 8);

    // Second model → active, first → old_model_to_delete_.
    ASSERT_TRUE(nl.load_model("../tests/assets/rtneural_test_model.json"));
    nl.process(buf, 8);

    // nl destroyed with old_model_to_delete_ still populated.
  }
}

// ---- model_path() GC sweep ----

TEST(nam_loader_model_path_sweeps_old_models) {
  // collect_garbage() exchanges old_model_to_delete_; this exercises the
  // deferred GC path on the GUI thread without relying on model_path() side
  // effects.
  NamLoader nl;
  nl.set_sample_rate(48000);

  ASSERT_TRUE(nl.load_model("../tests/assets/rtneural_test_model.json"));
  float buf[8] = {};
  nl.process(buf, 8);  // active = model_1

  ASSERT_TRUE(nl.load_model("../tests/assets/rtneural_test_model.json"));
  nl.process(buf, 8);  // active = model_2, old_to_delete = model_1

  // Explicit GC sweep (formerly done as a side effect of model_path()).
  nl.collect_garbage();
  ASSERT_FALSE(nl.model_path().empty());
}

// ---- check_pending_model: prev_old branch ----

TEST(nam_loader_triple_load_exercises_prev_old_in_pending_swap) {
  // Load 3 models, processing after each, so that when the 3rd is swapped in
  // the "prev_old != nullptr" branch inside check_pending_model fires.
  NamLoader nl;
  nl.set_sample_rate(48000);

  float buf[8] = {0.1f, 0.1f, 0.1f, 0.1f, 0.1f, 0.1f, 0.1f, 0.1f};

  ASSERT_TRUE(nl.load_model("../tests/assets/rtneural_test_model.json"));
  nl.process(buf, 8);  // model_1 → active

  ASSERT_TRUE(nl.load_model("../tests/assets/rtneural_test_model.json"));
  nl.process(buf, 8);  // model_2 → active, model_1 → old_to_delete

  ASSERT_TRUE(nl.load_model("../tests/assets/rtneural_test_model.json"));
  nl.process(buf,
             8);  // model_3 → active, model_2 → old_to_delete (model_1 deleted
                  // via prev_old path)

  for (int i = 0; i < 8; ++i) ASSERT_TRUE(std::isfinite(buf[i]));
}

// ---- Effect base interface ----

TEST(nam_loader_set_sample_rate_is_accepted) {
  NamLoader nl;
  nl.set_sample_rate(44100);
  nl.set_sample_rate(48000);
  nl.set_sample_rate(96000);
}

TEST(nam_loader_enable_disable_toggle) {
  NamLoader nl;
  ASSERT_TRUE(nl.is_enabled());
  nl.set_enabled(false);
  ASSERT_FALSE(nl.is_enabled());
  nl.set_enabled(true);
  ASSERT_TRUE(nl.is_enabled());
}

TEST(nam_loader_clear_without_active_model_then_process_does_not_delete) {
  NamLoader nl;
  nl.set_sample_rate(48000);
  nl.clear_model();
  float buf[8] = {};
  nl.process(buf, 8);
}

TEST(nam_loader_load_empty_json_fails_gracefully) {
  // Write an empty JSON object to a temp file in the build dir (not the
  // source tree) so the test is self-contained.
  const std::string empty_json = "./empty_model_test_tmp.json";
  {
    std::ofstream f(empty_json);
    ASSERT_TRUE(f.is_open());
    f << "{}";
  }
  NamLoader nl;
  nl.set_sample_rate(48000);
  bool ok = nl.load_model(empty_json);
  std::remove(empty_json.c_str());
  ASSERT_FALSE(ok);
}

// ---- Failed replacement retains prior model ----

TEST(nam_loader_failed_replacement_retains_prior_model) {
  // A failed load_model() must NOT clear the previously loaded model.
  // This verifies the behaviour introduced by removing clear_model() from
  // the file-open / parse-null / exception failure paths.
  NamLoader nl;
  nl.set_sample_rate(48000);
  ASSERT_TRUE(nl.load_model("../tests/assets/rtneural_test_model.json"));

  // Attempt to replace with an invalid path.
  bool ok = nl.load_model("/nonexistent/path/model.nam");
  ASSERT_FALSE(ok);

  // Prior model path must be unchanged.
  ASSERT_EQ(nl.model_path(),
            std::string("../tests/assets/rtneural_test_model.json"));
}
