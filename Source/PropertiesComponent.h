#pragma once
#include <JuceHeader.h>
#include "MediaEngine.h"
#include "EnvelopeEditor.h"

class PropertiesComponent  : public juce::Component
{
public:
    PropertiesComponent(MediaEngine& engine);
    ~PropertiesComponent() override = default;

    void paint(juce::Graphics& g) override;
    void paintOverChildren(juce::Graphics& g) override;
    void mouseDown(const juce::MouseEvent& event) override;
    void mouseMove(const juce::MouseEvent& event) override;
    void mouseExit(const juce::MouseEvent& event) override;
    void resized() override;

    // Load active clip state details into sliders/toggles
    void updateDetails(const ClipState& clip);
    void updateTimelinePosition(double progressNormalized);
    void updateParameterVisuals(double time);
    void updateClipFromSliders(ClipState& clip);
    void updateTransformFromSliders(ClipState& clip);

    // Callbacks to notify parent of changes
    std::function<void()> onPropertiesChanged;
    std::function<void(const juce::File&)> onFileLoaded;

private:
    juce::String currentHoverPath;
    juce::Point<int> currentMousePos;

    MediaEngine& mediaEngine;

    juce::Label titleLabel;
    juce::Label clipNameLabel;
    juce::TextButton loadMediaButton;
    std::unique_ptr<juce::FileChooser> fileChooser;
    
    // Transport
    juce::Label transportTitle;
    juce::TextButton playPauseButton;
    juce::TextButton loopButton;
    juce::Slider speedSlider;
    juce::Label speedLabel;
    juce::Slider positionSlider;
    juce::Label positionLabel;

    // Video Size / Opacity
    juce::Label videoSizeTitle;
    juce::Slider opacitySlider;
    juce::Label opacityLabel;
    juce::Slider widthSlider;
    juce::Label widthLabel;
    juce::Slider heightSlider;
    juce::Label heightLabel;

    // Transform
    juce::Label transformTitle;
    juce::Slider posXSlider;
    juce::Label posXLabel;
    juce::Slider posYSlider;
    juce::Label posYLabel;

    // Scale
    juce::Label scaleTitle;
    juce::Slider scaleSlider;
    juce::Label scaleLabel;
    juce::Slider scaleXSlider;
    juce::Label scaleXLabel;
    juce::Slider scaleYSlider;
    juce::Label scaleYLabel;

    // Rotation
    juce::Label rotationTitle;
    juce::Slider rotXSlider;
    juce::Label rotXLabel;
    juce::Slider rotYSlider;
    juce::Label rotYLabel;
    juce::Slider rotZSlider;
    juce::Label rotZLabel;

    // Anchor
    juce::Label anchorTitle;
    juce::Slider anchorXSlider;
    juce::Label anchorXLabel;
    juce::Slider anchorYSlider;
    juce::Label anchorYLabel;

    // Master FX
    juce::Slider fxVhsSlider;
    juce::Label fxVhsLabel;
    juce::Slider fxRgbShiftSlider;
    juce::Label fxRgbShiftLabel;
    juce::Slider fxScanlinesSlider;
    juce::Label fxScanlinesLabel;

    std::vector<std::unique_ptr<juce::TextButton>> cogButtons;
    std::map<juce::Slider*, juce::TextButton*> sliderToCogButton;
    std::map<juce::Slider*, std::function<Parameter*()>> sliderToParam;

    bool isUpdatingFromCode = false;
    ClipState* currentClip = nullptr;

    // Collapsing state
    bool transportCollapsed = false;
    bool specsCollapsed = false;
    bool transformCollapsed = false;
    bool scaleCollapsed = false;
    bool rotationCollapsed = false;
    bool anchorCollapsed = false;
    bool masterFxCollapsed = false;

    struct CollapsibleHeader : public juce::Component
    {
        juce::String title;
        bool& collapsedState;
        std::function<void()> onToggle;

        CollapsibleHeader (const juce::String& titleText, bool& collapsedRef, std::function<void()> onToggleCallback)
            : title (titleText), collapsedState (collapsedRef), onToggle (onToggleCallback)
        {
            setMouseCursor (juce::MouseCursor::PointingHandCursor);
        }

        void paint (juce::Graphics& g) override
        {
            g.fillAll (juce::Colour (0xff18181b));
            g.setColour (juce::Colour (0xff27272a));
            g.drawRect (getLocalBounds(), 1);

            g.setColour (juce::Colours::white);
            g.setFont (juce::Font (11.0f, juce::Font::bold));
            juce::String arrow = collapsedState ? "  +  " : "  -  ";
            g.drawText (arrow + title, 4, 0, getWidth() - 8, getHeight(), juce::Justification::centredLeft);
        }

        void mouseDown (const juce::MouseEvent&) override
        {
            collapsedState = !collapsedState;
            repaint();
            if (onToggle)
                onToggle();
        }
    };

    std::unique_ptr<CollapsibleHeader> transportHeader;
    std::unique_ptr<CollapsibleHeader> specsHeader;
    std::unique_ptr<CollapsibleHeader> transformHeader;
    std::unique_ptr<CollapsibleHeader> scaleHeader;
    std::unique_ptr<CollapsibleHeader> rotationHeader;
    std::unique_ptr<CollapsibleHeader> anchorHeader;
    std::unique_ptr<CollapsibleHeader> masterFxHeader;

    juce::Viewport viewport;
    juce::Component content;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PropertiesComponent)
};
