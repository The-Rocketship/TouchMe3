#include "MainComponent.h"

MainComponent::MainComponent()
{
    // Setup Menu
    menuBar = std::make_unique<juce::MenuBarComponent>(this);
    addAndMakeVisible(*menuBar);

    // Setup subcomponents
    propertiesPanel = std::make_unique<PropertiesComponent>(mediaEngine);
    clipGridPanel = std::make_unique<ClipGridComponent>(mediaEngine);
    monitorPanel = std::make_unique<MonitorComponent>(mediaEngine);
    nodeEditor = std::make_unique<NodeEditorComponent>();
    nodeEditor->setMappingManager(&mediaEngine.mappingManager);
    nodeEditor->setSettingsManager(&mediaEngine.settingsManager);

    addAndMakeVisible(*propertiesPanel);
    addAndMakeVisible(*clipGridPanel);
    addAndMakeVisible(*monitorPanel);
    addAndMakeVisible(*nodeEditor);

    // Connect Callbacks
    clipGridPanel->onClipSelected = [this](int layer, int col) {
        auto& previewClip = mediaEngine.getPreviewClip();
        propertiesPanel->updateDetails(previewClip);
        
        if (previewClip.isLoaded && previewClip.isNodeBased)
        {
            nodeEditor->setGraph(previewClip.nodeGraph);
            nodeEditor->setClipDuration(previewClip.transport.duration);
        }
        else
            nodeEditor->setGraph(nullptr);
    };

    clipGridPanel->onClipTriggered = [this](int layer, int col) {
        // If the triggered clip is also the currently selected preview clip, sync details
        if (mediaEngine.getSelectedLayer() == layer && mediaEngine.getSelectedCol() == col)
        {
            propertiesPanel->updateDetails(mediaEngine.getPreviewClip());
        }
    };

    propertiesPanel->onFileLoaded = [this](const juce::File& file) {
        int l = mediaEngine.getSelectedLayer();
        int c = mediaEngine.getSelectedCol();
        if (l >= 0 && c >= 0)
        {
            juce::StringArray files;
            files.add(file.getFullPathName());
            // Since ClipGridComponent needs an x, y for filesDropped, but we know the cell,
            // we can just call filesDropped with a mock position inside that cell, or refactor.
            // Actually, we can just do what filesDropped does right here!
            auto& clip = mediaEngine.getClipInGrid(l, c);
            clip.file = file;
            clip.name = file.getFileNameWithoutExtension();
            clip.isProcedural = false;
            clip.isLoaded = true;

            juce::String ext = file.getFileExtension().toLowerCase();
            if (ext == ".png" || ext == ".jpg" || ext == ".jpeg" || ext == ".bmp" || ext == ".gif")
            {
                juce::Image img = juce::ImageFileFormat::loadFrom(file);
                if (img.isValid())
                    clip.image = img;
            }
            else
            {
                clip.image = juce::Image(juce::Image::RGB, 100, 100, true);
                clip.transport.duration = 10.0;
            }

            clipGridPanel->markThumbnailDirty(l, c);
            mediaEngine.previewClipInGrid(l, c);
            propertiesPanel->updateDetails(mediaEngine.getPreviewClip());
            clipGridPanel->repaint();
        }
    };

    propertiesPanel->onPropertiesChanged = [this] {
        int l = mediaEngine.getSelectedLayer();
        int c = mediaEngine.getSelectedCol();
        if (l >= 0 && c >= 0)
        {
            auto& previewClip = mediaEngine.getPreviewClip();
            auto& gridClip = mediaEngine.getClipInGrid(l, c);

            // Sync UI inputs to clip states
            propertiesPanel->updateClipFromSliders(previewClip);
            propertiesPanel->updateClipFromSliders(gridClip);
            
            if (previewClip.videoClip != nullptr)
                previewClip.videoClip->setLooping(previewClip.transport.loop);
            
            // Sync envelopes and base values completely
            gridClip.transform = previewClip.transform;

            // If the clip is currently playing live on a layer, update its live transform too
            auto& activeClip = mediaEngine.getActiveLayerClip(l);
            if (activeClip.isLoaded && activeClip.sourceCol == c)
            {
                activeClip.transform = previewClip.transform;
                activeClip.transport.speed = previewClip.transport.speed;
                activeClip.transport.loop = previewClip.transport.loop;
                
                if (activeClip.videoClip != nullptr)
                    activeClip.videoClip->setLooping(activeClip.transport.loop);
                    
                activeClip.transport.isPlaying = previewClip.transport.isPlaying;
            }
        }
    };

    // Initialize Splitters
    leftSplitter = std::make_unique<ResizerBar>(true, [this](int screenX) {
        auto localPoint = getLocalPoint(nullptr, juce::Point<int>(screenX, 0));
        leftColumnWidth = juce::jlimit(200, getWidth() - 400, localPoint.x);
        resized();
    });
    addAndMakeVisible(*leftSplitter);

    rightSplitter = std::make_unique<ResizerBar>(true, [this](int screenX) {
        auto localPoint = getLocalPoint(nullptr, juce::Point<int>(screenX, 0));
        rightColumnWidth = juce::jlimit(250, getWidth() - 400, getWidth() - localPoint.x);
        resized();
    });
    addAndMakeVisible(*rightSplitter);

    bottomSplitter = std::make_unique<ResizerBar>(false, [this](int screenY) {
        auto localPoint = getLocalPoint(nullptr, juce::Point<int>(0, screenY));
        mainDividerY = juce::jlimit(150, getHeight() - 100, localPoint.y);
        resized();
    });
    addAndMakeVisible(*bottomSplitter);

    // Start Timer (60 FPS updates)
    lastTimeMs = juce::Time::getMillisecondCounter();
    startTimerHz(60);

    mediaEngine.settingsManager.onThemeChanged = [this]() {
        repaint();
        propertiesPanel->repaint();
        clipGridPanel->repaint();
        monitorPanel->repaint();
        nodeEditor->repaint();
    };

    setSize(1600, 900);
}

MainComponent::~MainComponent()
{
    stopTimer();
    leftSplitter.reset();
    rightSplitter.reset();
    bottomSplitter.reset();
}

void MainComponent::paint(juce::Graphics& g)
{
    auto t0 = juce::Time::getMillisecondCounterHiRes();

    g.fillAll(mediaEngine.settingsManager.getTheme().background);

    if (canvasSettingsOverlay != nullptr)
    {
        g.fillAll(juce::Colours::black.withAlpha(0.6f)); // Dim background
    }

    auto elapsed = juce::Time::getMillisecondCounterHiRes() - t0;
    if (elapsed > 5.0)
        DBG("MainComponent::paint took " + juce::String(elapsed, 1) + " ms");
}

void MainComponent::resized()
{
    auto area = getLocalBounds();
    int menuHeight = 24;
    menuBar->setBounds(area.removeFromTop(menuHeight));

    int topSectionHeight = mainDividerY - menuHeight;
    auto topArea = area.removeFromTop(topSectionHeight);

    // Bottom section divider
    bottomSplitter->setBounds(0, mainDividerY, getWidth(), 4);

    // Center Column: Clip Grid
    clipGridPanel->setBounds(topArea);
    
    // Bottom Section: Node Editor
    if (mainDividerY < getHeight())
    {
        nodeEditor->setBounds(0, mainDividerY + 4, getWidth(), getHeight() - mainDividerY - 4);
    }

    // Left Column: Properties Panel
    propertiesPanel->setBounds(topArea.removeFromLeft(leftColumnWidth));
    leftSplitter->setBounds(leftColumnWidth, menuHeight, 4, topSectionHeight);
    topArea.removeFromLeft(4); // spacing for resizer

    // Right Column: Monitors Panel
    int monitorX = getWidth() - rightColumnWidth;
    monitorPanel->setBounds(monitorX, menuHeight, rightColumnWidth, topSectionHeight);
    rightSplitter->setBounds(monitorX - 4, menuHeight, 4, topSectionHeight);
    
    // Middle Column: Playout Grid
    int gridWidth = monitorX - 4 - (leftColumnWidth + 4);
    clipGridPanel->setBounds(leftColumnWidth + 4, menuHeight, gridWidth, topSectionHeight);

    if (canvasSettingsOverlay != nullptr)
    {
        canvasSettingsOverlay->setBounds(getWidth() / 2 - 165, getHeight() / 2 - 170, 330, 340);
    }
}

void MainComponent::timerCallback()
{
    auto t0 = juce::Time::getMillisecondCounterHiRes();

    auto currentTimeMs = juce::Time::getMillisecondCounter();
    double deltaTime = (currentTimeMs - lastTimeMs) * 0.001;
    lastTimeMs = currentTimeMs;

    // Tick playhead animations
    mediaEngine.update(deltaTime);

    auto& previewClip = mediaEngine.getPreviewClip();

    int l = mediaEngine.getSelectedLayer();
    int c = mediaEngine.getSelectedCol();
    if (l >= 0 && c >= 0)
    {
        if (previewClip.isLoaded)
        {
            if (previewClip.transport.duration > 0.0)
            {
                double progress = previewClip.transport.position / previewClip.transport.duration;
                propertiesPanel->updateTimelinePosition(progress);
            }
            
            propertiesPanel->updateParameterVisuals(previewClip.transport.position);
            
            if (previewClip.isNodeBased)
            {
                nodeEditor->updateParameterVisuals(previewClip.transport.position);
            }

            // Continuously sync transforms so live envelope edits are instantly applied
            auto& gridClip = mediaEngine.getClipInGrid(l, c);
            gridClip.transform = previewClip.transform;

            auto& activeClip = mediaEngine.getActiveLayerClip(l);
            if (activeClip.isLoaded && activeClip.sourceCol == c)
            {
                activeClip.transform = previewClip.transform;
            }
        }
    }

    // Refresh screens unconditionally at 60Hz
    mediaEngine.updateCompositionFrame();
    monitorPanel->refreshMonitors();
    
    if (outputWindow != nullptr && outputWindow->isVisible())
    {
        outputWindow->repaint();
    }

    if (advancedOutputWindow != nullptr)
    {
        advancedOutputWindow->repaintDeviceWindows();
        if (advancedOutputWindow->isVisible())
            advancedOutputWindow->repaint();
    }

    auto elapsed = juce::Time::getMillisecondCounterHiRes() - t0;
    if (elapsed > 5.0)
        DBG("timerCallback took " + juce::String(elapsed, 1) + " ms");
}

// Menu Bar Configuration
juce::StringArray MainComponent::getMenuBarNames()
{
    return { "Composition", "Deck", "Group", "Layer", "Column", "Clip", "Output", "Shortcuts", "View" };
}

juce::PopupMenu MainComponent::getMenuForIndex(int menuIndex, const juce::String& menuName)
{
    juce::PopupMenu menu;
    if (menuName == "View")
    {
        menu.addItem(1, "Reset Layout");
        menu.addSeparator();
        menu.addItem(2, "Settings...");
    }
    else if (menuName == "Output")
    {
        menu.addItem(201, "Windowed");
        menu.addItem(202, "Fullscreen");
        menu.addItem(203, "Advanced...");
        menu.addItem(204, "Canvas Settings...");
    }
    else if (menuName == "Shortcuts")
    {
        menu.addItem(301, "Edit MIDI", true, mediaEngine.mappingManager.getEditMode() == MappingManager::EditMode::MIDI);
        menu.addItem(302, "Edit OSC", true, mediaEngine.mappingManager.getEditMode() == MappingManager::EditMode::OSC);
    }
    else if (menuName == "Composition")
    {
        menu.addItem(1001, "New");
        menu.addItem(1002, "Open...");
        menu.addItem(1003, "Save");
        menu.addItem(1004, "Save As...");
        menu.addSeparator();
        menu.addItem(1005, "Quit");
    }
    else if (menuName == "Deck")
    {
        menu.addItem(1101, "Clear Deck");
    }
    else if (menuName == "Group")
    {
        menu.addItem(1201, "Add Group");
    }
    else if (menuName == "Layer")
    {
        menu.addItem(1301, "Add Layer");
        menu.addItem(1302, "Remove Layer", true, mediaEngine.getSelectedLayer() >= 0);
        menu.addItem(1303, "Clear Layer", true, mediaEngine.getSelectedLayer() >= 0);
    }
    else if (menuName == "Column")
    {
        menu.addItem(1401, "Trigger Column", true, mediaEngine.getSelectedCol() >= 0);
        menu.addItem(1402, "Remove Column", true, mediaEngine.getSelectedCol() >= 0);
        menu.addItem(1403, "Clear Column", true, mediaEngine.getSelectedCol() >= 0);
    }
    else if (menuName == "Clip")
    {
        menu.addItem(1501, "Clear Clip", true, mediaEngine.getSelectedLayer() >= 0 && mediaEngine.getSelectedCol() >= 0);
    }
    return menu;
}

void MainComponent::menuItemSelected(int menuItemId, int topLevelMenuIndex)
{
    if (menuItemId == 1)
    {
        leftColumnWidth = 280;
        rightColumnWidth = 380;
        mainDividerY = 550;
        resized();
        repaint();
    }
    else if (menuItemId == 2)
    {
        if (settingsWindow == nullptr)
        {
            settingsWindow = std::make_unique<SettingsWindow>("Settings", mediaEngine);
        }
        settingsWindow->setVisible(true);
        settingsWindow->toFront(true);
    }
    else if (menuItemId == 201)
    {
        if (outputWindow == nullptr)
        {
            outputWindow = std::make_unique<OutputWindow>("Output Monitor", mediaEngine);
        }
        outputWindow->setVisible(true);
        outputWindow->toFront(true);
    }
    else if (menuItemId == 202)
    {
        juce::AlertWindow::showMessageBoxAsync(juce::AlertWindow::InfoIcon, "Output", "Output set to Fullscreen mode.");
    }
    else if (menuItemId == 203)
    {
        if (advancedOutputWindow == nullptr)
        {
            advancedOutputWindow = std::make_unique<AdvancedOutputWindow>("Advanced Output", mediaEngine);
        }
        advancedOutputWindow->setVisible(true);
        advancedOutputWindow->toFront(true);
    }
    else if (menuItemId == 204)
    {
        canvasSettingsOverlay = std::make_unique<CanvasSettingsOverlay>(
            [this](juce::String name, int w, int h, juce::String fps, juce::String depth) {
                juce::AlertWindow::showMessageBoxAsync(juce::AlertWindow::InfoIcon, "Canvas Settings", 
                    "Applied settings:\nName: " + name + "\nResolution: " + juce::String(w) + "x" + juce::String(h) + 
                    "\nFrameRate: " + fps + "\nColor Depth: " + depth);
                canvasSettingsOverlay.reset();
                resized();
                repaint();
            },
            [this] {
                canvasSettingsOverlay.reset();
                resized();
                repaint();
            }
        );
        addAndMakeVisible(*canvasSettingsOverlay);
        resized();
        repaint();
    }
    else if (menuItemId == 301) // Edit MIDI
    {
        if (mediaEngine.mappingManager.getEditMode() == MappingManager::EditMode::MIDI)
            mediaEngine.mappingManager.setEditMode(MappingManager::EditMode::None);
        else
            mediaEngine.mappingManager.setEditMode(MappingManager::EditMode::MIDI);
            
        mediaEngine.mappingManager.setLearningPath("");
        repaint();
        propertiesPanel->repaint();
        nodeEditor->triggerPropertiesRepaint();
    }
    else if (menuItemId == 302) // Edit OSC
    {
        if (mediaEngine.mappingManager.getEditMode() == MappingManager::EditMode::OSC)
            mediaEngine.mappingManager.setEditMode(MappingManager::EditMode::None);
        else
            mediaEngine.mappingManager.setEditMode(MappingManager::EditMode::OSC);
            
        mediaEngine.mappingManager.setLearningPath("");
        repaint();
        propertiesPanel->repaint();
        nodeEditor->triggerPropertiesRepaint();
    }
    else if (menuItemId == 1001) // Composition -> New
    {
        mediaEngine.clearDeck();
        repaint();
    }
    else if (menuItemId == 1002 || menuItemId == 1003 || menuItemId == 1004) // Composition -> Open/Save
    {
        juce::AlertWindow::showMessageBoxAsync(juce::AlertWindow::InfoIcon, "Coming Soon", "Save/Load architecture is not fully implemented yet.");
    }
    else if (menuItemId == 1005) // Composition -> Quit
    {
        juce::JUCEApplication::getInstance()->systemRequestedQuit();
    }
    else if (menuItemId == 1101) // Deck -> Clear Deck
    {
        mediaEngine.clearDeck();
        repaint();
    }
    else if (menuItemId == 1201) // Group -> Add Group
    {
        juce::AlertWindow::showMessageBoxAsync(juce::AlertWindow::InfoIcon, "Coming Soon", "Groups are not fully implemented yet.");
    }
    else if (menuItemId == 1301) // Layer -> Add Layer
    {
        mediaEngine.addLayer();
        resized();
        repaint();
    }
    else if (menuItemId == 1302) // Layer -> Remove Layer
    {
        int sel = mediaEngine.getSelectedLayer();
        if (sel >= 0) {
            mediaEngine.removeLayer(sel);
            resized();
            repaint();
        }
    }
    else if (menuItemId == 1303) // Layer -> Clear Layer
    {
        int sel = mediaEngine.getSelectedLayer();
        if (sel >= 0) {
            mediaEngine.clearLayer(sel);
            repaint();
        }
    }
    else if (menuItemId == 1401) // Column -> Trigger Column
    {
        int sel = mediaEngine.getSelectedCol();
        if (sel >= 0) {
            mediaEngine.triggerColumn(sel);
            repaint();
        }
    }
    else if (menuItemId == 1402) // Column -> Remove Column
    {
        int sel = mediaEngine.getSelectedCol();
        if (sel >= 0) {
            mediaEngine.removeColumn(sel);
            resized();
            repaint();
        }
    }
    else if (menuItemId == 1403) // Column -> Clear Column
    {
        int sel = mediaEngine.getSelectedCol();
        if (sel >= 0) {
            mediaEngine.clearColumn(sel);
            repaint();
        }
    }
    else if (menuItemId == 1501) // Clip -> Clear Clip
    {
        int selL = mediaEngine.getSelectedLayer();
        int selC = mediaEngine.getSelectedCol();
        if (selL >= 0 && selC >= 0) {
            mediaEngine.clearClip(selL, selC);
            repaint();
        }
    }
}
