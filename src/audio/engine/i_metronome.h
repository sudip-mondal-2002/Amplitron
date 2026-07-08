#pragma once

namespace Amplitron {

/**
 * @brief Interface for metronome engine.
 * Satisfies the Dependency Inversion Principle (DIP).
 */
class IMetronome {
   public:
    virtual ~IMetronome() = default;

    virtual void set_enabled(bool enabled) = 0;
    virtual bool is_enabled() const = 0;
    virtual void toggle() = 0;

    virtual void set_bpm(int bpm) = 0;
    virtual int get_bpm() const = 0;

    virtual void set_volume(float volume) = 0;
    virtual float get_volume() const = 0;

    virtual void set_sample_rate(int sample_rate) = 0;
    virtual void reset() = 0;

    // Generate next click sample
    virtual float next_sample() = 0;
};

}  // namespace Amplitron
