#pragma once
#include <JuceHeader.h>
#include <map>
#include <mutex>
#include "NodeGraph.h"

class MappingManager : public juce::MidiInputCallback,
                       public juce::OSCReceiver,
                       public juce::OSCReceiver::Listener<juce::OSCReceiver::MessageLoopCallback>
{
public:
    enum class EditMode { None, MIDI, OSC };

    MappingManager();
    ~MappingManager() override;

    void setupFromSettings(int oscInPort, int oscOutPort, const juce::StringArray& midiDevices);
    void reconnectOscIn(int port);
    void reconnectOscOut(int port);
    void setMidiDevices(const juce::StringArray& devices);

    // Call this whenever the active NodeGraph changes or nodes are added/removed
    void registerParameters(std::shared_ptr<NodeGraph> graph);

    void setEditMode(EditMode mode) { currentEditMode = mode; }
    EditMode getEditMode() const { return currentEditMode; }

    std::function<void(const juce::String& path, float value)> onGenericParameterUpdate;

    // Call this from UI to start learning for a specific parameter path
    void setLearningPath(const juce::String& path);
    juce::String getLearningPath() const { return learningPath; }

    // Check if a path is mapped
    bool isMapped(const juce::String& path) const;
    juce::String getMappingDescription(const juce::String& path) const;

    // juce::MidiInputCallback
    void handleIncomingMidiMessage(juce::MidiInput* source, const juce::MidiMessage& message) override;
    
    // juce::OSCReceiver::Listener
    void oscMessageReceived(const juce::OSCMessage& message) override;
    void oscBundleReceived(const juce::OSCBundle& bundle) override;

    // UI Callback when a mapping is learned
    std::function<void()> onMappingLearned;

    // UI Callback for debug logging
    std::function<void(const juce::String&)> onOscMessageReceived;

private:
    void bindMidiCC(int ccNumber);
    void bindOscAddress(const juce::String& address);
    void updateParameter(const juce::String& path, float value);

    std::shared_ptr<NodeGraph> currentGraph;

    // Mappings
    std::map<int, juce::String> midiBindings;
    std::map<juce::String, juce::String> oscBindings;
    
    // Active parameters mapped by path: e.g. "/node/1/Amplitude" -> Parameter*
    std::map<juce::String, Parameter*> registeredParams;

    // State
    EditMode currentEditMode = EditMode::None;
    juce::String learningPath;
    std::mutex mappingMutex;

    juce::AudioDeviceManager deviceManager;
    juce::OSCSender oscSender;
};
