#pragma once
#include <JuceHeader.h>
#include "NodeGraph.h"
#include <memory>
#include <functional>
#include "EnvelopeEditor.h"
#include "ColourEnvelopeEditor.h"
#include "MappingManager.h"
#include "SettingsManager.h"

class NodePropertiesComponent : public juce::Component, public juce::ChangeListener, public juce::ComboBox::Listener, public juce::TextEditor::Listener
{
public:
    NodePropertiesComponent();
    ~NodePropertiesComponent() override = default;

    void setNode(BaseNode* node);
    void setClipDuration(double duration) { currentClipDuration = duration; }
    void updateParameterVisuals(double time);

    void setMappingManager(MappingManager* mm) { mappingManager = mm; }
    void setSettingsManager(SettingsManager* sm) { settingsManager = sm; }

    void paint(juce::Graphics& g) override;
    void paintOverChildren(juce::Graphics& g) override;
    void mouseDown(const juce::MouseEvent& event) override;
    void mouseMove(const juce::MouseEvent& event) override;
    void mouseExit(const juce::MouseEvent& event) override;
    void resized() override;

    std::function<void()> onNodeChanged;

private:
    void rebuildUI();

    juce::String currentHoverPath;
    juce::Point<int> currentMousePos;

    SettingsManager* settingsManager = nullptr;
    MappingManager* mappingManager = nullptr;
    BaseNode* currentNode = nullptr;
    double currentClipDuration = 10.0;

    std::vector<std::unique_ptr<juce::Slider>> dynamicSliders;
    std::vector<std::unique_ptr<juce::Label>> dynamicLabels;
    std::vector<std::unique_ptr<juce::TextButton>> dynamicButtons;
    std::vector<Parameter*> dynamicSliderParams;
    std::unique_ptr<juce::ColourSelector> colourSelector;
    ColourParameter* currentColourParam = nullptr;

    juce::ComboBox colourModeSelector;
    juce::Label colourModeLabel;
    juce::TextEditor hexEditor;
    juce::Label hexLabel;
    int currentColourMode = 0; // 0=RGBA, 1=HSB, 2=Hex

    juce::ComboBox compositeBlendModeSelector;
    juce::Label compositeBlendModeLabel;

    juce::ComboBox noiseTypeSelector;
    juce::Label noiseTypeLabel;

    // ShaderToy editor
    std::unique_ptr<juce::TextEditor> shaderEditor;
    juce::TextButton compileButton  { "Compile" };
    juce::Label      shaderErrorLabel;
    ShaderToyNode*   currentShaderNode = nullptr;

    void changeListenerCallback(juce::ChangeBroadcaster* source) override;
    void comboBoxChanged(juce::ComboBox* comboBoxThatHasChanged) override;
    void textEditorTextChanged(juce::TextEditor& editor) override;
    
    void updateColourFromHex();

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(NodePropertiesComponent)
};
