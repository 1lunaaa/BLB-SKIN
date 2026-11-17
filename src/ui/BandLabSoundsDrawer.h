#pragma once

#include <functional>
#include <map>
#include <vector>

#include <juce_gui_basics/juce_gui_basics.h>

#include "engine/AudioEngine.h"
#include "ui/Theme.h"

namespace lynx
{
class BandLabSoundsDrawer final : public juce::Component,
                                  private juce::TextEditor::Listener
{
public:
    explicit BandLabSoundsDrawer (AudioEngine& engine);
    ~BandLabSoundsDrawer() override;

    std::function<void (const juce::File&)> onInsertSample;
    std::function<void()> onClose;

    void refreshPacks();
    void paint (juce::Graphics& g) override;
    void resized() override;
    void mouseDown (const juce::MouseEvent& e) override;
    void mouseDoubleClick (const juce::MouseEvent& e) override;
    void mouseDrag (const juce::MouseEvent& e) override;
    void mouseMove (const juce::MouseEvent& e) override;
    void mouseExit (const juce::MouseEvent&) override;
    void mouseWheelMove (const juce::MouseEvent& e, const juce::MouseWheelDetails& wheel) override;

private:
    struct Item
    {
        juce::File file;
        juce::String title;
        juce::String subtitle;
        int depth = 0;        // 0 = pack principal, 1+ = subcarpeta o archivo desplegado
        bool isFolder = false;
        bool expanded = false;
        juce::uint32 fallbackColour = 0xff00c4b4;
    };

    void textEditorTextChanged (juce::TextEditor&) override { rebuildVisibleRows(); }
    void rebuildVisibleRows();
    void appendFolderChildren (const juce::File& folder, int depth, const juce::String& filter);
    juce::Image coverForFolder (const juce::File& folder);
    int rowAt (int y) const;
    int rowHeight (const Item& item) const { return item.depth == 0 ? 64 : 34; }
    void addFolderDialog();
    void loadHiddenFolders();
    void saveHiddenFolders() const;
    void hideFolder (const juce::File& folder);
    void unhideAllFolders();

    AudioEngine& engine;
    juce::TextEditor searchBox;
    std::unique_ptr<juce::FileChooser> chooser;
    std::vector<juce::File> rootFolders;
    std::set<juce::String> expandedPaths;
    std::set<juce::String> hiddenPaths;
    std::vector<Item> rows;
    std::vector<int> rowTops;
    std::map<juce::String, juce::Image> coverCache;

    int scrollY = 0;
    int hoverRow = -1;
    juce::File playingFile;
    bool dragStarted = false;
    juce::Point<int> downPos;

    static constexpr int topBarHeight = 84;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (BandLabSoundsDrawer)
};
} // namespace lynx
