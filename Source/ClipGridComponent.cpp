#include "ClipGridComponent.h"

ClipGridComponent::ClipGridComponent(MediaEngine& engine)
    : mediaEngine(engine)
{
    rebuildUI();
}

void ClipGridComponent::rebuildUI()
{
    // Clear old UI
    layerControls.clear();
    thumbnailCache.clear();
    thumbnailDirty.clear();

    int nLayers = mediaEngine.getNumLayers();
    int nCols = mediaEngine.getNumCols();

    layerControls.resize(nLayers);
    thumbnailCache.resize(nLayers, std::vector<juce::Image>(nCols));
    thumbnailDirty.resize(nLayers, std::vector<bool>(nCols, false));

    // Configure Look and feel or colors
    for (int l = 0; l < nLayers; ++l)
    {
        auto& ctrl = layerControls[l];

        // Opacity Slider
        ctrl.opacitySlider = std::make_unique<juce::Slider>(juce::Slider::LinearHorizontal, juce::Slider::NoTextBox);
        ctrl.opacitySlider->setRange(0.0, 1.0, 0.01);
        ctrl.opacitySlider->setValue(mediaEngine.getLayerOpacity(l));
        ctrl.opacitySlider->setColour(juce::Slider::trackColourId, mediaEngine.settingsManager.getTheme().accent1);
        ctrl.opacitySlider->setColour(juce::Slider::backgroundColourId, mediaEngine.settingsManager.getTheme().background);
        ctrl.opacitySlider->setColour(juce::Slider::thumbColourId, mediaEngine.settingsManager.getTheme().text);
        
        int layerIdx = l;
        ctrl.opacitySlider->onValueChange = [this, layerIdx] {
            mediaEngine.setLayerOpacity(layerIdx, layerControls[layerIdx].opacitySlider->getValue());
        };
        addAndMakeVisible(*ctrl.opacitySlider);

        // Bypass Button
        ctrl.bypassButton = std::make_unique<juce::TextButton>("B");
        ctrl.bypassButton->setClickingTogglesState(true);
        ctrl.bypassButton->setToggleState(mediaEngine.isLayerBypassed(l), juce::dontSendNotification);
        ctrl.bypassButton->setColour(juce::TextButton::buttonColourId, mediaEngine.settingsManager.getTheme().background);
        ctrl.bypassButton->setColour(juce::TextButton::buttonOnColourId, mediaEngine.settingsManager.getTheme().accent1);
        ctrl.bypassButton->setColour(juce::TextButton::textColourOffId, mediaEngine.settingsManager.getTheme().text);
        ctrl.bypassButton->setColour(juce::TextButton::textColourOnId, mediaEngine.settingsManager.getTheme().text);
        ctrl.bypassButton->onClick = [this, layerIdx] {
            mediaEngine.setLayerBypassed(layerIdx, layerControls[layerIdx].bypassButton->getToggleState());
        };
        addAndMakeVisible(*ctrl.bypassButton);

        // Solo Button
        ctrl.soloButton = std::make_unique<juce::TextButton>("S");
        ctrl.soloButton->setClickingTogglesState(true);
        ctrl.soloButton->setToggleState(mediaEngine.getSoloedLayer() == l, juce::dontSendNotification);
        ctrl.soloButton->setColour(juce::TextButton::buttonColourId, mediaEngine.settingsManager.getTheme().background);
        ctrl.soloButton->setColour(juce::TextButton::buttonOnColourId, mediaEngine.settingsManager.getTheme().accent2);
        ctrl.soloButton->onClick = [this, layerIdx] {
            bool isSoloed = layerControls[layerIdx].soloButton->getToggleState();
            if (isSoloed)
            {
                mediaEngine.setSoloedLayer(layerIdx);
                // Turn off all other solo buttons
                for (int otherL = 0; otherL < mediaEngine.getNumLayers(); ++otherL)
                {
                    if (otherL != layerIdx)
                        layerControls[otherL].soloButton->setToggleState(false, juce::dontSendNotification);
                }
            }
            else
            {
                if (mediaEngine.getSoloedLayer() == layerIdx)
                    mediaEngine.setSoloedLayer(-1);
            }
            repaint();
        };
        addAndMakeVisible(*ctrl.soloButton);

        // Mute Button
        ctrl.muteButton = std::make_unique<juce::TextButton>("M");
        ctrl.muteButton->setClickingTogglesState(true);
        ctrl.muteButton->setColour(juce::TextButton::buttonColourId, mediaEngine.settingsManager.getTheme().background);
        ctrl.muteButton->setColour(juce::TextButton::buttonOnColourId, juce::Colours::orange);
        addAndMakeVisible(*ctrl.muteButton);
    }
    
    resized();
    repaint();
}

void ClipGridComponent::paint(juce::Graphics& g)
{
    g.fillAll(mediaEngine.settingsManager.getTheme().panelBackground);

    // Paint Top Column Headers
    g.setColour(mediaEngine.settingsManager.getTheme().border);
    g.fillRect(0, 0, getWidth(), topHeaderHeight);

    int selectedCol = mediaEngine.getSelectedCol();
    int selectedLayer = mediaEngine.getSelectedLayer();

    for (int c = 0; c < mediaEngine.getNumCols(); ++c)
    {
        int x = leftHeaderWidth + c * cellWidth;
        
        if (c == selectedCol)
        {
            g.setColour(mediaEngine.settingsManager.getTheme().accent1.withAlpha(0.2f));
            g.fillRect(x, 0, cellWidth, topHeaderHeight);
            g.setColour(mediaEngine.settingsManager.getTheme().accent1);
            g.drawRect(x, 0, cellWidth, topHeaderHeight, 1);
        }

        g.setColour(mediaEngine.settingsManager.getTheme().text);
        g.setFont(12.0f);
        g.drawText("Column " + juce::String(c + 1), x, 0, cellWidth, topHeaderHeight, juce::Justification::centred);
        
        // Column vertical grid line
        g.setColour(mediaEngine.settingsManager.getTheme().border);
        g.drawVerticalLine(x, 0.0f, (float)getHeight());
    }

    // Paint Layer Headers and Cells
    // NOTE: Layer 8 is drawn at the top (layer index 7), Layer 1 at the bottom (layer index 0)
    for (int l = 0; l < mediaEngine.getNumLayers(); ++l)
    {
        int drawLayerIdx = mediaEngine.getNumLayers() - 1 - l; // inverted drawing index
        int y = topHeaderHeight + l * cellHeight;

        // Layer Header area background
        if (drawLayerIdx == selectedLayer)
        {
            g.setColour(mediaEngine.settingsManager.getTheme().accent1.withAlpha(0.2f));
            g.fillRect(0, y, leftHeaderWidth, cellHeight);
            g.setColour(mediaEngine.settingsManager.getTheme().accent1);
            g.drawRect(0, y, leftHeaderWidth, cellHeight, 1);
        }
        else
        {
            g.setColour(mediaEngine.settingsManager.getTheme().border);
            g.fillRect(0, y, leftHeaderWidth, cellHeight);
        }

        // Draw Layer Label
        g.setColour(mediaEngine.settingsManager.getTheme().text);
        g.setFont(11.0f);
        g.drawText("Layer " + juce::String(drawLayerIdx + 1), 6, y + 4, 60, 16, juce::Justification::left);

        // Draw cells
        for (int c = 0; c < mediaEngine.getNumCols(); ++c)
        {
            int x = leftHeaderWidth + c * cellWidth;
            auto& clip = mediaEngine.getClipInGrid(drawLayerIdx, c);

            // Determine colors based on active / selected state
            bool isSelected = (mediaEngine.getSelectedLayer() == drawLayerIdx && mediaEngine.getSelectedCol() == c);
            bool isPlaying = mediaEngine.getActiveLayerClip(drawLayerIdx).isLoaded && 
                             mediaEngine.getActiveLayerClip(drawLayerIdx).sourceCol == c;

            // Background of cell
            g.setColour(mediaEngine.settingsManager.getTheme().background);
            g.fillRect(x + 2, y + 2, cellWidth - 4, cellHeight - 4);

            if (clip.isLoaded)
            {
                // Use cached thumbnail instead of re-rendering procedurals every frame
                float previewW = (float)cellWidth - 4;
                float previewH = (float)cellHeight - 16;
                ensureThumbnail(drawLayerIdx, c, previewW, previewH);
                
                auto& thumb = thumbnailCache[drawLayerIdx][c];
                if (thumb.isValid())
                {
                    g.drawImage(thumb, x + 2, y + 2, (int)previewW, (int)previewH,
                                0, 0, thumb.getWidth(), thumb.getHeight());
                }

                // Draw dark overlay for text readability at the bottom
                g.setColour(mediaEngine.settingsManager.getTheme().background.withAlpha(0.85f));
                g.fillRect(x + 2, y + cellHeight - 14, cellWidth - 4, 12);

                // Draw text
                g.setColour(mediaEngine.settingsManager.getTheme().text);
                g.setFont(8.5f);
                g.drawText(clip.name, x + 4, y + cellHeight - 14, cellWidth - 8, 12, juce::Justification::centred);
            }
            else
            {
                // Empty cell placeholder
                g.setColour(mediaEngine.settingsManager.getTheme().panelBackground.withAlpha(0.5f));
                g.drawRect(x + 2, y + 2, cellWidth - 4, cellHeight - 4, 1);
                
                g.setColour(mediaEngine.settingsManager.getTheme().border);
                g.setFont(8.5f);
                g.drawText("-", x + 4, y + 4, cellWidth - 8, cellHeight - 8, juce::Justification::centred);
            }

            // Outline playing or selected cell
            if (isPlaying)
            {
                g.setColour(mediaEngine.settingsManager.getTheme().accent1); // Neon green border for playing
                g.drawRect(x + 1, y + 1, cellWidth - 2, cellHeight - 2, 2);
            }
            else if (isSelected)
            {
                g.setColour(mediaEngine.settingsManager.getTheme().accent2); // Mint green border for selected
                g.drawRect(x + 1, y + 1, cellWidth - 2, cellHeight - 2, 2);
            }
            else
            {
                g.setColour(mediaEngine.settingsManager.getTheme().border); // Standard border
                g.drawRect(x + 1, y + 1, cellWidth - 2, cellHeight - 2, 1);
            }
        }

        // Horizontal line separator
        g.setColour(mediaEngine.settingsManager.getTheme().border);
        g.drawHorizontalLine(y + cellHeight, 0.0f, (float)getWidth());
    }
}

void ClipGridComponent::resized()
{
    // Place layer controls in the left header column
    for (int l = 0; l < mediaEngine.getNumLayers(); ++l)
    {
        int drawLayerIdx = mediaEngine.getNumLayers() - 1 - l; // inverted index
        int y = topHeaderHeight + l * cellHeight;

        auto& ctrl = layerControls[drawLayerIdx];
        
        // Layout: name at top-left.
        // Solo/Mute/Bypass buttons on the right.
        // Opacity slider at the bottom.
        ctrl.bypassButton->setBounds(leftHeaderWidth - 85, y + 4, 22, 18);
        ctrl.soloButton->setBounds(leftHeaderWidth - 60, y + 4, 22, 18);
        ctrl.muteButton->setBounds(leftHeaderWidth - 35, y + 4, 22, 18);
        
        ctrl.opacitySlider->setBounds(6, y + 24, leftHeaderWidth - 16, 16);
    }
}

bool ClipGridComponent::getCellAtPos(juce::Point<int> pos, int& layer, int& col)
{
    if (pos.x < leftHeaderWidth || pos.y < topHeaderHeight)
        return false;

    col = (pos.x - leftHeaderWidth) / cellWidth;
    int visualLayerRow = (pos.y - topHeaderHeight) / cellHeight;
    layer = mediaEngine.getNumLayers() - 1 - visualLayerRow;

    return (col >= 0 && col < mediaEngine.getNumCols() && layer >= 0 && layer < mediaEngine.getNumLayers());
}

void ClipGridComponent::mouseDown(const juce::MouseEvent& event)
{
    if (event.mods.isRightButtonDown())
    {
        int c = (event.getPosition().x - leftHeaderWidth) / cellWidth;
        int visualLayerRow = (event.getPosition().y - topHeaderHeight) / cellHeight;
        int l = mediaEngine.getNumLayers() - 1 - visualLayerRow;

        bool isColHeader = (event.getPosition().y < topHeaderHeight && c >= 0 && c < mediaEngine.getNumCols());
        bool isLayerHeader = (event.getPosition().x < leftHeaderWidth && l >= 0 && l < mediaEngine.getNumLayers());

        int cellL = -1, cellC = -1;
        bool isCell = getCellAtPos(event.getPosition(), cellL, cellC);

        juce::PopupMenu menu;
        if (isColHeader)
        {
            menu.addItem(1, "Remove Column");
            menu.showMenuAsync(juce::PopupMenu::Options(), [this, c](int result) {
                if (result == 1) {
                    mediaEngine.removeColumn(c);
                    rebuildUI();
                }
            });
        }
        else if (isLayerHeader)
        {
            menu.addItem(1, "Remove Layer");
            menu.showMenuAsync(juce::PopupMenu::Options(), [this, l](int result) {
                if (result == 1) {
                    mediaEngine.removeLayer(l);
                    rebuildUI();
                }
            });
        }
        else if (isCell)
        {
            auto& clip = mediaEngine.getClipInGrid(cellL, cellC);
            if (!clip.isLoaded)
            {
                menu.addItem(1, "Add Media Clip");
                menu.addItem(2, "Add Node Clip");
                menu.showMenuAsync(juce::PopupMenu::Options(), [this, cellL, cellC](int result) {
                    if (result == 1) {
                        promptLoadMediaClip(cellL, cellC);
                    } else if (result == 2) {
                        addNodeClip(cellL, cellC);
                    }
                });
            }
        }
        return;
    }

    int l = -1, c = -1;
    if (getCellAtPos(event.getPosition(), l, c))
    {
        mediaEngine.previewClipInGrid(l, c);
        if (onClipSelected)
            onClipSelected(l, c);
        repaint();
    }
}

void ClipGridComponent::mouseDoubleClick(const juce::MouseEvent& event)
{
    int l = -1, c = -1;
    if (getCellAtPos(event.getPosition(), l, c))
    {
        auto& clip = mediaEngine.getClipInGrid(l, c);
        if (clip.isLoaded)
        {
            mediaEngine.triggerClip(l, c);
            if (onClipTriggered)
                onClipTriggered(l, c);
            repaint();
        }
        else
        {
            promptLoadMediaClip(l, c);
        }
    }
}

void ClipGridComponent::promptLoadMediaClip(int l, int c)
{
    // Open native file explorer dialog to load video/image file
    fileChooser = std::make_unique<juce::FileChooser>(
        "Select a VJ clip...",
        juce::File(),
        "*.png;*.jpg;*.jpeg;*.mp4;*.mov;*.avi"
    );

    auto browseFlags = juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles;

    fileChooser->launchAsync(browseFlags, [this, l, c](const juce::FileChooser& chooser) {
        auto resultFile = chooser.getResult();
        if (resultFile.existsAsFile())
        {
            auto& gridClip = mediaEngine.getClipInGrid(l, c);
            gridClip.file = resultFile;
            gridClip.name = resultFile.getFileNameWithoutExtension();
            gridClip.isProcedural = false;
            gridClip.isNodeBased = false;
            gridClip.isLoaded = true;

            // Try loading as image
            juce::Image img = juce::ImageFileFormat::loadFrom(resultFile);
            if (img.isValid())
            {
                gridClip.image = img;
            }
            else
            {
                // Represent video as a visual placeholder
                gridClip.image = juce::Image(juce::Image::RGB, 100, 100, true);
                gridClip.transport.duration = 10.0; // default initial duration
            }

            thumbnailDirty[l][c] = true;
            repaint();
            
            // Re-select if it was selected
            if (mediaEngine.getSelectedLayer() == l && mediaEngine.getSelectedCol() == c)
            {
                mediaEngine.previewClipInGrid(l, c);
                if (onClipSelected)
                    onClipSelected(l, c);
            }
        }
    });
}

void ClipGridComponent::addNodeClip(int l, int c)
{
    auto& gridClip = mediaEngine.getClipInGrid(l, c);
    gridClip.isLoaded = true;
    gridClip.isProcedural = false;
    gridClip.isNodeBased = true;
    gridClip.name = "Node Clip";
    
    gridClip.nodeGraph = std::make_shared<NodeGraph>();
    auto solidNode = std::make_shared<SolidColourNode>(1, 100, 100);
    auto outputNode = std::make_shared<OutputNode>(2, 300, 100);
    
    gridClip.nodeGraph->addNode(solidNode);
    gridClip.nodeGraph->addNode(outputNode);
    gridClip.nodeGraph->addLink(1, 0, 2, 0);

    thumbnailDirty[l][c] = true;
    repaint();
    
    // Auto-select the newly created clip
    mediaEngine.previewClipInGrid(l, c);
    if (onClipSelected)
        onClipSelected(l, c);
}

bool ClipGridComponent::isInterestedInFileDrag(const juce::StringArray& files)
{
    // Interested in images or videos
    for (auto& file : files)
    {
        juce::File f(file);
        juce::String ext = f.getFileExtension().toLowerCase();
        if (ext == ".png" || ext == ".jpg" || ext == ".jpeg" || ext == ".mp4" || ext == ".mov" || ext == ".avi")
            return true;
    }
    return false;
}

void ClipGridComponent::filesDropped(const juce::StringArray& files, int x, int y)
{
    int l = -1, c = -1;
    if (getCellAtPos(juce::Point<int>(x, y), l, c))
    {
        juce::File file(files[0]);
        auto& clip = mediaEngine.getClipInGrid(l, c);
        clip.file = file;
        clip.name = file.getFileNameWithoutExtension();
        clip.isProcedural = false;
        clip.isLoaded = true;

        // Try loading as image
        juce::Image img = juce::ImageFileFormat::loadFrom(file);
        if (img.isValid())
        {
            clip.image = img;
        }
        else
        {
            // Just represent as a video mock image placeholder
            clip.image = juce::Image(juce::Image::RGB, 100, 100, true);
            clip.transport.duration = 10.0; // default initial duration
        }

        thumbnailDirty[l][c] = true;

        mediaEngine.previewClipInGrid(l, c);
        if (onClipSelected)
            onClipSelected(l, c);

        repaint();
    }
}

void ClipGridComponent::updateGrid()
{
    repaint();
}

void ClipGridComponent::ensureThumbnail(int layer, int col, float w, float h)
{
    auto& clip = mediaEngine.getClipInGrid(layer, col);
    auto& thumb = thumbnailCache[layer][col];
    
    // Only regenerate if the cache is empty/wrong size or marked dirty
    if (thumb.isValid() && thumb.getWidth() == (int)w && thumb.getHeight() == (int)h && !thumbnailDirty[layer][col])
        return;
    
    // Render clip to an offscreen image
    thumb = juce::Image(juce::Image::ARGB, (int)w, (int)h, true);
    juce::Graphics tg(thumb);
    
    ClipState thumbnailClip = clip;
    thumbnailClip.transform.posX = 0;
    thumbnailClip.transform.posY = 0;
    thumbnailClip.transform.scale = 1.0;
    thumbnailClip.transform.scaleX = 1.0;
    thumbnailClip.transform.scaleY = 1.0;
    thumbnailClip.transform.rotationZ = 0.0;
    thumbnailClip.transform.anchorX = 0.5;
    thumbnailClip.transform.anchorY = 0.5;
    thumbnailClip.transform.opacity = 1.0;
    
    mediaEngine.renderClip(tg, thumbnailClip, w, h, 1.0f, true);
    thumbnailDirty[layer][col] = false;
}
