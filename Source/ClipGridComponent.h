#pragma once
#include <JuceHeader.h>
#include "MediaEngine.h"
#include <memory>
#include <functional>

class ClipGridComponent  : public juce::Component,
                           public juce::FileDragAndDropTarget
{
public:
    ClipGridComponent(MediaEngine& engine);
    ~ClipGridComponent() override = default;

    void paint(juce::Graphics& g) override;
    void resized() override;

    // Callbacks
    std::function<void(int layer, int col)> onClipSelected;
    std::function<void(int layer, int col)> onClipTriggered;

    // FileDragAndDropTarget implementation
    bool isInterestedInFileDrag(const juce::StringArray& files) override;
    void filesDropped(const juce::StringArray& files, int x, int y) override;
    void fileDragMove(const juce::StringArray& files, int x, int y) override;
    void fileDragExit(const juce::StringArray& files) override;

    void updateGrid();
    void rebuildUI();
    void markThumbnailDirty(int layer, int col);

private:
    void mouseDown(const juce::MouseEvent& event) override;
    void mouseDoubleClick(const juce::MouseEvent& event) override;

    void promptLoadMediaClip(int l, int c);
    void addNodeClip(int l, int c);

    // Helpers to identify coordinates
    bool getCellAtPos(juce::Point<int> pos, int& layer, int& col);
    bool getLayerControlAtPos(juce::Point<int> pos, int& layer, int& controlType); // 0: volume slider, 1: solo, 2: mute, 3: bypass

    MediaEngine& mediaEngine;

    // UI elements for layer controls
    struct LayerControlUI
    {
        std::unique_ptr<juce::Slider> opacitySlider;
        std::unique_ptr<juce::TextButton> soloButton;
        std::unique_ptr<juce::TextButton> muteButton;
        std::unique_ptr<juce::TextButton> bypassButton;
        std::unique_ptr<juce::TextButton> playButton;
        std::unique_ptr<juce::TextButton> stopButton;
    };

    std::vector<LayerControlUI> layerControls;

    int cellWidth = 90;
    int cellHeight = 45;
    int leftHeaderWidth = 180;
    int topHeaderHeight = 30;

    std::unique_ptr<juce::FileChooser> fileChooser;

    std::vector<std::vector<juce::Image>> thumbnailCache;
    std::vector<std::vector<bool>> thumbnailDirty;

    int dragHoverLayer = -1;
    int dragHoverCol = -1;

    void ensureThumbnail(int layer, int col, float w, float h);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ClipGridComponent)
};
