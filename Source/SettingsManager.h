#pragma once
#include <JuceHeader.h>
#include <functional>

struct Theme
{
    juce::Colour background = juce::Colour(0xff09090b);      // zinc 950
    juce::Colour panelBackground = juce::Colour(0xff18181b); // zinc 900
    juce::Colour border = juce::Colour(0xff27272a);          // zinc 800
    juce::Colour text = juce::Colours::white;
    juce::Colour accent1 = juce::Colour(0xff00f0a8);         // neon green
    juce::Colour accent2 = juce::Colour(0xff60a5fa);         // blue 400

    bool operator==(const Theme& other) const
    {
        return background == other.background &&
               panelBackground == other.panelBackground &&
               border == other.border &&
               text == other.text &&
               accent1 == other.accent1 &&
               accent2 == other.accent2;
    }
    
    bool operator!=(const Theme& other) const
    {
        return !(*this == other);
    }
};

class SettingsManager
{
public:
    SettingsManager();
    ~SettingsManager() = default;

    void load();
    void save();

    void loadPreset(int presetIndex);

    Theme getTheme() const { return currentTheme; }
    void setTheme(const Theme& theme);

    int getOscPort() const { return oscPort; }
    void setOscPort(int port);

    int getOscOutPort() const { return oscOutPort; }
    void setOscOutPort(int port);

    juce::StringArray getEnabledMidiDevices() const { return enabledMidiDevices; }
    void setEnabledMidiDevices(const juce::StringArray& devices);

    // Callbacks
    std::function<void()> onThemeChanged;
    std::function<void(int)> onOscPortChanged;
    std::function<void(int)> onOscOutPortChanged;
    std::function<void(const juce::StringArray&)> onMidiDevicesChanged;

private:
    Theme currentTheme;
    int oscPort = 9000;
    int oscOutPort = 9001;
    juce::StringArray enabledMidiDevices;

    juce::File getSettingsFile() const;
};
