#include "NodeGraph.h"
#include "ShaderToyRenderer.h"

BaseNode::BaseNode(int id, const juce::String& name, int x, int y)
    : id(id), name(name), x(x), y(y)
{
}

SolidColourNode::SolidColourNode(int id, int x, int y)
    : BaseNode(id, "Solid Colour", x, y)
{
    // Generate a random bright color for now
    juce::Colour c = juce::Colour(
        (juce::uint8)juce::Random::getSystemRandom().nextInt(256),
        (juce::uint8)juce::Random::getSystemRandom().nextInt(256),
        (juce::uint8)juce::Random::getSystemRandom().nextInt(256)
    ).withAlpha(1.0f);
    
    colour = c;
}

void SolidColourNode::process(juce::Image& target, double time)
{
    juce::Graphics g(target);
    g.fillAll(colour.eval(time));
}

OutputNode::OutputNode(int id, int x, int y)
    : BaseNode(id, "Output", x, y)
{
}

void OutputNode::process(juce::Image& target, double time)
{
}

LineNode::LineNode(int id, int x, int y)
    : BaseNode(id, "Line Generator", x, y)
{
}

void LineNode::process(juce::Image& target, double time)
{
    juce::Graphics g(target);
    g.setColour(colour.eval(time));
    g.drawLine(startX.eval(time) * target.getWidth(), startY.eval(time) * target.getHeight(),
               endX.eval(time) * target.getWidth(), endY.eval(time) * target.getHeight(), thickness.eval(time));
}

NoiseNode::NoiseNode(int id, int x, int y) : BaseNode(id, "Noise Generator", x, y) {}

// ---- noise helpers -------------------------------------------------------
namespace NoiseHelpers
{
    // Integer hash
    static inline uint32_t hash(uint32_t x)
    {
        x = ((x >> 16) ^ x) * 0x45d9f3bu;
        x = ((x >> 16) ^ x) * 0x45d9f3bu;
        x = (x >> 16) ^ x;
        return x;
    }
    static inline uint32_t hash2(uint32_t x, uint32_t y) { return hash(x ^ hash(y)); }
    static inline uint32_t hash3(uint32_t x, uint32_t y, uint32_t z) { return hash(x ^ hash(y ^ hash(z))); }

    static inline float hashf(uint32_t h) { return (float)(h & 0xFFFF) / 65536.0f; }

    // Smoothstep
    static inline float fade(float t) { return t * t * t * (t * (t * 6.0f - 15.0f) + 10.0f); }
    static inline float lerp(float a, float b, float t) { return a + t * (b - a); }

    // Gradient for Perlin
    static inline float grad2(uint32_t h, float x, float y)
    {
        uint32_t hh = h & 3;
        float u = hh < 2 ? x : y;
        float v = hh < 2 ? y : x;
        return ((hh & 1) ? -u : u) + ((hh & 2) ? -v : v);
    }
    static inline float grad3(uint32_t h, float x, float y, float z)
    {
        uint32_t hh = h & 15;
        float u = hh < 8 ? x : y;
        float v = hh < 4 ? y : (hh == 12 || hh == 14 ? x : z);
        return ((hh & 1) ? -u : u) + ((hh & 2) ? -v : v);
    }

    // Value noise 2D
    static float valueNoise2D(float x, float y, uint32_t seed)
    {
        int xi = (int)std::floor(x); float xf = x - xi;
        int yi = (int)std::floor(y); float yf = y - yi;
        float tx = fade(xf), ty = fade(yf);
        float v00 = hashf(hash2((uint32_t)(xi + seed), (uint32_t)(yi)));
        float v10 = hashf(hash2((uint32_t)(xi+1 + seed), (uint32_t)(yi)));
        float v01 = hashf(hash2((uint32_t)(xi + seed), (uint32_t)(yi+1)));
        float v11 = hashf(hash2((uint32_t)(xi+1 + seed), (uint32_t)(yi+1)));
        return lerp(lerp(v00, v10, tx), lerp(v01, v11, tx), ty);
    }

    // Value noise 3D
    static float valueNoise3D(float x, float y, float z, uint32_t seed)
    {
        int xi = (int)std::floor(x); float xf = x - xi;
        int yi = (int)std::floor(y); float yf = y - yi;
        int zi = (int)std::floor(z); float zf = z - zi;
        float tx = fade(xf), ty = fade(yf), tz = fade(zf);
        float v000 = hashf(hash3((uint32_t)(xi + seed),(uint32_t)yi,(uint32_t)zi));
        float v100 = hashf(hash3((uint32_t)(xi+1 + seed),(uint32_t)yi,(uint32_t)zi));
        float v010 = hashf(hash3((uint32_t)(xi + seed),(uint32_t)(yi+1),(uint32_t)zi));
        float v110 = hashf(hash3((uint32_t)(xi+1 + seed),(uint32_t)(yi+1),(uint32_t)zi));
        float v001 = hashf(hash3((uint32_t)(xi + seed),(uint32_t)yi,(uint32_t)(zi+1)));
        float v101 = hashf(hash3((uint32_t)(xi+1 + seed),(uint32_t)yi,(uint32_t)(zi+1)));
        float v011 = hashf(hash3((uint32_t)(xi + seed),(uint32_t)(yi+1),(uint32_t)(zi+1)));
        float v111 = hashf(hash3((uint32_t)(xi+1 + seed),(uint32_t)(yi+1),(uint32_t)(zi+1)));
        float x0 = lerp(lerp(v000,v100,tx),lerp(v010,v110,tx),ty);
        float x1 = lerp(lerp(v001,v101,tx),lerp(v011,v111,tx),ty);
        return lerp(x0, x1, tz);
    }

    // Perlin gradient noise 2D
    static float perlin2D(float x, float y, uint32_t seed)
    {
        int xi = (int)std::floor(x); float xf = x - xi;
        int yi = (int)std::floor(y); float yf = y - yi;
        float u = fade(xf), v = fade(yf);
        float n00 = grad2(hash2((uint32_t)(xi+seed),(uint32_t)(yi)), xf, yf);
        float n10 = grad2(hash2((uint32_t)(xi+1+seed),(uint32_t)(yi)), xf-1.0f, yf);
        float n01 = grad2(hash2((uint32_t)(xi+seed),(uint32_t)(yi+1)), xf, yf-1.0f);
        float n11 = grad2(hash2((uint32_t)(xi+1+seed),(uint32_t)(yi+1)), xf-1.0f, yf-1.0f);
        return lerp(lerp(n00,n10,u),lerp(n01,n11,u),v) * 0.5f + 0.5f;
    }

    // Perlin gradient noise 3D
    static float perlin3D(float x, float y, float z, uint32_t seed)
    {
        int xi = (int)std::floor(x); float xf = x - xi;
        int yi = (int)std::floor(y); float yf = y - yi;
        int zi = (int)std::floor(z); float zf = z - zi;
        float u = fade(xf), v = fade(yf), w = fade(zf);
        float n000 = grad3(hash3((uint32_t)(xi+seed),(uint32_t)yi,(uint32_t)zi), xf,yf,zf);
        float n100 = grad3(hash3((uint32_t)(xi+1+seed),(uint32_t)yi,(uint32_t)zi), xf-1,yf,zf);
        float n010 = grad3(hash3((uint32_t)(xi+seed),(uint32_t)(yi+1),(uint32_t)zi), xf,yf-1,zf);
        float n110 = grad3(hash3((uint32_t)(xi+1+seed),(uint32_t)(yi+1),(uint32_t)zi), xf-1,yf-1,zf);
        float n001 = grad3(hash3((uint32_t)(xi+seed),(uint32_t)yi,(uint32_t)(zi+1)), xf,yf,zf-1);
        float n101 = grad3(hash3((uint32_t)(xi+1+seed),(uint32_t)yi,(uint32_t)(zi+1)), xf-1,yf,zf-1);
        float n011 = grad3(hash3((uint32_t)(xi+seed),(uint32_t)(yi+1),(uint32_t)(zi+1)), xf,yf-1,zf-1);
        float n111 = grad3(hash3((uint32_t)(xi+1+seed),(uint32_t)(yi+1),(uint32_t)(zi+1)), xf-1,yf-1,zf-1);
        float x0 = lerp(lerp(n000,n100,u),lerp(n010,n110,u),v);
        float x1 = lerp(lerp(n001,n101,u),lerp(n011,n111,u),v);
        return lerp(x0, x1, w) * 0.5f + 0.5f;
    }

    // Sparse (cellular/Worley-ish) noise
    static float sparseNoise(float x, float y, uint32_t seed)
    {
        int xi = (int)std::floor(x);
        int yi = (int)std::floor(y);
        float minD = 1e9f;
        for (int dy = -1; dy <= 1; ++dy)
        for (int dx = -1; dx <= 1; ++dx)
        {
            uint32_t h = hash2((uint32_t)(xi+dx+seed),(uint32_t)(yi+dy));
            float cx = (xi+dx) + hashf(h) ;
            float cy = (yi+dy) + hashf(hash(h));
            float d = std::sqrt((x-cx)*(x-cx)+(y-cy)*(y-cy));
            minD = std::min(minD, d);
        }
        return juce::jlimit(0.0f, 1.0f, minD);
    }

    // Alligator (difference of sparse)
    static float alligatorNoise(float x, float y, uint32_t seed)
    {
        int xi = (int)std::floor(x);
        int yi = (int)std::floor(y);
        float d1 = 1e9f, d2 = 1e9f;
        for (int dy = -1; dy <= 1; ++dy)
        for (int dx = -1; dx <= 1; ++dx)
        {
            uint32_t h = hash2((uint32_t)(xi+dx+seed),(uint32_t)(yi+dy));
            float cx = (xi+dx) + hashf(h);
            float cy = (yi+dy) + hashf(hash(h));
            float d = std::sqrt((x-cx)*(x-cx)+(y-cy)*(y-cy));
            if (d < d1) { d2 = d1; d1 = d; }
            else if (d < d2) { d2 = d; }
        }
        return juce::jlimit(0.0f, 1.0f, d2 - d1);
    }

    // fBm wrapper
    typedef std::function<float(float, float, float, uint32_t)> Noise3DFn;
    static float fbm(Noise3DFn fn, float x, float y, float z, uint32_t seed,
                     int octaves, float lacunarity, float gain)
    {
        float val = 0.0f, amp = 1.0f, maxAmp = 0.0f;
        for (int i = 0; i < octaves; ++i)
        {
            val += fn(x, y, z, seed) * amp;
            maxAmp += amp;
            amp *= gain;
            x *= lacunarity; y *= lacunarity; z *= lacunarity;
        }
        return maxAmp > 0 ? val / maxAmp : 0.0f;
    }
}

void NoiseNode::process(juce::Image& target, double time)
{
    const int W = target.getWidth();
    const int H = target.getHeight();

    float per    = period.baseValue;      if (per <= 0.0f) per = 0.001f;
    float amp    = amplitude.baseValue;
    float off    = offset.baseValue;
    float exp_   = exponent.baseValue;
    int   oct    = juce::jlimit(1, 16, (int)harmonics.baseValue);
    float lac    = harmonicSpread.baseValue;  if (lac <= 0.0f) lac = 0.001f;
    float gain   = harmonicGain.baseValue;
    uint32_t sd  = (uint32_t)std::abs(seed.baseValue);
    float tz     = (float)time;

    // ---- dirty cache: if nothing changed AND time hasn't moved, blit cache ----
    if (!dirty
        && time == lastRenderedTime
        && cachedImage.isValid()
        && cachedImage.getWidth()  == W
        && cachedImage.getHeight() == H)
    {
        juce::Graphics g(target);
        g.drawImageAt(cachedImage, 0, 0);
        return;
    }
    dirty = false;
    lastRenderedTime = time;

    // ---- allocate / reuse the cached image ----
    if (!cachedImage.isValid() || cachedImage.getWidth() != W || cachedImage.getHeight() != H)
        cachedImage = juce::Image(juce::Image::RGB, W, H, false);

    // ---- parallel row-band rendering ----
    const int numThreads = juce::jlimit(1, 8, (int)std::thread::hardware_concurrency());
    const int rowsPerThread = (H + numThreads - 1) / numThreads;

    auto renderRows = [&](int startRow, int endRow)
    {
        juce::Image::BitmapData bd(cachedImage, 0, startRow, W, endRow - startRow,
                                    juce::Image::BitmapData::writeOnly);
        for (int y = startRow; y < endRow; ++y)
        {
            auto* row = bd.getLinePointer(y - startRow);
            for (int x = 0; x < W; ++x)
            {
                float nx = (float)x / W / per;
                float ny = (float)y / H / per;
                float v  = 0.0f;

                switch (noiseType)
                {
                    case NoiseType::Simplex2D:
                    case NoiseType::Simplex3D:
                    {
                        auto fn = [](float px, float py, float pz, uint32_t s) {
                            return NoiseHelpers::valueNoise3D(px, py, pz, s);
                        };
                        v = NoiseHelpers::fbm(fn, nx, ny, tz, sd, oct, lac, gain);
                        break;
                    }
                    case NoiseType::Simplex4D:
                    {
                        auto fn = [](float px, float py, float pz, uint32_t s) {
                            return NoiseHelpers::valueNoise3D(px, py, pz, s) * 0.5f
                                 + NoiseHelpers::valueNoise3D(px+31.7f, py+47.3f, pz, s+1) * 0.5f;
                        };
                        v = NoiseHelpers::fbm(fn, nx, ny, tz, sd, oct, lac, gain);
                        break;
                    }
                    case NoiseType::Perlin2D:
                    {
                        float pv = 0.0f, a = 1.0f, mx = 0.0f, fx = nx, fy = ny;
                        for (int i = 0; i < oct; ++i) {
                            pv += NoiseHelpers::perlin2D(fx, fy, sd) * a;
                            mx += a; a *= gain; fx *= lac; fy *= lac;
                        }
                        v = mx > 0 ? pv / mx : 0.0f;
                        break;
                    }
                    case NoiseType::Perlin3D:
                    case NoiseType::Perlin4D:
                    {
                        auto fn = [](float px, float py, float pz, uint32_t s) {
                            return NoiseHelpers::perlin3D(px, py, pz, s);
                        };
                        v = NoiseHelpers::fbm(fn, nx, ny, tz, sd, oct, lac, gain);
                        break;
                    }
                    case NoiseType::RandomGPU:
                    case NoiseType::Random:
                    {
                        v = NoiseHelpers::hashf(NoiseHelpers::hash2(
                            (uint32_t)(nx * 1000) + sd,
                            (uint32_t)(ny * 1000)));
                        break;
                    }
                    case NoiseType::Sparse:
                    {
                        float pv = 0.0f, a = 1.0f, mx = 0.0f, fx = nx, fy = ny;
                        for (int i = 0; i < oct; ++i) {
                            pv += NoiseHelpers::sparseNoise(fx, fy, sd) * a;
                            mx += a; a *= gain; fx *= lac; fy *= lac;
                        }
                        v = mx > 0 ? pv / mx : 0.0f;
                        break;
                    }
                    case NoiseType::Hermite:
                    {
                        auto fn = [](float px, float py, float pz, uint32_t s) {
                            return NoiseHelpers::valueNoise3D(px, py, pz, s);
                        };
                        v = NoiseHelpers::fbm(fn, nx, ny, tz, sd, oct, lac, gain);
                        break;
                    }
                    case NoiseType::HarmonicSummation:
                    {
                        auto fn = [](float px, float py, float pz, uint32_t s) {
                            return NoiseHelpers::perlin3D(px, py, pz, s);
                        };
                        v = NoiseHelpers::fbm(fn, nx, ny, tz, sd, oct, lac, gain);
                        break;
                    }
                    case NoiseType::Alligator:
                    {
                        float pv = 0.0f, a = 1.0f, mx = 0.0f, fx = nx, fy = ny;
                        for (int i = 0; i < oct; ++i) {
                            pv += NoiseHelpers::alligatorNoise(fx, fy, sd) * a;
                            mx += a; a *= gain; fx *= lac; fy *= lac;
                        }
                        v = mx > 0 ? pv / mx : 0.0f;
                        break;
                    }
                }

                // Apply exponent and amplitude/offset
                v = juce::jlimit(0.0f, 1.0f, v);
                if (exp_ != 1.0f) v = std::pow(v, exp_);
                v = v * amp + off - 0.5f * amp;
                v = juce::jlimit(0.0f, 1.0f, v);

                uint8_t bv = (uint8_t)(v * 255.0f);
                // Write directly as packed RGB bytes (3 bytes per pixel for RGB)
                row[x * 3 + 0] = bv;
                row[x * 3 + 1] = bv;
                row[x * 3 + 2] = bv;
            }
        }
    };

    if (numThreads <= 1)
    {
        renderRows(0, H);
    }
    else
    {
        std::vector<std::thread> threads;
        threads.reserve(numThreads);
        for (int t = 0; t < numThreads; ++t)
        {
            int startRow = t * rowsPerThread;
            int endRow   = std::min(startRow + rowsPerThread, H);
            if (startRow >= H) break;
            threads.emplace_back(renderRows, startRow, endRow);
        }
        for (auto& th : threads) th.join();
    }

    // Blit cached image into target
    juce::Graphics g(target);
    g.drawImageAt(cachedImage, 0, 0);
}


CompositeNode::CompositeNode(int id, int x, int y)
    : BaseNode(id, "Composite", x, y)
{
}

void CompositeNode::process(juce::Image& target, double time)
{
}

DisplacementNode::DisplacementNode(int id, int x, int y) : BaseNode(id, "Displacement", x, y) {}

void DisplacementNode::process(juce::Image& target, double time) {}

EdgeDetectionNode::EdgeDetectionNode(int id, int x, int y) : BaseNode(id, "Edge Detection", x, y) {}

void EdgeDetectionNode::process(juce::Image& target, double time) {}

//==============================================================================
// ShaderToyNode
//==============================================================================
static const char* kDefaultShader = R"(
void mainImage(out vec4 fragColor, in vec2 fragCoord)
{
    vec2 uv = fragCoord / iResolution.xy;
    // Animated rainbow gradient — same as shadertoy.com default
    vec3 col = 0.5 + 0.5 * cos(iTime + uv.xyx + vec3(0.0, 2.0, 4.0));
    fragColor = vec4(col, 1.0);
}
)";

ShaderToyNode::ShaderToyNode(int id, int x, int y)
    : BaseNode(id, "ShaderToy", x, y)
{
    shaderSource = kDefaultShader;
}

ShaderToyNode::~ShaderToyNode()
{
    // renderer must be destroyed on message thread (it was created there)
    renderer.reset();
}

void ShaderToyNode::applyShader()
{
    if (renderer)
    {
        renderer->setShaderSource(shaderSource);
        lastError.clear();
    }
}

void ShaderToyNode::process(juce::Image& target, double time)
{
    // Lazy init — must be on the message thread
    if (renderer == nullptr)
    {
        jassert(juce::MessageManager::existsAndIsCurrentThread());
        renderer = std::make_unique<ShaderToyGLRenderer>();
        renderer->setShaderSource(shaderSource);
    }

    const double timeDelta = (lastTime >= 0.0) ? (time - lastTime) : 0.0;
    lastTime = time;

    // Look up any image connected to our input pin (pin 0 = iChannel0)
    // The evaluateNode machinery writes into 'target'; we pass nullptr for channel0
    // since the caller is responsible for providing the input via the graph.
    // We use a blank image as iChannel0 fallback here; the graph evaluator
    // will eventually call this after resolving inputs — for now pass nullptr.
    renderer->renderFrame(target, time, timeDelta, frameCount, nullptr);

    // Grab any compile error
    lastError = renderer->getLastError();
    ++frameCount;
}

NodeGraph::NodeGraph()
{
}

void NodeGraph::addNode(std::shared_ptr<BaseNode> node)
{
    nodes.push_back(node);
}

void NodeGraph::removeNode(int id)
{
    // Remove links connected to this node
    links.erase(std::remove_if(links.begin(), links.end(),
        [id](const NodeLink& l) { return l.fromNodeId == id || l.toNodeId == id; }), links.end());

    // Remove the node itself
    nodes.erase(std::remove_if(nodes.begin(), nodes.end(),
        [id](const std::shared_ptr<BaseNode>& n) { return n->id == id; }), nodes.end());
}

std::shared_ptr<BaseNode> NodeGraph::getNode(int id)
{
    for (auto& n : nodes) {
        if (n->id == id) return n;
    }
    return nullptr;
}

void NodeGraph::addLink(int fromId, int fromPin, int toId, int toPin)
{
    // Prevent an input pin from having multiple incoming links.
    // If a link already goes to this exact input pin, remove it first.
    links.erase(std::remove_if(links.begin(), links.end(),
        [toId, toPin](const NodeLink& l) { 
            return l.toNodeId == toId && l.toPinIndex == toPin; 
        }), links.end());

    links.push_back({fromId, fromPin, toId, toPin});
}

juce::Image NodeGraph::evaluateNode(int nodeId, double time)
{
    auto node = getNode(nodeId);
    if (!node) return juce::Image();

    // Create intermediate image for this node
    juce::Image result(juce::Image::ARGB, node->resolutionX, node->resolutionY, true);
    result.clear(result.getBounds(), juce::Colours::transparentBlack);

    if (auto* composite = dynamic_cast<CompositeNode*>(node.get()))
    {
        juce::Image imgA, imgB;
        for (const auto& link : links)
        {
            if (link.toNodeId == nodeId)
            {
                if (link.toPinIndex == 0) imgA = evaluateNode(link.fromNodeId, time);
                if (link.toPinIndex == 1) imgB = evaluateNode(link.fromNodeId, time);
            }
        }

        {
            juce::Graphics g(result);
            if (imgA.isValid())
            {
                g.setImageResamplingQuality(juce::Graphics::lowResamplingQuality);
                g.drawImageTransformed(imgA, juce::RectanglePlacement(juce::RectanglePlacement::stretchToFit).getTransformToFit(imgA.getBounds().toFloat(), result.getBounds().toFloat()));
            }
        }

        if (imgB.isValid())
        {
            if (composite->blendMode == 0) // Normal
            {
                juce::Graphics g(result);
                g.setImageResamplingQuality(juce::Graphics::lowResamplingQuality);
                g.drawImageTransformed(imgB, juce::RectanglePlacement(juce::RectanglePlacement::stretchToFit).getTransformToFit(imgB.getBounds().toFloat(), result.getBounds().toFloat()));
            }
            else
            {
                juce::Image scaledB(juce::Image::ARGB, result.getWidth(), result.getHeight(), true);
                {
                    juce::Graphics gb(scaledB);
                    gb.setImageResamplingQuality(juce::Graphics::lowResamplingQuality);
                    gb.drawImageTransformed(imgB, juce::RectanglePlacement(juce::RectanglePlacement::stretchToFit).getTransformToFit(imgB.getBounds().toFloat(), scaledB.getBounds().toFloat()));
                }

                juce::Image::BitmapData dataOut(result, juce::Image::BitmapData::readWrite);
                juce::Image::BitmapData dataB(scaledB, juce::Image::BitmapData::readOnly);

                for (int y = 0; y < result.getHeight(); ++y)
                {
                    auto* pOut = (juce::PixelARGB*)dataOut.getLinePointer(y);
                    auto* pB = (juce::PixelARGB*)dataB.getLinePointer(y);

                    for (int x = 0; x < result.getWidth(); ++x)
                    {
                        if (pB->getAlpha() > 0)
                        {
                            juce::PixelARGB ca = *pOut;
                            juce::PixelARGB cb = *pB;
                            juce::PixelARGB cout = ca;
                            
                            int alphaB = cb.getAlpha();

                            if (composite->blendMode == 1) // Add
                            {
                                cout.setARGB(alphaB,
                                    juce::jmin(255, ca.getRed() + cb.getRed()),
                                    juce::jmin(255, ca.getGreen() + cb.getGreen()),
                                    juce::jmin(255, ca.getBlue() + cb.getBlue()));
                            }
                            else if (composite->blendMode == 2) // Multiply
                            {
                                cout.setARGB(alphaB,
                                    (ca.getRed() * cb.getRed()) / 255,
                                    (ca.getGreen() * cb.getGreen()) / 255,
                                    (ca.getBlue() * cb.getBlue()) / 255);
                            }
                            else if (composite->blendMode == 3) // Screen
                            {
                                cout.setARGB(alphaB,
                                    255 - ((255 - ca.getRed()) * (255 - cb.getRed())) / 255,
                                    255 - ((255 - ca.getGreen()) * (255 - cb.getGreen())) / 255,
                                    255 - ((255 - ca.getBlue()) * (255 - cb.getBlue())) / 255);
                            }
                            
                            pOut->blend(cout);
                        }
                        pOut++;
                        pB++;
                    }
                }
            }
        }
    }
    else if (auto* disp = dynamic_cast<DisplacementNode*>(node.get()))
    {
        juce::Image imgA, imgB;
        for (const auto& link : links)
        {
            if (link.toNodeId == nodeId)
            {
                if (link.toPinIndex == 0) imgA = evaluateNode(link.fromNodeId, time);
                if (link.toPinIndex == 1) imgB = evaluateNode(link.fromNodeId, time);
            }
        }

        if (imgA.isValid())
        {
            if (!imgB.isValid())
            {
                juce::Graphics g(result);
                g.setImageResamplingQuality(juce::Graphics::lowResamplingQuality);
                g.drawImageTransformed(imgA, juce::RectanglePlacement(juce::RectanglePlacement::stretchToFit).getTransformToFit(imgA.getBounds().toFloat(), result.getBounds().toFloat()));
            }
            else
            {
                juce::Image scaledA(juce::Image::ARGB, result.getWidth(), result.getHeight(), true);
                {
                    juce::Graphics ga(scaledA);
                    ga.setImageResamplingQuality(juce::Graphics::lowResamplingQuality);
                    ga.drawImageTransformed(imgA, juce::RectanglePlacement(juce::RectanglePlacement::stretchToFit).getTransformToFit(imgA.getBounds().toFloat(), scaledA.getBounds().toFloat()));
                }

                juce::Image scaledB(juce::Image::ARGB, result.getWidth(), result.getHeight(), true);
                {
                    juce::Graphics gb(scaledB);
                    gb.setImageResamplingQuality(juce::Graphics::lowResamplingQuality);
                    gb.drawImageTransformed(imgB, juce::RectanglePlacement(juce::RectanglePlacement::stretchToFit).getTransformToFit(imgB.getBounds().toFloat(), scaledB.getBounds().toFloat()));
                }

                juce::Image::BitmapData dataOut(result, juce::Image::BitmapData::readWrite);
                juce::Image::BitmapData dataA(scaledA, juce::Image::BitmapData::readOnly);
                juce::Image::BitmapData dataB(scaledB, juce::Image::BitmapData::readOnly);

                int w = result.getWidth();
                int h = result.getHeight();
                
                float ax = disp->amountX.eval(time);
                float ay = disp->amountY.eval(time);

                for (int y = 0; y < h; ++y)
                {
                    auto* pOut = (juce::PixelARGB*)dataOut.getLinePointer(y);
                    auto* pB = (juce::PixelARGB*)dataB.getLinePointer(y);

                    for (int x = 0; x < w; ++x)
                    {
                        float dx = ((pB->getRed() / 255.0f) * 2.0f - 1.0f) * ax * w;
                        float dy = ((pB->getGreen() / 255.0f) * 2.0f - 1.0f) * ay * h;

                        int sx = juce::jlimit(0, w - 1, x + (int)dx);
                        int sy = juce::jlimit(0, h - 1, y + (int)dy);

                        *pOut = *((juce::PixelARGB*)dataA.getLinePointer(sy) + sx);

                        pOut++;
                        pB++;
                    }
                }
            }
        }
    }
    else if (auto* edge = dynamic_cast<EdgeDetectionNode*>(node.get()))
    {
        juce::Image imgA;
        for (const auto& link : links)
        {
            if (link.toNodeId == nodeId && link.toPinIndex == 0)
                imgA = evaluateNode(link.fromNodeId, time);
        }

        if (imgA.isValid())
        {
            juce::Image scaledA(juce::Image::ARGB, result.getWidth(), result.getHeight(), true);
            {
                juce::Graphics ga(scaledA);
                ga.setImageResamplingQuality(juce::Graphics::lowResamplingQuality);
                ga.drawImageTransformed(imgA, juce::RectanglePlacement(juce::RectanglePlacement::stretchToFit).getTransformToFit(imgA.getBounds().toFloat(), scaledA.getBounds().toFloat()));
            }

            juce::Image::BitmapData dataOut(result, juce::Image::BitmapData::readWrite);
            juce::Image::BitmapData dataA(scaledA, juce::Image::BitmapData::readOnly);

            int w = result.getWidth();
            int h = result.getHeight();
            
            float intensity = edge->intensity.eval(time);

            for (int y = 1; y < h - 1; ++y)
            {
                auto* pOut = (juce::PixelARGB*)dataOut.getLinePointer(y);
                auto* pA_up = (juce::PixelARGB*)dataA.getLinePointer(y - 1);
                auto* pA = (juce::PixelARGB*)dataA.getLinePointer(y);
                auto* pA_down = (juce::PixelARGB*)dataA.getLinePointer(y + 1);

                for (int x = 1; x < w - 1; ++x)
                {
                    auto brightness = [](const juce::PixelARGB& p) {
                        return (p.getRed() + p.getGreen() + p.getBlue()) / 3;
                    };

                    int vUp = brightness(pA_up[x]);
                    int vDown = brightness(pA_down[x]);
                    int vLeft = brightness(pA[x - 1]);
                    int vRight = brightness(pA[x + 1]);
                    int vCenter = brightness(pA[x]);

                    int val = vCenter * 4 - vUp - vDown - vLeft - vRight;
                    val = juce::jlimit(0, 255, (int)(abs(val) * intensity));

                    pOut[x].setARGB(255, val, val, val);
                }
            }
        }
    }
    else
    {
        // Normal generative node
        node->process(result, time);
    }

    return result;
}

void NodeGraph::renderOutput(juce::Graphics& g, juce::Rectangle<float> bounds, double time)
{
    BaseNode* outputNode = nullptr;
    for (auto& n : nodes) {
        if (dynamic_cast<OutputNode*>(n.get())) {
            outputNode = n.get();
            break;
        }
    }

    if (!outputNode) return;

    for (const auto& link : links)
    {
        if (link.toNodeId == outputNode->id)
        {
            juce::Image inputImg = evaluateNode(link.fromNodeId, time);
            
            if (inputImg.isValid())
            {
                g.setImageResamplingQuality(juce::Graphics::lowResamplingQuality);
                g.drawImageTransformed(inputImg, juce::RectanglePlacement(juce::RectanglePlacement::stretchToFit).getTransformToFit(inputImg.getBounds().toFloat(), bounds));
            }
        }
    }
}
