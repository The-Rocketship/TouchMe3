#include "SettingsWindow.h"

// ==============================================================================
// SettingsWindow
// ==============================================================================
SettingsWindow::SettingsWindow(juce::String name, MediaEngine& mediaEngine)
    : DocumentWindow(name, mediaEngine.settingsManager.getTheme().panelBackground, DocumentWindow::allButtons),
      contentComponent(mediaEngine)
{
    setUsingNativeTitleBar(true);
    setContentNonOwned(&contentComponent, true);
    setResizable(true, true);
    setResizeLimits(600, 500, 1200, 900);
    centreWithSize(800, 600);
}

SettingsWindow::~SettingsWindow()
{
}

void SettingsWindow::closeButtonPressed()
{
    setVisible(false);
}

// ==============================================================================
// SettingsContent
// ==============================================================================
SettingsWindow::SettingsContent::SettingsContent(MediaEngine& engine)
    : mediaEngine(engine), oscTab(engine), midiTab(engine), themeTab(engine)
{
    tabbedComponent.addTab("OSC", mediaEngine.settingsManager.getTheme().panelBackground, &oscTab, false);
    tabbedComponent.addTab("MIDI", mediaEngine.settingsManager.getTheme().panelBackground, &midiTab, false);
    tabbedComponent.addTab("Theme & Colors", mediaEngine.settingsManager.getTheme().panelBackground, &themeTab, false);
    addAndMakeVisible(tabbedComponent);
}

void SettingsWindow::SettingsContent::paint(juce::Graphics& g)
{
    g.fillAll(mediaEngine.settingsManager.getTheme().background);
}

void SettingsWindow::SettingsContent::resized()
{
    tabbedComponent.setBounds(getLocalBounds().reduced(10));
}

// ==============================================================================
// MidiTab
// ==============================================================================
class MidiDeviceItem : public juce::Component
{
public:
    MidiDeviceItem(MediaEngine& engine, const juce::MidiDeviceInfo& deviceInfo)
        : mediaEngine(engine), info(deviceInfo)
    {
        toggle.setButtonText(info.name);
        
        // Check if currently enabled
        bool isEnabled = mediaEngine.settingsManager.getEnabledMidiDevices().contains(info.identifier);
        toggle.setToggleState(isEnabled, juce::dontSendNotification);
        
        toggle.onClick = [this] {
            auto devices = mediaEngine.settingsManager.getEnabledMidiDevices();
            if (toggle.getToggleState()) {
                devices.addIfNotAlreadyThere(info.identifier);
            } else {
                devices.removeString(info.identifier);
            }
            mediaEngine.settingsManager.setEnabledMidiDevices(devices);
        };
        addAndMakeVisible(toggle);
    }
    
    void resized() override {
        toggle.setBounds(getLocalBounds().reduced(4, 0));
    }
    
private:
    MediaEngine& mediaEngine;
    juce::MidiDeviceInfo info;
    juce::ToggleButton toggle;
};

SettingsWindow::SettingsContent::MidiTab::MidiListModel::MidiListModel(MediaEngine& engine)
    : mediaEngine(engine)
{
    devices = juce::MidiInput::getAvailableDevices();
}

int SettingsWindow::SettingsContent::MidiTab::MidiListModel::getNumRows()
{
    return devices.size();
}

void SettingsWindow::SettingsContent::MidiTab::MidiListModel::paintListBoxItem(int rowNumber, juce::Graphics& g, int width, int height, bool rowIsSelected)
{
    // Custom item component does the painting
}

juce::Component* SettingsWindow::SettingsContent::MidiTab::MidiListModel::refreshComponentForRow(int rowNumber, bool isRowSelected, juce::Component* existingComponentToUpdate)
{
    if (rowNumber < 0 || rowNumber >= devices.size()) return nullptr;
    
    if (existingComponentToUpdate == nullptr)
        return new MidiDeviceItem(mediaEngine, devices[rowNumber]);
        
    return existingComponentToUpdate;
}

SettingsWindow::SettingsContent::MidiTab::MidiTab(MediaEngine& engine)
    : mediaEngine(engine), midiModel(engine)
{
    midiLabel.setText("MIDI Input Devices:", juce::dontSendNotification);
    midiLabel.setColour(juce::Label::textColourId, mediaEngine.settingsManager.getTheme().text);
    addAndMakeVisible(midiLabel);

    midiListBox.setModel(&midiModel);
    midiListBox.setRowHeight(24);
    midiListBox.setColour(juce::ListBox::backgroundColourId, mediaEngine.settingsManager.getTheme().background);
    addAndMakeVisible(midiListBox);
}

void SettingsWindow::SettingsContent::MidiTab::resized()
{
    auto area = getLocalBounds().reduced(20);
    
    midiLabel.setBounds(area.removeFromTop(30));
    midiListBox.setBounds(area);
}

// ==============================================================================
// OscTab
// ==============================================================================

SettingsWindow::SettingsContent::OscTab::OscTab(MediaEngine& engine)
    : mediaEngine(engine)
{
    oscInPortLabel.setText("OSC In Port:", juce::dontSendNotification);
    oscInPortLabel.setColour(juce::Label::textColourId, mediaEngine.settingsManager.getTheme().text);
    addAndMakeVisible(oscInPortLabel);

    oscInPortInput.setText(juce::String(mediaEngine.settingsManager.getOscPort()));
    oscInPortInput.setColour(juce::TextEditor::textColourId, mediaEngine.settingsManager.getTheme().text);
    oscInPortInput.setColour(juce::TextEditor::backgroundColourId, mediaEngine.settingsManager.getTheme().background);
    oscInPortInput.onReturnKey = [this] {
        mediaEngine.settingsManager.setOscPort(oscInPortInput.getText().getIntValue());
    };
    addAndMakeVisible(oscInPortInput);

    oscOutPortLabel.setText("OSC Out Port:", juce::dontSendNotification);
    oscOutPortLabel.setColour(juce::Label::textColourId, mediaEngine.settingsManager.getTheme().text);
    addAndMakeVisible(oscOutPortLabel);

    oscOutPortInput.setText(juce::String(mediaEngine.settingsManager.getOscOutPort()));
    oscOutPortInput.setColour(juce::TextEditor::textColourId, mediaEngine.settingsManager.getTheme().text);
    oscOutPortInput.setColour(juce::TextEditor::backgroundColourId, mediaEngine.settingsManager.getTheme().background);
    oscOutPortInput.onReturnKey = [this] {
        mediaEngine.settingsManager.setOscOutPort(oscOutPortInput.getText().getIntValue());
    };
    addAndMakeVisible(oscOutPortInput);

    logLabel.setText("OSC Debug Log:", juce::dontSendNotification);
    logLabel.setColour(juce::Label::textColourId, mediaEngine.settingsManager.getTheme().text);
    addAndMakeVisible(logLabel);

    logEditor.setMultiLine(true);
    logEditor.setReturnKeyStartsNewLine(true);
    logEditor.setReadOnly(true);
    logEditor.setScrollbarsShown(true);
    logEditor.setColour(juce::TextEditor::textColourId, mediaEngine.settingsManager.getTheme().text);
    logEditor.setColour(juce::TextEditor::backgroundColourId, mediaEngine.settingsManager.getTheme().background);
    addAndMakeVisible(logEditor);

    mediaEngine.mappingManager.onOscMessageReceived = [this](const juce::String& msg) {
        juce::MessageManager::callAsync([this, msg] {
            logEditor.moveCaretToEnd();
            logEditor.insertTextAtCaret(msg + "\n");
        });
    };
}

SettingsWindow::SettingsContent::OscTab::~OscTab()
{
    mediaEngine.mappingManager.onOscMessageReceived = nullptr;
}

void SettingsWindow::SettingsContent::OscTab::resized()
{
    auto area = getLocalBounds().reduced(20);
    
    auto row1 = area.removeFromTop(30);
    oscInPortLabel.setBounds(row1.removeFromLeft(100));
    oscInPortInput.setBounds(row1.removeFromLeft(100));
    
    auto row2 = area.removeFromTop(30);
    oscOutPortLabel.setBounds(row2.removeFromLeft(100));
    oscOutPortInput.setBounds(row2.removeFromLeft(100));
    
    area.removeFromTop(20); // space
    
    logLabel.setBounds(area.removeFromTop(30));
    logEditor.setBounds(area);
}

// ==============================================================================
// ThemeTab
// ==============================================================================
SettingsWindow::SettingsContent::ThemeTab::ThemeTab(MediaEngine& engine)
    : mediaEngine(engine)
{
    presetLabel.setText("Theme Preset:", juce::dontSendNotification);
    presetLabel.setColour(juce::Label::textColourId, mediaEngine.settingsManager.getTheme().text);
    addAndMakeVisible(presetLabel);
    
    presetDropdown.addItem("Dark Neon", 1);
    presetDropdown.addItem("Light", 2);
    presetDropdown.addItem("Synthwave", 3);
    presetDropdown.addItem("Cyberpunk", 4);
    presetDropdown.addItem("High Contrast", 5);
    presetDropdown.addItem("Custom", 6);
    presetDropdown.setSelectedId(1, juce::dontSendNotification); // Default to Dark Neon for now
    presetDropdown.addListener(this);
    addAndMakeVisible(presetDropdown);

    auto setupPicker = [this](juce::Label& lbl, juce::ColourSelector& picker, juce::String name) {
        lbl.setText(name, juce::dontSendNotification);
        lbl.setColour(juce::Label::textColourId, mediaEngine.settingsManager.getTheme().text);
        addAndMakeVisible(lbl);
        
        picker.addChangeListener(this);
        addAndMakeVisible(picker);
    };

    setupPicker(bgLabel, bgSelector, "Background");
    setupPicker(panelBgLabel, panelBgSelector, "Panel Background");
    setupPicker(borderLabel, borderSelector, "Border");
    setupPicker(textLabel, textSelector, "Text");
    setupPicker(accent1Label, accent1Selector, "Accent 1");
    setupPicker(accent2Label, accent2Selector, "Accent 2");

    updatePickersFromTheme();
}

SettingsWindow::SettingsContent::ThemeTab::~ThemeTab()
{
    bgSelector.removeChangeListener(this);
    panelBgSelector.removeChangeListener(this);
    borderSelector.removeChangeListener(this);
    textSelector.removeChangeListener(this);
    accent1Selector.removeChangeListener(this);
    accent2Selector.removeChangeListener(this);
}

void SettingsWindow::SettingsContent::ThemeTab::comboBoxChanged(juce::ComboBox* comboBoxThatHasChanged)
{
    if (comboBoxThatHasChanged == &presetDropdown)
    {
        int idx = presetDropdown.getSelectedId() - 1;
        if (idx >= 0 && idx < 5)
        {
            mediaEngine.settingsManager.loadPreset(idx);
            updatePickersFromTheme();
        }
    }
}

void SettingsWindow::SettingsContent::ThemeTab::updatePickersFromTheme()
{
    auto t = mediaEngine.settingsManager.getTheme();
    bgSelector.setCurrentColour(t.background, juce::dontSendNotification);
    panelBgSelector.setCurrentColour(t.panelBackground, juce::dontSendNotification);
    borderSelector.setCurrentColour(t.border, juce::dontSendNotification);
    textSelector.setCurrentColour(t.text, juce::dontSendNotification);
    accent1Selector.setCurrentColour(t.accent1, juce::dontSendNotification);
    accent2Selector.setCurrentColour(t.accent2, juce::dontSendNotification);
}

void SettingsWindow::SettingsContent::ThemeTab::changeListenerCallback(juce::ChangeBroadcaster* source)
{
    if (source == &bgSelector || source == &panelBgSelector || source == &borderSelector ||
        source == &textSelector || source == &accent1Selector || source == &accent2Selector)
    {
        applyCustomTheme();
        presetDropdown.setSelectedId(6, juce::dontSendNotification); // Set to Custom
    }
}

void SettingsWindow::SettingsContent::ThemeTab::applyCustomTheme()
{
    Theme t;
    t.background = bgSelector.getCurrentColour();
    t.panelBackground = panelBgSelector.getCurrentColour();
    t.border = borderSelector.getCurrentColour();
    t.text = textSelector.getCurrentColour();
    t.accent1 = accent1Selector.getCurrentColour();
    t.accent2 = accent2Selector.getCurrentColour();
    
    mediaEngine.settingsManager.setTheme(t);
}

void SettingsWindow::SettingsContent::ThemeTab::resized()
{
    auto area = getLocalBounds().reduced(20);
    
    auto row1 = area.removeFromTop(30);
    presetLabel.setBounds(row1.removeFromLeft(120));
    presetDropdown.setBounds(row1.removeFromLeft(200));
    
    area.removeFromTop(20);
    
    // Pickers are huge, so maybe just display them in a grid
    int w = area.getWidth() / 3;
    int h = area.getHeight() / 2;
    
    auto p1 = area.removeFromTop(h);
    auto p2 = area;
    
    auto r1 = p1.removeFromLeft(w);
    bgLabel.setBounds(r1.removeFromTop(20));
    bgSelector.setBounds(r1);
    
    auto r2 = p1.removeFromLeft(w);
    panelBgLabel.setBounds(r2.removeFromTop(20));
    panelBgSelector.setBounds(r2);
    
    auto r3 = p1.removeFromLeft(w);
    borderLabel.setBounds(r3.removeFromTop(20));
    borderSelector.setBounds(r3);
    
    auto r4 = p2.removeFromLeft(w);
    textLabel.setBounds(r4.removeFromTop(20));
    textSelector.setBounds(r4);
    
    auto r5 = p2.removeFromLeft(w);
    accent1Label.setBounds(r5.removeFromTop(20));
    accent1Selector.setBounds(r5);
    
    auto r6 = p2.removeFromLeft(w);
    accent2Label.setBounds(r6.removeFromTop(20));
    accent2Selector.setBounds(r6);
}
