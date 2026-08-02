#pragma once
#include <JuceHeader.h>
#include <vector>
#include <foleys_video_engine/foleys_video_engine.h>
#include "NodeGraph.h"
#include "Parameter.h"
#include "MappingManager.h"
#include "SettingsManager.h"

struct TransformState
{
    Parameter posX{0.0f};
    Parameter posY{0.0f};
    Parameter scale{1.0f};
    Parameter scaleX{1.0f};
    Parameter scaleY{1.0f};
    Parameter rotationX{0.0f};
    Parameter rotationY{0.0f};
    Parameter rotationZ{0.0f};
    Parameter anchorX{0.5f};
    Parameter anchorY{0.5f};
    Parameter opacity{1.0f};
    int blendMode = 0;      // 0: Normal, 1: Add, 2: Subtract, 3: Multiply
    int alphaType = 0;      // 0: Premultiplied, 1: Straight
    int width = 1920;
    int height = 1080;
};

struct TransportState
{
    bool isPlaying = false;
    double speed = 1.0;
    double duration = 10.0; // seconds
    double position = 0.0;  // seconds
    bool loop = true;
};

struct ClipState
{
    juce::String name = "Empty";
    juce::File file;
    juce::Image image;
    bool isLoaded = false;
    bool isProcedural = true;
    int proceduralType = 0; // 0: Wave Plasma, 1: Tunnel Grid, 2: Audio Spectrum Wave, 3: Spiral Fractal
    bool isNodeBased = false;
    std::shared_ptr<NodeGraph> nodeGraph;
    int sourceCol = -1;     // Column index in grid this clip belongs to

    TransportState transport;
    TransformState transform;

    std::shared_ptr<foleys::AVClip> videoClip;
};

class MediaEngine : public juce::AudioIODeviceCallback
{
public:
    SettingsManager settingsManager;
    foleys::VideoEngine videoEngine;
    MappingManager mappingManager;
    MediaEngine();
    ~MediaEngine() override = default;

    void audioDeviceAboutToStart (juce::AudioIODevice* device) override {}
    void audioDeviceStopped() override {}
    void audioDeviceIOCallbackWithContext (const float* const* inputChannelData, int numInputChannels,
                                           float* const* outputChannelData, int numOutputChannels,
                                           int numSamples, const juce::AudioIODeviceCallbackContext& context) override;


    // Update playhead positions based on time elapsed
    void update(double deltaTimeSeconds);

    // Render a clip state to a given graphics context
    void renderClip(juce::Graphics& g, const ClipState& clip, float width, float height, float masterOpacity = 1.0f, bool drawVideoAsPlaceholder = false);

    // Getters and setters for workspace grid
    ClipState& getClipInGrid(int layerIdx, int colIdx);
    void triggerClip(int layerIdx, int colIdx);
    void previewClipInGrid(int layerIdx, int colIdx);
    
    ClipState& getActiveLayerClip(int layerIdx);
    ClipState& getPreviewClip();
    
    int getSelectedLayer() const { return selectedLayer; }
    void setSelectedLayer(int layer) { selectedLayer = layer; }
    
    int getSelectedCol() const { return selectedCol; }
    void setSelectedCol(int col) { selectedCol = col; }

    std::atomic<float> fxVhs{0.0f};
    std::atomic<float> fxRgbShift{0.0f};
    std::atomic<float> fxScanlines{0.0f};

    double getLayerOpacity(int layerIdx) const { return (layerIdx >= 0 && layerIdx < getNumLayers()) ? layerOpacities[layerIdx] : 1.0; }
    void setLayerOpacity(int layerIdx, double opacity) { if (layerIdx >= 0 && layerIdx < getNumLayers()) layerOpacities[layerIdx] = opacity; }

    bool isLayerBypassed(int layerIdx) const { return (layerIdx >= 0 && layerIdx < getNumLayers()) ? layerBypassed[layerIdx] : false; }
    void setLayerBypassed(int layerIdx, bool bypassed) { if (layerIdx >= 0 && layerIdx < getNumLayers()) layerBypassed[layerIdx] = bypassed; }
    
    bool isLayerMuted(int layerIdx) const { return (layerIdx >= 0 && layerIdx < getNumLayers()) ? layerMuted[layerIdx] : false; }
    void setLayerMuted(int layerIdx, bool muted) { if (layerIdx >= 0 && layerIdx < getNumLayers()) layerMuted[layerIdx] = muted; }

    int getSoloedLayer() const { return soloedLayer; }
    void setSoloedLayer(int layerIdx) { soloedLayer = layerIdx; }

    int getNumLayers() const { return (int)gridClips.size(); }
    int getNumCols() const { return gridClips.empty() ? 0 : (int)gridClips[0].size(); }

    void removeLayer(int layerIdx);
    void removeColumn(int colIdx);
    
    void addLayer();
    void clearLayer(int layerIdx);
    void triggerColumn(int colIdx);
    void clearColumn(int colIdx);
    void clearClip(int layerIdx, int colIdx);
    void clearDeck();

    void updateCompositionFrame();
    const juce::Image& getCompositionFrame() const { return compositionFrame; }

private:
    void renderProceduralVisual(juce::Graphics& g, int type, double time, float w, float h, float opacity);

    // Dynamic layers by columns grid of clips
    std::vector<std::vector<ClipState>> gridClips;
    
    // Active clip being played in each layer (composited together)
    std::vector<ClipState> activeLayers;

    // Clip currently loaded in the preview monitor
    ClipState previewClip;

    std::vector<double> layerOpacities;
    std::vector<bool> layerBypassed;
    std::vector<bool> layerMuted;
    int soloedLayer = -1;

    int selectedLayer = -1;
    int selectedCol = -1;

    juce::Image compositionFrame;
};
