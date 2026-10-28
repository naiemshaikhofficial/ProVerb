cmake_minimum_required(VERSION 3.22)

# ==============================================================================
# ProducerToy — ProVerb Smart Algorithmic Reverb
# Commercial Plugin Build & Test Pipeline
# ==============================================================================
project(ProVerb VERSION 1.0.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Platform verification
if(WIN32)
    if(CMAKE_SIZEOF_VOID_P EQUAL 8)
        message(STATUS "[ProVerb] Targeted OS: Windows x64 (MSVC ABI)")
    else()
        message(STATUS "[ProVerb] Targeted OS: Windows x86 32-bit (MSVC ABI)")
    endif()
    if(MSVC)
        add_compile_options(/utf-8)
        # Release Binary Security Hardening:
        # /GS: Buffer Security Check
        # /guard:cf: Control Flow Guard
        add_compile_options("$<$<CONFIG:Release>:/GS;/guard:cf>")
        if(CMAKE_SIZEOF_VOID_P EQUAL 8)
            add_link_options("$<$<CONFIG:Release>:/DYNAMICBASE;/HIGHENTROPYVA;/NXCOMPAT;/GUARD:CF>")
        else()
            add_link_options("$<$<CONFIG:Release>:/DYNAMICBASE;/NXCOMPAT;/GUARD:CF>")
        endif()
    endif()
else()
    message(STATUS "[ProVerb] Targeted OS: Cross-platform POSIX")
endif()

include(FetchContent)

# Automatically fetch official JUCE 8 Framework
FetchContent_Declare(
    JUCE
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG        8.0.4
)
FetchContent_MakeAvailable(JUCE)

juce_add_binary_data(ProVerbResources
    SOURCES
        Resources/producertoy_logo.png
        Resources/proverb_logo.png
        Resources/producertoy_mascot.png
)

# 1. Plugin formats: Pure DAW formats (VST3, AU, AAX)
# Standalone is disabled by default per DAW-only production specification
option(PROVERB_BUILD_STANDALONE "Build Standalone executable target" OFF)

set(PROVERB_FORMATS VST3)

if(PROVERB_BUILD_STANDALONE)
    list(APPEND PROVERB_FORMATS Standalone)
endif()

# Optional AAX support (conditional on Avid AAX SDK availability)
if(DEFINED AAX_SDK_PATH OR DEFINED ENV{AAX_SDK_PATH} OR EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/SDKs/aax_sdk")
    if(DEFINED ENV{AAX_SDK_PATH})
        set(PROVERB_AAX_DIR "$ENV{AAX_SDK_PATH}")
    elseif(DEFINED AAX_SDK_PATH)
        set(PROVERB_AAX_DIR "${AAX_SDK_PATH}")
    else()
        set(PROVERB_AAX_DIR "${CMAKE_CURRENT_SOURCE_DIR}/SDKs/aax_sdk")
    endif()
    if(EXISTS "${PROVERB_AAX_DIR}")
        list(APPEND PROVERB_FORMATS AAX)
        message(STATUS "[ProVerb] AAX format enabled with SDK at: ${PROVERB_AAX_DIR}")
    endif()
endif()

# macOS AU Format
if(APPLE)
    list(APPEND PROVERB_FORMATS AU)
endif()

# Optional Legacy VST2 Support
if(DEFINED VST2_SDK_PATH OR DEFINED ENV{VST2_SDK_PATH} OR EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/SDKs/vst2_sdk/pluginterfaces/vst2.x/aeffect.h")
    if(DEFINED ENV{VST2_SDK_PATH})
        set(PROVERB_VST2_DIR "$ENV{VST2_SDK_PATH}")
    elseif(DEFINED VST2_SDK_PATH)
        set(PROVERB_VST2_DIR "${VST2_SDK_PATH}")
    else()
        set(PROVERB_VST2_DIR "${CMAKE_CURRENT_SOURCE_DIR}/SDKs/vst2_sdk")
    endif()
    if(EXISTS "${PROVERB_VST2_DIR}")
        juce_set_vst2_sdk_path("${PROVERB_VST2_DIR}")
        list(APPEND PROVERB_FORMATS VST)
        message(STATUS "[ProVerb] VST2 format enabled with SDK at: ${PROVERB_VST2_DIR}")
    endif()
endif()

# ==============================================================================
# 2. Shared ONNX Runtime CPU Offline Integration
# ==============================================================================
set(ORT_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/ThirdParty/onnxruntime")
set(ORT_HEADER "${ORT_ROOT}/include/onnxruntime_cxx_api.h")

add_library(ProVerb_ONNX_Runtime INTERFACE)

if(WIN32)
    if(CMAKE_SIZEOF_VOID_P EQUAL 8)
        set(ORT_LIB "${ORT_ROOT}/lib/onnxruntime.lib")
        set(ORT_DLL "${ORT_ROOT}/lib/onnxruntime.dll")
        set(ORT_SHARED_DLL "${ORT_ROOT}/lib/onnxruntime_providers_shared.dll")

        if(EXISTS "${ORT_HEADER}" AND EXISTS "${ORT_LIB}" AND EXISTS "${ORT_DLL}" AND EXISTS "${ORT_SHARED_DLL}")
            message(STATUS "[ProVerb] Complete ONNX Runtime C++ SDK validated for x64 at: ${ORT_ROOT}")
            set(PROVERB_ONNX_AVAILABLE TRUE)
            target_compile_definitions(ProVerb_ONNX_Runtime INTERFACE PROVERB_ENABLE_ONNX=1)
            target_include_directories(ProVerb_ONNX_Runtime INTERFACE "${ORT_ROOT}/include")
            target_link_libraries(ProVerb_ONNX_Runtime INTERFACE "${ORT_LIB}")
            if(MSVC)
                target_link_libraries(ProVerb_ONNX_Runtime INTERFACE delayimp.lib)
                target_link_options(ProVerb_ONNX_Runtime INTERFACE "/DELAYLOAD:onnxruntime.dll")
            endif()

            # Pre-copy runtime DLLs into generator config directories so juce_vst3_helper can resolve them during build
            file(MAKE_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/Release")
            file(MAKE_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/Debug")
            file(COPY "${ORT_DLL}" "${ORT_SHARED_DLL}" DESTINATION "${CMAKE_CURRENT_BINARY_DIR}/Release")
            file(COPY "${ORT_DLL}" "${ORT_SHARED_DLL}" DESTINATION "${CMAKE_CURRENT_BINARY_DIR}/Debug")
        else()
            message(WARNING "[ProVerb] Incomplete Windows ONNX Runtime SDK at ${ORT_ROOT}. Offline AI inference will be disabled.")
            set(PROVERB_ONNX_AVAILABLE FALSE)
            target_compile_definitions(ProVerb_ONNX_Runtime INTERFACE PROVERB_ENABLE_ONNX=0)
        endif()
    else()
        message(STATUS "[ProVerb] Windows 32-bit (x86) target: Using built-in high-precision heuristic smart analyzer")
        set(PROVERB_ONNX_AVAILABLE FALSE)
        target_compile_definitions(ProVerb_ONNX_Runtime INTERFACE PROVERB_ENABLE_ONNX=0)
    endif()
elseif(APPLE)
    find_library(ORT_LIB onnxruntime PATHS "${ORT_ROOT}/lib" /usr/local/lib /opt/homebrew/lib)
    if(EXISTS "${ORT_HEADER}" AND ORT_LIB)
        message(STATUS "[ProVerb] ONNX Runtime C++ SDK validated for macOS")
        set(PROVERB_ONNX_AVAILABLE TRUE)
        target_compile_definitions(ProVerb_ONNX_Runtime INTERFACE PROVERB_ENABLE_ONNX=1)
        target_include_directories(ProVerb_ONNX_Runtime INTERFACE "${ORT_ROOT}/include")
        target_link_libraries(ProVerb_ONNX_Runtime INTERFACE "${ORT_LIB}")
    else()
        message(STATUS "[ProVerb] macOS ONNX Runtime not present. Built-in heuristic analyzer active.")
        set(PROVERB_ONNX_AVAILABLE FALSE)
        target_compile_definitions(ProVerb_ONNX_Runtime INTERFACE PROVERB_ENABLE_ONNX=0)
    endif()
else()
    set(PROVERB_ONNX_AVAILABLE FALSE)
    target_compile_definitions(ProVerb_ONNX_Runtime INTERFACE PROVERB_ENABLE_ONNX=0)
endif()

# Reusable deployment function for target output directories
function(proverb_deploy_onnx_runtime target_name)
    if(PROVERB_ONNX_AVAILABLE AND WIN32)
        add_custom_command(TARGET ${target_name} POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "${ORT_DLL}"
                "$<TARGET_FILE_DIR:${target_name}>"
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "${ORT_SHARED_DLL}"
                "$<TARGET_FILE_DIR:${target_name}>"
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "${CMAKE_CURRENT_SOURCE_DIR}/Assets/ProVerb_AI.onnx"
                "$<TARGET_FILE_DIR:${target_name}>/ProVerb_AI.onnx"
            COMMENT "[ProVerb] Deploying ONNX Runtime DLLs & ProVerb_AI.onnx to $<TARGET_FILE_DIR:${target_name}>"
        )
    endif()
endfunction()

# ==============================================================================
# 3. ProVerb JUCE Plugin Target
# ==============================================================================
juce_add_plugin(ProVerb
    COMPANY_NAME "Producer Toy"
    BUNDLE_ID com.producertoy.proverb
    PLUGIN_MANUFACTURER_CODE PrTy
    PLUGIN_CODE PrVb
    PRODUCT_NAME "ProVerb"
    DESCRIPTION "Advanced Space Engine & Algorithmic Reverb by Producer Toy"
    AAX_CATEGORY AAX_ePlugInCategory_Reverb
    AU_MAIN_TYPE kAudioUnitType_Effect
    VST3_CATEGORIES "Fx" "Reverb" "Spatial"
    FORMATS ${PROVERB_FORMATS}
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    COPY_PLUGIN_AFTER_BUILD FALSE
)

target_sources(ProVerb
    PRIVATE
        Source/PluginProcessor.cpp
        Source/PluginProcessor.h
        Source/PluginEditor.cpp
        Source/PluginEditor.h
        Source/DSP/FDNReverb.h
        Source/DSP/DuckingEngine.h
        Source/DSP/ShimmerEngine.h
        Source/DSP/ToneEQ.h
        Source/DSP/AudioAnalyzerEngine.h
        Source/DSP/Presets.h
        Source/Parameters/ParameterDef.h
        Source/UI/ProVerbLookAndFeel.h
        Source/UI/SpaceVisualizer.h
        Source/UI/ModernReverbEQ.h
        Source/UI/SmartAutoLearnButton.h
        Source/AI/LocalAIModel.h
        Source/AI/LocalAIModel.cpp
        Source/AI/AudioFeatureExtractor.h
        Source/AI/ReverbDecisionEngine.h
        Source/AI/AIInferenceWorker.h
        Source/AI/AIInferenceWorker.cpp
        Source/AI/SourceDetector.h
        Source/AI/SourceDetectorWeights.h
        Source/Licensing/Ed25519Verifier.h
        Source/Licensing/LicenseClient.h
        Source/Licensing/LicenseClient.cpp
        Source/UI/LicensePanel.h
        Source/UI/LicensePanel.cpp
        Source/UI/ReverbAssistantPanel.h
        Source/UI/ReverbAssistantPanel.cpp
)

target_compile_definitions(ProVerb
    PUBLIC
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=0
        JUCE_VST3_CAN_REPLACE_VST2=0
)

target_link_libraries(ProVerb
    PRIVATE
        ProVerbResources
        ProVerb_ONNX_Runtime
        juce::juce_audio_utils
        juce::juce_audio_processors
        juce::juce_audio_formats
        juce::juce_dsp
        juce::juce_gui_basics
        juce::juce_gui_extra
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags
)

# Deploy ONNX DLLs to plugin bundle locations
if(TARGET ProVerb_VST3)
    if(MSVC AND PROVERB_ONNX_AVAILABLE)
        target_link_options(ProVerb_VST3 PRIVATE "/DELAYLOAD:onnxruntime.dll")
        target_link_libraries(ProVerb_VST3 PRIVATE delayimp.lib)
    endif()
    proverb_deploy_onnx_runtime(ProVerb_VST3)
endif()

if(TARGET ProVerb_AU)
    proverb_deploy_onnx_runtime(ProVerb_AU)
endif()

if(TARGET ProVerb_Standalone)
    if(MSVC AND PROVERB_ONNX_AVAILABLE)
        target_link_options(ProVerb_Standalone PRIVATE "/DELAYLOAD:onnxruntime.dll")
        target_link_libraries(ProVerb_Standalone PRIVATE delayimp.lib)
    endif()
    proverb_deploy_onnx_runtime(ProVerb_Standalone)
endif()

# ==============================================================================
# 4. Feature Parity & Inference C++ Test Executable
# ==============================================================================
add_executable(ProVerb_FeatureParityTest
    Tools/cpp_feature_extractor_test.cpp
    Source/AI/LocalAIModel.cpp
    Source/AI/AIInferenceWorker.cpp
)

target_include_directories(ProVerb_FeatureParityTest PRIVATE Source)

target_link_libraries(ProVerb_FeatureParityTest PRIVATE
    ProVerb_ONNX_Runtime
    juce::juce_core
    juce::juce_audio_basics
    juce::juce_dsp
    juce::juce_audio_formats
    juce::juce_recommended_config_flags
    juce::juce_recommended_warning_flags
)

proverb_deploy_onnx_runtime(ProVerb_FeatureParityTest)

# Deploy ProVerb_AI.onnx to test executable directory if already present in Assets
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/Assets/ProVerb_AI.onnx")
    add_custom_command(TARGET ProVerb_FeatureParityTest POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${CMAKE_CURRENT_SOURCE_DIR}/Assets/ProVerb_AI.onnx"
            "$<TARGET_FILE_DIR:ProVerb_FeatureParityTest>/ProVerb_AI.onnx"
        COMMENT "[ProVerb] Deploying Assets/ProVerb_AI.onnx to test directory"
    )
endif()

# ==============================================================================
# 5. CTest Registration (Standard & Strict Modes)
# ==============================================================================
enable_testing()

add_test(
    NAME ProVerb_FeatureParityTest_Standard
    COMMAND ProVerb_FeatureParityTest
    WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
)

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/Tools/test_vst3_instantiation.cpp")
    add_executable(ProVerb_HostTest Tools/test_vst3_instantiation.cpp)
    target_include_directories(ProVerb_HostTest PRIVATE "${juce_SOURCE_DIR}/modules/juce_audio_processors/format_types/VST3_SDK")
endif()

add_executable(ProVerb_RT60Test Tools/measure_rt60.cpp)
target_include_directories(ProVerb_RT60Test PRIVATE Source)
target_link_libraries(ProVerb_RT60Test PRIVATE
    juce::juce_core
    juce::juce_audio_basics
    juce::juce_dsp
)

add_executable(ProVerb_OctaveAudit Tools/measure_octave_rt60.cpp)
target_include_directories(ProVerb_OctaveAudit PRIVATE Source)
target_link_libraries(ProVerb_OctaveAudit PRIVATE
    juce::juce_core
    juce::juce_audio_basics
    juce::juce_dsp
)

add_executable(ProVerb_LicensingTest
    Tools/test_licensing.cpp
    Source/Licensing/LicenseClient.cpp
)
target_include_directories(ProVerb_LicensingTest PRIVATE Source)
target_link_libraries(ProVerb_LicensingTest PRIVATE
    juce::juce_core
    juce::juce_events
    juce::juce_gui_basics
    juce::juce_recommended_config_flags
    juce::juce_recommended_warning_flags
)

add_executable(ProVerb_DSPSuiteTest Tools/test_dsp_suite.cpp)
target_include_directories(ProVerb_DSPSuiteTest PRIVATE Source)
target_link_libraries(ProVerb_DSPSuiteTest PRIVATE
    juce::juce_core
    juce::juce_audio_basics
    juce::juce_dsp
    juce::juce_graphics
    juce::juce_recommended_config_flags
    juce::juce_recommended_warning_flags
)

add_test(
    NAME ProVerb_DSPSuiteTest
    COMMAND ProVerb_DSPSuiteTest
    WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
)

# Automated Cryptographic Security, Threat Model & Anti-Tamper Test Suite
add_executable(ProVerb_SecurityAuditTest
    Tools/test_security_audit.cpp
    Source/Licensing/LicenseClient.cpp
)
target_include_directories(ProVerb_SecurityAuditTest PRIVATE Source)
target_link_libraries(ProVerb_SecurityAuditTest PRIVATE
    juce::juce_core
    juce::juce_events
    juce::juce_gui_basics
    juce::juce_audio_basics
    juce::juce_dsp
    juce::juce_recommended_config_flags
    juce::juce_recommended_warning_flags
)

add_test(
    NAME ProVerb_SecurityAuditTest
    COMMAND ProVerb_SecurityAuditTest
    WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
)



