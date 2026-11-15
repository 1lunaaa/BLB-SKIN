#include "ui/BandLabFxDrawer.h"

#include "core/ValueMath.h"
#include "ui/Icons.h"
#include "ui/MixerComponent.h"

namespace lynx
{
BandLabFxDrawer::BandLabFxDrawer (AudioEngine& e)
    : engine (e)
{
    engine.addChangeListener (this);
}

BandLabFxDrawer::~BandLabFxDrawer()
{
    engine.removeChangeListener (this);
}

void BandLabFxDrawer::setTrack (int t)
{
    track = juce::jmax (0, t);
    repaint();
}

juce::Rectangle<int> BandLabFxDrawer::addCardBounds() const
{
    const auto& song = engine.getSong();
    int count = 0;
    if (track >= 0 && track < (int) song.mixer.size())
        count = (int) song.mixer[(std::size_t) track].effects.size();

    const int cardW = 146;
    const int gap = 10;
    const int x = 24 + count * (cardW + gap);
    return { x, 46, cardW, juce::jmax (96, getHeight() - 62) };
}

int BandLabFxDrawer::slotCardAt (juce::Point<int> p) const
{
    const auto& song = engine.getSong();
    if (track < 0 || track >= (int) song.mixer.size())
        return -1;
    const int count = (int) song.mixer[(std::size_t) track].effects.size();
    const int cardW = 146;
    const int gap = 10;
    for (int i = 0; i < count; ++i)
    {
        const juce::Rectangle<int> r (24 + i * (cardW + gap), 46, cardW, juce::jmax (96, getHeight() - 62));
        if (r.contains (p))
            return i;
    }
    return -1;
}

void BandLabFxDrawer::resized()
{
    repaint();
}

void BandLabFxDrawer::paint (juce::Graphics& g)
{
    using namespace theme;
    g.fillAll (colour::bg1);
    g.setColour (colour::border);
    g.fillRect (0, 0, getWidth(), 1);

    const auto& song = engine.getSong();
    juce::String trackTitle = tr ("Pista {0}", { juce::String (track + 1) });
    if (track >= 0 && track < (int) song.playlistTracks.size() && ! song.playlistTracks[(std::size_t) track].name.empty())
        trackTitle = juce::String::fromUTF8 (song.playlistTracks[(std::size_t) track].name.c_str());

    // Encabezado del panel inferior BandLab
    const auto cb = closeBtnBounds().toFloat();
    fillRoundRect (g, cb, 11.0f, hoverClose ? colour::bg4 : colour::bg3);
    g.setColour (colour::textPrimary);
    g.setFont (font (type::smallSize, true));
    g.drawText ("X", cb.toNearestInt(), juce::Justification::centred, false);

    g.setColour (colour::textPrimary);
    g.setFont (font (type::title, true));
    g.drawText (trackTitle, 40, 7, 260, 24, juce::Justification::centredLeft, true);

    // Ranuras de efectos (slots) del insert seleccionado
    const std::vector<EffectSlot>* slots = nullptr;
    if (track >= 0 && track < (int) song.mixer.size())
        slots = &song.mixer[(std::size_t) track].effects;

    const int cardW = 146;
    const int gap = 10;
    const int cardH = juce::jmax (96, getHeight() - 62);

    if (slots != nullptr)
    {
        for (int i = 0; i < (int) slots->size(); ++i)
        {
            const auto& slot = (*slots)[(std::size_t) i];
            const auto card = juce::Rectangle<float> ((float) (24 + i * (cardW + gap)), 46.0f, (float) cardW, (float) cardH);
            const bool hot = (i == hoverSlot);
            fillRoundRect (g, card, radius::md, hot ? colour::bg3 : colour::bg2);
            g.setColour (slot.enabled ? colour::borderStrong : colour::border);
            g.drawRoundedRectangle (card.reduced (0.5f), radius::md, 1.0f);

            // Boton bypass (encendido/apagado) arriba a la izquierda
            const auto led = juce::Rectangle<float> (card.getX() + 10.0f, card.getY() + 10.0f, 12.0f, 12.0f);
            g.setColour (slot.enabled ? colour::accent : colour::textDisabled);
            g.fillEllipse (led);

            // Tres puntos / opciones a la derecha
            g.setColour (colour::textSecondary);
            g.setFont (font (type::smallSize, true));
            g.drawText ("...", (int) card.getRight() - 28, (int) card.getY() + 6, 20, 18, juce::Justification::centred, false);

            // Nombre del efecto
            g.setColour (slot.enabled ? colour::textPrimary : colour::textDisabled);
            g.setFont (font (type::body, true));
            g.drawFittedText (addableEffectName (slot.kind),
                              juce::Rectangle<int> ((int) card.getX() + 10, (int) card.getY() + 30, (int) card.getWidth() - 20, 22),
                              juce::Justification::centredLeft, 1, 0.85f);

            // Boton "Abrir UI"
            const auto uiBtn = juce::Rectangle<float> (card.getX() + 10.0f, card.getY() + 56.0f, 64.0f, 20.0f);
            fillRoundRect (g, uiBtn, 10.0f, colour::bg4);
            g.setColour (colour::textSecondary);
            g.setFont (font (type::caption, true));
            g.drawText (tr ("Abrir UI"), uiBtn.toNearestInt(), juce::Justification::centred, false);

            // Mezcla (Mix %) abajo
            const int mixPct = juce::roundToInt (slot.mix * 100.0f);
            g.setColour (colour::textSecondary);
            g.setFont (font (type::caption, false));
            g.drawText (tr ("Mezcla") + " " + juce::String (mixPct) + "%",
                        (int) card.getX() + 10, (int) card.getBottom() - 24, (int) card.getWidth() - 20, 16,
                        juce::Justification::centredLeft, false);
        }
    }

    // Tarjeta "+ Añadir efecto" estilo BandLab
    const auto addCard = addCardBounds().toFloat();
    if (addCard.getRight() < (float) getWidth() - 160.0f)
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

    // Pan knob blanco L / R
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

void BandLabFxDrawer::showAddEffectMenu()
{
    juce::PopupMenu menu;
    for (const auto kind : addableEffects())
    {
        if (! addableEffectAvailable (kind))
            continue;
        menu.addItem (addableEffectName (kind), [this, kind] {
            auto slot = newAddableEffect (kind, engine.getSong().bpm);
            if (! slot.has_value())
                return;
            int newIdx = -1;
            engine.editSong ([this, &slot, &newIdx] (Song& s) {
                if (s.mixer.empty())
                    s.mixer = Song::defaultMixer (0);
                if (track >= 0 && track < (int) s.mixer.size() && (int) s.mixer[(std::size_t) track].effects.size() < maxEffectSlots)
                {
                    s.mixer[(std::size_t) track].effects.push_back (std::move (*slot));
                    newIdx = (int) s.mixer[(std::size_t) track].effects.size() - 1;
                }
            });
            repaint();
            if (newIdx >= 0 && onOpenSlotEditor)
                onOpenSlotEditor (track, newIdx);
        });
    }
    menu.showMenuAsync (juce::PopupMenu::Options().withTargetScreenArea (localAreaToGlobal (addCardBounds())));
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
    const bool hc = closeBtnBounds().contains (e.getPosition());
    if (hs != hoverSlot || ha != hoverAdd || hc != hoverClose)
    {
        hoverSlot = hs;
        hoverAdd = ha;
        hoverClose = hc;
        repaint();
    }
}

void BandLabFxDrawer::mouseExit (const juce::MouseEvent&)
{
    hoverSlot = -1;
    hoverAdd = false;
    hoverClose = false;
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

    const int slotIdx = slotCardAt (e.getPosition());
    if (slotIdx >= 0)
    {
        const int cardW = 146, gap = 10;
        const int cardX = 24 + slotIdx * (cardW + gap);
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
