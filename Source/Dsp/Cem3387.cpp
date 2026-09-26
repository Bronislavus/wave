#include "Cem3387.h"

#include <cmath>
#include <mutex>
#include <unordered_map>

namespace wave::dsp
{
std::shared_ptr<const Cem3387::CutoffCoefficientTable>
Cem3387::coefficientTableForSampleRate(double targetSampleRate)
{
    static std::mutex cacheMutex;
    static std::unordered_map<int, std::weak_ptr<const CutoffCoefficientTable>> cache;
    const auto key = juce::roundToInt(targetSampleRate);
    const std::scoped_lock lock(cacheMutex);
    if (const auto found = cache.find(key); found != cache.end())
        if (auto existing = found->second.lock())
            return existing;

    auto table = std::make_shared<CutoffCoefficientTable>();
    constexpr auto minimumCv = -0.125f;
    constexpr auto maximumCv = 1.125f;
    constexpr auto cutoffRangeOctaves = 127.0f / 12.0f;
    for (size_t index = 0; index < table->size(); ++index)
    {
        const auto unit = static_cast<float>(index)
                          / static_cast<float>(table->size() - 1);
        const auto cv = minimumCv + unit * (maximumCv - minimumCv);
        // Measurements from a Wave voice card at full resonance place the
        // oscillation at approximately 28 Hz, 957 Hz and 7.78 kHz for Sound
        // cutoff values 0, 62 and 100. The nominal OS table alone produces
        // 20 Hz, 710 Hz and 5.88 kHz in this four-pole topology. This stable
        // 1.35 analogue scale factor belongs at the CEM control-law boundary,
        // while the firmware-facing semitone table remains unchanged.
        constexpr auto waveCemFrequencyScale = 1.35f;
        const auto cutoff = juce::jlimit(
            12.0f, static_cast<float>(targetSampleRate * 0.225),
            waveCemFrequencyScale * 20.0f
                * std::exp2(cv * cutoffRangeOctaves));
        const auto g = std::tan(juce::MathConstants<float>::pi * cutoff
                                / static_cast<float>(targetSampleRate));
        (*table)[index] = g / (1.0f + g);
    }
    cache[key] = table;
    return table;
}

void Cem3387::prepare(double hostSampleRate, float voiceTolerance) noexcept
{
    sampleRate = juce::jmax(1.0, hostSampleRate * 2.0);
    cutoffCoefficientTable = coefficientTableForSampleRate(sampleRate);
    tolerance = juce::jlimit(-1.0f, 1.0f, voiceTolerance);
    cutoffCalibrationManuallySet = false;
    cutoffTrimCode = 0x0800u;
    couplingCoefficient = std::exp(-juce::MathConstants<float>::twoPi * 5.0f
                                   / static_cast<float>(sampleRate * 0.5));
    reset();
}

void Cem3387::reset() noexcept
{
    integrators.fill(0.0f);
    previousInput = 0.0f;
    couplingInput = 0.0f;
    couplingOutput = 0.0f;
    targetCutoffCv = cutoffCv = 0.0f;
    targetResonanceCv = resonanceCv = 0.0f;
    targetPanCv = panCv = 0.5f;
    targetVcaCv = vcaCv = 0.0f;
    resonanceInputGain = 1.0f;
    leftPanGain = rightPanGain = 0.70710678f;
    lastCutoffInput = lastResonanceInput = lastDriveInput = lastAgeInput = -1.0f;
    lastPanInput = -2.0f;
    lastVcaInput = -1.0f;
    coefficientCutoffCv = coefficientResonanceCv = coefficientPanCv = -1.0f;
    controlsInitialised = false;
}

void Cem3387::setControls(float cutoffHz, float resonance, float driveDb, float pan,
                          float circuitAge) noexcept
{
    const auto limitedAge = juce::jlimit(0.0f, 1.0f, circuitAge);
    if (limitedAge != lastAgeInput)
    {
        age = lastAgeInput = limitedAge;
        constexpr auto settlingTimeSeconds = 0.00028f;
        const auto hostSampleRate = static_cast<float>(sampleRate * 0.5);
        const auto timeConstant = settlingTimeSeconds * (1.0f + age * 0.15f);
        controlSlew = 1.0f - std::exp(-1.0f / (hostSampleRate * timeConstant));
        constexpr auto cutoffRangeOctaves = 127.0f / 12.0f;
        const auto toleranceFactor = 1.0f + tolerance * (0.008f + age * 0.035f);
        uncalibratedCutoffOffset
            = std::log2(juce::jmax(0.5f, toleranceFactor)) / cutoffRangeOctaves;
        if (cutoffCalibrationManuallySet)
            applyCutoffCalibrationCode(cutoffTrimCode, true);
        else
            calibrateCutoff();
        coefficientCutoffCv = coefficientResonanceCv = -1.0f;
    }

    constexpr auto cutoffRangeOctaves = 127.0f / 12.0f;
    if (cutoffHz != lastCutoffInput)
    {
        lastCutoffInput = cutoffHz;
        const auto cutoffNormalised
            = std::log2(juce::jlimit(20.0f, 32000.0f, cutoffHz) / 20.0f)
              / cutoffRangeOctaves;
        targetCutoffCv = quantiseCv(cutoffNormalised);
    }
    if (resonance != lastResonanceInput)
    {
        lastResonanceInput = resonance;
        targetResonanceCv = quantiseCv(juce::jlimit(0.0f, 1.0f, resonance));
    }
    if (driveDb != lastDriveInput)
    {
        lastDriveInput = driveDb;
        inputDrive = std::pow(10.0f, juce::jlimit(0.0f, 18.0f, driveDb) / 20.0f);
    }
    if (pan != lastPanInput)
    {
        lastPanInput = pan;
        targetPanCv = quantiseCv((juce::jlimit(-1.0f, 1.0f, pan) + 1.0f) * 0.5f);
    }

    // Static patch controls have already settled before a note sounds. Only
    // subsequent CV writes should exhibit acquisition transients.
    if (!controlsInitialised)
    {
        cutoffCv = targetCutoffCv;
        resonanceCv = targetResonanceCv;
        panCv = targetPanCv;
        controlsInitialised = true;
    }
}

void Cem3387::calibrateCutoff() noexcept
{
    // OS 1.700's VCF service page stores one 12-bit word per voice at
    // $15A680 and masks it with $0FFF. The service monitor displays that word
    // relative to $0800, confirming a bipolar trim centred at mid-scale.
    // Quantise the correction exactly as that trim DAC would: component spread
    // is cancelled, but its sub-LSB residual remains in the analogue path.
    constexpr auto centre = 2048;
    constexpr auto maximum = 4095.0f;
    const auto code = juce::jlimit(
        0, static_cast<int>(maximum),
        juce::roundToInt(static_cast<float>(centre)
                         - uncalibratedCutoffOffset * maximum));
    applyCutoffCalibrationCode(static_cast<uint16_t>(code), true);
}

void Cem3387::setCutoffCalibrationCode(uint16_t code) noexcept
{
    cutoffCalibrationManuallySet = true;
    applyCutoffCalibrationCode(static_cast<uint16_t>(code & 0x0fffu));
}

void Cem3387::applyCutoffCalibrationCode(uint16_t code, bool force) noexcept
{
    constexpr auto centre = 2048;
    constexpr auto maximum = 4095.0f;
    if (!force && cutoffTrimCode == code && cutoffCalibrationManuallySet)
        return;
    cutoffTrimCode = code;
    const auto trimCv = static_cast<float>(static_cast<int>(code) - centre) / maximum;
    cutoffCvOffset = uncalibratedCutoffOffset + trimCv;
    coefficientCutoffCv = -1.0f;
}

Cem3387::StereoSample Cem3387::process(float input, float vcaLevel) noexcept
{
    updateControlVoltages(vcaLevel);

    noiseState ^= noiseState << 13u;
    noiseState ^= noiseState >> 17u;
    noiseState ^= noiseState << 5u;
    const auto noise = static_cast<float>(static_cast<int32_t>(noiseState))
                       / 2147483648.0f;
    // The real feedback loop is never mathematically silent. Thermal/device
    // noise inside the CEM filter supplies the excitation from which maximum
    // resonance reaches sustained oscillation.
    const auto filterExcitation = noise * (0.00000012f + age * 0.0000003f);

    // Two circuit steps per host sample keep the nonlinear four-pole loop stable.
    const auto midpoint = 0.5f * (previousInput + input);
    auto filtered = runFilter(midpoint + filterExcitation);
    filtered = 0.5f * (filtered + runFilter(input + filterExcitation));
    previousInput = input;

    const auto vcaCurvature = vcaCv * vcaCv * (3.0f - 2.0f * vcaCv);
    const auto componentNoise = noise * (0.0000025f + age * 0.000008f);
    const auto vcaBleed = 0.000018f * (1.0f + age * 2.0f);
    const auto rawOutput = std::tanh(filtered * (1.0f + age * 0.08f))
                           * (vcaCurvature + vcaBleed) + componentNoise;
    const auto output = rawOutput - couplingInput
                        + couplingCoefficient * couplingOutput;
    couplingInput = rawOutput;
    couplingOutput = output;

    return { output * leftPanGain, output * rightPanGain };
}

float Cem3387::saturateVcfInput(float input) noexcept
{
    // Waldorf specifies the original input circuit as beginning to saturate
    // at about 70% of maximum oscillator mixer output (a level setting near
    // 75). A normalised tanh with modest drive is essentially unity for small
    // signals, is about 8% compressed at that boundary, and remains smooth
    // beyond it. Do not normalise its full-scale result back to one: that
    // would add makeup gain which is absent from the passive input network.
    constexpr auto inputDriveAtKnee = 0.75f;
    return std::tanh(input * inputDriveAtKnee) / inputDriveAtKnee;
}

void Cem3387::updateControlVoltages(float vcaLevel) noexcept
{
    if (vcaLevel != lastVcaInput)
    {
        lastVcaInput = vcaLevel;
        targetVcaCv = quantiseCv(juce::jlimit(0.0f, 1.0f, vcaLevel));
    }

    // WDV schematic sheets 8/9 show the AD7545 feeding PD508 multiplexers,
    // 3.3 nF hold capacitors and TL064 buffers. A light effective settling
    // time softens ideal edges while allowing more of the 53 Hz control
    // stepping through. Age slightly increases acquisition time through switch
    // and capacitor leakage/tolerance.
    const auto settle = [this](float current, float target) {
        return current + (target - current) * controlSlew;
    };

    cutoffCv = settle(cutoffCv, targetCutoffCv);
    resonanceCv = settle(resonanceCv, targetResonanceCv);
    panCv = settle(panCv, targetPanCv);
    vcaCv = settle(vcaCv, targetVcaCv);

    if (cutoffCv != coefficientCutoffCv)
    {
        coefficientCutoffCv = cutoffCv;
        constexpr auto minimumCv = -0.125f;
        constexpr auto maximumCv = 1.125f;
        const auto normalised = juce::jlimit(
            0.0f, 1.0f,
            (cutoffCv + cutoffCvOffset - minimumCv) / (maximumCv - minimumCv));
        const auto position = normalised
                              * static_cast<float>(cutoffCoefficientTable->size() - 1);
        const auto lower = static_cast<size_t>(position);
        const auto upper = juce::jmin(cutoffCoefficientTable->size() - 1, lower + 1);
        const auto fraction = position - static_cast<float>(lower);
        coefficient = (*cutoffCoefficientTable)[lower]
                      + ((*cutoffCoefficientTable)[upper]
                         - (*cutoffCoefficientTable)[lower]) * fraction;
    }
    if (resonanceCv != coefficientResonanceCv)
    {
        coefficientResonanceCv = resonanceCv;
        // Cutoff is service-calibrated per voice. There is no corresponding
        // measured Wave resonance-spread table, so do not add an unrelated
        // synthetic voice signature after calibration.
        resonanceAmount = juce::jlimit(0.0f, 1.04f, resonanceCv);

        // Hardware sine measurements at an open cutoff show passband losses
        // of about 3.8, 5.2, 5.9 and 6.4 dB at resonance 30, 62, 100 and 127.
        // A bare four-pole feedback loop loses over 14 dB at maximum. Model
        // the CEM/input network's makeup gain without changing the autonomous
        // feedback loop, so self-oscillation threshold and level remain genuine.
        constexpr auto feedbackGain = 4.15f;
        constexpr auto maximumPassbandLossDb = 6.4f;
        constexpr auto lossCurve = 3.5f;
        const auto lossPosition
            = (1.0f - std::exp(-lossCurve * resonanceCv))
              / (1.0f - std::exp(-lossCurve));
        const auto measuredPassbandGain = std::pow(
            10.0f, -maximumPassbandLossDb * lossPosition / 20.0f);
        const auto selfOscillationPosition = juce::jmax(
            0.0f, (resonanceCv - 0.5f) * 2.0f);
        const auto drivenLoopMakeup
            = 1.0f + 0.64f * std::pow(selfOscillationPosition, 0.6f);
        resonanceInputGain
            = (1.0f + feedbackGain * resonanceAmount)
              * measuredPassbandGain * drivenLoopMakeup;
    }
    if (panCv != coefficientPanCv)
    {
        coefficientPanCv = panCv;
        panPosition = panCv * 2.0f - 1.0f;
        const auto angle = (panPosition + 1.0f)
                           * juce::MathConstants<float>::pi * 0.25f;
        leftPanGain = std::cos(angle);
        rightPanGain = std::sin(angle);
    }
}

float Cem3387::runFilter(float input) noexcept
{
    const auto saturatedInput = saturateVcfInput(input);
    auto stageInput = std::tanh((saturatedInput * resonanceInputGain
                                 - integrators[3] * resonanceAmount * 4.15f)
                                * inputDrive);

    for (auto& state : integrators)
    {
        const auto delta = (stageInput - state) * coefficient;
        const auto output = state + delta;
        state = output + delta;
        stageInput = std::tanh(output);
    }

    return integrators[3];
}

float Cem3387::quantiseCv(float normalised) noexcept
{
    constexpr auto maximum = 4095.0f; // AD7545 is a 12-bit multiplying DAC.
    return std::round(juce::jlimit(0.0f, 1.0f, normalised) * maximum) / maximum;
}
} // namespace wave::dsp
