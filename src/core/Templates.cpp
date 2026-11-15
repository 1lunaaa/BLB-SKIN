#include "core/Templates.h"

#include "core/Effects.h"
#include "core/SynthPresets.h"

#include <algorithm>

namespace lynx
{
namespace
{
SynthSettings preset (const char* name)
{
    for (const auto& [n, s] : synthPresets())
        if (n == name)
            return s;
    return {};
}

ChannelDef synth (const char* name, const char* presetName, int track, float volume = 0.78f)
{
    ChannelDef c;
    c.name = name;
    c.instrument = InstrumentKind::synth;
    c.synth = preset (presetName);
    c.mixerTrack = track;
    c.volume = volume;
    return c;
}

void steps (Pattern& p, int channel, std::initializer_list<int> list, float velocity = 0.78f)
{
    for (auto s : list)
    {
        Note n;
        n.start = (std::int64_t) s * ticksPerStep;
        n.channel = channel;
        n.velocity = velocity;
        p.notes.push_back (n);
    }
}

void notes (Pattern& p, int channel, std::initializer_list<std::array<int, 3>> list)
{
    // {step, length in steps, key}
    for (const auto& [step, length, key] : list)
    {
        Note n;
        n.start = (std::int64_t) step * ticksPerStep;
        n.length = (std::int64_t) length * ticksPerStep;
        n.key = key;
        n.channel = channel;
        p.notes.push_back (n);
    }
}
} // namespace

std::vector<std::string> templateNames()
{
    return { "Empty" };
}

Song makeTemplate (int /*index*/)
{
    auto s = Song::makeDefault();
    s.patterns[0].notes.clear();
    return s;
}

std::string legalProjectName (const std::string& name)
{
    std::string out;
    for (auto c : name)
        out += std::string ("<>:\"/\\|?*").find (c) != std::string::npos || (unsigned char) c < 32 ? '_' : c;
    const auto first = out.find_first_not_of (" .");
    if (first == std::string::npos)
        return "Untitled";
    out.erase (0, first);
    while (! out.empty() && (out.back() == ' ' || out.back() == '.'))
        out.pop_back();
    return out.empty() ? "Untitled" : out;
}

std::string newProjectPath (const NewProjectSetup& setup, const char* extension)
{
    if (setup.folder.empty())
        return {};
    auto folder = setup.folder;
    while (folder.size() > 1 && (folder.back() == '/' || folder.back() == '\\'))
        folder.pop_back();
    const auto name = legalProjectName (setup.name);
    return folder + "/" + (setup.ownFolder ? name + "/" : std::string()) + name + extension;
}

void applySetup (Song& song, const NewProjectSetup& setup)
{
    song.bpm = std::clamp (setup.bpm, 10.0, 522.0);
    song.beatsPerBar = std::clamp (setup.beatsPerBar, 1, 16);
    song.info = setup.info;
    if (song.info.title.empty() && ! setup.name.empty())
        song.info.title = setup.name;
}

std::string templateDisplayName (const std::string& fileName)
{
    auto name = fileName;
    const auto slash = name.find_last_of ("/\\");
    if (slash != std::string::npos)
        name.erase (0, slash + 1);
    const auto dot = name.find_last_of ('.');
    if (dot != std::string::npos && dot > 0)
        name.erase (dot);
    return name;
}

std::string NewProjectDefaults::toText() const
{
    const char* chain = master == MasterChain::empty ? "empty" : master == MasterChain::clipper ? "clipper" : "default";
    return "newInserts=" + std::to_string (inserts) + "\nnewMasterChain=" + chain + "\nnewCleanMixer=" + (cleanMixer ? "1" : "0")
           + "\nplaylistTracks=" + std::to_string (playlistTracks) + "\nnewAutoCrossfades=" + (autoCrossfades ? "1" : "0") + "\n";
}

void NewProjectDefaults::readKey (const std::string& key, const std::string& value)
{
    auto number = [&value] (int fallback) {
        try
        {
            return std::stoi (value);
        }
        catch (...)
        {
            return fallback;
        }
    };

    if (key == "newInserts")
        inserts = std::clamp (number (defaultInserts), 0, maxMixerTracks - 1);
    else if (key == "newMasterChain")
        master = value == "empty" ? MasterChain::empty : value == "clipper" ? MasterChain::clipper : MasterChain::lynxDefault;
    else if (key == "newCleanMixer")
        cleanMixer = value == "1";
    else if (key == "playlistTracks")
        playlistTracks = std::clamp (number (16), 1, 500);
    else if (key == "newAutoCrossfades")
        autoCrossfades = value != "0";
}

void applyNewProjectDefaults (Song& song, const NewProjectDefaults& defaults)
{
    const auto inserts = std::clamp (defaults.inserts, 0, maxMixerTracks - 1);
    if (song.mixer.empty())
        song.mixer = Song::defaultMixer (inserts);
    else if ((int) song.mixer.size() > inserts + 1)
        song.mixer.resize ((std::size_t) inserts + 1);
    else
        while ((int) song.mixer.size() < inserts + 1)
        {
            MixerTrack track;
            track.name = "Insert " + std::to_string (song.mixer.size());
            song.mixer.push_back (std::move (track));
        }

    // nothing may point past the last insert
    for (auto& c : song.channels)
        if (c.mixerTrack > inserts)
            c.mixerTrack = 0;
    for (auto& m : song.mixer)
        m.sends.erase (std::remove_if (m.sends.begin(), m.sends.end(), [inserts] (const MixerSend& s) { return s.target > inserts; }), m.sends.end());
    for (auto& p : song.playlistTracks)
        if (p.mixerTrack > inserts)
            p.mixerTrack = -1;

    auto& master = song.mixer[0].effects;
    switch (defaults.master)
    {
        case MasterChain::empty: master.clear(); break;
        case MasterChain::clipper:
            master.clear();
            master.push_back (makeEffectSlot (EffectKind::fruitySoftClipper));
            break;
        case MasterChain::lynxDefault: break; // the template's own chain
    }

    song.cleanMixer = defaults.cleanMixer;
    song.autoCrossfades = defaults.autoCrossfades;
    song.playTruncatedNotes = true; // (on in FL's Empty template and new projects)
}

NewProjectDefaults& newProjectDefaults()
{
    static NewProjectDefaults defaults;
    return defaults;
}

Song makeNewProject (int templateIndex)
{
    // round 170h2 (user decision): the default new project is FL's way — the channels, their inserts and the Master
    // limiter, with EMPTY steps (the four-on-the-floor stays in the "Basic beat" template)
    auto song = makeTemplate (templateIndex < 0 ? 0 : templateIndex);
    applyNewProjectDefaults (song, newProjectDefaults());
    return song;
}
} // namespace lynx
