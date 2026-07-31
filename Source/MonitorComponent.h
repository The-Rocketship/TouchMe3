#pragma once
#include <JuceHeader.h>
#include "MediaEngine.h"

class MonitorComponent  : public juce::Component
{
public:
    MonitorComponent(MediaEngine& engine);
    ~MonitorComponent() override;

    void paint(juce::Graphics& g) override;
    void resized() override;

    void refreshMonitors();

private:
    class SingleMonitor : public juce::Component
    {
    public:
        SingleMonitor(MediaEngine& engine, bool isPreview);
        ~SingleMonitor() override;

        // Component overrides
        void paint(juce::Graphics& g) override;

    private:
        MediaEngine& mediaEngine;
        bool isPreviewMonitor;
    };

    MediaEngine& mediaEngine;

    std::unique_ptr<SingleMonitor> compositionMonitor;
    std::unique_ptr<SingleMonitor> previewMonitor;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MonitorComponent)
};
