#include "ui/BandLabSoundsDrawer.h"

#include "engine/PluginHost.h"
#include "ui/BrowserComponent.h"
#include "ui/I18n.h"
#include "ui/Icons.h"

namespace lynx
{
namespace
{
bool isSupportedMedia (const juce::File& f)
{
    return f.hasFileExtension ("wav;aif;aiff;flac;ogg;mp3;wv;mid;midi");
}

bool isImageFile (const juce::File& f)
{
    return f.hasFileExtension ("png;jpg;jpeg;webp;bmp");
}
} // namespace

BandLabSoundsDrawer::BandLabSoundsDrawer (AudioEngine& e)
    : engine (e)
{
    searchBox.setTextToShowWhenEmpty (tr ("Buscar packs o samples..."), theme::colour::textDisabled);
    searchBox.addListener (this);
    addAndMakeVisible (searchBox);
    refreshPacks();
}

BandLabSoundsDrawer::~BandLabSoundsDrawer()
{
    engine.previewFile ({});
}

void BandLabSoundsDrawer::loadHiddenFolders()
{
    hiddenPaths.clear();
    const auto hiddenFile = PluginHost::settingsFolder().getChildFile ("hidden-browser-folders.txt");
    for (const auto& line : juce::StringArray::fromLines (hiddenFile.loadFileAsString()))
    {
        const auto trimmed = line.trim();
        if (trimmed.isNotEmpty())
            hiddenPaths.insert (trimmed);
    }
}

void BandLabSoundsDrawer::saveHiddenFolders() const
{
    const auto hiddenFile = PluginHost::settingsFolder().getChildFile ("hidden-browser-folders.txt");
    hiddenFile.getParentDirectory().createDirectory();
    juce::StringArray lines;
    for (const auto& p : hiddenPaths)
        lines.add (p);
    hiddenFile.replaceWithText (lines.joinIntoString ("\n"));
}

void BandLabSoundsDrawer::hideFolder (const juce::File& folder)
{
    const auto path = folder.getFullPathName();
    if (path.isEmpty())
        return;
    hiddenPaths.insert (path);
    expandedPaths.erase (path);
    saveHiddenFolders();

    const auto foldersFile = PluginHost::settingsFolder().getChildFile ("browser-folders.txt");
    if (foldersFile.existsAsFile())
    {
        auto lines = juce::StringArray::fromLines (foldersFile.loadFileAsString());
        lines.removeString (path, true);
        foldersFile.replaceWithText (lines.joinIntoString ("\n"));
    }

    refreshPacks();
}

void BandLabSoundsDrawer::unhideAllFolders()
{
    hiddenPaths.clear();
    saveHiddenFolders();
    refreshPacks();
}

void BandLabSoundsDrawer::refreshPacks()
{
    loadHiddenFolders();
    rootFolders.clear();
    auto tryAddRoot = [this] (const juce::File& dir)
    {
        if (! dir.isDirectory())
            return;
        const auto path = dir.getFullPathName();
        if (hiddenPaths.count (path) > 0)
            return;
        for (const auto& existing : rootFolders)
            if (existing.getFullPathName() == path)
                return;
        rootFolders.push_back (dir);
    };

    const auto foldersFile = PluginHost::settingsFolder().getChildFile ("browser-folders.txt");
    for (const auto& line : juce::StringArray::fromLines (foldersFile.loadFileAsString()))
        tryAddRoot (juce::File (line.trim()));

    tryAddRoot (juce::File::getSpecialLocation (juce::File::userMusicDirectory));
    tryAddRoot (juce::File::getSpecialLocation (juce::File::userHomeDirectory).getChildFile ("Downloads"));

    rebuildVisibleRows();
}

juce::Image BandLabSoundsDrawer::coverForFolder (const juce::File& folder)
{
    const auto key = folder.getFullPathName();
    if (const auto it = coverCache.find (key); it != coverCache.end())
        return it->second;

    juce::File chosen;
    for (const auto* preferred : { "cover.jpg", "cover.png", "folder.jpg", "folder.png", "artwork.jpg", "artwork.png" })
    {
        if (const auto f = folder.getChildFile (preferred); f.existsAsFile())
        {
            chosen = f;
            break;
        }
    }
    if (! chosen.existsAsFile())
    {
        for (const auto& entry : juce::RangedDirectoryIterator (folder, false, "*", juce::File::findFiles))
        {
            if (isImageFile (entry.getFile()))
            {
                chosen = entry.getFile();
                break;
            }
        }
    }

    juce::Image img;
    if (chosen.existsAsFile())
    {
        auto loaded = juce::ImageFileFormat::loadFrom (chosen);
        if (loaded.isValid())
            img = loaded.rescaled (96, 96, juce::Graphics::highResamplingQuality);
    }
    coverCache[key] = img;
    return img;
}

void BandLabSoundsDrawer::rebuildVisibleRows()
{
    rows.clear();
    const auto filter = searchBox.getText().trim().toLowerCase();
    const std::uint32_t palette[] = { 0xff00c4b4, 0xfff12c33, 0xff3d8bfd, 0xfff59e0b, 0xffa855f7, 0xffec4899 };

    for (std::size_t i = 0; i < rootFolders.size(); ++i)
    {
        const auto& dir = rootFolders[i];
        const auto path = dir.getFullPathName();
        const bool isExp = expandedPaths.count (path) > 0 || filter.isNotEmpty();

        int audioCount = 0, dirCount = 0;
        for (const auto& entry : juce::RangedDirectoryIterator (dir, false, "*", juce::File::findFilesAndDirectories))
        {
            if (entry.isDirectory())
                ++dirCount;
            else if (isSupportedMedia (entry.getFile()))
                ++audioCount;
        }

        const auto name = dir.getFileName();
        const bool selfMatch = filter.isEmpty() || name.toLowerCase().contains (filter);

        Item pack;
        pack.file = dir;
        pack.title = name;
        pack.subtitle = juce::String (audioCount) + " samples" + (dirCount > 0 ? "  ·  " + juce::String (dirCount) + " carpetas" : "");
        pack.depth = 0;
        pack.isFolder = true;
        pack.expanded = isExp;
        pack.fallbackColour = palette[i % 6];

        const auto startIdx = rows.size();
        rows.push_back (std::move (pack));

        if (isExp)
            appendFolderChildren (dir, 1, filter);

        if (! selfMatch && rows.size() == startIdx + 1)
            rows.pop_back();
    }

    rowTops.assign (rows.size() + 1, 0);
    for (std::size_t i = 0; i < rows.size(); ++i)
        rowTops[i + 1] = rowTops[i] + rowHeight (rows[i]);

    const int maxScroll = juce::jmax (0, rowTops.back() - juce::jmax (1, getHeight() - topBarHeight));
    scrollY = juce::jlimit (0, maxScroll, scrollY);
    repaint();
}

void BandLabSoundsDrawer::appendFolderChildren (const juce::File& folder, int depth, const juce::String& filter)
{
    std::vector<juce::File> subDirs, mediaFiles;
    for (const auto& entry : juce::RangedDirectoryIterator (folder, false, "*", juce::File::findFilesAndDirectories))
    {
        const auto f = entry.getFile();
        if (f.getFileName().startsWith ("."))
            continue;
        if (entry.isDirectory())
        {
            if (hiddenPaths.count (f.getFullPathName()) == 0)
                subDirs.push_back (f);
        }
        else if (isSupportedMedia (f))
            mediaFiles.push_back (f);
    }

    std::sort (subDirs.begin(), subDirs.end(), [] (const auto& a, const auto& b) { return a.getFileName().compareNatural (b.getFileName()) < 0; });
    std::sort (mediaFiles.begin(), mediaFiles.end(), [] (const auto& a, const auto& b) { return a.getFileName().compareNatural (b.getFileName()) < 0; });

    for (const auto& d : subDirs)
    {
        const auto path = d.getFullPathName();
        const bool isExp = expandedPaths.count (path) > 0 || filter.isNotEmpty();
        const bool selfMatch = filter.isEmpty() || d.getFileName().toLowerCase().contains (filter);

        Item item;
        item.file = d;
        item.title = d.getFileName();
        item.depth = depth;
        item.isFolder = true;
        item.expanded = isExp;
        const auto startIdx = rows.size();
        rows.push_back (std::move (item));

        if (isExp && depth < 4)
            appendFolderChildren (d, depth + 1, filter);

        if (! selfMatch && rows.size() == startIdx + 1)
            rows.pop_back();
    }

    for (const auto& f : mediaFiles)
    {
        if (filter.isNotEmpty() && ! f.getFileNameWithoutExtension().toLowerCase().contains (filter))
            continue;
        Item item;
        item.file = f;
        item.title = f.getFileNameWithoutExtension();
        item.subtitle = f.getFileExtension().toUpperCase().trimCharactersAtStart (".");
        item.depth = depth;
        item.isFolder = false;
        rows.push_back (std::move (item));
    }
}

int BandLabSoundsDrawer::maxScrollY() const
{
    if (rowTops.empty())
        return 0;
    return juce::jmax (0, rowTops.back() - juce::jmax (1, getHeight() - topBarHeight));
}

juce::Rectangle<float> BandLabSoundsDrawer::scrollTrackBounds() const
{
    const float trackW = 8.0f;
    return { (float) getWidth() - trackW - 3.0f, (float) topBarHeight + 4.0f,
             trackW, (float) juce::jmax (0, getHeight() - topBarHeight - 8) };
}

juce::Rectangle<float> BandLabSoundsDrawer::scrollThumbBounds() const
{
    const int maxS = maxScrollY();
    if (maxS <= 0 || rowTops.empty())
        return {};
    const auto track = scrollTrackBounds();
    const float viewH = (float) juce::jmax (1, getHeight() - topBarHeight);
    const float totalH = (float) rowTops.back();
    const float thumbH = juce::jlimit (28.0f, track.getHeight(), track.getHeight() * (viewH / totalH));
    const float ratio = (float) juce::jlimit (0, maxS, scrollY) / (float) maxS;
    const float thumbY = track.getY() + ratio * (track.getHeight() - thumbH);
    return { track.getX(), thumbY, track.getWidth(), thumbH };
}

void BandLabSoundsDrawer::setScrollFromThumbTop (float thumbTop)
{
    const int maxS = maxScrollY();
    if (maxS <= 0)
    {
        scrollY = 0;
        return;
    }
    const auto track = scrollTrackBounds();
    const auto thumb = scrollThumbBounds();
    const float travel = juce::jmax (1.0f, track.getHeight() - thumb.getHeight());
    const float norm = juce::jlimit (0.0f, 1.0f, (thumbTop - track.getY()) / travel);
    scrollY = juce::roundToInt (norm * (float) maxS);
}

int BandLabSoundsDrawer::rowAt (int y) const
{
    if (y < topBarHeight || rows.empty())
        return -1;
    const int offset = y - topBarHeight + scrollY;
    if (offset < 0 || offset >= rowTops.back())
        return -1;
    const auto it = std::upper_bound (rowTops.begin(), rowTops.end(), offset);
    return juce::jlimit (0, (int) rows.size() - 1, (int) (it - rowTops.begin()) - 1);
}

void BandLabSoundsDrawer::resized()
{
    searchBox.setBounds (14, 46, getWidth() - 28, 28);
}

void BandLabSoundsDrawer::paint (juce::Graphics& g)
{
    using namespace theme;
    g.fillAll (colour::bg1);
    g.setColour (colour::border);
    g.fillRect (0, 0, 1, getHeight());

    // Header: BandLab Sounds title + "+ Carpeta" pill + close button
    g.setColour (colour::accent);
    g.fillEllipse (16.0f, 16.0f, 14.0f, 14.0f);
    g.setColour (colour::textPrimary);
    g.setFont (font (type::title, true));
    g.drawText (tr ("BandLab Sounds"), 36, 11, 165, 24, juce::Justification::centredLeft, false);

    const auto addBtn = juce::Rectangle<float> ((float) getWidth() - 114.0f, 12.0f, 76.0f, 24.0f);
    fillRoundRect (g, addBtn, 12.0f, colour::bg4);
    g.setColour (colour::borderStrong);
    g.drawRoundedRectangle (addBtn.reduced (0.5f), 12.0f, 1.0f);
    g.setColour (colour::textPrimary);
    g.setFont (font (type::caption, true));
    g.drawText (tr ("+ Carpeta"), addBtn.toNearestInt(), juce::Justification::centred, false);

    const auto closeBtn = juce::Rectangle<float> ((float) getWidth() - 32.0f, 12.0f, 22.0f, 24.0f);
    g.setColour (colour::textSecondary);
    g.drawText ("X", closeBtn.toNearestInt(), juce::Justification::centred, false);

    // List clip
    scrollY = juce::jlimit (0, maxScrollY(), scrollY);
    const float rightInset = maxScrollY() > 0 ? 14.0f : 0.0f;
    juce::Graphics::ScopedSaveState state (g);
    g.reduceClipRegion (0, topBarHeight, getWidth(), getHeight() - topBarHeight);

    for (int i = 0; i < (int) rows.size(); ++i)
    {
        const int y = topBarHeight + rowTops[(std::size_t) i] - scrollY;
        const int h = rowHeight (rows[(std::size_t) i]);
        if (y > getHeight() || y + h < topBarHeight)
            continue;

        const auto& item = rows[(std::size_t) i];
        const bool isPlaying = ! item.isFolder && item.file == playingFile;
        const bool isHover = i == hoverRow;

        if (item.depth == 0)
        {
            const auto card = juce::Rectangle<float> (10.0f, (float) y + 3.0f, (float) getWidth() - 20.0f - rightInset, (float) h - 6.0f);
            fillRoundRect (g, card, radius::md, isHover ? colour::bg3 : colour::bg2);

            // Cover image (46x46)
            const auto art = juce::Rectangle<float> (card.getX() + 7.0f, card.getY() + 6.0f, 46.0f, 46.0f);
            const auto cover = coverForFolder (item.file);
            if (cover.isValid())
            {
                juce::Path clip;
                clip.addRoundedRectangle (art, radius::sm);
                juce::Graphics::ScopedSaveState artState (g);
                g.reduceClipRegion (clip);
                g.drawImage (cover, art, juce::RectanglePlacement::fillDestination);
            }
            else
            {
                fillRoundRect (g, art, radius::sm, juce::Colour (item.fallbackColour).withAlpha (0.28f));
                icons::draw (g, Icon::audioFile, art.withSizeKeepingCentre (22.0f, 22.0f), juce::Colour (item.fallbackColour));
            }

            // Title & subtitle
            const float textX = art.getRight() + 10.0f;
            const float textW = card.getRight() - textX - 34.0f;
            g.setColour (colour::textPrimary);
            g.setFont (font (type::body, true));
            g.drawFittedText (item.title, juce::Rectangle<int> ((int) textX, (int) art.getY() + 2, (int) textW, 20),
                              juce::Justification::centredLeft, 1, 0.85f);
            g.setColour (colour::textSecondary);
            g.setFont (font (type::caption, false));
            g.drawFittedText (item.subtitle, juce::Rectangle<int> ((int) textX, (int) art.getY() + 24, (int) textW, 18),
                              juce::Justification::centredLeft, 1, 0.85f);

            // Down / Right chevron arrow on right
            const auto arrowArea = juce::Rectangle<float> (card.getRight() - 30.0f, card.getCentreY() - 11.0f, 22.0f, 22.0f);
            fillRoundRect (g, arrowArea, 11.0f, item.expanded ? colour::accent.withAlpha (0.22f) : colour::bg4);
            juce::Graphics::ScopedSaveState rotState (g);
            if (! item.expanded)
                g.addTransform (juce::AffineTransform::rotation (-juce::MathConstants<float>::halfPi, arrowArea.getCentreX(), arrowArea.getCentreY()));
            icons::draw (g, Icon::chevronDown, arrowArea.withSizeKeepingCentre (13.0f, 13.0f),
                         item.expanded ? colour::accent : colour::textPrimary);
        }
        else
        {
            const float indent = 14.0f + (float) (item.depth - 1) * 14.0f;
            const auto rowRect = juce::Rectangle<float> (indent, (float) y + 1.0f, (float) getWidth() - indent - 10.0f - rightInset, (float) h - 2.0f);
            if (isPlaying || isHover)
                fillRoundRect (g, rowRect, radius::sm, isPlaying ? colour::accentMuted : colour::bg3);

            const auto iconArea = juce::Rectangle<float> (rowRect.getX() + 6.0f, rowRect.getCentreY() - 7.0f, 14.0f, 14.0f);
            if (item.isFolder)
            {
                juce::Graphics::ScopedSaveState rotState (g);
                if (! item.expanded)
                    g.addTransform (juce::AffineTransform::rotation (-juce::MathConstants<float>::halfPi, iconArea.getCentreX(), iconArea.getCentreY()));
                icons::draw (g, Icon::chevronDown, iconArea, colour::textSecondary);
            }
            else
            {
                icons::draw (g, isPlaying ? Icon::play : Icon::audioFile, iconArea, isPlaying ? colour::accent : colour::audioClip);
            }

            g.setColour (isPlaying ? colour::textPrimary : (item.isFolder ? colour::textPrimary : colour::textSecondary.brighter (0.25f)));
            g.setFont (font (type::smallSize, item.isFolder || isPlaying));
            g.drawFittedText (item.title, juce::Rectangle<int> ((int) iconArea.getRight() + 6, y, (int) (rowRect.getRight() - iconArea.getRight() - 42.0f), h),
                              juce::Justification::centredLeft, 1, 0.85f);

            if (! item.isFolder)
            {
                g.setColour (colour::textDisabled);
                g.setFont (font (10.0f, true));
                g.drawText (item.subtitle, (int) rowRect.getRight() - 36, y, 30, h, juce::Justification::centredRight, false);
            }
        }
    }

    if (maxScrollY() > 0)
    {
        const auto track = scrollTrackBounds();
        const auto thumb = scrollThumbBounds();
        fillRoundRect (g, track, track.getWidth() * 0.5f, colour::bg0.withAlpha (0.55f));
        fillRoundRect (g, thumb, thumb.getWidth() * 0.5f,
                       (draggingScroll || hoverScroll) ? colour::textSecondary : colour::borderStrong.brighter (0.25f));
    }
}

void BandLabSoundsDrawer::mouseMove (const juce::MouseEvent& e)
{
    const bool overScroll = maxScrollY() > 0 && scrollTrackBounds().expanded (3.0f, 0.0f).contains (e.position);
    const int r = overScroll ? -1 : rowAt (e.y);
    if (r != hoverRow || overScroll != hoverScroll)
    {
        hoverRow = r;
        hoverScroll = overScroll;
        repaint();
    }
}

void BandLabSoundsDrawer::mouseExit (const juce::MouseEvent&)
{
    if (hoverRow != -1 || hoverScroll)
    {
        hoverRow = -1;
        hoverScroll = false;
        repaint();
    }
}

void BandLabSoundsDrawer::mouseDown (const juce::MouseEvent& e)
{
    downPos = e.getPosition();
    dragStarted = false;
    draggingScroll = false;

    if (maxScrollY() > 0 && scrollTrackBounds().expanded (3.0f, 0.0f).contains (e.position))
    {
        draggingScroll = true;
        const auto thumb = scrollThumbBounds();
        if (e.position.y >= thumb.getY() && e.position.y <= thumb.getBottom())
            scrollDragGrabOffset = e.position.y - thumb.getY();
        else
        {
            scrollDragGrabOffset = thumb.getHeight() * 0.5f;
            setScrollFromThumbTop (e.position.y - scrollDragGrabOffset);
        }
        repaint();
        return;
    }

    const auto addBtn = juce::Rectangle<int> (getWidth() - 114, 12, 76, 24);
    if (addBtn.contains (e.getPosition()))
    {
        addFolderDialog();
        return;
    }

    const auto closeBtn = juce::Rectangle<int> (getWidth() - 32, 12, 22, 24);
    if (closeBtn.contains (e.getPosition()))
    {
        if (onClose)
            onClose();
        return;
    }

    const int r = rowAt (e.y);
    if (e.mods.isPopupMenu())
    {
        juce::PopupMenu menu;
        if (r >= 0 && r < (int) rows.size())
        {
            const auto& item = rows[(std::size_t) r];
            const auto targetFile = item.file;
            const bool isFolder = item.isFolder;
            if (isFolder)
                menu.addItem (tr ("Ocultar carpeta"), [this, targetFile] { hideFolder (targetFile); });

            menu.addItem (tr ("Mover a la papelera"), [this, targetFile, isFolder] {
                if (! isFolder && playingFile == targetFile)
                {
                    playingFile = {};
                    engine.previewFile ({});
                }
                if (targetFile.moveToTrash())
                {
                    if (isFolder)
                    {
                        const auto p = targetFile.getFullPathName();
                        expandedPaths.erase (p);
                        hiddenPaths.erase (p);
                        saveHiddenFolders();
                        const auto foldersFile = PluginHost::settingsFolder().getChildFile ("browser-folders.txt");
                        if (foldersFile.existsAsFile())
                        {
                            auto lines = juce::StringArray::fromLines (foldersFile.loadFileAsString());
                            lines.removeString (p, true);
                            foldersFile.replaceWithText (lines.joinIntoString ("\n"));
                        }
                    }
                    refreshPacks();
                }
            });
        }
        if (! hiddenPaths.empty())
        {
            if (menu.getNumItems() > 0)
                menu.addSeparator();
            menu.addItem (tr ("Mostrar carpetas ocultas") + " (" + juce::String ((int) hiddenPaths.size()) + ")",
                          [this] { unhideAllFolders(); });
        }
        if (menu.getNumItems() > 0)
            menu.showMenuAsync (juce::PopupMenu::Options().withMousePosition());
        return;
    }

    if (r < 0 || r >= (int) rows.size())
        return;

    const auto& item = rows[(std::size_t) r];
    if (item.isFolder)
    {
        const auto path = item.file.getFullPathName();
        if (expandedPaths.count (path) > 0)
            expandedPaths.erase (path);
        else
            expandedPaths.insert (path);
        rebuildVisibleRows();
    }
    else
    {
        playingFile = item.file;
        engine.previewFile (item.file);
        repaint();
    }
}

void BandLabSoundsDrawer::mouseDoubleClick (const juce::MouseEvent& e)
{
    const int r = rowAt (e.y);
    if (r < 0 || r >= (int) rows.size())
        return;
    const auto& item = rows[(std::size_t) r];
    if (! item.isFolder && onInsertSample)
        onInsertSample (item.file);
}

void BandLabSoundsDrawer::mouseUp (const juce::MouseEvent&)
{
    if (draggingScroll)
    {
        draggingScroll = false;
        repaint();
    }
}

void BandLabSoundsDrawer::mouseDrag (const juce::MouseEvent& e)
{
    if (draggingScroll)
    {
        setScrollFromThumbTop (e.position.y - scrollDragGrabOffset);
        repaint();
        return;
    }

    if (dragStarted || e.getPosition().getDistanceFrom (downPos) < 6)
        return;
    const int r = rowAt (downPos.y);
    if (r < 0 || r >= (int) rows.size())
        return;
    const auto& item = rows[(std::size_t) r];
    if (item.isFolder)
        return;

    if (auto* container = juce::DragAndDropContainer::findParentDragContainerFor (this))
    {
        dragStarted = true;
        juce::var desc (juce::String (BrowserComponent::dragDescription) + "|" + item.file.getFullPathName());

        const auto label = item.file.getFileNameWithoutExtension();
        const int w = 220, h = 38;
        juce::Image pill (juce::Image::ARGB, w, h, true);
        {
            juce::Graphics g (pill);
            const auto r = juce::Rectangle<float> (1.0f, 1.0f, (float) (w - 2), (float) (h - 2));
            g.setColour (juce::Colour (0xe6112a2b));
            g.fillRoundedRectangle (r, 7.0f);
            g.setColour (juce::Colour (0xff00c4b4));
            g.drawRoundedRectangle (r, 7.0f, 1.4f);

            // Mini waveform bars icon
            const float bars[] { 0.35f, 0.75f, 1.0f, 0.55f, 0.85f, 0.4f };
            for (int i = 0; i < 6; ++i)
            {
                const float bh = 14.0f * bars[i];
                g.fillRoundedRectangle (11.0f + (float) i * 3.2f, ((float) h - bh) * 0.5f, 2.0f, bh, 1.0f);
            }

            g.setColour (juce::Colour (0xfff4f6f8));
            g.setFont (juce::FontOptions (12.5f, juce::Font::bold));
            g.drawText (label, 36, 0, w - 46, h, juce::Justification::centredLeft, true);
        }

        container->startDragging (desc, this, juce::ScaledImage (pill), true, nullptr);
    }
}

void BandLabSoundsDrawer::mouseWheelMove (const juce::MouseEvent&, const juce::MouseWheelDetails& wheel)
{
    if (rowTops.empty())
        return;
    scrollY = juce::jlimit (0, maxScrollY(), scrollY - juce::roundToInt (wheel.deltaY * 72.0f));
    repaint();
}

void BandLabSoundsDrawer::addFolderDialog()
{
    chooser = std::make_unique<juce::FileChooser> (tr ("Seleccionar carpetas de samples / beats"),
                                                   juce::File::getSpecialLocation (juce::File::userMusicDirectory));
    chooser->launchAsync (juce::FileBrowserComponent::openMode
                              | juce::FileBrowserComponent::canSelectDirectories
                              | juce::FileBrowserComponent::canSelectMultipleItems,
                          [this] (const juce::FileChooser& fc) {
                              const auto results = fc.getResults();
                              if (results.isEmpty())
                                  return;

                              const auto file = PluginHost::settingsFolder().getChildFile ("browser-folders.txt");
                              file.getParentDirectory().createDirectory();
                              auto lines = juce::StringArray::fromLines (file.loadFileAsString());
                              lines.removeEmptyStrings();

                              bool anyAdded = false;
                              for (const auto& dir : results)
                              {
                                  if (! dir.isDirectory())
                                      continue;
                                  const auto addedPath = dir.getFullPathName();
                                  hiddenPaths.erase (addedPath);
                                  lines.addIfNotAlreadyThere (addedPath);
                                  expandedPaths.insert (addedPath);
                                  anyAdded = true;
                              }

                              if (! anyAdded)
                                  return;

                              saveHiddenFolders();
                              file.replaceWithText (lines.joinIntoString ("\n"));
                              refreshPacks();
                          });
}
} // namespace lynx
