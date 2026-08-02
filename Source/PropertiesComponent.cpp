#include "PropertiesComponent.h"

PropertiesComponent::PropertiesComponent(MediaEngine& engine)
    : mediaEngine(engine)
{
    addAndMakeVisible(viewport);
    viewport.setViewedComponent(&content, false);
    viewport.setScrollBarsShown(true, false, true, false); // vertical only, auto-hide
    viewport.setScrollBarThickness(6);
    content.addMouseListener(this, false);

    // Setup Look & Feel or Colors
    auto makeLabel = [this](juce::Label& lbl, const juce::String& text, float size, bool bold = false) {
        content.addAndMakeVisible(lbl);
        lbl.setText(text, juce::dontSendNotification);
        lbl.setFont(juce::Font(size, bold ? juce::Font::bold : juce::Font::plain));
        lbl.setColour(juce::Label::textColourId, juce::Colours::white);
    };

    auto makeSlider = [this](juce::Slider& sld, juce::Label& lbl, const juce::String& labelText, double min, double max, double step, double val, std::function<Parameter*()> getParam = nullptr) {
        content.addAndMakeVisible(sld);
        sld.setRange(min, max, step);
        sld.setValue(val);
        sld.setDoubleClickReturnValue(true, val);
        sld.setSliderStyle(juce::Slider::LinearHorizontal);
        sld.setTextBoxStyle(juce::Slider::TextBoxRight, false, 50, 16);
        sld.setColour(juce::Slider::trackColourId, juce::Colour(0xff00f0a8)); // neon green
        sld.setColour(juce::Slider::backgroundColourId, juce::Colour(0xff27272a));
        sld.setColour(juce::Slider::thumbColourId, juce::Colours::white);
        
        sld.onValueChange = [this] {
            if (onPropertiesChanged)
                onPropertiesChanged();
        };

        content.addAndMakeVisible(lbl);
        lbl.setText(labelText, juce::dontSendNotification);
        lbl.setFont(10.0f);
        lbl.setColour(juce::Label::textColourId, juce::Colour(0xffa1a1aa));

        if (getParam)
        {
            auto btn = std::make_unique<juce::TextButton>(juce::String(juce::CharPointer_UTF8("\xe2\x9a\x99")));
            btn->setColour(juce::TextButton::buttonColourId, juce::Colour(0xff27272a));
            btn->setColour(juce::TextButton::textColourOffId, juce::Colours::grey);
            content.addAndMakeVisible(*btn);
            
            // Pass the clip duration if we have a currentClip
            auto getClipDuration = [this]() -> double {
                return currentClip ? currentClip->transport.duration : 10.0;
            };

            float minVal = (float)sld.getMinimum();
            float maxVal = (float)sld.getMaximum();
            btn->onClick = [getParam, labelText, getClipDuration, minVal, maxVal]() {
                if (auto* p = getParam()) {
                    new EnvelopeEditorWindow(*p, labelText, getClipDuration(), minVal, maxVal);
                }
            };
            sliderToCogButton[&sld] = btn.get();
            sliderToParam[&sld] = getParam;
            cogButtons.push_back(std::move(btn));
        }
    };

    // Main Headers
    makeLabel(titleLabel, "Clip Properties", 15.0f, true);
    titleLabel.setColour(juce::Label::textColourId, juce::Colour(0xff10ffd0)); // mint green header
    makeLabel(clipNameLabel, "- No Clip Selected -", 11.0f, false);
    clipNameLabel.setColour(juce::Label::textColourId, juce::Colour(0xffa1a1aa));

    content.addAndMakeVisible(loadMediaButton);
    loadMediaButton.setButtonText("Load Media...");
    loadMediaButton.setColour(juce::TextButton::buttonColourId, juce::Colour(0xff2d2d30));
    loadMediaButton.setColour(juce::TextButton::textColourOffId, juce::Colours::white);
    loadMediaButton.onClick = [this] {
        if (!currentClip) return;
        
        fileChooser = std::make_unique<juce::FileChooser>("Select a media file...",
                                                          juce::File::getSpecialLocation(juce::File::userHomeDirectory),
                                                          "*.mp4;*.mov;*.avi;*.png;*.jpg;*.jpeg");
        
        auto chooserFlags = juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles;
        fileChooser->launchAsync(chooserFlags, [this](const juce::FileChooser& fc) {
            auto result = fc.getResult();
            if (result.existsAsFile() && onFileLoaded)
            {
                onFileLoaded(result);
            }
        });
    };

    // Section headers configuration
    auto onToggle = [this] {
        resized();
        repaint();
    };

    transportHeader = std::make_unique<CollapsibleHeader>("Transport", transportCollapsed, onToggle);
    content.addAndMakeVisible(*transportHeader);

    specsHeader = std::make_unique<CollapsibleHeader>("Video Specs", specsCollapsed, onToggle);
    content.addAndMakeVisible(*specsHeader);

    transformHeader = std::make_unique<CollapsibleHeader>("Transform", transformCollapsed, onToggle);
    content.addAndMakeVisible(*transformHeader);

    scaleHeader = std::make_unique<CollapsibleHeader>("Scale", scaleCollapsed, onToggle);
    content.addAndMakeVisible(*scaleHeader);

    rotationHeader = std::make_unique<CollapsibleHeader>("Rotation", rotationCollapsed, onToggle);
    content.addAndMakeVisible(*rotationHeader);

    anchorHeader = std::make_unique<CollapsibleHeader>("Anchor", anchorCollapsed, onToggle);
    content.addAndMakeVisible(*anchorHeader);

    masterFxHeader = std::make_unique<CollapsibleHeader>("Master FX", masterFxCollapsed, onToggle);
    content.addAndMakeVisible(*masterFxHeader);

    // Section 1: Transport
    content.addAndMakeVisible(playPauseButton);
    playPauseButton.setButtonText("Play");
    playPauseButton.setClickingTogglesState(true);
    playPauseButton.onClick = [this] {
        if (isUpdatingFromCode) return;
        if (currentClip != nullptr) {
            currentClip->transport.isPlaying = playPauseButton.getToggleState();
        }
    };

    content.addAndMakeVisible(loopButton);
    loopButton.setButtonText("Loop");
    loopButton.setClickingTogglesState(true);
    loopButton.onClick = [this] {
        if (isUpdatingFromCode) return;
        if (currentClip != nullptr) {
            currentClip->transport.loop = loopButton.getToggleState();
        }
    };

    makeSlider(speedSlider, speedLabel, "Speed", 0.0, 10.0, 0.1, 1.0);
    makeSlider(positionSlider, positionLabel, "Timeline Position", 0.0, 10.0, 0.01, 0.0);
    positionSlider.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);

    // Section 2: Video Size & Opacity
    makeSlider(opacitySlider, opacityLabel, "Opacity", 0.0, 1.0, 0.01, 1.0, [this]() { return currentClip ? &currentClip->transform.opacity : nullptr; });
    makeSlider(widthSlider, widthLabel, "Width", 100.0, 3840.0, 1.0, 1920.0, nullptr);
    makeSlider(heightSlider, heightLabel, "Height", 100.0, 2160.0, 1.0, 1080.0, nullptr);

    // Section 3: Transform (Position)
    makeSlider(posXSlider, posXLabel, "Position X", -1000.0, 1000.0, 1.0, 0.0, [this]() { return currentClip ? &currentClip->transform.posX : nullptr; });
    makeSlider(posYSlider, posYLabel, "Position Y", -1000.0, 1000.0, 1.0, 0.0, [this]() { return currentClip ? &currentClip->transform.posY : nullptr; });

    // Section 4: Scale
    makeSlider(scaleSlider, scaleLabel, "Scale (Uniform)", 0.1, 5.0, 0.01, 1.0, [this]() { return currentClip ? &currentClip->transform.scale : nullptr; });
    makeSlider(scaleXSlider, scaleXLabel, "Scale X", 0.1, 5.0, 0.01, 1.0, [this]() { return currentClip ? &currentClip->transform.scaleX : nullptr; });
    makeSlider(scaleYSlider, scaleYLabel, "Scale Y", 0.1, 5.0, 0.01, 1.0, [this]() { return currentClip ? &currentClip->transform.scaleY : nullptr; });

    // Section 5: Rotation
    makeSlider(rotXSlider, rotXLabel, "Rotation X", -180.0, 180.0, 1.0, 0.0, [this]() { return currentClip ? &currentClip->transform.rotationX : nullptr; });
    makeSlider(rotYSlider, rotYLabel, "Rotation Y", -180.0, 180.0, 1.0, 0.0, [this]() { return currentClip ? &currentClip->transform.rotationY : nullptr; });
    makeSlider(rotZSlider, rotZLabel, "Rotation Z", -180.0, 180.0, 1.0, 0.0, [this]() { return currentClip ? &currentClip->transform.rotationZ : nullptr; });

    // Section 6: Anchor
    makeSlider(anchorXSlider, anchorXLabel, "Anchor X", 0.0, 1.0, 0.01, 0.5, [this]() { return currentClip ? &currentClip->transform.anchorX : nullptr; });
    makeSlider(anchorYSlider, anchorYLabel, "Anchor Y", 0.0, 1.0, 0.01, 0.5, [this]() { return currentClip ? &currentClip->transform.anchorY : nullptr; });

    // Master FX sliders do not use envelopes for now, just direct mapping
    makeSlider(fxVhsSlider, fxVhsLabel, "VHS Glitch", 0.0, 1.0, 0.01, 0.0, nullptr);
    makeSlider(fxRgbShiftSlider, fxRgbShiftLabel, "RGB Shift", 0.0, 1.0, 0.01, 0.0, nullptr);
    makeSlider(fxScanlinesSlider, fxScanlinesLabel, "Scanlines", 0.0, 1.0, 0.01, 0.0, nullptr);

    fxVhsSlider.onValueChange = [this] { mediaEngine.fxVhs.store(fxVhsSlider.getValue()); };
    fxRgbShiftSlider.onValueChange = [this] { mediaEngine.fxRgbShift.store(fxRgbShiftSlider.getValue()); };
    fxScanlinesSlider.onValueChange = [this] { mediaEngine.fxScanlines.store(fxScanlinesSlider.getValue()); };
}

void PropertiesComponent::paint(juce::Graphics& g)
{
    g.fillAll(mediaEngine.settingsManager.getTheme().panelBackground);

    g.setColour(mediaEngine.settingsManager.getTheme().border);
    g.drawRect(getLocalBounds(), 1);

    // Draw thin divider lines
    g.setColour(juce::Colour(0xff18181b));
}

void PropertiesComponent::paintOverChildren(juce::Graphics& g)
{
    if (mediaEngine.mappingManager.getEditMode() != MappingManager::EditMode::None)
    {
        g.fillAll(juce::Colour(0x66000000));
        
        juce::Colour overlayColour = (mediaEngine.mappingManager.getEditMode() == MappingManager::EditMode::MIDI) ? 
            juce::Colours::lime.withAlpha(0.4f) : juce::Colours::cyan.withAlpha(0.4f);

        for (auto* comp : content.getChildren())
        {
            if (auto* slider = dynamic_cast<juce::Slider*>(comp))
            {
                if (slider->getProperties().contains("mappingPath"))
                {
                    juce::String path = slider->getProperties()["mappingPath"].toString();
                    bool isSelected = (mediaEngine.mappingManager.getLearningPath() == path);
                    
                    auto bounds = slider->getBoundsInParent().getIntersection(viewport.getViewArea());
                    bounds = bounds.translated(0, -viewport.getViewPositionY());
                    
                    g.setColour(isSelected ? juce::Colour(0x88ff0000) : overlayColour);
                    g.fillRect(bounds);
                    
                    g.setColour(isSelected ? juce::Colours::red : juce::Colours::white);
                    g.drawRect(bounds, 2.0f);
                }
            }
        }
        
        if (mediaEngine.mappingManager.getEditMode() == MappingManager::EditMode::OSC && currentHoverPath.isNotEmpty())
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

void PropertiesComponent::mouseDown(const juce::MouseEvent& event)
{
    if (mediaEngine.mappingManager.getEditMode() != MappingManager::EditMode::None)
    {
        auto pos = event.getEventRelativeTo(this).getPosition();
        pos.y += viewport.getViewPositionY(); // account for scroll
        
        juce::String debugStr = "PropertiesComponent::mouseDown at pos: " + pos.toString() + "\n";
        juce::File::getCurrentWorkingDirectory().getChildFile("click_debug.txt").appendText(debugStr);
        
        for (auto* comp : content.getChildren())
        {
            if (auto* slider = dynamic_cast<juce::Slider*>(comp))
            {
                if (slider->getBounds().contains(pos))
                {
                    if (slider->getProperties().contains("mappingPath"))
                    {
                        juce::String path = slider->getProperties()["mappingPath"].toString();
                        juce::File::getCurrentWorkingDirectory().getChildFile("click_debug.txt").appendText("Clicked mappingPath: " + path + "\n");
                        mediaEngine.mappingManager.setLearningPath(path);
                        repaint();
                        return;
                    }
                    else
                    {
                        juce::File::getCurrentWorkingDirectory().getChildFile("click_debug.txt").appendText("Slider clicked but has no mappingPath\n");
                    }
                }
            }
        }
    }
    juce::Component::mouseDown(event);
}

void PropertiesComponent::mouseMove(const juce::MouseEvent& event)
{
    if (mediaEngine.mappingManager.getEditMode() == MappingManager::EditMode::OSC)
    {
        auto pos = event.getEventRelativeTo(this).getPosition();
        currentMousePos = pos;
        pos.y += viewport.getViewPositionY(); // account for scroll
        
        for (auto* comp : content.getChildren())
        {
            if (auto* slider = dynamic_cast<juce::Slider*>(comp))
            {
                if (slider->getBounds().contains(pos) && slider->getProperties().contains("mappingPath"))
                {
                    juce::String newPath = slider->getProperties()["mappingPath"].toString();
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

void PropertiesComponent::mouseExit(const juce::MouseEvent& event)
{
    if (currentHoverPath.isNotEmpty())
    {
        currentHoverPath.clear();
        repaint();
    }
}

void PropertiesComponent::resized()
{
    viewport.setBounds(getLocalBounds());
    
    int scrollWidth = viewport.getMaximumVisibleWidth();
    
    // We will do layout in a temporary rect of fixed width, and accumulate height
    int y = 10;
    int margin = 10;
    int w = scrollWidth - margin * 2;
    
    titleLabel.setBounds(margin, y, w, 20);
    y += 20;
    
    clipNameLabel.setBounds(margin, y, w, 18);
    y += 18 + 4; // spacing

    loadMediaButton.setBounds(margin, y, w, 24);
    y += 24 + 10; // spacing

    auto layoutSectionHeader = [&](std::unique_ptr<CollapsibleHeader>& header, const juce::String& name) {
        header->setBounds(margin, y, w, 22);
        y += 22 + 6;
    };

    auto layoutSlider = [&](juce::Slider& sld, juce::Label& lbl, bool visible) {
        sld.setVisible(visible);
        lbl.setVisible(visible);
        auto it = sliderToCogButton.find(&sld);
        if (it != sliderToCogButton.end() && it->second)
        {
            it->second->setVisible(visible);
        }

        if (visible)
        {
            lbl.setBounds(margin, y, w, 12);
            y += 12;
            
            if (it != sliderToCogButton.end() && it->second)
            {
                sld.setBounds(margin, y, w - 30, 20);
                it->second->setBounds(margin + w - 20, y, 20, 20);
            }
            else
            {
                sld.setBounds(margin, y, w, 20);
            }
            
            y += 20 + 4; // spacing
        }
    };

    // 1. Transport Section
    layoutSectionHeader(transportHeader, "Transport");
    playPauseButton.setVisible(!transportCollapsed);
    loopButton.setVisible(!transportCollapsed);
    if (!transportCollapsed)
    {
        playPauseButton.setBounds(margin, y, w / 2 - 4, 20);
        loopButton.setBounds(margin + w / 2 + 4, y, w / 2 - 4, 20);
        y += 20 + 6;
    }
    layoutSlider(speedSlider, speedLabel, !transportCollapsed);
    layoutSlider(positionSlider, positionLabel, !transportCollapsed);

    y += 4; // section spacing

    // 2. Video Specs Section
    layoutSectionHeader(specsHeader, "Video Specs");
    layoutSlider(opacitySlider, opacityLabel, !specsCollapsed);
    layoutSlider(widthSlider, widthLabel, !specsCollapsed);
    layoutSlider(heightSlider, heightLabel, !specsCollapsed);

    y += 4;

    // 3. Transform Section
    layoutSectionHeader(transformHeader, "Transform");
    layoutSlider(posXSlider, posXLabel, !transformCollapsed);
    layoutSlider(posYSlider, posYLabel, !transformCollapsed);

    y += 4;

    // 4. Scale Section
    layoutSectionHeader(scaleHeader, "Scale");
    layoutSlider(scaleSlider, scaleLabel, !scaleCollapsed);
    layoutSlider(scaleXSlider, scaleXLabel, !scaleCollapsed);
    layoutSlider(scaleYSlider, scaleYLabel, !scaleCollapsed);

    y += 4;

    // 5. Rotation Section
    layoutSectionHeader(rotationHeader, "Rotation");
    layoutSlider(rotXSlider, rotXLabel, !rotationCollapsed);
    layoutSlider(rotYSlider, rotYLabel, !rotationCollapsed);
    layoutSlider(rotZSlider, rotZLabel, !rotationCollapsed);

    y += 4;

    // 6. Anchor Section
    layoutSectionHeader(anchorHeader, "Anchor");
    layoutSlider(anchorXSlider, anchorXLabel, !anchorCollapsed);
    layoutSlider(anchorYSlider, anchorYLabel, !anchorCollapsed);

    y += 4;

    // 7. Master FX Section
    layoutSectionHeader(masterFxHeader, "- Master FX");
    layoutSlider(fxVhsSlider, fxVhsLabel, !masterFxCollapsed);
    layoutSlider(fxRgbShiftSlider, fxRgbShiftLabel, !masterFxCollapsed);
    layoutSlider(fxScanlinesSlider, fxScanlinesLabel, !masterFxCollapsed);

    y += 10; // bottom margin
    content.setBounds(0, 0, scrollWidth, y);
}

void PropertiesComponent::updateDetails(const ClipState& clip)
{
    isUpdatingFromCode = true;
    currentClip = const_cast<ClipState*>(&clip);

    juce::String layerStr = "preview";
    int layerIdx = mediaEngine.getSelectedLayer();
    if (layerIdx >= 0) layerStr = juce::String(layerIdx);

    // Helper to update slider and its mapping path
    auto updateSlider = [&](juce::Slider& sld, const juce::String& name, double val) {
        sld.setValue(val, juce::dontSendNotification);
        sld.getProperties().set("mappingPath", "/layer/" + layerStr + "/" + name);
    };

    if (currentClip != nullptr) {
        isUpdatingFromCode = true;
        playPauseButton.setToggleState(currentClip->transport.isPlaying, juce::dontSendNotification);
        loopButton.setToggleState(currentClip->transport.loop, juce::dontSendNotification);
        isUpdatingFromCode = false;
    }
    
    if (clip.isLoaded)
    {
        clipNameLabel.setText(clip.name, juce::dontSendNotification);
        
        updateSlider(speedSlider, "speed", clip.transport.speed);
        updateSlider(opacitySlider, "opacity", clip.transform.opacity.baseValue);
        updateSlider(widthSlider, "width", clip.transform.width);
        updateSlider(heightSlider, "height", clip.transform.height);
        
        updateSlider(posXSlider, "posx", clip.transform.posX.baseValue);
        updateSlider(posYSlider, "posy", clip.transform.posY.baseValue);
        updateSlider(scaleSlider, "scale", clip.transform.scale.baseValue);
        updateSlider(scaleXSlider, "scalex", clip.transform.scaleX.baseValue);
        updateSlider(scaleYSlider, "scaley", clip.transform.scaleY.baseValue);
        updateSlider(rotXSlider, "rotx", clip.transform.rotationX.baseValue);
        updateSlider(rotYSlider, "roty", clip.transform.rotationY.baseValue);
        updateSlider(rotZSlider, "rotz", clip.transform.rotationZ.baseValue);
        updateSlider(anchorXSlider, "anchorx", clip.transform.anchorX.baseValue);
        updateSlider(anchorYSlider, "anchory", clip.transform.anchorY.baseValue);
        
        if (clip.transport.duration > 0.0)
            positionSlider.setValue(clip.transport.position / clip.transport.duration, juce::dontSendNotification);
        else
            positionSlider.setValue(0.0, juce::dontSendNotification);
    }
    else
    {
        clipNameLabel.setText("- No Clip Selected -", juce::dontSendNotification);
    }

    isUpdatingFromCode = false;
}

void PropertiesComponent::updateTimelinePosition(double progressNormalized)
{
    if (!positionSlider.isMouseButtonDown())
    {
        isUpdatingFromCode = true;
        positionSlider.setValue(progressNormalized * positionSlider.getMaximum(), juce::dontSendNotification);
        isUpdatingFromCode = false;
    }
}

void PropertiesComponent::updateParameterVisuals(double time)
{
    isUpdatingFromCode = true;
    bool isEditMode = mediaEngine.mappingManager.getEditMode() != MappingManager::EditMode::None;
    bool isOscMode = mediaEngine.mappingManager.getEditMode() == MappingManager::EditMode::OSC;
    
    // Disable intercepts on all sliders so clicks fall through in edit mode
    for (auto* comp : content.getChildren())
    {
        if (auto* slider = dynamic_cast<juce::Slider*>(comp))
        {
            slider->setInterceptsMouseClicks(!isEditMode, !isEditMode);
        }
    }
    
    for (auto& pair : sliderToParam)
    {
        auto* slider = pair.first;
        auto getParam = pair.second;
        
        if (getParam && !slider->isMouseButtonDown())
        {
            if (auto* p = getParam())
            {
                if (p->envelope.isEnabled)
                {
                    slider->setValue(p->eval(time), juce::dontSendNotification);
                }
                else if (std::abs(slider->getValue() - p->baseValue) > 0.0001f)
                {
                    // Sync with external MIDI/OSC changes
                    slider->setValue(p->baseValue, juce::dontSendNotification);
                }
            }
        }
    }
    isUpdatingFromCode = false;
}

void PropertiesComponent::updateClipFromSliders(ClipState& clip)
{
    if (!clip.isLoaded)
        return;

    clip.transport.isPlaying = playPauseButton.getToggleState();
    clip.transport.loop = loopButton.getToggleState();
    clip.transport.speed = speedSlider.getValue();
    
    if (positionSlider.isMouseButtonDown())
        clip.transport.position = positionSlider.getValue();

    updateTransformFromSliders(clip);
}

void PropertiesComponent::updateTransformFromSliders(ClipState& clip)
{
    if (!clip.isLoaded)
        return;

    clip.transform.opacity.baseValue = opacitySlider.getValue();
    clip.transform.width = (int)widthSlider.getValue();
    clip.transform.height = (int)heightSlider.getValue();

    clip.transform.posX.baseValue = posXSlider.getValue();
    clip.transform.posY.baseValue = posYSlider.getValue();

    clip.transform.scale.baseValue = scaleSlider.getValue();
    clip.transform.scaleX.baseValue = scaleXSlider.getValue();
    clip.transform.scaleY.baseValue = scaleYSlider.getValue();

    clip.transform.rotationX.baseValue = rotXSlider.getValue();
    clip.transform.rotationY.baseValue = rotYSlider.getValue();
    clip.transform.rotationZ.baseValue = rotZSlider.getValue();

    clip.transform.anchorX.baseValue = anchorXSlider.getValue();
    clip.transform.anchorY.baseValue = anchorYSlider.getValue();
}

