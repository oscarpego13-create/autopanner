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

  // Read params via delegate (IGEditorDelegate::GetParam is available)
  auto* del = GetDelegate();
  double depth    = del->GetParam(kDepth)->Value()         / 100.0;
  double phaseOff = del->GetParam(kPhaseOffset)->Value()   / 180.0 * M_PI;
  double noiseAmt = del->GetParam(kNoiseAmt)->Value()      / 100.0;
  float  nSpeed   = (float)(del->GetParam(kNoiseSpeed)->Value()   / 100.0);
  float  spikeA   = (float)(del->GetParam(kSpikeAmt)->Value()     / 100.0);
  float  spikeD   = (float)(del->GetParam(kSpikeDensity)->Value() / 100.0);
  int    shape    = (int)(del->GetParam(kShape)->Value() + 0.5);

  // Background — warm beige matching plugin bg
  g.FillRect(IColor(255, 237, 234, 227), b);

  // Header label top-right
  {
    IText hdr(10.f, IColor(140, 22, 20, 18), nullptr, EAlign::Far, EVAlign::Middle);
    g.DrawText(hdr, "noise panner  \xc2\xb7  v1.0",
               IRECT(b.L, b.T + 4.f, b.R - 8.f, b.T + 18.f));
  }

  // Subtle center axis
  g.DrawLine(IColor(18, 0, 0, 0), b.L, cy, b.R, cy, nullptr, 1.f);

  mNoise.tick(nSpeed);
  mSpikes.tick(spikeD, spikeA);

  // Main waveform
  static const IColor kWaveCol(255, 22, 20, 18);
  IStrokeOptions strokeOpts;
  strokeOpts.mCapOption = ELineCap::Round;

  g.PathClear();
  bool first = true;
  for (int px = 0; px < (int)W; ++px) {
    float  t   = (float)px / W;
    double ph  = t * M_PI * 2.0 + phaseOff;
    float  lfo = lfoSample(ph, shape);
    float  nse = mNoise.sample(t) * (float)noiseAmt * 0.70f;
    float  y   = cy - (lfo * (float)depth + nse) * half;
    if (first) { g.PathMoveTo(b.L + px, y); first = false; }
    else          g.PathLineTo(b.L + px, y);
  }
  g.PathStroke(kWaveCol, 1.6f, strokeOpts);

  // Spikes — base points follow the waveform curve at their exact X positions
  for (const auto& sp : mSpikes.pool) {
    double normX = std::fmod((sp.phaseAngle - phaseOff) / (M_PI * 2.0), 1.0);
    if (normX < 0.0) normX += 1.0;
    float  px    = (float)normX * W + b.L;
    float  lfo   = lfoSample(sp.phaseAngle, shape);
    float  nse   = mNoise.sample((float)normX) * (float)noiseAmt * 0.70f;
    float  baseY = cy - (lfo * (float)depth + nse) * half;
    float  sAmp  = sp.amp * spikeA * half * 1.6f * (float)sp.dir;
    float  tipY  = baseY - sAmp;
    float  bw    = 3.f + sp.amp * spikeA * 5.f;
    float  alpha = std::min(1.f, sp.framesLeft / 5.f) * 0.75f;
    IBlend spikeBlend(EBlend::Default, alpha);

    // Evaluate waveform at the left and right base corners of the spike
    auto waveY = [&](float screenX) -> float {
      float t = std::clamp((screenX - b.L) / W, 0.f, 1.f);
      float y = lfoSample(t * M_PI * 2.0 + phaseOff, shape) * (float)depth
                + mNoise.sample(t) * (float)noiseAmt * 0.70f;
      return cy - y * half;
    };
    float lBaseY = waveY(px - bw * 0.5f);
    float rBaseY = waveY(px + bw * 0.5f);

    g.PathClear();
    g.PathMoveTo(px - bw * 0.5f, lBaseY);
    g.PathLineTo(px, tipY);
    g.PathLineTo(px + bw * 0.5f, rBaseY);
    g.PathStroke(kWaveCol, 1.2f, strokeOpts, &spikeBlend);
  }

  // (watermark removed — header at top)

  // Keep animating every frame
  SetDirty(false);
}

void WaveformDisplay::OnAttached()
{
  // Request a redraw every frame so noise + LFO phase animate continuously
  SetAnimation([](IControl* pCtrl){ pCtrl->SetDirty(false); }, 0);
}

void WaveformDisplay::OnMouseDown(float x, float, const IMouseMod&)
{
  mDragStartX     = x;
  // GetParam() returns the IParam* for our associated parameter (kPhaseOffset)
  mDragStartPhase = GetParam()->Value();  // raw degrees -180..+180
}

void WaveformDisplay::OnMouseDrag(float x, float, float, float, const IMouseMod&)
{
  double dx     = x - mDragStartX;
  double newVal = mDragStartPhase - (dx / mRECT.W()) * 360.0;
  while (newVal >  180.0) newVal -= 360.0;
  while (newVal < -180.0) newVal += 360.0;
  // iPlug2 expects normalized 0..1 for SetValue()
  SetValue((newVal + 180.0) / 360.0);
  SetDirty(true);
}

// ─────────────────────────── Plugin constructor ───────────────────────────

NoisePanner::NoisePanner(const InstanceInfo& info)
  : Plugin(info, MakeConfig(kNumParams, 1))
{
  GetParam(kRate)->InitDouble("Rate",           0.3,   0.0,  1.0,   0.001, "");
  GetParam(kRate)->SetDisplayFunc([this](double value, WDL_String& str) {
    if (GetParam(kSync)->Bool()) {
      int idx = (int)(value * 6.9999);
      str.Set(kSyncDivLabels[idx]);
    } else {
      double hz = 0.1 * std::pow(100.0, value);
      str.SetFormatted(32, hz >= 1.0 ? "%.1f Hz" : "%.2f Hz", hz);
    }
  });
  GetParam(kDepth)->InitDouble("Depth",         75.0,  0.0,  100.0, 0.1,   "%");
  GetParam(kShape)->InitEnum("Shape",           0,     3,    "",    0, "", "Sine", "Tri", "Square");
  GetParam(kPhaseOffset)->InitDouble("Phase",   0.0, -180.0, 180.0, 0.1, "°");
  GetParam(kNoiseAmt)->InitDouble("Cantidad",   0.0,   0.0,  100.0, 0.1, "%");
  GetParam(kNoiseSpeed)->InitDouble("Velocidad",30.0,  0.0,  100.0, 0.1, "%");
  GetParam(kSpikeAmt)->InitDouble("Longitud",   0.0,   0.0,  100.0, 0.1, "%");
  GetParam(kSpikeDensity)->InitDouble("Densidad",30.0, 0.0,  100.0, 0.1, "%");
  GetParam(kSync)->InitBool("BPM Sync",         false);

#if IPLUG_EDITOR
  mMakeGraphicsFunc = [&]() {
    // GetScaleForScreen takes width AND height in current iPlug2
    return MakeGraphics(*this, PLUG_WIDTH, PLUG_HEIGHT, PLUG_FPS,
                        GetScaleForScreen(PLUG_WIDTH, PLUG_HEIGHT));
  };

  mLayoutFunc = [&](IGraphics* pG) {
    pG->AttachCornerResizer(EUIResizerMode::Scale, false);
    pG->LoadFont("Roboto-Regular", ROBOTO_FN);
    pG->EnableMouseOver(true);

    // Warm beige background
    pG->AttachPanelBackground(IColor(255, 237, 234, 227));

    const IRECT full = pG->GetBounds();

    // ── Waveform display ──────────────────────────────────────────────────
    const IRECT disp(full.L + 10.f, full.T + 8.f,
                     full.R - 10.f, full.T + 162.f);
    pG->AttachControl(new WaveformDisplay(disp, kPhaseOffset));

    // Logo tooltip: hit zone = top-right of waveform, popup extends below
    pG->AttachControl(new LogoControl(
      IRECT(disp.R - 172.f, disp.T + 10.f, disp.R - 6.f, disp.T + 58.f)));

    // ── Controls area ─────────────────────────────────────────────────────
    const float ctrlT = full.T + 170.f;
    const float ctrlL = full.L +   8.f;
    const float ctrlR = full.R -   8.f;
    const float gW    = (ctrlR - ctrlL) / 3.f;

    // Group fill colors
    static const IColor kLfoFill (255, 155, 152, 144);  // warm gray
    static const IColor kNosFill (255, 130, 170, 198);  // dusty blue
    static const IColor kSpkFill (255, 182, 162, 106);  // tan / khaki

    IText grpTxt(8.5f, IColor(255, 100, 98, 92), nullptr, EAlign::Center, EVAlign::Middle);

    // Each knob: 62px wide, 80px tall (circle + value + label all inside bounds)
    const float kKW   = 62.f;
    const float kKH   = 80.f;
    const float kHalf = kKW * 0.5f + 3.f;  // lateral offset from group center to knob center

    auto addKnob = [&](float cx, int param, const char* lbl, IColor fill) {
      pG->AttachControl(new BrutalistKnob(
        IRECT(cx - kKW * 0.5f, ctrlT + 16.f,
              cx + kKW * 0.5f, ctrlT + 16.f + kKH),
        param, lbl, fill));
    };

    // ── LFO ──────────────────────────────────────────────────────────────
    {
      float gmid = ctrlL + gW * 0.5f;
      pG->AttachControl(new ITextControl(
        IRECT(ctrlL, ctrlT, ctrlL + gW, ctrlT + 14.f), "LFO", grpTxt));
      addKnob(gmid - kHalf, kRate,  "RATE",  kLfoFill);
      addKnob(gmid + kHalf, kDepth, "DEPTH", kLfoFill);
      float btnT = ctrlT + 16.f + kKH + 4.f;
      float btnB = btnT + 18.f;
      pG->AttachControl(new ShapeButton(IRECT(ctrlL + 4.f,  btnT, ctrlL + 46.f,      btnB)));
      pG->AttachControl(new SyncButton (IRECT(ctrlL + 50.f, btnT, ctrlL + gW - 4.f,  btnB)));
    }

    // ── Ruido ─────────────────────────────────────────────────────────────
    {
      float gmid = ctrlL + gW * 1.5f;
      pG->AttachControl(new ITextControl(
        IRECT(ctrlL + gW, ctrlT, ctrlL + gW * 2.f, ctrlT + 14.f), "RUIDO", grpTxt));
      addKnob(gmid - kHalf, kNoiseAmt,   "CANTIDAD",  kNosFill);
      addKnob(gmid + kHalf, kNoiseSpeed, "VELOCIDAD", kNosFill);
    }

    // ── Spikes ────────────────────────────────────────────────────────────
    {
      float gmid = ctrlL + gW * 2.5f;
      pG->AttachControl(new ITextControl(
        IRECT(ctrlL + gW * 2.f, ctrlT, ctrlR, ctrlT + 14.f), "SPIKES", grpTxt));
      addKnob(gmid - kHalf, kSpikeAmt,     "LONGITUD", kSpkFill);
      addKnob(gmid + kHalf, kSpikeDensity, "DENSIDAD", kSpkFill);
    }

  };
#endif
}

// ─────────────────────────── UI helpers ──────────────────────────────────

#if IPLUG_EDITOR
void NoisePanner::OnUIOpen()
{
  if (GetUI()) GetUI()->SetAllControlsDirty();
}
#endif

// ─────────────────────────── DSP ─────────────────────────────────────────

#if IPLUG_DSP
void NoisePanner::OnReset()
{
  mPhaseAccum  = 0.0;
  mControlTick = 0;
  mSlewPan     = 0.f;
  mActiveSpikes.clear();
  mDspNoise    = NoiseState{};
  mDspSpikes   = SpikePool{};
}

void NoisePanner::OnParamChangeUI(int, EParamSource)
{
  if (GetUI()) GetUI()->SetAllControlsDirty();
}

void NoisePanner::ProcessBlock(sample** inputs, sample** outputs, int nFrames)
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

  double lfoHz;
  if (sync) {
    double bpm = GetTempo();
    if (bpm <= 0.0) bpm = 120.0;
    int divIdx = (int)(GetParam(kRate)->Value() * 6.9999);
    lfoHz = bpm / 60.0 / (4.0 * kSyncDivBars[divIdx]);
  } else {
    lfoHz = 0.1 * std::pow(100.0, GetParam(kRate)->Value());
  }

  const double phaseInc = lfoHz / sr * M_PI * 2.0;

  for (int s = 0; s < nFrames; ++s) {
    if (mControlTick == 0) {
      mDspNoise.tick(noiseSpd);
      mDspSpikes.tick(spikeDens, spikeAmt);

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

    double ph  = mPhaseAccum + phaseOff;
    float  lfo = lfoSample(ph, shape);

    float t   = (float)std::fmod(mPhaseAccum / (M_PI * 2.0), 1.0);
    if (t < 0.f) t += 1.f;
    float nse = mDspNoise.sample(t) * (float)noiseAmt * 0.70f;

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

    double pan = std::clamp((double)(lfo * (float)depth + nse + spikeContrib), -1.0, 1.0);

    // Slew limiter: cap rate of pan movement to ~5 ms full-range transition.
    // This eliminates clicks from square-wave jumps and noise extremes without
    // audibly dulling the LFO at normal speeds (sine/tri max rate << maxSlew).
    const float maxSlew = 2.f / (float)(sr * 0.005);
    float slewDelta = (float)pan - mSlewPan;
    mSlewPan += std::clamp(slewDelta, -maxSlew, maxSlew);
    pan = mSlewPan;

    // Balance-style gain law — preserves the original stereo image.
    // L and R are attenuated independently and are never summed to mono.
    // At pan == 0 both gains are exactly 1.0, so with no modulation
    // (Depth/Cantidad/Densidad all at 0) the signal passes through
    // completely unchanged: 0 dB, full stereo width intact.
    double gainL = pan <= 0.0 ? 1.0 : 1.0 - pan;
    double gainR = pan >= 0.0 ? 1.0 : 1.0 + pan;

    outputs[0][s] = inputs[0][s] * gainL;
    outputs[1][s] = inputs[1][s] * gainR;

    mPhaseAccum += phaseInc;
    if (mPhaseAccum >= M_PI * 2.0) mPhaseAccum -= M_PI * 2.0;
  }
}
#endif
