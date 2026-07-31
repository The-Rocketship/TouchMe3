#include "SettingsManager.h"

SettingsManager::SettingsManager()
{
    // By default, enable all currently connected MIDI devices
    for (const auto& dev : juce::MidiInput::getAvailableDevices()) {
        enabledMidiDevices.add(dev.identifier);
    }
}

juce::File SettingsManager::getSettingsFile() const
{
    auto userDir = juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory);
    return userDir.getChildFile("TouchMe3").getChildFile("settings.json");
}

void SettingsManager::load()
{
    auto file = getSettingsFile();
    if (!file.existsAsFile())
        return; // Use defaults

    juce::var data = juce::JSON::parse(file);
    if (data.isObject())
    {
        if (data.hasProperty("oscPort"))
            oscPort = data["oscPort"];

        if (data.hasProperty("oscOutPort"))
            oscOutPort = data["oscOutPort"];

        if (data.hasProperty("enabledMidiDevices"))
        {
            enabledMidiDevices.clear();
            auto* array = data["enabledMidiDevices"].getArray();
            if (array != nullptr)
            {
                for (auto& item : *array)
                    enabledMidiDevices.add(item.toString());
            }
        }

        if (data.hasProperty("theme"))
        {
            auto themeData = data["theme"];
            if (themeData.hasProperty("background")) currentTheme.background = juce::Colour::fromString(themeData["background"].toString());
            if (themeData.hasProperty("panelBackground")) currentTheme.panelBackground = juce::Colour::fromString(themeData["panelBackground"].toString());
            if (themeData.hasProperty("border")) currentTheme.border = juce::Colour::fromString(themeData["border"].toString());
            if (themeData.hasProperty("text")) currentTheme.text = juce::Colour::fromString(themeData["text"].toString());
            if (themeData.hasProperty("accent1")) currentTheme.accent1 = juce::Colour::fromString(themeData["accent1"].toString());
            if (themeData.hasProperty("accent2")) currentTheme.accent2 = juce::Colour::fromString(themeData["accent2"].toString());
        }
    }
}

void SettingsManager::save()
{
    juce::DynamicObject::Ptr themeObj = new juce::DynamicObject();
    themeObj->setProperty("background", currentTheme.background.toString());
    themeObj->setProperty("panelBackground", currentTheme.panelBackground.toString());
    themeObj->setProperty("border", currentTheme.border.toString());
    themeObj->setProperty("text", currentTheme.text.toString());
    themeObj->setProperty("accent1", currentTheme.accent1.toString());
    themeObj->setProperty("accent2", currentTheme.accent2.toString());

    juce::Array<juce::var> midiArr;
    for (const auto& dev : enabledMidiDevices)
        midiArr.add(dev);

    juce::DynamicObject::Ptr mainObj = new juce::DynamicObject();
    mainObj->setProperty("oscPort", oscPort);
    mainObj->setProperty("oscOutPort", oscOutPort);
    mainObj->setProperty("enabledMidiDevices", midiArr);
    mainObj->setProperty("theme", juce::var(themeObj.get()));

    juce::var data(mainObj.get());
    juce::String jsonString = juce::JSON::toString(data);
    
    auto file = getSettingsFile();
    file.getParentDirectory().createDirectory(); // Ensure TouchMe3 folder exists
    file.replaceWithText(jsonString);
}

void SettingsManager::loadPreset(int presetIndex)
{
    Theme t;
    switch (presetIndex)
    {
    case 0: // Dark Neon
        t.background = juce::Colour(0xff09090b);
        t.panelBackground = juce::Colour(0xff18181b);
        t.border = juce::Colour(0xff27272a);
        t.text = juce::Colours::white;
        t.accent1 = juce::Colour(0xff00f0a8);
        t.accent2 = juce::Colour(0xff60a5fa);
        break;
    case 1: // Light
        t.background = juce::Colour(0xfff3f4f6); // gray 100
        t.panelBackground = juce::Colour(0xffffffff); // white
        t.border = juce::Colour(0xffd1d5db); // gray 300
        t.text = juce::Colour(0xff111827); // gray 900
        t.accent1 = juce::Colour(0xff3b82f6); // blue 500
        t.accent2 = juce::Colour(0xff10b981); // emerald 500
        break;
    case 2: // Synthwave
        t.background = juce::Colour(0xff1a0b2e);
        t.panelBackground = juce::Colour(0xff2d1b4e);
        t.border = juce::Colour(0xff4a2574);
        t.text = juce::Colour(0xfff8f9fa);
        t.accent1 = juce::Colour(0xffff007f); // neon pink
        t.accent2 = juce::Colour(0xff00ffff); // cyan
        break;
    case 3: // Cyberpunk
        t.background = juce::Colour(0xfffcee0a); // cyberpunk yellow
        t.panelBackground = juce::Colour(0xff111111); // black
        t.border = juce::Colour(0xff222222);
        t.text = juce::Colour(0xff00ffff); // cyan
        t.accent1 = juce::Colour(0xffff0055); // neon red
        t.accent2 = juce::Colour(0xfffcee0a);
        break;
    case 4: // High Contrast
        t.background = juce::Colours::black;
        t.panelBackground = juce::Colours::black;
        t.border = juce::Colours::white;
        t.text = juce::Colours::white;
        t.accent1 = juce::Colours::yellow;
        t.accent2 = juce::Colours::magenta;
        break;
    default:
        break;
    }
    setTheme(t);
}

void SettingsManager::setTheme(const Theme& theme)
{
    if (currentTheme != theme)
    {
        currentTheme = theme;
        if (onThemeChanged) onThemeChanged();
        save();
    }
}

void SettingsManager::setOscPort(int port)
{
    if (oscPort != port)
    {
        oscPort = port;
        if (onOscPortChanged) onOscPortChanged(oscPort);
        save();
    }
}

void SettingsManager::setOscOutPort(int port)
{
    if (oscOutPort != port)
    {
        oscOutPort = port;
        if (onOscOutPortChanged) onOscOutPortChanged(oscOutPort);
        save();
    }
}

void SettingsManager::setEnabledMidiDevices(const juce::StringArray& devices)
{
    enabledMidiDevices = devices;
    if (onMidiDevicesChanged) onMidiDevicesChanged(enabledMidiDevices);
    save();
}
