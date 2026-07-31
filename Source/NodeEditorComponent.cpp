#include "NodeEditorComponent.h"

NodeEditorComponent::NodeEditorComponent()
{
    addAndMakeVisible(propertiesPanel);
    propertiesPanel.onNodeChanged = [this]() {
        repaint();
    };
}

void NodeEditorComponent::setGraph(std::shared_ptr<NodeGraph> graph)
{
    currentGraph = graph;
    selectedNode = nullptr;
    propertiesPanel.setNode(nullptr);
    if (onGraphChanged) onGraphChanged(currentGraph);
    repaint();
}

void NodeEditorComponent::paint(juce::Graphics& g)
{
    g.fillAll(settingsManager ? settingsManager->getTheme().background : juce::Colour(0xff121214)); // Very dark background

    if (!currentGraph)
    {
        g.setColour(settingsManager ? settingsManager->getTheme().border : juce::Colour(0xff3f3f46));
        g.setFont(14.0f);
        g.drawText("No Node Graph Selected", getLocalBounds(), juce::Justification::centred);
        return;
    }

    g.saveState();
    g.addTransform(juce::AffineTransform::translation(panX, panY));

    // Draw links
    g.setColour(settingsManager ? settingsManager->getTheme().text.withAlpha(0.5f) : juce::Colours::white.withAlpha(0.5f));
    for (const auto& link : currentGraph->links)
    {
        BaseNode* fromNode = nullptr;
        BaseNode* toNode = nullptr;

        for (auto& n : currentGraph->nodes)
        {
            if (n->id == link.fromNodeId) fromNode = n.get();
            if (n->id == link.toNodeId) toNode = n.get();
        }

        if (fromNode && toNode)
        {
            float startY = fromNode->y + (fromNode->height / (float)(fromNode->getNumOutputPins() + 1)) * (link.fromPinIndex + 1);
            float endY = toNode->y + (toNode->height / (float)(toNode->getNumInputPins() + 1)) * (link.toPinIndex + 1);

            float startX = fromNode->x + fromNode->width;
            float endX = toNode->x;

            juce::Path p;
            p.startNewSubPath(startX, startY);
            p.cubicTo(startX + 50, startY, endX - 50, endY, endX, endY);
            g.strokePath(p, juce::PathStrokeType(2.0f));
        }
    }

    if (drawingLinkFromNode != nullptr)
    {
        float startY = drawingLinkFromNode->y + (drawingLinkFromNode->height / (float)(drawingLinkFromNode->getNumOutputPins() + 1)) * (drawingLinkFromPin + 1);
        float startX = drawingLinkFromNode->x + drawingLinkFromNode->width;
        float endX = drawingLinkEndPos.x;
        float endY = drawingLinkEndPos.y;

        juce::Path p;
        p.startNewSubPath(startX, startY);
        p.cubicTo(startX + 50, startY, endX - 50, endY, endX, endY);
        g.setColour(settingsManager ? settingsManager->getTheme().text : juce::Colours::white);
        g.strokePath(p, juce::PathStrokeType(2.0f));
    }

    // Draw nodes
    for (auto& n : currentGraph->nodes)
    {
        juce::Rectangle<int> nodeBounds(n->x, n->y, n->width, n->height);
        
        g.setColour(settingsManager ? settingsManager->getTheme().panelBackground : juce::Colour(0xff27272a));
        g.fillRoundedRectangle(nodeBounds.toFloat(), 5.0f);

        if (selectedNode == n.get())
        {
            g.setColour(settingsManager ? settingsManager->getTheme().accent1 : juce::Colour(0xff00f0a8)); // neon green border for selected
            g.drawRoundedRectangle(nodeBounds.toFloat(), 5.0f, 2.0f);
        }
        else
        {
            g.setColour(settingsManager ? settingsManager->getTheme().border : juce::Colour(0xff3f3f46)); // dull border for normal
            g.drawRoundedRectangle(nodeBounds.toFloat(), 5.0f, 1.0f);
        }

        g.setColour(settingsManager ? settingsManager->getTheme().text : juce::Colours::white);
        g.setFont(12.0f);
        g.drawText(n->name, nodeBounds, juce::Justification::centred);

        // Draw node color if it's a solid color node
        if (auto* scNode = dynamic_cast<SolidColourNode*>(n.get()))
        {
            g.setColour(scNode->colour.baseValue);
            g.fillRoundedRectangle(n->x + 5.0f, n->y + 5.0f, 15.0f, 15.0f, 3.0f);
        }

        // Draw pins
        g.setColour(settingsManager ? settingsManager->getTheme().text : juce::Colours::white);
        for (int i = 0; i < n->getNumInputPins(); ++i)
        {
            float py = n->y + (n->height / (float)(n->getNumInputPins() + 1)) * (i + 1);
            g.fillEllipse(n->x - 5.0f, py - 5.0f, 10.0f, 10.0f);
        }
        for (int i = 0; i < n->getNumOutputPins(); ++i)
        {
            float py = n->y + (n->height / (float)(n->getNumOutputPins() + 1)) * (i + 1);
            g.fillEllipse(n->x + n->width - 5.0f, py - 5.0f, 10.0f, 10.0f);
        }
    }

    g.restoreState();
}

void NodeEditorComponent::mouseDown(const juce::MouseEvent& event)
{
    draggingNode = nullptr;

    if (!currentGraph) return;

    if (event.mods.isMiddleButtonDown())
    {
        isPanning = true;
        panStartX = event.getScreenX() - (int)panX;
        panStartY = event.getScreenY() - (int)panY;
        return;
    }

    bool clickedOnNode = false;
    auto worldPos = event.getPosition().translated(-(int)panX, -(int)panY);

    // Reverse iterate to find top-most node
    for (int i = (int)currentGraph->nodes.size() - 1; i >= 0; --i)
    {
        auto* n = currentGraph->nodes[i].get();

        // Check if clicking on output pin
        for (int p = 0; p < n->getNumOutputPins(); ++p)
        {
            float py = n->y + (n->height / (float)(n->getNumOutputPins() + 1)) * (p + 1);
            juce::Rectangle<float> outPin(n->x + n->width - 10.0f, py - 10.0f, 20.0f, 20.0f);
            if (outPin.contains(worldPos.toFloat()))
            {
                drawingLinkFromNode = n;
                drawingLinkFromPin = p;
                drawingLinkEndPos = worldPos.toFloat();
                return;
            }
        }

        juce::Rectangle<int> bounds(n->x, n->y, n->width, n->height);
        if (bounds.contains(worldPos))
        {
            if (event.mods.isCommandDown() || event.mods.isAltDown())
            {
                currentGraph->removeNode(n->id);
                if (selectedNode == n)
                {
                    selectedNode = nullptr;
                    propertiesPanel.setNode(nullptr);
                }
                if (onGraphChanged) onGraphChanged(currentGraph);
                repaint();
                return;
            }

            draggingNode = n;
            if (selectedNode != n)
            {
                selectedNode = n;
                propertiesPanel.setNode(selectedNode);
            }
            dragOffsetX = n->x - worldPos.x;
            dragOffsetY = n->y - worldPos.y;
            clickedOnNode = true;
            if (event.mods.isRightButtonDown())
            {
                juce::PopupMenu menu;
                menu.addItem(1, "Remove Node");
                menu.showMenuAsync(juce::PopupMenu::Options(), [this, id = n->id](int result) {
                    if (result == 1)
                    {
                        if (selectedNode && selectedNode->id == id)
                        {
                            selectedNode = nullptr;
                            propertiesPanel.setNode(nullptr);
                        }
                        currentGraph->removeNode(id);
                        if (onGraphChanged) onGraphChanged(currentGraph);
                        repaint();
                    }
                });
            }

            break;
        }
    }

    if (!clickedOnNode)
    {
        selectedNode = nullptr;
        propertiesPanel.setNode(nullptr);

        if (event.mods.isRightButtonDown())
        {
            juce::PopupMenu menu;
            menu.addItem(1, "Add Solid Colour Node");
            menu.addItem(2, "Add Line Generator Node");
            menu.addItem(3, "Add Noise Generator Node");
            menu.addItem(4, "Add Output Node");
            menu.addItem(5, "Add Composite Node");
            menu.addItem(6, "Add Displacement Node");
            menu.addItem(7, "Add Edge Detection Node");
            menu.addItem(8, "Add ShaderToy Node");
            menu.showMenuAsync(juce::PopupMenu::Options(), [this, worldPos](int result) {
                if (result == 0) return;

                int newId = 1;
                for (const auto& n : currentGraph->nodes)
                    newId = juce::jmax(newId, n->id + 1);

                if (result == 1)
                {
                    currentGraph->addNode(std::make_shared<SolidColourNode>(newId, worldPos.x, worldPos.y));
                }
                else if (result == 2)
                {
                    currentGraph->addNode(std::make_shared<LineNode>(newId, worldPos.x, worldPos.y));
                }
                else if (result == 3)
                {
                    currentGraph->addNode(std::make_shared<NoiseNode>(newId, worldPos.x, worldPos.y));
                }
                else if (result == 4)
                {
                    currentGraph->addNode(std::make_shared<OutputNode>(newId, worldPos.x, worldPos.y));
                }
                else if (result == 5)
                {
                    currentGraph->addNode(std::make_shared<CompositeNode>(newId, worldPos.x, worldPos.y));
                }
                else if (result == 6)
                {
                    currentGraph->addNode(std::make_shared<DisplacementNode>(newId, worldPos.x, worldPos.y));
                }
                else if (result == 7)
                {
                    currentGraph->addNode(std::make_shared<EdgeDetectionNode>(newId, worldPos.x, worldPos.y));
                }
                else if (result == 8)
                {
                    currentGraph->addNode(std::make_shared<ShaderToyNode>(newId, worldPos.x, worldPos.y));
                }
                if (onGraphChanged) onGraphChanged(currentGraph);
                repaint();
            });
        }
    }
    
    repaint();
}

void NodeEditorComponent::mouseDrag(const juce::MouseEvent& event)
{
    if (isPanning)
    {
        panX = (float)(event.getScreenX() - panStartX);
        panY = (float)(event.getScreenY() - panStartY);
        repaint();
    }
    else if (drawingLinkFromNode != nullptr)
    {
        drawingLinkEndPos = event.getPosition().translated(-(int)panX, -(int)panY).toFloat();
        repaint();
    }
    else if (draggingNode != nullptr)
    {
        auto worldPos = event.getPosition().translated(-(int)panX, -(int)panY);
        draggingNode->x = worldPos.x + dragOffsetX;
        draggingNode->y = worldPos.y + dragOffsetY;
        repaint();
    }
}

void NodeEditorComponent::mouseUp(const juce::MouseEvent& event)
{
    if (drawingLinkFromNode != nullptr)
    {
        auto worldPos = event.getPosition().translated(-(int)panX, -(int)panY).toFloat();
        for (int i = (int)currentGraph->nodes.size() - 1; i >= 0; --i)
        {
            auto* n = currentGraph->nodes[i].get();
            if (n != drawingLinkFromNode)
            {
                for (int p = 0; p < n->getNumInputPins(); ++p)
                {
                    float py = n->y + (n->height / (float)(n->getNumInputPins() + 1)) * (p + 1);
                    juce::Rectangle<float> inPin(n->x - 10.0f, py - 10.0f, 20.0f, 20.0f);
                    if (inPin.contains(worldPos))
                    {
                        // Check if link already exists
                        bool linkExists = false;
                        for (const auto& link : currentGraph->links)
                        {
                            if (link.fromNodeId == drawingLinkFromNode->id && link.fromPinIndex == drawingLinkFromPin && link.toNodeId == n->id && link.toPinIndex == p)
                            {
                                linkExists = true;
                                break;
                            }
                        }
                        if (!linkExists)
                        {
                            currentGraph->addLink(drawingLinkFromNode->id, drawingLinkFromPin, n->id, p);
                        }
                        drawingLinkFromNode = nullptr;
                        repaint();
                        return;
                    }
                }
            }
        }
        drawingLinkFromNode = nullptr;
        repaint();
    }

    isPanning = false;
    draggingNode = nullptr;
}

void NodeEditorComponent::mouseDoubleClick(const juce::MouseEvent& event)
{
    if (!currentGraph) return;

    auto worldPos = event.getPosition().translated(-(int)panX, -(int)panY).toFloat();

    // Iterate backwards through links to find the top one
    for (int i = (int)currentGraph->links.size() - 1; i >= 0; --i)
    {
        auto& link = currentGraph->links[i];
        BaseNode* fromNode = nullptr;
        BaseNode* toNode = nullptr;

        for (auto& n : currentGraph->nodes)
        {
            if (n->id == link.fromNodeId) fromNode = n.get();
            if (n->id == link.toNodeId) toNode = n.get();
        }

        if (fromNode && toNode)
        {
            float startY = fromNode->y + (fromNode->height / (float)(fromNode->getNumOutputPins() + 1)) * (link.fromPinIndex + 1);
            float endY = toNode->y + (toNode->height / (float)(toNode->getNumInputPins() + 1)) * (link.toPinIndex + 1);

            float startX = fromNode->x + fromNode->width;
            float endX = toNode->x;

            juce::Path p;
            p.startNewSubPath(startX, startY);
            p.cubicTo(startX + 50, startY, endX - 50, endY, endX, endY);

            juce::PathStrokeType stroke(10.0f); // Generous hit area
            juce::Path strokedPath;
            stroke.createStrokedPath(strokedPath, p);

            if (strokedPath.contains(worldPos))
            {
                currentGraph->links.erase(currentGraph->links.begin() + i);
                repaint();
                break;
            }
        }
    }
}

void NodeEditorComponent::resized()
{
    propertiesPanel.setBounds(0, 0, 250, getHeight());
}
