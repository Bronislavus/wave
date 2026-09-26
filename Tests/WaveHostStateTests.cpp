#include "PluginProcessor.h"

#include <cstdlib>
#include <iostream>
#include <stdexcept>
#include <thread>

int main(int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI initialiseJuce;
    try
    {
        if (argc == 4 && juce::String(argv[1]) == "--check-remembered-firmware")
        {
            auto reopened = std::make_unique<WaveEmulationAudioProcessor>(juce::File(argv[2]));
            if (!reopened->getFirmwareReport().hasBothImages()
                || reopened->getFirmwareDirectory() != juce::File(argv[3]))
                throw std::runtime_error("A fresh process did not reload remembered firmware");
            return 0;
        }
        // Never read or change the developer's real firmware preference.
        juce::TemporaryFile preference(".txt");
        auto first = std::make_unique<WaveEmulationAudioProcessor>(preference.getFile());
        auto second = std::make_unique<WaveEmulationAudioProcessor>(preference.getFile());
        const auto* directory = std::getenv("WAVE_FIRMWARE_DIR");
        for (auto* processor : { first.get(), second.get() })
        {
            if (directory != nullptr
                && !processor->loadFirmware(juce::File(directory)).hasBothImages())
                throw std::runtime_error("Unable to load the supplied test firmware");
            processor->prepareToPlay(48000.0, 128);
        }
        if (directory != nullptr)
        {
            const auto remembered = preference.getFile().loadFileAsString();
            juce::TemporaryFile emptyFolder(".dir");
            emptyFolder.getFile().createDirectory();
            const auto invalidReport = first->loadFirmware(emptyFolder.getFile());
            emptyFolder.getFile().deleteRecursively();
            if (invalidReport.hasBothImages()
                || preference.getFile().loadFileAsString() != remembered
                || !first->getFirmwareReport().hasBothImages())
                throw std::runtime_error("Invalid firmware selection replaced a working preference");
            juce::ChildProcess reopened;
            const juce::StringArray command {
                juce::File::getSpecialLocation(juce::File::currentExecutableFile).getFullPathName(),
                "--check-remembered-firmware", preference.getFile().getFullPathName(),
                juce::File(directory).getFullPathName()
            };
            if (!reopened.start(command) || !reopened.waitForProcessToFinish(30000)
                || reopened.getExitCode() != 0)
                throw std::runtime_error("Firmware preference did not survive process restart");

            // A saved project chooses its own firmware without changing the
            // default for unrelated new instances.
            juce::MemoryBlock savedProject;
            first->getStateInformation(savedProject);
            juce::TemporaryFile unrelatedPreference(".txt");
            const auto missingDefault = juce::File(directory)
                                            .getChildFile("missing-test-default").getFullPathName();
            unrelatedPreference.getFile().replaceWithText(missingDefault);
            auto recalled = std::make_unique<WaveEmulationAudioProcessor>(unrelatedPreference.getFile());
            recalled->setStateInformation(savedProject.getData(), static_cast<int>(savedProject.getSize()));
            if (!recalled->getFirmwareReport().hasBothImages()
                || recalled->getFirmwareDirectory() != juce::File(directory)
                || unrelatedPreference.getFile().loadFileAsString() != missingDefault)
                throw std::runtime_error("Project recall replaced the default firmware preference");
        }
        auto render = [](std::stop_token stop, WaveEmulationAudioProcessor* processor)
        {
            juce::AudioBuffer<float> audio(2, 128);
            juce::MidiBuffer midi;
            while (!stop.stop_requested())
            {
                const juce::ScopedLock lock(processor->getCallbackLock());
                audio.clear();
                midi.clear();
                processor->processBlock(audio, midi);
            }
        };
        std::jthread firstRender(render, first.get());
        std::jthread secondRender(render, second.get());
        for (int recall = 0; recall < 8; ++recall)
        {
            juce::MemoryBlock state;
            {
                const juce::ScopedLock lock(first->getCallbackLock());
                first->getStateInformation(state);
            }
            if (state.isEmpty())
                throw std::runtime_error("Host preset contained no state");
            auto restored = std::make_unique<WaveEmulationAudioProcessor>(preference.getFile());
            restored->setStateInformation(state.getData(), static_cast<int>(state.getSize()));
            if (directory != nullptr && !restored->getFirmwareReport().hasBothImages())
                throw std::runtime_error("Host preset lost the firmware reference");
            if (restored->getCurrentProgram() != first->getCurrentProgram())
                throw std::runtime_error("Host preset lost the selected performance");
            restored->prepareToPlay(48000.0, 128);
            juce::AudioBuffer<float> audio(2, 128);
            juce::MidiBuffer midi;
            restored->processBlock(audio, midi);
        }
        std::cout << "Host state saved and recalled while two instances rendered"
                  << (directory != nullptr ? " with firmware\n" : " without firmware\n");
    }
    catch (const std::exception& error)
    {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
