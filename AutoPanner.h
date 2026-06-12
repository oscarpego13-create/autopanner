#pragma once

#include "IPlug_include_in_plug_hdr.h"
#include "IControls.h"

#include <array>
#include <vector>
#include <cmath>
#include <random>
#include <algorithm>

using namespace iplug;
using namespace igraphics;

// ─────────────────────────── Parameter enum ──────────────────────────────
enum EParams
{
  kRate        = 0,  // LFO rate  (0–1 log-mapped to 0.1–10 Hz, or sync division index)
  kDepth       = 1,  // LFO depth (0–100 %)
  kShape       = 2,  // 0=sine 1=tri 2=square
  kPhaseOffset = 3,  // –180 to +180 deg
  kNoiseAmt    = 4,  // noise amplitude (0–100 %)
  kNoiseSpeed  = 5,  // noise velocity  (0–100 %)
  kSpikeAmt    = 6,  // spike amplitude (0–100 %)
  kSpikeDensity= 7,  // spike probability (0–100 %)
  kSync        = 8,  // 0 = Hz mode, 1 = BPM sync
  kNumParams
};

// BPM-sync divisions (bars): 4/1, 2/1, 1/1, 1/2, 1/4, 1/8, 1/16
static constexpr std::array<double,7> kSyncDivBars = {4.0,2.0,1.0,0.5,0.25,0.125,0.0625};
static constexpr const char* kSyncDivLabels[] = {"4/1","2/1","1/1","1/2","1/4","1/8","1/16"};

// ─────────────────────────── Noise / Spike helpers (shared DSP + UI) ─────
static constexpr int kNoiseRes = 64;

struct NoiseState
{
  std::array<float, kNoiseRes> arr {};
  std::mt19937 rng { std::random_device{}() };
  std::uniform_real_distribution<float> dist {-1.f, 1.f};

  void tick(float speed)
  {
    float step = speed * 0.10f;
    if (step >= 0.0005f) {
      for (auto& v : arr) {
        v += dist(rng) * step;
        v = std::clamp(v, -1.f, 1.f);
      }
      // 3-point smoothing
      auto tmp = arr;
      for (int i = 1; i < kNoiseRes - 1; ++i)
        arr[i] = tmp[i]*0.5f + (tmp[i-1]+tmp[i+1])*0.25f;
    }
    // Always remove DC
    float mean = 0.f;
    for (auto v : arr) mean += v;
    mean /= kNoiseRes;
    for (auto& v : arr) v -= mean;
  }

  float sample(float t) const
  {
    float pos = t * (kNoiseRes - 1);
    int   i   = (int)pos;
    int   j   = std::min(i+1, kNoiseRes-1);
    float f   = pos - i;
    return arr[i]*(1.f-f) + arr[j]*f;
  }
};

struct Spike
{
  double phaseAngle; // LFO phase at which spike is anchored
  float  amp;
  int    dir;        // +1 or -1
  int    framesLeft;
};

struct SpikePool
{
  std::vector<Spike> pool;
  std::mt19937 rng { std::random_device{}() };

  void tick(float density, float /*amount*/)
  {
    // Age out
    for (int i = (int)pool.size()-1; i >= 0; --i)
      if (--pool[i].framesLeft <= 0) pool.erase(pool.begin()+i);

    // Spawn
    if (pool.size() < 6) {
      std::uniform_real_distribution<float> rnd(0.f,1.f);
      if (rnd(rng) < density * 0.06f) {
        int burst = 1 + (rnd(rng) > 0.7f ? 1 : 0);
        for (int k = 0; k < burst && pool.size() < 6; ++k) {
          Spike sp;
          sp.phaseAngle = rnd(rng) * M_PI * 2.0;
          sp.amp        = (0.35f + rnd(rng) * 0.65f);
          sp.dir        = rnd(rng) > 0.5f ? 1 : -1;
          sp.framesLeft = 10 + (int)(rnd(rng) * 18.f);
          pool.push_back(sp);
        }
      }
    }
  }
};

// ─────────────────────────── LFO function ────────────────────────────────
inline float lfoSample(double phase, int shape)
{
  switch (shape) {
    case 1:  return (float)((2.0/M_PI) * std::asin(std::sin(phase)));  // tri
    case 2:  return (float)(std::sin(phase) >= 0.0 ? 1.0 : -1.0);      // square
    default: return (float)std::sin(phase);                              // sine
  }
}

// ─────────────────────────── Waveform display control ────────────────────
class WaveformDisplay final : public IControl
{
public:
  WaveformDisplay(const IRECT& bounds, int paramIdxPhase);

  void Draw(IGraphics& g) override;
  void OnAttached() override;
  void OnMouseDrag(float x, float y, float dX, float dY, const IMouseMod& mod) override;
  void OnMouseDown(float x, float y, const IMouseMod& mod) override;

private:
  NoiseState mNoise;
  SpikePool  mSpikes;
  float      mDragStartX     { 0.f };
  double     mDragStartPhase { 0.0 };
};

// ─────────────────────────── Shape selector button ───────────────────────
class ShapeButton final : public IControl
{
public:
  ShapeButton(const IRECT& bounds) : IControl(bounds, kShape) {}

  void Draw(IGraphics& g) override {
    // Sync from param so automation/host changes are reflected
    mCachedShape = std::clamp((int)std::round(GetParam()->Value()), 0, 2);
    const char* names[] = { "Sine", "Tri", "Square" };
    g.FillRoundRect(IColor(255, 48, 48, 52), mRECT, 4.f);
    IText txt(11.f, IColor(255, 195, 195, 200), nullptr, EAlign::Center, EVAlign::Middle);
    g.DrawText(txt, names[mCachedShape], mRECT);
  }

  void OnMouseDown(float, float, const IMouseMod&) override {
    // Use local cache — avoids reading stale param if host queues the update
    mCachedShape = (mCachedShape + 1) % 3;
    SetValue((double)mCachedShape / 2.0);
    SetDirty(true);
  }

private:
  int mCachedShape = 0;
};

// ─────────────────────────── Sync toggle button ──────────────────────────
class SyncButton final : public IControl
{
public:
  SyncButton(const IRECT& bounds) : IControl(bounds, kSync) {}

  void Draw(IGraphics& g) override {
    bool on = GetParam()->Bool();
    IColor bg = on ? IColor(255, 225, 225, 230) : IColor(255, 48, 48, 52);
    IColor fg = on ? IColor(255,  50,  50,  55) : IColor(255, 190, 190, 195);
    g.FillRoundRect(bg, mRECT, 4.f);
    IText txt(11.f, fg, nullptr, EAlign::Center, EVAlign::Middle);
    g.DrawText(txt, "Sync", mRECT);
  }

  void OnMouseDown(float, float, const IMouseMod&) override {
    SetValue(GetValue() < 0.5 ? 1.0 : 0.0);
    SetDirty(true);
  }
};

// ─────────────────────── Plugin title with animated noise effect ─────────
class NoiseTitleControl final : public IControl
{
public:
  NoiseTitleControl(const IRECT& bounds) : IControl(bounds) {}

  void OnAttached() override {
    SetAnimation([](IControl* p){ p->SetDirty(false); }, 0);
  }

  void Draw(IGraphics& g) override {
    ++mFrame;
    const float fsz = 19.f;
    IText titleTxt(fsz, IColor(255, 25, 25, 30), nullptr, EAlign::Near, EVAlign::Middle);

    // Pixel width of "Noise" at 19pt Roboto (approx)
    const float noiseW  = 56.f;
    const IRECT noiseR (mRECT.L,          mRECT.T, mRECT.L + noiseW, mRECT.B);
    const IRECT pannerR(mRECT.L + noiseW, mRECT.T, mRECT.R,          mRECT.B);

    // Slowly-shifting ghost copies give the blur/noise look
    std::mt19937 rng(mFrame / 4);
    std::uniform_real_distribution<float> jit(-1.8f, 1.8f);
    IText ghostTxt(fsz, IColor(28, 25, 25, 30), nullptr, EAlign::Near, EVAlign::Middle);
    for (int i = 0; i < 12; ++i) {
      float dx = jit(rng), dy = jit(rng);
      IRECT sr(noiseR.L + dx, noiseR.T + dy, noiseR.R + dx, noiseR.B + dy);
      g.DrawText(ghostTxt, "Noise", sr);
    }
    g.DrawText(titleTxt, "Noise",  noiseR);
    g.DrawText(titleTxt, "Panner", pannerR);

    SetDirty(false);
  }

private:
  int mFrame = 0;
};

// ─────────────────────────── Main plugin class ───────────────────────────
class NoisePanner final : public Plugin
{
public:
  NoisePanner(const InstanceInfo& info);

#if IPLUG_DSP
  void ProcessBlock(sample** inputs, sample** outputs, int nFrames) override;
  void OnReset() override;
  void OnParamChangeUI(int paramIdx, EParamSource source) override;
#endif
#if IPLUG_EDITOR
  void OnUIOpen() override;
#endif


private:
#if IPLUG_DSP
  // LFO state
  double mPhaseAccum  { 0.0 };

  // DSP-side noise
  NoiseState mDspNoise;
  SpikePool  mDspSpikes;

  // Active spike envelopes for per-sample processing
  struct ActiveSpike {
    double phaseAngle;
    float  amp;
    int    dir;
    float  env;      // decays from 1→0 over lifetime
    float  envDecay;
  };
  std::vector<ActiveSpike> mActiveSpikes;

  // Rate smoothing
  double mSmoothRate  { 0.0 };

  // Counters for control-rate updates
  int mControlTick { 0 };
  static constexpr int kControlPeriod = 64; // samples between noise/spike updates
#endif
};
