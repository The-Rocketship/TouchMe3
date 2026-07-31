#include "MonitorComponent.h"

MonitorComponent::MonitorComponent(MediaEngine& engine)
    : mediaEngine(engine)
{
    compositionMonitor = std::make_unique<SingleMonitor>(mediaEngine, false);
    previewMonitor = std::make_unique<SingleMonitor>(mediaEngine, true);

    addAndMakeVisible(*compositionMonitor);
    addAndMakeVisible(*previewMonitor);
}

MonitorComponent::~MonitorComponent()
{
    compositionMonitor.reset();
    previewMonitor.reset();
}

void MonitorComponent::paint(juce::Graphics& g)
{
    g.fillAll(juce::Colour(0xff09090b)); // Sleek zinc dark background
}

void MonitorComponent::resized()
{
    auto area = getLocalBounds();
    int halfHeight = area.getHeight() / 2;
    
    // Top is Composition
    compositionMonitor->setBounds(area.removeFromTop(halfHeight).reduced(4));
    
    // Bottom is Preview
    previewMonitor->setBounds(area.reduced(4));
}

void MonitorComponent::refreshMonitors()
{
    compositionMonitor->repaint();
    previewMonitor->repaint();
}

// ==========================================
// Single Monitor Component Implementation
// ==========================================

MonitorComponent::SingleMonitor::SingleMonitor(MediaEngine& engine, bool isPreview)
    : mediaEngine(engine), isPreviewMonitor(isPreview)
{
}

MonitorComponent::SingleMonitor::~SingleMonitor()
{
}

void MonitorComponent::SingleMonitor::paint(juce::Graphics& g)
{
    auto w = (float)getWidth();
    auto h = (float)getHeight();

    // 1. Dark background
    g.fillAll(juce::Colour(0xff0e0e10));

    // 2. Render Clips (respecting border margins)
    juce::Rectangle<int> clipArea(3, 26, getWidth() - 6, getHeight() - 29);
    
    g.saveState();
    g.reduceClipRegion(clipArea);
    
    // Position graphics context inside the clipArea to align rendering
    g.addTransform(juce::AffineTransform::translation((float)clipArea.getX(), (float)clipArea.getY()));

    if (isPreviewMonitor)
    {
        auto& clip = mediaEngine.getPreviewClip();
        if (clip.isLoaded)
        {
            mediaEngine.renderClip(g, clip, (float)clipArea.getWidth(), (float)clipArea.getHeight(), 1.0f);
        }
        else
        {
            g.restoreState();
            g.setColour(juce::Colours::white.withAlpha(0.2f));
            g.setFont(12.0f);
            g.drawText("Preview Monitor (Click Grid Slot)", 0, 0, getWidth(), getHeight(), juce::Justification::centred);
            g.saveState(); // dummy to balance restoreState
        }
    }
    else
    {
        // Composition Monitor: draw cached composition frame
        auto& compFrame = mediaEngine.getCompositionFrame();
        if (!compFrame.isNull())
        {
            g.drawImage(compFrame, 0, 0, clipArea.getWidth(), clipArea.getHeight(),
                        0, 0, compFrame.getWidth(), compFrame.getHeight());
        }
        else
        {
            g.restoreState();
            g.setColour(juce::Colours::white.withAlpha(0.2f));
            g.setFont(12.0f);
            g.drawText("Composition Monitor (Double Click Grid Slot to Trigger)", 0, 0, getWidth(), getHeight(), juce::Justification::centred);
            g.saveState(); // dummy to balance restoreState
        }
    }

    g.restoreState();

    // 3. Draw Header Overlay
    g.setColour(juce::Colour(0xbb000000));
    g.fillRect(0, 0, getWidth(), 24);

    g.setColour(isPreviewMonitor ? juce::Colour(0xff10ffd0) : juce::Colour(0xff00f0a8)); // Accent colors
    g.drawRect(0, 0, getWidth(), getHeight(), 1);

    g.setColour(juce::Colours::white);
    g.setFont(juce::Font(11.0f, juce::Font::bold));
    g.drawText(isPreviewMonitor ? "PREVIEW MONITOR" : "COMPOSITION MONITOR", 8, 4, getWidth() - 16, 16, juce::Justification::left);
}
