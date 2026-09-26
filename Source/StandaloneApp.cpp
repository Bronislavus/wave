#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_audio_utils/juce_audio_utils.h>
#include <juce_gui_extra/juce_gui_extra.h>

#include "PluginProcessor.h"

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter();

#include <juce_audio_plugin_client/Standalone/juce_StandaloneFilterWindow.h>

#if JucePlugin_Build_Standalone
namespace
{
class WaveStandaloneApplication final : public juce::JUCEApplication
{
public:
    WaveStandaloneApplication()
    {
        juce::PropertiesFile::Options options;
        options.applicationName = juce::CharPointer_UTF8(JucePlugin_Name);
        options.filenameSuffix = ".settings";
        options.osxLibrarySubFolder = "Application Support";
        properties.setStorageParameters(options);
    }

    const juce::String getApplicationName() override
    {
        return juce::CharPointer_UTF8(JucePlugin_Name);
    }
    const juce::String getApplicationVersion() override { return JucePlugin_VersionString; }
    bool moreThanOneInstanceAllowed() override { return false; }
    void anotherInstanceStarted(const juce::String&) override {}

    void initialise(const juce::String&) override
    {
        auto holder = std::make_unique<juce::StandalonePluginHolder>(
            properties.getUserSettings(), false, juce::String{}, nullptr,
            juce::Array<juce::StandalonePluginHolder::PluginInOuts>{}, true);
        window = std::make_unique<juce::StandaloneFilterWindow>(
            getApplicationName(),
            juce::LookAndFeel::getDefaultLookAndFeel().findColour(
                juce::ResizableWindow::backgroundColourId),
            std::move(holder));
        // StandalonePluginHolder has already restored the last saved plug-in
        // state here. Keep that state intact so the mounted SET, patch list,
        // selected Performance, and current sound edits survive relaunches.
        // DAW plug-in instances continue to use their project-owned state.
        if (auto* processor = dynamic_cast<WaveEmulationAudioProcessor*>(
                window->getAudioProcessor()))
        {
            // A controller connected to the standalone replaces the Wave's own
            // keybed, so factory Keyboard+MIDI layers play regardless of their
            // separate external MIDI receive channels.
            processor->setMidiInputActsAsLocalKeyboard(true);
        }
        window->setVisible(true);
        if (auto* processor = window->getAudioProcessor())
            if (auto* editor = processor->getActiveEditor())
                juce::MessageManager::callAsync(
                    [safe = juce::Component::SafePointer(editor)] {
                        if (safe != nullptr)
                            safe->grabKeyboardFocus();
                    });
    }

    void shutdown() override
    {
        if (window != nullptr && window->pluginHolder != nullptr)
            window->pluginHolder->savePluginState();
        window.reset();
        properties.saveIfNeeded();
    }

    void systemRequestedQuit() override
    {
        if (window != nullptr && window->pluginHolder != nullptr)
            window->pluginHolder->savePluginState();
        quit();
    }

private:
    juce::ApplicationProperties properties;
    std::unique_ptr<juce::StandaloneFilterWindow> window;
};
} // namespace

JUCE_CREATE_APPLICATION_DEFINE(WaveStandaloneApplication)
#endif
