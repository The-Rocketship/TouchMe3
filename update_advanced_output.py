import re

with open('Source/AdvancedOutputWindow.h', 'r') as f:
    content = f.read()

# 1. Add Edge Blending to WarpSlice
warp_slice_addition = """    std::vector<juce::Point<float>> gridPoints;

    // Edge Blending
    float blendTop = 0.0f;
    float blendBottom = 0.0f;
    float blendLeft = 0.0f;
    float blendRight = 0.0f;
    float blendGamma = 1.8f;"""
content = content.replace("    std::vector<juce::Point<float>> gridPoints;", warp_slice_addition)

# 2. Add UI controls to EditorComponent
ui_controls = """        juce::TextEditor subdivYEditor;
        juce::TextButton resetGridBtn;

        // 4. Edge Blending Controls
        juce::Label edgeBlendLabel;
        juce::Label blendTopLabel, blendBottomLabel, blendLeftLabel, blendRightLabel, blendGammaLabel;
        juce::Slider blendTopSlider, blendBottomSlider, blendLeftSlider, blendRightSlider, blendGammaSlider;"""
content = content.replace("        juce::TextEditor subdivYEditor;\n        juce::TextButton resetGridBtn;", ui_controls)

# 3. Add sliceCoord to Vertex
vertex_update = """    struct Vertex {
        float position[2];
        float texCoord[2];
        float sliceCoord[2];
    };"""
content = re.sub(r'    struct Vertex \{\s*float position\[2\];\s*float texCoord\[2\];\s*\};', vertex_update, content)

with open('Source/AdvancedOutputWindow.h', 'w') as f:
    f.write(content)
