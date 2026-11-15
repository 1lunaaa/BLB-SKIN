#pragma once

#include <functional>
#include <vector>

#include <juce_gui_basics/juce_gui_basics.h>

#include "engine/AudioEngine.h"
#include "ui/I18n.h"
#include "ui/Theme.h"

namespace lynx
{
class BandLabFxDrawer final : public juce::Component,
                              private juce::ChangeListener
{
public:
    explicit BandLabFxDrawer (AudioEngine& engine);
    ~BandLabFxDrawer() override;

    std::function<void()> onClose;
    std::function<void (int track, int slot)> onOpenSlotEditor;

    void setTrack (int track);
    int getTrack() const noexcept { return track; }

    void paint (juce::Graphics& g) override;
    void resized() override;
    void mouseDown (const juce::MouseEvent& e) override;
    void mouseDrag (const juce::MouseEvent& e) override;
    void mouseDoubleClick (const juce::MouseEvent& e) override;
    void mouseMove (const juce::MouseEvent& e) override;
    void mouseExit (const juce::MouseEvent&) override;

private:
    void changeListenerCallback (juce::ChangeBroadcaster*) override { repaint(); }
    void showAddEffectMenu();
    void showSlotOptionsMenu (int slotIndex);
    int slotCardAt (juce::Point<int> p) const;
    juce::Rectangle<int> addCardBounds() const;
    juce::Rectangle<int> closeBtnBounds() const { return { 10, 8, 22, 22 }; }

    AudioEngine& engine;
    int track = 0;
    int hoverSlot = -1;
    bool hoverAdd = false;
    bool hoverClose = false;

    enum class DragKind { none, mix, volume, pan };
    DragKind dragKind = DragKind::none;
    int dragSlot = -1;
    float dragStartVal = 0.0f;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (BandLabFxDrawer)
};
} // namespace lynx
