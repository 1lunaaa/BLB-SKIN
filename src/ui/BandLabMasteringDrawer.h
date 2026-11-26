#pragma once

#include <array>
#include <functional>
#include <juce_gui_basics/juce_gui_basics.h>

namespace lynx
{
class BandLabMasteringDrawer final : public juce::Component
{
public:
    BandLabMasteringDrawer();

    std::function<void()> onClose;

    void paint (juce::Graphics& g) override;
    void mouseMove (const juce::MouseEvent& e) override;
    void mouseExit (const juce::MouseEvent&) override;
    void mouseDown (const juce::MouseEvent& e) override;
    void mouseDrag (const juce::MouseEvent& e) override;
    void mouseUp (const juce::MouseEvent&) override;

private:
    enum class DragTarget { none, intensity, knobLow, knobMid, knobHigh };

    juce::Rectangle<int> speakerBtnBounds() const { return { getWidth() - 68, 14, 26, 26 }; }
    juce::Rectangle<int> closeBtnBounds() const   { return { getWidth() - 36, 14, 24, 24 }; }
    juce::Rectangle<int> presetCardBounds (int idx) const;
    juce::Rectangle<int> intensityBoxBounds() const;
    juce::Rectangle<int> intensityTrackBounds() const;
    juce::Rectangle<int> eqBoxBounds() const;
    juce::Rectangle<int> eqToggleBounds() const;
    juce::Rectangle<int> eqKnobBounds (int idx) const;
    juce::Rectangle<int> applyBtnBounds() const;

    void updateIntensityFromX (int x);

    int selectedPreset = 0;
    int hoverPreset = -1;
    bool hoverSpeaker = false;
    bool hoverClose = false;
    bool hoverApply = false;
    bool bypassActive = false;

    float intensity = 0.5f;
    bool eqEnabled = true;
    std::array<float, 3> eqValues { 0.5f, 0.5f, 0.5f };

    DragTarget dragTarget = DragTarget::none;
    int dragStartY = 0;
    float dragStartVal = 0.5f;
};
} // namespace lynx
