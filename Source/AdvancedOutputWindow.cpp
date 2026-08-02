#include "AdvancedOutputWindow.h"

#ifndef GL_FALSE
#define GL_FALSE 0
#endif
#ifndef GL_TRIANGLES
#define GL_TRIANGLES 0x0004
#endif
#ifndef GL_BLEND
#define GL_BLEND 0x0BE2
#endif
#ifndef GL_SRC_ALPHA
#define GL_SRC_ALPHA 0x0302
#endif
#ifndef GL_ONE_MINUS_SRC_ALPHA
#define GL_ONE_MINUS_SRC_ALPHA 0x0303
#endif
#ifndef GL_FLOAT
#define GL_FLOAT 0x1406
#endif
#ifndef GL_UNSIGNED_INT
#define GL_UNSIGNED_INT 0x1405
#endif
AdvancedOutputWindow::AdvancedOutputWindow(const juce::String& name, MediaEngine& engine)
    : DocumentWindow(name, juce::Colours::black, DocumentWindow::closeButton)
{
    setUsingNativeTitleBar(true);
    setResizable(false, false);

    editorComp = std::make_unique<EditorComponent>(engine);
    setContentNonOwned(editorComp.get(), true);

    // Standard large interface size
    setBounds(100, 100, 1024, 600);
}

AdvancedOutputWindow::~AdvancedOutputWindow()
{
    clearContentComponent();
    editorComp.reset();
}

// ==============================================================================
// ListBox Models Implementation
// ==============================================================================

// Removed old list models

// ==============================================================================
// Canvas Area Implementation
// ==============================================================================

void AdvancedOutputWindow::EditorComponent::CanvasArea::paint(juce::Graphics& g)
{
    g.fillAll(juce::Colour(0xff09090b)); // dark workspace

    // Determine the 16:9 composition display area inside the viewport
    auto compositionBounds = getLocalBounds().toFloat().reduced(40);
    float compAspect = 16.0f / 9.0f;
    float areaAspect = compositionBounds.getWidth() / compositionBounds.getHeight();
    juce::Rectangle<float> renderArea = compositionBounds;
    if (areaAspect > compAspect)
    {
        float newWidth = compositionBounds.getHeight() * compAspect;
        renderArea.setX(compositionBounds.getX() + (compositionBounds.getWidth() - newWidth) * 0.5f);
        renderArea.setWidth(newWidth);
    }
    else
    {
        float newHeight = compositionBounds.getWidth() / compAspect;
        renderArea.setY(compositionBounds.getY() + (compositionBounds.getHeight() - newHeight) * 0.5f);
        renderArea.setHeight(newHeight);
    }

    // 1. Draw live composition in the background
    {
        g.setColour(juce::Colour(0xff18181b));
        g.fillRect(renderArea);

        // Draw cached composition frame dimmed
        const auto& compFrame = editor.mediaEngine.getCompositionFrame();
        if (!compFrame.isNull())
        {
            g.setOpacity(0.4f);
            g.drawImage(compFrame, renderArea);
            g.setOpacity(1.0f);
        }
    }

    // 2. Draw composition border
    g.setColour(juce::Colour(0xff3f3f46));
    g.drawRect(renderArea, 1.5f);

    // 3. Draw grid lines over the background (coarse grid for performance)
    g.setColour(juce::Colour(0xff27272a).withAlpha(0.3f));
    int grid = 80;
    for (int x = 0; x < getWidth(); x += grid)
        g.drawVerticalLine(x, 0.0f, (float)getHeight());
    for (int y = 0; y < getHeight(); y += grid)
        g.drawHorizontalLine(y, 0.0f, (float)getWidth());

    // 4. Render slices
    if (editor.selectedScreenIdx >= 0 && editor.selectedScreenIdx < (int)editor.screens.size())
    {
        auto& screen = editor.screens[editor.selectedScreenIdx];
        float scaleX = renderArea.getWidth() / 1920.0f;
        float scaleY = renderArea.getHeight() / 1080.0f;

        if (!screen.isEnabled)
            return;

        for (int i = 0; i < (int)screen.slices.size(); ++i)
        {
            auto& slice = screen.slices[i];
            if (!slice.isEnabled)
                continue;

            bool isSelectedSlice = (i == editor.selectedSliceIdx);

            if (editor.isInputSelectionMode)
            {
                // Input Selection Mode: Draw simple selection rectangle mapped to renderArea
                juce::Rectangle<float> screenRect (
                    renderArea.getX() + slice.inputRect.getX() * scaleX,
                    renderArea.getY() + slice.inputRect.getY() * scaleY,
                    slice.inputRect.getWidth() * scaleX,
                    slice.inputRect.getHeight() * scaleY
                );

                g.setColour(isSelectedSlice ? juce::Colour(0xff00f0a8).withAlpha(0.15f) : juce::Colour(0x11ffffff));
                g.fillRect(screenRect);

                g.setColour(isSelectedSlice ? juce::Colour(0xff00f0a8) : juce::Colour(0x55ffffff));
                g.drawRect(screenRect, 1.5f);

                g.setColour(juce::Colours::white);
                g.setFont(10.0f);
                g.drawText(slice.name, screenRect, juce::Justification::centred);

                // Draw handles for selected slice
                if (isSelectedSlice)
                {
                    g.setColour(juce::Colour(0xff10ffd0)); // mint green handles
                    g.fillEllipse(screenRect.getX() - 5.0f, screenRect.getY() - 5.0f, 10.0f, 10.0f);
                    g.fillEllipse(screenRect.getRight() - 5.0f, screenRect.getY() - 5.0f, 10.0f, 10.0f);
                    g.fillEllipse(screenRect.getRight() - 5.0f, screenRect.getBottom() - 5.0f, 10.0f, 10.0f);
                    g.fillEllipse(screenRect.getX() - 5.0f, screenRect.getBottom() - 5.0f, 10.0f, 10.0f);
                }
            }
            else
            {
                // Output Transformation Mode: Draw subdivided grid
                if (slice.gridPoints.empty())
                    slice.initGrid(); // Safety check

                int cols = slice.subdivX + 1;
                int rows = slice.subdivY + 1;

                g.setColour(isSelectedSlice ? juce::Colour(0xff00f0a8).withAlpha(0.15f) : juce::Colour(0x22ffffff));
                
                // Draw filled quad (just the outer bounds)
                juce::Path p;
                p.startNewSubPath(slice.corners[0]);
                p.lineTo(slice.corners[1]);
                p.lineTo(slice.corners[2]);
                p.lineTo(slice.corners[3]);
                p.closeSubPath();
                g.fillPath(p);

                // Draw grid lines
                g.setColour(isSelectedSlice ? juce::Colour(0xff00f0a8) : juce::Colour(0x88ffffff));
                for (int y = 0; y < rows; ++y)
                {
                    for (int x = 0; x < cols - 1; ++x)
                        g.drawLine(juce::Line<float>(slice.gridPoints[y * cols + x], slice.gridPoints[y * cols + x + 1]), 1.5f);
                }
                for (int x = 0; x < cols; ++x)
                {
                    for (int y = 0; y < rows - 1; ++y)
                        g.drawLine(juce::Line<float>(slice.gridPoints[y * cols + x], slice.gridPoints[(y + 1) * cols + x]), 1.5f);
                }

                float cx = (slice.corners[0].x + slice.corners[1].x + slice.corners[2].x + slice.corners[3].x) * 0.25f;
                float cy = (slice.corners[0].y + slice.corners[1].y + slice.corners[2].y + slice.corners[3].y) * 0.25f;
                g.setColour(juce::Colours::white);
                g.setFont(10.0f);
                g.drawText(slice.name, (int)cx - 50, (int)cy - 8, 100, 16, juce::Justification::centred);

                if (isSelectedSlice)
                {
                    g.setColour(juce::Colour(0xff10ffd0));
                    for (const auto& pt : slice.gridPoints)
                    {
                        g.fillEllipse(pt.x - 4.0f, pt.y - 4.0f, 8.0f, 8.0f);
                        g.setColour(juce::Colours::white);
                        g.drawEllipse(pt.x - 4.0f, pt.y - 4.0f, 8.0f, 8.0f, 1.0f);
                        g.setColour(juce::Colour(0xff10ffd0));
                    }
                }
            }
        }
    }
    else
    {
        g.setColour(juce::Colours::white.withAlpha(0.3f));
        g.setFont(14.0f);
        g.drawText("Add or select a Display to begin mapping", getLocalBounds(), juce::Justification::centred);
    }
}

void AdvancedOutputWindow::EditorComponent::CanvasArea::mouseDown(const juce::MouseEvent& e)
{
    if (editor.selectedScreenIdx >= 0 && editor.selectedScreenIdx < (int)editor.screens.size() &&
        editor.selectedSliceIdx >= 0 && editor.selectedSliceIdx < (int)editor.screens[editor.selectedScreenIdx].slices.size())
    {
        auto& slice = editor.screens[editor.selectedScreenIdx].slices[editor.selectedSliceIdx];
        juce::Point<float> mousePos = e.position.toFloat();

        // Calculate composition layout bounds to map coordinates
        auto compositionBounds = getLocalBounds().toFloat().reduced(40);
        float compAspect = 16.0f / 9.0f;
        float areaAspect = compositionBounds.getWidth() / compositionBounds.getHeight();
        juce::Rectangle<float> renderArea = compositionBounds;
        if (areaAspect > compAspect)
        {
            float newWidth = compositionBounds.getHeight() * compAspect;
            renderArea.setX(compositionBounds.getX() + (compositionBounds.getWidth() - newWidth) * 0.5f);
            renderArea.setWidth(newWidth);
        }
        else
        {
            float newHeight = compositionBounds.getWidth() / compAspect;
            renderArea.setY(compositionBounds.getY() + (compositionBounds.getHeight() - newHeight) * 0.5f);
            renderArea.setHeight(newHeight);
        }

        float scaleX = renderArea.getWidth() / 1920.0f;
        float scaleY = renderArea.getHeight() / 1080.0f;

        if (editor.isInputSelectionMode)
        {
            // Input Selection Mode: Drag coordinates of inputRect (X, Y, W, H)
            juce::Rectangle<float> screenRect (
                renderArea.getX() + slice.inputRect.getX() * scaleX,
                renderArea.getY() + slice.inputRect.getY() * scaleY,
                slice.inputRect.getWidth() * scaleX,
                slice.inputRect.getHeight() * scaleY
            );

            juce::Point<float> corners[4] = {
                { screenRect.getX(), screenRect.getY() },
                { screenRect.getRight(), screenRect.getY() },
                { screenRect.getRight(), screenRect.getBottom() },
                { screenRect.getX(), screenRect.getBottom() }
            };

            // Check if clicked close to any corner handle (within 12 pixels radius)
            for (int c = 0; c < 4; ++c)
            {
                if (mousePos.getDistanceFrom(corners[c]) < 12.0f)
                {
                    editor.draggedCornerIdx = c;
                    editor.rectDragStart = slice.inputRect;
                    editor.dragStartOffset = mousePos;
                    return;
                }
            }

            // If not clicked on a corner, check if clicked inside the rectangle to move it
            if (screenRect.contains(mousePos))
            {
                editor.draggedCornerIdx = 4; // Move mode
                editor.rectDragStart = slice.inputRect;
                editor.dragStartOffset = mousePos;
            }
        }
        else
        {
            // Output Transformation Mode: Drag grid points
            if (slice.gridPoints.empty())
                slice.initGrid();

            bool clickedPoint = false;
            for (size_t c = 0; c < slice.gridPoints.size(); ++c)
            {
                if (mousePos.getDistanceFrom(slice.gridPoints[c]) < 12.0f)
                {
                    editor.draggedCornerIdx = (int)c;
                    editor.dragStartOffset = slice.gridPoints[c] - mousePos;
                    clickedPoint = true;
                    break;
                }
            }

            if (!clickedPoint)
            {
                juce::Path p;
                p.startNewSubPath(slice.corners[0]);
                p.lineTo(slice.corners[1]);
                p.lineTo(slice.corners[2]);
                p.lineTo(slice.corners[3]);
                p.closeSubPath();
                if (p.contains(mousePos))
                {
                    editor.draggedCornerIdx = -2; // Move mode for whole grid
                    editor.dragStartOffset = mousePos;
                    editor.gridDragStart = slice.gridPoints;
                }
            }
        }
    }
}

void AdvancedOutputWindow::EditorComponent::CanvasArea::mouseDrag(const juce::MouseEvent& e)
{
    if (editor.draggedCornerIdx >= 0 && editor.selectedScreenIdx >= 0 &&
        editor.selectedSliceIdx >= 0)
    {
        auto& slice = editor.screens[editor.selectedScreenIdx].slices[editor.selectedSliceIdx];
        juce::Point<float> mousePos = e.position.toFloat();

        // Calculate composition layout bounds to map coordinates
        auto compositionBounds = getLocalBounds().toFloat().reduced(40);
        float compAspect = 16.0f / 9.0f;
        float areaAspect = compositionBounds.getWidth() / compositionBounds.getHeight();
        juce::Rectangle<float> renderArea = compositionBounds;
        if (areaAspect > compAspect)
        {
            float newWidth = compositionBounds.getHeight() * compAspect;
            renderArea.setX(compositionBounds.getX() + (compositionBounds.getWidth() - newWidth) * 0.5f);
            renderArea.setWidth(newWidth);
        }
        else
        {
            float newHeight = compositionBounds.getWidth() / compAspect;
            renderArea.setY(compositionBounds.getY() + (compositionBounds.getHeight() - newHeight) * 0.5f);
            renderArea.setHeight(newHeight);
        }

        float scaleX = renderArea.getWidth() / 1920.0f;
        float scaleY = renderArea.getHeight() / 1080.0f;

        if (editor.isInputSelectionMode)
        {
            float deltaX = (mousePos.x - editor.dragStartOffset.x) / scaleX;
            float deltaY = (mousePos.y - editor.dragStartOffset.y) / scaleY;

            if (editor.draggedCornerIdx == 4)
            {
                // Move entire rectangle
                float newX = editor.rectDragStart.getX() + deltaX;
                float newY = editor.rectDragStart.getY() + deltaY;

                newX = juce::jlimit(0.0f, 1920.0f - editor.rectDragStart.getWidth(), newX);
                newY = juce::jlimit(0.0f, 1080.0f - editor.rectDragStart.getHeight(), newY);
                slice.inputRect.setPosition(newX, newY);
            }
            else
            {
                // Drag individual corner to resize rectangle
                float fixedX = 0, fixedY = 0;
                if (editor.draggedCornerIdx == 0) { fixedX = editor.rectDragStart.getRight(); fixedY = editor.rectDragStart.getBottom(); }
                else if (editor.draggedCornerIdx == 1) { fixedX = editor.rectDragStart.getX(); fixedY = editor.rectDragStart.getBottom(); }
                else if (editor.draggedCornerIdx == 2) { fixedX = editor.rectDragStart.getX(); fixedY = editor.rectDragStart.getY(); }
                else if (editor.draggedCornerIdx == 3) { fixedX = editor.rectDragStart.getRight(); fixedY = editor.rectDragStart.getY(); }

                float curX = (mousePos.x - renderArea.getX()) / scaleX;
                float curY = (mousePos.y - renderArea.getY()) / scaleY;

                curX = juce::jlimit(0.0f, 1920.0f, curX);
                curY = juce::jlimit(0.0f, 1080.0f, curY);

                slice.inputRect = juce::Rectangle<float>::leftTopRightBottom(
                    std::min(fixedX, curX), std::min(fixedY, curY),
                    std::max(fixedX, curX), std::max(fixedY, curY)
                );
            }

            // Update text editors on the right inspector
            editor.inputCoordsEditors[0].setText(juce::String((int)slice.inputRect.getX()), false);
            editor.inputCoordsEditors[1].setText(juce::String((int)slice.inputRect.getY()), false);
            editor.inputCoordsEditors[2].setText(juce::String((int)slice.inputRect.getWidth()), false);
            editor.inputCoordsEditors[3].setText(juce::String((int)slice.inputRect.getHeight()), false);
        }
        else
        {
            // Output Transformation Mode: Warp grid points
            if (editor.draggedCornerIdx == -2)
            {
                float deltaX = mousePos.x - editor.dragStartOffset.x;
                float deltaY = mousePos.y - editor.dragStartOffset.y;
                
                for (size_t c = 0; c < slice.gridPoints.size(); ++c)
                {
                    slice.gridPoints[c].x = editor.gridDragStart[c].x + deltaX;
                    slice.gridPoints[c].y = editor.gridDragStart[c].y + deltaY;
                    slice.gridPoints[c].x = juce::jlimit(0.0f, (float)getWidth(), slice.gridPoints[c].x);
                    slice.gridPoints[c].y = juce::jlimit(0.0f, (float)getHeight(), slice.gridPoints[c].y);
                }

                // Sync the 4 outer corners back for backwards compatibility/inspector display
                if (slice.gridPoints.size() >= 4)
                {
                    int cols = slice.subdivX + 1;
                    int rows = slice.subdivY + 1;
                    slice.corners[0] = slice.gridPoints[0]; // TL
                    slice.corners[1] = slice.gridPoints[cols - 1]; // TR
                    slice.corners[2] = slice.gridPoints[rows * cols - 1]; // BR
                    slice.corners[3] = slice.gridPoints[(rows - 1) * cols]; // BL
                    for (int i = 0; i < 4; ++i)
                    {
                        editor.cornerXEditors[i].setText(juce::String((int)slice.corners[i].x), false);
                        editor.cornerYEditors[i].setText(juce::String((int)slice.corners[i].y), false);
                    }
                }
            }
            else if (editor.draggedCornerIdx >= 0 && editor.draggedCornerIdx < (int)slice.gridPoints.size())
            {
                slice.gridPoints[editor.draggedCornerIdx] = mousePos + editor.dragStartOffset;
                
                // Clamp to canvas borders
                slice.gridPoints[editor.draggedCornerIdx].x = juce::jlimit(0.0f, (float)getWidth(), slice.gridPoints[editor.draggedCornerIdx].x);
                slice.gridPoints[editor.draggedCornerIdx].y = juce::jlimit(0.0f, (float)getHeight(), slice.gridPoints[editor.draggedCornerIdx].y);

                // If a corner was moved, sync it back
                int cols = slice.subdivX + 1;
                int rows = slice.subdivY + 1;
                int cornerIdx = -1;
                if (editor.draggedCornerIdx == 0) cornerIdx = 0;
                else if (editor.draggedCornerIdx == cols - 1) cornerIdx = 1;
                else if (editor.draggedCornerIdx == rows * cols - 1) cornerIdx = 2;
                else if (editor.draggedCornerIdx == (rows - 1) * cols) cornerIdx = 3;

                if (cornerIdx >= 0)
                {
                    slice.corners[cornerIdx] = slice.gridPoints[editor.draggedCornerIdx];
                    editor.cornerXEditors[cornerIdx].setText(juce::String((int)slice.corners[cornerIdx].x), false);
                    editor.cornerYEditors[cornerIdx].setText(juce::String((int)slice.corners[cornerIdx].y), false);
                }
            }
        }

        repaint();
    }
}

void AdvancedOutputWindow::EditorComponent::CanvasArea::mouseUp(const juce::MouseEvent&)
{
    editor.draggedCornerIdx = -1;
}

class MappingTreeItem : public juce::TreeViewItem
{
public:
    MappingTreeItem(AdvancedOutputWindow::EditorComponent& ed, int screenId, int sliceId = -1)
        : editor(ed), screenIdx(screenId), sliceIdx(sliceId)
    {
    }

    juce::Component::SafePointer<juce::Component> myComponent;

    bool mightContainSubItems() override { return sliceIdx == -1; }
    
    juce::String getUniqueName() const override 
    { 
        return "Item_" + juce::String(screenIdx) + "_" + juce::String(sliceIdx); 
    }
    
    void itemSelectionChanged(bool isNowSelected) override
    {
        if (myComponent != nullptr)
            myComponent->repaint();
            
        if (isNowSelected)
        {
            if (sliceIdx == -1)
                editor.selectScreen(screenIdx);
            else
            {
                editor.selectScreen(screenIdx);
                editor.selectSlice(sliceIdx);
            }
        }
    }
    
    void itemOpennessChanged(bool isNowOpen) override
    {
        if (isNowOpen && sliceIdx == -1)
        {
            clearSubItems();
            if (screenIdx >= 0 && screenIdx < (int)editor.screens.size())
            {
                for (int i = 0; i < (int)editor.screens[screenIdx].slices.size(); ++i)
                {
                    addSubItem(new MappingTreeItem(editor, screenIdx, i));
                }
            }
        }
    }
    
    // Custom component for the row
    class RowComponent : public juce::Component
    {
    public:
        RowComponent(MappingTreeItem& itemNode, AdvancedOutputWindow::EditorComponent& ed, int scIdx, int slIdx)
            : item(itemNode), editor(ed), screenIdx(scIdx), sliceIdx(slIdx)
        {
            setInterceptsMouseClicks(true, true);
            addAndMakeVisible(deleteBtn);
            deleteBtn.onClick = [this] {
                // Delete logic
                if (sliceIdx == -1)
                {
                    editor.screens.erase(editor.screens.begin() + screenIdx);
                    editor.selectScreen(-1);
                    editor.syncDisplayWindows();
                }
                else
                {
                    editor.screens[screenIdx].slices.erase(editor.screens[screenIdx].slices.begin() + sliceIdx);
                    editor.selectSlice(-1);
                }
                editor.rebuildTreeView();
            };

            addAndMakeVisible(eyeBtn);
            eyeBtn.setClickingTogglesState(true);
            
            bool isEnabled = true;
            if (sliceIdx == -1 && screenIdx < (int)editor.screens.size())
                isEnabled = editor.screens[screenIdx].isEnabled;
            else if (sliceIdx >= 0 && screenIdx < (int)editor.screens.size() && sliceIdx < (int)editor.screens[screenIdx].slices.size())
                isEnabled = editor.screens[screenIdx].slices[sliceIdx].isEnabled;
                
            eyeBtn.setToggleState(isEnabled, juce::dontSendNotification);
            
            eyeBtn.onClick = [this] {
                bool enabled = eyeBtn.getToggleState();
                if (sliceIdx == -1 && screenIdx < (int)editor.screens.size())
                    editor.screens[screenIdx].isEnabled = enabled;
                else if (sliceIdx >= 0 && screenIdx < (int)editor.screens.size() && sliceIdx < (int)editor.screens[screenIdx].slices.size())
                    editor.screens[screenIdx].slices[sliceIdx].isEnabled = enabled;
                
                // Repaint canvas to reflect visibility
                editor.canvasArea->repaint();
            };
        }

        void paint(juce::Graphics& g) override
        {
            bool isActive = (editor.selectedScreenIdx == screenIdx && editor.selectedSliceIdx == sliceIdx);
            
            if (isActive)
            {
                g.fillAll(juce::Colour(0xff00f0a8).withAlpha(0.4f)); // Bright semi-transparent neon green fill
                g.setColour(juce::Colour(0xff00f0a8)); // Solid neon green border
                g.drawRect(getLocalBounds(), 2); 
            }
            
            g.setColour(sliceIdx == -1 ? juce::Colour(0xff00f0a8) : juce::Colours::white);
            g.setFont(sliceIdx == -1 ? juce::Font(12.0f, juce::Font::bold) : juce::Font(12.0f));
            
            juce::String name = "Item";
            if (sliceIdx == -1 && screenIdx < (int)editor.screens.size())
                name = editor.screens[screenIdx].name;
            else if (sliceIdx >= 0 && screenIdx < (int)editor.screens.size() && sliceIdx < (int)editor.screens[screenIdx].slices.size())
                name = editor.screens[screenIdx].slices[sliceIdx].name;
                
            g.drawText(name, 4, 0, getWidth() - 50, getHeight(), juce::Justification::centredLeft);
        }

        void mouseDown(const juce::MouseEvent& e) override
        {
            item.setSelected(true, true);
        }

        void mouseDoubleClick(const juce::MouseEvent& e) override
        {
            item.setOpen(!item.isOpen());
        }

        void resized() override
        {
            auto bounds = getLocalBounds();
            deleteBtn.setBounds(bounds.removeFromRight(20).reduced(2));
            eyeBtn.setBounds(bounds.removeFromRight(20).reduced(2));
        }

    private:
        class IconButton : public juce::Button
        {
        public:
            IconButton(const juce::String& name, bool isEye) : juce::Button(name), eyeMode(isEye) {}
            
            void paintButton(juce::Graphics& g, bool shouldDrawButtonAsHighlighted, bool /*shouldDrawButtonAsDown*/) override
            {
                juce::Path p;
                auto bounds = getLocalBounds().toFloat().reduced(4.0f);
                
                if (eyeMode)
                {
                    bool active = getToggleState();
                    g.setColour(active ? juce::Colours::white : juce::Colour(0xff555555));
                    
                    p.startNewSubPath(bounds.getX(), bounds.getCentreY());
                    p.quadraticTo(bounds.getCentreX(), bounds.getY() - 3.0f, bounds.getRight(), bounds.getCentreY());
                    p.quadraticTo(bounds.getCentreX(), bounds.getBottom() + 3.0f, bounds.getX(), bounds.getCentreY());
                    g.strokePath(p, juce::PathStrokeType(1.5f));
                    
                    g.drawEllipse(bounds.getCentreX() - 2.0f, bounds.getCentreY() - 2.0f, 4.0f, 4.0f, 1.5f);
                    
                    if (!active)
                    {
                        g.drawLine(bounds.getX(), bounds.getBottom(), bounds.getRight(), bounds.getY(), 1.5f);
                    }
                }
                else
                {
                    g.setColour(shouldDrawButtonAsHighlighted ? juce::Colours::red : juce::Colour(0xffff0055));
                    g.drawLine(bounds.getX(), bounds.getY(), bounds.getRight(), bounds.getBottom(), 2.0f);
                    g.drawLine(bounds.getX(), bounds.getBottom(), bounds.getRight(), bounds.getY(), 2.0f);
                }
            }
        private:
            bool eyeMode;
        };

        MappingTreeItem& item;
        AdvancedOutputWindow::EditorComponent& editor;
        int screenIdx;
        int sliceIdx;
        IconButton deleteBtn { "Delete", false };
        IconButton eyeBtn { "Visibility", true };
    };

    std::unique_ptr<juce::Component> createItemComponent() override
    {
        auto* c = new RowComponent(*this, editor, screenIdx, sliceIdx);
        myComponent = c;
        return std::unique_ptr<juce::Component>(c);
    }
    
    int getItemHeight() const override { return 24; }

    AdvancedOutputWindow::EditorComponent& editor;
    int screenIdx;
    int sliceIdx;
};

AdvancedOutputWindow::EditorComponent::~EditorComponent()
{
    mappingTreeView.setRootItem(nullptr);
}

AdvancedOutputWindow::EditorComponent::EditorComponent(MediaEngine& engine)
    : mediaEngine(engine)
{
    addAndMakeVisible(mappingTreeView);
    mappingTreeView.setColour(juce::TreeView::backgroundColourId, juce::Colour(0xff18181b));
    mappingTreeView.setDefaultOpenness(true);

    // Left Sidebar title
    addAndMakeVisible(leftSidebarTitle);
    leftSidebarTitle.setText("Mapping Layout", juce::dontSendNotification);
    leftSidebarTitle.setFont(juce::Font(12.0f, juce::Font::bold));
    leftSidebarTitle.setColour(juce::Label::textColourId, juce::Colour(0xff10ffd0));

    // Right Sidebar Title
    addAndMakeVisible(rightSidebarTitle);
    rightSidebarTitle.setText("Display Properties", juce::dontSendNotification);
    rightSidebarTitle.setFont(juce::Font(12.0f, juce::Font::bold));
    rightSidebarTitle.setColour(juce::Label::textColourId, juce::Colour(0xff00f0a8));

    // Left sidebar buttons
    addAndMakeVisible(addScreenBtn);
    addScreenBtn.setButtonText("+ Display");
    addScreenBtn.setColour(juce::TextButton::buttonColourId, juce::Colour(0xff27272a));
    addScreenBtn.onClick = [this] { addScreen(); };

    addAndMakeVisible(addSliceBtn);
    addSliceBtn.setButtonText("+ Quad Slice");
    addSliceBtn.setColour(juce::TextButton::buttonColourId, juce::Colour(0xff27272a));
    addSliceBtn.onClick = [this] { addSlice(); };

    // Middle Tabs setup
    addAndMakeVisible(inputTabBtn);
    inputTabBtn.setButtonText("Input Selection");
    inputTabBtn.setClickingTogglesState(true);
    inputTabBtn.setToggleState(isInputSelectionMode, juce::dontSendNotification);
    inputTabBtn.setColour(juce::TextButton::buttonOnColourId, juce::Colour(0xff00f0a8));
    inputTabBtn.setColour(juce::TextButton::buttonColourId, juce::Colour(0xff27272a));
    inputTabBtn.setColour(juce::TextButton::textColourOnId, juce::Colours::black);
    inputTabBtn.onClick = [this] {
        isInputSelectionMode = true;
        inputTabBtn.setToggleState(true, juce::dontSendNotification);
        outputTabBtn.setToggleState(false, juce::dontSendNotification);
        updateInspectorVisibility();
        canvasArea->repaint();
    };

    addAndMakeVisible(outputTabBtn);
    outputTabBtn.setButtonText("Output Transformation");
    outputTabBtn.setClickingTogglesState(true);
    outputTabBtn.setToggleState(!isInputSelectionMode, juce::dontSendNotification);
    outputTabBtn.setColour(juce::TextButton::buttonOnColourId, juce::Colour(0xff00f0a8));
    outputTabBtn.setColour(juce::TextButton::buttonColourId, juce::Colour(0xff27272a));
    outputTabBtn.setColour(juce::TextButton::textColourOnId, juce::Colours::black);
    outputTabBtn.onClick = [this] {
        isInputSelectionMode = false;
        inputTabBtn.setToggleState(false, juce::dontSendNotification);
        outputTabBtn.setToggleState(true, juce::dontSendNotification);
        updateInspectorVisibility();
        canvasArea->repaint();
    };

    // Canvas Setup
    canvasArea = std::make_unique<CanvasArea>(*this);
    addAndMakeVisible(*canvasArea);

    // Right Sidebar details
    addAndMakeVisible(nameLabel);
    nameLabel.setText("Name", juce::dontSendNotification);
    nameLabel.setFont(10.0f);
    nameLabel.setColour(juce::Label::textColourId, juce::Colour(0xffa1a1aa));

    addAndMakeVisible(nameEditor);
    nameEditor.setColour(juce::TextEditor::backgroundColourId, juce::Colour(0xff27272a));
    nameEditor.setColour(juce::TextEditor::textColourId, juce::Colours::white);
    nameEditor.onTextChange = [this] {
        if (selectedScreenIdx >= 0)
        {
            if (selectedSliceIdx >= 0)
            {
                screens[selectedScreenIdx].slices[selectedSliceIdx].name = nameEditor.getText();
                rebuildTreeView();
            }
            else
            {
                screens[selectedScreenIdx].name = nameEditor.getText();
                rebuildTreeView();
                syncDisplayWindows();
            }
            canvasArea->repaint();
        }
    };

    // === 1. Display Inspector Setup ===
    addAndMakeVisible(deviceLabel);
    deviceLabel.setText("Device", juce::dontSendNotification);
    deviceLabel.setFont(10.0f);
    deviceLabel.setColour(juce::Label::textColourId, juce::Colour(0xffa1a1aa));

    addAndMakeVisible(deviceSelector);
    deviceSelector.addItem("Windowed", 1);
    deviceSelector.addItem("NDI", 2);
    deviceSelector.addItem("Display 1", 3);
    deviceSelector.addItem("Display 2", 4);
    deviceSelector.setSelectedId(1, juce::dontSendNotification);
    deviceSelector.onChange = [this] {
        if (selectedScreenIdx >= 0 && selectedSliceIdx == -1)
        {
            auto& screen = screens[selectedScreenIdx];
            screen.device = deviceSelector.getText();

            // Auto-detect resolution from selected display (DPI-aware)
            if (screen.device.startsWith("Display"))
            {
                int dispIdx = screen.device.getLastCharacters(1).getIntValue() - 1;
                auto& displays = juce::Desktop::getInstance().getDisplays();
                if (dispIdx >= 0 && dispIdx < displays.displays.size())
                {
                    const auto& disp = displays.displays[dispIdx];
                    // totalArea is in logical coords; multiply by scale for physical pixels
                    screen.width = (int)(disp.totalArea.getWidth() * disp.scale);
                    screen.height = (int)(disp.totalArea.getHeight() * disp.scale);
                    widthEditor.setText(juce::String(screen.width), false);
                    heightEditor.setText(juce::String(screen.height), false);
                }
            }

            syncDisplayWindows();
        }
    };

    addAndMakeVisible(resolutionLabel);
    resolutionLabel.setText("Resolution (W / H)", juce::dontSendNotification);
    resolutionLabel.setFont(10.0f);
    resolutionLabel.setColour(juce::Label::textColourId, juce::Colour(0xffa1a1aa));

    addAndMakeVisible(widthEditor);
    widthEditor.setColour(juce::TextEditor::backgroundColourId, juce::Colour(0xff27272a));
    widthEditor.onTextChange = [this] {
        if (selectedScreenIdx >= 0 && selectedSliceIdx == -1)
        {
            screens[selectedScreenIdx].width = widthEditor.getText().getIntValue();
            syncDisplayWindows();
        }
    };

    addAndMakeVisible(heightEditor);
    heightEditor.setColour(juce::TextEditor::backgroundColourId, juce::Colour(0xff27272a));
    heightEditor.onTextChange = [this] {
        if (selectedScreenIdx >= 0 && selectedSliceIdx == -1)
        {
            screens[selectedScreenIdx].height = heightEditor.getText().getIntValue();
            syncDisplayWindows();
        }
    };

    addAndMakeVisible(displayOpacityLabel);
    displayOpacityLabel.setText("Opacity", juce::dontSendNotification);
    displayOpacityLabel.setFont(10.0f);
    displayOpacityLabel.setColour(juce::Label::textColourId, juce::Colour(0xffa1a1aa));

    addAndMakeVisible(displayOpacitySlider);
    displayOpacitySlider.setRange(0.0, 1.0, 0.01);
    displayOpacitySlider.setSliderStyle(juce::Slider::LinearHorizontal);
    displayOpacitySlider.setTextBoxStyle(juce::Slider::TextBoxRight, false, 45, 16);
    displayOpacitySlider.setColour(juce::Slider::trackColourId, juce::Colour(0xff00f0a8));
    displayOpacitySlider.setColour(juce::Slider::thumbColourId, juce::Colours::white);
    displayOpacitySlider.onValueChange = [this] {
        if (selectedScreenIdx >= 0 && selectedSliceIdx == -1)
            screens[selectedScreenIdx].opacity = (float)displayOpacitySlider.getValue();
    };

    addAndMakeVisible(brightnessLabel);
    brightnessLabel.setText("Brightness", juce::dontSendNotification);
    brightnessLabel.setFont(10.0f);
    brightnessLabel.setColour(juce::Label::textColourId, juce::Colour(0xffa1a1aa));

    addAndMakeVisible(brightnessSlider);
    brightnessSlider.setRange(0.0, 1.0, 0.01);
    brightnessSlider.setSliderStyle(juce::Slider::LinearHorizontal);
    brightnessSlider.setTextBoxStyle(juce::Slider::TextBoxRight, false, 45, 16);
    brightnessSlider.setColour(juce::Slider::trackColourId, juce::Colour(0xff00f0a8));
    brightnessSlider.onValueChange = [this] {
        if (selectedScreenIdx >= 0 && selectedSliceIdx == -1)
            screens[selectedScreenIdx].brightness = (float)brightnessSlider.getValue();
    };

    addAndMakeVisible(contrastLabel);
    contrastLabel.setText("Contrast", juce::dontSendNotification);
    contrastLabel.setFont(10.0f);
    contrastLabel.setColour(juce::Label::textColourId, juce::Colour(0xffa1a1aa));

    addAndMakeVisible(contrastSlider);
    contrastSlider.setRange(0.0, 1.0, 0.01);
    contrastSlider.setSliderStyle(juce::Slider::LinearHorizontal);
    contrastSlider.setTextBoxStyle(juce::Slider::TextBoxRight, false, 45, 16);
    contrastSlider.setColour(juce::Slider::trackColourId, juce::Colour(0xff00f0a8));
    contrastSlider.onValueChange = [this] {
        if (selectedScreenIdx >= 0 && selectedSliceIdx == -1)
            screens[selectedScreenIdx].contrast = (float)contrastSlider.getValue();
    };

    addAndMakeVisible(rgbLabel);
    rgbLabel.setText("RGB Levels (R / G / B)", juce::dontSendNotification);
    rgbLabel.setFont(10.0f);
    rgbLabel.setColour(juce::Label::textColourId, juce::Colour(0xffa1a1aa));

    addAndMakeVisible(redSlider);
    redSlider.setRange(0.0, 1.0, 0.01);
    redSlider.setSliderStyle(juce::Slider::LinearHorizontal);
    redSlider.setTextBoxStyle(juce::Slider::TextBoxRight, false, 45, 16);
    redSlider.setColour(juce::Slider::trackColourId, juce::Colours::red);
    redSlider.onValueChange = [this] {
        if (selectedScreenIdx >= 0 && selectedSliceIdx == -1)
            screens[selectedScreenIdx].red = (float)redSlider.getValue();
    };

    addAndMakeVisible(greenSlider);
    greenSlider.setRange(0.0, 1.0, 0.01);
    greenSlider.setSliderStyle(juce::Slider::LinearHorizontal);
    greenSlider.setTextBoxStyle(juce::Slider::TextBoxRight, false, 45, 16);
    greenSlider.setColour(juce::Slider::trackColourId, juce::Colours::green);
    greenSlider.onValueChange = [this] {
        if (selectedScreenIdx >= 0 && selectedSliceIdx == -1)
            screens[selectedScreenIdx].green = (float)greenSlider.getValue();
    };

    addAndMakeVisible(blueSlider);
    blueSlider.setRange(0.0, 1.0, 0.01);
    blueSlider.setSliderStyle(juce::Slider::LinearHorizontal);
    blueSlider.setTextBoxStyle(juce::Slider::TextBoxRight, false, 45, 16);
    blueSlider.setColour(juce::Slider::trackColourId, juce::Colours::blue);
    blueSlider.onValueChange = [this] {
        if (selectedScreenIdx >= 0 && selectedSliceIdx == -1)
            screens[selectedScreenIdx].blue = (float)blueSlider.getValue();
    };

    // === 2. Input Slice Inspector Setup ===
    addAndMakeVisible(inputCoordsLabel);
    inputCoordsLabel.setText("Input Bounds", juce::dontSendNotification);
    inputCoordsLabel.setFont(juce::Font(11.0f, juce::Font::bold));
    inputCoordsLabel.setColour(juce::Label::textColourId, juce::Colour(0xffa1a1aa));

    juce::String inputCoordLabels[4] = { "X Offset", "Y Offset", "Width", "Height" };
    for (int i = 0; i < 4; ++i)
    {
        addAndMakeVisible(inputLabels[i]);
        inputLabels[i].setText(inputCoordLabels[i], juce::dontSendNotification);
        inputLabels[i].setFont(10.0f);
        inputLabels[i].setColour(juce::Label::textColourId, juce::Colour(0xffa1a1aa));

        addAndMakeVisible(inputCoordsEditors[i]);
        inputCoordsEditors[i].setColour(juce::TextEditor::backgroundColourId, juce::Colour(0xff27272a));
        inputCoordsEditors[i].onTextChange = [this, i] {
            if (selectedScreenIdx >= 0 && selectedSliceIdx >= 0 && draggedCornerIdx == -1)
            {
                auto& slice = screens[selectedScreenIdx].slices[selectedSliceIdx];
                float val = (float)inputCoordsEditors[i].getText().getIntValue();
                if (i == 0) slice.inputRect.setX(val);
                else if (i == 1) slice.inputRect.setY(val);
                else if (i == 2) slice.inputRect.setWidth(val);
                else if (i == 3) slice.inputRect.setHeight(val);
                canvasArea->repaint();
            }
        };
    }

    // === 3. Output Slice Inspector Setup ===
    addAndMakeVisible(opacityLabel);
    opacityLabel.setText("Opacity", juce::dontSendNotification);
    opacityLabel.setFont(10.0f);
    opacityLabel.setColour(juce::Label::textColourId, juce::Colour(0xffa1a1aa));

    addAndMakeVisible(opacitySlider);
    opacitySlider.setRange(0.0, 1.0, 0.01);
    opacitySlider.setSliderStyle(juce::Slider::LinearHorizontal);
    opacitySlider.setTextBoxStyle(juce::Slider::TextBoxRight, false, 45, 16);
    opacitySlider.setColour(juce::Slider::trackColourId, juce::Colour(0xff00f0a8));
    opacitySlider.setColour(juce::Slider::thumbColourId, juce::Colours::white);
    opacitySlider.onValueChange = [this] {
        if (selectedScreenIdx >= 0 && selectedSliceIdx >= 0)
        {
            screens[selectedScreenIdx].slices[selectedSliceIdx].opacity = (float)opacitySlider.getValue();
            canvasArea->repaint();
        }
    };

    addAndMakeVisible(cornersLabel);
    cornersLabel.setText("Corner Warp Offsets", juce::dontSendNotification);
    cornersLabel.setFont(juce::Font(11.0f, juce::Font::bold));
    cornersLabel.setColour(juce::Label::textColourId, juce::Colour(0xffa1a1aa));

    juce::String labels[4] = { "TL (X / Y)", "TR (X / Y)", "BR (X / Y)", "BL (X / Y)" };
    for (int i = 0; i < 4; ++i)
    {
        addAndMakeVisible(cornerLabels[i]);
        cornerLabels[i].setText(labels[i], juce::dontSendNotification);
        cornerLabels[i].setFont(10.0f);
        cornerLabels[i].setColour(juce::Label::textColourId, juce::Colour(0xffa1a1aa));

        addAndMakeVisible(cornerXEditors[i]);
        cornerXEditors[i].setColour(juce::TextEditor::backgroundColourId, juce::Colour(0xff27272a));
        cornerXEditors[i].onTextChange = [this, i] {
            if (selectedScreenIdx >= 0 && selectedSliceIdx >= 0 && draggedCornerIdx == -1)
            {
                screens[selectedScreenIdx].slices[selectedSliceIdx].corners[i].x = (float)cornerXEditors[i].getText().getIntValue();
                canvasArea->repaint();
            }
        };

        addAndMakeVisible(cornerYEditors[i]);
        cornerYEditors[i].setColour(juce::TextEditor::backgroundColourId, juce::Colour(0xff27272a));
        cornerYEditors[i].onTextChange = [this, i] {
            if (selectedScreenIdx >= 0 && selectedSliceIdx >= 0 && draggedCornerIdx == -1)
            {
                auto& slice = screens[selectedScreenIdx].slices[selectedSliceIdx];
                slice.corners[i].y = (float)cornerYEditors[i].getText().getIntValue();
                slice.initGrid();
                canvasArea->repaint();
            }
        };
    }

    addAndMakeVisible(subdivLabel);
    subdivLabel.setText("Subdivisions", juce::dontSendNotification);
    subdivLabel.setFont(juce::Font(11.0f, juce::Font::bold));
    subdivLabel.setColour(juce::Label::textColourId, juce::Colour(0xffa1a1aa));

    addAndMakeVisible(subdivXLabel);
    subdivXLabel.setText("X:", juce::dontSendNotification);
    subdivXLabel.setFont(10.0f);
    subdivXLabel.setColour(juce::Label::textColourId, juce::Colour(0xffa1a1aa));

    addAndMakeVisible(subdivYLabel);
    subdivYLabel.setText("Y:", juce::dontSendNotification);
    subdivYLabel.setFont(10.0f);
    subdivYLabel.setColour(juce::Label::textColourId, juce::Colour(0xffa1a1aa));

    addAndMakeVisible(subdivXEditor);
    subdivXEditor.setColour(juce::TextEditor::backgroundColourId, juce::Colour(0xff27272a));
    subdivXEditor.onTextChange = [this] {
        if (selectedScreenIdx >= 0 && selectedSliceIdx >= 0)
        {
            auto& slice = screens[selectedScreenIdx].slices[selectedSliceIdx];
            int val = juce::jlimit(1, 32, subdivXEditor.getText().getIntValue());
            if (slice.subdivX != val)
            {
                slice.subdivX = val;
                slice.initGrid();
                canvasArea->repaint();
            }
        }
    };

    addAndMakeVisible(subdivYEditor);
    subdivYEditor.setColour(juce::TextEditor::backgroundColourId, juce::Colour(0xff27272a));
    subdivYEditor.onTextChange = [this] {
        if (selectedScreenIdx >= 0 && selectedSliceIdx >= 0)
        {
            auto& slice = screens[selectedScreenIdx].slices[selectedSliceIdx];
            int val = juce::jlimit(1, 32, subdivYEditor.getText().getIntValue());
            if (slice.subdivY != val)
            {
                slice.subdivY = val;
                slice.initGrid();
                canvasArea->repaint();
            }
        }
    };

    addAndMakeVisible(resetGridBtn);
    resetGridBtn.setButtonText("Reset Grid");
    resetGridBtn.setColour(juce::TextButton::buttonColourId, juce::Colour(0xff27272a));
    resetGridBtn.onClick = [this] {
        if (selectedScreenIdx >= 0 && selectedSliceIdx >= 0)
        {
            screens[selectedScreenIdx].slices[selectedSliceIdx].initGrid();
            canvasArea->repaint();
        }
    };

    // 4. Edge Blending Controls
    addAndMakeVisible(edgeBlendLabel);
    edgeBlendLabel.setText("Edge Blending", juce::dontSendNotification);
    edgeBlendLabel.setFont(juce::Font(11.0f, juce::Font::bold));
    edgeBlendLabel.setColour(juce::Label::textColourId, juce::Colour(0xffa1a1aa));

    auto setupBlendSlider = [this](juce::Slider& sl, juce::Label& lb, const juce::String& text, float maxVal) {
        addAndMakeVisible(lb);
        lb.setText(text, juce::dontSendNotification);
        lb.setFont(10.0f);
        lb.setColour(juce::Label::textColourId, juce::Colour(0xffa1a1aa));

        addAndMakeVisible(sl);
        sl.setRange(0.0, maxVal, 0.01);
        sl.setSliderStyle(juce::Slider::LinearHorizontal);
        sl.setTextBoxStyle(juce::Slider::TextBoxRight, false, 45, 16);
        sl.setColour(juce::Slider::trackColourId, juce::Colour(0xff00f0a8));
        sl.onValueChange = [this, &sl] {
            if (selectedScreenIdx >= 0 && selectedSliceIdx >= 0) {
                auto& slice = screens[selectedScreenIdx].slices[selectedSliceIdx];
                if (&sl == &blendTopSlider) slice.blendTop = (float)sl.getValue();
                else if (&sl == &blendBottomSlider) slice.blendBottom = (float)sl.getValue();
                else if (&sl == &blendLeftSlider) slice.blendLeft = (float)sl.getValue();
                else if (&sl == &blendRightSlider) slice.blendRight = (float)sl.getValue();
                else if (&sl == &blendGammaSlider) slice.blendGamma = (float)sl.getValue();
                repaintDeviceWindows();
            }
        };
    };
    setupBlendSlider(blendTopSlider, blendTopLabel, "Top", 1.0f);
    setupBlendSlider(blendBottomSlider, blendBottomLabel, "Bottom", 1.0f);
    setupBlendSlider(blendLeftSlider, blendLeftLabel, "Left", 1.0f);
    setupBlendSlider(blendRightSlider, blendRightLabel, "Right", 1.0f);
    setupBlendSlider(blendGammaSlider, blendGammaLabel, "Gamma", 3.0f);

    // Default mock data to populate immediately
    addScreen();
    addSlice();
}

void AdvancedOutputWindow::EditorComponent::paint(juce::Graphics& g)
{
    g.fillAll(juce::Colour(0xff18181b)); // dark sidebar fills

    // Draw borders separating panels
    g.setColour(juce::Colour(0xff27272a));
    g.drawVerticalLine(200, 0.0f, (float)getHeight());
    g.drawVerticalLine(getWidth() - 200, 0.0f, (float)getHeight());
}

void AdvancedOutputWindow::EditorComponent::resized()
{
    auto area = getLocalBounds();
    auto leftArea = area.removeFromLeft(200).reduced(10);
    auto rightArea = area.removeFromRight(200).reduced(10);

    // Left Sidebar Layout
    leftSidebarTitle.setBounds(leftArea.removeFromTop(20));
    leftArea.removeFromTop(4);
    
    // Add Screen Button
    addScreenBtn.setBounds(leftArea.removeFromTop(24));
    leftArea.removeFromTop(4);

    // Add Slice Button
    addSliceBtn.setBounds(leftArea.removeFromTop(24));
    leftArea.removeFromTop(8);

    // Mapping Tree View (takes remaining left space)
    mappingTreeView.setBounds(leftArea);

    // Middle Area: top tabs + canvas area below
    auto topBar = area.removeFromTop(40);
    int tabW = topBar.getWidth() / 2;
    inputTabBtn.setBounds(topBar.removeFromLeft(tabW).reduced(6, 4));
    outputTabBtn.setBounds(topBar.reduced(6, 4));

    canvasArea->setBounds(area.reduced(2));

    // Right Sidebar Inspector Layout
    rightSidebarTitle.setBounds(rightArea.removeFromTop(20));
    rightArea.removeFromTop(8);

    if (nameEditor.isVisible())
    {
        nameLabel.setBounds(rightArea.removeFromTop(14));
        nameEditor.setBounds(rightArea.removeFromTop(24));
        rightArea.removeFromTop(10);
    }

    // 1. Display properties layout positioning
    if (deviceSelector.isVisible())
    {
        deviceLabel.setBounds(rightArea.removeFromTop(14));
        deviceSelector.setBounds(rightArea.removeFromTop(24));
        rightArea.removeFromTop(10);

        resolutionLabel.setBounds(rightArea.removeFromTop(14));
        auto resRow = rightArea.removeFromTop(24);
        widthEditor.setBounds(resRow.removeFromLeft(resRow.getWidth() / 2 - 4));
        heightEditor.setBounds(resRow.withTrimmedLeft(8));
        rightArea.removeFromTop(10);

        displayOpacityLabel.setBounds(rightArea.removeFromTop(14));
        displayOpacitySlider.setBounds(rightArea.removeFromTop(24));
        rightArea.removeFromTop(10);

        brightnessLabel.setBounds(rightArea.removeFromTop(14));
        brightnessSlider.setBounds(rightArea.removeFromTop(24));
        rightArea.removeFromTop(10);

        contrastLabel.setBounds(rightArea.removeFromTop(14));
        contrastSlider.setBounds(rightArea.removeFromTop(24));
        rightArea.removeFromTop(10);

        rgbLabel.setBounds(rightArea.removeFromTop(14));
        auto rRow = rightArea.removeFromTop(20);
        redSlider.setBounds(rRow);
        auto gRow = rightArea.removeFromTop(20);
        greenSlider.setBounds(gRow);
        auto bRow = rightArea.removeFromTop(20);
        blueSlider.setBounds(bRow);
    }

    // 2. Input Slice layout positioning
    if (inputCoordsLabel.isVisible())
    {
        inputCoordsLabel.setBounds(rightArea.removeFromTop(16));
        rightArea.removeFromTop(6);

        for (int i = 0; i < 4; ++i)
        {
            inputLabels[i].setBounds(rightArea.removeFromTop(14));
            inputCoordsEditors[i].setBounds(rightArea.removeFromTop(24));
            rightArea.removeFromTop(6);
        }
    }

    // 3. Output Slice layout positioning
    if (opacitySlider.isVisible())
    {
        opacityLabel.setBounds(rightArea.removeFromTop(14));
        opacitySlider.setBounds(rightArea.removeFromTop(24));
        rightArea.removeFromTop(16);

        cornersLabel.setBounds(rightArea.removeFromTop(16));
        rightArea.removeFromTop(6);

        for (int i = 0; i < 4; ++i)
        {
            cornerLabels[i].setBounds(rightArea.removeFromTop(14));
            auto row = rightArea.removeFromTop(24);
            cornerXEditors[i].setBounds(row.removeFromLeft(row.getWidth() / 2 - 4));
            cornerYEditors[i].setBounds(row.withTrimmedLeft(8));
            rightArea.removeFromTop(4);
        }

        rightArea.removeFromTop(10);
        subdivLabel.setBounds(rightArea.removeFromTop(16));
        rightArea.removeFromTop(6);

        auto subRow = rightArea.removeFromTop(24);
        subdivXLabel.setBounds(subRow.removeFromLeft(20));
        subdivXEditor.setBounds(subRow.removeFromLeft(40));
        subRow.removeFromLeft(10); // spacing
        subdivYLabel.setBounds(subRow.removeFromLeft(20));
        subdivYEditor.setBounds(subRow.removeFromLeft(40));
        
        rightArea.removeFromTop(4);
        resetGridBtn.setBounds(rightArea.removeFromTop(24));
        rightArea.removeFromTop(10);

        edgeBlendLabel.setBounds(rightArea.removeFromTop(16));
        rightArea.removeFromTop(4);

        auto layoutBlend = [](juce::Rectangle<int>& area, juce::Label& lb, juce::Slider& sl) {
            auto row = area.removeFromTop(20);
            lb.setBounds(row.removeFromLeft(40));
            sl.setBounds(row);
        };
        layoutBlend(rightArea, blendTopLabel, blendTopSlider);
        layoutBlend(rightArea, blendBottomLabel, blendBottomSlider);
        layoutBlend(rightArea, blendLeftLabel, blendLeftSlider);
        layoutBlend(rightArea, blendRightLabel, blendRightSlider);
        layoutBlend(rightArea, blendGammaLabel, blendGammaSlider);
    }
}

void AdvancedOutputWindow::EditorComponent::updateLists()
{
    rebuildTreeView();
    canvasArea->repaint();
    repaintDeviceWindows();
}

void AdvancedOutputWindow::EditorComponent::rebuildTreeView()
{
    mappingTreeView.setRootItem(nullptr);
    
    auto* root = new MappingTreeItem(*this, -1, -1); // Dummy root
    mappingTreeView.setRootItem(root);
    mappingTreeView.setRootItemVisible(false);
    
    for (int i = 0; i < (int)screens.size(); ++i)
    {
        root->addSubItem(new MappingTreeItem(*this, i, -1));
    }
}

void AdvancedOutputWindow::EditorComponent::selectScreen(int index)
{
    selectedScreenIdx = index;
    selectedSliceIdx = -1;

    if (selectedScreenIdx >= 0 && selectedScreenIdx < (int)screens.size())
    {
        auto& screen = screens[selectedScreenIdx];
        nameEditor.setText(screen.name, false);
        deviceSelector.setText(screen.device, juce::dontSendNotification);
        widthEditor.setText(juce::String(screen.width), false);
        heightEditor.setText(juce::String(screen.height), false);
        displayOpacitySlider.setValue(screen.opacity, juce::dontSendNotification);
        brightnessSlider.setValue(screen.brightness, juce::dontSendNotification);
        contrastSlider.setValue(screen.contrast, juce::dontSendNotification);
        redSlider.setValue(screen.red, juce::dontSendNotification);
        greenSlider.setValue(screen.green, juce::dontSendNotification);
        blueSlider.setValue(screen.blue, juce::dontSendNotification);
    }
    updateInspectorVisibility();
    updateLists();
}

void AdvancedOutputWindow::EditorComponent::selectSlice(int index)
{
    selectedSliceIdx = index;
    if (selectedScreenIdx >= 0 && selectedScreenIdx < (int)screens.size() &&
        selectedSliceIdx >= 0 && selectedSliceIdx < (int)screens[selectedScreenIdx].slices.size())
    {
        auto& slice = screens[selectedScreenIdx].slices[selectedSliceIdx];
        nameEditor.setText(slice.name, false);
        opacitySlider.setValue(slice.opacity, juce::dontSendNotification);
        for (int i = 0; i < 4; ++i)
        {
            cornerXEditors[i].setText(juce::String((int)slice.corners[i].x), false);
            cornerYEditors[i].setText(juce::String((int)slice.corners[i].y), false);
        }
        inputCoordsEditors[0].setText(juce::String((int)slice.inputRect.getX()), false);
        inputCoordsEditors[1].setText(juce::String((int)slice.inputRect.getY()), false);
        inputCoordsEditors[2].setText(juce::String((int)slice.inputRect.getWidth()), false);
        inputCoordsEditors[3].setText(juce::String((int)slice.inputRect.getHeight()), false);
        subdivXEditor.setText(juce::String(slice.subdivX), false);
        subdivYEditor.setText(juce::String(slice.subdivY), false);

        blendTopSlider.setValue(slice.blendTop, juce::dontSendNotification);
        blendBottomSlider.setValue(slice.blendBottom, juce::dontSendNotification);
        blendLeftSlider.setValue(slice.blendLeft, juce::dontSendNotification);
        blendRightSlider.setValue(slice.blendRight, juce::dontSendNotification);
        blendGammaSlider.setValue(slice.blendGamma, juce::dontSendNotification);
    }
    updateInspectorVisibility();
    updateLists();
}

void AdvancedOutputWindow::EditorComponent::addScreen()
{
    OutputScreen screen;
    screen.name = "Display " + juce::String(screens.size() + 1);
    screens.push_back(screen);
    selectScreen((int)screens.size() - 1);
    syncDisplayWindows();
}

void AdvancedOutputWindow::EditorComponent::addSlice()
{
    if (selectedScreenIdx >= 0 && selectedScreenIdx < (int)screens.size())
    {
        auto& screen = screens[selectedScreenIdx];
        WarpSlice slice;
        slice.name = "Slice " + juce::String(screen.slices.size() + 1);
        slice.inputRect = juce::Rectangle<float>(0, 0, 1920, 1080);
        
        // Quad corners initialization (centered quad)
        slice.corners[0] = juce::Point<float>(150, 120); // TL
        slice.corners[1] = juce::Point<float>(450, 120); // TR
        slice.corners[2] = juce::Point<float>(450, 420); // BR
        slice.corners[3] = juce::Point<float>(150, 420); // BL
        
        screen.slices.push_back(slice);
        selectSlice((int)screen.slices.size() - 1);
    }
}

void AdvancedOutputWindow::EditorComponent::deleteSelected()
{
    if (selectedScreenIdx >= 0 && selectedScreenIdx < (int)screens.size())
    {
        if (selectedSliceIdx >= 0 && selectedSliceIdx < (int)screens[selectedScreenIdx].slices.size())
        {
            // Delete slice
            screens[selectedScreenIdx].slices.erase(screens[selectedScreenIdx].slices.begin() + selectedSliceIdx);
            selectSlice(-1);
        }
        else
        {
            // Delete screen
            screens.erase(screens.begin() + selectedScreenIdx);
            selectScreen(-1);
            syncDisplayWindows();
        }
    }
}

void AdvancedOutputWindow::EditorComponent::updateInspectorVisibility()
{
    // Common Title and Name
    bool hasSelection = (selectedScreenIdx >= 0);
    rightSidebarTitle.setVisible(hasSelection);
    nameLabel.setVisible(hasSelection);
    nameEditor.setVisible(hasSelection);

    if (!hasSelection)
    {
        // Hide everything on right sidebar
        deviceLabel.setVisible(false);
        deviceSelector.setVisible(false);
        resolutionLabel.setVisible(false);
        widthEditor.setVisible(false);
        heightEditor.setVisible(false);
        displayOpacityLabel.setVisible(false);
        displayOpacitySlider.setVisible(false);
        brightnessLabel.setVisible(false);
        brightnessSlider.setVisible(false);
        contrastLabel.setVisible(false);
        contrastSlider.setVisible(false);
        rgbLabel.setVisible(false);
        redSlider.setVisible(false);
        greenSlider.setVisible(false);
        blueSlider.setVisible(false);

        inputCoordsLabel.setVisible(false);
        for (int i = 0; i < 4; ++i)
        {
            inputLabels[i].setVisible(false);
            inputCoordsEditors[i].setVisible(false);
            cornerLabels[i].setVisible(false);
            cornerXEditors[i].setVisible(false);
            cornerYEditors[i].setVisible(false);
        }
        opacityLabel.setVisible(false);
        opacitySlider.setVisible(false);
        cornersLabel.setVisible(false);
        subdivLabel.setVisible(false);
        subdivXLabel.setVisible(false);
        subdivXEditor.setVisible(false);
        subdivYLabel.setVisible(false);
        subdivYEditor.setVisible(false);
        resetGridBtn.setVisible(false);

        edgeBlendLabel.setVisible(false);
        blendTopLabel.setVisible(false); blendTopSlider.setVisible(false);
        blendBottomLabel.setVisible(false); blendBottomSlider.setVisible(false);
        blendLeftLabel.setVisible(false); blendLeftSlider.setVisible(false);
        blendRightLabel.setVisible(false); blendRightSlider.setVisible(false);
        blendGammaLabel.setVisible(false); blendGammaSlider.setVisible(false);
        return;
    }

    bool isSliceSelected = (selectedSliceIdx >= 0);

    // 1. Display controls visible if no slice selected
    bool displayVisible = !isSliceSelected;
    rightSidebarTitle.setText(displayVisible ? "Display Properties" : "Slice Properties", juce::dontSendNotification);

    deviceLabel.setVisible(displayVisible);
    deviceSelector.setVisible(displayVisible);
    resolutionLabel.setVisible(displayVisible);
    widthEditor.setVisible(displayVisible);
    heightEditor.setVisible(displayVisible);
    displayOpacityLabel.setVisible(displayVisible);
    displayOpacitySlider.setVisible(displayVisible);
    brightnessLabel.setVisible(displayVisible);
    brightnessSlider.setVisible(displayVisible);
    contrastLabel.setVisible(displayVisible);
    contrastSlider.setVisible(displayVisible);
    rgbLabel.setVisible(displayVisible);
    redSlider.setVisible(displayVisible);
    greenSlider.setVisible(displayVisible);
    blueSlider.setVisible(displayVisible);

    // 2. Input Slice controls visible if slice selected and in Input Selection Mode
    bool inputSliceVisible = (isSliceSelected && isInputSelectionMode);
    inputCoordsLabel.setVisible(inputSliceVisible);
    for (int i = 0; i < 4; ++i)
    {
        inputLabels[i].setVisible(inputSliceVisible);
        inputCoordsEditors[i].setVisible(inputSliceVisible);
    }

    // 3. Output Slice controls visible if slice selected and in Output Transformation Mode
    bool outputSliceVisible = (isSliceSelected && !isInputSelectionMode);
    opacityLabel.setVisible(outputSliceVisible);
    opacitySlider.setVisible(outputSliceVisible);
    cornersLabel.setVisible(outputSliceVisible);
    for (int i = 0; i < 4; ++i)
    {
        cornerLabels[i].setVisible(outputSliceVisible);
        cornerXEditors[i].setVisible(outputSliceVisible);
        cornerYEditors[i].setVisible(outputSliceVisible);
    }
    subdivLabel.setVisible(outputSliceVisible);
    subdivXLabel.setVisible(outputSliceVisible);
    subdivXEditor.setVisible(outputSliceVisible);
    subdivYLabel.setVisible(outputSliceVisible);
    subdivYEditor.setVisible(outputSliceVisible);
    resetGridBtn.setVisible(outputSliceVisible);

    edgeBlendLabel.setVisible(outputSliceVisible);
    blendTopLabel.setVisible(outputSliceVisible); blendTopSlider.setVisible(outputSliceVisible);
    blendBottomLabel.setVisible(outputSliceVisible); blendBottomSlider.setVisible(outputSliceVisible);
    blendLeftLabel.setVisible(outputSliceVisible); blendLeftSlider.setVisible(outputSliceVisible);
    blendRightLabel.setVisible(outputSliceVisible); blendRightSlider.setVisible(outputSliceVisible);
    blendGammaLabel.setVisible(outputSliceVisible); blendGammaSlider.setVisible(outputSliceVisible);

    resized(); // Re-layout right sidebar controls
}

void AdvancedOutputWindow::EditorComponent::syncDisplayWindows()
{
    // Close windows that are no longer needed or changed device type
    for (int i = (int)activeWindows.size() - 1; i >= 0; --i)
    {
        auto& win = activeWindows[i];
        bool stillExists = false;
        for (auto& screen : screens)
        {
            if (screen.name == win->getName() && (screen.device == "Windowed" || screen.device.startsWith("Display")))
            {
                stillExists = true;
                break;
            }
        }
        if (!stillExists)
        {
            activeWindows.erase(activeWindows.begin() + i);
        }
    }

    // Open or update windows for displays
    for (auto& screen : screens)
    {
        if (screen.device == "Windowed" || screen.device.startsWith("Display"))
        {
            // Check if window already exists
            OutputDeviceWindow* existingWin = nullptr;
            for (auto& win : activeWindows)
            {
                if (win->getName() == screen.name)
                {
                    existingWin = win.get();
                    break;
                }
            }

            if (existingWin == nullptr)
            {
                // Create a new window (pass *this so the window has access to the canvas editor dimensions)
                auto newWin = std::make_unique<OutputDeviceWindow>(screen.name, mediaEngine, screen, *this);
                newWin->setVisible(true);
                existingWin = newWin.get();
                activeWindows.push_back(std::move(newWin));
            }

            // Apply device properties (bounds/fullscreen)
            if (screen.device == "Windowed")
            {
                existingWin->setUsingNativeTitleBar(true);
                existingWin->setResizable(true, false);
                existingWin->setFullScreen(false);
                existingWin->setAlwaysOnTop(false);
                existingWin->setBounds(100, 100, screen.width, screen.height);
            }
            else if (screen.device.startsWith("Display"))
            {
                // Parse display index
                int dispIdx = screen.device.getLastCharacters(1).getIntValue() - 1;
                auto& displays = juce::Desktop::getInstance().getDisplays();
                if (dispIdx >= 0 && dispIdx < displays.displays.size())
                {
                    auto bounds = displays.displays[dispIdx].totalArea;
                    existingWin->setUsingNativeTitleBar(false);
                    existingWin->setTitleBarHeight(0);
                    existingWin->setResizable(false, false);
                    existingWin->setBounds(bounds);
                    existingWin->setAlwaysOnTop(true); // Cover taskbar
                }
                else
                {
                    // Fallback to Windowed size
                    existingWin->setUsingNativeTitleBar(true);
                    existingWin->setResizable(true, false);
                    existingWin->setAlwaysOnTop(false);
                    existingWin->setBounds(100, 100, screen.width, screen.height);
                }
            }
        }
    }
}

void OutputDeviceView::createShaders()
{
    juce::String vertexShader = R"(
        attribute vec2 position;
        attribute vec2 texCoord;
        attribute vec2 sliceCoord;
        varying vec2 vTexCoord;
        varying vec2 vSliceCoord;
        void main() {
            gl_Position = vec4(position, 0.0, 1.0);
            vTexCoord = texCoord;
            vSliceCoord = sliceCoord;
        }
    )";

    juce::String fragmentShader = R"(
        uniform sampler2D tex;
        uniform float alpha;
        uniform float blendLeft;
        uniform float blendRight;
        uniform float blendTop;
        uniform float blendBottom;
        uniform float blendGamma;
        
        uniform float fxVhs;
        uniform float fxRgbShift;
        uniform float fxScanlines;

        varying vec2 vTexCoord;
        varying vec2 vSliceCoord;
        
        float applyEdgeBlend(float coord, float blendSize, float gamma) {
            if (blendSize <= 0.0) return 1.0;
            if (coord >= blendSize) return 1.0;
            return pow(coord / blendSize, gamma);
        }

        void main() {
            vec2 uv = vTexCoord;
            
            // VHS Tearing (horizontal shift based on Y and fxVhs)
            if (fxVhs > 0.0) {
                float tear = sin(uv.y * 50.0) * cos(uv.y * 13.0) * sin(uv.y * 123.0);
                uv.x += tear * fxVhs * 0.05;
            }
            
            // RGB Shift
            vec4 col;
            if (fxRgbShift > 0.0) {
                float shift = fxRgbShift * 0.02;
                col.r = texture2D(tex, uv + vec2(shift, 0.0)).r;
                col.g = texture2D(tex, uv).g;
                col.b = texture2D(tex, uv - vec2(shift, 0.0)).b;
                col.a = texture2D(tex, uv).a;
            } else {
                col = texture2D(tex, uv);
            }
            
            // Scanlines
            if (fxScanlines > 0.0) {
                float sl = sin(uv.y * 800.0) * 0.5 + 0.5; // 0 to 1
                col.rgb *= mix(1.0, sl, fxScanlines * 0.5);
            }

            float blendX1 = applyEdgeBlend(vSliceCoord.x, blendLeft, blendGamma);
            float blendX2 = applyEdgeBlend(1.0 - vSliceCoord.x, blendRight, blendGamma);
            float blendY1 = applyEdgeBlend(vSliceCoord.y, blendTop, blendGamma);
            float blendY2 = applyEdgeBlend(1.0 - vSliceCoord.y, blendBottom, blendGamma);
            
            float mask = blendX1 * blendX2 * blendY1 * blendY2;
            gl_FragColor = col * alpha * mask;
        }
    )";

    std::unique_ptr<juce::OpenGLShaderProgram> newShader (new juce::OpenGLShaderProgram (glContext));
    if (newShader->addVertexShader (vertexShader)
     && newShader->addFragmentShader (fragmentShader)
     && newShader->link())
    {
        shaderProgram = std::move (newShader);
    }
    else
    {
        juce::Logger::writeToLog ("Shader compilation failed: " + newShader->getLastError());
    }
}

void OutputDeviceView::renderOpenGL()
{
    juce::OpenGLHelpers::clear (juce::Colours::black);
    
    if (screen.slices.empty() || !screen.isEnabled)
        return;
        
    float canvasW = 0.0f;
    float canvasH = 0.0f;
    if (editor.canvasArea != nullptr)
    {
        canvasW = (float)editor.canvasArea->getWidth();
        canvasH = (float)editor.canvasArea->getHeight();
    }
    if (canvasW <= 0.0f || canvasH <= 0.0f)
        return;

    const auto& compImage = mediaEngine.getCompositionFrame();
    if (compImage.isNull())
        return;

    texture.loadImage (compImage);
    texture.bind();

    if (shaderProgram != nullptr)
    {
        shaderProgram->use();
        
        for (const auto& slice : screen.slices)
        {
            if (!slice.isEnabled)
                continue;

            int cols = slice.subdivX + 1;
            int rows = slice.subdivY + 1;
            
            if (slice.gridPoints.size() != cols * rows)
                continue; // Safety check
                
            std::vector<Vertex> vertices;
            std::vector<juce::uint32> indices;
            
            // Build vertices
            for (int y = 0; y < rows; ++y)
            {
                for (int x = 0; x < cols; ++x)
                {
                    const auto& pt = slice.gridPoints[y * cols + x];
                    
                    Vertex v;
                    // Map canvas coords to NDC (-1 to 1)
                    v.position[0] = (pt.x / canvasW) * 2.0f - 1.0f;
                    v.position[1] = 1.0f - (pt.y / canvasH) * 2.0f; // Flip Y for OpenGL
                    
                    // Calculate UV
                    float tx = (float)x / (cols - 1);
                    float ty = (float)y / (rows - 1);
                    
                    float srcX = slice.inputRect.getX() + tx * slice.inputRect.getWidth();
                    float srcY = slice.inputRect.getY() + ty * slice.inputRect.getHeight();
                    
                    v.texCoord[0] = srcX / compImage.getWidth();
                    v.texCoord[1] = srcY / compImage.getHeight();
                    
                    v.sliceCoord[0] = tx;
                    v.sliceCoord[1] = ty;
                    
                    vertices.push_back(v);
                }
            }
            
            // Build indices for triangles
            for (int y = 0; y < rows - 1; ++y)
            {
                for (int x = 0; x < cols - 1; ++x)
                {
                    int tl = y * cols + x;
                    int tr = tl + 1;
                    int bl = (y + 1) * cols + x;
                    int br = bl + 1;
                    
                    // Triangle 1
                    indices.push_back(tl);
                    indices.push_back(bl);
                    indices.push_back(tr);
                    
                    // Triangle 2
                    indices.push_back(tr);
                    indices.push_back(bl);
                    indices.push_back(br);
                }
            }
            
            // Setup attributes and uniforms
            GLint posAttrib = glContext.extensions.glGetAttribLocation (shaderProgram->getProgramID(), "position");
            GLint texAttrib = glContext.extensions.glGetAttribLocation (shaderProgram->getProgramID(), "texCoord");
            GLint sliceAttrib = glContext.extensions.glGetAttribLocation (shaderProgram->getProgramID(), "sliceCoord");
            
            if (posAttrib >= 0)
            {
                glContext.extensions.glVertexAttribPointer (posAttrib, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex), &vertices[0].position);
                glContext.extensions.glEnableVertexAttribArray (posAttrib);
            }
            if (texAttrib >= 0)
            {
                glContext.extensions.glVertexAttribPointer (texAttrib, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex), &vertices[0].texCoord);
                glContext.extensions.glEnableVertexAttribArray (texAttrib);
            }
            if (sliceAttrib >= 0)
            {
                glContext.extensions.glVertexAttribPointer (sliceAttrib, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex), &vertices[0].sliceCoord);
                glContext.extensions.glEnableVertexAttribArray (sliceAttrib);
            }
            
            shaderProgram->setUniform("tex", 0);
            shaderProgram->setUniform("alpha", slice.opacity * screen.opacity);
            shaderProgram->setUniform("blendLeft", slice.blendLeft);
            shaderProgram->setUniform("blendRight", slice.blendRight);
            shaderProgram->setUniform("blendTop", slice.blendTop);
            shaderProgram->setUniform("blendBottom", slice.blendBottom);
            shaderProgram->setUniform("blendGamma", slice.blendGamma);

            // Master FX Uniforms
            shaderProgram->setUniform("fxVhs", mediaEngine.fxVhs.load());
            shaderProgram->setUniform("fxRgbShift", mediaEngine.fxRgbShift.load());
            shaderProgram->setUniform("fxScanlines", mediaEngine.fxScanlines.load());
            
            // Enable blending
            juce::gl::glEnable (GL_BLEND);
            juce::gl::glBlendFunc (GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
            
            juce::gl::glDrawElements (GL_TRIANGLES, (GLsizei)indices.size(), GL_UNSIGNED_INT, indices.data());
            
            if (posAttrib >= 0) glContext.extensions.glDisableVertexAttribArray (posAttrib);
            if (texAttrib >= 0) glContext.extensions.glDisableVertexAttribArray (texAttrib);
            if (sliceAttrib >= 0) glContext.extensions.glDisableVertexAttribArray (sliceAttrib);
        }
    }
    
    texture.unbind();
}
