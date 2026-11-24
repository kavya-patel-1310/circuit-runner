#include "app/Audio.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <vector>

namespace cr::app {

namespace {

constexpr int kRate = 22050;
constexpr float kTau = 6.28318530718f;

enum class Osc { Square, Triangle, Sine, Noise, Saw };

struct Synth {
  std::vector<float> samples;
  std::uint32_t noise = 0x12345678u;

  float osc(Osc w, float phase) {
    const float p = phase - std::floor(phase);
    switch (w) {
      case Osc::Square: return p < 0.5f ? 0.6f : -0.6f;
      case Osc::Triangle: return 4.0f * std::fabs(p - 0.5f) - 1.0f;
      case Osc::Sine: return std::sin(p * kTau);
      case Osc::Saw: return (2.0f * p - 1.0f) * 0.6f;
      case Osc::Noise:
        noise ^= noise << 13;
        noise ^= noise >> 17;
        noise ^= noise << 5;
        return static_cast<float>(noise & 0xFFFF) / 32768.0f - 1.0f;
    }
    return 0.0f;
  }

  /// Appends a tone sweeping from f0 to f1 Hz with a quick attack and linear decay.
  Synth& tone(float f0, float f1, float seconds, Osc w, float volume = 0.5f, float vibrato = 0.0f) {
    const int n = static_cast<int>(seconds * kRate);
    float phase = 0.0f;
    for (int i = 0; i < n; ++i) {
      const float t = static_cast<float>(i) / static_cast<float>(n);
      float f = f0 + (f1 - f0) * t;
      if (vibrato > 0.0f) f *= 1.0f + vibrato * std::sin(static_cast<float>(i) / kRate * 30.0f);
      phase += f / kRate;
      const float attack = std::min(1.0f, static_cast<float>(i) / (0.004f * kRate));
      const float env = attack * (1.0f - t);
      samples.push_back(osc(w, phase) * env * volume);
    }
    return *this;
  }

  Synth& rest(float seconds) {
    samples.insert(samples.end(), static_cast<std::size_t>(seconds * kRate), 0.0f);
    return *this;
  }

  Sound build() const {
    Wave w{};
    w.frameCount = static_cast<unsigned int>(samples.size());
    w.sampleRate = kRate;
    w.sampleSize = 16;
    w.channels = 1;
    auto* data = static_cast<short*>(MemAlloc(static_cast<unsigned int>(samples.size() * sizeof(short))));
    for (std::size_t i = 0; i < samples.size(); ++i)
      data[i] = static_cast<short>(std::clamp(samples[i], -1.0f, 1.0f) * 32000.0f);
    w.data = data;
    Sound s = LoadSoundFromWave(w);
    UnloadWave(w);
    return s;
  }
};

}  // namespace

bool AudioSystem::init() {
  InitAudioDevice();
  if (!IsAudioDeviceReady()) return false;
  ready_ = true;

  auto set = [this](Sfx s, const Synth& synth) { sounds_[static_cast<std::size_t>(s)] = synth.build(); };

  set(Sfx::Blip, Synth{}.tone(980, 1180, 0.035f, Osc::Triangle, 0.35f));
  set(Sfx::Blip2, Synth{}.tone(1180, 980, 0.035f, Osc::Triangle, 0.35f));
  set(Sfx::Core, Synth{}.tone(220, 1400, 0.28f, Osc::Square, 0.35f).tone(1400, 900, 0.12f, Osc::Triangle, 0.3f));
  set(Sfx::Quarantine,
      Synth{}.tone(1600, 400, 0.08f, Osc::Square, 0.35f).tone(1200, 300, 0.08f, Osc::Square, 0.3f).tone(800, 1800, 0.1f, Osc::Triangle, 0.3f));
  set(Sfx::Caught, Synth{}.tone(900, 60, 0.9f, Osc::Saw, 0.45f, 0.08f).tone(200, 40, 0.3f, Osc::Noise, 0.25f));
  {
    Synth s;
    for (float f : {523.f, 659.f, 784.f, 1046.f, 784.f, 1046.f}) s.tone(f, f, 0.09f, Osc::Square, 0.3f).rest(0.01f);
    set(Sfx::LevelClear, s);
  }
  set(Sfx::ExtraLife, Synth{}.tone(784, 784, 0.1f, Osc::Triangle, 0.4f).tone(1175, 1175, 0.1f, Osc::Triangle, 0.4f).tone(1568, 1568, 0.2f, Osc::Triangle, 0.4f));
  set(Sfx::KeySpawn, Synth{}.tone(600, 900, 0.07f, Osc::Sine, 0.4f).tone(900, 1350, 0.07f, Osc::Sine, 0.4f));
  set(Sfx::KeyGet, Synth{}.tone(1046, 2093, 0.15f, Osc::Square, 0.3f).tone(2093, 2093, 0.1f, Osc::Triangle, 0.3f));
  set(Sfx::Blink, Synth{}.tone(3000, 200, 0.18f, Osc::Noise, 0.25f));
  set(Sfx::Overclock, Synth{}.tone(150, 900, 0.35f, Osc::Saw, 0.35f, 0.05f));
  set(Sfx::FreezeEnding, Synth{}.tone(500, 500, 0.05f, Osc::Square, 0.2f).rest(0.05f).tone(500, 500, 0.05f, Osc::Square, 0.2f));
  set(Sfx::MenuMove, Synth{}.tone(700, 700, 0.03f, Osc::Square, 0.2f));
  set(Sfx::MenuSelect, Synth{}.tone(700, 1400, 0.09f, Osc::Square, 0.25f));
  return true;
}

void AudioSystem::shutdown() {
  if (!ready_) return;
  for (Sound& s : sounds_) UnloadSound(s);
  CloseAudioDevice();
  ready_ = false;
}

void AudioSystem::play(Sfx sfx) {
  if (!ready_ || muted_) return;
  PlaySound(sounds_[static_cast<std::size_t>(sfx)]);
}

}  // namespace cr::app
