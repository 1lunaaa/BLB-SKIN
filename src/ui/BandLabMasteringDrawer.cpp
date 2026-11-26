#include "ui/BandLabMasteringDrawer.h"

#include "ui/Theme.h"

#include <cmath>

namespace lynx
{
namespace
{
struct MasteringPresetInfo
{
    const char* name;
    std::uint32_t colour;
};

constexpr MasteringPresetInfo kPresets[8] = {
    { "Universal",  0xffa855f7 },
    { "Fuego",      0xfff97316 },
    { "Claridad",   0xff38bdf8 },
    { "Cinta",      0xfff59e0b },
    { "Natural",    0xff10b981 },
    { "Espacial",   0xff06b6d4 },
    { "Cinemático", 0xffeab308 },
    { "Empuje",     0xffec4899 }
};

void drawPresetIcon (juce::Graphics& g, int idx, juce::Rectangle<float> box, juce::Colour col)
{
    g.setColour (col);
    const float cx = box.getCentreX();
    const float cy = box.getCentreY();

    switch (idx)
    {
        case 0: // Universal
        {
            g.drawLine (cx - 4.0f, cy + 5.0f, cx - 4.0f, cy - 6.0f, 1.5f);
            g.drawLine (cx + 5.0f, cy + 3.0f, cx + 5.0f, cy - 8.0f, 1.5f);
            g.drawLine (cx - 4.0f, cy - 6.0f, cx + 5.0f, cy - 8.0f, 2.0f);
            g.drawLine (cx - 4.0f, cy - 2.5f, cx + 5.0f, cy - 4.5f, 1.4f);
            g.drawEllipse (cx - 8.5f, cy + 2.5f, 5.2f, 4.0f, 1.5f);
            g.drawEllipse (cx + 0.5f, cy + 0.5f, 5.2f, 4.0f, 1.5f);
            break;
        }
        case 1: // Fuego
        {
            juce::Path p;
            p.startNewSubPath (cx, cy - 9.0f);
            p.cubicTo (cx + 6.0f, cy - 3.0f, cx + 8.0f, cy + 3.0f, cx + 4.0f, cy + 7.5f);
            p.cubicTo (cx + 1.5f, cy + 9.5f, cx - 2.5f, cy + 9.5f, cx - 4.5f, cy + 7.0f);
            p.cubicTo (cx - 8.0f, cy + 3.0f, cx - 5.5f, cy - 2.0f, cx - 2.5f, cy - 4.5f);
            p.cubicTo (cx - 2.5f, cy - 1.5f, cx - 0.5f, cy - 0.5f, cx, cy - 9.0f);
            g.strokePath (p, juce::PathStrokeType (1.5f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
            break;
        }
        case 2: // Claridad
        {
            juce::Path p;
            p.startNewSubPath (cx - 5.0f, cy - 6.0f);
            p.lineTo (cx + 5.0f, cy - 6.0f);
            p.lineTo (cx + 9.0f, cy - 1.5f);
            p.lineTo (cx, cy + 8.5f);
            p.lineTo (cx - 9.0f, cy - 1.5f);
            p.closeSubPath();
            g.strokePath (p, juce::PathStrokeType (1.5f));
            g.drawLine (cx - 9.0f, cy - 1.5f, cx + 9.0f, cy - 1.5f, 1.2f);
            g.drawLine (cx - 3.5f, cy - 1.5f, cx, cy + 8.5f, 1.2f);
            g.drawLine (cx + 3.5f, cy - 1.5f, cx, cy + 8.5f, 1.2f);
            g.drawLine (cx - 5.0f, cy - 6.0f, cx - 3.5f, cy - 1.5f, 1.2f);
            g.drawLine (cx + 5.0f, cy - 6.0f, cx + 3.5f, cy - 1.5f, 1.2f);
            break;
        }
        case 3: // Cinta
        {
            juce::Path p;
            p.addRoundedRectangle (-9.0f, -6.0f, 18.0f, 12.0f, 2.0f);
            p.applyTransform (juce::AffineTransform::rotation (-0.22f).translated (cx, cy));
            g.strokePath (p, juce::PathStrokeType (1.5f));
            g.drawEllipse (cx - 5.5f, cy - 2.2f, 4.0f, 4.0f, 1.3f);
            g.drawEllipse (cx + 1.5f, cy - 3.5f, 4.0f, 4.0f, 1.3f);
            break;
        }
        case 4: // Natural
        {
            juce::Path centre;
            centre.startNewSubPath (cx, cy - 8.5f);
            centre.quadraticTo (cx + 4.5f, cy - 1.0f, cx, cy + 6.5f);
            centre.quadraticTo (cx - 4.5f, cy - 1.0f, cx, cy - 8.5f);
            g.strokePath (centre, juce::PathStrokeType (1.5f));

            juce::Path left;
            left.startNewSubPath (cx - 7.5f, cy - 4.0f);
            left.quadraticTo (cx - 1.0f, cy - 1.5f, cx - 0.5f, cy + 6.5f);
            left.quadraticTo (cx - 8.0f, cy + 5.0f, cx - 7.5f, cy - 4.0f);
            g.strokePath (left, juce::PathStrokeType (1.5f));

            juce::Path right;
            right.startNewSubPath (cx + 7.5f, cy - 4.0f);
            right.quadraticTo (cx + 1.0f, cy - 1.5f, cx + 0.5f, cy + 6.5f);
            right.quadraticTo (cx + 8.0f, cy + 5.0f, cx + 7.5f, cy - 4.0f);
            g.strokePath (right, juce::PathStrokeType (1.5f));
            break;
        }
        case 5: // Espacial
        {
            juce::Path p;
            for (int step = 0; step <= 48; ++step)
            {
                const float t = (float) step / 48.0f;
                const float ang = t * 3.2f * juce::MathConstants<float>::pi;
                const float rad = 1.0f + t * 8.0f;
                const float px = cx + std::cos (ang) * rad;
                const float py = cy + std::sin (ang) * rad;
                if (step == 0) p.startNewSubPath (px, py);
                else p.lineTo (px, py);
            }
            g.strokePath (p, juce::PathStrokeType (1.5f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
            g.fillEllipse (cx + 6.5f, cy - 7.5f, 2.5f, 2.5f);
            g.fillEllipse (cx - 8.5f, cy + 5.5f, 2.5f, 2.5f);
            break;
        }
        case 6: // Cinemático
        {
            g.drawEllipse (cx - 8.5f, cy - 8.5f, 17.0f, 17.0f, 1.5f);
            g.fillEllipse (cx - 1.5f, cy - 1.5f, 3.0f, 3.0f);
            for (int k = 0; k < 5; ++k)
            {
                const float ang = (float) k * juce::MathConstants<float>::twoPi / 5.0f - 0.3f;
                const float hx = cx + std::cos (ang) * 4.6f;
                const float hy = cy + std::sin (ang) * 4.6f;
                g.drawEllipse (hx - 1.6f, hy - 1.6f, 3.2f, 3.2f, 1.2f);
            }
            break;
        }
        case 7: // Empuje
        {
            juce::Path p;
            constexpr int pts = 14;
            for (int k = 0; k < pts; ++k)
            {
                const float ang = (float) k * juce::MathConstants<float>::twoPi / (float) pts;
                const float rad = (k % 2 == 0) ? 9.5f : 4.2f;
                const float px = cx + std::cos (ang) * rad;
                const float py = cy + std::sin (ang) * rad;
                if (k == 0) p.startNewSubPath (px, py);
                else p.lineTo (px, py);
            }
            p.closeSubPath();
            g.strokePath (p, juce::PathStrokeType (1.4f));
            break;
        }
        default: break;
    }
}
} // namespace

BandLabMasteringDrawer::BandLabMasteringDrawer()
{
    setOpaque (true);
}

juce::Rectangle<int> BandLabMasteringDrawer::presetCardBounds (int idx) const
{
    const int col = idx % 4;
    const int row = idx / 4;
    const int padX = 16;
    const int gap = 8;
    const int availW = getWidth() - padX * 2 - gap * 3;
    const int cardW = juce::jmax (54, availW / 4);
    const int cardH = 98;
    const int startY = 54;
    return { padX + col * (cardW + gap), startY + row * (cardH + gap), cardW, cardH };
}

juce::Rectangle<int> BandLabMasteringDrawer::intensityBoxBounds() const
{
    return { 16, 268, getWidth() - 32, 86 };
}

juce::Rectangle<int> BandLabMasteringDrawer::intensityTrackBounds() const
{
    const auto box = intensityBoxBounds();
    return { box.getX() + 16, box.getY() + 42, box.getWidth() - 32, 16 };
}

juce::Rectangle<int> BandLabMasteringDrawer::eqBoxBounds() const
{
    return { 16, 364, getWidth() - 32, 132 };
}

juce::Rectangle<int> BandLabMasteringDrawer::eqToggleBounds() const
{
    const auto box = eqBoxBounds();
    return { box.getRight() - 48, box.getY() + 14, 34, 18 };
}

juce::Rectangle<int> BandLabMasteringDrawer::eqKnobBounds (int idx) const
{
    const auto box = eqBoxBounds();
    const int colW = box.getWidth() / 3;
    const int cx = box.getX() + colW * idx + colW / 2;
    const int cy = box.getY() + 72;
    return { cx - 22, cy - 22, 44, 44 };
}

juce::Rectangle<int> BandLabMasteringDrawer::applyBtnBounds() const
{
    const int y = juce::jmin (getHeight() - 56, eqBoxBounds().getBottom() + 16);
    return { 16, y, getWidth() - 32, 40 };
}

void BandLabMasteringDrawer::updateIntensityFromX (int x)
{
    const auto track = intensityTrackBounds();
    if (track.getWidth() <= 0)
        return;
    const float raw = juce::jlimit (0.0f, 1.0f, (float) (x - track.getX()) / (float) track.getWidth());
    intensity = std::round (raw * 10.0f) / 10.0f;
}

void BandLabMasteringDrawer::paint (juce::Graphics& g)
{
    using namespace theme;
    g.fillAll (juce::Colour (0xff121316));

    g.setColour (juce::Colour (0xff252830));
    g.drawVerticalLine (0, 0.0f, (float) getHeight());

    g.setColour (colour::textPrimary);
    g.setFont (font (17.5f, true));
    g.drawText (juce::String::fromUTF8 ("Masterizando"), 18, 12, getWidth() - 100, 28, juce::Justification::centredLeft, false);

    // Speaker icon button
    {
        const auto sp = speakerBtnBounds().toFloat();
        if (hoverSpeaker || bypassActive)
        {
            g.setColour (bypassActive ? colour::accent.withAlpha (0.22f) : colour::bg3);
            g.fillRoundedRectangle (sp, 6.0f);
        }
        g.setColour (bypassActive ? colour::textDisabled : colour::textPrimary);
        juce::Path cone;
        const float cx = sp.getCentreX();
        const float cy = sp.getCentreY();
        cone.startNewSubPath (cx - 6.0f, cy - 2.5f);
        cone.lineTo (cx - 3.0f, cy - 2.5f);
        cone.lineTo (cx + 1.0f, cy - 6.0f);
        cone.lineTo (cx + 1.0f, cy + 6.0f);
        cone.lineTo (cx - 3.0f, cy + 2.5f);
        cone.lineTo (cx - 6.0f, cy + 2.5f);
        cone.closeSubPath();
        g.strokePath (cone, juce::PathStrokeType (1.4f));
        g.drawLine (cx + 4.0f, cy - 3.0f, cx + 4.0f, cy + 3.0f, 1.4f);
        g.drawLine (cx + 6.8f, cy - 5.0f, cx + 6.8f, cy + 5.0f, 1.4f);
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

    // 4x2 Grid of Mastering Presets
    for (int i = 0; i < 8; ++i)
    {
        const auto r = presetCardBounds (i).toFloat();
        const bool isSel = (selectedPreset == i);
        const bool isHov = (hoverPreset == i);
        const auto presetCol = juce::Colour (kPresets[i].colour);

        g.setColour (isSel ? juce::Colour (0xff191722) : (isHov ? juce::Colour (0xff1f2126) : juce::Colour (0xff181a1e)));
        g.fillRoundedRectangle (r, 9.0f);

        g.setColour (isSel ? presetCol : juce::Colour (0xff2b2e36));
        g.drawRoundedRectangle (r.reduced (0.5f), 9.0f, isSel ? 1.4f : 1.0f);

        drawPresetIcon (g, i, juce::Rectangle<float> (r.getCentreX() - 14.0f, r.getY() + 24.0f, 28.0f, 28.0f), presetCol);

        g.setColour (colour::textPrimary);
        g.setFont (font (11.5f, true));
        g.drawText (juce::String::fromUTF8 (kPresets[i].name), r.withTrimmedTop (56.0f).withTrimmedBottom (10.0f).toNearestInt(),
                    juce::Justification::centred, false);
    }

    // Intensidad card
    {
        const auto ib = intensityBoxBounds().toFloat();
        g.setColour (juce::Colour (0xff1a1c20));
        g.fillRoundedRectangle (ib, 10.0f);

        g.setColour (colour::textPrimary);
        g.setFont (font (12.5f, true));
        g.drawText (juce::String::fromUTF8 ("Intensidad"), (int) ib.getX() + 14, (int) ib.getY() + 10, 140, 18, juce::Justification::centredLeft, false);

        const auto trk = intensityTrackBounds().toFloat();
        const float midY = trk.getCentreY();
        const float thumbX = trk.getX() + intensity * trk.getWidth();

        g.setColour (juce::Colour (0xff343842));
        g.drawLine (trk.getX(), midY, trk.getRight(), midY, 2.0f);

        const auto accentPurple = juce::Colour (0xffa855f7);
        g.setColour (accentPurple);
        g.drawLine (trk.getX(), midY, thumbX, midY, 2.2f);

        for (int t = 0; t <= 10; ++t)
        {
            const float tx = trk.getX() + ((float) t / 10.0f) * trk.getWidth();
            const bool activeTick = ((float) t / 10.0f) <= intensity + 0.001f;
            g.setColour (activeTick ? accentPurple : juce::Colour (0xff454954));
            g.drawLine (tx, midY - 3.0f, tx, midY + 3.0f, 1.2f);
        }

        g.setColour (accentPurple);
        g.fillRoundedRectangle (thumbX - 2.5f, midY - 7.5f, 5.0f, 15.0f, 2.5f);

        g.setColour (colour::textSecondary);
        g.setFont (font (11.0f, false));
        const int labelY = (int) ib.getBottom() - 24;
        g.drawText (juce::String::fromUTF8 ("Ligero"), (int) trk.getX(), labelY, 60, 16, juce::Justification::centredLeft, false);
        g.drawText (juce::String::fromUTF8 ("Normal"), (int) trk.getCentreX() - 35, labelY, 70, 16, juce::Justification::centred, false);
        g.drawText (juce::String::fromUTF8 ("Pesado"), (int) trk.getRight() - 60, labelY, 60, 16, juce::Justification::centredRight, false);
    }

    // Ecualizador card
    {
        const auto eb = eqBoxBounds().toFloat();
        g.setColour (juce::Colour (0xff1a1c20));
        g.fillRoundedRectangle (eb, 10.0f);

        g.setColour (colour::textPrimary);
        g.setFont (font (12.5f, true));
        g.drawText (juce::String::fromUTF8 ("Ecualizador"), (int) eb.getX() + 14, (int) eb.getY() + 12, 82, 18, juce::Justification::centredLeft, false);

        const auto sw = eqToggleBounds().toFloat();
        g.setColour (eqEnabled ? juce::Colours::white : juce::Colour (0xff3a3e47));
        g.fillRoundedRectangle (sw, sw.getHeight() * 0.5f);
        const float dotD = sw.getHeight() - 4.0f;
        const float dotX = eqEnabled ? (sw.getRight() - dotD - 2.0f) : (sw.getX() + 2.0f);
        g.setColour (juce::Colour (0xff121316));
        g.fillEllipse (dotX, sw.getY() + 2.0f, dotD, dotD);

        const char* knobLabels[3] = { "Low", "Mid", "High" };
        for (int k = 0; k < 3; ++k)
        {
            const auto kb = eqKnobBounds (k).toFloat();
            const float cx = kb.getCentreX();
            const float cy = kb.getCentreY();
            const float outerR = 19.0f;
            const float innerR = 13.5f;

            g.setColour (juce::Colour (0xff2c3038));
            g.drawEllipse (cx - outerR, cy - outerR, outerR * 2.0f, outerR * 2.0f, 1.6f);

            g.setColour (eqEnabled ? juce::Colours::white : juce::Colour (0xff7c828d));
            g.fillEllipse (cx - innerR, cy - innerR, innerR * 2.0f, innerR * 2.0f);

            const float angle = (eqValues[(std::size_t) k] - 0.5f) * 2.0f * (juce::MathConstants<float>::pi * 0.75f);
            const float ix = cx + std::sin (angle) * (innerR - 1.5f);
            const float iy = cy - std::cos (angle) * (innerR - 1.5f);
            g.setColour (juce::Colour (0xff121316));
            g.drawLine (cx, cy, ix, iy, 1.8f);

            g.setColour (colour::textSecondary);
            g.setFont (font (11.2f, false));
            g.drawText (knobLabels[k], (int) kb.getX() - 10, (int) kb.getBottom() + 4, (int) kb.getWidth() + 20, 16,
                        juce::Justification::centred, false);
        }
    }

    // Bottom pill button "Solicitar"
    {
        const auto ab = applyBtnBounds().toFloat();
        g.setColour (hoverApply ? juce::Colour (0xffe5e7eb) : juce::Colours::white);
        g.fillRoundedRectangle (ab, ab.getHeight() * 0.5f);
        g.setColour (juce::Colour (0xff121316));
        g.setFont (font (13.5f, true));
        g.drawText (juce::String::fromUTF8 ("Solicitar"), ab.toNearestInt(), juce::Justification::centred, false);
    }
}

void BandLabMasteringDrawer::mouseMove (const juce::MouseEvent& e)
{
    const auto pt = e.getPosition();
    const bool newSpeaker = speakerBtnBounds().contains (pt);
    const bool newClose = closeBtnBounds().contains (pt);
    const bool newApply = applyBtnBounds().contains (pt);

    int newPreset = -1;
    for (int i = 0; i < 8; ++i)
        if (presetCardBounds (i).contains (pt))
            newPreset = i;

    if (newSpeaker != hoverSpeaker || newClose != hoverClose
        || newApply != hoverApply || newPreset != hoverPreset)
    {
        hoverSpeaker = newSpeaker;
        hoverClose = newClose;
        hoverApply = newApply;
        hoverPreset = newPreset;
        const bool hand = hoverSpeaker || hoverClose || hoverApply || hoverPreset >= 0
                          || eqToggleBounds().contains (pt);
        setMouseCursor (hand ? juce::MouseCursor::PointingHandCursor : juce::MouseCursor::NormalCursor);
        repaint();
    }
}

void BandLabMasteringDrawer::mouseExit (const juce::MouseEvent&)
{
    hoverSpeaker = hoverClose = hoverApply = false;
    hoverPreset = -1;
    setMouseCursor (juce::MouseCursor::NormalCursor);
    repaint();
}

void BandLabMasteringDrawer::mouseDown (const juce::MouseEvent& e)
{
    const auto pt = e.getPosition();
    dragTarget = DragTarget::none;

    if (closeBtnBounds().contains (pt))
    {
        if (onClose) onClose();
        return;
    }
    if (speakerBtnBounds().contains (pt))
    {
        bypassActive = ! bypassActive;
        repaint();
        return;
    }
    for (int i = 0; i < 8; ++i)
    {
        if (presetCardBounds (i).contains (pt))
        {
            selectedPreset = i;
            repaint();
            return;
        }
    }
    if (intensityTrackBounds().expanded (4, 8).contains (pt))
    {
        dragTarget = DragTarget::intensity;
        updateIntensityFromX (pt.x);
        repaint();
        return;
    }
    if (eqToggleBounds().expanded (4, 4).contains (pt))
    {
        eqEnabled = ! eqEnabled;
        repaint();
        return;
    }
    for (int k = 0; k < 3; ++k)
    {
        if (eqKnobBounds (k).contains (pt))
        {
            if (e.mods.isAltDown())
            {
                eqValues[(std::size_t) k] = 0.5f;
                repaint();
                return;
            }
            dragTarget = (k == 0) ? DragTarget::knobLow : (k == 1) ? DragTarget::knobMid : DragTarget::knobHigh;
            dragStartY = pt.y;
            dragStartVal = eqValues[(std::size_t) k];
            return;
        }
    }
    if (applyBtnBounds().contains (pt))
    {
        repaint();
    }
}

void BandLabMasteringDrawer::mouseDrag (const juce::MouseEvent& e)
{
    if (dragTarget == DragTarget::intensity)
    {
        updateIntensityFromX (e.x);
        repaint();
    }
    else if (dragTarget == DragTarget::knobLow || dragTarget == DragTarget::knobMid || dragTarget == DragTarget::knobHigh)
    {
        const int idx = (dragTarget == DragTarget::knobLow) ? 0 : (dragTarget == DragTarget::knobMid) ? 1 : 2;
        const float delta = (float) (dragStartY - e.y) / 110.0f;
        eqValues[(std::size_t) idx] = juce::jlimit (0.0f, 1.0f, dragStartVal + delta);
        repaint();
    }
}

void BandLabMasteringDrawer::mouseUp (const juce::MouseEvent&)
{
    dragTarget = DragTarget::none;
}
} // namespace lynx
