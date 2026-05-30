#pragma once
#include "audio/effects/effect.h"
#include <vector>
#include <string>

namespace Amplitron {

class MockEffect : public Effect {
public:
    std::string mock_name;
    std::vector<EffectParam> mock_params;
    bool is_reset = false;
    bool is_processed = false;

    MockEffect(const std::string& name = "MockEffect") : mock_name(name) {
        mock_params = {
            {"Drive", 0.5f, 0.0f, 1.0f, 0.5f, "", ""},
            {"Level", 0.8f, 0.0f, 2.0f, 0.8f, "", ""},
        };
    }

    const char* name() const override { return mock_name.c_str(); }
    std::vector<EffectParam>& params() override { return mock_params; }
    void process(float* /*buffer*/, int /*num_samples*/) override {
        is_processed = true;
    }
    void reset() override {
        is_reset = true;
    }
};

class MockTunerEffect : public Effect {
public:
    bool processed = false;
    
    MockTunerEffect() : Effect() {}
    void process(float* /*buffer*/, int /*num_samples*/) override {
        processed = true;
    }
    void reset() override {}
    const char* name() const override { return "TestTuner"; }
    std::vector<EffectParam>& params() override { static std::vector<EffectParam> p; return p; }
};

} // namespace Amplitron
