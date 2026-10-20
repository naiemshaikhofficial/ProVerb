#include <juce_audio_processors/juce_audio_processors.h>
#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "Licensing/LicenseClient.h"
#include <juce_audio_formats/juce_audio_formats.h>

ProVerbAudioProcessor::ProVerbAudioProcessor()
#ifndef JucePlugin_PreferredChannelConfigurations
    : AudioProcessor (BusesProperties()
                      #if ! JucePlugin_IsMidiEffect
                       #if ! JucePlugin_IsSynth
                        .withInput  ("Input",     juce::AudioChannelSet::stereo(), true)
                       #endif
                        .withInput  ("Sidechain", juce::AudioChannelSet::stereo(), false)
                        .withOutput ("Output",    juce::AudioChannelSet::stereo(), true)
                      #endif
                      ),
      apvts (*this, nullptr, "ProVerbState", createProVerbParameterLayout())
#endif
{
    aiWorker.onRecommendationReady = [this](const ReverbDecisionEngine::Recommendation& rec)
    {
        pendingRecommendation = rec;
        triggerAsyncUpdate();
    };

    aiWorker.autoDiscoverModel();
    aiWorker.startWorker();

    // Initialize licensing engine once on UI/host instantiation thread (never on audio thread)
    ProducerToy::Licensing::LicenseClient::getInstance().initialize();
    updateLicenseStatus();
}

void ProVerbAudioProcessor::updateLicenseStatus() noexcept
{
    isLicensedAtomic.store (ProducerToy::Licensing::LicenseClient::getInstance().isLicenseValid(),
                            std::memory_order_relaxed);
}

ProVerbAudioProcessor::~ProVerbAudioProcessor()
{
    sourceDetector.stop();
    aiWorker.stopWorker();
}

const juce::String ProVerbAudioProcessor::getName() const
{
    return JucePlugin_Name;
}

bool ProVerbAudioProcessor::acceptsMidi() const { return false; }
bool ProVerbAudioProcessor::producesMidi() const { return false; }
bool ProVerbAudioProcessor::isMidiEffect() const { return false; }
double ProVerbAudioProcessor::getTailLengthSeconds() const
{
    if (auto* frz = apvts.getRawParameterValue ("freeze"); frz && frz->load() > 0.5f)
        return std::numeric_limits<double>::infinity();

    double predelaySec = 0.0;
    if (auto* pd = apvts.getRawParameterValue ("predelayMs"))
        predelaySec = static_cast<double> (pd->load()) / 1000.0;

    double decaySec = 2.8;
    if (auto* param = apvts.getRawParameterValue ("decaySeconds"))
        decaySec = static_cast<double> (param->load());

    // Conservative tail length: predelay + decay + 0.35s ducking release and tail margin
    return predelaySec + decaySec + 0.35;
}

const std::vector<ProVerbAudioProcessor::Preset>& ProVerbAudioProcessor::getPresetList()
{
    static const std::vector<Preset> presets = []()
    {
        std::vector<Preset> list;
        const auto& factory = ProVerbPresets::getFactoryPresets();
        list.reserve (factory.size());

        for (const auto& fp : factory)
        {
            Preset p;
            p.category     = fp.category;
            p.name         = fp.name;
            p.predelayMs   = fp.predelay;
            p.size         = fp.size;
            p.decaySeconds = fp.decay;
            p.diffusion    = fp.diffusion;
            p.damping      = fp.damping;
            p.lowCut       = fp.lowcut;
            p.highCut      = fp.highcut;
            p.ducking      = fp.ducking;
            p.shimmer      = fp.shimmer;
            p.width        = fp.width;
            p.wet          = fp.wet;
            p.freeze       = fp.freeze ? 1.0f : 0.0f;
            p.dry          = fp.dry;
            p.inputGain    = 0.0f;
            p.outputGain   = 0.0f;
            p.midSide      = 0.0f;
            p.aliases      = fp.aliases;
            p.description  = fp.description;
            list.push_back (p);
        }
        return list;
    }();
    return presets;
}

void ProVerbAudioProcessor::loadPreset (int index)
{
    const auto& list = getPresetList();
    if (index < 0 || index >= static_cast<int> (list.size()))
        return;

    currentProgram = index;
    const auto& p = list[(size_t) index];

    auto setParam = [this](const char* id, float realVal)
    {
        if (auto* param = dynamic_cast<juce::RangedAudioParameter*> (apvts.getParameter (id)))
        {
            param->beginChangeGesture();
            param->setValueNotifyingHost (param->convertTo0to1 (realVal));
            param->endChangeGesture();
        }
    };

    setParam ("predelayMs",   p.predelayMs);
    setParam ("size",         p.size);
    setParam ("decaySeconds", p.decaySeconds);
    setParam ("diffusion",    p.diffusion);
    setParam ("damping",      p.damping);
    setParam ("lowCut",       p.lowCut);
    setParam ("highCut",      p.highCut);
    setParam ("ducking",      p.ducking);
    setParam ("shimmer",      p.shimmer);
    setParam ("width",        p.width);
    setParam ("wet",          p.wet);
    setParam ("freeze",       p.freeze);
    setParam ("dry",          p.dry);
    setParam ("inputGain",    p.inputGain);
    setParam ("outputGain",   p.outputGain);
    setParam ("midSide",      p.midSide);

    updateHostDisplay (juce::AudioProcessor::ChangeDetails().withProgramChanged (true));

    if (onProgramChange)
        onProgramChange (currentProgram);
}

void ProVerbAudioProcessor::applyDetectedSourcePreset()
{
    const int dom = sourceDetector.getDominantFamily();
    const char* recName = SourceDetector::getRecommendedPresetName (dom);
    if (recName == nullptr)
        return;

    const auto& list = getPresetList();
    for (size_t i = 0; i < list.size(); ++i)
    {
        if (list[i].name == recName || list[i].name.equalsIgnoreCase (recName) || list[i].name.containsIgnoreCase (recName))
        {
            loadPreset (static_cast<int> (i));
            currentProgram = static_cast<int> (i);
            updateHostDisplay();
            if (onProgramChange)
                onProgramChange (currentProgram);
            break;
        }
    }
}

int ProVerbAudioProcessor::getNumPrograms()
{
    return static_cast<int> (getPresetList().size());
}

int ProVerbAudioProcessor::getCurrentProgram()
{
    return currentProgram;
}

void ProVerbAudioProcessor::setCurrentProgram (int index)
{
    loadPreset (index);
}

const juce::String ProVerbAudioProcessor::getProgramName (int index)
{
    const auto& list = getPresetList();
    if (index >= 0 && index < static_cast<int> (list.size()))
        return list[(size_t) index].category + ": " + list[(size_t) index].name;
    return {};
}

void ProVerbAudioProcessor::changeProgramName (int, const juce::String&)
{
}

void ProVerbAudioProcessor::triggerAutoLearn()
{
    // Snapshot current parameters so user can instant Undo/Revert
    stateBeforeAutoLearn.clear();
    for (auto* param : getParameters())
    {
        if (auto* p = dynamic_cast<juce::RangedAudioParameter*> (param))
            stateBeforeAutoLearn[p->paramID] = p->getValue();
    }
    programBeforeAutoLearn = currentProgram;
    hasUndoState = true;

    isAnalyzingActive.store (true, std::memory_order_release);
    hasAnalyzedResult = false;
    hasPendingSuggestion = false;
    detectedInstrument.clear();
    detectedPresetName.clear();
    sourceDetector.setEnabled (true);
    analyzerEngine.startLearning (2.5f);
}

void ProVerbAudioProcessor::abortAutoLearn()
{
    isAnalyzingActive.store (false, std::memory_order_release);
    analyzerEngine.abortLearning();
}

void ProVerbAudioProcessor::undoAutoLearn()
{
    if (! hasUndoState)
        return;

    for (const auto& pair : stateBeforeAutoLearn)
    {
        if (auto* p = apvts.getParameter (pair.first))
        {
            p->beginChangeGesture();
            p->setValueNotifyingHost (pair.second);
            p->endChangeGesture();
        }
    }

    currentProgram = programBeforeAutoLearn;
    if (onProgramChange)
        onProgramChange (currentProgram);

    hasUndoState = false;
    hasAnalyzedResult = false;
    hasPendingSuggestion = false;
    updateHostDisplay();
}

void ProVerbAudioProcessor::analyzeAudioFileAsync (const juce::File& audioFile)
{
    if (! audioFile.existsAsFile())
        return;

    // Snapshot parameters so user can Undo/Revert
    stateBeforeAutoLearn.clear();
    for (auto* param : getParameters())
    {
        if (auto* p = dynamic_cast<juce::RangedAudioParameter*> (param))
            stateBeforeAutoLearn[p->paramID] = p->getValue();
    }
    programBeforeAutoLearn = currentProgram;
    hasUndoState = true;
    isAnalyzingActive.store (true, std::memory_order_release);

    juce::Thread::launch ([this, audioFile]()
    {
        juce::AudioFormatManager formatManager;
        formatManager.registerBasicFormats();

        std::unique_ptr<juce::AudioFormatReader> reader (formatManager.createReaderFor (audioFile));
        if (reader == nullptr)
        {
            isAnalyzingActive.store (false, std::memory_order_release);
            return;
        }

        const int maxSamples = static_cast<int> (std::min<int64_t> (reader->lengthInSamples, static_cast<int64_t> (reader->sampleRate * 6.0)));
        if (maxSamples <= 0)
        {
            isAnalyzingActive.store (false, std::memory_order_release);
            return;
        }

        juce::AudioBuffer<float> tempBuffer (static_cast<int> (reader->numChannels), maxSamples);
        reader->read (&tempBuffer, 0, maxSamples, 0, true, true);

        AudioAnalyzerEngine engine;
        engine.prepare (reader->sampleRate);
        const float durationSec = static_cast<float> (maxSamples) / static_cast<float> (reader->sampleRate);
        engine.startLearning (durationSec);

        const float* left = tempBuffer.getReadPointer (0);
        const float* right = tempBuffer.getNumChannels() > 1 ? tempBuffer.getReadPointer (1) : left;

        ProVerbAI::FeatureSnapshot snapshot;
        const int blockSize = 512;
        for (int offset = 0; offset < maxSamples; offset += blockSize)
        {
            const int curChunk = std::min (maxSamples - offset, blockSize);
            engine.process (left + offset, right + offset, curChunk, snapshot);
        }

        engine.extractFeatureSnapshot (snapshot);
        aiWorker.postAnalysisJob (snapshot);
    });
}

void ProVerbAudioProcessor::handleAsyncUpdate()
{
    isAnalyzingActive.store (false, std::memory_order_release);

    auto rec = aiWorker.hasNewResult() ? aiWorker.popLatestResult() : pendingRecommendation;

    const int detectedFamily = sourceDetector.getDominantFamily();
    const float detectorConf = sourceDetector.getDominantConfidence();

    // 1. Check for genuine silence or uncertain/low-confidence detection
    if (rec.profile.id == AudioAnalyzerEngine::ProfileId::NoAudio
        || (detectedFamily == SourceDetector::FAMILY_UNKNOWN && detectorConf < 0.22f && ! rec.isAIModel))
    {
        autoLearnProfileName = AudioAnalyzerEngine::getProfileName (rec.profile.id);
        detectedInstrument = (rec.profile.id == AudioAnalyzerEngine::ProfileId::NoAudio) ? "NO AUDIO" : "Uncertain / Ambience";
        detectedPresetName = "Kept Current Mix (No Change)";
        hasAnalyzedResult = false;
        hasPendingSuggestion = false;
        if (onAutoLearnResult)
            onAutoLearnResult (detectedInstrument, detectedPresetName, false);
        return;
    }

    juce::String chosenInstrument;
    juce::String chosenPreset;

    // 2. PRIMARY: High-Confidence ONNX Deep Learning Neural Network
    if (rec.isAIModel && aiWorker.isModelLoaded() && rec.confidence >= 0.25f
        && rec.instrumentCategory.isNotEmpty() && ! rec.instrumentCategory.containsIgnoreCase ("Uncertain"))
    {
        chosenInstrument = rec.instrumentCategory;
        chosenPreset = rec.profileName;

        // Sub-family & multi-label corroboration from real-time source detector
        if (detectedFamily != SourceDetector::FAMILY_UNKNOWN && detectorConf >= 0.35f)
        {
            auto detName = juce::String (SourceDetector::getFamilyName (detectedFamily));
            if (detName.containsIgnoreCase ("Acoustic") && chosenInstrument.containsIgnoreCase ("Guitar"))
                chosenInstrument = "ACOUSTIC GUITAR (ONNX)";
            else if (detName.containsIgnoreCase ("Electric") && chosenInstrument.containsIgnoreCase ("Guitar"))
                chosenInstrument = "ELECTRIC GUITAR (ONNX)";
        }
    }
    // 3. SECONDARY: Multi-Label Source Family Detector
    else if (detectedFamily != SourceDetector::FAMILY_UNKNOWN && detectorConf >= 0.22f)
    {
        if (detectedFamily == SourceDetector::FAMILY_MIXED)
        {
            const auto summary = sourceDetector.getActiveFamiliesSummary();
            chosenInstrument = summary.isNotEmpty() ? summary : "Mixed / Ensemble";
            chosenPreset = "Small Acoustic Ensemble - Intimate Glue";
        }
        else
        {
            chosenInstrument = SourceDetector::getFamilyName (detectedFamily);
            const char* recommended = SourceDetector::getRecommendedPresetName (detectedFamily);
            if (recommended != nullptr)
                chosenPreset = recommended;
        }
    }
    // 4. TERTIARY: Heuristic Engine Fallback
    else if (rec.profile.id != AudioAnalyzerEngine::ProfileId::None && rec.instrumentCategory.isNotEmpty())
    {
        chosenInstrument = rec.instrumentCategory;
        chosenPreset = rec.profileName;
    }

    int matchedPresetIndex = -1;
    const auto& list = getPresetList();

    auto findPresetIndexByName = [&](const juce::String& targetName) -> int
    {
        for (size_t i = 0; i < list.size(); ++i)
        {
            if (list[i].name == targetName || list[i].name.equalsIgnoreCase (targetName))
                return static_cast<int> (i);
        }
        for (size_t i = 0; i < list.size(); ++i)
        {
            if (list[i].name.containsIgnoreCase (targetName))
                return static_cast<int> (i);
        }
        return -1;
    };

    // 4. Smart Mapping to Known Curated Factory Presets
    if (chosenPreset.isNotEmpty())
    {
        matchedPresetIndex = findPresetIndexByName (chosenPreset);
        if (matchedPresetIndex >= 0)
            chosenPreset = list[(size_t) matchedPresetIndex].name;
    }

    if (matchedPresetIndex < 0 && chosenInstrument.isNotEmpty())
    {
        if (chosenInstrument.containsIgnoreCase ("Vocal") || chosenInstrument.containsIgnoreCase ("Voice"))
        {
            if (rec.profile.id == AudioAnalyzerEngine::ProfileId::VocalsRap
                || rec.profile.ducking > 38.0f || rec.profile.decaySeconds < 1.35f)
            {
                chosenInstrument = "RAP / HIP-HOP (AI)";
                chosenPreset = "Hip-Hop & Trap Vocal (Tight & Punchy)";
            }
            else if (rec.profile.id == AudioAnalyzerEngine::ProfileId::VocalWarmStudioHall
                     || rec.profile.decaySeconds > 2.0f || rec.profile.shimmer > 3.0f)
            {
                chosenInstrument = "AIRY VOCAL HALL (AI)";
                chosenPreset = "Warm Studio Vocal Hall";
            }
            else
            {
                chosenInstrument = "POP VOCALS (AI)";
                chosenPreset = "Modern Billboard Pop Vocal";
            }
        }
        else if (chosenInstrument.containsIgnoreCase ("Drum") || chosenInstrument.containsIgnoreCase ("Perc"))
        {
            if (rec.profile.id == AudioAnalyzerEngine::ProfileId::PercussionTablaHand
                || rec.profileName.containsIgnoreCase ("Tabla"))
            {
                chosenInstrument = "PERCUSSION (AI)";
                chosenPreset = "Tabla & Dholak - Intimate Acoustic";
            }
            else if (rec.profile.id == AudioAnalyzerEngine::ProfileId::DrumsKickLowClean
                     || rec.profile.decaySeconds < 0.75f)
            {
                chosenInstrument = "KICK / 808 (AI)";
                chosenPreset = "Kick Drum - Low-End Room Clean";
            }
            else
            {
                chosenInstrument = "DRUMS (AI)";
                chosenPreset = "Snare Drum - Tight Studio Room";
            }
        }
        else if (chosenInstrument.containsIgnoreCase ("Bass"))
        {
            chosenInstrument = "BASS (AI)";
            chosenPreset = "Bass Dry / Keep the Low End Tight";
        }
        else if (chosenInstrument.containsIgnoreCase ("Piano") || chosenInstrument.containsIgnoreCase ("Keys"))
        {
            chosenInstrument = "KEYS / PIANO (AI)";
            chosenPreset = "Grand Piano - Concert Hall";
        }
        else if (chosenInstrument.containsIgnoreCase ("Guitar") || chosenInstrument.containsIgnoreCase ("Pluck"))
        {
            if (rec.profile.id == AudioAnalyzerEngine::ProfileId::StringsPluckedSitar
                || rec.profileName.containsIgnoreCase ("Sitar"))
            {
                chosenInstrument = "SITAR / PLUCK (AI)";
                chosenPreset = "Sitar & Classical Plucked Strings";
            }
            else if (rec.profile.decaySeconds > 1.6f)
            {
                chosenInstrument = "ELECTRIC GUITAR (AI)";
                chosenPreset = "Clean Electric Guitar - Studio Plate-Style";
            }
            else
            {
                chosenInstrument = "ACOUSTIC GUITAR (AI)";
                chosenPreset = "Steel-String Acoustic Guitar";
            }
        }
        else if (chosenInstrument.containsIgnoreCase ("String") || chosenInstrument.containsIgnoreCase ("Bowed"))
        {
            chosenInstrument = "STRINGS (AI)";
            chosenPreset = "String Section - Lush Scoring Stage";
        }
        else if (chosenInstrument.containsIgnoreCase ("Wind") || chosenInstrument.containsIgnoreCase ("Brass") || chosenInstrument.containsIgnoreCase ("Flute"))
        {
            chosenInstrument = "WINDS / FLUTE (AI)";
            chosenPreset = "Flute & Bansuri - Breath of Sky";
        }
        else if (chosenInstrument.containsIgnoreCase ("Synth") || chosenInstrument.containsIgnoreCase ("Electronic") || chosenInstrument.containsIgnoreCase ("Pad"))
        {
            chosenInstrument = "SYNTH / PAD (AI)";
            chosenPreset = "Synth Lead & Pluck - Polished Modern";
        }
        else if (chosenInstrument.containsIgnoreCase ("Mixed") || chosenInstrument.containsIgnoreCase ("Ensemble"))
        {
            chosenInstrument = "MIXED ENSEMBLE (AI)";
            chosenPreset = "Small Acoustic Ensemble - Intimate Glue";
        }

        matchedPresetIndex = findPresetIndexByName (chosenPreset);
    }

    auto setParam = [this](const juce::String& paramId, float value)
    {
        if (auto* p = apvts.getParameter (paramId))
        {
            p->beginChangeGesture();
            p->setValueNotifyingHost (p->getNormalisableRange().convertTo0to1 (value));
            p->endChangeGesture();
        }
    };

    if (rec.profile.id != AudioAnalyzerEngine::ProfileId::None
        && rec.profile.id != AudioAnalyzerEngine::ProfileId::NoAudio)
    {
        // Direct dynamic application of AI-calculated waveform parameters:
        setParam ("predelayMs",    rec.profile.predelayMs);
        setParam ("size",          rec.profile.size);
        setParam ("decaySeconds",  rec.profile.decaySeconds);
        setParam ("diffusion",     rec.profile.diffusion);
        setParam ("damping",       rec.profile.damping);
        setParam ("lowCut",        rec.profile.lowCut);
        setParam ("highCut",       rec.profile.highCut);
        setParam ("ducking",       rec.profile.ducking);
        setParam ("shimmer",       rec.profile.shimmer);
        setParam ("width",         rec.profile.width);
        setParam ("wet",           rec.profile.wet);

        hasAnalyzedResult = true;
        hasPendingSuggestion = true;
        suggestedProgram = matchedPresetIndex;
        suggestedConfidence = detectorConf;
    }
    else
    {
        // Low confidence / uncertain - safely preserve user's current settings
        detectedInstrument = "Uncertain / Mixed";
        detectedPresetName = "Kept Current Mix (No Change)";
        autoLearnProfileName = detectedPresetName;
        hasAnalyzedResult = false;
        hasPendingSuggestion = false;
        if (onAutoLearnResult)
            onAutoLearnResult (detectedInstrument, detectedPresetName, false);
        return;
    }

    lastProfileId = rec.profile.id;
    detectedInstrument = chosenInstrument.isNotEmpty() ? chosenInstrument : rec.instrumentCategory;
    detectedPresetName = chosenPreset.isNotEmpty() ? chosenPreset : rec.profileName;
    autoLearnProfileName = detectedPresetName;

    updateHostDisplay();

    if (onAutoLearnResult)
        onAutoLearnResult (detectedInstrument, detectedPresetName, true);
}

void ProVerbAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    // ProVerb's internal DSP processing pipeline (preDelayLine, toneEQ, fdnReverb, ducking, shimmer)
    // is strictly 2-channel stereo. Mono host inputs are duplicated at entry and folded down at exit.
    juce::dsp::ProcessSpec internalStereoSpec;
    internalStereoSpec.sampleRate = sampleRate;
    internalStereoSpec.maximumBlockSize = static_cast<juce::uint32> (samplesPerBlock);
    internalStereoSpec.numChannels = 2;

    fdnReverb.prepare (sampleRate, samplesPerBlock, apvts.getRawParameterValue ("size")->load());
    duckingEngine.prepare (sampleRate);
    shimmerEngine.prepare (sampleRate);
    toneEQ.prepare (internalStereoSpec);
    analyzerEngine.prepare (sampleRate);
    sourceDetector.prepare (sampleRate);

    preDelayLine.prepare (internalStereoSpec);
    preDelayLine.setMaximumDelayInSamples (static_cast<int> (sampleRate * 0.5)); // 500ms max

    const float initPreDelay = apvts.getRawParameterValue ("predelayMs")->load();
    const float initInG      = apvts.getRawParameterValue ("inputGain")->load();
    const float initOutG     = apvts.getRawParameterValue ("outputGain")->load();
    const float initWet      = apvts.getRawParameterValue ("wet")->load();
    const float initDry      = apvts.getRawParameterValue ("dry")->load();

    smoothedPreDelay.reset (sampleRate, 0.035);
    smoothedPreDelay.setCurrentAndTargetValue (static_cast<float> (sampleRate * (initPreDelay / 1000.0f)));

    smoothedInputGain.reset (sampleRate, 0.04);
    smoothedInputGain.setCurrentAndTargetValue (juce::Decibels::decibelsToGain (initInG));

    smoothedOutputGain.reset (sampleRate, 0.04);
    smoothedOutputGain.setCurrentAndTargetValue (juce::Decibels::decibelsToGain (initOutG));

    smoothedWetGain.reset (sampleRate, 0.04);
    smoothedWetGain.setCurrentAndTargetValue (initWet / 100.0f);

    smoothedDryGain.reset (sampleRate, 0.04);
    smoothedDryGain.setCurrentAndTargetValue (initDry / 100.0f);

    smoothedBypassGain.reset (sampleRate, 0.025);
    smoothedBypassGain.setCurrentAndTargetValue (apvts.getRawParameterValue ("bypass")->load() > 0.5f ? 0.0f : 1.0f);

    const float initWidth = apvts.getRawParameterValue ("width")->load();
    smoothedWidth.reset (sampleRate, 0.035);
    smoothedWidth.setCurrentAndTargetValue (initWidth / 100.0f);

    // Initialize Next-Level Smart AI Automation Smoothers (0.035s de-zippering)
    smoothedSmartSpace.reset (sampleRate, 0.035);
    smoothedSmartSpace.setCurrentAndTargetValue (apvts.getRawParameterValue ("smartSpace") != nullptr ? apvts.getRawParameterValue ("smartSpace")->load() : 50.0f);

    smoothedSmartClarity.reset (sampleRate, 0.035);
    smoothedSmartClarity.setCurrentAndTargetValue (apvts.getRawParameterValue ("smartClarity") != nullptr ? apvts.getRawParameterValue ("smartClarity")->load() : 50.0f);

    smoothedSmartPumping.reset (sampleRate, 0.035);
    smoothedSmartPumping.setCurrentAndTargetValue (apvts.getRawParameterValue ("smartPumping") != nullptr ? apvts.getRawParameterValue ("smartPumping")->load() : 50.0f);

    // Refresh audio-safe license state on prepare
    updateLicenseStatus();

    // Generous pre-allocation to guarantee zero memory reallocations on the audio thread
    const int bufferCapacity = std::max (samplesPerBlock * 2, 8192);
    dryBuffer.setSize (2, bufferCapacity, false, true, false);
    wetBuffer.setSize (2, bufferCapacity, false, true, false);
    immediateBuffer.setSize (2, bufferCapacity, false, true, false);
}

void ProVerbAudioProcessor::releaseResources()
{
    fdnReverb.reset();
    duckingEngine.reset();
    shimmerEngine.reset();
    toneEQ.reset();
    analyzerEngine.reset();
    sourceDetector.reset();
    preDelayLine.reset();
}

bool ProVerbAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto& mainIn  = layouts.getMainInputChannelSet();
    const auto& mainOut = layouts.getMainOutputChannelSet();

    if (mainOut != juce::AudioChannelSet::mono()
     && mainOut != juce::AudioChannelSet::stereo())
        return false;

    // Supported routing configurations for main bus:
    // 1. Mono In -> Mono Out
    // 2. Mono In -> Stereo Out (Essential for mono vocal/instrument tracks in DAWs)
    // 3. Stereo In -> Stereo Out
    bool mainSupported = false;
    if (mainIn == juce::AudioChannelSet::mono())
        mainSupported = true;
    else if (mainIn == juce::AudioChannelSet::stereo() && mainOut == juce::AudioChannelSet::stereo())
        mainSupported = true;

    if (! mainSupported)
        return false;

    // Auxiliary sidechain bus (bus index 1):
    // Can be disabled (unconnected), mono, or stereo
    if (layouts.inputBuses.size() > 1)
    {
        const auto& sidechainIn = layouts.getChannelSet (true, 1);
        if (! sidechainIn.isDisabled()
            && sidechainIn != juce::AudioChannelSet::mono()
            && sidechainIn != juce::AudioChannelSet::stereo())
            return false;
    }

    return true;
}

void ProVerbAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;

    auto mainInput  = getBusBuffer (buffer, true, 0);
    auto mainOutput = getBusBuffer (buffer, false, 0);
    const int mainInChannels  = mainInput.getNumChannels();
    const int mainOutChannels = mainOutput.getNumChannels();
    const int numSamples      = buffer.getNumSamples();

    if (numSamples == 0 || mainInChannels == 0 || mainOutChannels == 0)
        return;

    // Clear unused output channels
    for (int i = mainInChannels; i < mainOutChannels; ++i)
        mainOutput.clear (i, 0, numSamples);

    // Multi-bus External Sidechain Input (Bus index 1)
    const bool hasSidechainBus = (getBusCount (true) > 1 && getBus (true, 1) != nullptr && getBus (true, 1)->isEnabled());
    auto sidechainInput = hasSidechainBus ? getBusBuffer (buffer, true, 1) : juce::AudioBuffer<float>();
    isSidechainBusConnected.store (hasSidechainBus && sidechainInput.getNumChannels() > 0, std::memory_order_relaxed);

    // Measure sidechain RMS and active signal presence (> -66 dB)
    float curRmsSc = 0.0f;
    if (sidechainInput.getNumChannels() > 0)
    {
        for (int ch = 0; ch < sidechainInput.getNumChannels(); ++ch)
            curRmsSc = juce::jmax (curRmsSc, sidechainInput.getRMSLevel (ch, 0, numSamples));
    }
    rmsSidechain.store (curRmsSc, std::memory_order_relaxed);
    const bool scSignalPresent = (curRmsSc > 0.0005f);
    isSidechainSignalActive.store (scSignalPresent, std::memory_order_relaxed);

    // Multi-label real-time Source Detector (lock-free SPSC queue, listens strictly to primary track)
    sourceDetector.pushAudio (mainInput, mainInChannels);

    // 1. Measure input RMS (thread-safe)
    float curRmsIn = 0.0f;
    for (int ch = 0; ch < mainInChannels; ++ch)
        curRmsIn = juce::jmax (curRmsIn, mainInput.getRMSLevel (ch, 0, numSamples));
    rmsInput.store (curRmsIn, std::memory_order_relaxed);

    // 2. Read base parameter values
    const float inGainDb   = apvts.getRawParameterValue ("inputGain")->load();
    const float outGainDb  = apvts.getRawParameterValue ("outputGain")->load();
    const float preDelayMs = apvts.getRawParameterValue ("predelayMs")->load();
    const float decaySec   = apvts.getRawParameterValue ("decaySeconds")->load();
    const float sizePct    = apvts.getRawParameterValue ("size")->load();
    const float diffPct    = apvts.getRawParameterValue ("diffusion")->load();
    const float dampPct    = apvts.getRawParameterValue ("damping")->load();
    const float widthPct   = apvts.getRawParameterValue ("width")->load();
    const float duckPct    = apvts.getRawParameterValue ("ducking")->load();
    const float shimPct    = apvts.getRawParameterValue ("shimmer")->load();
    const float lowCutHz   = apvts.getRawParameterValue ("lowCut")->load();
    const float highCutHz  = apvts.getRawParameterValue ("highCut")->load();
    const float wetPct     = apvts.getRawParameterValue ("wet")->load();
    const float dryPct     = apvts.getRawParameterValue ("dry")->load();

    // Read Next-Level Smart AI Automation & Sidechain parameters
    const float smartSpaceVal   = apvts.getRawParameterValue ("smartSpace") != nullptr ? apvts.getRawParameterValue ("smartSpace")->load() : 50.0f;
    const float smartClarityVal = apvts.getRawParameterValue ("smartClarity") != nullptr ? apvts.getRawParameterValue ("smartClarity")->load() : 50.0f;
    const float smartPumpVal    = apvts.getRawParameterValue ("smartPumping") != nullptr ? apvts.getRawParameterValue ("smartPumping")->load() : 50.0f;

    const int scSourceMode      = apvts.getRawParameterValue ("duckingSource") != nullptr ? static_cast<int> (apvts.getRawParameterValue ("duckingSource")->load()) : 0;
    const float duckThreshDb    = apvts.getRawParameterValue ("duckingThreshold") != nullptr ? apvts.getRawParameterValue ("duckingThreshold")->load() : -32.0f;
    const float duckAttackMs    = apvts.getRawParameterValue ("duckingAttack") != nullptr ? apvts.getRawParameterValue ("duckingAttack")->load() : 8.0f;
    const float duckReleaseMs   = apvts.getRawParameterValue ("duckingRelease") != nullptr ? apvts.getRawParameterValue ("duckingRelease")->load() : 160.0f;

    // Zero-allocation, zero-lock real-time audio thread license read
    const bool isUnlocked  = isLicensedAtomic.load (std::memory_order_relaxed);
    const bool isFrozen    = isUnlocked && (apvts.getRawParameterValue ("freeze")->load() > 0.5f);
    const bool isBypassed  = apvts.getRawParameterValue ("bypass")->load() > 0.5f;
    const bool isMidSide   = apvts.getRawParameterValue ("midSide") != nullptr
                                && apvts.getRawParameterValue ("midSide")->load() > 0.5f;

    // When locked/unregistered, wet reverb engine is completely silenced (zero audio tail)
    const float effectiveWet = isUnlocked ? (wetPct / 100.0f) : 0.0f;

    // Set smoothed targets
    smoothedInputGain.setTargetValue (juce::Decibels::decibelsToGain (inGainDb));
    smoothedOutputGain.setTargetValue (juce::Decibels::decibelsToGain (outGainDb));
    smoothedWetGain.setTargetValue (effectiveWet);
    smoothedDryGain.setTargetValue (dryPct / 100.0f);
    smoothedBypassGain.setTargetValue (isBypassed ? 0.0f : 1.0f);
    smoothedWidth.setTargetValue (widthPct / 100.0f);

    smoothedSmartSpace.setTargetValue (smartSpaceVal);
    smoothedSmartClarity.setTargetValue (smartClarityVal);
    smoothedSmartPumping.setTargetValue (smartPumpVal);

    currentSmartSpace.store (smartSpaceVal, std::memory_order_relaxed);
    currentSmartClarity.store (smartClarityVal, std::memory_order_relaxed);

    // Dynamic Smart AI Macro Modulation:
    // Morph factors: -1.0 to +1.0 (0.0 = 50% neutral default)
    const float curSpaceMacro   = smoothedSmartSpace.getCurrentValue();
    const float curClarityMacro = smoothedSmartClarity.getCurrentValue();
    const float curPumpMacro    = smoothedSmartPumping.getCurrentValue();

    const float spaceMorph   = (curSpaceMacro - 50.0f) / 50.0f;
    const float clarityMorph = (curClarityMacro - 50.0f) / 50.0f;
    const float pumpMorph    = (curPumpMacro - 50.0f) / 50.0f;

    // 1. AI Space Morph: Smoothly morphs room size, decay multiplier, and pre-delay
    float decayScale = 1.0f;
    float sizeScale  = 1.0f;
    float predelayScale = 1.0f;
    float predelayOffset = 0.0f;
    if (spaceMorph < 0.0f)
    {
        decayScale    = 1.0f + spaceMorph * 0.65f;   // Tightens to 0.35x
        sizeScale     = 1.0f + spaceMorph * 0.50f;   // Tightens to 0.50x
        predelayScale = 1.0f + spaceMorph * 0.70f;   // Tightens to 0.30x
    }
    else if (spaceMorph > 0.0f)
    {
        decayScale     = 1.0f + spaceMorph * 1.50f;  // Expands up to 2.50x
        sizeScale      = 1.0f + spaceMorph * 0.40f;  // Expands up to 1.40x
        predelayOffset = spaceMorph * 25.0f;         // Spaced by +25ms for lush depth
    }

    const float effDecaySec = juce::jlimit (0.1f, 30.0f, decaySec * decayScale);
    const float effSizePct  = juce::jlimit (10.0f, 100.0f, sizePct * sizeScale);
    const float effPreDelay = juce::jlimit (0.0f, 300.0f, preDelayMs * predelayScale + predelayOffset);
    smoothedPreDelay.setTargetValue (static_cast<float> (getSampleRate() * (effPreDelay / 1000.0f)));

    // 2. AI Clarity & Anti-Mud: Progressively engages Abbey Road 240Hz HPF & 11kHz air smoothing
    float effLowCut  = lowCutHz;
    float effHighCut = highCutHz;
    float effDamp    = dampPct;
    if (clarityMorph > 0.0f)
    {
        effLowCut  = juce::jmax (lowCutHz, lowCutHz + clarityMorph * (240.0f - lowCutHz));
        effHighCut = juce::jmin (highCutHz, highCutHz - clarityMorph * (highCutHz - 10500.0f));
        effDamp    = juce::jlimit (0.0f, 100.0f, dampPct + clarityMorph * 20.0f);
    }
    else if (clarityMorph < 0.0f)
    {
        effLowCut  = juce::jmax (20.0f, lowCutHz * (1.0f + clarityMorph * 0.5f));
        effHighCut = juce::jlimit (1000.0f, 20000.0f, highCutHz * (1.0f + clarityMorph * 0.25f));
    }

    // 3. AI Rhythm Pump & Ducking Ballistics
    float effDuck    = duckPct;
    float effAttack  = duckAttackMs;
    float effRelease = duckReleaseMs;
    if (pumpMorph > 0.0f)
    {
        effDuck    = juce::jlimit (0.0f, 100.0f, duckPct + pumpMorph * 40.0f);
        effAttack  = juce::jmax (1.0f, duckAttackMs * (1.0f - pumpMorph * 0.5f));
        effRelease = juce::jmax (20.0f, duckReleaseMs * (1.0f - pumpMorph * 0.4f));
    }
    else if (pumpMorph < 0.0f)
    {
        effDuck = juce::jlimit (0.0f, 100.0f, duckPct * (1.0f + pumpMorph * 0.6f));
    }

    // Ducking Source Selection: 0 = Auto (Smart AI), 1 = Internal (Dry), 2 = External (Sidechain)
    bool useExternalSidechain = false;
    if (scSourceMode == 0) // Auto Smart AI: Uses external sidechain if routed and active, else internal dry
        useExternalSidechain = (sidechainInput.getNumChannels() > 0 && scSignalPresent);
    else if (scSourceMode == 2) // Explicit External Sidechain
        useExternalSidechain = (sidechainInput.getNumChannels() > 0);

    // 3. Process in bounded sub-blocks using pre-allocated scratch capacity (Zero Heap Allocation)
    const int maxChunk = dryBuffer.getNumSamples();

    for (int offset = 0; offset < numSamples; offset += maxChunk)
    {
        const int curChunk = std::min (numSamples - offset, maxChunk);

        if (mainInChannels == 1)
        {
            dryBuffer.copyFrom (0, 0, mainInput, 0, offset, curChunk);
            dryBuffer.copyFrom (1, 0, mainInput, 0, offset, curChunk);
        }
        else
        {
            dryBuffer.copyFrom (0, 0, mainInput, 0, offset, curChunk);
            dryBuffer.copyFrom (1, 0, mainInput, 1, offset, curChunk);
        }

        // Apply smoothed input gain
        for (int s = 0; s < curChunk; ++s)
        {
            const float inG = smoothedInputGain.getNextValue();
            dryBuffer.setSample (0, s, dryBuffer.getSample (0, s) * inG);
            dryBuffer.setSample (1, s, dryBuffer.getSample (1, s) * inG);
        }

        // 4. Real-Time Audio Analyzer Engine (Auto Learn, lock-free, zero allocation, enabled only when unlocked)
        if (isUnlocked && analyzerEngine.isListening())
        {
            const float* dryL = dryBuffer.getReadPointer (0);
            const float* dryR = dryBuffer.getReadPointer (1);
            ProVerbAI::FeatureSnapshot snapshot;
            if (analyzerEngine.process (dryL, dryR, curChunk, snapshot))
            {
                aiWorker.postAnalysisJob (snapshot);
            }
        }

        // 5. Sample-by-Sample Smoothed Pre-Delay (Late-Only Pre-Delay Architecture)
        for (int s = 0; s < curChunk; ++s)
        {
            const float curDelaySamples = smoothedPreDelay.getNextValue();
            float inL = dryBuffer.getSample (0, s);
            float inR = dryBuffer.getSample (1, s);

            if (isMidSide)
            {
                const float mid  = (inL + inR) * 0.5f;
                const float side = (inR - inL) * 0.5f;
                inL = (mid * 0.30f) - (side * 1.25f);
                inR = (mid * 0.30f) + (side * 1.25f);
            }

            immediateBuffer.setSample (0, s, inL);
            immediateBuffer.setSample (1, s, inR);

            preDelayLine.pushSample (0, inL);
            preDelayLine.pushSample (1, inR);

            wetBuffer.setSample (0, s, preDelayLine.popSample (0, curDelaySamples));
            wetBuffer.setSample (1, s, preDelayLine.popSample (1, curDelaySamples));
        }

        // 6. FDN Algorithmic Reverb Core (Late-Only Pre-Delay Architecture with Smart Space Modulation)
        const float* immL  = immediateBuffer.getReadPointer (0);
        const float* immR  = immediateBuffer.getReadPointer (1);
        const float* lateL = wetBuffer.getReadPointer (0);
        const float* lateR = wetBuffer.getReadPointer (1);
        float* wetL = wetBuffer.getWritePointer (0);
        float* wetR = wetBuffer.getWritePointer (1);

        const float erBalance = juce::jlimit (0.35f, 0.65f, 0.60f - (effSizePct / 100.0f) * 0.20f);
        fdnReverb.process (immL, immR, wetL, wetR, curChunk, effSizePct, effDecaySec, effDamp, diffPct, isFrozen,
                           erBalance, 1.0f, lateL, lateR);

        // 7. Celestial Shimmer Pitch Feedback (+1 Octave)
        shimmerEngine.process (wetL, wetR, curChunk, shimPct);

        // 8. Modern Tone EQ (Smooth TPT Filters with Anti-Mud & Clarity Modulation)
        toneEQ.process (wetBuffer, curChunk, effLowCut, effHighCut);

        // 9. Smart Vocal & Drum Reverb Ducking (External DAW Sidechain or Dry Input)
        if (useExternalSidechain)
        {
            const float* scL = sidechainInput.getReadPointer (0, offset);
            const float* scR = sidechainInput.getNumChannels() > 1 ? sidechainInput.getReadPointer (1, offset) : scL;
            duckingEngine.process (scL, scR, wetL, wetR, curChunk, effDuck, duckThreshDb, effAttack, effRelease);
        }
        else
        {
            const float* dryL = dryBuffer.getReadPointer (0);
            const float* dryR = dryBuffer.getReadPointer (1);
            duckingEngine.process (dryL, dryR, wetL, wetR, curChunk, effDuck, duckThreshDb, effAttack, effRelease);
        }

        // 10. Continuous Smooth Soft Saturation
        for (int ch = 0; ch < 2; ++ch)
        {
            float* w = wetBuffer.getWritePointer (ch);
            for (int s = 0; s < curChunk; ++s)
            {
                const float x = w[s];
                w[s] = x / (1.0f + 0.18f * x * x);
            }
        }

        // 11. Stereo Width Adjustment (Mid/Side processing with smooth sample interpolation)
        for (int s = 0; s < curChunk; ++s)
        {
            const float wScale = smoothedWidth.getNextValue();
            const float mid  = (wetL[s] + wetR[s]) * 0.5f;
            const float side = (wetR[s] - wetL[s]) * 0.5f * wScale;
            wetL[s] = mid - side;
            wetR[s] = mid + side;
        }

        // 12. Output Mix, Master Gain, and Click-Free Bypass Crossfade
        for (int s = 0; s < curChunk; ++s)
        {
            const float outGain   = smoothedOutputGain.getNextValue();
            const float activeMix = smoothedBypassGain.getNextValue();
            const float wetGain   = smoothedWetGain.getNextValue() * outGain;
            const float dryGain   = smoothedDryGain.getNextValue() * outGain;
            const float bypassMix = 1.0f - activeMix;

            const float originalL = mainInput.getSample (0, offset + s);
            const float originalR = mainInChannels > 1 ? mainInput.getSample (1, offset + s) : originalL;
            const float processedL = (dryBuffer.getSample (0, s) * dryGain) + (wetBuffer.getSample (0, s) * wetGain);
            const float processedR = (dryBuffer.getSample (1, s) * dryGain) + (wetBuffer.getSample (1, s) * wetGain);
            const float outSampleL = (processedL * activeMix) + (originalL * bypassMix);
            const float outSampleR = (processedR * activeMix) + (originalR * bypassMix);

            if (mainOutChannels == 1)
            {
                mainOutput.setSample (0, offset + s, (outSampleL + outSampleR) * 0.5f);
            }
            else
            {
                mainOutput.setSample (0, offset + s, outSampleL);
                mainOutput.setSample (1, offset + s, outSampleR);
            }
        }
    }

    // Measure true output RMS (reflects bypass and all processing accurately)
    float curRmsOut = 0.0f;
    for (int ch = 0; ch < mainOutChannels; ++ch)
        curRmsOut = juce::jmax (curRmsOut, mainOutput.getRMSLevel (ch, 0, numSamples));
    rmsOutput.store (curRmsOut, std::memory_order_relaxed);

    // Measure Real-Time Stereo Phase Correlation (Mono compatibility check)
    if (mainOutChannels >= 2)
    {
        float sumLR = 0.0f;
        float sumL2 = 0.0f;
        float sumR2 = 0.0f;
        const float* lPtr = mainOutput.getReadPointer (0);
        const float* rPtr = mainOutput.getReadPointer (1);
        for (int s = 0; s < numSamples; ++s)
        {
            sumLR += lPtr[s] * rPtr[s];
            sumL2 += lPtr[s] * lPtr[s];
            sumR2 += rPtr[s] * rPtr[s];
        }
        const float denom = std::sqrt (sumL2 * sumR2);
        const float rawCorr = (denom > 1.0e-5f) ? juce::jlimit (-1.0f, 1.0f, sumLR / denom) : 1.0f;
        const float prev = stereoPhaseCorrelation.load (std::memory_order_relaxed);
        stereoPhaseCorrelation.store (prev + 0.12f * (rawCorr - prev), std::memory_order_relaxed);
    }
    else
    {
        stereoPhaseCorrelation.store (1.0f, std::memory_order_relaxed);
    }
}

bool ProVerbAudioProcessor::hasEditor() const { return true; }
juce::AudioProcessorEditor* ProVerbAudioProcessor::createEditor()
{
    return new ProVerbAudioProcessorEditor (*this);
}

void ProVerbAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    std::unique_ptr<juce::XmlElement> xml (state.createXml());
    if (xml != nullptr)
    {
        xml->setAttribute ("schemaVersion", 2);
        xml->setAttribute ("hasAnalyzedResult", hasAnalyzedResult);
        xml->setAttribute ("detectedInstrument", detectedInstrument);
        xml->setAttribute ("detectedPresetName", detectedPresetName);
        xml->setAttribute ("autoLearnProfileName", autoLearnProfileName);
        xml->setAttribute ("currentProgram", currentProgram);
        copyXmlToBinary (*xml, destData);
    }
}

void ProVerbAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    std::unique_ptr<juce::XmlElement> xmlState (getXmlFromBinary (data, sizeInBytes));
    if (xmlState != nullptr && xmlState->hasTagName (apvts.state.getType()))
    {
        hasAnalyzedResult = xmlState->getBoolAttribute ("hasAnalyzedResult", false);
        detectedInstrument = xmlState->getStringAttribute ("detectedInstrument");
        detectedPresetName = xmlState->getStringAttribute ("detectedPresetName");
        autoLearnProfileName = xmlState->getStringAttribute ("autoLearnProfileName");

        const int totalPresets = static_cast<int> (getPresetList().size());
        const int restoredProg = xmlState->getIntAttribute ("currentProgram", currentProgram);
        if (restoredProg >= 0 && restoredProg < totalPresets)
            currentProgram = restoredProg;
        else
            currentProgram = 0;

        apvts.replaceState (juce::ValueTree::fromXml (*xmlState));
    }
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new ProVerbAudioProcessor();
}
