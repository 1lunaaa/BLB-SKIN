#include <algorithm>
#include "ui/BandLabInstrumentDrawer.h"
#include "core/ControlSurface.h"
#include "core/NativeEngines.h"
#include "core/SoundFont.h"
#include "core/VideoPlayer.h"
#include "core/VoltageController.h"
#include "engine/PluginHost.h"
#include "ui/PluginPicker.h"
#include "ui/Theme.h"

namespace lynx
{
BandLabInstrumentDrawer::BandLabInstrumentDrawer (AudioEngine& e)
    : engine (e)
{
    setOpaque (true);
    searchBox.setTextToShowWhenEmpty (juce::String::fromUTF8 ("Search"), theme::colour::textDisabled);
    searchBox.setColour (juce::TextEditor::backgroundColourId, juce::Colour (0xff15171b));
    searchBox.setColour (juce::TextEditor::outlineColourId, juce::Colour (0xff2e323b));
    searchBox.setColour (juce::TextEditor::focusedOutlineColourId, juce::Colour (0xff525866));
    searchBox.setColour (juce::TextEditor::textColourId, theme::colour::textPrimary);
    searchBox.setFont (theme::font (13.0f, false));
    searchBox.setIndents (28, 6);
    searchBox.addListener (this);
    addAndMakeVisible (searchBox);

    refreshItems();
}

void BandLabInstrumentDrawer::refreshItems()
{
    allItems.clear();
    categories.clear();

    auto addItem = [this] (const juce::String& name, const juce::String& cat, const juce::String& sub, std::function<ChannelDef()> fn) {
        allItems.push_back ({ name, cat, sub, std::move (fn) });
        if (std::find (categories.begin(), categories.end(), cat) == categories.end())
            categories.push_back (cat);
    };

    // LYNX SHARE built-ins
    addItem ("Clap", "LYNX SHARE", "Built-in", [] { ChannelDef c; c.name = "Clap"; c.instrument = InstrumentKind::clap; return c; });
    addItem ("Hat", "LYNX SHARE", "Built-in", [] { ChannelDef c; c.name = "Hat"; c.instrument = InstrumentKind::hat; return c; });
    addItem ("Kick", "LYNX SHARE", "Built-in", [] { ChannelDef c; c.name = "Kick"; c.instrument = InstrumentKind::kick; return c; });
    addItem ("LYNX Soundfont", "LYNX SHARE", "SoundFont Player", [] { return sf::makeChannel(); });
    addItem ("Snare", "LYNX SHARE", "Built-in", [] { ChannelDef c; c.name = "Snare"; c.instrument = InstrumentKind::snare; return c; });
    addItem ("Synth", "LYNX SHARE", "Built-in", [] { ChannelDef c; c.name = "Synth"; c.instrument = InstrumentKind::synth; return c; });

    // Controller / MIDI
    addItem ("Dashboard", "Controller", "MIDI Surface", [] {
        ChannelDef c; c.name = "Dashboard"; c.instrument = InstrumentKind::dashboard;
        c.patcher.surface = surface::starterControls(); return c;
    });
    addItem ("Voltage Controller", "Controller", "CV / Modulation", [] {
        ChannelDef c; c.name = "Voltage Controller"; c.instrument = InstrumentKind::voltage;
        c.pluginState = voltage::write (voltage::Settings {}); return c;
    });
    addItem ("Layer", "Misc", "Multi-channel Layer", [] {
        ChannelDef c; c.name = "Layer"; c.instrument = InstrumentKind::layer; return c;
    });
    addItem ("Fruity Video Player", "Visual", "Video Sync", [] {
        ChannelDef c; c.name = "Fruity Video Player"; c.instrument = InstrumentKind::videoplayer;
        c.pluginState = video::write (video::Settings {}); return c;
    });

    // Native FL / LYNX synth & drum engines
    for (const auto& eng : natives::instrumentEngines())
    {
        if (! natives::available (eng, flStudioAccess()))
            continue;
        const auto nm = juce::String (eng.name);
        juce::String cat = "Synth Classic";
        if (nm.containsIgnoreCase ("Drum") || nm.containsIgnoreCase ("BassDrum") || nm.containsIgnoreCase ("Kick"))
            cat = "Drum";
        else if (nm.containsIgnoreCase ("Granulizer") || nm.containsIgnoreCase ("Wave"))
            cat = "Sampler";
        else if (nm.containsIgnoreCase ("Harmor") || nm.containsIgnoreCase ("Morphine") || nm.containsIgnoreCase ("Ogun")
                 || nm.containsIgnoreCase ("Plucked") || nm.containsIgnoreCase ("Sakura") || nm.containsIgnoreCase ("BeepMap"))
            cat = "Synth Special";

        const auto kind = eng.kind;
        addItem (nm, cat, "Instrument", [kind, nm] {
            if (auto c = natives::makeChannel (kind, flStudioAccess()))
                return *c;
            ChannelDef fallback;
            fallback.name = nm.toStdString();
            fallback.instrument = InstrumentKind::synth;
            return fallback;
        });
    }

    // Scanned VST / VST3 / CLAP instruments
    for (const auto& type : engine.getPluginHost().getKnownPlugins().getTypes())
    {
        if (! type.isInstrument)
            continue;
        PluginRef ref;
        ref.id = type.createIdentifierString().toStdString();
        ref.name = type.name.toStdString();
        addItem (type.name, "Plugins (VST/CLAP)", type.manufacturerName + " · " + type.pluginFormatName, [ref] {
            ChannelDef c;
            c.name = ref.name;
            c.instrument = InstrumentKind::plugin;
            c.plugin = ref;
            return c;
        });
    }

    if (selectedCategory >= (int) categories.size())
        selectedCategory = 0;
    rebuildFiltered();
}

void BandLabInstrumentDrawer::rebuildFiltered()
{
    filteredIndices.clear();
    const auto q = searchBox.getText().trim().toLowerCase();
    const auto activeCat = (selectedCategory >= 0 && selectedCategory < (int) categories.size())
                             ? categories[(std::size_t) selectedCategory]
                             : juce::String();

    for (int i = 0; i < (int) allItems.size(); ++i)
    {
        const auto& it = allItems[(std::size_t) i];
        if (q.isEmpty())
        {
            if (activeCat.isNotEmpty() && it.category != activeCat)
                continue;
        }
        else
        {
            if (! it.name.toLowerCase().contains (q) && ! it.category.toLowerCase().contains (q) && ! it.subtitle.toLowerCase().contains (q))
                continue;
        }
        filteredIndices.push_back (i);
    }
    itemScrollY = 0;
    selectedItem = filteredIndices.empty() ? -1 : 0;
    repaint();
}

void BandLabInstrumentDrawer::textEditorTextChanged (juce::TextEditor&)
{
    rebuildFiltered();
}

void BandLabInstrumentDrawer::openExternalPluginPicker()
{
    juce::Component::SafePointer<BandLabInstrumentDrawer> safe (this);
    showPluginPicker (engine, true,
        [safe] (PluginRef ref) {
            if (safe == nullptr || ! safe->onSelectInstrument)
                return;
            ChannelDef c;
            c.name = ref.name;
            c.instrument = InstrumentKind::plugin;
            c.plugin = ref;
            safe->onSelectInstrument (std::move (c));
        },
        [safe] (ChannelDef c) {
            if (safe != nullptr && safe->onSelectInstrument)
                safe->onSelectInstrument (std::move (c));
        });
}

void BandLabInstrumentDrawer::resized()
{
    searchBox.setBounds (214, 12, juce::jmax (140, getWidth() - 264), 32);
}

void BandLabInstrumentDrawer::paint (juce::Graphics& g)
{
    using namespace theme;
    g.fillAll (juce::Colour (0xff121316));

    // Left border
    g.setColour (juce::Colour (0xff252830));
    g.drawVerticalLine (0, 0.0f, (float) getHeight());

    // Header title: "Browse Instruments"
    g.setColour (colour::textPrimary);
    g.setFont (font (17.5f, true));
    g.drawText (juce::String::fromUTF8 ("Browse Instruments"), 18, 12, 190, 32, juce::Justification::centredLeft, false);

    // Search magnifier icon inside searchBox left padding
    {
        const auto sb = searchBox.getBounds().toFloat();
        g.setColour (colour::textSecondary);
        g.drawEllipse (sb.getX() + 10.0f, sb.getY() + 10.0f, 9.0f, 9.0f, 1.4f);
        g.drawLine (sb.getX() + 17.5f, sb.getY() + 17.5f, sb.getX() + 21.0f, sb.getY() + 21.0f, 1.4f);
    }

    // Close X button
    {
        const auto cb = closeBtnBounds().toFloat();
        if (hoverClose)
        {
            g.setColour (colour::bg3);
            g.fillRoundedRectangle (cb, 6.0f);
        }
        g.setColour (hoverClose ? colour::textPrimary : colour::textSecondary);
        const auto c = cb.getCentre();
        g.drawLine (c.x - 4.5f, c.y - 4.5f, c.x + 4.5f, c.y + 4.5f, 1.5f);
        g.drawLine (c.x + 4.5f, c.y - 4.5f, c.x - 4.5f, c.y + 4.5f, 1.5f);
    }

    // Header bottom divider
    g.setColour (juce::Colour (0xff23262e));
    g.drawHorizontalLine (55, 0.0f, (float) getWidth());

    // Left Column: Categories
    const auto leftR = leftColBounds();
    {
        juce::Graphics::ScopedSaveState ss (g);
        g.reduceClipRegion (leftR);
        const int rowH = 38;
        for (int i = 0; i < (int) categories.size(); ++i)
        {
            const int y = leftR.getY() + 8 + i * rowH - catScrollY;
            if (y + rowH < leftR.getY() || y > leftR.getBottom())
                continue;
            const auto row = juce::Rectangle<int> (leftR.getX() + 8, y, leftR.getWidth() - 16, rowH - 4);
            const bool isSel = (selectedCategory == i && searchBox.getText().trim().isEmpty());
            const bool isHov = (hoverCategory == i);
            if (isSel || isHov)
            {
                g.setColour (isSel ? juce::Colour (0xff23262f) : juce::Colour (0xff1a1c22));
                g.fillRoundedRectangle (row.toFloat(), 7.0f);
            }
            g.setColour (isSel ? colour::textPrimary : colour::textSecondary);
            g.setFont (font (13.0f, isSel));
            g.drawText (categories[(std::size_t) i], row.withTrimmedLeft (10), juce::Justification::centredLeft, true);
        }
    }

    // Vertical divider between categories and instruments list
    g.setColour (juce::Colour (0xff23262e));
    g.drawVerticalLine (leftR.getRight(), 55.0f, (float) leftR.getBottom());

    // Right Column: Category heading + Instrument cards
    const auto rightR = rightColBounds();
    {
        juce::Graphics::ScopedSaveState ss (g);
        g.reduceClipRegion (rightR);

        const juce::String sectionTitle = searchBox.getText().trim().isNotEmpty()
                                            ? juce::String::fromUTF8 ("Resultados")
                                            : ((selectedCategory >= 0 && selectedCategory < (int) categories.size())
                                                ? categories[(std::size_t) selectedCategory]
                                                : juce::String::fromUTF8 ("Instruments"));

        int y = rightR.getY() + 12 - itemScrollY;
        g.setColour (colour::textPrimary);
        g.setFont (font (15.5f, true));
        g.drawText (sectionTitle, rightR.getX() + 16, y, rightR.getWidth() - 32, 24, juce::Justification::centredLeft, false);
        y += 32;

        const int cardH = 48;
        const int cardGap = 6;
        for (int idx = 0; idx < (int) filteredIndices.size(); ++idx)
        {
            const auto& item = allItems[(std::size_t) filteredIndices[(std::size_t) idx]];
            const auto card = juce::Rectangle<int> (rightR.getX() + 14, y, rightR.getWidth() - 28, cardH);
            y += cardH + cardGap;

            if (card.getBottom() < rightR.getY() || card.getY() > rightR.getBottom())
                continue;

            const bool isSel = (selectedItem == idx);
            const bool isHov = (hoverItem == idx);

            if (isSel || isHov)
            {
                g.setColour (isSel ? juce::Colour (0xff242730) : juce::Colour (0xff1a1c22));
                g.fillRoundedRectangle (card.toFloat(), 8.0f);
            }

            // Play triangle icon on left
            const float px = (float) card.getX() + 16.0f;
            const float py = (float) card.getCentreY();
            juce::Path tri;
            tri.addTriangle (px, py - 5.5f, px, py + 5.5f, px + 9.0f, py);
            g.setColour (colour::textPrimary);
            g.fillPath (tri);

            // Instrument Name
            g.setColour (colour::textPrimary);
            g.setFont (font (14.0f, true));
            g.drawText (item.name, card.withTrimmedLeft (36).withTrimmedRight (36), juce::Justification::centredLeft, true);

            // Blue checkmark on selected item
            if (isSel)
            {
                const float cx = (float) card.getRight() - 22.0f;
                const float cy = (float) card.getCentreY();
                g.setColour (juce::Colour (0xff3b82f6));
                g.drawLine (cx - 4.5f, cy, cx - 1.0f, cy + 4.0f, 1.8f);
                g.drawLine (cx - 1.0f, cy + 4.0f, cx + 5.5f, cy - 4.0f, 1.8f);
            }
        }
    }

    // Footer button: "Plugin (VST/VST3/CLAP) >"
    {
        const auto vb = vstBtnBounds().toFloat();
        g.setColour (hoverVstBtn ? juce::Colour (0xff262932) : juce::Colour (0xff1a1c21));
        g.fillRoundedRectangle (vb, 8.0f);
        g.setColour (juce::Colour (0xff2c303a));
        g.drawRoundedRectangle (vb.reduced (0.5f), 8.0f, 1.0f);
        g.setColour (colour::textPrimary);
        g.setFont (font (12.5f, true));
        g.drawText (juce::String::fromUTF8 ("Plugin (VST/VST3/CLAP)  >"), vb.toNearestInt(), juce::Justification::centred, false);
    }
}

void BandLabInstrumentDrawer::mouseMove (const juce::MouseEvent& e)
{
    const auto pt = e.getPosition();
    const bool newClose = closeBtnBounds().contains (pt);
    const bool newVst = vstBtnBounds().contains (pt);
    int newCat = -1;
    int newItem = -1;

    const auto leftR = leftColBounds();
    if (leftR.contains (pt))
    {
        const int rowH = 38;
        const int idx = (pt.y - (leftR.getY() + 8) + catScrollY) / rowH;
        if (idx >= 0 && idx < (int) categories.size())
            newCat = idx;
    }

    const auto rightR = rightColBounds();
    if (rightR.contains (pt))
    {
        const int listStartY = rightR.getY() + 44 - itemScrollY;
        const int stride = 54;
        const int idx = (pt.y - listStartY) / stride;
        if (idx >= 0 && idx < (int) filteredIndices.size())
            newItem = idx;
    }

    if (newClose != hoverClose || newVst != hoverVstBtn || newCat != hoverCategory || newItem != hoverItem)
    {
        hoverClose = newClose;
        hoverVstBtn = newVst;
        hoverCategory = newCat;
        hoverItem = newItem;
        const bool hand = hoverClose || hoverVstBtn || hoverCategory >= 0 || hoverItem >= 0;
        setMouseCursor (hand ? juce::MouseCursor::PointingHandCursor : juce::MouseCursor::NormalCursor);
        repaint();
    }
}

void BandLabInstrumentDrawer::mouseExit (const juce::MouseEvent&)
{
    hoverClose = hoverVstBtn = false;
    hoverCategory = hoverItem = -1;
    setMouseCursor (juce::MouseCursor::NormalCursor);
    repaint();
}

void BandLabInstrumentDrawer::mouseDown (const juce::MouseEvent& e)
{
    const auto pt = e.getPosition();
    if (closeBtnBounds().contains (pt))
    {
        if (onClose) onClose();
        return;
    }
    if (vstBtnBounds().contains (pt))
    {
        openExternalPluginPicker();
        return;
    }

    const auto leftR = leftColBounds();
    if (leftR.contains (pt))
    {
        const int rowH = 38;
        const int idx = (pt.y - (leftR.getY() + 8) + catScrollY) / rowH;
        if (idx >= 0 && idx < (int) categories.size())
        {
            selectedCategory = idx;
            searchBox.clear();
            rebuildFiltered();
        }
        return;
    }

    const auto rightR = rightColBounds();
    if (rightR.contains (pt))
    {
        const int listStartY = rightR.getY() + 44 - itemScrollY;
        const int stride = 54;
        const int idx = (pt.y - listStartY) / stride;
        if (idx >= 0 && idx < (int) filteredIndices.size())
        {
            selectedItem = idx;
            repaint();
            const auto& item = allItems[(std::size_t) filteredIndices[(std::size_t) idx]];
            if (onSelectInstrument && item.createDef)
                onSelectInstrument (item.createDef());
        }
    }
}

void BandLabInstrumentDrawer::mouseWheelMove (const juce::MouseEvent& e, const juce::MouseWheelDetails& wheel)
{
    const int delta = juce::roundToInt (-wheel.deltaY * 45.0f);
    if (leftColBounds().contains (e.getPosition()))
    {
        const int maxScroll = juce::jmax (0, (int) categories.size() * 38 - leftColBounds().getHeight() + 16);
        catScrollY = juce::jlimit (0, maxScroll, catScrollY + delta);
        repaint();
    }
    else if (rightColBounds().contains (e.getPosition()))
    {
        const int maxScroll = juce::jmax (0, (int) filteredIndices.size() * 54 + 44 - rightColBounds().getHeight() + 16);
        itemScrollY = juce::jlimit (0, maxScroll, itemScrollY + delta);
        repaint();
    }
}
} // namespace lynx
