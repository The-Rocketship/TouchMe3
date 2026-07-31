#pragma once
#include <JuceHeader.h>

class CanvasSettingsOverlay : public juce::Component
{
public:
    CanvasSettingsOverlay(std::function<void(juce::String, int, int, juce::String, juce::String)> onApplyCallback, std::function<void()> onCancelCallback)
        : onApply(onApplyCallback), onCancel(onCancelCallback)
    {
        // Name Label & Text Editor
        addAndMakeVisible(nameLabel);
        nameLabel.setText("Name", juce::dontSendNotification);
        nameLabel.setFont(12.0f);
        nameLabel.setColour(juce::Label::textColourId, juce::Colour(0xffa1a1aa));

        addAndMakeVisible(nameEditor);
        nameEditor.setText("Composition");
        nameEditor.setFont(12.0f);
        nameEditor.setColour(juce::TextEditor::backgroundColourId, juce::Colour(0xff27272a));
        nameEditor.setColour(juce::TextEditor::textColourId, juce::Colours::white);
        nameEditor.setColour(juce::TextEditor::focusedOutlineColourId, juce::Colour(0xff10ffd0));

        // Size Labels & Text Editors
        addAndMakeVisible(sizeLabel);
        sizeLabel.setText("Size", juce::dontSendNotification);
        sizeLabel.setFont(12.0f);
        sizeLabel.setColour(juce::Label::textColourId, juce::Colour(0xffa1a1aa));

        addAndMakeVisible(widthEditor);
        widthEditor.setText("1920");
        widthEditor.setFont(12.0f);
        widthEditor.setInputRestrictions(5, "0123456789");
        widthEditor.setColour(juce::TextEditor::backgroundColourId, juce::Colour(0xff27272a));
        widthEditor.setColour(juce::TextEditor::textColourId, juce::Colours::white);
        widthEditor.setColour(juce::TextEditor::focusedOutlineColourId, juce::Colour(0xff10ffd0));

        addAndMakeVisible(xLabel);
        xLabel.setText("x", juce::dontSendNotification);
        xLabel.setFont(12.0f);
        xLabel.setColour(juce::Label::textColourId, juce::Colour(0xffa1a1aa));

        addAndMakeVisible(heightEditor);
        heightEditor.setText("1080");
        heightEditor.setFont(12.0f);
        heightEditor.setInputRestrictions(5, "0123456789");
        heightEditor.setColour(juce::TextEditor::backgroundColourId, juce::Colour(0xff27272a));
        heightEditor.setColour(juce::TextEditor::textColourId, juce::Colours::white);
        heightEditor.setColour(juce::TextEditor::focusedOutlineColourId, juce::Colour(0xff10ffd0));

        // Frame Rate Label & Combo Box
        addAndMakeVisible(fpsLabel);
        fpsLabel.setText("FrameRate", juce::dontSendNotification);
        fpsLabel.setFont(12.0f);
        fpsLabel.setColour(juce::Label::textColourId, juce::Colour(0xffa1a1aa));

        addAndMakeVisible(fpsCombo);
        fpsCombo.addItem("Auto", 1);
        fpsCombo.addItem("60 fps", 2);
        fpsCombo.addItem("50 fps", 3);
        fpsCombo.addItem("30 fps", 4);
        fpsCombo.addItem("25 fps", 5);
        fpsCombo.setSelectedId(1);
        fpsCombo.setColour(juce::ComboBox::backgroundColourId, juce::Colour(0xff27272a));
        fpsCombo.setColour(juce::ComboBox::textColourId, juce::Colours::white);
        fpsCombo.setColour(juce::ComboBox::focusedOutlineColourId, juce::Colour(0xff10ffd0));

        // Color Depth Label & Combo Box
        addAndMakeVisible(colorLabel);
        colorLabel.setText("Color Depth", juce::dontSendNotification);
        colorLabel.setFont(12.0f);
        colorLabel.setColour(juce::Label::textColourId, juce::Colour(0xffa1a1aa));

        addAndMakeVisible(colorCombo);
        colorCombo.addItem("8bpc", 1);
        colorCombo.addItem("16bpc", 2);
        colorCombo.setSelectedId(1);
        colorCombo.setColour(juce::ComboBox::backgroundColourId, juce::Colour(0xff27272a));
        colorCombo.setColour(juce::ComboBox::textColourId, juce::Colours::white);
        colorCombo.setColour(juce::ComboBox::focusedOutlineColourId, juce::Colour(0xff10ffd0));

        // Buttons
        addAndMakeVisible(cancelButton);
        cancelButton.setButtonText("Cancel");
        cancelButton.setColour(juce::TextButton::buttonColourId, juce::Colour(0xff27272a));
        cancelButton.setColour(juce::TextButton::textColourOffId, juce::Colours::white);
        cancelButton.onClick = [this] { if (onCancel) onCancel(); };

        addAndMakeVisible(applyButton);
        applyButton.setButtonText("Apply");
        applyButton.setColour(juce::TextButton::buttonColourId, juce::Colour(0xff00f0a8)); // neon green
        applyButton.setColour(juce::TextButton::textColourOffId, juce::Colours::black);
        applyButton.onClick = [this] {
            if (onApply)
                onApply(nameEditor.getText(), widthEditor.getText().getIntValue(), heightEditor.getText().getIntValue(), fpsCombo.getText(), colorCombo.getText());
        };
    }

    void paint(juce::Graphics& g) override
    {
        // Rounded dark card background
        g.fillAll(juce::Colour(0x00000000)); // Transparent container
        
        auto bounds = getLocalBounds().toFloat();
        g.setColour(juce::Colour(0xee18181b)); // Sleek zinc 900 translucent card
        g.fillRoundedRectangle(bounds, 8.0f);
        
        g.setColour(juce::Colour(0xff10ffd0)); // Mint green border
        g.drawRoundedRectangle(bounds, 8.0f, 1.5f);

        // Dialog Title
        g.setColour(juce::Colours::white);
        g.setFont(juce::Font(14.0f, juce::Font::bold));
        g.drawText("Composition Settings", 16, 16, getWidth() - 32, 24, juce::Justification::centredLeft);
    }

    void resized() override
    {
        int xStart = 20;
        int yStart = 60;
        int labelW = 90;
        int inputW = 180;
        int rowH = 26;
        int spacing = 16;

        nameLabel.setBounds(xStart, yStart, labelW, rowH);
        nameEditor.setBounds(xStart + labelW + 10, yStart, inputW, rowH);
        yStart += rowH + spacing;

        sizeLabel.setBounds(xStart, yStart, labelW, rowH);
        widthEditor.setBounds(xStart + labelW + 10, yStart, 65, rowH);
        xLabel.setBounds(xStart + labelW + 10 + 65, yStart, 20, rowH);
        xLabel.setJustificationType(juce::Justification::centred);
        heightEditor.setBounds(xStart + labelW + 10 + 65 + 20, yStart, 65, rowH);
        yStart += rowH + spacing;

        fpsLabel.setBounds(xStart, yStart, labelW, rowH);
        fpsCombo.setBounds(xStart + labelW + 10, yStart, inputW, rowH);
        yStart += rowH + spacing;

        colorLabel.setBounds(xStart, yStart, labelW, rowH);
        colorCombo.setBounds(xStart + labelW + 10, yStart, inputW, rowH);
        yStart += rowH + spacing * 2;

        cancelButton.setBounds(getWidth() - 190, yStart, 80, 28);
        applyButton.setBounds(getWidth() - 100, yStart, 80, 28);
    }

private:
    juce::Label nameLabel;
    juce::TextEditor nameEditor;

    juce::Label sizeLabel;
    juce::TextEditor widthEditor;
    juce::Label xLabel;
    juce::TextEditor heightEditor;

    juce::Label fpsLabel;
    juce::ComboBox fpsCombo;

    juce::Label colorLabel;
    juce::ComboBox colorCombo;

    juce::TextButton cancelButton;
    juce::TextButton applyButton;

    std::function<void(juce::String, int, int, juce::String, juce::String)> onApply;
    std::function<void()> onCancel;
};
