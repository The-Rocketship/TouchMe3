#pragma once
#include <JuceHeader.h>
#include "NodeGraph.h"
#include "NodePropertiesComponent.h"
#include "SettingsManager.h"

class NodeEditorComponent : public juce::Component
{
public:
    NodeEditorComponent();
    ~NodeEditorComponent() override = default;

    void setGraph(std::shared_ptr<NodeGraph> newGraph);
    void setClipDuration(double duration) { propertiesPanel.setClipDuration(duration); }
    void updateParameterVisuals(double time) { propertiesPanel.updateParameterVisuals(time); }
    void setMappingManager(MappingManager* mm) { propertiesPanel.setMappingManager(mm); }
    void setSettingsManager(SettingsManager* sm) { settingsManager = sm; propertiesPanel.setSettingsManager(sm); }
    void triggerPropertiesRepaint() { propertiesPanel.repaint(); }

    std::function<void(std::shared_ptr<NodeGraph>)> onGraphChanged;

    void paint(juce::Graphics& g) override;
    void resized() override;
    void mouseDown(const juce::MouseEvent& event) override;
    void mouseDrag(const juce::MouseEvent& event) override;
    void mouseUp(const juce::MouseEvent& event) override;
    void mouseDoubleClick(const juce::MouseEvent& event) override;

private:
    SettingsManager* settingsManager = nullptr;
    std::shared_ptr<NodeGraph> currentGraph;
    BaseNode* draggingNode = nullptr;
    BaseNode* selectedNode = nullptr;
    int dragOffsetX = 0;
    int dragOffsetY = 0;

    float panX = 0.0f;
    float panY = 0.0f;
    bool isPanning = false;
    int panStartX = 0;
    int panStartY = 0;

    BaseNode* drawingLinkFromNode = nullptr;
    int drawingLinkFromPin = 0;
    juce::Point<float> drawingLinkEndPos;

    NodePropertiesComponent propertiesPanel;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(NodeEditorComponent)
};
