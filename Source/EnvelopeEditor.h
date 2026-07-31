#pragma once
#include <JuceHeader.h>
#include "Parameter.h"

class EnvelopeEditor : public juce::Component
{
public:
    EnvelopeEditor(Parameter& paramToEdit, const juce::String& paramName, double clipDuration, float minV, float maxV) 
        : param(paramToEdit), minVal(minV), maxVal(maxV)
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
        
        durationSlider.setRange(1.0, 60.0, 0.1);
        param.envelope.duration = clipDuration;
        durationSlider.setValue(param.envelope.duration);
        durationSlider.onValueChange = [this] {
            param.envelope.duration = durationSlider.getValue();
            repaint();
        };
        durationSlider.setSliderStyle(juce::Slider::LinearHorizontal);
        durationSlider.setTextBoxStyle(juce::Slider::TextBoxLeft, false, 50, 20);
        addAndMakeVisible(durationSlider);
        
        setSize(400, 300);
    }
    
    void paint(juce::Graphics& g) override
    {
        g.fillAll(juce::Colour(0xff18181b));
        g.setColour(juce::Colours::white.withAlpha(0.2f));
        g.drawRect(getLocalBounds(), 1);
        
        auto graphArea = getLocalBounds().withTrimmedTop(60).withTrimmedBottom(25).withTrimmedLeft(45).withTrimmedRight(15);
        g.setColour(juce::Colour(0xff27272a));
        g.fillRect(graphArea);
        
        // Draw grid and legends
        g.setColour(juce::Colours::white.withAlpha(0.1f));
        g.setFont(10.0f);
        int numTimeDivs = 5;
        for (int i = 0; i <= numTimeDivs; ++i) {
            float x = graphArea.getX() + (float)i / numTimeDivs * graphArea.getWidth();
            g.setColour(juce::Colours::white.withAlpha(0.1f));
            g.drawVerticalLine((int)x, (float)graphArea.getY(), (float)graphArea.getBottom());
            
            double t = (double)i / numTimeDivs * param.envelope.duration;
            g.setColour(juce::Colours::white.withAlpha(0.5f));
            g.drawText(juce::String(t, 1) + "s", (int)x - 20, graphArea.getBottom() + 4, 40, 15, juce::Justification::centred);
        }
        
        int numValDivs = 4;
        for (int i = 0; i <= numValDivs; ++i) {
            float y = graphArea.getY() + (float)i / numValDivs * graphArea.getHeight();
            g.setColour(juce::Colours::white.withAlpha(0.1f));
            g.drawHorizontalLine((int)y, (float)graphArea.getX(), (float)graphArea.getRight());
            
            float val = maxVal - (float)i / numValDivs * (maxVal - minVal);
            g.setColour(juce::Colours::white.withAlpha(0.5f));
            g.drawText(juce::String(val, 2), graphArea.getX() - 42, (int)y - 7, 40, 15, juce::Justification::centredRight);
        }
        
        if (!param.envelope.isEnabled)
        {
            g.setColour(juce::Colours::grey);
            g.drawText("Automation Disabled", graphArea, juce::Justification::centred);
            return;
        }
        
        // Draw points and lines
        g.setColour(juce::Colour(0xff00f0a8)); // Neon green
        juce::Path p;
        
        auto timeToX = [&](double time) {
            return graphArea.getX() + (float)(time / param.envelope.duration) * graphArea.getWidth();
        };
        auto valueToY = [&](float val) {
            float norm = (val - minVal) / (maxVal - minVal);
            // clamp norm
            norm = juce::jlimit(0.0f, 1.0f, norm);
            return graphArea.getBottom() - norm * graphArea.getHeight();
        };
        
        if (!param.envelope.points.empty())
        {
            p.startNewSubPath(timeToX(param.envelope.points[0].time), valueToY(param.envelope.points[0].value));
            for (size_t i = 1; i < param.envelope.points.size(); ++i)
            {
                p.lineTo(timeToX(param.envelope.points[i].time), valueToY(param.envelope.points[i].value));
            }
            g.strokePath(p, juce::PathStrokeType(2.0f));
            
            // Draw handles
            g.setColour(juce::Colours::white);
            for (auto& pt : param.envelope.points)
            {
                g.fillEllipse(timeToX(pt.time) - 4, valueToY(pt.value) - 4, 8, 8);
            }
        }
    }
    
    void resized() override
    {
        nameLabel.setBounds(10, 5, 200, 20);
        enableButton.setBounds(10, 30, 150, 20);
        clearButton.setBounds(170, 30, 100, 20);
        durationSlider.setBounds(280, 30, 110, 20);
    }
    
    void mouseDown(const juce::MouseEvent& e) override
    {
        if (!param.envelope.isEnabled) return;
        
        auto graphArea = getLocalBounds().withTrimmedTop(60).withTrimmedBottom(25).withTrimmedLeft(45).withTrimmedRight(15);
        
        auto timeToX = [&](double time) {
            return graphArea.getX() + (float)(time / param.envelope.duration) * graphArea.getWidth();
        };
        auto valueToY = [&](float val) {
            float norm = (val - minVal) / (maxVal - minVal);
            norm = juce::jlimit(0.0f, 1.0f, norm);
            return graphArea.getBottom() - norm * graphArea.getHeight();
        };

        int clickedIdx = -1;
        for (size_t i = 0; i < param.envelope.points.size(); ++i)
        {
            float px = timeToX(param.envelope.points[i].time);
            float py = valueToY(param.envelope.points[i].value);
            if (std::abs(e.x - px) < 10.0f && std::abs(e.y - py) < 10.0f)
            {
                clickedIdx = (int)i;
                break;
            }
        }

        if (e.mods.isRightButtonDown())
        {
            if (clickedIdx != -1)
            {
                param.envelope.points.erase(param.envelope.points.begin() + clickedIdx);
                repaint();
            }
            else if (graphArea.contains(e.getPosition()))
            {
                double t = (e.x - graphArea.getX()) / (double)graphArea.getWidth() * param.envelope.duration;
                float norm = 1.0f - (float)(e.y - graphArea.getY()) / graphArea.getHeight();
                float v = minVal + norm * (maxVal - minVal);
                param.envelope.addPoint(t, v);
                repaint();
            }
        }
        else if (e.mods.isLeftButtonDown())
        {
            if (clickedIdx != -1)
            {
                draggingPointIndex = clickedIdx;
            }
        }
    }

    void mouseDrag(const juce::MouseEvent& e) override
    {
        if (draggingPointIndex != -1 && draggingPointIndex < param.envelope.points.size())
        {
            auto graphArea = getLocalBounds().withTrimmedTop(60).withTrimmedBottom(25).withTrimmedLeft(45).withTrimmedRight(15);
            double t = (e.x - graphArea.getX()) / (double)graphArea.getWidth() * param.envelope.duration;
            t = juce::jlimit(0.0, param.envelope.duration, t);
            
            float norm = 1.0f - (float)(e.y - graphArea.getY()) / graphArea.getHeight();
            float v = minVal + norm * (maxVal - minVal);
            v = juce::jlimit(minVal, maxVal, v);

            auto pt = param.envelope.points[draggingPointIndex];
            pt.time = t;
            pt.value = v;
            param.envelope.points[draggingPointIndex] = pt;

            std::sort(param.envelope.points.begin(), param.envelope.points.end());
            
            // Re-find the dragged point index
            for (size_t i = 0; i < param.envelope.points.size(); ++i)
            {
                if (param.envelope.points[i].time == pt.time && param.envelope.points[i].value == pt.value)
                {
                    draggingPointIndex = (int)i;
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
    
private:
    Parameter& param;
    float minVal;
    float maxVal;
    juce::Label nameLabel;
    juce::ToggleButton enableButton;
    juce::TextButton clearButton;
    juce::Slider durationSlider;
    int draggingPointIndex = -1;
};

class EnvelopeEditorWindow : public juce::DocumentWindow
{
public:
    EnvelopeEditorWindow(Parameter& param, const juce::String& name, double clipDuration, float minVal, float maxVal)
        : DocumentWindow("Envelope Editor - " + name, juce::Colour(0xff18181b), DocumentWindow::closeButton)
    {
        setUsingNativeTitleBar(true);
        setContentOwned(new EnvelopeEditor(param, name, clipDuration, minVal, maxVal), true);
        setResizable(false, false);
        centreWithSize(getWidth(), getHeight());
        setVisible(true);
    }
    
    void closeButtonPressed() override
    {
        delete this;
    }
};
