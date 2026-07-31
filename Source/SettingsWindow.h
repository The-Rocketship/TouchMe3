#pragma once
#include <JuceHeader.h>
#include "MediaEngine.h"

class SettingsWindow : public juce::DocumentWindow
{
public:
    SettingsWindow(juce::String name, MediaEngine& mediaEngine);
    ~SettingsWindow() override;

    void closeButtonPressed() override;

private:
    class SettingsContent : public juce::Component
    {
    public:
        SettingsContent(MediaEngine& engine);
        ~SettingsContent() override = default;

        void paint(juce::Graphics& g) override;
        void resized() override;

    private:
        MediaEngine& mediaEngine;

        juce::TabbedComponent tabbedComponent{juce::TabbedButtonBar::Orientation::TabsAtTop};

        // OSC Tab
        class OscTab : public juce::Component
        {
        public:
            OscTab(MediaEngine& engine);
            ~OscTab() override;
            void resized() override;
        private:
            MediaEngine& mediaEngine;
            juce::Label oscInPortLabel;
            juce::TextEditor oscInPortInput;
            juce::Label oscOutPortLabel;
            juce::TextEditor oscOutPortInput;
            juce::Label logLabel;
            juce::TextEditor logEditor;
        };

        // MIDI Tab
        class MidiTab : public juce::Component
        {
        public:
            MidiTab(MediaEngine& engine);
            void resized() override;
        private:
            MediaEngine& mediaEngine;
            juce::Label midiLabel;
            juce::ListBox midiListBox;

            struct MidiListModel : public juce::ListBoxModel
            {
                MediaEngine& mediaEngine;
                juce::Array<juce::MidiDeviceInfo> devices;
                
                MidiListModel(MediaEngine& engine);
                int getNumRows() override;
                void paintListBoxItem(int rowNumber, juce::Graphics& g, int width, int height, bool rowIsSelected) override;
                juce::Component* refreshComponentForRow(int rowNumber, bool isRowSelected, juce::Component* existingComponentToUpdate) override;
            } midiModel;
        };

        // Theme Tab
        class ThemeTab : public juce::Component, public juce::ComboBox::Listener, public juce::ChangeListener
        {
        public:
            ThemeTab(MediaEngine& engine);
            ~ThemeTab() override;
            void resized() override;
            void comboBoxChanged(juce::ComboBox* comboBoxThatHasChanged) override;
            void changeListenerCallback(juce::ChangeBroadcaster* source) override;
        private:
            MediaEngine& mediaEngine;
            juce::Label presetLabel;
            juce::ComboBox presetDropdown;
            
            juce::Label bgLabel, panelBgLabel, borderLabel, textLabel, accent1Label, accent2Label;
            juce::ColourSelector bgSelector, panelBgSelector, borderSelector, textSelector, accent1Selector, accent2Selector;
            
            void updatePickersFromTheme();
            void applyCustomTheme();
        };

        OscTab oscTab;
        MidiTab midiTab;
        ThemeTab themeTab;
    };

    SettingsContent contentComponent;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SettingsWindow)
};
