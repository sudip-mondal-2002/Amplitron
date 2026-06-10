#include "audio/effects/core/effect.h"

#include "audio/effects/core/effect_factory.h"

namespace Amplitron {

std::shared_ptr<Effect> Effect::clone() const {
    auto new_effect = EffectFactory::instance().create(type_id());
    if (new_effect) {
        new_effect->set_params(get_params());
        new_effect->set_enabled(enabled_);
        new_effect->set_mix(mix_);
    }
    return new_effect;
}

}  // namespace Amplitron
