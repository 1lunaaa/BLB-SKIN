#include "ui/BandLabFxDrawer.h"
#include "ui/ChannelRackComponent.h"

#include "core/ValueMath.h"
#include "engine/PluginHost.h"
#include "ui/Icons.h"
#include "ui/MixerComponent.h"
#include "ui/BandLabEffectPicker.h"
#include "ui/PluginPicker.h"

namespace lynx
{
BandLabFxDrawer::BandLabFxDrawer (AudioEngine& e)
    : engine (e)
{
    drumViewport.setScrollBarsShown (true, true);
    drumViewport.setScrollBarThickness (8);
    addChildComponent (drumViewport);
    engine.addChangeListener (this);
}

BandLabFxDrawer::~BandLabFxDrawer()
{
    stopTimer();
    clearEmbeddedEditor();
    engine.removeChangeListener (this);
}

void BandLabFxDrawer::clearEmbeddedEditor()
{
    if (embeddedEditor != nullptr)
    {
        removeChildComponent (embeddedEditor.get());
        embeddedEditor.reset();
    }
    hostedEffectOwner.reset();
}

juce::File BandLabFxDrawer::locateAutoPitchBundle()
{
    const auto exeDir = juce::File::getSpecialLocation (juce::File::currentExecutableFile).getParentDirectory();
    const juce::StringArray candidates {
        exeDir.getChildFile ("plugins/BLP AutoPitch.vst3").getFullPathName(),
        exeDir.getChildFile ("plugins/AutoPitch.vst3").getFullPathName(),
        juce::File::getCurrentWorkingDirectory().getChildFile ("plugins/BLP AutoPitch.vst3").getFullPathName(),
        juce::File::getCurrentWorkingDirectory().getChildFile ("build/LYNX_artefacts/Debug/plugins/BLP AutoPitch.vst3").getFullPathName(),
        juce::File ("C:/dev/BLB-SKIN/build/LYNX_artefacts/Debug/plugins/BLP AutoPitch.vst3").getFullPathName(),
        juce::File ("C:/dev/BLB-SKIN/plugins/BLP AutoPitch.vst3").getFullPathName()
    };
    for (const auto& path : candidates)
    {
        juce::File f (path);
        if (f.exists())
            return f;
    }
    return {};
}

int BandLabFxDrawer::findAutoPitchSlot() const
{
    const auto& song = engine.getSong();
    if (track < 0 || track >= (int) song.mixer.size())
        return -1;
    const auto& effects = song.mixer[(std::size_t) track].effects;
    for (int i = 0; i < (int) effects.size(); ++i)
    {
        const auto& sl = effects[(std::size_t) i];
        if (sl.kind == EffectKind::plugin)
        {
            const auto nm = juce::String::fromUTF8 (sl.plugin.name.c_str()).toLowerCase();
            const auto id = juce::String::fromUTF8 (sl.plugin.id.c_str()).toLowerCase();
            if (nm.contains ("autopitch") || id.contains ("autopitch"))
                return i;
        }
    }
    return -1;
}

void BandLabFxDrawer::ensureAutoPitchOnTrack()
{
    if (track <= 0)
        return;

    if (findAutoPitchSlot() >= 0)
    {
        syncEmbeddedEditor();
        return;
    }

    const auto bundle = locateAutoPitchBundle();
    if (! bundle.exists())
    {
        autoPitchStatus = tr ("No se encontro plugins/BLP AutoPitch.vst3 junto al programa.");
        repaint();
        return;
    }

    auto& host = engine.getPluginHost();
    juce::PluginDescription matchedDesc;
    bool found = false;

    for (const auto& d : host.getKnownPlugins().getTypes())
    {
        if (d.fileOrIdentifier == bundle.getFullPathName()
            || d.name.containsIgnoreCase ("AutoPitch"))
        {
            matchedDesc = d;
            found = true;
            break;
        }
    }

    if (! found)
    {
        host.addPluginFile (bundle);
        for (const auto& d : host.getKnownPlugins().getTypes())
        {
            if (d.fileOrIdentifier == bundle.getFullPathName()
                || d.name.containsIgnoreCase ("AutoPitch"))
            {
                matchedDesc = d;
                found = true;
                break;
            }
        }
    }

    if (! found)
    {
        autoPitchStatus = tr ("No se pudo leer el modulo VST3: ") + bundle.getFileName();
        repaint();
        return;
    }

    autoPitchStatus.clear();
    engine.editSong ([this, matchedDesc] (Song& s) {
        if (s.mixer.empty())
            s.mixer = Song::defaultMixer (0);
        if (track <= 0 || track >= (int) s.mixer.size())
            return;

        auto& effects = s.mixer[(std::size_t) track].effects;
        for (const auto& existing : effects)
            if (existing.kind == EffectKind::plugin && juce::String (existing.plugin.name).containsIgnoreCase ("AutoPitch"))
                return;

        EffectSlot slot;
        slot.kind = EffectKind::plugin;
        slot.enabled = true;
        slot.mix = 1.0f;
        slot.plugin.id = matchedDesc.createIdentifierString().toStdString();
        slot.plugin.name = matchedDesc.name.toStdString();
        effects.insert (effects.begin(), std::move (slot));
    });

    syncEmbeddedEditor();
}

void BandLabFxDrawer::syncEmbeddedEditor()
{
    if (! isVisible() || mode != Mode::input)
    {
        clearEmbeddedEditor();
        return;
    }

    const int slotIdx = findAutoPitchSlot();
    if (slotIdx < 0)
    {
        clearEmbeddedEditor();
        return;
    }

    auto fx = engine.getEffect (track, slotIdx);
    auto* pe = dynamic_cast<PluginEffect*> (fx.get());
    if (pe == nullptr || pe->hasCrashed())
    {
        clearEmbeddedEditor();
        return;
    }

    if (hostedEffectOwner.get() == fx.get() && embeddedEditor != nullptr)
    {
        resized();
        return;
    }

    clearEmbeddedEditor();
    hostedEffectOwner = fx;

    auto& instance = pe->getInstance();
    juce::AudioProcessorEditor* ed = instance.createEditorIfNeeded();
    if (ed == nullptr)
        ed = new juce::GenericAudioProcessorEditor (instance);

    embeddedEditor.reset (ed);
    addAndMakeVisible (*embeddedEditor);
    resized();
}

void BandLabFxDrawer::changeListenerCallback (juce::ChangeBroadcaster*)
{
    if (isVisible() && mode == Mode::input)
        syncEmbeddedEditor();
    repaint();
}

void BandLabFxDrawer::setTrack (int t)
{
    const int next = juce::jmax (0, t);
    if (track != next)
    {
        track = next;
        clearEmbeddedEditor();
    }
    if (isVisible() && mode == Mode::input)
        ensureAutoPitchOnTrack();
    repaint();
}

void BandLabFxDrawer::setMode (Mode newMode)
{
    mode = newMode;
    if (mode == Mode::input && isVisible())
    {
        startTimerHz (30);
        ensureAutoPitchOnTrack();
    }
    else
    {
        stopTimer();
        clearEmbeddedEditor();
    }

    if (mode == Mode::drumMachine)
    {
        if (drumRack == nullptr)
        {
            auto rack = std::make_unique<ChannelRackComponent> (engine);
            rack->onOpenPianoRoll = [this] {
                if (onOpenPianoRoll)
                    onOpenPianoRoll();
            };
            rack->customChannelFilter = [this] (const ChannelDef& ch) -> bool {
                if (ch.instrument == InstrumentKind::audioClip)
                    return false;
                const int targetInsert = juce::jlimit (1, maxMixerTracks - 1, track > 0 ? track : 1);
                return ch.mixerTrack == targetInsert;
            };
            drumRack = std::move (rack);
            drumViewport.setViewedComponent (drumRack.get(), false);
        }
        if (auto* r = dynamic_cast<ChannelRackComponent*> (drumRack.get()))
            r->updateSize();
        drumViewport.setVisible (true);
    }
    else
    {
        drumViewport.setVisible (false);
    }

    resized();
    repaint();
}

void BandLabFxDrawer::timerCallback()
{
    if (! isVisible() || mode != Mode::input)
    {
        stopTimer();
        return;
    }
    const float p0 = engine.takeInputPeak (track, 0);
    const float p1 = engine.takeInputPeak (track, 1);
    const float peak = juce::jmax (p0, p1);
    liveInputPeak = juce::jmax (peak, liveInputPeak * 0.82f);
    if (liveInputPeak > 0.002f || peak > 0.002f)
        repaint (20, 155, 240, 50);
}

int BandLabFxDrawer::effectsStartX() const
{
    if (mode != Mode::input)
        return 24;
    if (embeddedEditor != nullptr)
        return embeddedEditor->getRight() + 14;
    return 268;
}

std::vector<int> BandLabFxDrawer::visibleEffectSlotIndices() const
{
    std::vector<int> out;
    const auto& song = engine.getSong();
    if (track < 0 || track >= (int) song.mixer.size())
        return out;
    const int apSlot = (mode == Mode::input) ? findAutoPitchSlot() : -1;
    const auto& effects = song.mixer[(std::size_t) track].effects;
    for (int i = 0; i < (int) effects.size(); ++i)
        if (i != apSlot)
            out.push_back (i);
    return out;
}

juce::Rectangle<int> BandLabFxDrawer::addCardBounds() const
{
    const auto indices = visibleEffectSlotIndices();
    const int count = (int) indices.size();
    const int cardW = 146;
    const int gap = 10;
    const int x = effectsStartX() + count * (cardW + gap);
    return { x, 46, cardW, juce::jmax (96, getHeight() - 62) };
}

int BandLabFxDrawer::slotCardAt (juce::Point<int> p) const
{
    const auto indices = visibleEffectSlotIndices();
    const int cardW = 146;
    const int gap = 10;
    const int startX = effectsStartX();
    for (int idx = 0; idx < (int) indices.size(); ++idx)
    {
        const juce::Rectangle<int> r (startX + idx * (cardW + gap), 46, cardW, juce::jmax (96, getHeight() - 62));
        if (r.contains (p))
            return indices[(std::size_t) idx];
    }
    return -1;
}

void BandLabFxDrawer::resized()
{
    if (isVisible() && mode == Mode::input && ! isTimerRunning())
        startTimerHz (30);

    if (mode == Mode::drumMachine)
    {
        if (drumRack == nullptr)
        {
            auto rack = std::make_unique<ChannelRackComponent> (engine);
            rack->onOpenPianoRoll = [this] {
                if (onOpenPianoRoll)
                    onOpenPianoRoll();
            };
            rack->customChannelFilter = [this] (const ChannelDef& ch) -> bool {
                if (ch.instrument == InstrumentKind::audioClip)
                    return false;
                const int targetInsert = juce::jlimit (1, maxMixerTracks - 1, track > 0 ? track : 1);
                return ch.mixerTrack == targetInsert;
            };
            drumRack = std::move (rack);
            drumViewport.setViewedComponent (drumRack.get(), false);
        }
        drumViewport.setVisible (true);
    }
    if (drumViewport.isVisible())
    {
        const int rightX = getWidth() - 136;
        const int rackW = juce::jmax (240, rightX - 24);
        const int rackH = juce::jmax (120, getHeight() - 50);
        drumViewport.setBounds (16, 42, rackW, rackH);
        if (auto* r = dynamic_cast<ChannelRackComponent*> (drumRack.get()))
        {
            r->updateSize();
            if (r->getWidth() < rackW)
                r->setSize (rackW, r->getHeight());
        }
    }

    if (embeddedEditor != nullptr)
    {
        const int rightX = getWidth() - 136;
        const int startX = 310;
        const int availW = juce::jmax (260, rightX - startX - 18);
        const int availH = juce::jmax (150, getHeight() - 40);
        const int ew = embeddedEditor->getWidth() > 0 ? embeddedEditor->getWidth() : 460;
        const int eh = embeddedEditor->getHeight() > 0 ? embeddedEditor->getHeight() : availH;
        const int w = juce::jmin (availW, ew);
        const int h = juce::jmin (availH, eh);
        const int y = 32 + juce::jmax (0, (availH - h) / 2);
        embeddedEditor->setBounds (startX, y, w, h);
    }
    repaint();
}

void BandLabFxDrawer::paint (juce::Graphics& g)
{
    using namespace theme;
    g.fillAll (colour::bg1);
    g.setColour (colour::border);
    g.fillRect (0, 0, getWidth(), 1);

    const auto& song = engine.getSong();
    juce::String trackTitle = tr ("Pista {0}", { juce::String (track) });
    const int plIdx = track - 1;
    if (plIdx >= 0 && plIdx < (int) song.playlistTracks.size() && ! song.playlistTracks[(std::size_t) plIdx].name.empty())
        trackTitle = juce::String::fromUTF8 (song.playlistTracks[(std::size_t) plIdx].name.c_str());

    const auto cb = closeBtnBounds().toFloat();
    fillRoundRect (g, cb, 11.0f, hoverClose ? colour::bg4 : colour::bg3);
    g.setColour (colour::textPrimary);
    g.setFont (font (type::smallSize, true));
    g.drawText ("X", cb.toNearestInt(), juce::Justification::centred, false);

    if (mode == Mode::drumMachine)
    {
        const auto padBox = juce::Rectangle<float> (40.0f, 11.0f, 16.0f, 16.0f);
        fillRoundRect (g, padBox, 3.5f, juce::Colour (0xffffb300));
        g.setColour (juce::Colours::black.withAlpha (0.85f));
        for (int gy = 0; gy < 2; ++gy)
            for (int gx = 0; gx < 2; ++gx)
                g.fillRect (padBox.getX() + 4.0f + gx * 5.0f, padBox.getY() + 4.0f + gy * 5.0f, 3.0f, 3.0f);
    }
    else
    {
        icons::draw (g, Icon::mic, { 40.0f, 12.0f, 14.0f, 14.0f }, colour::warning);
    }

    g.setColour (colour::textPrimary);
    g.setFont (font (type::title, true));
    g.drawText (mode == Mode::drumMachine ? tr ("Caja de ritmos") : trackTitle, 62, 7, 165, 24, juce::Justification::centredLeft, true);

    if (mode == Mode::drumMachine)
    {
        const auto addRowR = addDrumRowBtnBounds().toFloat();
        fillRoundRect (g, addRowR, 12.0f, hoverAddDrumRow ? colour::bg4 : colour::bg3);
        g.setColour (colour::borderStrong);
        g.drawRoundedRectangle (addRowR.reduced (0.5f), 12.0f, 1.0f);
        g.setColour (colour::textPrimary);
        g.setFont (font (type::caption, true));
        g.drawText (tr ("+ Fila"), addRowR.toNearestInt(), juce::Justification::centred, false);

        const auto newPatR = newDrumPatternBtnBounds().toFloat();
        fillRoundRect (g, newPatR, newPatR.getHeight() * 0.5f, hoverNewDrumPat ? colour::bg4 : colour::bg3);
        g.setColour (colour::borderStrong);
        g.drawRoundedRectangle (newPatR.reduced (0.5f), newPatR.getHeight() * 0.5f, 1.0f);
        g.setColour (colour::textPrimary);
        g.setFont (font (type::body, true));
        g.drawText ("+", newPatR.toNearestInt(), juce::Justification::centred, false);

        g.setColour (colour::textSecondary);
        g.setFont (font (type::caption, false));
        g.drawText (tr ("Arrastra samples (.wav / .mp3) sobre una fila para armar hi-hats, claps o kicks"),
                    newPatR.getRight() + 14, 8, juce::jmax (120, getWidth() - 136 - (int) newPatR.getRight() - 24), 22,
                    juce::Justification::centredLeft, true);
    }

    if (mode == Mode::input)
    {
        const bool hasMt = (track >= 0 && track < (int) song.mixer.size());
        const int inCh = hasMt ? song.mixer[(std::size_t) track].inputChannel : -1;
        const bool inStereo = hasMt ? song.mixer[(std::size_t) track].inputStereo : false;
        const bool monOn = hasMt && song.mixer[(std::size_t) track].monitor != InputMonitor::off;

        g.setColour (colour::textPrimary);
        g.setFont (font (type::caption, true));
        g.drawText (tr ("Entrada"), 24, 46, 200, 16, juce::Justification::centredLeft, false);

        juce::String devName = tr ("Seleccionar microfono...");
            const auto curSetup = engine.getDeviceManager().getAudioDeviceSetup();
            if (curSetup.inputDeviceName.isNotEmpty())
                devName = curSetup.inputDeviceName;
            else if (auto* dev = engine.getDeviceManager().getCurrentAudioDevice(); dev != nullptr && dev->getName().isNotEmpty())
                devName = dev->getName();

        const auto devBox = inputDeviceBoxBounds().toFloat();
        fillRoundRect (g, devBox, radius::sm, hoverInputPart == 1 ? colour::bg3 : colour::bg2);
        g.setColour (colour::borderStrong);
        g.drawRoundedRectangle (devBox.reduced (0.5f), radius::sm, 1.0f);
        g.setColour (colour::textPrimary);
        g.setFont (font (type::smallSize, false));
        g.drawFittedText (devName, inputDeviceBoxBounds().reduced (10, 0).withTrimmedRight (20),
                          juce::Justification::centredLeft, 1, 0.85f);
        icons::draw (g, Icon::chevronDown, devBox.withLeft (devBox.getRight() - 22.0f).withSizeKeepingCentre (12.0f, 12.0f), colour::textSecondary);

        g.setColour (colour::textPrimary);
        g.setFont (font (type::caption, true));
        g.drawText (tr ("Canal"), 24, 102, 200, 16, juce::Justification::centredLeft, false);

        juce::String chLabel = tr ("Sin entrada (ninguno)");
        const auto chNames = engine.getInputChannelNames();
        if (inCh >= 0 && inCh < (int) chNames.size())
        {
            if (inStereo && inCh + 1 < (int) chNames.size())
                chLabel = tr ("Canal {0} + {1}", { juce::String (inCh + 1), juce::String (inCh + 2) });
            else
                chLabel = tr ("Canal {0}", { juce::String (inCh + 1) }) + " (" + juce::String::fromUTF8 (chNames[(std::size_t) inCh].c_str()) + ")";
        }
        else if (inCh >= 0)
        {
            chLabel = inStereo ? tr ("Canal {0} + {1}", { juce::String (inCh + 1), juce::String (inCh + 2) })
                               : tr ("Canal {0}", { juce::String (inCh + 1) });
        }

        const auto chBox = inputChannelBoxBounds().toFloat();
        fillRoundRect (g, chBox, radius::sm, hoverInputPart == 2 ? colour::bg3 : colour::bg2);
        g.setColour (inCh >= 0 ? colour::accent : colour::borderStrong);
        g.drawRoundedRectangle (chBox.reduced (0.5f), radius::sm, 1.0f);
        g.setColour (colour::textPrimary);
        g.setFont (font (type::smallSize, false));
        g.drawFittedText (chLabel, inputChannelBoxBounds().reduced (10, 0).withTrimmedRight (20),
                          juce::Justification::centredLeft, 1, 0.85f);
        icons::draw (g, Icon::chevronDown, chBox.withLeft (chBox.getRight() - 22.0f).withSizeKeepingCentre (12.0f, 12.0f), colour::textSecondary);

        g.setColour (colour::textPrimary);
        g.setFont (font (type::caption, true));
        g.drawText (tr ("Nivel de entrada"), 24, 162, 104, 16, juce::Justification::centredLeft, false);

        const auto meterBg = juce::Rectangle<float> (24.0f, 184.0f, 96.0f, 5.0f);
        fillRoundRect (g, meterBg, 2.5f, colour::bg3);
        const float lvl = juce::jlimit (0.0f, 1.0f, std::sqrt (liveInputPeak));
        if (lvl > 0.01f)
            fillRoundRect (g, meterBg.withWidth (meterBg.getWidth() * lvl), 2.5f, lvl > 0.88f ? colour::accent : juce::Colour (0xff22c55e));

        const auto monR = monitorBtnBounds().toFloat();
        fillRoundRect (g, monR, monR.getHeight() * 0.5f, monOn ? colour::accent : (hoverInputPart == 3 ? colour::bg4 : colour::bg3));
        g.setColour (monOn ? colour::onAccent : colour::borderStrong);
        if (! monOn)
            g.drawRoundedRectangle (monR.reduced (0.5f), monR.getHeight() * 0.5f, 1.0f);
        g.setColour (monOn ? colour::onAccent : colour::textPrimary);
        g.setFont (font (type::caption, true));
        g.drawText (tr ("Monitorizacion"), monR.toNearestInt(), juce::Justification::centred, false);

        g.setColour (colour::border);
        g.fillRect (256, 44, 1, getHeight() - 56);
    }

    if (mode != Mode::drumMachine)
    {
    // Ranuras de efectos (a la derecha de Fuente/AutoPitch, o desde la izquierda en modo Efectos)
    const auto indices = visibleEffectSlotIndices();
    const int cardW = 146;
    const int gap = 10;
    const int cardH = juce::jmax (96, getHeight() - 62);
    const int startX = effectsStartX();

    if (track >= 0 && track < (int) song.mixer.size())
    {
        const auto& effects = song.mixer[(std::size_t) track].effects;
        for (int idx = 0; idx < (int) indices.size(); ++idx)
        {
            const int slotIdx = indices[(std::size_t) idx];
            const auto& slot = effects[(std::size_t) slotIdx];
            const auto card = juce::Rectangle<float> ((float) (startX + idx * (cardW + gap)), 46.0f, (float) cardW, (float) cardH);
            if (card.getRight() > (float) getWidth() - 144.0f)
                break;
            const bool hot = (slotIdx == hoverSlot);
            fillRoundRect (g, card, radius::md, hot ? colour::bg3 : colour::bg2);
            g.setColour (slot.enabled ? colour::borderStrong : colour::border);
            g.drawRoundedRectangle (card.reduced (0.5f), radius::md, 1.0f);

            const auto led = juce::Rectangle<float> (card.getX() + 10.0f, card.getY() + 10.0f, 12.0f, 12.0f);
            g.setColour (slot.enabled ? colour::accent : colour::textDisabled);
            g.fillEllipse (led);

            g.setColour (colour::textSecondary);
            g.setFont (font (type::smallSize, true));
            g.drawText ("...", (int) card.getRight() - 28, (int) card.getY() + 6, 20, 18, juce::Justification::centred, false);

            const auto slotTitle = (slot.kind == EffectKind::plugin && ! slot.plugin.name.empty())
                                       ? juce::String::fromUTF8 (slot.plugin.name.c_str())
                                       : addableEffectName (slot.kind);
            g.setColour (slot.enabled ? colour::textPrimary : colour::textDisabled);
            g.setFont (font (type::body, true));
            g.drawFittedText (slotTitle,
                              juce::Rectangle<int> ((int) card.getX() + 10, (int) card.getY() + 30, (int) card.getWidth() - 20, 22),
                              juce::Justification::centredLeft, 1, 0.85f);

            const auto uiBtn = juce::Rectangle<float> (card.getX() + 10.0f, card.getY() + 56.0f, 64.0f, 20.0f);
            fillRoundRect (g, uiBtn, 10.0f, colour::bg4);
            g.setColour (colour::textSecondary);
            g.setFont (font (type::caption, true));
            g.drawText (tr ("Abrir UI"), uiBtn.toNearestInt(), juce::Justification::centred, false);

            const int mixPct = juce::roundToInt (slot.mix * 100.0f);
            g.setColour (colour::textSecondary);
            g.setFont (font (type::caption, false));
            g.drawText (tr ("Mezcla") + " " + juce::String (mixPct) + "%",
                        (int) card.getX() + 10, (int) card.getBottom() - 24, (int) card.getWidth() - 20, 16,
                        juce::Justification::centredLeft, false);
        }
    }

    // Tarjeta "+ Añadir efecto" siempre disponible a la derecha del plugin / slots
    const auto addCard = addCardBounds().toFloat();
    if (addCard.getRight() < (float) getWidth() - 144.0f)
    {
        fillRoundRect (g, addCard, radius::md, hoverAdd ? colour::bg3 : colour::bg2);
        g.setColour (colour::borderStrong);
        const float dash[] = { 5.0f, 4.0f };
        g.drawDashedLine (juce::Line<float> (addCard.getTopLeft(), addCard.getTopRight()), dash, 2, 1.0f);
        g.drawDashedLine (juce::Line<float> (addCard.getBottomLeft(), addCard.getBottomRight()), dash, 2, 1.0f);
        g.drawDashedLine (juce::Line<float> (addCard.getTopLeft(), addCard.getBottomLeft()), dash, 2, 1.0f);
        g.drawDashedLine (juce::Line<float> (addCard.getTopRight(), addCard.getBottomRight()), dash, 2, 1.0f);

        const auto c = addCard.getCentre();
        g.setColour (colour::textPrimary);
        g.setFont (font (22.0f, true));
        g.drawText ("+", juce::Rectangle<int> ((int) c.x - 16, (int) c.y - 26, 32, 28), juce::Justification::centred, false);
        g.setFont (font (type::caption, true));
        g.drawText (tr ("Anadir efecto"), juce::Rectangle<int> ((int) addCard.getX() + 6, (int) c.y + 4, (int) addCard.getWidth() - 12, 18),
                    juce::Justification::centred, false);
    }

    // BLB: tarjeta "VST" (plugins externos) junto a "Anadir efecto": abre el picker de plugins de LYNX
    {
        const auto vstCard = vstCardBounds().toFloat();
        if (vstCard.getRight() < (float) getWidth() - 144.0f)
        {
            fillRoundRect (g, vstCard, radius::md, hoverVst ? colour::bg3 : colour::bg2);
            g.setColour (colour::borderStrong);
            g.drawRoundedRectangle (vstCard.reduced (0.5f), radius::md, 1.0f);

            const auto vc = vstCard.getCentre();
            g.setColour (colour::textPrimary);
            g.setFont (font (20.0f, true));
            g.drawText ("VST", juce::Rectangle<int> ((int) vc.x - 30, (int) vc.y - 28, 60, 26), juce::Justification::centred, false);
            g.setFont (font (type::caption, true));
            g.drawText (juce::String::fromUTF8 ("Plugins externos"), juce::Rectangle<int> ((int) vstCard.getX() + 6, (int) vc.y, (int) vstCard.getWidth() - 12, 16),
                        juce::Justification::centred, false);
            g.setColour (colour::textSecondary);
            g.setFont (font (type::caption, false));
            g.drawText (juce::String::fromUTF8 ("Elegir / administrar"), juce::Rectangle<int> ((int) vstCard.getX() + 6, (int) vc.y + 18, (int) vstCard.getWidth() - 12, 16),
                        juce::Justification::centred, false);
        }
    }
    }

    // Tira de Volumen vertical + Pan L/R a la derecha del panel
    const int rightX = getWidth() - 136;
    g.setColour (colour::border);
    g.fillRect (rightX, 12, 1, getHeight() - 24);

    float volNorm = 0.8f;
    float panNorm = 0.0f;
    if (track >= 0 && track < (int) song.mixer.size())
    {
        volNorm = juce::jlimit (0.0f, 1.0f, song.mixer[(std::size_t) track].volume);
        panNorm = juce::jlimit (-1.0f, 1.0f, song.mixer[(std::size_t) track].pan);
    }

    const auto faderTrack = juce::Rectangle<float> ((float) rightX + 34.0f, 28.0f, 6.0f, (float) getHeight() - 62.0f);
    fillRoundRect (g, faderTrack, 3.0f, colour::bg4);
    const float thumbY = faderTrack.getBottom() - faderTrack.getHeight() * volNorm;
    fillRoundRect (g, juce::Rectangle<float> (faderTrack.getX(), thumbY, faderTrack.getWidth(), faderTrack.getBottom() - thumbY), 3.0f, colour::accent);
    fillRoundRect (g, juce::Rectangle<float> (faderTrack.getCentreX() - 12.0f, thumbY - 5.0f, 24.0f, 10.0f), 4.0f, juce::Colours::white);

    g.setColour (colour::textSecondary);
    g.setFont (font (type::caption, true));
    g.drawText (tr ("Volumen"), rightX + 12, getHeight() - 22, 52, 16, juce::Justification::centred, false);

    const auto panR = juce::Rectangle<float> ((float) rightX + 86.0f, (float) getHeight() - 56.0f, 26.0f, 26.0f);
    g.setColour (juce::Colour (0xffeef0f4));
    g.fillEllipse (panR.reduced (2.0f));
    const float angle = panNorm * 2.2f;
    const auto pc = panR.getCentre();
    g.setColour (colour::bg0);
    g.drawLine (pc.x, pc.y, pc.x + std::sin (angle) * 7.5f, pc.y - std::cos (angle) * 7.5f, 1.8f);
    g.setColour (colour::textSecondary);
    g.setFont (font (type::caption, true));
    g.drawText (tr ("Pan"), rightX + 74, getHeight() - 22, 50, 16, juce::Justification::centred, false);
}

void BandLabFxDrawer::showInputDeviceMenu()
{
    juce::PopupMenu menu;
    menu.addSectionHeader (tr ("Dispositivo de entrada (Microfono)"));

    const auto setupNow = engine.getDeviceManager().getAudioDeviceSetup();
    juce::String currentDev = setupNow.inputDeviceName;
    if (currentDev.isEmpty())
        if (auto* dev = engine.getDeviceManager().getCurrentAudioDevice())
            currentDev = dev->getName();

    if (auto* typeObj = engine.getDeviceManager().getCurrentDeviceTypeObject())
    {
        typeObj->scanForDevices();
        const auto inNames = typeObj->getDeviceNames (true);
        for (const auto& name : inNames)
        {
            const bool ticked = (name == currentDev);
            menu.addItem (name, true, ticked, [this, name] {
                const auto notice = engine.openAudioInput (name);
                if (notice.issue != AudioEngine::InputIssue::none)
                    mixerinput::explain (notice);
                else if (track > 0 && track < (int) engine.getSong().mixer.size() && engine.getSong().mixer[(std::size_t) track].inputChannel < 0)
                {
                    engine.setInsertInput (track, 0, false);
                    engine.setInsertArmed (track, true);
                }
                repaint();
            });
        }
    }

    menu.addSeparator();
    menu.addItem (tr ("Configuracion de audio..."), mixerinput::openAudioSettings != nullptr, false, [] {
        if (mixerinput::openAudioSettings)
            mixerinput::openAudioSettings();
    });

    menu.showMenuAsync (juce::PopupMenu::Options().withTargetScreenArea (localAreaToGlobal (inputDeviceBoxBounds())));
}

void BandLabFxDrawer::showInputChannelMenu()
{
    if (! engine.hasAudioInput())
    {
        const auto notice = engine.openAudioInput();
        if (notice.issue != AudioEngine::InputIssue::none)
        {
            mixerinput::explain (notice);
            return;
        }
    }

    const auto& song = engine.getSong();
    const int curCh = (track >= 0 && track < (int) song.mixer.size()) ? song.mixer[(std::size_t) track].inputChannel : -1;
    const bool curStereo = (track >= 0 && track < (int) song.mixer.size()) ? song.mixer[(std::size_t) track].inputStereo : false;

    const auto names = engine.getInputChannelNames();
    juce::PopupMenu menu;
    menu.addSectionHeader (tr ("Canal de entrada"));
    menu.addItem (tr ("Sin entrada (ninguno)"), true, curCh < 0, [this] {
        engine.setInsertInput (track, -1, true);
        if (track > 0)
            engine.setInsertArmed (track, false);
        repaint();
    });

    if (! names.empty())
    {
        menu.addSeparator();
        for (int c = 0; c < (int) names.size(); ++c)
        {
            const auto label = tr ("Canal {0}", { juce::String (c + 1) }) + " - " + juce::String::fromUTF8 (names[(std::size_t) c].c_str()) + " (Mono)";
            const bool ticked = (curCh == c && ! curStereo);
            menu.addItem (label, true, ticked, [this, c] {
                engine.setInsertInput (track, c, false);
                if (track > 0)
                    engine.setInsertArmed (track, true);
                repaint();
            });
        }
        if (names.size() >= 2)
        {
            menu.addSeparator();
            for (int c = 0; c + 1 < (int) names.size(); c += 2)
            {
                const auto label = tr ("Canal {0} + {1}", { juce::String (c + 1), juce::String (c + 2) }) + " (Estereo)";
                const bool ticked = (curCh == c && curStereo);
                menu.addItem (label, true, ticked, [this, c] {
                    engine.setInsertInput (track, c, true);
                    if (track > 0)
                        engine.setInsertArmed (track, true);
                    repaint();
                });
            }
        }
    }
    else
    {
        menu.addItem (tr ("No hay canales de entrada disponibles"), false, false, nullptr);
    }

    menu.showMenuAsync (juce::PopupMenu::Options().withTargetScreenArea (localAreaToGlobal (inputChannelBoxBounds())));
}

juce::Rectangle<int> BandLabFxDrawer::vstCardBounds() const
{
    return addCardBounds().translated (146 + 10, 0); // al lado de "Anadir efecto" (cardW 146 + gap 10)
}

void BandLabFxDrawer::addEffectSlot (EffectSlot slot)
{
    int newIdx = -1;
    engine.editSong ([this, &slot, &newIdx] (Song& s) {
        if (s.mixer.empty())
            s.mixer = Song::defaultMixer (0);
        if (track >= 0 && track < (int) s.mixer.size() && (int) s.mixer[(std::size_t) track].effects.size() < maxEffectSlots)
        {
            s.mixer[(std::size_t) track].effects.push_back (std::move (slot));
            newIdx = (int) s.mixer[(std::size_t) track].effects.size() - 1;
        }
    });
    repaint();
    if (newIdx >= 0 && onOpenSlotEditor)
        onOpenSlotEditor (track, newIdx);
}

// BLB: el selector de efectos estilo BandLab (ui/BandLabEffectPicker) reemplaza al menu plano de efectos
void BandLabFxDrawer::showAddEffectMenu()
{
    juce::Component::SafePointer<BandLabFxDrawer> safe (this);
    BandLabEffectPicker::show (engine, localAreaToGlobal (addCardBounds()), [safe] (EffectSlot slot) {
        if (safe != nullptr)
            safe->addEffectSlot (std::move (slot));
    });
}

// BLB: tarjeta "VST": abre el picker de plugins de LYNX (elegir un plugin externo, o administrarlos / escanear)
void BandLabFxDrawer::showVstPicker()
{
    juce::Component::SafePointer<BandLabFxDrawer> safe (this);
    showPluginPicker (engine, false, [safe] (PluginRef ref) {
        if (safe == nullptr)
            return;
        EffectSlot slot;
        slot.kind = EffectKind::plugin;
        slot.enabled = true;
        slot.mix = 1.0f;
        slot.plugin.id = ref.id;
        slot.plugin.name = ref.name;
        safe->addEffectSlot (std::move (slot));
    });
}

void BandLabFxDrawer::showSlotOptionsMenu (int slotIndex)
{
    juce::PopupMenu menu;
    menu.addItem (tr ("Abrir interfaz"), [this, slotIndex] {
        if (onOpenSlotEditor)
            onOpenSlotEditor (track, slotIndex);
    });
    menu.addItem (tr ("Quitar efecto"), [this, slotIndex] {
        engine.editSong ([this, slotIndex] (Song& s) {
            if (track >= 0 && track < (int) s.mixer.size() && slotIndex >= 0 && slotIndex < (int) s.mixer[(std::size_t) track].effects.size())
                s.mixer[(std::size_t) track].effects.erase (s.mixer[(std::size_t) track].effects.begin() + slotIndex);
        });
        repaint();
    });
    menu.showMenuAsync (juce::PopupMenu::Options().withMousePosition());
}

void BandLabFxDrawer::mouseMove (const juce::MouseEvent& e)
{
    const int hs = slotCardAt (e.getPosition());
    const bool ha = addCardBounds().contains (e.getPosition());
    const bool hv = vstCardBounds().contains (e.getPosition());
    const bool hc = closeBtnBounds().contains (e.getPosition());
    const bool hadr = (mode == Mode::drumMachine) && addDrumRowBtnBounds().contains (e.getPosition());
    const bool hndp = (mode == Mode::drumMachine) && newDrumPatternBtnBounds().contains (e.getPosition());
    if (hadr != hoverAddDrumRow || hndp != hoverNewDrumPat)
    {
        hoverAddDrumRow = hadr;
        hoverNewDrumPat = hndp;
        repaint();
    }
    int hip = 0;
    if (mode == Mode::input)
    {
        if (inputDeviceBoxBounds().contains (e.getPosition())) hip = 1;
        else if (inputChannelBoxBounds().contains (e.getPosition())) hip = 2;
        else if (monitorBtnBounds().contains (e.getPosition())) hip = 3;
    }
    if (hs != hoverSlot || ha != hoverAdd || hv != hoverVst || hc != hoverClose || hip != hoverInputPart)
    {
        hoverSlot = hs;
        hoverAdd = ha;
        hoverVst = hv;
        hoverClose = hc;
        hoverInputPart = hip;
        repaint();
    }
}

void BandLabFxDrawer::mouseExit (const juce::MouseEvent&)
{
    hoverSlot = -1;
    hoverAdd = false;
    hoverVst = false;
    hoverClose = false;
    hoverAddDrumRow = false;
    hoverNewDrumPat = false;
    hoverInputPart = 0;
    repaint();
}

void BandLabFxDrawer::mouseDown (const juce::MouseEvent& e)
{
    dragKind = DragKind::none;
    dragSlot = -1;

    if (closeBtnBounds().contains (e.getPosition()))
    {
        if (onClose)
            onClose();
        return;
    }

    if (mode == Mode::drumMachine && newDrumPatternBtnBounds().contains (e.getPosition()))
    {
        engine.editSong ([this] (Song& s) {
            int drumPatCount = 0;
            for (const auto& pat : s.patterns)
                if (pat.name.rfind ("Caja de ritmos", 0) == 0 || pat.name.rfind ("Ritmos", 0) == 0)
                    ++drumPatCount;

            Pattern pat;
            pat.name = "Caja de ritmos " + std::to_string (drumPatCount + 1);
            const int patIdx = (int) s.patterns.size();
            s.patterns.push_back (std::move (pat));
            s.currentPattern = patIdx;

            const int plIdx = track - 1;
            if (plIdx >= 0 && plIdx < (int) s.playlistTracks.size())
            {
                std::int64_t nextStart = 0;
                for (const auto& c : s.playlist)
                    if (c.track == plIdx)
                        nextStart = std::max<std::int64_t> (nextStart, c.end());

                Clip clip;
                clip.kind = ClipKind::pattern;
                clip.pattern = patIdx;
                clip.track = plIdx;
                clip.start = nextStart;
                clip.length = s.patterns[(std::size_t) patIdx].effectiveLength (s.beatsPerBar);
                s.playlist.push_back (clip);
                s.mode = PlayMode::song;
            }
        });
        if (auto* r = dynamic_cast<ChannelRackComponent*> (drumRack.get()))
            r->updateSize();
        resized();
        repaint();
        return;
    }

    if (mode == Mode::drumMachine && addDrumRowBtnBounds().contains (e.getPosition()))
    {
        engine.editSong ([this] (Song& s) {
            const std::uint32_t pal[] = { 0xfff59e0b, 0xff00c4b4, 0xffa855f7, 0xff3d8bfd, 0xff10b981, 0xffff6b1a };
            const int idx = (int) s.channels.size();
            ChannelDef ch;
            ch.name = "Sample " + std::to_string (idx + 1);
            ch.instrument = InstrumentKind::sampler;
            ch.mixerTrack = juce::jlimit (1, maxMixerTracks - 1, track > 0 ? track : 1);
            ch.colour = pal[(std::size_t) idx % 6];
            s.channels.push_back (std::move (ch));
        });
        if (auto* r = dynamic_cast<ChannelRackComponent*> (drumRack.get()))
            r->updateSize();
        resized();
        repaint();
        return;
    }

    if (mode == Mode::input)
    {
        if (inputDeviceBoxBounds().contains (e.getPosition()))
        {
            showInputDeviceMenu();
            return;
        }
        if (inputChannelBoxBounds().contains (e.getPosition()))
        {
            showInputChannelMenu();
            return;
        }
        if (monitorBtnBounds().contains (e.getPosition()))
        {
            const auto& s = engine.getSong();
            const bool currentlyOn = (track >= 0 && track < (int) s.mixer.size()) && s.mixer[(std::size_t) track].monitor != InputMonitor::off;
            engine.setInsertMonitor (track, currentlyOn ? InputMonitor::off : InputMonitor::on);
            repaint();
            return;
        }
    }

    const int rightX = getWidth() - 136;
    if (e.x >= rightX && e.x < rightX + 74)
    {
        dragKind = DragKind::volume;
        const auto& s = engine.getSong();
        dragStartVal = (track >= 0 && track < (int) s.mixer.size()) ? s.mixer[(std::size_t) track].volume : 0.8f;
        return;
    }
    if (e.x >= rightX + 74)
    {
        dragKind = DragKind::pan;
        const auto& s = engine.getSong();
        dragStartVal = (track >= 0 && track < (int) s.mixer.size()) ? s.mixer[(std::size_t) track].pan : 0.0f;
        return;
    }

    if (addCardBounds().contains (e.getPosition()))
    {
        showAddEffectMenu();
        return;
    }

    if (vstCardBounds().contains (e.getPosition()))
    {
        showVstPicker();
        return;
    }

    const int slotIdx = slotCardAt (e.getPosition());
    if (slotIdx >= 0)
    {
        const auto indices = visibleEffectSlotIndices();
        int visPos = 0;
        for (int i = 0; i < (int) indices.size(); ++i)
            if (indices[(std::size_t) i] == slotIdx)
                visPos = i;

        const int cardW = 146, gap = 10;
        const int cardX = effectsStartX() + visPos * (cardW + gap);
        const juce::Rectangle<int> ledRect (cardX + 10, 56, 12, 12);
        if (ledRect.contains (e.getPosition()))
        {
            engine.editSong ([this, slotIdx] (Song& s) {
                if (track >= 0 && track < (int) s.mixer.size() && slotIdx < (int) s.mixer[(std::size_t) track].effects.size())
                    s.mixer[(std::size_t) track].effects[(std::size_t) slotIdx].enabled = ! s.mixer[(std::size_t) track].effects[(std::size_t) slotIdx].enabled;
            });
            repaint();
            return;
        }
        const juce::Rectangle<int> uiRect (cardX + 10, 102, 64, 20);
        if (uiRect.contains (e.getPosition()))
        {
            if (onOpenSlotEditor)
                onOpenSlotEditor (track, slotIdx);
            return;
        }
        if (e.mods.isPopupMenu() || e.x > cardX + cardW - 30)
        {
            showSlotOptionsMenu (slotIdx);
            return;
        }
        dragKind = DragKind::mix;
        dragSlot = slotIdx;
        const auto& s = engine.getSong();
        dragStartVal = (track >= 0 && track < (int) s.mixer.size() && slotIdx < (int) s.mixer[(std::size_t) track].effects.size())
                           ? s.mixer[(std::size_t) track].effects[(std::size_t) slotIdx].mix : 1.0f;
    }
}

void BandLabFxDrawer::mouseDrag (const juce::MouseEvent& e)
{
    if (dragKind == DragKind::none)
        return;
    const float deltaY = (float) (e.getDistanceFromDragStartY()) / 120.0f;
    if (dragKind == DragKind::volume)
    {
        const float next = juce::jlimit (0.0f, 1.0f, dragStartVal - deltaY);
        engine.editLevels ([this, next] (Song& s) {
            if (track >= 0 && track < (int) s.mixer.size())
                s.mixer[(std::size_t) track].volume = next;
        });
    }
    else if (dragKind == DragKind::pan)
    {
        const float next = juce::jlimit (-1.0f, 1.0f, dragStartVal - deltaY * 2.0f);
        engine.editLevels ([this, next] (Song& s) {
            if (track >= 0 && track < (int) s.mixer.size())
                s.mixer[(std::size_t) track].pan = next;
        });
    }
    else if (dragKind == DragKind::mix && dragSlot >= 0)
    {
        const float next = juce::jlimit (0.0f, 1.0f, dragStartVal - deltaY);
        engine.editSong ([this, next] (Song& s) {
            if (track >= 0 && track < (int) s.mixer.size() && dragSlot < (int) s.mixer[(std::size_t) track].effects.size())
                s.mixer[(std::size_t) track].effects[(std::size_t) dragSlot].mix = next;
        });
    }
    repaint();
}

void BandLabFxDrawer::mouseDoubleClick (const juce::MouseEvent& e)
{
    const int slotIdx = slotCardAt (e.getPosition());
    if (slotIdx >= 0 && onOpenSlotEditor)
        onOpenSlotEditor (track, slotIdx);
}
} // namespace lynx
