#pragma once

#include <juce_core/juce_core.h>
#include <juce_audio_processors/juce_audio_processors.h>
#include <vector>
#include <cmath>

/**
 * ReverbPreset
 * 
 * Central maintainable factory preset data structure for ProducerToy ProVerb.
 * All presets represent practical acoustic style starting points for inserts
 * across vocals, instruments, world percussion, ensembles, and musical styles.
 * 
 * Design Principles:
 * 1. Insert starting points: Mix / Wet values (10% to 40%) are calibrated for track inserts.
 *    For DAW Aux / Send Return channels, set Wet to 100%, Dry to 0%, and balance with send level.
 * 2. Low-end integrity: Kick & Bass presets are kept dry or ultra-subtle (Wet 0-6%, LowCut >= 220 Hz)
 *    to preserve low-end punch. Fully dry is often preferable for clean low-end clarity.
 * 3. Honest acoustic labels: Styles are described as "Style" starting points rather
 *    than claiming physical plate, spring, or chamber hardware simulation.
 * 4. Multi-Label AI Source Detection: Detects broad acoustic families (Voice, Drums, Bass, Keys,
 *    Plucked Strings, Bowed Strings, Winds/Brass, Electronic) and suggests matching starting points.
 */
struct ReverbPreset
{
    const char* name;
    const char* category;
    const char* aliases;
    const char* description;
    float size;         // Room Size % (10 to 100)
    float decay;        // Decay Time in seconds (0.1 to 30.0)
    float damping;      // High Frequency Damping % (0 to 100)
    float predelay;     // Pre-delay in ms (0 to 300)
    float diffusion;    // Diffusion % (0 to 100)
    float erBalance;    // Early Reflections % (0 to 100)
    float width;        // Stereo Width % (0 to 200)
    float lowcut;       // Low Cut filter in Hz (20 to 1000)
    float highcut;      // High Cut filter in Hz (1000 to 20000)
    float ducking;      // Ducking % (0 to 100)
    float shimmer;      // Shimmer % (0 to 100)
    float wet;          // Wet Mix % (0 to 100)
    float dry;          // Dry Level % (0 to 100)
    bool freeze;        // Freeze mode
};

class ProVerbPresets
{
public:
    static constexpr int CUSTOM_PRESET_ID = 10000;

    static const std::vector<juce::String>& getCategories()
    {
        static const std::vector<juce::String> categories =
        {
            "All Categories",
            "1. Vocals",
            "2. Drums & Percussion",
            "3. Bass & Low End",
            "4. Keys & Synths",
            "5. Plucked Strings",
            "6. Bowed Strings & Orch",
            "7. Winds & Brass",
            "8. World & Ensembles",
            "9. Style Starting Points"
        };
        return categories;
    }

    static const std::vector<ReverbPreset>& getFactoryPresets()
    {
        static const std::vector<ReverbPreset> presets =
        {
            // Preset 0: Default Init State
            {
                "Init / Default",
                "General",
                "Reset, Default, Neutral, Flat",
                "Neutral starting point with balanced decay and natural early reflections. Insert Wet 35%; set to 100% on send.",
                70.0f, 2.50f, 45.0f, 20.0f, 75.0f, 45.0f, 100.0f, 80.0f, 14000.0f, 0.0f, 0.0f, 35.0f, 100.0f, false
            },

            // =========================================================================
            // CATEGORY 1: VOCALS (Presets 1 to 12)
            // =========================================================================
            {
                "Modern Billboard Pop Vocal",
                "1. Vocals",
                "Billboard Pop, Top 40, Lead Vocal, Studio Plate, Modern Lead",
                "Polished modern commercial pop vocal space with bright high-frequency sheen and musical ducking. Insert Wet 18%; set to 100% on send.",
                68.0f, 2.20f, 36.0f, 32.0f, 75.0f, 42.0f, 130.0f, 150.0f, 12000.0f, 26.0f, 0.0f, 18.0f, 100.0f, false
            },
            {
                "Hip-Hop & Trap Vocal (Tight & Punchy)",
                "1. Vocals",
                "Trap Vocal, Rap Vocal, Modern Hip-Hop, 808 Vocal, Punchy Lead",
                "Tight, short-decay space that adds three-dimensional depth while keeping fast rap lyrics aggressive and upfront. Insert Wet 12%; set to 100% on send.",
                32.0f, 0.75f, 58.0f, 12.0f, 55.0f, 68.0f, 105.0f, 180.0f, 8500.0f, 22.0f, 0.0f, 12.0f, 100.0f, false
            },
            {
                "R&B / Trapsoul Velvet Silk",
                "1. Vocals",
                "Neo-Soul, Trapsoul, Smooth R&B, Velvet Vocal, Falsetto Runs",
                "Silky, warm plate-style reverb space that wraps around expressive runs and falsetto phrases. Insert Wet 20%; set to 100% on send.",
                72.0f, 2.50f, 42.0f, 30.0f, 80.0f, 38.0f, 135.0f, 150.0f, 10500.0f, 24.0f, 0.0f, 20.0f, 100.0f, false
            },
            {
                "Drill & Fast Flow Rap Vocal",
                "1. Vocals",
                "UK Drill, US Drill, Fast Flow, Grime Vocal, Punchy Speech",
                "Ultra-clean, zero low-end mud, fast consonant recovery for rapid-fire multi-syllabic rhymes. Insert Wet 9%; set to 100% on send.",
                26.0f, 0.55f, 62.0f, 8.0f, 50.0f, 75.0f, 95.0f, 220.0f, 8000.0f, 16.0f, 0.0f, 9.0f, 100.0f, false
            },
            {
                "Warm Studio Vocal Hall",
                "1. Vocals",
                "Studio Hall, Ballad Vocal, Cinematic Lead, Expansive Vocal",
                "Lush, expansive hall-style acoustic for emotional anthems, power ballads, and film themes. Insert Wet 22%; set to 100% on send.",
                80.0f, 3.20f, 30.0f, 38.0f, 78.0f, 40.0f, 145.0f, 140.0f, 12500.0f, 25.0f, 0.0f, 22.0f, 100.0f, false
            },
            {
                "Modern Pop Vocal Plate",
                "1. Vocals",
                "Plate-Style Vocal, Bright Vocal Plate, Top 40 Vocal, Dance Vocal",
                "Dense, bright plate-style reflection profile that cuts through modern commercial EDM and pop tracks. Insert Wet 17%; set to 100% on send.",
                64.0f, 2.10f, 42.0f, 35.0f, 80.0f, 38.0f, 135.0f, 160.0f, 12000.0f, 26.0f, 0.0f, 17.0f, 100.0f, false
            },
            {
                "Bollywood & Desi Pop Lead Vocal",
                "1. Vocals",
                "Bollywood Lead, Desi Pop, Hindi Vocal, Punjabi Pop Vocal",
                "Polished commercial Bollywood lead vocal space with presence boost and ducking for vocal consonants. Insert Wet 18%; set to 100% on send.",
                68.0f, 2.40f, 38.0f, 32.0f, 72.0f, 45.0f, 130.0f, 140.0f, 11500.0f, 24.0f, 0.0f, 18.0f, 100.0f, false
            },
            {
                "Romantic Filmi / Ghazal Chamber",
                "1. Vocals",
                "Ghazal, Semi-Classical Vocal, Sufi Vocal, Slow Ballad",
                "Warm acoustic hall tailored for expressive vocal nuances and microtonal phrasing. Insert Wet 20%; set to 100% on send.",
                72.0f, 2.80f, 50.0f, 28.0f, 78.0f, 42.0f, 130.0f, 130.0f, 10000.0f, 18.0f, 0.0f, 20.0f, 100.0f, false
            },
            {
                "Intimate Bedroom Pop / Indie",
                "1. Vocals",
                "Close Vocal, Singer-Songwriter, Bedroom Pop Vocal, Acoustic Lead",
                "Tight, close acoustic room providing subtle dimension without pushing vocals back. Insert Wet 14%; set to 100% on send.",
                35.0f, 0.85f, 55.0f, 18.0f, 60.0f, 65.0f, 100.0f, 140.0f, 9500.0f, 12.0f, 0.0f, 14.0f, 100.0f, false
            },
            {
                "Double-Tracked Vocal Widener",
                "1. Vocals",
                "Vocal Double, Stacks, Side Vocal, Stereo Doubler",
                "Wide early-reflection aura designed to spread doubled vocals around a dry lead. Insert Wet 16%; set to 100% on send.",
                48.0f, 1.10f, 48.0f, 15.0f, 65.0f, 55.0f, 160.0f, 150.0f, 10500.0f, 15.0f, 0.0f, 16.0f, 100.0f, false
            },
            {
                "Backing Vocals & Harmonies",
                "1. Vocals",
                "BVs, Vocal Chords, Background Vocal, Harmony Bus",
                "Wide, diffused reverb profile that blends backing vocal stacks behind the lead. Insert Wet 24%; set to 100% on send.",
                82.0f, 2.30f, 35.0f, 20.0f, 82.0f, 32.0f, 170.0f, 180.0f, 12000.0f, 10.0f, 0.0f, 24.0f, 100.0f, false
            },
            {
                "Ad-Lib & Throw Long Tail",
                "1. Vocals",
                "Vocal Throw, Reverb Throw, Delay Reverb Tail, Vocal FX",
                "Long, dramatic tail with heavy ducking, swelling up seamlessly between vocal phrases. Insert Wet 30%; set to 100% on send.",
                95.0f, 5.50f, 22.0f, 48.0f, 88.0f, 24.0f, 180.0f, 150.0f, 13000.0f, 32.0f, 0.0f, 30.0f, 100.0f, false
            },

            // =========================================================================
            // CATEGORY 2: DRUMS AND PERCUSSION (Presets 13 to 28)
            // =========================================================================
            {
                "Kick & Sub (Dry Tight Anchor)",
                "2. Drums & Percussion",
                "Bass Drum, 808 Kick, Sub Impact, Tight Lows, Dry Option",
                "Fully dry / high-passed profile to keep kick drums punchy and phase-aligned. Wet 0% keeps low end completely unblurred.",
                15.0f, 0.25f, 80.0f, 0.0f, 30.0f, 85.0f, 40.0f, 350.0f, 3500.0f, 0.0f, 0.0f, 0.0f, 100.0f, false
            },
            {
                "Snare Drum - Tight Studio Room",
                "2. Drums & Percussion",
                "Snare Room, Clap Room, Rimshot, Tight Snare",
                "Short, punchy room reflection profile providing physical wooden box presence. Insert Wet 16%; set to 100% on send.",
                25.0f, 0.85f, 58.0f, 8.0f, 45.0f, 75.0f, 100.0f, 160.0f, 10000.0f, 0.0f, 0.0f, 16.0f, 100.0f, false
            },
            {
                "Snare Drum - Bright Plate-Style",
                "2. Drums & Percussion",
                "Snare Plate, 80s Snare, Pop Snare, Metallic Plate",
                "Bright, dense plate-style reflection profile that gives modern snares sustain and snap. Insert Wet 18%; set to 100% on send.",
                52.0f, 1.35f, 32.0f, 14.0f, 78.0f, 45.0f, 120.0f, 180.0f, 13500.0f, 0.0f, 0.0f, 18.0f, 100.0f, false
            },
            {
                "Snare Drum - Big Explosive Ambience",
                "2. Drums & Percussion",
                "Rock Snare, Gated-Style Snare, Arena Snare, Power Snare",
                "Large, expansive room space for arena rock and heavy pop snare hits. Insert Wet 22%; set to 100% on send.",
                78.0f, 2.20f, 40.0f, 20.0f, 82.0f, 50.0f, 140.0f, 150.0f, 11000.0f, 12.0f, 0.0f, 22.0f, 100.0f, false
            },
            {
                "Toms - Focused Space",
                "2. Drums & Percussion",
                "Rack Toms, Floor Tom, Rototoms, Drum Fills",
                "Controlled room profile that adds dimension to tom rolls without muddying the low-mids. Insert Wet 15%; set to 100% on send.",
                42.0f, 1.15f, 55.0f, 6.0f, 60.0f, 65.0f, 110.0f, 140.0f, 9000.0f, 0.0f, 0.0f, 15.0f, 100.0f, false
            },
            {
                "Acoustic Drum Kit - Live Room",
                "2. Drums & Percussion",
                "Drum Bus, Drum Overhead Room, Live Kit, Drum Glue",
                "Believable live room glue for full acoustic drum kit buses and overhead mics. Insert Wet 16%; set to 100% on send.",
                58.0f, 1.25f, 48.0f, 10.0f, 65.0f, 58.0f, 125.0f, 150.0f, 10500.0f, 0.0f, 0.0f, 16.0f, 100.0f, false
            },
            {
                "Electronic Drums - Synthetic Room",
                "2. Drums & Percussion",
                "808 Kit, 909 Kit, Synth Percussion, Trap Drums, Drum Machine",
                "Crisp, diffused modern room for 808/909 samples and synthesized drum hits. Insert Wet 14%; set to 100% on send.",
                45.0f, 1.00f, 35.0f, 5.0f, 75.0f, 60.0f, 120.0f, 180.0f, 12500.0f, 10.0f, 0.0f, 14.0f, 100.0f, false
            },
            {
                "Hi-Hat, Shaker & Tambourine",
                "2. Drums & Percussion",
                "Cymbals, Shakers, Cabasa, Triangle, High Percussion",
                "Band-passed high reflection space that softens harsh top-end transients. Insert Wet 11%; set to 100% on send.",
                32.0f, 0.75f, 65.0f, 5.0f, 50.0f, 70.0f, 110.0f, 350.0f, 11000.0f, 0.0f, 0.0f, 11.0f, 100.0f, false
            },
            {
                "Tabla & Dholak - Intimate Clarity",
                "2. Drums & Percussion",
                "Dayan, Bayan, Dholak, Mridangam, Pakhawaj, Indian Percussion",
                "Tight, articulate acoustic profile with low-frequency filtering to protect the bayan punch. Insert Wet 12%; set to 100% on send.",
                32.0f, 0.70f, 62.0f, 5.0f, 52.0f, 72.0f, 100.0f, 180.0f, 9000.0f, 0.0f, 0.0f, 12.0f, 100.0f, false
            },
            {
                "Dhol & Large Folk Drums",
                "2. Drums & Percussion",
                "Bhangra Dhol, Nagada, Tasha, Chande, Folk Percussion",
                "Punchy, resonant chamber space suited for energetic folk and wedding percussion. Insert Wet 15%; set to 100% on send.",
                40.0f, 1.10f, 55.0f, 8.0f, 65.0f, 68.0f, 110.0f, 170.0f, 10000.0f, 0.0f, 0.0f, 15.0f, 100.0f, false
            },
            {
                "Cajon - Natural Wooden Chamber",
                "2. Drums & Percussion",
                "Flamenco Cajon, Box Drum, Tap Snare, Acoustic Percussion",
                "Warm, organic wooden enclosure profile that supports both bass thump and corner slap. Insert Wet 13%; set to 100% on send.",
                34.0f, 0.80f, 58.0f, 6.0f, 55.0f, 68.0f, 100.0f, 180.0f, 9500.0f, 0.0f, 0.0f, 13.0f, 100.0f, false
            },
            {
                "Conga & Bongo - Latin Room",
                "2. Drums & Percussion",
                "Tumbadora, Quinto, Bongo, Timbales, Latin Percussion",
                "Lively acoustic room that enhances skin slap harmonics and high open tones. Insert Wet 13%; set to 100% on send.",
                38.0f, 0.90f, 50.0f, 7.0f, 58.0f, 65.0f, 105.0f, 190.0f, 10000.0f, 0.0f, 0.0f, 13.0f, 100.0f, false
            },
            {
                "Djembe & Frame Drum",
                "2. Drums & Percussion",
                "Bodhran, Tar, Bendir, Daf, Kanjira, Shamanic Drum",
                "Deep, earthy resonant chamber that flatters animal skin overtones and rim taps. Insert Wet 14%; set to 100% on send.",
                46.0f, 1.15f, 52.0f, 8.0f, 62.0f, 62.0f, 110.0f, 170.0f, 9500.0f, 0.0f, 0.0f, 14.0f, 100.0f, false
            },
            {
                "Darbuka & Riq - Crisp Transients",
                "2. Drums & Percussion",
                "Doumbek, Tombak, Riq, Middle Eastern Percussion",
                "Short, highly articulate room designed to preserve lightning-fast finger roll transients. Insert Wet 12%; set to 100% on send.",
                35.0f, 0.80f, 45.0f, 6.0f, 60.0f, 70.0f, 105.0f, 220.0f, 11000.0f, 0.0f, 0.0f, 12.0f, 100.0f, false
            },
            {
                "Taiko & Large Cinematic Drums",
                "2. Drums & Percussion",
                "War Drums, O-Daiko, Epic Percussion, Film Hits, Gran Cassa",
                "Massive scoring stage space for thunderous film hits and cinematic percussion ensembles. Insert Wet 24%; set to 100% on send.",
                88.0f, 3.20f, 35.0f, 25.0f, 82.0f, 45.0f, 150.0f, 130.0f, 11000.0f, 15.0f, 0.0f, 24.0f, 100.0f, false
            },
            {
                "Percussion Ensemble - Spatial Glue",
                "2. Drums & Percussion",
                "Drum Ensemble, World Percussion Bus, Group Claps, Percussion Bus",
                "Medium room space tailored to glue multiple percussion players into a cohesive performance. Insert Wet 15%; set to 100% on send.",
                55.0f, 1.20f, 48.0f, 12.0f, 65.0f, 60.0f, 120.0f, 180.0f, 10500.0f, 5.0f, 0.0f, 15.0f, 100.0f, false
            },

            // =========================================================================
            // CATEGORY 3: BASS AND LOW-FREQUENCY SOURCES (Presets 29 to 33)
            // =========================================================================
            {
                "Electric Bass - Tight Space",
                "3. Bass & Low End",
                "Fender Bass, Picked Bass, Slap Bass Room, Bass Amp Room",
                "Subtle, high-passed room tone adding amp cabinet body while keeping low fundamentals bone dry. Insert Wet 4%; set to 100% on send.",
                22.0f, 0.45f, 75.0f, 2.0f, 35.0f, 78.0f, 60.0f, 260.0f, 4800.0f, 0.0f, 0.0f, 4.0f, 100.0f, false
            },
            {
                "Upright & Acoustic Bass - Studio Wood",
                "3. Bass & Low End",
                "Contrabass, Double Bass Pizz, Jazz Bass, Acoustic Bass",
                "Warm acoustic room tone for upright bass body resonance with conservative wet mix. Insert Wet 6%; set to 100% on send.",
                32.0f, 0.65f, 68.0f, 4.0f, 45.0f, 72.0f, 75.0f, 220.0f, 5500.0f, 0.0f, 0.0f, 6.0f, 100.0f, false
            },
            {
                "Synth Bass - Midrange Stereo Aura",
                "3. Bass & Low End",
                "Acid Bass, Moog Bass, Reeses Mid Reverb, Bass Width",
                "Stereo space focused purely on harmonic upper mids while sub fundamentals remain dry and mono. Insert Wet 7%; set to 100% on send.",
                38.0f, 0.75f, 50.0f, 5.0f, 55.0f, 65.0f, 100.0f, 280.0f, 8000.0f, 8.0f, 0.0f, 7.0f, 100.0f, false
            },
            {
                "Sub Bass - Safe Low-End Dry Anchor",
                "3. Bass & Low End",
                "Sub 808, Sub Sine, Mono Low Anchor, Sub Kick",
                "High-passed zero-mix safe preset for keeping 808s and sub-bass completely clean and mono. Wet 0% keeps headroom pristine.",
                10.0f, 0.15f, 90.0f, 0.0f, 20.0f, 90.0f, 0.0f, 400.0f, 2000.0f, 0.0f, 0.0f, 0.0f, 100.0f, false
            },
            {
                "Bass Dry / Keep the Low End Tight",
                "3. Bass & Low End",
                "Clean Low End, Mix Saver, Tight Bass Room, Master Safety",
                "Defensive low-end setting; adds faint upper-mid dimension while completely protecting sub headroom. Insert Wet 4%; set to 100% on send.",
                20.0f, 0.45f, 75.0f, 0.0f, 35.0f, 80.0f, 75.0f, 250.0f, 5000.0f, 0.0f, 0.0f, 4.0f, 100.0f, false
            },

            // =========================================================================
            // CATEGORY 4: KEYS, MALLETS, AND SYNTHS (Presets 34 to 42)
            // =========================================================================
            {
                "Grand Piano - Concert Hall",
                "4. Keys & Synths",
                "Acoustic Piano, Steinway, Classical Piano, Concert Grand",
                "Warm, expansive concert hall with generous stereo width and natural high-frequency decay. Insert Wet 20%; set to 100% on send.",
                78.0f, 2.50f, 36.0f, 24.0f, 78.0f, 45.0f, 135.0f, 120.0f, 13000.0f, 8.0f, 0.0f, 20.0f, 100.0f, false
            },
            {
                "Upright Piano - Warm Living Room",
                "4. Keys & Synths",
                "Felt Piano, Intimate Piano, Honky-Tonk, Indie Piano",
                "Intimate wooden parlor room providing warmth and closeness for felt and upright pianos. Insert Wet 15%; set to 100% on send.",
                42.0f, 1.15f, 58.0f, 12.0f, 62.0f, 60.0f, 110.0f, 140.0f, 9500.0f, 0.0f, 0.0f, 15.0f, 100.0f, false
            },
            {
                "Electric Piano & Rhodes - Sweet Chime",
                "4. Keys & Synths",
                "Fender Rhodes, Wurlitzer, DX7 E-Piano, Clavinet",
                "Lush plate-style space that enhances bell-like tine harmonics and tremolo motion. Insert Wet 16%; set to 100% on send.",
                55.0f, 1.60f, 42.0f, 16.0f, 72.0f, 48.0f, 125.0f, 130.0f, 11000.0f, 5.0f, 0.0f, 16.0f, 100.0f, false
            },
            {
                "Organ & Harmonium - Acoustic Sanctum",
                "4. Keys & Synths",
                "Harmonium, B3 Organ, Pipe Organ, Accordion, Reed Organ",
                "Enveloping sanctuary space designed for sustaining reed and pipe instruments. Insert Wet 17%; set to 100% on send.",
                65.0f, 2.10f, 46.0f, 18.0f, 75.0f, 50.0f, 120.0f, 140.0f, 10500.0f, 6.0f, 0.0f, 17.0f, 100.0f, false
            },
            {
                "Celesta & Glockenspiel - Crystal Chamber",
                "4. Keys & Synths",
                "Music Box, Toy Piano, Chimes, Bell Lyre, Metallic Mallets",
                "Bright, crystalline reflection space with airy top-end sheen for delicate bells. Insert Wet 18%; set to 100% on send.",
                62.0f, 1.90f, 24.0f, 15.0f, 70.0f, 52.0f, 130.0f, 200.0f, 15000.0f, 0.0f, 10.0f, 18.0f, 100.0f, false
            },
            {
                "Vibraphone & Marimba - Resonant Mallets",
                "4. Keys & Synths",
                "Xylophone, Balafon, Steelpan, Kalimba, Wooden Mallets",
                "Acoustic chamber tuned to preserve fast mallet attack transients and bar warmth. Insert Wet 15%; set to 100% on send.",
                50.0f, 1.45f, 45.0f, 14.0f, 68.0f, 55.0f, 115.0f, 150.0f, 11500.0f, 4.0f, 0.0f, 15.0f, 100.0f, false
            },
            {
                "Synth Lead & Pluck - Polished Modern",
                "4. Keys & Synths",
                "Arpeggio, EDM Pluck, Synth Hook, PolySynth, Synth Lead",
                "Modern stereo space with ducking to keep fast arpeggio notes clean and sharp. Insert Wet 18%; set to 100% on send.",
                58.0f, 1.60f, 32.0f, 18.0f, 74.0f, 45.0f, 135.0f, 160.0f, 13000.0f, 12.0f, 0.0f, 18.0f, 100.0f, false
            },
            {
                "Soft Pad & Ambient Texture",
                "4. Keys & Synths",
                "String Pad, Synth Wash, Warm Atmosphere, Ambient Pad",
                "Expansive, wide reverberant cloud that surrounds pads with smooth stereo diffusion. Insert Wet 26%; set to 100% on send.",
                84.0f, 3.80f, 28.0f, 32.0f, 84.0f, 32.0f, 150.0f, 140.0f, 12500.0f, 10.0f, 0.0f, 26.0f, 100.0f, false
            },
            {
                "Long Ambient Pad - Infinite Bloom",
                "4. Keys & Synths",
                "Drone, Shimmer Pad, Ethereal Cloud, Space Wash",
                "Extended ambient space with near-lossless circulation for dreamlike soundscapes. Insert Wet 34%; set to 100% on send.",
                96.0f, 6.80f, 18.0f, 42.0f, 90.0f, 22.0f, 160.0f, 120.0f, 14500.0f, 14.0f, 25.0f, 34.0f, 100.0f, false
            },

            // =========================================================================
            // CATEGORY 5: PLUCKED STRINGS (Presets 43 to 51)
            // =========================================================================
            {
                "Steel-String Acoustic Guitar",
                "5. Plucked Strings",
                "Folk Guitar, Dreadnought, 12-String, Strummed Guitar",
                "Natural acoustic room that flatters wood body warmth while avoiding low-mid boxiness. Insert Wet 13%; set to 100% on send.",
                50.0f, 1.20f, 55.0f, 18.0f, 65.0f, 50.0f, 115.0f, 150.0f, 10000.0f, 5.0f, 0.0f, 13.0f, 100.0f, false
            },
            {
                "Nylon & Classical Guitar - Warm Salon",
                "5. Plucked Strings",
                "Spanish Guitar, Fingerstyle, Flamenco Guitar, Classical Guitar",
                "Warm wooden salon environment that enhances the round touch of fingerpicked nylon strings. Insert Wet 14%; set to 100% on send.",
                48.0f, 1.35f, 60.0f, 16.0f, 68.0f, 52.0f, 110.0f, 140.0f, 9000.0f, 4.0f, 0.0f, 14.0f, 100.0f, false
            },
            {
                "Clean Electric Guitar - Studio Plate-Style",
                "5. Plucked Strings",
                "Stratocaster, Funk Guitar, Jazz Guitar, R&B Guitar, Telecaster",
                "Silky plate-style reverb profile adding vintage sparkle and depth to clean guitar riffs. Insert Wet 16%; set to 100% on send.",
                60.0f, 1.70f, 50.0f, 20.0f, 72.0f, 45.0f, 125.0f, 150.0f, 10000.0f, 8.0f, 0.0f, 16.0f, 100.0f, false
            },
            {
                "Distorted & Solo Electric Guitar",
                "5. Plucked Strings",
                "Guitar Solo, Rock Lead, Heavy Guitar, Overdrive Lead",
                "Wide, punchy hall-style space with ducking to maintain articulate solo attack. Insert Wet 18%; set to 100% on send.",
                68.0f, 2.20f, 45.0f, 25.0f, 76.0f, 42.0f, 135.0f, 160.0f, 9000.0f, 14.0f, 0.0f, 18.0f, 100.0f, false
            },
            {
                "Sitar & Classical Plucked Strings",
                "5. Plucked Strings",
                "Sarod, Veena, Tanpura, Surbahar, Indian Plucked Strings",
                "Rich acoustic chamber designed to complement sympathetic string overtones without mud. Insert Wet 17%; set to 100% on send.",
                60.0f, 1.70f, 40.0f, 25.0f, 70.0f, 45.0f, 125.0f, 160.0f, 11000.0f, 8.0f, 0.0f, 17.0f, 100.0f, false
            },
            {
                "Oud, Bouzouki & Saz",
                "5. Plucked Strings",
                "Baglama, Cumbus, Mandocello, Mediterranean Lute, Middle Eastern Lute",
                "Articulate acoustic room that highlights fast plectrum tremolo and bowl body resonance. Insert Wet 15%; set to 100% on send.",
                52.0f, 1.40f, 48.0f, 18.0f, 68.0f, 50.0f, 120.0f, 170.0f, 10000.0f, 6.0f, 0.0f, 15.0f, 100.0f, false
            },
            {
                "Mandolin, Banjo & Lute",
                "5. Plucked Strings",
                "Ukulele, Charango, Balalaika, Bluegrass Strings, Folk Pluck",
                "Crisp, short-decay space that controls sharp plucks and prevents flutter echo. Insert Wet 13%; set to 100% on send.",
                44.0f, 1.10f, 56.0f, 14.0f, 64.0f, 56.0f, 115.0f, 180.0f, 9800.0f, 5.0f, 0.0f, 13.0f, 100.0f, false
            },
            {
                "Harp - Celestial Chamber",
                "5. Plucked Strings",
                "Concert Harp, Celtic Harp, Swarmandal, Santoor, Dulcimer",
                "Lush, resonant hall profile providing elegant sustain for arpeggiated glissandi. Insert Wet 20%; set to 100% on send.",
                76.0f, 2.60f, 32.0f, 22.0f, 78.0f, 42.0f, 135.0f, 130.0f, 12500.0f, 6.0f, 0.0f, 20.0f, 100.0f, false
            },
            {
                "Koto, Shamisen, Guzheng & Pipa",
                "5. Plucked Strings",
                "Gayageum, Dan Tranh, Chinese Zither, Biwa, East Asian Pluck",
                "Transparent, poetic acoustic space honoring delicate bending nuances and string snaps. Insert Wet 16%; set to 100% on send.",
                58.0f, 1.65f, 42.0f, 20.0f, 70.0f, 48.0f, 125.0f, 160.0f, 11000.0f, 7.0f, 0.0f, 16.0f, 100.0f, false
            },

            // =========================================================================
            // CATEGORY 6: BOWED STRINGS AND ORCHESTRA (Presets 52 to 57)
            // =========================================================================
            {
                "Solo Violin - Expressive Hall",
                "6. Bowed Strings & Orch",
                "Violin Solo, Fiddle, Sarangi Solo, Dilruba, Bowed Lead",
                "Silky concert hall space that softens bow-hair rasp while preserving sweet vibrato. Insert Wet 18%; set to 100% on send.",
                70.0f, 2.10f, 42.0f, 25.0f, 76.0f, 44.0f, 130.0f, 160.0f, 11500.0f, 10.0f, 0.0f, 18.0f, 100.0f, false
            },
            {
                "Solo Viola & Cello - Deep Resonance",
                "6. Bowed Strings & Orch",
                "Cello Solo, Viola Solo, Esraj, Kamancha, Cello Lead",
                "Warm, resonant hall tuned to support rich cello chest tones and low-mid warmth. Insert Wet 19%; set to 100% on send.",
                72.0f, 2.30f, 46.0f, 26.0f, 76.0f, 46.0f, 130.0f, 140.0f, 10500.0f, 8.0f, 0.0f, 19.0f, 100.0f, false
            },
            {
                "Double Bass - Bowed Acoustic Depth",
                "6. Bowed Strings & Orch",
                "Arco Bass, Orchestral Bass, Contrabass Solo, Bass Bow",
                "Controlled room profile that adds majestic hall depth without muddying fundamental low notes. Insert Wet 14%; set to 100% on send.",
                60.0f, 1.60f, 58.0f, 16.0f, 68.0f, 54.0f, 105.0f, 160.0f, 8000.0f, 5.0f, 0.0f, 14.0f, 100.0f, false
            },
            {
                "Pizzicato Strings - Crisp Room",
                "6. Bowed Strings & Orch",
                "Plucked Orchestra, Spiccato Strings, Staccato Strings, String Pluck",
                "Tight, articulate room profile that sharpens rhythmic string pluck definitions. Insert Wet 14%; set to 100% on send.",
                45.0f, 1.15f, 50.0f, 12.0f, 64.0f, 62.0f, 120.0f, 160.0f, 10500.0f, 0.0f, 0.0f, 14.0f, 100.0f, false
            },
            {
                "String Section - Lush Scoring Stage",
                "6. Bowed Strings & Orch",
                "String Quartet, Chamber Strings, Bollywood String Bus, String Section",
                "Expansive scoring stage acoustic providing cinematic sheen for string section runs. Insert Wet 23%; set to 100% on send.",
                84.0f, 2.90f, 32.0f, 30.0f, 82.0f, 38.0f, 150.0f, 150.0f, 13000.0f, 8.0f, 0.0f, 23.0f, 100.0f, false
            },
            {
                "Orchestral Strings & Cinematic Ensemble",
                "6. Bowed Strings & Orch",
                "Full Orchestra Strings, Epic Strings, Symphony Hall, Cinematic Strings",
                "Large symphonic auditorium space designed for rich, soaring orchestral climaxes. Insert Wet 26%; set to 100% on send.",
                92.0f, 3.60f, 26.0f, 36.0f, 86.0f, 32.0f, 160.0f, 140.0f, 13500.0f, 10.0f, 0.0f, 26.0f, 100.0f, false
            },

            // =========================================================================
            // CATEGORY 7: WINDS AND BRASS (Presets 58 to 65)
            // =========================================================================
            {
                "Flute & Bansuri - Breath of Sky",
                "7. Winds & Brass",
                "Indian Bansuri, Concert Flute, Pan Flute, Piccolo, Bamboo Flute",
                "Airy, open hall profile that carries delicate breath overtones and graceful ornaments. Insert Wet 19%; set to 100% on send.",
                68.0f, 2.10f, 35.0f, 28.0f, 75.0f, 42.0f, 135.0f, 150.0f, 13000.0f, 12.0f, 0.0f, 19.0f, 100.0f, false
            },
            {
                "Shakuhachi, Ney, Duduk & World Winds",
                "7. Winds & Brass",
                "Duduk, Ney, Shakuhachi, Xiao, Ocarina, Quena, World Winds",
                "Mystical, atmospheric hall profile that brings out the haunting, breathy soul of ethnic flutes. Insert Wet 21%; set to 100% on send.",
                75.0f, 2.60f, 38.0f, 32.0f, 78.0f, 40.0f, 135.0f, 160.0f, 11000.0f, 10.0f, 0.0f, 21.0f, 100.0f, false
            },
            {
                "Clarinet, Oboe & Bassoon - Chamber Wood",
                "7. Winds & Brass",
                "Woodwinds, English Horn, Contrabassoon, Shehnai, Reed Winds",
                "Intimate wooden chamber tuned for smooth reed harmonics and melodic chamber woodwinds. Insert Wet 16%; set to 100% on send.",
                56.0f, 1.60f, 48.0f, 18.0f, 70.0f, 52.0f, 120.0f, 150.0f, 10500.0f, 6.0f, 0.0f, 16.0f, 100.0f, false
            },
            {
                "Saxophone - Velvet Lounge",
                "7. Winds & Brass",
                "Alto Sax, Tenor Sax, Baritone Sax, Soprano Sax, Jazz Sax",
                "Smooth, warm club ambiance that flatters smoky sax vibrato and breath articulation. Insert Wet 17%; set to 100% on send.",
                60.0f, 1.85f, 45.0f, 22.0f, 72.0f, 48.0f, 125.0f, 150.0f, 11000.0f, 10.0f, 0.0f, 17.0f, 100.0f, false
            },
            {
                "Trumpet & Trombone - Warm Brass Room",
                "7. Winds & Brass",
                "Lead Trumpet, Trombone Solo, Flugelhorn, Cornet, Solo Brass",
                "Balanced brass chamber space that tames piercing high frequencies while preserving punch. Insert Wet 15%; set to 100% on send.",
                58.0f, 1.55f, 52.0f, 18.0f, 68.0f, 52.0f, 125.0f, 160.0f, 10000.0f, 6.0f, 0.0f, 15.0f, 100.0f, false
            },
            {
                "French Horn & Tuba - Symphonic Horizon",
                "7. Winds & Brass",
                "French Horn, Tuba, Euphonium, Orchestral Brass, Low Horns",
                "Majestic symphonic hall acoustic for heroic French horn calls and deep orchestral foundation. Insert Wet 22%; set to 100% on send.",
                80.0f, 2.80f, 40.0f, 28.0f, 80.0f, 38.0f, 135.0f, 140.0f, 10500.0f, 6.0f, 0.0f, 22.0f, 100.0f, false
            },
            {
                "Brass Section - Punchy Stage",
                "7. Winds & Brass",
                "Horn Section, Pop Brass, Big Band Brass, Bollywood Horns, Horns Bus",
                "Dynamic stage environment that glues brass stabs together without muddying fast syncopations. Insert Wet 15%; set to 100% on send.",
                62.0f, 1.70f, 55.0f, 18.0f, 68.0f, 48.0f, 125.0f, 160.0f, 9500.0f, 5.0f, 0.0f, 15.0f, 100.0f, false
            },
            {
                "Wind Ensemble - Acoustic Concert",
                "7. Winds & Brass",
                "Symphonic Winds, Woodwind Choir, Pipe & Flute Ensemble, Wind Band",
                "Open concert hall space that unifies diverse woodwind and brass instruments into a cohesive choir. Insert Wet 20%; set to 100% on send.",
                74.0f, 2.30f, 38.0f, 24.0f, 76.0f, 44.0f, 135.0f, 150.0f, 12000.0f, 8.0f, 0.0f, 20.0f, 100.0f, false
            },

            // =========================================================================
            // CATEGORY 8: WORLD, FOLK, AND ENSEMBLE PROFILES (Presets 66 to 72)
            // =========================================================================
            {
                "Indian Classical & Folk Ensemble",
                "8. World & Ensembles",
                "Baul Ensemble, Qawwali Group, Carnatic Concert, Raga Space, Folk Group",
                "Believable performance space unifying Indian strings, flutes, and percussion naturally. Insert Wet 16%; set to 100% on send.",
                64.0f, 1.80f, 45.0f, 20.0f, 72.0f, 50.0f, 125.0f, 160.0f, 10500.0f, 8.0f, 0.0f, 16.0f, 100.0f, false
            },
            {
                "Middle Eastern & Turkish Ensemble",
                "8. World & Ensembles",
                "Takht Ensemble, Arabic Orchestra, Anatolian Folk, Oriental Ensemble",
                "Warm, resonant stone courtyard space tailored for microtonal strings and hand drums. Insert Wet 16%; set to 100% on send.",
                62.0f, 1.75f, 42.0f, 18.0f, 72.0f, 48.0f, 125.0f, 170.0f, 11000.0f, 8.0f, 0.0f, 16.0f, 100.0f, false
            },
            {
                "East Asian Ensemble - Temple Hall",
                "8. World & Ensembles",
                "Gagaku, Chinese Silk & Bamboo, Traditional Court Space, Temple Hall",
                "Refined, resonant wooden temple hall profile honoring space and silence between notes. Insert Wet 18%; set to 100% on send.",
                70.0f, 2.20f, 36.0f, 24.0f, 75.0f, 46.0f, 130.0f, 150.0f, 11500.0f, 6.0f, 0.0f, 18.0f, 100.0f, false
            },
            {
                "African & Latin Percussion Ensemble",
                "8. World & Ensembles",
                "Bata Drumming, Afrobeat Percussion, Salsa Rhythm Section, Poly-Rhythm",
                "Fast, lively acoustic room that prevents dense polyrhythms from losing their attack. Insert Wet 14%; set to 100% on send.",
                50.0f, 1.15f, 52.0f, 10.0f, 62.0f, 62.0f, 120.0f, 180.0f, 9500.0f, 5.0f, 0.0f, 14.0f, 100.0f, false
            },
            {
                "Celtic & Folk Acoustic Ensemble",
                "8. World & Ensembles",
                "Ceilidh Band, Bluegrass Jam, Acoustic Folk Session, Pub Acoustic",
                "Cozy acoustic room profile that places fiddle, bodhran, whistle, and guitar in the same room. Insert Wet 14%; set to 100% on send.",
                52.0f, 1.30f, 54.0f, 14.0f, 66.0f, 56.0f, 120.0f, 160.0f, 10000.0f, 4.0f, 0.0f, 14.0f, 100.0f, false
            },
            {
                "Small Acoustic Ensemble - Intimate Glue",
                "8. World & Ensembles",
                "Chamber Music, Coffeehouse Group, Unplugged Trio, Acoustic Bus Glue",
                "Transparent, organic acoustic glue for small live acoustic groups and bus channels. Insert Wet 12%; set to 100% on send.",
                48.0f, 1.05f, 52.0f, 12.0f, 62.0f, 60.0f, 110.0f, 170.0f, 10000.0f, 4.0f, 0.0f, 12.0f, 100.0f, false
            },
            {
                "Large Orchestral Ensemble - Concert Hall",
                "8. World & Ensembles",
                "Symphony Orchestra, Film Score Bus, Scoring Hall, Symphony Hall",
                "Deep, opulent concert hall environment for complete orchestral works and film scores. Insert Wet 24%; set to 100% on send.",
                90.0f, 3.40f, 28.0f, 32.0f, 84.0f, 34.0f, 150.0f, 140.0f, 13500.0f, 8.0f, 0.0f, 24.0f, 100.0f, false
            },

            // =========================================================================
            // CATEGORY 9: STYLE STARTING POINTS (Presets 73 to 89)
            // =========================================================================
            {
                "Space: Modern Bollywood Mix Bus",
                "9. Style Starting Points",
                "Bollywood Pop, Desi Pop, Hindi Vocal, Punjabi Pop Vocal, Mix Bus",
                "Commercial Bollywood lead vocal starting point with modern high-frequency polish and ducking. Insert Wet 18%; set to 100% on send.",
                68.0f, 2.40f, 38.0f, 32.0f, 72.0f, 45.0f, 130.0f, 140.0f, 11500.0f, 24.0f, 0.0f, 18.0f, 100.0f, false
            },
            {
                "Space: Romantic Filmi Ballad Space",
                "9. Style Starting Points",
                "Bollywood Ballad, Aashiqui Style, Love Song Vocal, Romantic Track",
                "Dreamy, spacious hall-style space designed for emotional love songs and cinematic vocal phrases. Insert Wet 20%; set to 100% on send.",
                74.0f, 3.00f, 35.0f, 36.0f, 78.0f, 42.0f, 135.0f, 140.0f, 11000.0f, 22.0f, 0.0f, 20.0f, 100.0f, false
            },
            {
                "Space: Desi Club Dancefloor Energy",
                "9. Style Starting Points",
                "Bollywood Club, Item Song, Punjabi Pop Beat, Dancefloor Space",
                "Punchy, controlled dance track space that keeps high-tempo beats dry while adding hype. Insert Wet 15%; set to 100% on send.",
                52.0f, 1.30f, 48.0f, 16.0f, 68.0f, 55.0f, 125.0f, 180.0f, 11000.0f, 18.0f, 0.0f, 15.0f, 100.0f, false
            },
            {
                "Space: Cinematic Historical Epic",
                "9. Style Starting Points",
                "Period Drama, Historical Score, Epic Bollywood, Film Score Bus",
                "Grand, dramatic film scoring stage profile for historical epics and orchestral climaxes. Insert Wet 24%; set to 100% on send.",
                88.0f, 3.50f, 26.0f, 38.0f, 84.0f, 36.0f, 150.0f, 140.0f, 13000.0f, 14.0f, 0.0f, 24.0f, 100.0f, false
            },
            {
                "Space: Mehfil Intimate Chamber",
                "9. Style Starting Points",
                "Mehfil, Live Classical, Khayal, Thumri, Intimate Concert",
                "Warm, respectful acoustic space mimicking an intimate mehfil chamber setting. Insert Wet 19%; set to 100% on send.",
                70.0f, 2.60f, 52.0f, 26.0f, 76.0f, 44.0f, 125.0f, 140.0f, 9500.0f, 12.0f, 0.0f, 19.0f, 100.0f, false
            },
            {
                "Space: Top-40 Radio Air Hitroom",
                "9. Style Starting Points",
                "Commercial Pop, Top 40, Synth Pop Space, Radio Vocal",
                "Clean, articulate commercial pop starting point that keeps lead vocals centered and bright. Insert Wet 17%; set to 100% on send.",
                62.0f, 1.95f, 36.0f, 30.0f, 75.0f, 42.0f, 130.0f, 160.0f, 12500.0f, 24.0f, 0.0f, 17.0f, 100.0f, false
            },
            {
                "Space: Silky Velvet Neo-Soul",
                "9. Style Starting Points",
                "Neo-Soul, Slow Jam, Smooth R&B, Trapsoul Vocal",
                "Silky, warm plate-style reverb space that wraps around expressive runs and falsetto phrases. Insert Wet 20%; set to 100% on send.",
                72.0f, 2.60f, 44.0f, 32.0f, 80.0f, 38.0f, 135.0f, 150.0f, 10500.0f, 22.0f, 0.0f, 20.0f, 100.0f, false
            },
            {
                "Space: Drill & Trap Tight Iso-Room",
                "9. Style Starting Points",
                "Trap Vocal, Boom Bap Space, Modern Rap, Drill Vocal",
                "Short, highly focused space that adds subtle depth without softening aggressive vocal delivery. Insert Wet 11%; set to 100% on send.",
                30.0f, 0.70f, 58.0f, 10.0f, 52.0f, 70.0f, 100.0f, 180.0f, 8500.0f, 14.0f, 0.0f, 11.0f, 100.0f, false
            },
            {
                "Space: EDM Festival Mainstage Drop",
                "9. Style Starting Points",
                "Festival Drop, Big Room, Progressive House, Future Bass Space",
                "Huge, wide stereo space with high ducking, allowing huge synth drops to explode cleanly. Insert Wet 25%; set to 100% on send.",
                82.0f, 3.20f, 25.0f, 24.0f, 84.0f, 35.0f, 160.0f, 160.0f, 14000.0f, 28.0f, 0.0f, 25.0f, 100.0f, false
            },
            {
                "Space: Live Studio Live-Room A",
                "9. Style Starting Points",
                "Indie Rock, Modern Rock, Drum Ambience, Guitar Amp Space",
                "Acoustically lively live-room character that adds natural grit and energy to guitars and drums. Insert Wet 15%; set to 100% on send.",
                54.0f, 1.40f, 52.0f, 14.0f, 66.0f, 58.0f, 125.0f, 150.0f, 9500.0f, 8.0f, 0.0f, 15.0f, 100.0f, false
            },
            {
                "Space: Djent & Blast Beat Definition",
                "9. Style Starting Points",
                "Heavy Metal, Djent, Blast Beat Reverb, Extreme Metal",
                "Very short, dark space designed so rapid blast beats and double-kick runs stay tight and readable. Insert Wet 12%; set to 100% on send.",
                34.0f, 0.80f, 62.0f, 8.0f, 55.0f, 70.0f, 105.0f, 190.0f, 8500.0f, 10.0f, 0.0f, 12.0f, 100.0f, false
            },
            {
                "Space: Vintage Blue Note Club",
                "9. Style Starting Points",
                "Jazz Trio, Acoustic Blues, Club Acoustic, Intimate Jazz",
                "Classic warm club acoustic providing natural air around upright bass, brushes, and piano. Insert Wet 14%; set to 100% on send.",
                48.0f, 1.25f, 58.0f, 15.0f, 64.0f, 58.0f, 115.0f, 150.0f, 9000.0f, 4.0f, 0.0f, 14.0f, 100.0f, false
            },
            {
                "Space: Natural Porch Acoustic Room",
                "9. Style Starting Points",
                "Americana, Acoustic Duo, Warm Living Room, Intimate Folk",
                "Unpretentious, organic room dimension that complements natural acoustic guitars and candid vocals. Insert Wet 13%; set to 100% on send.",
                46.0f, 1.15f, 55.0f, 16.0f, 64.0f, 56.0f, 115.0f, 150.0f, 9500.0f, 6.0f, 0.0f, 13.0f, 100.0f, false
            },
            {
                "Space: Philharmonic Symphony Auditorium",
                "9. Style Starting Points",
                "Auditorium, Philharmonic Hall, Cathedral, Classical Stage",
                "Spacious auditorium acoustic honoring natural symphonic decay and orchestral section balance. Insert Wet 22%; set to 100% on send.",
                88.0f, 3.20f, 30.0f, 30.0f, 82.0f, 35.0f, 150.0f, 140.0f, 13000.0f, 6.0f, 0.0f, 22.0f, 100.0f, false
            },
            {
                "Space: Vintage Jamaican Dub Echo",
                "9. Style Starting Points",
                "King Tubby Style, Dub Reverb, Reggae Snare, Echo Chamber",
                "Characterful resonant space inspired by vintage Jamaican dub echo chambers and spring units. Insert Wet 22%; set to 100% on send.",
                60.0f, 2.40f, 48.0f, 35.0f, 70.0f, 45.0f, 130.0f, 170.0f, 8000.0f, 12.0f, 0.0f, 22.0f, 100.0f, false
            },
            {
                "Space: Vibrant Latin Ballroom",
                "9. Style Starting Points",
                "Salsa, Bachata, Reggaeton Space, Merengue, Latin Ballroom",
                "Vibrant, energetic dancehall space that brings horn sections and Latin hand percussion alive. Insert Wet 15%; set to 100% on send.",
                56.0f, 1.45f, 46.0f, 14.0f, 68.0f, 52.0f, 125.0f, 170.0f, 10500.0f, 6.0f, 0.0f, 15.0f, 100.0f, false
            },
            {
                "Space: Ambient Infinite Cosmos",
                "9. Style Starting Points",
                "Sound Design, Infinite Tail, Sci-Fi Drone, Nebula, Space Reverb",
                "Ethereal, near-boundless atmospheric wash for sci-fi sound design, film cues, and evolving drones. Insert Wet 40%; set to 100% on send.",
                98.0f, 8.50f, 14.0f, 45.0f, 92.0f, 20.0f, 175.0f, 100.0f, 16000.0f, 18.0f, 35.0f, 40.0f, 100.0f, false
            }
        };

        return presets;
    }

    // Search and filter helper for categories and instrument aliases
    static std::vector<int> searchPresets (int categoryIndex, const juce::String& searchQuery)
    {
        return getFilteredIndices (categoryIndex, searchQuery);
    }

    static std::vector<int> getFilteredIndices (int categoryIndex, const juce::String& searchQuery)
    {
        std::vector<int> results;
        const auto& presets = getFactoryPresets();
        const auto& categories = getCategories();
        
        juce::String catFilter;
        if (categoryIndex > 0 && categoryIndex < static_cast<int> (categories.size()))
            catFilter = categories[static_cast<size_t> (categoryIndex)];

        const juce::String query = searchQuery.trim().toLowerCase();

        for (int i = 0; i < static_cast<int> (presets.size()); ++i)
        {
            const auto& p = presets[static_cast<size_t> (i)];
            
            // Category match check
            if (catFilter.isNotEmpty() && ! juce::String (p.category).equalsIgnoreCase (catFilter))
                continue;

            // Search query match check (name, aliases, category)
            if (query.isNotEmpty())
            {
                const juce::String nameStr = juce::String (p.name).toLowerCase();
                const juce::String aliasStr = juce::String (p.aliases).toLowerCase();
                const juce::String catStr = juce::String (p.category).toLowerCase();

                if (! nameStr.contains (query) && ! aliasStr.contains (query) && ! catStr.contains (query))
                    continue;
            }

            results.push_back (i);
        }

        return results;
    }

    // Identifies if current processor parameters match any known factory preset
    static int findMatchingPresetIndex (const juce::AudioProcessorValueTreeState& apvts)
    {
        auto getVal = [&](const juce::String& id) -> float
        {
            auto* p = apvts.getRawParameterValue (id);
            return p != nullptr ? p->load() : 0.0f;
        };

        const float s    = getVal ("size");
        const float d    = getVal ("decaySeconds");
        const float damp = getVal ("damping");
        const float pre  = getVal ("predelayMs");
        const float diff = getVal ("diffusion");
        const float w    = getVal ("width");
        const float lc   = getVal ("lowCut");
        const float hc   = getVal ("highCut");
        const float dk   = getVal ("ducking");
        const float shim = getVal ("shimmer");
        const float wt   = getVal ("wet");
        const bool  frz  = getVal ("freeze") > 0.5f;

        const auto& presets = getFactoryPresets();
        for (size_t i = 0; i < presets.size(); ++i)
        {
            const auto& p = presets[i];
            if (std::abs (s - p.size) < 1.0f &&
                std::abs (d - p.decay) < 0.15f &&
                std::abs (damp - p.damping) < 1.0f &&
                std::abs (pre - p.predelay) < 1.0f &&
                std::abs (diff - p.diffusion) < 1.0f &&
                std::abs (w - p.width) < 2.0f &&
                std::abs (lc - p.lowcut) < 6.0f &&
                std::abs (hc - p.highcut) < 60.0f &&
                std::abs (dk - p.ducking) < 1.0f &&
                std::abs (shim - p.shimmer) < 1.0f &&
                std::abs (wt - p.wet) < 1.0f &&
                frz == p.freeze)
            {
                return static_cast<int> (i);
            }
        }
        return -1; // -1 indicates Custom / User-modified settings
    }
};
