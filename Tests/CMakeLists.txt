add_executable(WaveCoreTests WaveCoreTests.cpp)
target_link_libraries(WaveCoreTests
    PRIVATE
        WaveCore
        juce::juce_audio_formats
        juce::juce_recommended_config_flags
        juce::juce_recommended_warning_flags)

add_test(NAME WaveCoreTests COMMAND WaveCoreTests)

if(WAVE_HAS_PRIVATE_ROMS)
    add_executable(WaveIntegrationTests WaveIntegrationTests.cpp)
    target_include_directories(WaveIntegrationTests PRIVATE ${CMAKE_SOURCE_DIR}/Source)
    target_link_libraries(WaveIntegrationTests
        PRIVATE
            WaveEmulation
            juce::juce_audio_utils
            juce::juce_recommended_config_flags
            juce::juce_recommended_warning_flags)
    add_test(NAME WaveIntegrationTests COMMAND WaveIntegrationTests)

    add_executable(WaveEditorTests WaveEditorTests.cpp)
    target_include_directories(WaveEditorTests PRIVATE ${CMAKE_SOURCE_DIR}/Source)
    target_link_libraries(WaveEditorTests
        PRIVATE
            WaveEmulation
            juce::juce_audio_utils
            juce::juce_recommended_config_flags
            juce::juce_recommended_warning_flags)
    add_test(NAME WaveEditorTests COMMAND WaveEditorTests)

    add_executable(WavePerformanceBenchmark WavePerformanceBenchmark.cpp)
    target_include_directories(WavePerformanceBenchmark PRIVATE ${CMAKE_SOURCE_DIR}/Source)
    target_link_libraries(WavePerformanceBenchmark
        PRIVATE
            WaveEmulation
            juce::juce_audio_utils
            juce::juce_recommended_config_flags)
endif()

if(WAVE_HAS_PRIVATE_ROMS)
    set_tests_properties(WaveCoreTests PROPERTIES ENVIRONMENT
        "WAVE_FIRMWARE_DIR=${CMAKE_SOURCE_DIR}/Firmware/wave_sys1_700;PPG_WAVETABLE_ROM=${CMAKE_SOURCE_DIR}/Firmware/wave_sys1_700/ppg-wave-2.3-v6-wavetables.rom;WAVE_FACTORY_SET=${CMAKE_SOURCE_DIR}/wave.set")
    set_tests_properties(WaveIntegrationTests PROPERTIES
        WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
        ENVIRONMENT "WAVE_FACTORY_SET=${CMAKE_SOURCE_DIR}/wave.set")
    set_tests_properties(WaveEditorTests PROPERTIES
        WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
        ENVIRONMENT "WAVE_FACTORY_SET=${CMAKE_SOURCE_DIR}/wave.set")
endif()
