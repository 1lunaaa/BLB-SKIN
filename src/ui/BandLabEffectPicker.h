#pragma once

#include <functional>
#include <vector>

#include <juce_gui_basics/juce_gui_basics.h>

#include "engine/AudioEngine.h"

namespace lynx
{
// BLB: selector de efectos estilo BandLab. Categorias a la izquierda, buscador arriba y la lista de efectos con una
// descripcion corta. Un clic en un efecto lo agrega (onPick) y cierra el panel. Los plugins VST externos se eligen
// aparte, desde la tarjeta "VST" del drawer (BandLabFxDrawer::showVstPicker).
class BandLabEffectPicker final : public juce::Component,
                                  private juce::ListBoxModel,
                                  private juce::TextEditor::Listener
{
public:
    BandLabEffectPicker (AudioEngine& engine, std::function<void (EffectSlot)> onPick);
    ~BandLabEffectPicker() override;

    // Abre el panel apuntando a `screenArea` (coordenadas de pantalla). `onPick` recibe el efecto ya creado.
    static void show (AudioEngine& engine, juce::Rectangle<int> screenArea, std::function<void (EffectSlot)> onPick);

    void paint (juce::Graphics& g) override;
    void resized() override;
    void mouseMove (const juce::MouseEvent& e) override;
    void mouseDown (const juce::MouseEvent& e) override;
    void mouseExit (const juce::MouseEvent& e) override;
    bool keyPressed (const juce::KeyPress& key) override;

private:
    struct Item
    {
        EffectKind kind {};
        juce::String name, description, lower, initials;
        juce::StringArray cats; // la primera es la principal
        juce::Colour tint;
    };

    // ListBoxModel
    int getNumRows() override;
    void paintListBoxItem (int row, juce::Graphics& g, int width, int height, bool selected) override;
    void listBoxItemClicked (int row, const juce::MouseEvent& e) override;

    // TextEditor::Listener
    void textEditorTextChanged (juce::TextEditor&) override;
    void textEditorReturnKeyPressed (juce::TextEditor&) override;
    void textEditorEscapeKeyPressed (juce::TextEditor&) override;

    void buildItems();
    void refilter();
    void pickRow (int row);
    void dismiss();
    juce::Rectangle<int> categoryRect (int index) const;
    int categoryAt (juce::Point<int> p) const;

    AudioEngine& engine;
    std::function<void (EffectSlot)> onPick;

    std::vector<Item> items;
    std::vector<int> shown; // indices into `items`
    int selectedCat = 0;
    int hoverCat = -1;
    int hoverRow = -1;

    juce::TextEditor searchBox;
    juce::ListBox list;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (BandLabEffectPicker)
};
} // namespace lynx
