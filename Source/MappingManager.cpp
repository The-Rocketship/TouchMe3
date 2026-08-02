#include "MappingManager.h"

MappingManager::MappingManager()
{
    // Will be initialized via setupFromSettings
}

void MappingManager::setupFromSettings(int oscInPort, int oscOutPort, const juce::StringArray& midiDevices)
{
    deviceManager.initialise(0, 2, nullptr, true);
    setMidiDevices(midiDevices);
    reconnectOscIn(oscInPort);
    reconnectOscOut(oscOutPort);
}

void MappingManager::reconnectOscIn(int port)
{
    disconnect();
    if (!connect(port)) {
        juce::Logger::writeToLog("MappingManager: Failed to connect OSC IN to port " + juce::String(port));
    }
    addListener(this);
}

void MappingManager::reconnectOscOut(int port)
{
    oscSender.disconnect();
    if (!oscSender.connect("127.0.0.1", port)) {
        juce::Logger::writeToLog("MappingManager: Failed to connect OSC OUT to port " + juce::String(port));
    }
}

void MappingManager::setMidiDevices(const juce::StringArray& devices)
{
    auto midiInputs = juce::MidiInput::getAvailableDevices();
    for (const auto& dev : midiInputs) {
        if (devices.contains(dev.identifier)) {
            deviceManager.setMidiInputDeviceEnabled(dev.identifier, true);
            deviceManager.addMidiInputDeviceCallback(dev.identifier, this);
        } else {
            deviceManager.removeMidiInputDeviceCallback(dev.identifier, this);
            deviceManager.setMidiInputDeviceEnabled(dev.identifier, false);
        }
    }
}

MappingManager::~MappingManager()
{
    auto midiInputs = juce::MidiInput::getAvailableDevices();
    for (const auto& dev : midiInputs) {
        deviceManager.removeMidiInputDeviceCallback(dev.identifier, this);
    }
    disconnect();
}

void MappingManager::registerParameters(std::shared_ptr<NodeGraph> graph)
{
    std::lock_guard<std::mutex> lk(mappingMutex);
    currentGraph = graph;
    registeredParams.clear();

    if (!currentGraph) return;

    for (const auto& node : currentGraph->nodes) {
        auto params = node->getParameters();
        for (const auto& p : params) {
            juce::String path = "/node/" + juce::String(node->id) + "/" + p.first;
            registeredParams[path] = p.second;
        }
    }
}

void MappingManager::setLearningPath(const juce::String& path)
{
    std::lock_guard<std::mutex> lk(mappingMutex);
    learningPath = path;
}

bool MappingManager::isMapped(const juce::String& path) const
{
    for (const auto& pair : midiBindings) {
        if (pair.second == path) return true;
    }
    for (const auto& pair : oscBindings) {
        if (pair.second == path) return true;
    }
    return false;
}

juce::String MappingManager::getMappingDescription(const juce::String& path) const
{
    juce::StringArray desc;
    for (const auto& pair : midiBindings) {
        if (pair.second == path) desc.add("CC " + juce::String(pair.first));
    }
    for (const auto& pair : oscBindings) {
        if (pair.second == path) desc.add("OSC: " + pair.first);
    }
    return desc.joinIntoString(", ");
}

void MappingManager::bindMidiCC(int ccNumber)
{
    midiBindings[ccNumber] = learningPath;
    juce::Logger::writeToLog("Mapped MIDI CC " + juce::String(ccNumber) + " to " + learningPath);
    learningPath.clear();
    if (onMappingLearned) juce::MessageManager::callAsync(onMappingLearned);
}

void MappingManager::bindOscAddress(const juce::String& address)
{
    oscBindings[address] = learningPath;
    juce::Logger::writeToLog("Mapped OSC " + address + " to " + learningPath);
    learningPath.clear();
    if (onMappingLearned) juce::MessageManager::callAsync(onMappingLearned);
}

void MappingManager::updateParameter(const juce::String& path, float value)
{
    if (registeredParams.count(path)) {
        registeredParams[path]->baseValue = value;
    }
    if (onGenericParameterUpdate) {
        onGenericParameterUpdate(path, value);
    }
}

void MappingManager::handleIncomingMidiMessage(juce::MidiInput* /*source*/, const juce::MidiMessage& message)
{
    if (message.isController()) {
        int cc = message.getControllerNumber();
        float val = message.getControllerValue() / 127.0f; // Normalize 0-1

        std::lock_guard<std::mutex> lk(mappingMutex);
        if (currentEditMode == EditMode::MIDI && learningPath.isNotEmpty()) {
            bindMidiCC(cc);
            // We can optionally apply the first value immediately
            updateParameter(midiBindings[cc], val);
        } else if (midiBindings.count(cc)) {
            updateParameter(midiBindings[cc], val);
        }
    }
}

void MappingManager::oscMessageReceived(const juce::OSCMessage& message)
{
    juce::String address = message.getAddressPattern().toString();
    float val = 0.0f;

    juce::String logStr = "[" + juce::Time::getCurrentTime().formatted("%H:%M:%S") + "] IN: " + address;

    if (!message.isEmpty()) {
        if (message[0].isFloat32()) {
            val = message[0].getFloat32();
            logStr += " " + juce::String(val);
        } else if (message[0].isInt32()) {
            val = (float)message[0].getInt32();
            logStr += " " + juce::String(val);
        }
    }

    if (onOscMessageReceived) {
        onOscMessageReceived(logStr);
    }

    if (!message.isEmpty() && message[0].isFloat32()) {
        val = message[0].getFloat32();
    } else if (!message.isEmpty() && message[0].isInt32()) {
        val = (float)message[0].getInt32(); // might need scaling?
    }

    std::lock_guard<std::mutex> lk(mappingMutex);
    if (currentEditMode == EditMode::OSC && learningPath.isNotEmpty()) {
        bindOscAddress(address);
        updateParameter(oscBindings[address], val);
    } else if (oscBindings.count(address)) {
        updateParameter(oscBindings[address], val);
    } else {
        // Automatic routing if the address exactly matches an internal mapping path
        updateParameter(address, val);
    }
}

void MappingManager::oscBundleReceived(const juce::OSCBundle& bundle)
{
    for (const auto& elem : bundle) {
        if (elem.isMessage()) oscMessageReceived(elem.getMessage());
        else if (elem.isBundle()) oscBundleReceived(elem.getBundle());
    }
}
