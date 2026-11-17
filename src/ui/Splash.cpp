#include "ui/Splash.h"

#include "core/StartupSound.h"
#include "engine/PluginHost.h"
#include "ui/I18n.h"
#include "ui/SplashData.h"
#include "ui/Theme.h"

namespace lynx
{
namespace
{
double clamp01 (double x) { return juce::jlimit (0.0, 1.0, x); }
double easeOutCubic (double u) { return 1.0 - std::pow (1.0 - clamp01 (u), 3.0); }

double easeOutBack (double u)
{
    u = clamp01 (u);
    constexpr double c1 = 1.5, c3 = c1 + 1.0;
    return 1.0 + c3 * std::pow (u - 1.0, 3.0) + c1 * std::pow (u - 1.0, 2.0);
}

// a ball dropped on the floor: 0 = up, 1 = resting; it touches the floor at u = 1/2.75, 2/2.75, 2.5/2.75, 1
double easeOutBounce (double u)
{
    u = clamp01 (u);
    constexpr double n1 = 7.5625, d1 = 2.75;

    if (u < 1.0 / d1)
        return n1 * u * u;
    auto arc = [] (double v, double centre, double floor) { return n1 * (v - centre) * (v - centre) + floor; };
    if (u < 2.0 / d1)
        return arc (u, 1.5 / d1, 0.75);
    if (u < 2.5 / d1)
        return arc (u, 2.25 / d1, 0.9375);
    return arc (u, 2.625 / d1, 0.984375);
}

// the drop lasts so that its first touch is the sound's landing "pop"
constexpr double dropTime = startup::impactTime * 2.75;
constexpr double touches[] = { dropTime / 2.75, dropTime * 2.0 / 2.75, dropTime * 2.5 / 2.75 };
constexpr double touchStrength[] = { 1.0, 0.35, 0.12 };

double impactAt (double t)
{
    double k = 0.0;
    for (int i = 0; i < 3; ++i)
        if (t >= touches[i])
            k += touchStrength[i] * std::exp (-(t - touches[i]) / 0.07);
    return k;
}
} // namespace

// --- options --------------------------------------------------------------------------------------

StartupOptions StartupOptions::load()
{
    StartupOptions options;
    juce::StringArray lines;
    lines.addLines (PluginHost::settingsFolder().getChildFile ("ui-options.txt").loadFileAsString());

    for (const auto& line : lines)
    {
        const auto key = line.upToFirstOccurrenceOf ("=", false, false).trim();
        const auto on = line.fromFirstOccurrenceOf ("=", false, false).trim() != "0";

        if (key == "splash")
            options.splash = on;
        else if (key == "startupSound")
            options.sound = on;
    }

    return options;
}

std::shared_ptr<const SampleData> makeStartupSample()
{
    auto sound = startup::makeSound (48000.0);
    auto sample = std::make_shared<SampleData>();
    sample->left = std::move (sound.left);
    sample->right = std::move (sound.right);
    sample->sampleRate = sound.sampleRate;
    return sample;
}

// --- the splash -----------------------------------------------------------------------------------

SplashScreen::SplashScreen()
    : pepper (juce::ImageCache::getFromMemory (splashdata::pepperPng, (int) sizeof (splashdata::pepperPng)))
{
    setOpaque (false);
    setSize (width, height);
    status = tr ("Starting the audio engine...");
}

SplashScreen::~SplashScreen()
{
    stopTimer();
}

void SplashScreen::showOnDesktop()
{
    if (const auto* display = juce::Desktop::getInstance().getDisplays().getPrimaryDisplay())
        setCentrePosition (display->userArea.getCentre());

    addToDesktop (0); // no title bar, no taskbar entry
    setAlwaysOnTop (true);
    setVisible (true);
    toFront (false);
    startTimerHz (60);
}

void SplashScreen::startClock()
{
    clockStartMs = juce::Time::getMillisecondCounterHiRes();
    time = 0.0;
}

double SplashScreen::remainingDuration (double sinceProcessStart, double landing)
{
    return juce::jmax (landing, minimumDuration - juce::jmax (0.0, sinceProcessStart));
}

double SplashScreen::getElapsed() const
{
    return clockStartMs < 0.0 ? 0.0 : (juce::Time::getMillisecondCounterHiRes() - clockStartMs) / 1000.0;
}

void SplashScreen::setStatus (const juce::String& text)
{
    status = text;
    repaint();
}

void SplashScreen::finish (std::function<void()> whenGone)
{
    onGone = std::move (whenGone);
    fadeStartMs = juce::Time::getMillisecondCounterHiRes();

    if (! isTimerRunning())
        startTimerHz (60);
}

void SplashScreen::setTimeForSnapshot (double seconds)
{
    time = seconds;
    repaint();
}

void SplashScreen::timerCallback()
{
    time = getElapsed();

    if (fadeStartMs >= 0.0)
    {
        const auto f = (juce::Time::getMillisecondCounterHiRes() - fadeStartMs) / 1000.0 / fadeOutTime;
        setAlpha ((float) (1.0 - clamp01 (f)));

        if (f >= 1.0)
        {
            stopTimer();
            setVisible (false);
            removeFromDesktop();

            if (auto gone = std::move (onGone))
                juce::MessageManager::callAsync (std::move (gone)); // may delete this splash
            return;
        }
    }

    repaint();
}

void SplashScreen::paint (juce::Graphics& g)
{
    if (! pepper.isValid())
        return;

    const auto t = time;
    const auto alpha = (float) clamp01 (t / 0.25);
    const auto scale = 0.92f + 0.08f * (float) easeOutCubic (t / 0.45);

    const auto bounds = getLocalBounds().toFloat().reduced (24.0f);
    const auto imgW = (float) pepper.getWidth();
    const auto imgH = (float) pepper.getHeight();
    const auto fit = juce::jmin (bounds.getWidth() / juce::jmax (1.0f, imgW),
                                 bounds.getHeight() / juce::jmax (1.0f, imgH)) * scale;

    const auto drawW = imgW * fit;
    const auto drawH = imgH * fit;
    const auto dest = juce::Rectangle<float> (drawW, drawH).withCentre (bounds.getCentre());

    g.setImageResamplingQuality (juce::Graphics::highResamplingQuality);
    g.setOpacity (alpha);
    g.drawImage (pepper, dest, juce::RectanglePlacement::stretchToFit);
    g.setOpacity (1.0f);
}
} // namespace lynx
