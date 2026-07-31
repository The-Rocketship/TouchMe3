#pragma once
#include <JuceHeader.h>
#include "Parameter.h"

class ColourEnvelopeEditor : public juce::Component, public juce::ChangeListener
{
public:
    ColourEnvelopeEditor(ColourParameter& paramToEdit, const juce::String& paramName, double clipDuration) 
        : param(paramToEdit)
    {
        nameLabel.setText(paramName + " Envelope", juce::dontSendNotification);
        nameLabel.setColour(juce::Label::textColourId, juce::Colours::white);
        addAndMakeVisible(nameLabel);
        
        enableButton.setButtonText("Enable Automation");
        enableButton.setToggleState(param.envelope.isEnabled, juce::dontSendNotification);
        enableButton.onClick = [this] {
            param.envelope.isEnabled = enableButton.getToggleState();
            repaint();
        };
        addAndMakeVisible(enableButton);
        
        clearButton.setButtonText("Clear Points");
        clearButton.onClick = [this] {
            param.envelope.points.clear();
            repaint();
        };
        addAndMakeVisible(clearButton);
        
        param.envelope.duration = clipDuration;
        
        setSize(400, 150);
    }
    
    void paint(juce::Graphics& g) override
    {
        g.fillAll(juce::Colour(0xff18181b));
        g.setColour(juce::Colours::white.withAlpha(0.2f));
        g.drawRect(getLocalBounds(), 1);
        auto graphArea = getLocalBounds().withTrimmedTop(60).withTrimmedBottom(25).withTrimmedLeft(45).withTrimmedRight(15);
        
        if (!param.envelope.isEnabled)
        {
            g.setColour(juce::Colour(0xff27272a));
            g.fillRect(graphArea);
            g.setColour(juce::Colours::grey);
            g.drawText("Automation Disabled", graphArea, juce::Justification::centred);
            return;
        }

        // Draw colour gradient bar
        int width = graphArea.getWidth();
        for (int x = 0; x < width; ++x)
        {
            double t = (double)x / width * param.envelope.duration;
            g.setColour(param.eval(t));
            g.drawLine((float)(graphArea.getX() + x), (float)graphArea.getY(), (float)(graphArea.getX() + x), (float)graphArea.getBottom());
        }
        
        g.setColour(juce::Colours::white.withAlpha(0.5f));
        g.drawRect(graphArea, 1);
        
        // Draw grid and legends
        g.setColour(juce::Colours::white.withAlpha(0.1f));
        g.setFont(10.0f);
        int numTimeDivs = 5;
        for (int i = 0; i <= numTimeDivs; ++i) {
            float x = graphArea.getX() + (float)i / numTimeDivs * graphArea.getWidth();
            g.setColour(juce::Colours::white.withAlpha(0.3f));
            g.drawVerticalLine((int)x, (float)graphArea.getY(), (float)graphArea.getBottom());
            
            double t = (double)i / numTimeDivs * param.envelope.duration;
            g.setColour(juce::Colours::white.withAlpha(0.5f));
            g.drawText(juce::String(t, 1) + "s", (int)x - 20, graphArea.getBottom() + 4, 40, 15, juce::Justification::centred);
        }
        
        // Draw handles
        auto timeToX = [&](double time) {
            return graphArea.getX() + (float)(time / param.envelope.duration) * graphArea.getWidth();
        };

        for (size_t i = 0; i < param.envelope.points.size(); ++i)
        {
            auto& pt = param.envelope.points[i];
            float x = timeToX(pt.time);
            
            g.setColour(juce::Colours::white);
            if (i == selectedPointIndex)
            {
                g.drawEllipse(x - 6, graphArea.getY() - 6, 12, 12, 2.0f);
            }
            g.setColour(pt.value);
            g.fillEllipse(x - 5, graphArea.getY() - 5, 10, 10);
            g.setColour(juce::Colours::white);
            g.drawEllipse(x - 5, graphArea.getY() - 5, 10, 10, 1.0f);
        }
    }
    
    void resized() override
    {
        nameLabel.setBounds(10, 5, 200, 20);
        enableButton.setBounds(10, 30, 150, 20);
        clearButton.setBounds(170, 30, 100, 20);
    }
    
    void mouseDown(const juce::MouseEvent& e) override
    {
        if (!param.envelope.isEnabled) return;
        
        auto graphArea = getLocalBounds().withTrimmedTop(60).withTrimmedBottom(25).withTrimmedLeft(45).withTrimmedRight(15);
        auto timeToX = [&](double time) {
            return graphArea.getX() + (float)(time / param.envelope.duration) * graphArea.getWidth();
        };

        // Check if we clicked a point
        int clickedIdx = -1;
        for (size_t i = 0; i < param.envelope.points.size(); ++i)
        {
            float x = timeToX(param.envelope.points[i].time);
            if (std::abs(e.x - x) < 10.0f)
            {
                clickedIdx = (int)i;
                break;
            }
        }

        if (e.mods.isRightButtonDown())
        {
            if (clickedIdx == -1 && graphArea.contains(e.getPosition()))
            {
                // Add a point
                double t = (e.x - graphArea.getX()) / (double)graphArea.getWidth() * param.envelope.duration;
                param.envelope.addPoint(t, param.baseValue);
                repaint();
            }
            else if (clickedIdx != -1)
            {
                // Remove point
                param.envelope.points.erase(param.envelope.points.begin() + clickedIdx);
                selectedPointIndex = -1;
                repaint();
            }
        }
        else if (e.mods.isLeftButtonDown())
        {
            selectedPointIndex = clickedIdx;
            if (selectedPointIndex != -1)
            {
                draggingPointIndex = clickedIdx;
                // Show colour selector
                if (colourSelector != nullptr)
                {
                    removeChildComponent(colourSelector.get());
                    colourSelector.reset();
                }
                
                colourSelector = std::make_unique<juce::ColourSelector>(juce::ColourSelector::showAlphaChannel | juce::ColourSelector::showColourAtTop | juce::ColourSelector::showSliders | juce::ColourSelector::showColourspace);
                colourSelector->setName("Colour");
                colourSelector->setSize(300, 300);
                colourSelector->setCurrentColour(param.envelope.points[selectedPointIndex].value);
                colourSelector->addChangeListener(this);
                
                juce::CallOutBox::launchAsynchronously(std::move(colourSelector), getScreenBounds().translated(0, 150), nullptr);
            }
            repaint();
        }
    }

    void mouseDrag(const juce::MouseEvent& e) override
    {
        if (draggingPointIndex != -1 && draggingPointIndex < param.envelope.points.size())
        {
            auto graphArea = getLocalBounds().withTrimmedTop(60).withTrimmedBottom(25).withTrimmedLeft(45).withTrimmedRight(15);
            double t = (e.x - graphArea.getX()) / (double)graphArea.getWidth() * param.envelope.duration;
            t = juce::jlimit(0.0, param.envelope.duration, t);
            
            auto pt = param.envelope.points[draggingPointIndex];
            pt.time = t;
            param.envelope.points[draggingPointIndex] = pt;

            std::sort(param.envelope.points.begin(), param.envelope.points.end());
            
            // Re-find the dragged point index
            for (size_t i = 0; i < param.envelope.points.size(); ++i)
            {
                if (param.envelope.points[i].time == pt.time && param.envelope.points[i].value == pt.value)
                {
                    draggingPointIndex = (int)i;
                    selectedPointIndex = (int)i; // update selected point so color selector affects the right point
                    break;
                }
            }
            repaint();
        }
    }

    void mouseUp(const juce::MouseEvent& e) override
    {
        draggingPointIndex = -1;
    }
    
    void changeListenerCallback(juce::ChangeBroadcaster* source) override
    {
        if (auto* cs = dynamic_cast<juce::ColourSelector*>(source))
        {
            if (selectedPointIndex >= 0 && selectedPointIndex < param.envelope.points.size())
            {
                param.envelope.points[selectedPointIndex].value = cs->getCurrentColour();
                repaint();
            }
        }
    }

private:
    ColourParameter& param;
    juce::Label nameLabel;
    juce::ToggleButton enableButton;
    juce::TextButton clearButton;
    
    int selectedPointIndex = -1;
    int draggingPointIndex = -1;
    std::unique_ptr<juce::ColourSelector> colourSelector;
};

class ColourEnvelopeEditorWindow : public juce::DocumentWindow
{
public:
    ColourEnvelopeEditorWindow(ColourParameter& param, const juce::String& name, double clipDuration)
        : DocumentWindow("Colour Envelope Editor - " + name, juce::Colour(0xff18181b), DocumentWindow::closeButton)
    {
        setUsingNativeTitleBar(true);
        setContentOwned(new ColourEnvelopeEditor(param, name, clipDuration), true);
        setResizable(false, false);
        centreWithSize(getWidth(), getHeight());
        setVisible(true);
    }
    
    void closeButtonPressed() override
    {
        delete this;
    }
};
