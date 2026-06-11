#include "AutoPanner.h"
#include "IPlug_include_in_plug_src.h"
#include "IControls.h"

// ─────────────────────────── WaveformDisplay ─────────────────────────────

WaveformDisplay::WaveformDisplay(const IRECT& bounds, int paramIdxPhase)
  : IControl(bounds, paramIdxPhase)
{
  SetTooltip("Drag left/right to set phase offset");
}

void WaveformDisplay::Draw(IGraphics& g)
{
  const IRECT& b  = mRECT;
  const float  W  = b.W();
  const float  H  = b.H();
  const float  cy = b.T + H * 0.5f;
  const float  half = H * 0.5f - 10.f;

  // Background
  g.FillRect(IColor(255, 248, 248, 248), b);

  // Center line
  g.DrawLine(IColor(50, 0, 0, 0), b.L, cy, b.R, cy);

  // Read params
  const IPlugAPIBase* api = GetDelegate();
  double depth    = api->GetParam(kDepth)->Value()         / 100.0;
  double phaseOff = api->GetParam(kPhaseOffset)->Value()   / 180.0 * M_PI;
  double noiseAmt = api->GetParam(kNoiseAmt)->Value()      / 100.0;
  float  nSpeed   = (float)(api->GetParam(kNoiseSpeed)->Value()   / 100.0);
  float  spikeA   = (float)(api->GetParam(kSpikeAmt)->Value()     / 100.0);
  float  spikeD   = (float)(api->GetParam(kSpikeDensity)->Value() / 100.0);
  int    shape    = (int)(api->GetParam(kShape)->Value() + 0.5);

  mNoise.tick(nSpeed);
  mSpikes.tick(spikeD, spikeA);

  // Noise haze
  if (noiseAmt > 0.01) {
    float hazeH = (float)(noiseAmt * 0.48 * half * 0.45);
    g.FillRect(IColor(16, 58, 122, 154),
               IRECT(b.L, cy - hazeH, b.R, cy + hazeH));
  }

  // Main waveform
  static const IColor kWaveCol(255, 58, 122, 154);
  IStrokeOptions strokeOpts;
  strokeOpts.mCapOption = ELineCap::Round;

  g.PathClear();
  bool first = true;
  for (int px = 0; px < (int)W; ++px) {
    float  t   = (float)px / W;
    double ph  = t * M_PI * 2.0 + phaseOff;
    float  lfo = lfoSample(ph, shape);
    float  nse = mNoise.sample(t) * (float)noiseAmt * 0.48f;
    float  y   = cy - (lfo * (float)depth + nse) * half;
    if (first) { g.PathMoveTo(b.L + px, y); first = false; }
    else          g.PathLineTo(b.L + px, y);
  }
  g.PathStroke(kWaveCol, 1.6f, strokeOpts);

  // Spikes
  for (const auto& sp : mSpikes.pool) {
    double normX = std::fmod((sp.phaseAngle - phaseOff) / (M_PI * 2.0), 1.0);
    if (normX < 0.0) normX += 1.0;
    float  px    = (float)normX * W + b.L;
    float  lfo   = lfoSample(sp.phaseAngle, shape);
    float  nse   = mNoise.sample((float)normX) * (float)noiseAmt * 0.48f;
    float  baseY = cy - (lfo * (float)depth + nse) * half;
    float  sAmp  = sp.amp * spikeA * half * 1.6f * (float)sp.dir;
    float  tipY  = baseY - sAmp;
    float  bw    = 3.f + sp.amp * spikeA * 5.f;
    float  alpha = std::min(1.f, sp.framesLeft / 5.f) * 0.75f;
    IBlend spikeBlend(EBlend::Default, alpha);
    g.PathClear();
    g.PathMoveTo(px - bw * 0.5f, baseY);
    g.PathLineTo(px, tipY);
    g.PathLineTo(px + bw * 0.5f, baseY);
    g.PathStroke(kWaveCol, 1.2f, strokeOpts, &spikeBlend);
  }

  // Watermark
  IText wm(9.f, IColor(22, 0, 0, 0), nullptr, EAlign::Far, EVAlign::Bottom);
  g.DrawText(wm, "auto panner", IRECT(b.L, b.T, b.R - 8.f, b.B - 5.f));
}

void WaveformDisplay::OnMouseDown(float x, float, const IMouseMod&)
{
  mDragStartX     = x;
  mDragStartPhase = GetDelegate()->GetParam(kPhaseOffset)->Value();
}

void WaveformDisplay::OnMouseDrag(float x, float, float, float, const IMouseMod&)
{
  double dx     = x - mDragStartX;
  double newVal = mDragStartPhase - (dx / mRECT.W()) * 360.0;
  newVal = std::fmod(newVal, 360.0);
  if (newVal < -180.0) newVal += 360.0;
  if (newVal >  180.0) newVal -= 360.0;
  GetDelegate()->SetParameterValue(kPhaseOffset, newVal);
  SetDirty(false);
}

// ─────────────────────────── Plugin constructor ───────────────────────────

AutoPanner::AutoPanner(const InstanceInfo& info)
  : Plugin(info, MakeConfig(kNumParams, 1))
{
  GetParam(kRate)->InitDouble("Rate",          0.3,   0.0,  1.0,   0.001, "");
  GetParam(kDepth)->InitDouble("Depth",        75.0,  0.0,  100.0, 0.1,   "%");
  GetParam(kShape)->InitEnum("Shape",          0,     3,    "",    0, "", "Sine", "Tri", "Square");
  GetParam(kPhaseOffset)->InitDouble("Phase",  0.0, -180.0, 180.0, 0.1, "°");
  GetParam(kNoiseAmt)->InitDouble("Cantidad",  0.0,   0.0,  100.0, 0.1, "%");
  GetParam(kNoiseSpeed)->InitDouble("Velocidad",30.0,  0.0, 100.0, 0.1, "%");
  GetParam(kSpikeAmt)->InitDouble("Cantidad",  0.0,   0.0,  100.0, 0.1, "%");
  GetParam(kSpikeDensity)->InitDouble("Densidad",30.0, 0.0, 100.0, 0.1, "%");
  GetParam(kSync)->InitBool("BPM Sync",        false);

#if IPLUG_EDITOR
  mMakeGraphicsFunc = [&]() {
    return MakeGraphics(*this, PLUG_WIDTH, PLUG_HEIGHT, PLUG_FPS,
                        GetScaleForScreen(PLUG_HEIGHT));
  };

  mLayoutFunc = [&](IGraphics* pG) {
    pG->AttachCornerResizer(EUIResizerMode::Scale, false);
    pG->LoadFont("Roboto-Regular", ROBOTO_FN);

    const IColor kBg      (255, 252, 252, 252);
    const IColor kBlue    (255,  58, 122, 154);
    const IColor kGray    (180,  70,  70,  70);
    const IColor kLight   (255, 240, 240, 240);

    pG->AttachBackground(kBg);

    // ── Waveform display ────────────────────────────────────────────────────
    // Top 160px, padded 16 left/right, 14 top, 4 bottom
    const IRECT full  = pG->GetBounds();
    const IRECT disp  = IRECT(full.L + 16.f, full.T + 14.f,
                               full.R - 16.f, full.T + 158.f);
    pG->AttachControl(new WaveformDisplay(disp, kPhaseOffset));

    // ── Controls row ────────────────────────────────────────────────────────
    // Bottom 110px area, split into 3 equal groups
    const float ctrlT = full.B - 108.f;
    const float ctrlB = full.B -   6.f;
    const float ctrlL = full.L +  10.f;
    const float ctrlR = full.R -  10.f;
    const float gW    = (ctrlR - ctrlL) / 3.f;

    // Group boundaries
    const float g0L = ctrlL,       g0R = ctrlL + gW;      // LFO
    const float g1L = ctrlL + gW,  g1R = ctrlL + gW*2.f;  // Noise
    const float g2L = ctrlL + gW*2.f, g2R = ctrlR;        // Spikes

    // Shared knob style
    IVStyle kvStyle = DEFAULT_STYLE
      .WithColor(kFG, kBlue)
      .WithColor(kBG, kLight)
      .WithColor(kFR, IColor(255,220,220,220))
      .WithColor(kHL, kBlue)
      .WithLabelText(IText(9.f, kGray, nullptr, EAlign::Center))
      .WithValueText(IText(8.f, kGray, nullptr, EAlign::Center))
      .WithDrawShadows(false)
      .WithRoundness(0.5f);

    // Label style for group headers
    IText grpTxt(8.f, IColor(120,100,100,100), nullptr, EAlign::Center);

    // Helper: attach a knob centered inside a column slice
    auto addKnob = [&](float cx, int paramIdx, const char* label) {
      const float kh = 68.f, kw = 52.f;
      IRECT r(cx - kw*0.5f, ctrlT + 2.f, cx + kw*0.5f, ctrlT + 2.f + kh);
      pG->AttachControl(
        new IVKnobControl(r, paramIdx, label, kvStyle, true, false,
                          -135.f, 135.f, -135.f, EDirection::Vertical, 0.01)
      );
    };

    // ── LFO group ──────────────────────────────────────────────────────────
    {
      float mid = (g0L + g0R) * 0.5f;
      addKnob(mid - 30.f, kRate,  "Rate");
      addKnob(mid + 30.f, kDepth, "Depth");

      // Shape: radio buttons (Sine / Tri / Sq)
      IVStyle radioStyle = kvStyle
        .WithLabelText(IText(8.f, kGray, nullptr, EAlign::Center));
      IRECT radioR(g0L + 4.f, ctrlB - 28.f, mid + 12.f, ctrlB - 8.f);
      pG->AttachControl(
        new IVRadioButtonControl(radioR, kShape, {"Sin","Tri","Sq"},
                                 "Shape", radioStyle,
                                 EDirection::Horizontal, 0.f)
      );

      // Sync toggle
      IVStyle synStyle = kvStyle
        .WithLabelText(IText(8.f, kGray, nullptr, EAlign::Center));
      IRECT syncR(mid + 16.f, ctrlB - 28.f, g0R - 4.f, ctrlB - 8.f);
      pG->AttachControl(new IVToggleControl(syncR, kSync, "Sync", synStyle));

      // Group header
      pG->AttachControl(new ITextControl(
        IRECT(g0L, ctrlT - 14.f, g0R, ctrlT), "LFO", grpTxt));
    }

    // ── Noise group ─────────────────────────────────────────────────────────
    {
      float mid = (g1L + g1R) * 0.5f;
      addKnob(mid - 30.f, kNoiseAmt,   "Cantidad");
      addKnob(mid + 30.f, kNoiseSpeed, "Velocidad");
      pG->AttachControl(new ITextControl(
        IRECT(g1L, ctrlT - 14.f, g1R, ctrlT), "RUIDO", grpTxt));
    }

    // ── Spike group ─────────────────────────────────────────────────────────
    {
      float mid = (g2L + g2R) * 0.5f;
      addKnob(mid - 30.f, kSpikeAmt,     "Cantidad");
      addKnob(mid + 30.f, kSpikeDensity, "Densidad");
      pG->AttachControl(new ITextControl(
        IRECT(g2L, ctrlT - 14.f, g2R, ctrlT), "SPIKES", grpTxt));
    }

    // Subtle separator lines (as thin colored panels)
    pG->AttachControl(new IPanelControl(
      IRECT(g0R - 0.5f, ctrlT + 8.f, g0R + 0.5f, ctrlB - 8.f),
      IColor(35, 0, 0, 0)));
    pG->AttachControl(new IPanelControl(
      IRECT(g1R - 0.5f, ctrlT + 8.f, g1R + 0.5f, ctrlB - 8.f),
      IColor(35, 0, 0, 0)));
  };
#endif
}

// ─────────────────────────── DSP ─────────────────────────────────────────

#if IPLUG_DSP
void AutoPanner::OnReset()
{
  mPhaseAccum  = 0.0;
  mControlTick = 0;
  mActiveSpikes.clear();
  mDspNoise    = NoiseState{};
  mDspSpikes   = SpikePool{};
}

void AutoPanner::OnParamChangeUI(int, EParamSource)
{
  if (GetUI()) GetUI()->SetAllControlsDirty();
}

void AutoPanner::ProcessBlock(sample** inputs, sample** outputs, int nFrames)
{
  const double sr        = GetSampleRate();
  const double depth     = GetParam(kDepth)->Value()         / 100.0;
  const double noiseAmt  = GetParam(kNoiseAmt)->Value()      / 100.0;
  const float  noiseSpd  = (float)(GetParam(kNoiseSpeed)->Value()   / 100.0);
  const float  spikeAmt  = (float)(GetParam(kSpikeAmt)->Value()     / 100.0);
  const float  spikeDens = (float)(GetParam(kSpikeDensity)->Value() / 100.0);
  const double phaseOff  = GetParam(kPhaseOffset)->Value()   / 180.0 * M_PI;
  const int    shape     = (int)(GetParam(kShape)->Value() + 0.5);
  const bool   sync      = GetParam(kSync)->Bool();

  // LFO frequency
  double lfoHz;
  if (sync) {
    double bpm = GetTempo();
    if (bpm <= 0.0) bpm = 120.0;
    int divIdx = (int)(GetParam(kRate)->Value() * 6.9999);
    lfoHz = bpm / 60.0 / (4.0 * kSyncDivBars[divIdx]);
  } else {
    lfoHz = 0.1 * std::pow(100.0, GetParam(kRate)->Value()); // 0.1–10 Hz
  }

  const double phaseInc = lfoHz / sr * M_PI * 2.0;

  for (int s = 0; s < nFrames; ++s) {
    // Control-rate updates
    if (mControlTick == 0) {
      mDspNoise.tick(noiseSpd);
      mDspSpikes.tick(spikeDens, spikeAmt);

      // Promote new spikes into active envelope pool
      for (const auto& sp : mDspSpikes.pool) {
        bool found = false;
        for (const auto& as : mActiveSpikes)
          if (std::abs(as.phaseAngle - sp.phaseAngle) < 1e-6) { found = true; break; }
        if (!found && mActiveSpikes.size() < 8) {
          ActiveSpike as;
          as.phaseAngle = sp.phaseAngle;
          as.amp        = sp.amp * spikeAmt;
          as.dir        = sp.dir;
          as.env        = 1.f;
          as.envDecay   = 1.f / (float)(sp.framesLeft * kControlPeriod);
          mActiveSpikes.push_back(as);
        }
      }
    }
    if (++mControlTick >= kControlPeriod) mControlTick = 0;

    // LFO
    double ph  = mPhaseAccum + phaseOff;
    float  lfo = lfoSample(ph, shape);

    // Noise sample (position in wave = current phase normalized 0..1)
    float t   = (float)std::fmod(mPhaseAccum / (M_PI * 2.0), 1.0);
    if (t < 0.f) t += 1.f;
    float nse = mDspNoise.sample(t) * (float)noiseAmt * 0.48f;

    // Spike contribution
    float spikeContrib = 0.f;
    for (auto& as : mActiveSpikes) {
      double dist = std::abs(std::fmod(mPhaseAccum - as.phaseAngle, M_PI * 2.0));
      if (dist > M_PI) dist = M_PI * 2.0 - dist;
      float w = std::max(0.f, 1.f - (float)(dist / (M_PI * 0.15)));
      spikeContrib += as.amp * (float)as.dir * as.env * w;
      as.env -= as.envDecay;
    }
    mActiveSpikes.erase(
      std::remove_if(mActiveSpikes.begin(), mActiveSpikes.end(),
                     [](const ActiveSpike& a) { return a.env <= 0.f; }),
      mActiveSpikes.end());

    // Final pan (–1…+1)
    double pan = std::clamp((double)(lfo * (float)depth + nse + spikeContrib), -1.0, 1.0);

    // Equal-power pan law
    double angle = (pan + 1.0) * 0.25 * M_PI;
    double gainL = std::cos(angle);
    double gainR = std::sin(angle);

    // Mono in → panned stereo out
    sample mono    = (inputs[0][s] + inputs[1][s]) * 0.5;
    outputs[0][s]  = mono * gainL;
    outputs[1][s]  = mono * gainR;

    mPhaseAccum += phaseInc;
    if (mPhaseAccum >= M_PI * 2.0) mPhaseAccum -= M_PI * 2.0;
  }
}
#endif
