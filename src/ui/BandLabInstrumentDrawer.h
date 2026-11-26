#pragma once

#include <functional>
#include <vector>
#include <juce_gui_basics/juce_gui_basics.h>
#include "engine/AudioEngine.h"

namespace lynx
{
class BandLabInstrumentDrawer final : public juce::Component,
                                      private juce::TextEditor::Listener
{
public:
    explicit BandLabInstrumentDrawer (AudioEngine& engine);

    std::function<void (ChannelDef)> onSelectInstrument;
    std::function<void()> onClose;

    void refreshItems();
    void setTargetChannel (int ch) noexcept { targetChannel = ch; }
    int getTargetChannel() const noexcept { return targetChannel; }

    void paint (juce::Graphics& g) override;
    void resized() override;
    void mouseMove (const juce::MouseEvent& e) override;
    void mouseExit (const juce::MouseEvent&) override;
    void mouseDown (const juce::MouseEvent& e) override;
    void mouseWheelMove (const juce::MouseEvent& e, const juce::MouseWheelDetails& wheel) override;

private:
    struct Item
    {
        juce::String name;
        juce::String category;
        juce::String subtitle;
        std::function<ChannelDef()> createDef;
    };

    void textEditorTextChanged (juce::TextEditor&) override;
    void rebuildFiltered();
    void openExternalPluginPicker();

    juce::Rectangle<int> closeBtnBounds() const { return { getWidth() - 38, 14, 24, 24 }; }
    juce::Rectangle<int> leftColBounds() const  { return { 0, 56, 152, getHeight() - 104 }; }
    juce::Rectangle<int> rightColBounds() const { return { 153, 56, getWidth() - 153, getHeight() - 104 }; }
    juce::Rectangle<int> vstBtnBounds() const   { return { 14, getHeight() - 42, getWidth() - 28, 32 }; }

    AudioEngine& engine;
    int targetChannel = -1;

    juce::TextEditor searchBox;
    std::vector<Item> allItems;
    std::vector<juce::String> categories;
    std::vector<int> filteredIndices;

    int selectedCategory = 0;
    int hoverCategory = -1;
    int selectedItem = 0;
    int hoverItem = -1;
    bool hoverClose = false;
    bool hoverVstBtn = false;

    int catScrollY = 0;
    int itemScrollY = 0;
};
} // namespace lynx
