# Amplitron Presets Guide

Welcome to the Amplitron example presets! This collection is designed to demonstrate the versatility and power of the Amplitron DSP engine right out of the box.

**Want to see them in action?** Check out the [Amplitron demo video on YouTube](https://youtu.be/OLGx1zYj0W4) for a first look at how the presets and effects work.

You can load these presets directly from the "Presets" menu in the GUI.

## Included Presets

### `01 Sparkling Clean`
* **Style:** Lush, shimmering clean tone
* **Amp Model:** Clean American (Fender Twin)
* **Active Effects:** Noise Gate, Compressor, Chorus, Delay, Reverb, Amp Sim
* **Use Case:** Perfect for arpeggiated chords, indie pop, and 80s ballads. Uses the Fender Twin emulation along with smooth chorus and warm hall-style reverb.

### `02 Classic Rock Crunch`
* **Style:** Warm, mid-focused driven rock tone
* **Amp Model:** British Crunch (Marshall JCM800)
* **Active Effects:** Noise Gate, Overdrive, Equalizer, Phaser, Reverb, Amp Sim
* **Use Case:** Excellent for classic rock rhythm playing, blues solos, and vintage-inspired tones. The Phaser adds classic 70s character to the Marshall punch.

### `03 Modern Metal Lead`
* **Style:** High-gain, tight-tracking lead tone
* **Amp Model:** High Gain Modern (Mesa Boogie Rectifier)
* **Active Effects:** Noise Gate, Distortion, Equalizer (mid-scooped), Delay, Reverb, Amp Sim
* **Use Case:** Fast alternate picking, sweep picking, and heavy breakdowns. The mid-scoop EQ tightens the tone, and the delay adds space without muddiness.

### `04 Ambient Swells`
* **Style:** Spatially massive, washed-out tone
* **Amp Model:** Jazz Warm (Roland JC-120)
* **Active Effects:** Noise Gate, Compressor (high sustain), Equalizer, Chorus, Flanger, Delay, Reverb, Amp Sim
* **Use Case:** Ambient soundscapes, volume swells, and cinematic textures. Combines thick modulation (chorus + flanger) with long reverb decays for expansive, ethereal tones.

### `05 Phase Shift Lead`
* **Style:** Slow, hypnotic phase sweep on a clean tone
* **Amp Model:** Clean American (Fender Twin)
* **Active Effects:** Noise Gate, Compressor, Equalizer, Phaser, Delay, Reverb, Amp Sim
* **Use Case:** Classic rock solos, funky chords, and psychedelic textures. Inspired by the legendary MXR Phase 90 in 4-stage mode. Subtle delay adds depth to the sweep.

### `06 Jet Flanger`
* **Style:** Dramatic jet-plane flanger sweep with strong resonance
* **Amp Model:** Jazz Warm (Roland JC-120)
* **Active Effects:** Noise Gate, Compressor, Equalizer, Flanger, Reverb, Amp Sim
* **Use Case:** Striking on open chords, harmonics, and slow single-note lines. Inspired by the MXR M117R and Boss BF-2. The high flanger feedback creates that classic "jet" effect.

## Adapting Presets to Your Guitar

The provided presets sound great, but their response depends heavily on your specific guitar's electronics. Here's how to tweak them for the best results:

* **Noise Gate Threshold:** If you're using single-coil pickups, you may hear a 60-cycle hum on the high-gain patches (`03 Modern Metal Lead`). Try lowering the `Threshold` knob on the Noise Gate until the hum disappears while you aren't playing.
* **Input Gain/Amp Gain:** If your guitar uses high-output active pickups (like EMGs), the clean presets might distort slightly. You can counter this by slightly lowering the `Input Gain` in the main UI, or lowering the `Gain` knob directly on the `Amp Sim` effect block.
* **EQ Balancing:** Different woods and pickup styles impart different EQ curves. Use the `Equalizer` effect block to shape the sound. If a tone sounds muddy on your neck pickup, try cutting `Bass` or slightly boosting `Presence`.
