#pragma once
#include <JuceHeader.h>
#include <vector>
#include <memory>
#include <thread>
#include <atomic>
#include "Parameter.h"

class BaseNode
{
public:
    BaseNode(int id, const juce::String& name, int x, int y);
    virtual ~BaseNode() = default;

    virtual void process(juce::Image& target, double time) = 0;

    virtual int getNumInputPins() const { return 1; }
    virtual int getNumOutputPins() const { return 1; }

    virtual std::vector<std::pair<juce::String, Parameter*>> getParameters() { return {}; }

    int id;
    juce::String name;
    int x, y;
    int width = 120;
    int height = 60;

    int resolutionX = 512;
    int resolutionY = 512;
};

class SolidColourNode : public BaseNode
{
public:
    SolidColourNode(int id, int x, int y);
    void process(juce::Image& target, double time) override;

    int getNumInputPins() const override { return 0; }

    ColourParameter colour{juce::Colours::white};
};

class OutputNode : public BaseNode
{
public:
    OutputNode(int id, int x, int y);
    void process(juce::Image& target, double time) override;

    int getNumOutputPins() const override { return 0; }
};

class LineNode : public BaseNode
{
public:
    LineNode(int id, int x, int y);
    void process(juce::Image& target, double time) override;

    int getNumInputPins() const override { return 0; }

    Parameter startX{0.1f};
    Parameter startY{0.5f};
    Parameter endX{0.9f};
    Parameter endY{0.5f};
    Parameter thickness{10.0f};
    ColourParameter colour{juce::Colours::white};

    std::vector<std::pair<juce::String, Parameter*>> getParameters() override
    {
        return {
            {"Start X", &startX},
            {"Start Y", &startY},
            {"End X", &endX},
            {"End Y", &endY},
            {"Thickness", &thickness}
        };
    }
};

enum class NoiseType
{
    Simplex2D = 0,
    Simplex3D,
    Simplex4D,
    Perlin2D,
    Perlin3D,
    Perlin4D,
    Random,
    Sparse,
    Hermite,
    HarmonicSummation,
    RandomGPU,
    Alligator
};

class NoiseNode : public BaseNode
{
public:
    NoiseNode(int id, int x, int y);
    void process(juce::Image& target, double time) override;

    int getNumInputPins() const override { return 0; }

    NoiseType noiseType = NoiseType::Simplex3D;

    Parameter seed        { 1.0f };
    Parameter period      { 1.0f };
    Parameter harmonics   { 2.0f };
    Parameter harmonicSpread { 2.0f };
    Parameter harmonicGain { 0.7f };
    Parameter exponent    { 1.0f };
    Parameter amplitude   { 0.5f };
    Parameter offset      { 0.5f };

    // Cache: avoid recomputing when params haven't changed
    mutable juce::Image cachedImage;
    mutable bool        dirty = true;  // set to true whenever a param changes
    mutable double      lastRenderedTime = -1e9; // invalidates when time changes

    std::vector<std::pair<juce::String, Parameter*>> getParameters() override
    {
        return {
            {"Seed", &seed},
            {"Period", &period},
            {"Harmonics", &harmonics},
            {"Harmonic Spread", &harmonicSpread},
            {"Harmonic Gain", &harmonicGain},
            {"Exponent", &exponent},
            {"Amplitude", &amplitude},
            {"Offset", &offset}
        };
    }
};

struct NodeLink
{
    int fromNodeId;
    int fromPinIndex = 0;
    int toNodeId;
    int toPinIndex = 0;
};

class CompositeNode : public BaseNode
{
public:
    CompositeNode(int id, int x, int y);
    void process(juce::Image& target, double time) override;

    int getNumInputPins() const override { return 2; }

    int blendMode = 0; // 0=Normal, 1=Add, 2=Multiply, 3=Screen
};

class DisplacementNode : public BaseNode
{
public:
    DisplacementNode(int id, int x, int y);
    void process(juce::Image& target, double time) override;

    int getNumInputPins() const override { return 2; }

    Parameter amountX{0.1f};
    Parameter amountY{0.1f};

    std::vector<std::pair<juce::String, Parameter*>> getParameters() override
    {
        return {
            {"Amount X", &amountX},
            {"Amount Y", &amountY}
        };
    }
};

class EdgeDetectionNode : public BaseNode
{
public:
    EdgeDetectionNode(int id, int x, int y);
    void process(juce::Image& target, double time) override;

    int getNumInputPins() const override { return 1; }

    Parameter intensity{1.0f};

    std::vector<std::pair<juce::String, Parameter*>> getParameters() override
    {
        return {
            {"Intensity", &intensity}
        };
    }
};

class ShaderToyNode : public BaseNode
{
public:
    ShaderToyNode(int id, int x, int y);
    ~ShaderToyNode() override;
    void process(juce::Image& target, double time) override;

    int getNumInputPins() const override { return 1; }

    juce::String shaderSource;
    juce::String lastError;

    // Called after editing shaderSource to recompile
    void applyShader();

private:
    // Lazy-created on first process() call (must be on message thread)
    std::unique_ptr<class ShaderToyGLRenderer> renderer;
    int frameCount = 0;
    double lastTime = -1.0;
};

class NodeGraph
{
public:
    NodeGraph();
    ~NodeGraph() = default;

    void addNode(std::shared_ptr<BaseNode> node);
    void removeNode(int id);
    std::shared_ptr<BaseNode> getNode(int id);

    void addLink(int fromId, int fromPin, int toId, int toPin);

    juce::Image evaluateNode(int nodeId, double time);
    void renderOutput(juce::Graphics& g, juce::Rectangle<float> bounds, double time);

    std::vector<std::shared_ptr<BaseNode>> nodes;
    std::vector<NodeLink> links;
};
