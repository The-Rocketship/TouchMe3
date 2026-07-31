#pragma once
#include <JuceHeader.h>
#include "MediaEngine.h"

class OutputWindow : public juce::DocumentWindow
{
public:
    class CompositionView : public juce::Component
    {
    public:
        CompositionView(MediaEngine& engine) : mediaEngine(engine)
        {
        }

        ~CompositionView() override = default;

        void paint(juce::Graphics& g) override
        {
            g.fillAll(juce::Colours::black);
            g.drawImage(mediaEngine.getCompositionFrame(), getLocalBounds().toFloat());
        }

    private:
        MediaEngine& mediaEngine;
    };

    OutputWindow(const juce::String& name, MediaEngine& engine)
        : DocumentWindow(name, juce::Colours::black, DocumentWindow::allButtons)
    {
        // Borderless style flags
        setUsingNativeTitleBar(true);
        setResizable(true, false);

        compView = std::make_unique<CompositionView>(engine);
        setContentNonOwned(compView.get(), true);

        // Center on screen and set default size
        setBounds(100, 100, 1280, 720);
    }

    ~OutputWindow() override
    {
        clearContentComponent();
        compView.reset();
    }

    void closeButtonPressed() override
    {
        setVisible(false);
    }

private:
    std::unique_ptr<CompositionView> compView;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(OutputWindow)
};
