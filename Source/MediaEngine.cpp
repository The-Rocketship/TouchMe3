#include "MediaEngine.h"
#include <cmath>

MediaEngine::MediaEngine()
{
    settingsManager.load();
    mappingManager.setupFromSettings(settingsManager.getOscPort(), settingsManager.getOscOutPort(), settingsManager.getEnabledMidiDevices());

    settingsManager.onOscPortChanged = [this](int port) {
        mappingManager.reconnectOscIn(port);
    };

    settingsManager.onOscOutPortChanged = [this](int port) {
        mappingManager.reconnectOscOut(port);
    };

    settingsManager.onMidiDevicesChanged = [this](const juce::StringArray& devices) {
        mappingManager.setMidiDevices(devices);
    };

    // Register FFmpeg reader format
    videoEngine.getFormatManager().registerFormat (std::make_unique<foleys::FFmpegFormat>());

    soloedLayer = -1;
    // Populate the grid with some procedural default templates so the user can play with them immediately!
    juce::String names[4] = { "Cyber Plasma", "Grid Tunnel", "Spectral Neon", "Hypno Spiral" };

    // Initialize vectors
    int initialLayers = 8;
    int initialCols = 12;

    gridClips.resize(initialLayers, std::vector<ClipState>(initialCols));
    activeLayers.resize(initialLayers);
    layerOpacities.resize(initialLayers, 1.0);
    layerBypassed.resize(initialLayers, false);

    for (int l = 0; l < initialLayers; ++l)
    {
        layerOpacities[l] = 1.0;
        layerBypassed[l] = false;

        for (int c = 0; c < initialCols; ++c)
        {
            auto& clip = gridClips[l][c];
            clip.isLoaded = false;
            clip.sourceCol = c;
            // Put some preset procedural clips in columns 0-3
            if (c < 4)
            {
                clip.isLoaded = true;
                clip.isProcedural = true;
                clip.proceduralType = c;
                clip.name = names[c];
                clip.transport.isPlaying = false; // default to NO clips playing
                clip.transport.speed = 1.0;
                clip.transport.loop = true;
                clip.transport.duration = 10.0;
                
                // Varied defaults to make the grid look interesting
                clip.transform.opacity = 1.0;
                clip.transform.scale = 1.0;
            }
            else
            {
                clip.name = "- Empty -";
            }
        }
    }

    mappingManager.onGenericParameterUpdate = [this](const juce::String& path, float val) {
        if (path.startsWith("/layer/")) {
            juce::StringArray parts;
            parts.addTokens(path, "/", "");
            if (parts.size() >= 3) {
                juce::String layerStr = parts[1];
                juce::String paramStr = parts[2];
                
                auto applyToClip = [&](ClipState* target, const juce::String& param, float v) {
                    if (!target) return;
                    juce::Logger::writeToLog("Applying " + param + "=" + juce::String(v) + " to clip");
                    if (param == "opacity") target->transform.opacity = v;
                    else if (param == "speed") target->transport.speed = v * 10.0f;
                    else if (param == "width") target->transform.width = v * 3840.0f;
                    else if (param == "height") target->transform.height = v * 2160.0f;
                    else if (param == "posx") target->transform.posX = (v * 2000.0f) - 1000.0f;
                    else if (param == "posy") target->transform.posY = (v * 2000.0f) - 1000.0f;
                    else if (param == "scale") target->transform.scale = (v * 4.9f) + 0.1f;
                    else if (param == "scalex") target->transform.scaleX = (v * 4.9f) + 0.1f;
                    else if (param == "scaley") target->transform.scaleY = (v * 4.9f) + 0.1f;
                    else if (param == "rotx") target->transform.rotationX = (v * 360.0f) - 180.0f;
                    else if (param == "roty") target->transform.rotationY = (v * 360.0f) - 180.0f;
                    else if (param == "rotz") target->transform.rotationZ = (v * 360.0f) - 180.0f;
                    else if (param == "anchorx") target->transform.anchorX = v;
                    else if (param == "anchory") target->transform.anchorY = v;
                };
                
                if (layerStr == "preview") {
                    juce::Logger::writeToLog("Routing to preview clip");
                    applyToClip(&previewClip, paramStr, val);
                } else {
                    int l = layerStr.getIntValue();
                    juce::Logger::writeToLog("Routing to layer " + juce::String(l));
                    if (l >= 0 && l < activeLayers.size()) {
                        applyToClip(&activeLayers[l], paramStr, val);
                        
                        if (getSelectedLayer() == l) {
                            juce::Logger::writeToLog("Also applying to previewClip because layer is selected");
                            applyToClip(&previewClip, paramStr, val);
                        }
                        
                        int c = activeLayers[l].sourceCol;
                        if (c >= 0 && c < getNumCols()) {
                            applyToClip(&gridClips[l][c], paramStr, val);
                        }
                    }
                }
            }
        }
    };
}

void MediaEngine::update(double deltaTimeSeconds)
{
    // Update active layers
    for (int l = 0; l < getNumLayers(); ++l)
    {
        auto& clip = activeLayers[l];
        if (clip.isLoaded && clip.transport.isPlaying)
        {
            double oldPos = clip.transport.position;
            double delta = deltaTimeSeconds * clip.transport.speed;
            double duration = clip.transport.duration > 0.0 ? clip.transport.duration : 10.0;
            clip.transport.position += delta;
            
            bool looped = false;
            if (clip.transport.position >= duration)
            {
                if (clip.transport.loop)
                {
                    clip.transport.position = std::fmod(clip.transport.position, duration);
                    looped = true;
                }
                else
                {
                    clip.transport.position = duration;
                    clip.transport.isPlaying = false;
                }
            }

            // Sync seek to videoClip decoder if looped or position jumped
            if (clip.videoClip != nullptr)
            {
                double expectedPos = oldPos + delta;
                if (looped || std::abs(clip.transport.position - expectedPos) > 0.5)
                {
                    clip.videoClip->setNextReadPosition (juce::int64 (clip.transport.position * clip.videoClip->getSampleRate()));
                }
            }
        }
    }

    // Update preview clip
    if (previewClip.isLoaded && previewClip.transport.isPlaying)
    {
        double oldPos = previewClip.transport.position;
        double delta = deltaTimeSeconds * previewClip.transport.speed;
        double duration = previewClip.transport.duration > 0.0 ? previewClip.transport.duration : 10.0;
        previewClip.transport.position += delta;
        
        bool looped = false;
        if (previewClip.transport.position >= duration)
        {
            if (previewClip.transport.loop)
            {
                previewClip.transport.position = std::fmod(previewClip.transport.position, duration);
                looped = true;
            }
            else
            {
                previewClip.transport.position = duration;
                previewClip.transport.isPlaying = false;
            }
        }

        // Sync seek to videoClip decoder if looped or position jumped
        if (previewClip.videoClip != nullptr)
        {
            double expectedPos = oldPos + delta;
            if (looped || std::abs(previewClip.transport.position - expectedPos) > 0.5)
            {
                previewClip.videoClip->setNextReadPosition (juce::int64 (previewClip.transport.position * previewClip.videoClip->getSampleRate()));
            }
        }
    }
}

void MediaEngine::updateCompositionFrame()
{
    if (compositionFrame.isNull() || compositionFrame.getWidth() != 1920 || compositionFrame.getHeight() != 1080)
    {
        compositionFrame = juce::Image(juce::Image::ARGB, 1920, 1080, true);
    }

    // Quick check: is there anything to render?
    int soloedL = getSoloedLayer();
    bool anyActive = false;
    for (int l = 0; l < getNumLayers(); ++l)
    {
        if (soloedL != -1 && soloedL != l)
            continue;
        if (!isLayerBypassed(l) && activeLayers[l].isLoaded)
        {
            anyActive = true;
            break;
        }
    }

    compositionFrame.clear(compositionFrame.getBounds(), juce::Colours::transparentBlack);

    if (!anyActive)
        return;

    juce::Graphics cg(compositionFrame);

    for (int l = 0; l < getNumLayers(); ++l)
    {
        if (soloedL != -1 && soloedL != l)
            continue;

        if (!isLayerBypassed(l))
        {
            auto& clip = activeLayers[l];
            if (clip.isLoaded)
            {
                renderClip(cg, clip, 1920.0f, 1080.0f, (float)layerOpacities[l]);
            }
        }
    }
}

void MediaEngine::renderClip(juce::Graphics& g, const ClipState& clip, float width, float height, float masterOpacity, bool drawVideoAsPlaceholder)
{
    if (!clip.isLoaded)
        return;

    g.saveState();

    // Calculate center and pivot
    float cx = width * 0.5f;
    float cy = height * 0.5f;

    // Apply Opacity
    float finalOpacity = clip.transform.opacity.eval(clip.transport.position) * masterOpacity;
    g.setOpacity(finalOpacity);

    // Setup transformation matrix
    // Rotation values
    float rotRad = juce::degreesToRadians(clip.transform.rotationZ.eval(clip.transport.position));
    float finalScaleX = clip.transform.scale.eval(clip.transport.position) * clip.transform.scaleX.eval(clip.transport.position);
    float finalScaleY = clip.transform.scale.eval(clip.transport.position) * clip.transform.scaleY.eval(clip.transport.position);

    // Apply anchor offsets
    float ax = width * clip.transform.anchorX.eval(clip.transport.position);
    float ay = height * clip.transform.anchorY.eval(clip.transport.position);

    // Translate, Rotate, Scale relative to anchor point
    juce::AffineTransform t = juce::AffineTransform::translation(-ax, -ay)
        .scaled(finalScaleX, finalScaleY)
        .rotated(rotRad)
        .translated(ax + clip.transform.posX.eval(clip.transport.position), ay + clip.transform.posY.eval(clip.transport.position));

    g.addTransform(t);

    // Blend mode setup (JUCE does this through its graphics context state or image blending)
    if (clip.transform.blendMode == 1) // Add
    {
        // Custom add blending is handled at context level, fallback to standard drawing
    }

    if (clip.isNodeBased && clip.nodeGraph)
    {
        clip.nodeGraph->renderOutput(g, juce::Rectangle<float>(0.0f, 0.0f, width, height), clip.transport.position);
    }
    else if (clip.isProcedural)
    {
        renderProceduralVisual(g, clip.proceduralType, clip.transport.position, width, height, finalOpacity);
    }
    else if (clip.videoClip != nullptr)
    {
        // Render foleys_video_engine clip directly inside graphics context
        clip.videoClip->render(g, juce::Rectangle<float>(0.0f, 0.0f, width, height), clip.transport.position, 0.0f, 100.0f, {}, finalOpacity);
    }
    else if (clip.image.isValid())
    {
        juce::String ext = clip.file.getFileExtension().toLowerCase();
        bool isVideo = (ext == ".mp4" || ext == ".mov" || ext == ".avi");

        if (isVideo)
        {
            // Render a high-end VJ Video Visualizer HUD instead of a black screen
            // 1. Draw a dark grid background
            g.setColour(juce::Colour(0xff09090b).withAlpha(finalOpacity));
            g.fillRect(0.0f, 0.0f, width, height);

            // Draw a grid pattern
            g.setColour(juce::Colour(0xff00f0a8).withAlpha(0.08f * finalOpacity));
            int gridSize = 40;
            for (int x = 0; x < width; x += gridSize)
                g.drawVerticalLine(x, 0.0f, height);
            for (int y = 0; y < height; y += gridSize)
                g.drawHorizontalLine(y, 0.0f, width);

            // 2. Draw shifting audio waveform simulation in background
            g.setColour(juce::Colour(0xff1080ff).withAlpha(0.2f * finalOpacity));
            juce::Path wavePath;
            float cy = height * 0.5f;
            wavePath.startNewSubPath(0.0f, cy);
            for (int x = 0; x < width; x += 10)
            {
                float angle = (float)x * 0.015f + (float)clip.transport.position * 4.0f;
                float yVal = cy + std::sin(angle) * (height * 0.15f) * std::cos(angle * 0.4f);
                wavePath.lineTo((float)x, yVal);
            }
            g.strokePath(wavePath, juce::PathStrokeType(1.5f));

            // 3. Draw a spinning neon video reel in the center
            float reelSize = std::min(width, height) * 0.35f;
            float rx = (width - reelSize) * 0.5f;
            float ry = (height - reelSize) * 0.5f;
            
            float spinAngle = (float)clip.transport.position * 2.5f * (float)clip.transport.speed;
            
            g.saveState();
            g.addTransform(juce::AffineTransform::rotation(spinAngle, width * 0.5f, height * 0.5f));
            
            // Outer ring
            g.setColour(juce::Colour(0xff10ffd0).withAlpha(0.7f * finalOpacity));
            g.drawEllipse(rx, ry, reelSize, reelSize, 2.5f);
            
            // Spokes of the reel
            int spokes = 5;
            for (int i = 0; i < spokes; ++i)
            {
                float angle = (float)i / spokes * juce::MathConstants<float>::twoPi;
                float sx = width * 0.5f + std::cos(angle) * (reelSize * 0.5f);
                float sy = height * 0.5f + std::sin(angle) * (reelSize * 0.5f);
                g.setColour(juce::Colour(0xff00f0a8).withAlpha(0.4f * finalOpacity));
                g.drawLine(width * 0.5f, height * 0.5f, sx, sy, 1.5f);
            }
            g.restoreState();

            // 4. Draw HUD Overlays
            g.setColour(juce::Colours::white.withAlpha(0.8f * finalOpacity));
            g.setFont(juce::Font(13.0f, juce::Font::bold));
            g.drawText("VIDEO DECODER SIMULATOR", 20, 20, (int)width - 40, 20, juce::Justification::left);
            
            g.setColour(juce::Colour(0xffa1a1aa).withAlpha(finalOpacity));
            g.setFont(10.0f);
            g.drawText("CLIP SOURCE: " + clip.file.getFileName(), 20, 42, (int)width - 40, 16, juce::Justification::left);

            // Timecode calculation
            double pos = clip.transport.position;
            int minutes = (int)(pos / 60.0);
            double seconds = std::fmod(pos, 60.0);
            juce::String timeStr = juce::String::formatted("%02d:%05.2f", minutes, seconds);

            g.setColour(juce::Colour(0xff10ffd0).withAlpha(finalOpacity));
            g.setFont(juce::Font(20.0f, juce::Font::bold));
            g.drawText(timeStr, 20, height - 55, 200, 30, juce::Justification::left);

            g.setColour(juce::Colours::white.withAlpha(0.6f * finalOpacity));
            g.setFont(9.0f);
            int frame = (int)(pos * 60.0);
            g.drawText("FPS: 60.0  |  FRAME: " + juce::String(frame) + "  |  SPEED: " + juce::String(clip.transport.speed) + "x", 
                       20, height - 28, (int)width - 40, 16, juce::Justification::left);

            // Progress bar
            float barH = 4.0f;
            float progress = (float)(pos / clip.transport.duration);
            g.setColour(juce::Colour(0xff27272a).withAlpha(0.4f * finalOpacity));
            g.fillRect(20.0f, height - 62.0f, width - 40.0f, barH);

            g.setColour(juce::Colour(0xff00f0a8).withAlpha(finalOpacity));
            g.fillRect(20.0f, height - 62.0f, (width - 40.0f) * progress, barH);
        }
        else
        {
            // Draw loaded image scaled to fit the destination bounds
            g.drawImage(clip.image, 0, 0, (int)width, (int)height,
                        0, 0, clip.image.getWidth(), clip.image.getHeight());
        }
    }

    g.restoreState();
}

void MediaEngine::renderProceduralVisual(juce::Graphics& g, int type, double time, float w, float h, float opacity)
{
    auto r = juce::Rectangle<float>(0, 0, w, h);

    if (type == 0) // Cyber Plasma
    {
        // Render swirling glowing neon fields with coarse cells for performance
        int step = 16;
        for (int y = 0; y < (int)h; y += step)
        {
            float ny = (float)y / h;
            for (int x = 0; x < (int)w; x += step)
            {
                float nx = (float)x / w;
                
                float val1 = std::sin(nx * 8.0f + (float)time * 2.0f);
                float val2 = std::cos(ny * 6.0f - (float)time * 1.5f);
                float val3 = std::sin((nx + ny) * 5.0f + (float)time);
                float val = (val1 + val2 + val3) / 3.0f;

                float hue = 0.45f + val * 0.2f;
                float sat = 0.9f;
                float bri = 0.4f + val * 0.3f;

                g.setColour(juce::Colour::fromHSV(hue, sat, bri, opacity));
                g.fillRect(x, y, step, step);
            }
        }
    }
    else if (type == 1) // Grid Tunnel
    {
        // Dark background with opacity
        g.setColour(juce::Colour(0xff09090b).withAlpha(opacity));
        g.fillRect(0.0f, 0.0f, w, h);

        // Draw expanding 3D grid lines
        float cx = w * 0.5f;
        float cy = h * 0.5f;

        int numRings = 12;
        float speedFactor = 2.0f;
        float phase = std::fmod((float)time * speedFactor, 1.0f);

        for (int i = 0; i < numRings; ++i)
        {
            float progress = (float)i / numRings + phase / numRings;
            // Exponential expansion to simulate 3D tunnel depth
            float radius = std::pow(progress, 2.5f) * (w * 0.8f);
            
            float ringOpacity = std::sin(progress * juce::MathConstants<float>::pi);
            g.setColour(juce::Colour(0xff00f0a8).withAlpha(ringOpacity * 0.6f * opacity));
            g.drawEllipse(cx - radius, cy - radius, radius * 2.0f, radius * 2.0f, 1.5f);
        }

        // Draw spoke lines
        int numSpokes = 8;
        for (int i = 0; i < numSpokes; ++i)
        {
            float angle = (float)i / numSpokes * juce::MathConstants<float>::twoPi + (float)time * 0.1f;
            float sx = cx + std::cos(angle) * (w * 0.8f);
            float sy = cy + std::sin(angle) * (w * 0.8f);
            g.setColour(juce::Colour(0xff10ffd0).withAlpha(0.2f * opacity));
            g.drawLine(cx, cy, sx, sy, 1.0f);
        }
    }
    else if (type == 2) // Spectral Neon
    {
        g.setColour(juce::Colour(0xff050508).withAlpha(opacity));
        g.fillRect(0.0f, 0.0f, w, h);

        // Draw simulated bouncing spectral wave lines
        float cx = w * 0.5f;
        float cy = h * 0.5f;

        g.setColour(juce::Colour(0xff1080ff).withAlpha(0.4f * opacity)); // Blue neon glow
        
        // Draw grid floor simulation
        for (float y = cy; y < h; y += 15.0f)
        {
            float progress = (y - cy) / (h - cy);
            g.setColour(juce::Colour(0xff1080ff).withAlpha((1.0f - progress) * 0.2f * opacity));
            g.drawLine(0.0f, y, w, y, 1.0f);
        }

        // Draw vertical columns
        int columns = 32;
        for (int i = 0; i < columns; ++i)
        {
            float nx = (float)i / (columns - 1);
            float x = nx * w;
            
            // Calculate a wave amplitude
            float ampVal = std::sin(nx * 12.0f + (float)time * 4.0f) * 0.4f +
                           std::cos(nx * 24.0f - (float)time * 6.5f) * 0.2f;
            ampVal = std::abs(ampVal) * (h * 0.4f);

            // Draw glowing bar
            juce::Colour barCol = juce::Colour::fromHSV(0.7f + nx * 0.3f, 0.9f, 0.8f, 1.0f);
            g.setColour(barCol.withAlpha(0.7f * opacity));
            g.fillRect(x - 3.0f, cy - ampVal, 6.0f, ampVal * 2.0f);
        }
    }
    else if (type == 3) // Hypno Spiral
    {
        g.setColour(juce::Colour(0xff100b18).withAlpha(opacity));
        g.fillRect(0.0f, 0.0f, w, h);

        float cx = w * 0.5f;
        float cy = h * 0.5f;
        int maxPoints = 250;
        
        juce::Path p;
        for (int i = 0; i < maxPoints; ++i)
        {
            float theta = (float)i * 0.15f + (float)time * 2.0f;
            float r = (float)i * (w * 0.0025f);
            float px = cx + std::cos(theta) * r;
            float py = cy + std::sin(theta) * r;
            
            if (i == 0)
                p.startNewSubPath(px, py);
            else
                p.lineTo(px, py);
        }

        g.setColour(juce::Colour(0xffff007f).withAlpha(opacity)); // Pink hot neon
        g.strokePath(p, juce::PathStrokeType(3.0f, juce::PathStrokeType::mitered, juce::PathStrokeType::rounded));
    }
}

ClipState& MediaEngine::getClipInGrid(int layerIdx, int colIdx)
{
    return gridClips[layerIdx][colIdx];
}

void MediaEngine::triggerClip(int layerIdx, int colIdx)
{
    if (layerIdx < 0 || layerIdx >= getNumLayers() || colIdx < 0 || colIdx >= getNumCols())
        return;

    auto& activeClip = activeLayers[layerIdx];
    auto& gridClip = gridClips[layerIdx][colIdx];

    if (activeClip.isLoaded && activeClip.sourceCol == colIdx)
    {
        // Toggle off if already playing
        activeClip.isLoaded = false;
        activeClip.transport.isPlaying = false;
        activeClip.videoClip.reset();
    }
    else
    {
        // Copy states to active layer playback slot
        activeClip = gridClip;
        activeClip.transport.isPlaying = true;
        activeClip.transport.position = 0.0;

        if (gridClip.isLoaded && !gridClip.isProcedural)
        {
            juce::String ext = gridClip.file.getFileExtension().toLowerCase();
            if (ext == ".mp4" || ext == ".mov" || ext == ".avi")
            {
                activeClip.videoClip = videoEngine.createClipFromFile(juce::URL(gridClip.file));
                if (activeClip.videoClip != nullptr)
                {
                    activeClip.videoClip->setAspectType(foleys::Aspect::ZoomScale);
                    activeClip.videoClip->prepareToPlay(512, 44100.0);
                    activeClip.transport.duration = activeClip.videoClip->getLengthInSeconds();
                }
            }
        }
    }
}

ClipState& MediaEngine::getActiveLayerClip(int layerIdx)
{
    return activeLayers[layerIdx];
}

void MediaEngine::removeLayer(int layerIdx)
{
    if (layerIdx < 0 || layerIdx >= getNumLayers() || getNumLayers() <= 1)
        return; // Don't delete if it's the last layer
        
    gridClips.erase(gridClips.begin() + layerIdx);
    activeLayers.erase(activeLayers.begin() + layerIdx);
    layerOpacities.erase(layerOpacities.begin() + layerIdx);
    layerBypassed.erase(layerBypassed.begin() + layerIdx);
    
    if (selectedLayer == layerIdx) selectedLayer = -1;
    else if (selectedLayer > layerIdx) selectedLayer--;
    
    if (soloedLayer == layerIdx) soloedLayer = -1;
    else if (soloedLayer > layerIdx) soloedLayer--;
}

void MediaEngine::removeColumn(int colIdx)
{
    if (colIdx < 0 || colIdx >= getNumCols() || getNumCols() <= 1)
        return; // Don't delete if it's the last column
        
    for (auto& row : gridClips)
    {
        row.erase(row.begin() + colIdx);
    }
    
    // Update sourceCol for active layers if they shifted
    for (auto& active : activeLayers)
    {
        if (active.sourceCol == colIdx) {
            active.transport.isPlaying = false;
            active.isLoaded = false;
            active.sourceCol = -1;
        } else if (active.sourceCol > colIdx) {
            active.sourceCol--;
        }
    }
    
    // Update sourceCol for grid clips
    for (auto& row : gridClips)
    {
        for (auto& clip : row)
        {
            if (clip.sourceCol > colIdx)
                clip.sourceCol--;
        }
    }
    
    if (selectedCol == colIdx) selectedCol = -1;
    else if (selectedCol > colIdx) selectedCol--;
}

ClipState& MediaEngine::getPreviewClip()
{
    return previewClip;
}

void MediaEngine::previewClipInGrid(int layerIdx, int colIdx)
{
    if (layerIdx < 0 || layerIdx >= getNumLayers() || colIdx < 0 || colIdx >= getNumCols())
        return;

    selectedLayer = layerIdx;
    selectedCol = colIdx;
    
    auto& gridClip = gridClips[layerIdx][colIdx];
    
    // Copy to preview clip
    previewClip = gridClip;
    previewClip.transport.isPlaying = true;
    previewClip.transport.position = 0.0;

    if (gridClip.isLoaded && !gridClip.isProcedural)
    {
        juce::String ext = gridClip.file.getFileExtension().toLowerCase();
        if (ext == ".mp4" || ext == ".mov" || ext == ".avi")
        {
            previewClip.videoClip = videoEngine.createClipFromFile(juce::URL(gridClip.file));
            if (previewClip.videoClip != nullptr)
            {
                previewClip.videoClip->setAspectType(foleys::Aspect::ZoomScale);
                previewClip.videoClip->prepareToPlay(512, 44100.0);
                previewClip.transport.duration = previewClip.videoClip->getLengthInSeconds();
            }
        }
    }
}
