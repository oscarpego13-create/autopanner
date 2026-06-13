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
    mCachedShape = std::clamp((int)std::round(GetParam()->Value()), 0, 2);
    static const char* kNames[] = { "SINE", "TRI", "SQUARE" };
    g.FillRect(IColor(255, 22, 22, 22), mRECT);
    IText txt(10.f, IColor(255, 232, 229, 222), nullptr, EAlign::Center, EVAlign::Middle);
    g.DrawText(txt, kNames[mCachedShape], mRECT);
  }

  void OnMouseDown(float, float, const IMouseMod&) override {
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
    IColor bg = on ? IColor(255, 182, 162, 106) : IColor(255, 210, 207, 200);
    IColor fg = on ? IColor(255,  22,  20,  18) : IColor(255, 120, 118, 110);
    g.FillRect(bg, mRECT);
    IText txt(10.f, fg, nullptr, EAlign::Center, EVAlign::Middle);
    g.DrawText(txt, "SYNC", mRECT);
  }

  void OnMouseDown(float, float, const IMouseMod&) override {
    SetValue(GetValue() < 0.5 ? 1.0 : 0.0);
    SetDirty(true);
  }
};

// ─────────────────────────── Brutalist solid knob ────────────────────────
class BrutalistKnob final : public IControl
{
public:
  BrutalistKnob(const IRECT& b, int param, const char* lbl, IColor fill)
    : IControl(b, param), mLabel(lbl), mFill(fill) {}

  void Draw(IGraphics& g) override {
    const float cx  = mRECT.MW();
    const float kR  = mRECT.W() * 0.35f;
    const float kCy = mRECT.T + kR + 3.f;

    g.FillCircle(mFill, cx, kCy, kR);

    double angle = (-135.0 + GetValue() * 270.0) * (M_PI / 180.0);
    float  nx = cx  + (float)(std::sin(angle) * kR * 0.78f);
    float  ny = kCy - (float)(std::cos(angle) * kR * 0.78f);
    IStrokeOptions so; so.mCapOption = ELineCap::Round;
    g.DrawLine(IColor(255, 22, 20, 18), cx, kCy, nx, ny, &so, 2.4f);
    g.FillCircle(IColor(255, 22, 20, 18), cx, kCy, 2.2f);

    WDL_String vs;
    GetParam()->GetDisplay(vs, false);
    const char* u = GetParam()->GetLabel();
    if (u && u[0]) vs.Append(u);

    IText vt(12.f, IColor(255, 22, 20, 18), nullptr, EAlign::Center, EVAlign::Middle);
    g.DrawText(vt, vs.Get(),
               IRECT(mRECT.L, kCy + kR + 4.f, mRECT.R, kCy + kR + 18.f));

    IText lt(9.f, IColor(255, 108, 105, 98), nullptr, EAlign::Center, EVAlign::Middle);
    g.DrawText(lt, mLabel,
               IRECT(mRECT.L, kCy + kR + 18.f, mRECT.R, kCy + kR + 32.f));
  }

  void OnMouseDown(float, float y, const IMouseMod& mod) override {
    mDragY = y; mStartVal = GetValue();
    IControl::OnMouseDown(0, y, mod);
  }
  void OnMouseDblClick(float, float, const IMouseMod&) override {
    SetValue(GetParam()->GetDefault(true));
    SetDirty(true);
  }
  void OnMouseDrag(float, float y, float, float, const IMouseMod& mod) override {
    double sens = mod.S ? 2000.0 : 200.0;
    SetValue(std::clamp(mStartVal - (y - mDragY) / sens, 0.0, 1.0));
    SetDirty(true);
  }
  void OnMouseWheel(float, float, const IMouseMod& mod, float d) override {
    SetValue(std::clamp(GetValue() + d * (mod.S ? 0.002 : 0.01), 0.0, 1.0));
    SetDirty(true);
  }

private:
  const char* mLabel;
  IColor      mFill;
  float       mDragY    {0.f};
  double      mStartVal {0.0};
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
