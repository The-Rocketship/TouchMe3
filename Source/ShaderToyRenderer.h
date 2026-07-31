#pragma once
#include <JuceHeader.h>
#include <mutex>
#include <condition_variable>

//==============================================================================
// ShaderToyGLRenderer
// A hidden JUCE component with its own OpenGLContext used to compile and run
// ShaderToy-compatible GLSL fragment shaders into a juce::Image via FBO.
//==============================================================================
class ShaderToyGLRenderer : public juce::Component,
                             public juce::OpenGLRenderer
{
public:
    ShaderToyGLRenderer()
    {
        setOpaque(true);
        // Position it offscreen so it's not visible, but keep it 'visible' to the OS
        setBounds(-1000, -1000, 10, 10);
        addToDesktop(juce::ComponentPeer::windowIsTemporary);
        setVisible(true);

        glContext.setRenderer(this);
        glContext.setContinuousRepainting(false);
        glContext.attachTo(*this);
    }

    ~ShaderToyGLRenderer() override
    {
        glContext.detach();
    }

    //==========================================================================
    // Public API
    //==========================================================================

    /** Replace the shader source and mark for recompile. */
    void setShaderSource(const juce::String& source)
    {
        std::lock_guard<std::mutex> lk(mtx);
        pendingSource = source;
        shaderDirty   = true;
        lastError.clear();
    }

    juce::String getLastError()
    {
        std::lock_guard<std::mutex> lk(mtx);
        return lastError;
    }

    /**
     * Render one frame synchronously.
     * Blocks until the GL thread has finished and pixel data is in @target.
     * @param channel0   Optional input image bound to iChannel0 (may be nullptr).
     * Returns true on success.
     */
    bool renderFrame(juce::Image& target, double time, double timeDelta, int frame,
                     const juce::Image* channel0 = nullptr)
    {
        const int W = target.getWidth();
        const int H = target.getHeight();
        if (W <= 0 || H <= 0) return false;

        // Pack request
        {
            std::lock_guard<std::mutex> lk(mtx);
            req.target     = &target;
            req.time       = (float)time;
            req.timeDelta  = (float)timeDelta;
            req.frame      = frame;
            req.channel0   = channel0;
            req.width      = W;
            req.height     = H;
            renderDone     = false;
        }

        // Run on the GL thread (blocks until done)
        glContext.executeOnGLThread([this](juce::OpenGLContext&) { doGLRender(); }, true);

        std::lock_guard<std::mutex> lk(mtx);
        if (!renderDone)
        {
            juce::Graphics g(target);
            g.fillAll(juce::Colours::red);
            g.setColour(juce::Colours::white);
            g.drawLine(0, 0, (float)W, (float)H, 5.0f);
            g.drawLine(0, (float)H, (float)W, 0, 5.0f);
            lastError = "GL Context failed to execute.";
        }
        return renderDone && lastError.isEmpty();
    }

    // OpenGLRenderer callbacks —————————————————————————————————————————————
    void newOpenGLContextCreated() override {}
    void renderOpenGL() override {}   // We don't use continuous repaint
    void openGLContextClosing() override
    {
        destroyFBO();
        shaderProgram.reset();
    }

private:
    //==========================================================================
    // GL-thread work
    //==========================================================================
    void doGLRender()
    {
        using namespace juce::gl;
        
        static bool hasLoggedGLStart = false;
        if (!hasLoggedGLStart)
        {
            juce::Logger::writeToLog("ShaderToyGLRenderer: GL Thread started successfully.");
            hasLoggedGLStart = true;
        }

        // Recompile shader if needed
        {
            std::lock_guard<std::mutex> lk(mtx);
            if (shaderDirty)
            {
                compileShaderOnGLThread();
                shaderDirty = false;
            }
        }

        if (shaderProgram == nullptr) return;

        const int W = req.width;
        const int H = req.height;

        // Resize FBO
        if (fboWidth != W || fboHeight != H)
        {
            destroyFBO();
            createFBO(W, H);
        }
        if (fbo == 0) return;

        // ---------- Render into FBO ----------
        glBindFramebuffer(GL_FRAMEBUFFER, fbo);
        glViewport(0, 0, W, H);
        glClearColor(0.f, 0.f, 0.f, 1.f);
        glClear(GL_COLOR_BUFFER_BIT);

        shaderProgram->use();

        setUniform3f("iResolution", (float)W, (float)H, 1.f);
        setUniform1f("iTime",       req.time);
        setUniform1f("iTimeDelta",  req.timeDelta);
        setUniform1i("iFrame",      req.frame);
        setUniform4f("iMouse",      0.f, 0.f, 0.f, 0.f);

        // Channel0 texture
        glActiveTexture(GL_TEXTURE0);
        if (req.channel0 != nullptr && req.channel0->isValid())
        {
            channel0Tex.loadImage(*req.channel0);
            channel0Tex.bind();
        }
        else
        {
            // Bind a 1x1 black texture as fallback
            if (blackTex == 0)
            {
                glGenTextures(1, &blackTex);
                glBindTexture(GL_TEXTURE_2D, blackTex);
                uint8_t black[4] = {0,0,0,255};
                glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 1, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, black);
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
            }
            glBindTexture(GL_TEXTURE_2D, blackTex);
        }
        setUniform1i("iChannel0", 0);

        // Full-screen triangle strip
        const float verts[] = { -1.f,-1.f, 1.f,-1.f, -1.f,1.f, 1.f,1.f };
        if (posAttr >= 0)
        {
            glVertexAttribPointer((GLuint)posAttr, 2, GL_FLOAT, GL_FALSE, 0, verts);
            glEnableVertexAttribArray((GLuint)posAttr);
            glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
            glDisableVertexAttribArray((GLuint)posAttr);
        }

        // ---------- Read pixels back ----------
        std::vector<uint8_t> pixels((size_t)(W * H * 4));
        glReadPixels(0, 0, W, H, GL_RGBA, GL_UNSIGNED_BYTE, pixels.data());
        glBindFramebuffer(GL_FRAMEBUFFER, 0);

        // Write into target (flip Y: GL is bottom-left origin)
        if (req.target != nullptr)
        {
            juce::Image::BitmapData bd(*req.target, juce::Image::BitmapData::writeOnly);
            for (int row = 0; row < H; ++row)
            {
                const uint8_t* src = pixels.data() + (size_t)((H - 1 - row) * W * 4);
                for (int x = 0; x < W; ++x)
                {
                    // Shadertoy shaders often leave the alpha channel at 0, making the texture fully transparent. 
                    // We force alpha to 255 here so the result is always fully opaque.
                    bd.setPixelColour(x, row, juce::Colour(src[x*4], src[x*4+1], src[x*4+2], (uint8_t)255));
                }
            }
        }

        std::lock_guard<std::mutex> lk(mtx);
        renderDone = true;
    }

    //==========================================================================
    void compileShaderOnGLThread()
    {
        static const char* vertSrc = R"(
            attribute vec2 position;
            void main() { gl_Position = vec4(position, 0.0, 1.0); }
        )";

        // ShaderToy-compatible preamble
        static const char* preamble = R"(
            #ifdef GL_ES
            precision highp float;
            #endif
            uniform vec3  iResolution;
            uniform float iTime;
            uniform float iTimeDelta;
            uniform int   iFrame;
            uniform vec4  iMouse;
            uniform sampler2D iChannel0;
            uniform sampler2D iChannel1;
            uniform sampler2D iChannel2;
            uniform sampler2D iChannel3;
        )";

        static const char* suffix = R"(
            void main() {
                vec4 fragColor;
                mainImage(fragColor, gl_FragCoord.xy);
                gl_FragColor = fragColor;
            }
        )";

        juce::String fragSrc = juce::String(preamble)
                             + "\n" + pendingSource
                             + "\n" + suffix;

        auto prog = std::make_unique<juce::OpenGLShaderProgram>(glContext);
        if (prog->addVertexShader(juce::String(vertSrc))
            && prog->addFragmentShader(fragSrc)
            && prog->link())
        {
            shaderProgram = std::move(prog);
            posAttr = glContext.extensions.glGetAttribLocation(shaderProgram->getProgramID(), "position");
            lastError.clear();
        }
        else
        {
            lastError = prog->getLastError();
            shaderProgram.reset();
            posAttr = -1;
        }
    }

    //==========================================================================
    void createFBO(int w, int h)
    {
        using namespace juce::gl;
        glContext.extensions.glGenFramebuffers(1, &fbo);
        glContext.extensions.glBindFramebuffer(GL_FRAMEBUFFER, fbo);

        glGenTextures(1, &colorTex);
        glBindTexture(GL_TEXTURE_2D, colorTex);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        glContext.extensions.glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, colorTex, 0);

        glContext.extensions.glBindFramebuffer(GL_FRAMEBUFFER, 0);
        fboWidth = w; fboHeight = h;
    }

    void destroyFBO()
    {
        if (fbo)      { glContext.extensions.glDeleteFramebuffers(1, &fbo); fbo = 0; }
        if (colorTex) { juce::gl::glDeleteTextures(1, &colorTex); colorTex = 0; }
        if (blackTex) { juce::gl::glDeleteTextures(1, &blackTex); blackTex = 0; }
        fboWidth = fboHeight = 0;
    }

    //==========================================================================
    // Uniform helpers
    //==========================================================================
    void setUniform1f(const char* name, float v) const
    {
        if (shaderProgram) {
            auto loc = glContext.extensions.glGetUniformLocation(shaderProgram->getProgramID(), name);
            if (loc >= 0) glContext.extensions.glUniform1f(loc, v);
        }
    }
    void setUniform1i(const char* name, int v) const
    {
        if (shaderProgram) {
            auto loc = glContext.extensions.glGetUniformLocation(shaderProgram->getProgramID(), name);
            if (loc >= 0) glContext.extensions.glUniform1i(loc, v);
        }
    }
    void setUniform3f(const char* name, float x, float y, float z) const
    {
        if (shaderProgram) {
            auto loc = glContext.extensions.glGetUniformLocation(shaderProgram->getProgramID(), name);
            if (loc >= 0) glContext.extensions.glUniform3f(loc, x, y, z);
        }
    }
    void setUniform4f(const char* name, float x, float y, float z, float w) const
    {
        if (shaderProgram) {
            auto loc = glContext.extensions.glGetUniformLocation(shaderProgram->getProgramID(), name);
            if (loc >= 0) glContext.extensions.glUniform4f(loc, x, y, z, w);
        }
    }

    //==========================================================================
    // State
    //==========================================================================
    juce::OpenGLContext glContext;
    std::unique_ptr<juce::OpenGLShaderProgram> shaderProgram;
    juce::OpenGLTexture channel0Tex;

    GLuint fbo = 0, colorTex = 0, blackTex = 0;
    int fboWidth = 0, fboHeight = 0;
    int posAttr = -1;

    mutable std::mutex mtx;

    // Shader source
    juce::String pendingSource;
    juce::String lastError;
    bool shaderDirty = true;

    // Render request
    struct RenderReq {
        juce::Image* target = nullptr;
        const juce::Image* channel0 = nullptr;
        float time = 0.f, timeDelta = 0.f;
        int frame = 0, width = 0, height = 0;
    } req;
    bool renderDone = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ShaderToyGLRenderer)
};
