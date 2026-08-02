#pragma once
#include <JuceHeader.h>
#include <juce_opengl/juce_opengl.h>
#include "MediaEngine.h"

struct WarpSlice
{
    juce::String name = "Slice";
    juce::Rectangle<float> inputRect;
    juce::Point<float> corners[4]; // TL, TR, BR, BL for quad warping
    
    int subdivX = 1;
    int subdivY = 1;
    std::vector<juce::Point<float>> gridPoints;

    // Edge Blending
    float blendTop = 0.0f;
    float blendBottom = 0.0f;
    float blendLeft = 0.0f;
    float blendRight = 0.0f;
    float blendGamma = 1.8f;

    float opacity = 1.0f;
    bool isEnabled = true;

    void initGrid()
    {
        gridPoints.clear();
        gridPoints.resize((subdivX + 1) * (subdivY + 1));
        for (int y = 0; y <= subdivY; ++y)
        {
            float ty = (float)y / subdivY;
            juce::Point<float> left = corners[0] + (corners[3] - corners[0]) * ty;
            juce::Point<float> right = corners[1] + (corners[2] - corners[1]) * ty;
            for (int x = 0; x <= subdivX; ++x)
            {
                float tx = (float)x / subdivX;
                gridPoints[y * (subdivX + 1) + x] = left + (right - left) * tx;
            }
        }
    }
};

struct OutputScreen
{
    juce::String name = "Display 1";
    juce::String device = "Windowed";
    int width = 1280;
    int height = 720;
    
    float opacity = 1.0f;
    float brightness = 0.5f;
    float contrast = 0.5f;
    float red = 0.5f;
    float green = 0.5f;
    float blue = 0.5f;
    
    bool isEnabled = true;

    std::vector<WarpSlice> slices;
};

class OutputDeviceView;
class OutputDeviceWindow;

class AdvancedOutputWindow : public juce::DocumentWindow
{
public:
    class EditorComponent : public juce::Component
    {
    public:
        EditorComponent(MediaEngine& engine);
        ~EditorComponent() override;

        void paint(juce::Graphics& g) override;
        void resized() override;

    private:
        void updateLists();
        void selectScreen(int index);
        void selectSlice(int index);
        void addScreen();
        void addSlice();
        void deleteSelected();
        void updateInspectorVisibility();

        MediaEngine& mediaEngine;

        std::vector<OutputScreen> screens;
        int selectedScreenIdx = -1;
        int selectedSliceIdx = -1;

        bool isInputSelectionMode = true; // Tab State: Input Selection vs Output Transformation

        // Warp Handle dragging state
        int draggedCornerIdx = -1; // -1 if none, 0..3 for selected slice corners/rect corners, 4 for dragging rect center
        juce::Point<float> dragStartOffset;
        juce::Rectangle<float> rectDragStart;
        std::vector<juce::Point<float>> gridDragStart;

        // UI Controls
        // Left Sidebar: Mapping Layout
        juce::Label leftSidebarTitle;
        juce::TextButton addScreenBtn;
        juce::TextButton addSliceBtn;
        juce::TreeView mappingTreeView;

        // Middle Top: Tabs
        juce::TextButton inputTabBtn;
        juce::TextButton outputTabBtn;

        // Right Sidebar: Inspector
        juce::Label rightSidebarTitle;
        juce::Label nameLabel;
        juce::TextEditor nameEditor;

        // 1. Display Inspector Controls
        juce::Label deviceLabel;
        juce::ComboBox deviceSelector;
        juce::Label resolutionLabel;
        juce::TextEditor widthEditor;
        juce::TextEditor heightEditor;
        juce::Label displayOpacityLabel;
        juce::Slider displayOpacitySlider;
        juce::Label brightnessLabel;
        juce::Slider brightnessSlider;
        juce::Label contrastLabel;
        juce::Slider contrastSlider;
        juce::Label rgbLabel;
        juce::Slider redSlider, greenSlider, blueSlider;

        // 2. Input Slice Inspector Controls
        juce::Label inputCoordsLabel;
        juce::Label inputLabels[4]; // X, Y, W, H
        juce::TextEditor inputCoordsEditors[4];

        // 3. Output Slice Inspector Controls
        juce::Label opacityLabel;
        juce::Slider opacitySlider;
        juce::Label cornersLabel;
        juce::Label cornerLabels[4];
        juce::TextEditor cornerXEditors[4];
        juce::TextEditor cornerYEditors[4];

        juce::Label subdivLabel;
        juce::Label subdivXLabel;
        juce::Label subdivYLabel;
        juce::TextEditor subdivXEditor;
        juce::TextEditor subdivYEditor;
        juce::TextButton resetGridBtn;

        // 4. Edge Blending Controls
        juce::Label edgeBlendLabel;
        juce::Label blendTopLabel, blendBottomLabel, blendLeftLabel, blendRightLabel, blendGammaLabel;
        juce::Slider blendTopSlider, blendBottomSlider, blendLeftSlider, blendRightSlider, blendGammaSlider;

        // Center Canvas Viewport & Editor Area
        class CanvasArea : public juce::Component
        {
        public:
            CanvasArea(EditorComponent& owner) : editor(owner) {}
            void paint(juce::Graphics& g) override;
            void mouseDown(const juce::MouseEvent& e) override;
            void mouseDrag(const juce::MouseEvent& e) override;
            void mouseUp(const juce::MouseEvent& e) override;

        private:
            EditorComponent& editor;
        };

        std::unique_ptr<CanvasArea> canvasArea;

        // Helper to refresh the tree view data
        void rebuildTreeView();

        // Active display windows mapping
        std::vector<std::unique_ptr<OutputDeviceWindow>> activeWindows;
        void syncDisplayWindows();

    public:
        void repaintDeviceWindows();
        void hideDeviceWindows();

        friend class CanvasArea;
        friend class MappingTreeItem;
        friend class OutputDeviceWindow;
        friend class OutputDeviceView;
        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(EditorComponent)
    };

    AdvancedOutputWindow(const juce::String& name, MediaEngine& engine);
    ~AdvancedOutputWindow() override;

    void closeButtonPressed() override
    {
        setVisible(false);
    }

    void repaintDeviceWindows()
    {
        if (editorComp != nullptr)
            editorComp->repaintDeviceWindows();
    }

    void hideDeviceWindows()
    {
        if (editorComp != nullptr)
            editorComp->hideDeviceWindows();
    }

private:
    std::unique_ptr<EditorComponent> editorComp;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(AdvancedOutputWindow)
};

class OutputDeviceView : public juce::Component, public juce::OpenGLRenderer
{
public:
    OutputDeviceView(MediaEngine& engine, OutputScreen& screenRef, AdvancedOutputWindow::EditorComponent& editorRef)
        : mediaEngine(engine), screen(screenRef), editor(editorRef)
    {
        // Attach OpenGL context for GPU-accelerated rendering
        glContext.setRenderer(this);
        glContext.setContinuousRepainting(false);
        glContext.attachTo(*this);
    }

    ~OutputDeviceView() override
    {
        glContext.detach();
    }

    void paint(juce::Graphics&) override
    {
        // Rendering is now handled in renderOpenGL
    }

    void newOpenGLContextCreated() override
    {
        createShaders();
    }

    void renderOpenGL() override;

    void openGLContextClosing() override
    {
        shaderProgram.reset();
        texture.release();
    }

private:
    void createShaders();

    MediaEngine& mediaEngine;
    OutputScreen& screen;
    AdvancedOutputWindow::EditorComponent& editor;
    juce::OpenGLContext glContext;
    
    std::unique_ptr<juce::OpenGLShaderProgram> shaderProgram;
    juce::OpenGLTexture texture;
    
    struct Vertex {
        float position[2];
        float texCoord[2];
        float sliceCoord[2];
    };
};

class OutputDeviceWindow : public juce::DocumentWindow
{
public:
    OutputDeviceWindow(const juce::String& name, MediaEngine& engine, OutputScreen& screenRef, AdvancedOutputWindow::EditorComponent& editorRef)
        : DocumentWindow(name, juce::Colours::black, DocumentWindow::allButtons),
          screen(screenRef)
    {
        setUsingNativeTitleBar(true);
        setResizable(true, false);

        view = std::make_unique<OutputDeviceView>(engine, screenRef, editorRef);
        setContentNonOwned(view.get(), true);
    }

    ~OutputDeviceWindow() override
    {
        clearContentComponent();
        view.reset();
    }

    void closeButtonPressed() override
    {
        setVisible(false);
    }

    std::unique_ptr<OutputDeviceView> view;
    OutputScreen& screen;
};

inline void AdvancedOutputWindow::EditorComponent::repaintDeviceWindows()
{
    for (auto& w : activeWindows)
    {
        if (w != nullptr && w->isVisible() && w->view != nullptr)
            w->view->repaint();
    }
}

inline void AdvancedOutputWindow::EditorComponent::hideDeviceWindows()
{
    for (auto& w : activeWindows)
    {
        if (w != nullptr)
            w->setVisible(false);
    }
}
