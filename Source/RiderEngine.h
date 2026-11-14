#pragma once

#include <algorithm>
#include <cmath>

namespace vp
{

//==============================================================================
// Minimal RBJ biquad used for the detection side-chain (never touches the audio path).
struct Biquad
{
    double b0 = 1, b1 = 0, b2 = 0, a1 = 0, a2 = 0;
    double z1 = 0, z2 = 0;

    void setHighPass (double sr, double f, double q) { design (sr, f, q, true); }
    void setLowPass  (double sr, double f, double q) { design (sr, f, q, false); }
    void reset() { z1 = z2 = 0; }

    inline float process (float x) noexcept
    {
        const double y = b0 * x + z1;
        z1 = b1 * x - a1 * y + z2;
        z2 = b2 * x - a2 * y;
        return (float) y;
    }

private:
    void design (double sr, double f, double q, bool highPass)
    {
        const double w0 = 2.0 * 3.14159265358979323846 * std::min (f, sr * 0.45) / sr;
        const double cw = std::cos (w0), alpha = std::sin (w0) / (2.0 * q);
        const double a0 = 1.0 + alpha;

        if (highPass) { b0 = (1 + cw) / 2; b1 = -(1 + cw); b2 = (1 + cw) / 2; }
        else          { b0 = (1 - cw) / 2; b1 =   1 - cw;  b2 = (1 - cw) / 2; }

        b0 /= a0; b1 /= a0; b2 /= a0;
        a1 = (-2.0 * cw) / a0;
        a2 = (1.0 - alpha) / a0;
    }
};

//==============================================================================
/**
    The level-riding brain.

    For every sample it:
      1. measures the vocal level in the voice band (HPF 110 Hz / LPF 7 kHz, RMS),
      2. decides whether a voice is present (threshold + bleed guard + hold),
      3. optionally shifts the target with the music side-chain ("Music Follow"),
      4. computes the gain needed to reach the target, clamped to the range,
      5. glides towards that gain with a programme-dependent speed.

    While no voice is detected the gain is frozen, so breaths and bleed between
    phrases are not pumped up.
*/
class RiderEngine
{
public:
    struct Settings
    {
        float targetDb    = -18.0f; // desired vocal RMS level
        float detectDb    = -45.0f; // absolute voice-detection threshold
        float bleedGuard  = 0.0f;   // 0..1, raises detection above bleed / noise floor
        float musicFollow = 0.0f;   // 0..1, how much the target follows the music
        float rangeLowDb  = -6.0f;  // maximum cut
        float rangeHighDb = 6.0f;   // maximum boost
        bool  fast        = false;  // response speed
        float lookaheadMs = 0.0f;   // how far the detector runs ahead of the (delayed) audio
    };

    void prepare (double newSampleRate)
    {
        sr = newSampleRate;
        hpf.setHighPass (sr, 110.0, 0.707);
        lpf.setLowPass  (sr, 7000.0, 0.707);
        musicHpf.setHighPass (sr, 60.0, 0.707);

        holdSamples = (int) (0.25 * sr);
        floorRisePerSample = 2.5f / (float) sr; // noise floor creeps up 2.5 dB/s
        floorFallCoef  = coef (0.05);
        fastCoef       = (double) coef (0.010);
        midCoef        = (double) coef (0.030);
        seedSamples    = (int) (0.040 * sr);
        updateLookahead();
        musicShortCoef = coef (0.4);
        musicLongCoef  = coef (4.0);
        updateSpeed();
        reset();
    }

    void reset()
    {
        hpf.reset(); lpf.reset(); musicHpf.reset();
        vocalMs = fastMs = midMs = musicShortMs = musicLongMs = 0.0;
        seedCounter = catchUpCounter = 0;
        gainDb = 0.0f;
        noiseFloorDb = -90.0f;
        holdCounter = 0;
        active = riding = false;
    }

    void setSettings (const Settings& s)
    {
        const bool speedChanged = s.fast != settings.fast;
        const bool lookaheadChanged = std::abs (s.lookaheadMs - settings.lookaheadMs) > 0.01f;
        settings = s;
        if (speedChanged) updateSpeed();
        if (lookaheadChanged) updateLookahead();
    }

    /** Feeds one sample of (mono) vocal + music and returns the computed rider gain in dB. */
    inline float processSample (float vocal, float music, bool hasMusic) noexcept
    {
        // --- 1. vocal level --------------------------------------------------
        const float v = lpf.process (hpf.process (vocal));
        const double v2 = (double) v * v;
        vocalMs = detCoef * vocalMs + (1.0 - detCoef) * v2;
        fastMs  = fastCoef * fastMs + (1.0 - fastCoef) * v2;
        midMs   = midCoef  * midMs  + (1.0 - midCoef)  * v2;

        // At the start of a phrase the slow detector lags behind; seed it from the 30 ms detector
        // so the rider gets a correct level reading right away.
        if (seedCounter > 0)
        {
            --seedCounter;
            vocalMs = std::max (vocalMs, midMs);
        }
        vocalDb = toDb (vocalMs);
        const float fastDb = toDb (fastMs);

        // --- music level (side-chain) ----------------------------------------
        if (hasMusic)
        {
            const float m = musicHpf.process (music);
            const double m2 = (double) m * m;
            musicShortMs = musicShortCoef * musicShortMs + (1.0 - musicShortCoef) * m2;
            musicLongMs  = musicLongCoef  * musicLongMs  + (1.0 - musicLongCoef)  * m2;
            musicDb = toDb (musicShortMs);
        }
        else
        {
            musicShortMs = musicLongMs = 0.0;
            musicDb = -100.0f;
        }

        // --- adaptive noise floor (used by bleed guard without side-chain) ----
        if (vocalDb < noiseFloorDb)
            noiseFloorDb += (vocalDb - noiseFloorDb) * (1.0f - floorFallCoef);
        else
            noiseFloorDb = std::min (noiseFloorDb + floorRisePerSample, vocalDb);

        // --- 2. voice detection ---------------------------------------------
        float thr = settings.detectDb;
        if (settings.bleedGuard > 0.001f)
        {
            float guardThr;
            if (hasMusic && musicDb > -80.0f)
                guardThr = musicDb - lerp (40.0f, 10.0f, settings.bleedGuard); // bleed sits well below the music
            else
                guardThr = noiseFloorDb + lerp (4.0f, 20.0f, settings.bleedGuard);
            thr = std::max (thr, guardThr);
        }
        effectiveThresholdDb = thr;

        if (vocalDb > thr && fastDb > thr)
        {
            if (! active)
            {
                // new phrase: seed the detector and (with lookahead) allow a fast catch-up glide,
                // which lands on the right gain before the delayed audio arrives
                seedCounter = seedSamples;
                catchUpCounter = lookaheadSamples;
                vocalMs = std::max (vocalMs, midMs);
                vocalDb = toDb (vocalMs);
            }
            active = true;
            holdCounter = holdSamples;
        }
        else if (fastDb < thr - 3.0f)
        {
            if (holdCounter > 0) --holdCounter;
            else active = false;
        }

        // --- 3. target (optionally following the music) ----------------------
        float target = settings.targetDb;
        if (hasMusic && settings.musicFollow > 0.001f && toDb (musicLongMs) > -70.0f)
        {
            const float diff = std::clamp (musicDb - toDb (musicLongMs), -6.0f, 6.0f);
            target += settings.musicFollow * diff;
        }
        effectiveTargetDb = target;

        // --- 4/5. gain computer + glide --------------------------------------
        // Only ride while the voice is really sounding and the slow detector is trustworthy:
        // during phrase onsets/decays the fast and slow envelopes disagree, and riding there
        // would pump up the tail of a phrase or the attack of the next one.
        const bool steady = std::abs (fastDb - vocalDb) < 9.0f;
        riding = active && fastDb > thr && steady;

        if (riding)
        {
            const float desired = std::clamp (target - vocalDb, settings.rangeLowDb, settings.rangeHighDb);
            float c = desired < gainDb ? downCoef : upCoef;
            if (catchUpCounter > 0)
                c = catchUpCoef;
            gainDb = desired + (gainDb - desired) * c;
        }
        else
        {
            // Frozen while idle, but still respect a range that was narrowed meanwhile.
            gainDb = std::clamp (gainDb, settings.rangeLowDb, settings.rangeHighDb);
        }

        if (catchUpCounter > 0)
            --catchUpCounter;

        return gainDb;
    }

    float getGainDb() const noexcept               { return gainDb; }
    float getVocalDb() const noexcept              { return vocalDb; }
    float getMusicDb() const noexcept              { return musicDb; }
    float getEffectiveTargetDb() const noexcept    { return effectiveTargetDb; }
    float getEffectiveThresholdDb() const noexcept { return effectiveThresholdDb; }
    bool  isActive() const noexcept                { return active; }
    bool  isRiding() const noexcept                { return riding; }

    static inline float toDb (double meanSquare) noexcept
    {
        return (float) (10.0 * std::log10 (meanSquare + 1.0e-10));
    }

private:
    float coef (double seconds) const { return (float) std::exp (-1.0 / (seconds * sr)); }
    static float lerp (float a, float b, float t) { return a + (b - a) * t; }

    void updateLookahead()
    {
        lookaheadSamples = (int) (settings.lookaheadMs * 0.001 * sr);
        // the catch-up glide must settle well inside the lookahead window
        catchUpCoef = coef (std::clamp (settings.lookaheadMs * 0.001 / 4.0, 0.004, 0.025));
    }

    void updateSpeed()
    {
        // Detector integrates over a phrase-friendly window; the gain glides slower still.
        detCoef  = (double) coef (settings.fast ? 0.045 : 0.110);
        upCoef   = coef (settings.fast ? 0.070 : 0.320);
        downCoef = coef (settings.fast ? 0.045 : 0.200);
    }

    Settings settings;
    double sr = 44100.0;

    Biquad hpf, lpf, musicHpf;
    double vocalMs = 0, musicShortMs = 0, musicLongMs = 0;
    double detCoef = 0.999, fastCoef = 0.99, fastMs = 0, midCoef = 0.99, midMs = 0;
    float catchUpCoef = 0.99f;
    int seedSamples = 0, seedCounter = 0, lookaheadSamples = 0, catchUpCounter = 0;
    float upCoef = 0.999f, downCoef = 0.999f;
    float musicShortCoef = 0.999f, musicLongCoef = 0.9999f, floorFallCoef = 0.999f;
    float floorRisePerSample = 0.0f;

    float vocalDb = -100.0f, musicDb = -100.0f, noiseFloorDb = -90.0f;
    float gainDb = 0.0f, effectiveTargetDb = -18.0f, effectiveThresholdDb = -45.0f;
    int holdSamples = 0, holdCounter = 0;
    bool active = false, riding = false;
};

} // namespace vp
