#include "ui/BandLabEffectPicker.h"

#include <algorithm>

#include "core/ValueMath.h"
#include "engine/PluginHost.h"
#include "ui/Icons.h"
#include "ui/I18n.h"
#include "ui/MixerComponent.h"
#include "ui/Theme.h"

namespace lynx
{
using namespace theme;
namespace
{
constexpr int blbPickerW = 640;
constexpr int blbPickerH = 540;
constexpr int blbLeftW = 176;
constexpr int blbCatTop = 54;
constexpr int blbCatH = 28;

struct BlbCategoryDef
{
    const char* id;
    const char* label; // UTF-8
};

const BlbCategoryDef blbCategoryDefs[] = {
    { "rec", "Recomendados" }, { "all", "Todos" },        { "voice", "Voz" },          { "eq", "EQ" },
    { "dyn", "Din\xC3\xA1mica" }, { "lim", "Limitador" }, { "delay", "Delay" },        { "reverb", "Reverb" },
    { "mod", "Modulaci\xC3\xB3n" }, { "dist", "Distorsi\xC3\xB3n" }, { "sat", "Saturaci\xC3\xB3n" }, { "pitch", "Pitch" },
    { "rhythm", "R\xC3\xADtmico" }, { "filter", "Filtro" }, { "stereo", "Est\xC3\xA9reo" }, { "util", "Utilidad" },
    { "legacy", "Legacy" },
};
constexpr int blbNumCategories = (int) (sizeof (blbCategoryDefs) / sizeof (blbCategoryDefs[0]));

// Efectos que se muestran en "Recomendados" (los que existan, en este orden)
const char* const blbRecommendedNames[] = { "Parametric EQ", "Compressor", "Apex Limiter", "Reverb", "Epoch Verb", "Delay",
                                         "Hush De-esser", "Gemini Doubler", "Voice Shift", "Quell Denoiser",
                                         "Ember Saturator", "Chorus" };

juce::String blbUtf8 (const char* s) { return juce::String::fromUTF8 (s); }

// Categorias de un efecto, segun palabras de su nombre (heuristica; un efecto puede estar en varias)
juce::StringArray blbClassify (const juce::String& raw)
{
    const auto n = raw.toLowerCase();
    const auto tokens = juce::StringArray::fromTokens (n, " -()/", "");
    const auto has = [&n] (const char* s) { return n.contains (s); };

    juce::StringArray out;
    if (tokens.contains ("eq") || has ("equo") || has ("equalizer"))
        out.add ("eq");
    if (has ("compress") || has ("gate") || has ("leveler") || has ("transient") || has ("de-esser") || has ("denoiser"))
        out.add ("dyn");
    if (has ("limiter") || has ("clipper") || has ("maximus"))
        out.add ("lim");
    if (has ("delay") || has ("echo"))
        out.add ("delay");
    if (has ("verb") || has ("plate") || has ("atrium") || has ("nebula") || has ("halo") || has ("horizon") || has ("convolver"))
        out.add ("reverb");
    if (has ("chorus") || has ("flang") || has ("phaser") || has ("lattice") || has ("doubler") || has ("vibrato"))
        out.add ("mod");
    if (has ("dist") || has ("overdrive") || has ("waveshaper") || has ("hardcore") || has ("lo-fi") || has ("bitcrush"))
        out.add ("dist");
    if (has ("saturat") || has ("channel") || has ("tube") || has ("tape"))
        out.add ("sat");
    if (has ("pitch") || has ("shifter") || has ("voice shift") || has ("newtone") || has ("newtime") || has ("tune"))
        out.add ("pitch");
    if (has ("gross beat") || has ("auto-pan") || has ("rhythmic") || has ("scratcher"))
        out.add ("rhythm");
    if (has ("filter") || has ("philter") || has ("fast lp") || has ("crossover") || has ("splitter"))
        out.add ("filter");
    if (has ("stereo") || has ("spreader") || has ("pan") || has ("balance") || has ("center") || has ("phase inverter"))
        out.add ("stereo");
    if (has ("hush") || has ("de-esser") || has ("doubler") || has ("voice") || has ("denoiser") || has ("vocod") || has ("tune"))
        out.add ("voice");
    if (out.isEmpty())
        out.add ("util");
    if (n.startsWith ("fruity"))
        out.add ("legacy");
    return out;
}

juce::Colour blbTintFor (const juce::String& cat)
{
    if (cat == "eq") return juce::Colour (0xff3b82f6);
    if (cat == "dyn") return juce::Colour (0xff10b981);
    if (cat == "lim") return juce::Colour (0xff14b8a6);
    if (cat == "delay") return juce::Colour (0xff8b5cf6);
    if (cat == "reverb") return juce::Colour (0xff6366f1);
    if (cat == "mod") return juce::Colour (0xffec4899);
    if (cat == "dist") return juce::Colour (0xffef4444);
    if (cat == "sat") return juce::Colour (0xfff97316);
    if (cat == "pitch") return juce::Colour (0xfff59e0b);
    if (cat == "rhythm") return juce::Colour (0xffa855f7);
    if (cat == "filter") return juce::Colour (0xff0ea5e9);
    if (cat == "stereo") return juce::Colour (0xff22d3ee);
    if (cat == "voice") return juce::Colour (0xff84cc16);
    return juce::Colour (0xff64748b);
}

struct BlbKnownDescription
{
    const char* name; // en minusculas
    const char* text; // UTF-8
};

const BlbKnownDescription blbKnownDescriptions[] = {
    { "parametric eq", "EQ param\xC3\xA9trico de varias bandas con control total." },
    { "facet eq", "EQ visual para esculpir el tono con precisi\xC3\xB3n." },
    { "eq", "Ecualizador simple para ajustar graves, medios y agudos." },
    { "compressor", "Compresor cl\xC3\xA1sico para controlar la din\xC3\xA1mica." },
    { "limiter", "Limitador para que el nivel no pase de un techo." },
    { "apex limiter", "Limitador transparente para subir el volumen sin distorsi\xC3\xB3n." },
    { "reverb", "Reverb limpia para dar ambiente a la voz." },
    { "epoch verb", "Reverb espaciosa para dar profundidad." },
    { "delay", "Eco sincronizado con el tempo del proyecto." },
    { "chorus", "Engrosa el sonido con copias levemente desafinadas." },
    { "filter", "Filtro para recortar graves o agudos." },
    { "distortion", "Distorsi\xC3\xB3n para agregar mordida y car\xC3\xA1" "cter." },
    { "utility", "Volumen, paneo y fase en un solo efecto." },
    { "lo-fi", "Sonido sucio y comprimido estilo lo-fi." },
    { "auto-pan", "Mueve el sonido de izquierda a derecha." },
    { "vocoder", "Convierte la voz en un sonido rob\xC3\xB3tico." },
    { "hush de-esser", "Suaviza las eses y las sibilancias de la voz." },
    { "gemini doubler", "Duplica la voz para darle cuerpo y ancho." },
    { "voice shift", "Cambia el tono y el timbre de la voz." },
    { "quell denoiser", "Reduce el ruido de fondo de la grabaci\xC3\xB3n." },
    { "ember saturator", "Saturaci\xC3\xB3n c\xC3\xA1lida para dar cuerpo y color." },
    { "sentry gate", "Puerta de ruido: corta la se\xC3\xB1" "al por debajo de un nivel." },
    { "volume gate", "Corta el sonido cuando baja de un nivel." },
    { "stereo enhancer", "Ensancha la imagen est\xC3\xA9reo." },
    { "maximus", "Procesador multibanda para dar volumen y presencia." },
    { "gross beat", "Cortes y cambios de velocidad r\xC3\xADtmicos." },
};

const char* blbCategoryDescription (const juce::String& cat)
{
    if (cat == "eq") return "Ecualizador para dar forma al tono y al balance de frecuencias.";
    if (cat == "dyn") return "Controla la din\xC3\xA1mica y los picos de volumen.";
    if (cat == "lim") return "Limita el nivel m\xC3\xA1ximo y sube el volumen sin saturar.";
    if (cat == "delay") return "Repeticiones y ecos sincronizados con el tempo.";
    if (cat == "reverb") return "Reverb para dar espacio y profundidad.";
    if (cat == "mod") return "Modulaci\xC3\xB3n para dar movimiento y amplitud.";
    if (cat == "dist") return "Distorsi\xC3\xB3n para agregar mordida y car\xC3\xA1" "cter.";
    if (cat == "sat") return "Saturaci\xC3\xB3n c\xC3\xA1lida para dar cuerpo y color.";
    if (cat == "pitch") return "Cambia o corrige el tono de la se\xC3\xB1" "al.";
    if (cat == "rhythm") return "Efecto r\xC3\xADtmico sincronizado con el tempo.";
    if (cat == "filter") return "Filtro para recortar o resaltar zonas del espectro.";
    if (cat == "stereo") return "Ajusta el ancho y la imagen est\xC3\xA9reo.";
    if (cat == "voice") return "Herramienta pensada para mejorar la voz.";
    return "Utilidad para ajustar, enrutar o analizar la se\xC3\xB1" "al.";
}

juce::String blbDescribe (const juce::String& name, const juce::String& primary)
{
    auto key = name.toLowerCase();
    if (key.startsWith ("fruity "))
        key = key.substring (7);
    for (const auto& k : blbKnownDescriptions)
        if (key == k.name)
            return blbUtf8 (k.text);
    return blbUtf8 (blbCategoryDescription (primary));
}

juce::String blbInitialsOf (const juce::String& name)
{
    auto tokens = juce::StringArray::fromTokens (name, " -()/", "");
    tokens.removeEmptyStrings();
    if (tokens.size() > 1 && tokens[0].equalsIgnoreCase ("fruity"))
        tokens.remove (0);
    juce::String out;
    for (int i = 0; i < tokens.size() && i < 2; ++i)
        out += tokens[i].substring (0, 1);
    if (out.length() < 2 && tokens.size() > 0 && tokens[0].length() > 1)
        out += tokens[0].substring (1, 2);
    return out.toUpperCase();
}
} // namespace

BandLabEffectPicker::BandLabEffectPicker (AudioEngine& e, std::function<void (EffectSlot)> pick)
    : engine (e), onPick (std::move (pick))
{
    buildItems();

    searchBox.setJustification (juce::Justification::centredLeft);
    searchBox.setIndents (12, 0);
    searchBox.setColour (juce::TextEditor::backgroundColourId, colour::bg3);
    searchBox.setColour (juce::TextEditor::textColourId, colour::textPrimary);
    searchBox.setColour (juce::TextEditor::outlineColourId, colour::border);
    searchBox.setColour (juce::TextEditor::focusedOutlineColourId, colour::accent);
    searchBox.addListener (this);
    addAndMakeVisible (searchBox);

    list.setModel (this);
    list.setRowHeight (68);
    list.setOutlineThickness (0);
    list.setColour (juce::ListBox::backgroundColourId, juce::Colours::transparentBlack);
    list.addMouseListener (this, true);
    addAndMakeVisible (list);

    setSize (blbPickerW, blbPickerH);
    refilter();

    juce::Component::SafePointer<BandLabEffectPicker> safe (this);
    juce::MessageManager::callAsync ([safe] {
        if (safe != nullptr)
            safe->searchBox.grabKeyboardFocus();
    });
}

BandLabEffectPicker::~BandLabEffectPicker()
{
    list.removeMouseListener (this);
    list.setModel (nullptr);
    searchBox.removeListener (this);
}

void BandLabEffectPicker::show (AudioEngine& engine, juce::Rectangle<int> screenArea, std::function<void (EffectSlot)> onPick)
{
    auto picker = std::make_unique<BandLabEffectPicker> (engine, std::move (onPick));
    juce::CallOutBox::launchAsynchronously (std::move (picker), screenArea, nullptr);
}

void BandLabEffectPicker::buildItems()
{
    items.clear();
    for (const auto kind : addableEffects())
    {
        if (! addableEffectAvailable (kind))
            continue;
        Item it;
        it.kind = kind;
        it.name = addableEffectName (kind);
        it.cats = blbClassify (it.name);
        it.tint = blbTintFor (it.cats[0]);
        it.description = blbDescribe (it.name, it.cats[0]);
        it.initials = blbInitialsOf (it.name);
        it.lower = (it.name + " " + it.description).toLowerCase();
        items.push_back (std::move (it));
    }
    std::sort (items.begin(), items.end(), [] (const Item& a, const Item& b) { return a.name.compareIgnoreCase (b.name) < 0; });
}

void BandLabEffectPicker::refilter()
{
    shown.clear();
    const auto query = searchBox.getText().toLowerCase().trim();
    const auto words = juce::StringArray::fromTokens (query, " ", "");
    const juce::String cat (blbCategoryDefs[selectedCat].id);

    const auto matches = [&words] (const Item& it) {
        for (const auto& w : words)
            if (! it.lower.contains (w))
                return false;
        return true;
    };

    if (words.size() > 0) // buscando: en todas las categorias
    {
        for (int i = 0; i < (int) items.size(); ++i)
            if (matches (items[(std::size_t) i]))
                shown.push_back (i);
    }
    else if (cat == "rec")
    {
        for (const auto* wanted : blbRecommendedNames)
            for (int i = 0; i < (int) items.size(); ++i)
                if (items[(std::size_t) i].name.equalsIgnoreCase (wanted))
                {
                    shown.push_back (i);
                    break;
                }
    }
    else
    {
        for (int i = 0; i < (int) items.size(); ++i)
            if (cat == "all" || items[(std::size_t) i].cats.contains (cat))
                shown.push_back (i);
    }

    hoverRow = -1;
    list.updateContent();
    list.scrollToEnsureRowIsOnscreen (0);
    list.repaint();
    repaint();
}

juce::Rectangle<int> BandLabEffectPicker::categoryRect (int index) const
{
    const int rowH = juce::jmax (16, juce::jmin (blbCatH, (getHeight() - blbCatTop - 8) / blbNumCategories));
    return { 8, blbCatTop + index * rowH, blbLeftW - 16, rowH - 2 };
}

int BandLabEffectPicker::categoryAt (juce::Point<int> p) const
{
    for (int i = 0; i < blbNumCategories; ++i)
        if (categoryRect (i).contains (p))
            return i;
    return -1;
}

void BandLabEffectPicker::paint (juce::Graphics& g)
{
    fillRoundRect (g, getLocalBounds().toFloat(), radius::md, colour::bg2);

    g.setColour (colour::border);
    g.fillRect (blbLeftW, 0, 1, getHeight());

    g.setColour (colour::textPrimary);
    g.setFont (font (17.0f, true));
    g.drawText (blbUtf8 ("Efectos"), 16, 16, blbLeftW - 24, 26, juce::Justification::centredLeft, false);

    for (int i = 0; i < blbNumCategories; ++i)
    {
        const auto r = categoryRect (i);
        const bool selected = (i == selectedCat) && searchBox.getText().isEmpty();
        if (selected)
            fillRoundRect (g, r.toFloat(), radius::sm, colour::bg3);
        else if (i == hoverCat)
            fillRoundRect (g, r.toFloat(), radius::sm, colour::bg3.withAlpha (0.5f));
        g.setColour (selected ? colour::accent : colour::textPrimary);
        g.setFont (font (type::body, selected));
        g.drawText (blbUtf8 (blbCategoryDefs[i].label), r.reduced (10, 0), juce::Justification::centredLeft, false);
    }

    const bool searching = searchBox.getText().trim().isNotEmpty();
    const auto heading = searching ? blbUtf8 ("Resultados") : blbUtf8 (blbCategoryDefs[selectedCat].label);
    g.setColour (colour::textPrimary);
    g.setFont (font (17.0f, true));
    g.drawText (heading, blbLeftW + 18, 58, getWidth() - blbLeftW - 36, 28, juce::Justification::centredLeft, false);

    if (shown.empty())
    {
        g.setColour (colour::textSecondary);
        g.setFont (font (type::body, false));
        g.drawText (blbUtf8 ("No hay efectos que coincidan."), blbLeftW + 18, 110, getWidth() - blbLeftW - 36, 24,
                    juce::Justification::centredLeft, false);
    }
}

void BandLabEffectPicker::paintOverChildren (juce::Graphics& g)
{
    if (searchBox.getText().isNotEmpty())
        return;
    g.setColour (colour::textDisabled);
    g.setFont (font (type::body, false));
    g.drawText (blbUtf8 ("Buscar efectos..."), searchBox.getBounds().withTrimmedLeft (14), juce::Justification::centredLeft, false);
}

void BandLabEffectPicker::resized()
{
    searchBox.setBounds (blbLeftW + 16, 14, getWidth() - blbLeftW - 32, 32);
    list.setBounds (blbLeftW + 8, 92, getWidth() - blbLeftW - 16, getHeight() - 100);
}

void BandLabEffectPicker::mouseMove (const juce::MouseEvent& e)
{
    int newCat = -1;
    int newRow = -1;
    if (e.originalComponent == this)
        newCat = categoryAt (e.getPosition());
    else
    {
        const auto p = e.getEventRelativeTo (&list).getPosition();
        newRow = list.getRowContainingPosition (p.x, p.y);
    }
    if (newCat != hoverCat || newRow != hoverRow)
    {
        hoverCat = newCat;
        hoverRow = newRow;
        repaint();
        list.repaint();
    }
}

void BandLabEffectPicker::mouseExit (const juce::MouseEvent&)
{
    if (hoverCat != -1 || hoverRow != -1)
    {
        hoverCat = -1;
        hoverRow = -1;
        repaint();
        list.repaint();
    }
}

void BandLabEffectPicker::mouseDown (const juce::MouseEvent& e)
{
    if (e.originalComponent != this)
        return;
    const int cat = categoryAt (e.getPosition());
    if (cat < 0)
        return;
    selectedCat = cat;
    searchBox.clear(); // vuelve a mostrar la categoria (dispara refilter por textEditorTextChanged)
    refilter();
    searchBox.grabKeyboardFocus();
}

bool BandLabEffectPicker::keyPressed (const juce::KeyPress& key)
{
    if (key == juce::KeyPress::escapeKey)
    {
        dismiss();
        return true;
    }
    return false;
}

int BandLabEffectPicker::getNumRows()
{
    return (int) shown.size();
}

void BandLabEffectPicker::paintListBoxItem (int row, juce::Graphics& g, int w, int h, bool)
{
    if (row < 0 || row >= (int) shown.size())
        return;
    const auto& it = items[(std::size_t) shown[(std::size_t) row]];

    if (row == hoverRow)
        fillRoundRect (g, juce::Rectangle<float> (0.0f, 2.0f, (float) w - 6.0f, (float) h - 4.0f), radius::md, colour::bg3);

    const auto icon = juce::Rectangle<float> (10.0f, (float) h * 0.5f - 24.0f, 48.0f, 48.0f);
    fillRoundRect (g, icon, radius::sm, it.tint);
    g.setColour (juce::Colours::white.withAlpha (0.94f));
    g.setFont (font (17.0f, true));
    g.drawText (it.initials, icon.toNearestInt(), juce::Justification::centred, false);

    g.setColour (colour::textPrimary);
    g.setFont (font (type::body, true));
    g.drawText (it.name, 72, 10, w - 90, 22, juce::Justification::centredLeft, true);

    g.setColour (colour::textSecondary);
    g.setFont (font (type::smallSize, false));
    g.drawFittedText (it.description, juce::Rectangle<int> (72, 32, w - 90, h - 38), juce::Justification::topLeft, 2, 0.95f);
}

void BandLabEffectPicker::listBoxItemClicked (int row, const juce::MouseEvent& e)
{
    if (e.mods.isPopupMenu())
        return;
    pickRow (row);
}

void BandLabEffectPicker::textEditorTextChanged (juce::TextEditor&)
{
    refilter();
}

void BandLabEffectPicker::textEditorReturnKeyPressed (juce::TextEditor&)
{
    if (! shown.empty())
        pickRow (0);
}

void BandLabEffectPicker::textEditorEscapeKeyPressed (juce::TextEditor&)
{
    dismiss();
}

void BandLabEffectPicker::pickRow (int row)
{
    if (row < 0 || row >= (int) shown.size())
        return;
    const auto kind = items[(std::size_t) shown[(std::size_t) row]].kind;
    auto slot = newAddableEffect (kind, engine.getSong().bpm);
    if (! slot.has_value())
        return;

    auto callback = onPick;
    const EffectSlot made = *slot;
    dismiss(); // no tocar miembros despues de esto: el panel se destruye
    juce::MessageManager::callAsync ([callback, made] {
        if (callback)
            callback (made);
    });
}

void BandLabEffectPicker::dismiss()
{
    if (auto* box = findParentComponentOfClass<juce::CallOutBox>())
        box->dismiss();
}
} // namespace lynx
