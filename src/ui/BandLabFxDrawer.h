#pragma once

#include <functional>
#include <memory>
#include <vector>

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>

#include "engine/AudioEngine.h"
#include "ui/I18n.h"
#include "ui/Theme.h"

namespace lynx
{
class BandLabFxDrawer final : public juce::Component,
                              private juce::ChangeListener,
                              private juce::Timer
{
public:
    enum class Mode { effects, input };

    explicit BandLabFxDrawer (AudioEngine& engine);
    ~BandLabFxDrawer() override;

    std::function<void()> onClose;
    std::function<void (int track, int slot)> onOpenSlotEditor;

    void setTrack (int track);
    int getTrack() const noexcept { return track; }

    void setMode (Mode newMode);
    Mode getMode() const noexcept { return mode; }

    void ensureAutoPitchOnTrack();

    void paint (juce::Graphics& g) override;
    void resized() override;
    void mouseDown (const juce::MouseEvent& e) override;
    void mouseDrag (const juce::MouseEvent& e) override;
    void mouseDoubleClick (const juce::MouseEvent& e) override;
    void mouseMove (const juce::MouseEvent& e) override;
    void mouseExit (const juce::MouseEvent&) override;

private:
    void changeListenerCallback (juce::ChangeBroadcaster*) override;
    void timerCallback() override;

    void syncEmbeddedEditor();
    void clearEmbeddedEditor();
    int findAutoPitchSlot() const;
    static juce::File locateAutoPitchBundle();

    int effectsStartX() const;
    std::vector<int> visibleEffectSlotIndices() const;

    void showAddEffectMenu();
    void showVstPicker();
    void addEffectSlot (EffectSlot slot);
    void showSlotOptionsMenu (int slotIndex);
    void showInputDeviceMenu();
    void showInputChannelMenu();

    int slotCardAt (juce::Point<int> p) const;
    juce::Rectangle<int> addCardBounds() const;
    juce::Rectangle<int> vstCardBounds() const;
    juce::Rectangle<int> closeBtnBounds() const { return { 10, 8, 22, 22 }; }

    // Controles del panel Entrada (estilo BandLab)
    juce::Rectangle<int> inputDeviceBoxBounds() const { return { 24, 66, 220, 30 }; }
    juce::Rectangle<int> inputChannelBoxBounds() const { return { 24, 122, 220, 30 }; }
    juce::Rectangle<int> monitorBtnBounds() const { return { 132, 174, 112, 28 }; }

    AudioEngine& engine;
    int track = 0;
    Mode mode = Mode::input;
    float liveInputPeak = 0.0f;

    std::shared_ptr<Effect> hostedEffectOwner;
    std::unique_ptr<juce::AudioProcessorEditor> embeddedEditor;
    juce::String autoPitchStatus;

    int hoverSlot = -1;
    bool hoverAdd = false;
    bool hoverVst = false;
    bool hoverClose = false;
    int hoverInputPart = 0; // 1=device, 2=channel, 3=monitor

    enum class DragKind { none, mix, volume, pan };
    DragKind dragKind = DragKind::none;
    int dragSlot = -1;
    float dragStartVal = 0.0f;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (BandLabFxDrawer)
};
} // namespace lynx
