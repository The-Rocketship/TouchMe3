#pragma once
#include <JuceHeader.h>
#include "MediaEngine.h"
#include "ClipGridComponent.h"
#include "PropertiesComponent.h"
#include "MonitorComponent.h"
#include "NodeEditorComponent.h"
#include "CanvasSettingsOverlay.h"
#include "OutputWindow.h"
#include "AdvancedOutputWindow.h"
#include "SettingsWindow.h"

class CustomLookAndFeel : public juce::LookAndFeel_V4
{
public:
    int getMenuWindowFlags() override
    {
        // Disable drop shadows on popup menus to prevent Windows DWM lag with OpenGL
        return juce::ComponentPeer::windowIsTemporary | juce::ComponentPeer::windowIgnoresKeyPresses;
    }
};

class MainComponent  : public juce::Component,
                       public juce::Timer,
                       public juce::MenuBarModel
                       
{
public:
    MainComponent();
    ~MainComponent() override;

    void paint(juce::Graphics&) override;
    void resized() override;

    // MenuBarModel overrides
    juce::StringArray getMenuBarNames() override;
    juce::PopupMenu getMenuForIndex(int menuIndex, const juce::String& menuName) override;
    void menuItemSelected(int menuItemId, int topLevelMenuIndex) override;

    // Timer override
    void timerCallback() override;

    void hideOutputWindows()
    {
        if (outputWindow != nullptr)
            outputWindow->setVisible(false);
        if (advancedOutputWindow != nullptr)
        {
            advancedOutputWindow->hideDeviceWindows();
            advancedOutputWindow->setVisible(false);
        }
    }

private:
    MediaEngine mediaEngine;

    std::unique_ptr<juce::MenuBarComponent> menuBar;
    
    std::unique_ptr<PropertiesComponent> propertiesPanel;
    std::unique_ptr<ClipGridComponent> clipGridPanel;
    std::unique_ptr<MonitorComponent> monitorPanel;
    std::unique_ptr<NodeEditorComponent> nodeEditor;

    juce::TooltipWindow tooltipWindow;

    // Splitter heights and widths
    int leftColumnWidth = 280;
    int rightColumnWidth = 380;
    int mainDividerY = 550; // divider for bottom node editor space

    // Resizer Bar class for mouse dragging dividers
    class ResizerBar : public juce::Component
    {
    public:
        ResizerBar(bool isVerticalBar, std::function<void(int)> onDragCallback)
            : isVertical(isVerticalBar), onDrag(onDragCallback)
        {
            setMouseCursor(isVertical ? juce::MouseCursor::LeftRightResizeCursor : juce::MouseCursor::UpDownResizeCursor);
        }

        void paint(juce::Graphics& g) override
        {
            g.fillAll(juce::Colour(0xff27272a)); // Dark border fill
        }

        void mouseDrag(const juce::MouseEvent& e) override
        {
            if (isVertical)
                onDrag(e.getScreenX());
            else
                onDrag(e.getScreenY());
        }

    private:
        bool isVertical;
        std::function<void(int)> onDrag;
    };

    std::unique_ptr<ResizerBar> leftSplitter;
    std::unique_ptr<ResizerBar> rightSplitter;
    std::unique_ptr<ResizerBar> bottomSplitter;

    juce::uint32 lastTimeMs = 0;

    std::unique_ptr<CanvasSettingsOverlay> canvasSettingsOverlay;
    std::unique_ptr<OutputWindow> outputWindow;
    std::unique_ptr<AdvancedOutputWindow> advancedOutputWindow;
    std::unique_ptr<SettingsWindow> settingsWindow;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MainComponent)
};
