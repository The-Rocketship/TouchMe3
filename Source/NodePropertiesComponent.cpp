#include "NodePropertiesComponent.h"

NodePropertiesComponent::NodePropertiesComponent()
{
    colourModeLabel.setText("Colour Mode", juce::dontSendNotification);
    colourModeLabel.setColour(juce::Label::textColourId, juce::Colour(0xffa1a1aa));
    addChildComponent(colourModeLabel);

    colourModeSelector.addItem("RGBA", 1);
    colourModeSelector.addItem("HSBA", 2);
    colourModeSelector.addItem("Hex", 3);
    colourModeSelector.setSelectedId(1, juce::dontSendNotification);
    colourModeSelector.addListener(this);
    addChildComponent(colourModeSelector);

    hexLabel.setText("Hex Code", juce::dontSendNotification);
    hexLabel.setColour(juce::Label::textColourId, juce::Colour(0xffa1a1aa));
    addChildComponent(hexLabel);

    hexEditor.setMultiLine(false);
    hexEditor.setReturnKeyStartsNewLine(false);
    hexEditor.setReadOnly(false);
    hexEditor.setScrollbarsShown(false);
    hexEditor.setCaretVisible(true);
    hexEditor.setPopupMenuEnabled(true);
    hexEditor.setColour(juce::TextEditor::textColourId, juce::Colours::white);
    hexEditor.setColour(juce::TextEditor::backgroundColourId, juce::Colour(0xff222224));
    hexEditor.addListener(this);
    addChildComponent(hexEditor);

    compositeBlendModeLabel.setText("Blend Mode", juce::dontSendNotification);
    compositeBlendModeLabel.setColour(juce::Label::textColourId, juce::Colour(0xffa1a1aa));
    addChildComponent(compositeBlendModeLabel);

    compositeBlendModeSelector.addItem("Normal", 1);
    compositeBlendModeSelector.addItem("Add", 2);
    compositeBlendModeSelector.addItem("Multiply", 3);
    compositeBlendModeSelector.addItem("Screen", 4);
    compositeBlendModeSelector.addListener(this);
    addChildComponent(compositeBlendModeSelector);

    noiseTypeLabel.setText("Type", juce::dontSendNotification);
    noiseTypeLabel.setColour(juce::Label::textColourId, juce::Colour(0xffa1a1aa));
    addChildComponent(noiseTypeLabel);

    noiseTypeSelector.addItem("Simplex 2D",  1);
    noiseTypeSelector.addItem("Simplex 3D",  2);
    noiseTypeSelector.addItem("Simplex 4D",  3);
    noiseTypeSelector.addItem("Perlin 2D",   4);
    noiseTypeSelector.addItem("Perlin 3D",   5);
    noiseTypeSelector.addItem("Perlin 4D",   6);
    noiseTypeSelector.addItem("Random",      7);
    noiseTypeSelector.addItem("Sparse",      8);
    noiseTypeSelector.addItem("Hermite",     9);
    noiseTypeSelector.addItem("Harmonic Summation", 10);
    noiseTypeSelector.addItem("Random (GPU)", 11);
    noiseTypeSelector.addItem("Alligator",   12);
    noiseTypeSelector.addListener(this);
    addChildComponent(noiseTypeSelector);

    shaderEditor = std::make_unique<juce::TextEditor>();
    shaderEditor->setMultiLine(true);
    shaderEditor->setReturnKeyStartsNewLine(true);
    shaderEditor->setFont(juce::Font(juce::Font::getDefaultMonospacedFontName(), 14.0f, juce::Font::plain));
    shaderEditor->setColour(juce::TextEditor::backgroundColourId, juce::Colour(0xff1e1e1e));
    shaderEditor->setColour(juce::TextEditor::textColourId, juce::Colour(0xffd4d4d4));
    addChildComponent(*shaderEditor);

    compileButton.onClick = [this]() {
        if (currentShaderNode) {
            currentShaderNode->shaderSource = shaderEditor->getText();
            currentShaderNode->applyShader();
            shaderErrorLabel.setText(currentShaderNode->lastError, juce::dontSendNotification);
            if (onNodeChanged) onNodeChanged();
        }
    };
    compileButton.setColour(juce::TextButton::buttonColourId, juce::Colour(0xff00f0a8));
    compileButton.setColour(juce::TextButton::textColourOnId, juce::Colours::black);
    compileButton.setColour(juce::TextButton::textColourOffId, juce::Colours::black);
    addChildComponent(compileButton);

    shaderErrorLabel.setColour(juce::Label::textColourId, juce::Colours::red);
    shaderErrorLabel.setFont(12.0f);
    addChildComponent(shaderErrorLabel);
}

void NodePropertiesComponent::setNode(BaseNode* node)
{
    if (currentNode == node) return;
    currentNode = node;
    rebuildUI();
}

void NodePropertiesComponent::rebuildUI()
{
    dynamicSliders.clear();
    dynamicLabels.clear();
    dynamicButtons.clear();
    dynamicSliderParams.clear();
    if (colourSelector)
    {
        colourSelector->removeChangeListener(this);
        colourSelector.reset();
    }
    
    colourModeLabel.setVisible(false);
    colourModeSelector.setVisible(false);
    hexLabel.setVisible(false);
    hexEditor.setVisible(false);
    compositeBlendModeLabel.setVisible(false);
    compositeBlendModeSelector.setVisible(false);
    noiseTypeLabel.setVisible(false);
    noiseTypeSelector.setVisible(false);

    shaderEditor->setVisible(false);
    compileButton.setVisible(false);
    shaderErrorLabel.setVisible(false);
    currentShaderNode = nullptr;

    auto addSlider = [this](const juce::String& name, float min, float max, float step, float value, std::function<void(float)> onChange) {
        auto label = std::make_unique<juce::Label>("", name);
        label->setColour(juce::Label::textColourId, juce::Colour(0xffa1a1aa));
        addAndMakeVisible(*label);

        auto slider = std::make_unique<juce::Slider>(juce::Slider::LinearHorizontal, juce::Slider::TextBoxRight);
        slider->setTextBoxStyle(juce::Slider::TextBoxRight, false, 50, 20);
        slider->setColour(juce::Slider::textBoxTextColourId, juce::Colours::white);
        slider->setColour(juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
        slider->setRange(min, max, step);
        slider->setValue(value);
        slider->setColour(juce::Slider::trackColourId, juce::Colour(0xff00f0a8));
        slider->setColour(juce::Slider::backgroundColourId, juce::Colour(0xff222224));
        slider->setColour(juce::Slider::thumbColourId, juce::Colours::white);
        addAndMakeVisible(*slider);

        slider->onValueChange = [slider = slider.get(), onChange, this]() {
            onChange((float)slider->getValue());
            if (onNodeChanged) onNodeChanged();
        };

        dynamicLabels.push_back(std::move(label));
        dynamicSliders.push_back(std::move(slider));
        dynamicButtons.push_back(nullptr); // Empty slot for non-params
        dynamicSliderParams.push_back(nullptr);
    };

    auto addParam = [this](const juce::String& name, float min, float max, float step, Parameter& param) {
        auto label = std::make_unique<juce::Label>("", name);
        label->setColour(juce::Label::textColourId, juce::Colour(0xffa1a1aa));
        addAndMakeVisible(*label);

        auto slider = std::make_unique<juce::Slider>(juce::Slider::LinearHorizontal, juce::Slider::TextBoxRight);
        slider->setTextBoxStyle(juce::Slider::TextBoxRight, false, 50, 20);
        slider->setColour(juce::Slider::textBoxTextColourId, juce::Colours::white);
        slider->setColour(juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
        slider->setRange(min, max, step);
        slider->setValue(param.baseValue);
        slider->setColour(juce::Slider::trackColourId, juce::Colour(0xff00f0a8));
        slider->setColour(juce::Slider::backgroundColourId, juce::Colour(0xff222224));
        slider->setColour(juce::Slider::thumbColourId, juce::Colours::white);
        
        if (currentNode) {
            juce::String path = "/node/" + juce::String(currentNode->id) + "/" + name;
            slider->getProperties().set("mappingPath", path);
        }
        addAndMakeVisible(*slider);

        slider->onValueChange = [slider = slider.get(), &param, this]() {
            param.baseValue = (float)slider->getValue();
            // Invalidate noise cache if this is a noise node
            if (auto* noiseNode = dynamic_cast<NoiseNode*>(currentNode))
                noiseNode->dirty = true;
            if (onNodeChanged) onNodeChanged();
        };

        auto btn = std::make_unique<juce::TextButton>(juce::String(juce::CharPointer_UTF8("\xe2\x9a\x99"))); // cog emoji
        btn->setColour(juce::TextButton::buttonColourId, juce::Colour(0xff27272a));
        btn->setColour(juce::TextButton::textColourOffId, param.envelope.isEnabled ? juce::Colour(0xff00f0a8) : juce::Colours::grey);
        addAndMakeVisible(*btn);

        btn->onClick = [&param, name, min, max, this]() {
            new EnvelopeEditorWindow(param, name, currentClipDuration, min, max);
        };

        dynamicLabels.push_back(std::move(label));
        dynamicSliders.push_back(std::move(slider));
        dynamicButtons.push_back(std::move(btn));
        dynamicSliderParams.push_back(&param);
    };

    auto addColourParam = [this](const juce::String& name, ColourParameter& param) {
        auto label = std::make_unique<juce::Label>("", name);
        label->setColour(juce::Label::textColourId, juce::Colour(0xffa1a1aa));
        addAndMakeVisible(*label);

        auto btn = std::make_unique<juce::TextButton>(juce::String(juce::CharPointer_UTF8("\xe2\x9a\x99"))); // cog emoji
        btn->setColour(juce::TextButton::buttonColourId, juce::Colour(0xff27272a));
        btn->setColour(juce::TextButton::textColourOffId, param.envelope.isEnabled ? juce::Colour(0xff00f0a8) : juce::Colours::grey);
        addAndMakeVisible(*btn);

        btn->onClick = [&param, name, this]() {
            new ColourEnvelopeEditorWindow(param, name, currentClipDuration);
        };

        currentColourParam = &param;
        colourSelector = std::make_unique<juce::ColourSelector>(juce::ColourSelector::showAlphaChannel | juce::ColourSelector::showColourAtTop | juce::ColourSelector::showSliders | juce::ColourSelector::showColourspace);
        colourSelector->setCurrentColour(param.baseValue);
        addAndMakeVisible(*colourSelector);
        colourSelector->addChangeListener(this);

        colourModeLabel.setVisible(true);
        colourModeSelector.setVisible(true);
        hexLabel.setVisible(true);
        hexEditor.setVisible(true);
        hexEditor.setText(param.baseValue.toDisplayString(true).substring(2), juce::dontSendNotification);

        dynamicLabels.push_back(std::move(label));
        dynamicSliders.push_back(nullptr); // no dummy slider
        dynamicButtons.push_back(std::move(btn));
        dynamicSliderParams.push_back(nullptr); // Don't try to update value visually via a normal parameter pointer
    };

    if (currentNode)
    {
        addSlider("Res X", 1.0f, 4096.0f, 1.0f, currentNode->resolutionX, [this](float v) { currentNode->resolutionX = (int)v; });
        addSlider("Res Y", 1.0f, 4096.0f, 1.0f, currentNode->resolutionY, [this](float v) { currentNode->resolutionY = (int)v; });
    }

    auto* scNode = dynamic_cast<SolidColourNode*>(currentNode);
    auto* lineNode = dynamic_cast<LineNode*>(currentNode);
    auto* noiseNode = dynamic_cast<NoiseNode*>(currentNode);
    auto* compNode = dynamic_cast<CompositeNode*>(currentNode);

    if (scNode)
    {
        addColourParam("Colour", scNode->colour);
    }
    else if (lineNode)
    {
        addColourParam("Colour", lineNode->colour);
    }
    else if (noiseNode)
    {
        noiseTypeLabel.setVisible(true);
        noiseTypeSelector.setVisible(true);
        noiseTypeSelector.setSelectedId((int)noiseNode->noiseType + 1, juce::dontSendNotification);
    }
    else if (compNode)
    {
        compositeBlendModeLabel.setVisible(true);
        compositeBlendModeSelector.setVisible(true);
        compositeBlendModeSelector.setSelectedId(compNode->blendMode + 1, juce::dontSendNotification);
    }

    if (currentNode)
    {
        auto params = currentNode->getParameters();
        for (const auto& p : params)
        {
            float min = 0.0f, max = 1.0f, step = 0.01f;
            if (p.first == "Seed") { max = 1000.0f; step = 1.0f; }
            else if (p.first == "Period") { min = 0.01f; max = 10.0f; }
            else if (p.first == "Harmonics") { min = 1.0f; max = 16.0f; step = 1.0f; }
            else if (p.first == "Harmonic Spread" || p.first == "Exponent") { min = 0.1f; max = 8.0f; }
            else if (p.first == "Amplitude") { min = 0.0f; max = 2.0f; }
            else if (p.first == "Offset") { min = -1.0f; max = 2.0f; }
            else if (p.first == "Thickness") { min = 1.0f; max = 100.0f; step = 1.0f; }
            else if (p.first == "Intensity") { min = 0.0f; max = 10.0f; step = 0.1f; }
            
            addParam(p.first, min, max, step, *p.second);
        }
    }
    else if (auto* shaderNode = dynamic_cast<ShaderToyNode*>(currentNode))
    {
        currentShaderNode = shaderNode;
        shaderEditor->setVisible(true);
        shaderEditor->setText(shaderNode->shaderSource, false);
        compileButton.setVisible(true);
        shaderErrorLabel.setVisible(true);
        shaderErrorLabel.setText(shaderNode->lastError, juce::dontSendNotification);
    }

    resized();
    repaint();
}

void NodePropertiesComponent::paint(juce::Graphics& g)
{
    g.fillAll(settingsManager ? settingsManager->getTheme().panelBackground : juce::Colour(0xff18181b));

    g.setColour(settingsManager ? settingsManager->getTheme().border : juce::Colour(0xff3f3f46));
    g.fillRect((float)getWidth() - 1.0f, 0.0f, 1.0f, (float)getHeight());

    if (!currentNode)
    {
        g.setColour(settingsManager ? settingsManager->getTheme().border : juce::Colour(0xffa1a1aa));
        g.setFont(14.0f);
        g.drawText("No Node Selected", getLocalBounds(), juce::Justification::centred);
        return;
    }

    g.setColour(settingsManager ? settingsManager->getTheme().text : juce::Colours::white);
    g.setFont(juce::Font(16.0f, juce::Font::bold));
    g.drawText(currentNode->name + " Properties", 10, 10, getWidth() - 20, 20, juce::Justification::left);
}

void NodePropertiesComponent::paintOverChildren(juce::Graphics& g)
{
    if (mappingManager && mappingManager->getEditMode() != MappingManager::EditMode::None)
    {
        // Darken everything slightly
        g.fillAll(juce::Colour(0x66000000));
        
        juce::Colour overlayColour = (mappingManager->getEditMode() == MappingManager::EditMode::MIDI) ? 
            juce::Colours::lime.withAlpha(0.4f) : juce::Colours::cyan.withAlpha(0.4f);

        for (size_t i = 0; i < dynamicSliders.size(); ++i)
        {
            if (dynamicSliders[i])
            {
                juce::String path = "/node/" + juce::String(currentNode->id) + "/" + dynamicLabels[i]->getText();
                bool isSelected = (mappingManager->getLearningPath() == path);
                
                auto bounds = dynamicSliders[i]->getBounds();
                g.setColour(isSelected ? juce::Colour(0x88ff0000) : overlayColour);
                g.fillRect(bounds);
                
                g.setColour(isSelected ? juce::Colours::red : juce::Colours::white);
                g.drawRect(bounds, 2.0f);
            }
        }
        
        if (mappingManager->getEditMode() == MappingManager::EditMode::OSC && currentHoverPath.isNotEmpty())
        {
            g.setFont(14.0f);
            float textWidth = g.getCurrentFont().getStringWidth(currentHoverPath) + 16.0f;
            juce::Rectangle<float> tooltipBounds(currentMousePos.x + 10.0f, currentMousePos.y + 10.0f, textWidth, 24.0f);
            
            // Adjust if it goes offscreen
            if (tooltipBounds.getRight() > getWidth())
                tooltipBounds.setX(getWidth() - tooltipBounds.getWidth() - 2.0f);
            if (tooltipBounds.getBottom() > getHeight())
                tooltipBounds.setY(getHeight() - tooltipBounds.getHeight() - 2.0f);
            
            g.setColour(juce::Colours::black.withAlpha(0.9f));
            g.fillRect(tooltipBounds);
            
            g.setColour(juce::Colours::white);
            g.drawRect(tooltipBounds, 1.0f);
            
            g.drawText(currentHoverPath, tooltipBounds, juce::Justification::centred, false);
        }
    }
}

void NodePropertiesComponent::mouseDown(const juce::MouseEvent& event)
{
    if (mappingManager && mappingManager->getEditMode() != MappingManager::EditMode::None && currentNode)
    {
        auto pos = event.getEventRelativeTo(this).getPosition();
        juce::File::getCurrentWorkingDirectory().getChildFile("click_debug.txt").appendText("NodePropertiesComponent::mouseDown at pos: " + pos.toString() + "\n");
        for (size_t i = 0; i < dynamicSliders.size(); ++i)
        {
            if (dynamicSliders[i] && dynamicSliders[i]->getBounds().contains(pos))
            {
                juce::String path = "/node/" + juce::String(currentNode->id) + "/" + dynamicLabels[i]->getText();
                juce::File::getCurrentWorkingDirectory().getChildFile("click_debug.txt").appendText("Clicked Node path: " + path + "\n");
                mappingManager->setLearningPath(path);
                repaint();
                return;
            }
        }
        juce::File::getCurrentWorkingDirectory().getChildFile("click_debug.txt").appendText("NodePropertiesComponent: No slider hit at pos: " + pos.toString() + "\n");
    }
    juce::Component::mouseDown(event);
}

void NodePropertiesComponent::mouseMove(const juce::MouseEvent& event)
{
    if (mappingManager && mappingManager->getEditMode() == MappingManager::EditMode::OSC)
    {
        auto pos = event.getEventRelativeTo(this).getPosition();
        currentMousePos = pos;
        
        for (size_t i = 0; i < dynamicSliders.size(); ++i)
        {
            if (dynamicSliders[i] && dynamicSliders[i]->getBounds().contains(pos))
            {
                if (dynamicSliders[i]->getProperties().contains("mappingPath"))
                {
                    juce::String newPath = dynamicSliders[i]->getProperties()["mappingPath"].toString();
                    if (newPath != currentHoverPath)
                    {
                        currentHoverPath = newPath;
                        repaint();
                    }
                    return;
                }
            }
        }
    }
    
    if (currentHoverPath.isNotEmpty())
    {
        currentHoverPath.clear();
        repaint();
    }
}

void NodePropertiesComponent::mouseExit(const juce::MouseEvent& event)
{
    if (currentHoverPath.isNotEmpty())
    {
        currentHoverPath.clear();
        repaint();
    }
}

void NodePropertiesComponent::resized()
{
    int y = 50;
    
    if (colourSelector)
    {
        colourSelector->setBounds(10, y, getWidth() - 20, 200);
        y += 210;

        colourModeLabel.setBounds(10, y, 80, 20);
        colourModeSelector.setBounds(90, y, getWidth() - 100, 20);
        y += 30;
    }

    if (hexEditor.isVisible())
    {
        hexLabel.setBounds(10, y, 80, 20);
        hexEditor.setBounds(90, y, getWidth() - 100, 20);
        y += 30;
    }

    if (noiseTypeSelector.isVisible())
    {
        noiseTypeLabel.setBounds(10, y, 80, 20);
        noiseTypeSelector.setBounds(90, y, getWidth() - 100, 20);
        y += 30;
    }

    if (compositeBlendModeSelector.isVisible())
    {
        compositeBlendModeLabel.setBounds(10, y, 80, 20);
        compositeBlendModeSelector.setBounds(90, y, getWidth() - 100, 20);
        y += 30;
    }

    if (currentShaderNode)
    {
        shaderEditor->setBounds(10, y, getWidth() - 20, 300);
        y += 310;
        compileButton.setBounds(10, y, 100, 30);
        y += 40;
        shaderErrorLabel.setBounds(10, y, getWidth() - 20, 60);
        y += 70;
    }

    for (size_t i = 0; i < dynamicSliders.size(); ++i)
    {
        dynamicLabels[i]->setBounds(10, y, 80, 20);
        
        if (dynamicButtons[i])
        {
            if (dynamicSliders[i])
                dynamicSliders[i]->setBounds(90, y, getWidth() - 130, 20);
            dynamicButtons[i]->setBounds(getWidth() - 30, y, 20, 20);
        }
        else if (dynamicSliders[i])
        {
            dynamicSliders[i]->setBounds(90, y, getWidth() - 100, 20);
        }
        
        y += 30;
    }
}

void NodePropertiesComponent::updateParameterVisuals(double time)
{
    bool isEditMode = mappingManager && mappingManager->getEditMode() != MappingManager::EditMode::None;
    bool isOscMode = mappingManager && mappingManager->getEditMode() == MappingManager::EditMode::OSC;
    for (size_t i = 0; i < dynamicSliders.size(); ++i)
    {
        if (dynamicSliders[i])
        {
            dynamicSliders[i]->setInterceptsMouseClicks(!isEditMode, !isEditMode);
        }

        if (auto* param = dynamicSliderParams[i])
        {
            if (dynamicSliders[i] && !dynamicSliders[i]->isMouseButtonDown())
            {
                if (param->envelope.isEnabled)
                {
                    dynamicSliders[i]->setValue(param->eval(time), juce::dontSendNotification);
                }
                else if (std::abs(dynamicSliders[i]->getValue() - param->baseValue) > 0.0001f)
                {
                    // Sync with external MIDI/OSC changes
                    dynamicSliders[i]->setValue(param->baseValue, juce::dontSendNotification);
                }
            }
        }
    }
}

void NodePropertiesComponent::changeListenerCallback(juce::ChangeBroadcaster* source)
{
    if (colourSelector && source == colourSelector.get() && currentColourParam)
    {
        currentColourParam->baseValue = colourSelector->getCurrentColour();
        hexEditor.setText(colourSelector->getCurrentColour().toDisplayString(true).substring(2), juce::dontSendNotification);
        if (onNodeChanged) onNodeChanged();
    }
}

void NodePropertiesComponent::comboBoxChanged(juce::ComboBox* comboBoxThatHasChanged)
{
    if (comboBoxThatHasChanged == &colourModeSelector)
    {
        currentColourMode = colourModeSelector.getSelectedId() - 1;
        rebuildUI();
    }
    else if (comboBoxThatHasChanged == &compositeBlendModeSelector)
    {
        if (auto* compNode = dynamic_cast<CompositeNode*>(currentNode))
        {
            compNode->blendMode = compositeBlendModeSelector.getSelectedId() - 1;
            if (onNodeChanged) onNodeChanged();
        }
    }
    else if (comboBoxThatHasChanged == &noiseTypeSelector)
    {
        if (auto* noiseNode = dynamic_cast<NoiseNode*>(currentNode))
        {
            noiseNode->noiseType = (NoiseType)(noiseTypeSelector.getSelectedId() - 1);
            noiseNode->dirty = true;
            if (onNodeChanged) onNodeChanged();
        }
    }
}

void NodePropertiesComponent::textEditorTextChanged(juce::TextEditor& editor)
{
    if (&editor == &hexEditor)
    {
        updateColourFromHex();
    }
}

void NodePropertiesComponent::updateColourFromHex()
{
    if (currentColourParam)
    {
        auto text = hexEditor.getText();
        if (text.isNotEmpty() && text.length() <= 8)
        {
            auto newColour = juce::Colour::fromString(text);
            currentColourParam->baseValue = newColour;
            if (colourSelector)
                colourSelector->setCurrentColour(newColour, juce::dontSendNotification);
            if (onNodeChanged) onNodeChanged();
        }
    }
}
