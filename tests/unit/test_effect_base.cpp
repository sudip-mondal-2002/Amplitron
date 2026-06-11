#include <cmath>
#include <memory>
#include <string>
#include <vector>

#include "audio/effects/core/effect.h"
#include "audio/effects/core/effect_factory.h"
#include "test_framework.h"

using namespace Amplitron;

class MockEffect : public Effect {
   public:
    MockEffect() {
        params_.push_back({"Param1", 5.0f, 0.0f, 10.0f, 5.0f, "Hz", "Tooltip1"});
        params_.push_back({"Param2", 0.5f, 0.0f, 1.0f, 0.5f, "%", "Tooltip2"});
    }

    void process(float* buffer, int num_samples) override {
        float scale = get_param_value("Param2");
        for (int i = 0; i < num_samples; ++i) {
            buffer[i] *= scale;
        }
    }

    void reset() override {}

    const char* name() const override { return "MockEffect"; }
    const char* type_id() const override { return "MockEffect"; }

    std::vector<EffectParam>& params() override { return params_; }
    const std::vector<EffectParam>& params() const override { return params_; }

   private:
    std::vector<EffectParam> params_;
};

TEST(get_param_names) {
    MockEffect fx;
    auto names = fx.get_param_names();
    ASSERT_EQ(names.size(), 2);
    ASSERT_EQ(names[0], "Param1");
    ASSERT_EQ(names[1], "Param2");
}

TEST(get_param_value_existing) {
    MockEffect fx;
    ASSERT_NEAR(fx.get_param_value("Param1"), 5.0f, 0.001f);
    ASSERT_NEAR(fx.get_param_value("Param2"), 0.5f, 0.001f);
}

TEST(get_param_value_missing) {
    MockEffect fx;
    ASSERT_NEAR(fx.get_param_value("Nonexistent"), 0.0f, 0.001f);
}

TEST(set_param_by_name_existing) {
    MockEffect fx;
    fx.set_param_by_name("Param1", 8.0f);
    ASSERT_NEAR(fx.get_param_value("Param1"), 8.0f, 0.001f);

    // Test clamping
    fx.set_param_by_name("Param1", 15.0f);
    ASSERT_NEAR(fx.get_param_value("Param1"), 10.0f, 0.001f);

    fx.set_param_by_name("Param1", -5.0f);
    ASSERT_NEAR(fx.get_param_value("Param1"), 0.0f, 0.001f);
}

TEST(set_param_by_name_missing) {
    MockEffect fx;
    fx.set_param_by_name("Nonexistent", 9.9f);
    ASSERT_NEAR(fx.get_param_value("Param1"), 5.0f, 0.001f);
    ASSERT_NEAR(fx.get_param_value("Param2"), 0.5f, 0.001f);
}

TEST(get_json_serialization_roundtrip) {
    MockEffect fx;
    fx.set_enabled(false);
    fx.set_mix(0.75f);
    fx.set_param_by_name("Param1", 4.2f);
    fx.set_param_by_name("Param2", 0.12f);

    nlohmann::json j = fx.get_params();

    MockEffect fx2;
    fx2.set_params(j);

    ASSERT_EQ(fx2.is_enabled(), false);
    ASSERT_NEAR(fx2.get_mix(), 0.75f, 0.001f);
    ASSERT_NEAR(fx2.get_param_value("Param1"), 4.2f, 0.001f);
    ASSERT_NEAR(fx2.get_param_value("Param2"), 0.12f, 0.001f);
}

TEST(process_stereo_default) {
    MockEffect fx;
    fx.set_mix(1.0f);
    float left[4] = {1.0f, 2.0f, 3.0f, 4.0f};
    float right[4] = {0.0f, 0.0f, 0.0f, 0.0f};

    fx.process_stereo(left, right, 4);

    // Scaling is by 0.5f
    ASSERT_NEAR(left[0], 0.5f, 0.001f);
    ASSERT_NEAR(left[1], 1.0f, 0.001f);
    ASSERT_NEAR(left[2], 1.5f, 0.001f);
    ASSERT_NEAR(left[3], 2.0f, 0.001f);

    ASSERT_NEAR(right[0], 0.5f, 0.001f);
    ASSERT_NEAR(right[1], 1.0f, 0.001f);
    ASSERT_NEAR(right[2], 1.5f, 0.001f);
    ASSERT_NEAR(right[3], 2.0f, 0.001f);
}

TEST(clone_preserves_params) {
    auto fx = EffectFactory::instance().create("Overdrive");
    ASSERT_TRUE(fx != nullptr);

    fx->set_enabled(false);
    fx->set_mix(0.65f);
    if (!fx->params().empty()) {
        fx->params()[0].value = fx->params()[0].min_val + 1.0f;
    }

    auto cloned = fx->clone();
    ASSERT_TRUE(cloned != nullptr);
    ASSERT_EQ(std::string(cloned->name()), std::string(fx->name()));
    ASSERT_EQ(cloned->is_enabled(), fx->is_enabled());
    ASSERT_NEAR(cloned->get_mix(), fx->get_mix(), 0.001f);
    if (!fx->params().empty()) {
        ASSERT_NEAR(cloned->params()[0].value, fx->params()[0].value, 0.001f);
    }
}

TEST(clone_unknown_type_returns_nullptr) {
    MockEffect unregistered_fx;
    auto cloned = unregistered_fx.clone();
    ASSERT_TRUE(cloned == nullptr);
}

TEST(factory_create_known) {
    auto fx = EffectFactory::instance().create("Overdrive");
    ASSERT_TRUE(fx != nullptr);
    ASSERT_EQ(std::string(fx->name()), "Overdrive");
}

TEST(factory_create_unknown) {
    auto fx = EffectFactory::instance().create("UnknownEffectType123");
    ASSERT_TRUE(fx == nullptr);
}

TEST(factory_registered_types) {
    auto types = EffectFactory::instance().registered_types();
    ASSERT_TRUE(!types.empty());
    bool found_od = false;
    for (const auto& t : types) {
        if (t == "Overdrive") {
            found_od = true;
            break;
        }
    }
    ASSERT_TRUE(found_od);
}
