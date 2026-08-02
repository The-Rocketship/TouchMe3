import re

def refactor_nodegraph():
    with open('Source/NodeGraph.cpp', 'r') as f:
        content = f.read()

    # 1. Update signatures
    content = re.sub(r'void ([a-zA-Z0-9_]+)::process\(juce::Image& target, double time\)', 
                     r'void \1::process(juce::Image& target, const std::vector<juce::Image>& inputs, double time)', 
                     content)

    # 2. Update CompositeNode::process
    composite_body = """{
    juce::Image imgA = (inputs.size() > 0) ? inputs[0] : juce::Image();
    juce::Image imgB = (inputs.size() > 1) ? inputs[1] : juce::Image();

    {
        juce::Graphics g(target);
        if (imgA.isValid())
        {
            g.setImageResamplingQuality(juce::Graphics::lowResamplingQuality);
            g.drawImageTransformed(imgA, juce::RectanglePlacement(juce::RectanglePlacement::stretchToFit).getTransformToFit(imgA.getBounds().toFloat(), target.getBounds().toFloat()));
        }
    }

    if (imgB.isValid())
    {
        if (blendMode == 0) // Normal
        {
            juce::Graphics g(target);
            g.setImageResamplingQuality(juce::Graphics::lowResamplingQuality);
            g.drawImageTransformed(imgB, juce::RectanglePlacement(juce::RectanglePlacement::stretchToFit).getTransformToFit(imgB.getBounds().toFloat(), target.getBounds().toFloat()));
        }
        else
        {
            juce::Image scaledB(juce::Image::ARGB, target.getWidth(), target.getHeight(), true);
            {
                juce::Graphics gb(scaledB);
                gb.setImageResamplingQuality(juce::Graphics::lowResamplingQuality);
                gb.drawImageTransformed(imgB, juce::RectanglePlacement(juce::RectanglePlacement::stretchToFit).getTransformToFit(imgB.getBounds().toFloat(), scaledB.getBounds().toFloat()));
            }

            juce::Image::BitmapData dataOut(target, juce::Image::BitmapData::readWrite);
            juce::Image::BitmapData dataB(scaledB, juce::Image::BitmapData::readOnly);

            for (int y = 0; y < target.getHeight(); ++y)
            {
                auto* pOut = (juce::PixelARGB*)dataOut.getLinePointer(y);
                auto* pB = (juce::PixelARGB*)dataB.getLinePointer(y);

                for (int x = 0; x < target.getWidth(); ++x)
                {
                    if (pB->getAlpha() > 0)
                    {
                        juce::PixelARGB ca = *pOut;
                        juce::PixelARGB cb = *pB;
                        juce::PixelARGB cout = ca;
                        
                        int alphaB = cb.getAlpha();

                        if (blendMode == 1) // Add
                        {
                            cout.setARGB(alphaB,
                                juce::jmin(255, ca.getRed() + cb.getRed()),
                                juce::jmin(255, ca.getGreen() + cb.getGreen()),
                                juce::jmin(255, ca.getBlue() + cb.getBlue()));
                        }
                        else if (blendMode == 2) // Multiply
                        {
                            cout.setARGB(alphaB,
                                (ca.getRed() * cb.getRed()) / 255,
                                (ca.getGreen() * cb.getGreen()) / 255,
                                (ca.getBlue() * cb.getBlue()) / 255);
                        }
                        else if (blendMode == 3) // Screen
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
}"""
    content = re.sub(r'void CompositeNode::process\(juce::Image& target, const std::vector<juce::Image>& inputs, double time\)\s*\{\s*\}',
                     'void CompositeNode::process(juce::Image& target, const std::vector<juce::Image>& inputs, double time)\n' + composite_body, content)

    # 3. Update DisplacementNode::process
    displacement_body = """{
    juce::Image imgA = (inputs.size() > 0) ? inputs[0] : juce::Image();
    juce::Image imgB = (inputs.size() > 1) ? inputs[1] : juce::Image();

    if (imgA.isValid())
    {
        if (!imgB.isValid())
        {
            juce::Graphics g(target);
            g.setImageResamplingQuality(juce::Graphics::lowResamplingQuality);
            g.drawImageTransformed(imgA, juce::RectanglePlacement(juce::RectanglePlacement::stretchToFit).getTransformToFit(imgA.getBounds().toFloat(), target.getBounds().toFloat()));
        }
        else
        {
            juce::Image scaledA(juce::Image::ARGB, target.getWidth(), target.getHeight(), true);
            {
                juce::Graphics ga(scaledA);
                ga.setImageResamplingQuality(juce::Graphics::lowResamplingQuality);
                ga.drawImageTransformed(imgA, juce::RectanglePlacement(juce::RectanglePlacement::stretchToFit).getTransformToFit(imgA.getBounds().toFloat(), scaledA.getBounds().toFloat()));
            }

            juce::Image scaledB(juce::Image::ARGB, target.getWidth(), target.getHeight(), true);
            {
                juce::Graphics gb(scaledB);
                gb.setImageResamplingQuality(juce::Graphics::lowResamplingQuality);
                gb.drawImageTransformed(imgB, juce::RectanglePlacement(juce::RectanglePlacement::stretchToFit).getTransformToFit(imgB.getBounds().toFloat(), scaledB.getBounds().toFloat()));
            }

            juce::Image::BitmapData dataOut(target, juce::Image::BitmapData::readWrite);
            juce::Image::BitmapData dataA(scaledA, juce::Image::BitmapData::readOnly);
            juce::Image::BitmapData dataB(scaledB, juce::Image::BitmapData::readOnly);

            int w = target.getWidth();
            int h = target.getHeight();
            
            float ax = amountX.eval(time);
            float ay = amountY.eval(time);

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
}"""
    content = re.sub(r'void DisplacementNode::process\(juce::Image& target, const std::vector<juce::Image>& inputs, double time\)\s*\{\s*\}',
                     'void DisplacementNode::process(juce::Image& target, const std::vector<juce::Image>& inputs, double time)\n' + displacement_body, content)

    # 4. Update EdgeDetectionNode::process
    edge_body = """{
    juce::Image imgA = (inputs.size() > 0) ? inputs[0] : juce::Image();

    if (imgA.isValid())
    {
        juce::Image scaledA(juce::Image::ARGB, target.getWidth(), target.getHeight(), true);
        {
            juce::Graphics ga(scaledA);
            ga.setImageResamplingQuality(juce::Graphics::lowResamplingQuality);
            ga.drawImageTransformed(imgA, juce::RectanglePlacement(juce::RectanglePlacement::stretchToFit).getTransformToFit(imgA.getBounds().toFloat(), scaledA.getBounds().toFloat()));
        }

        juce::Image::BitmapData dataOut(target, juce::Image::BitmapData::readWrite);
        juce::Image::BitmapData dataA(scaledA, juce::Image::BitmapData::readOnly);

        int w = target.getWidth();
        int h = target.getHeight();
        
        float intensity_val = intensity.eval(time);

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
                val = juce::jlimit(0, 255, (int)(abs(val) * intensity_val));

                pOut[x].setARGB(255, val, val, val);
            }
        }
    }
}"""
    content = re.sub(r'void EdgeDetectionNode::process\(juce::Image& target, const std::vector<juce::Image>& inputs, double time\)\s*\{\s*\}',
                     'void EdgeDetectionNode::process(juce::Image& target, const std::vector<juce::Image>& inputs, double time)\n' + edge_body, content)

    # 5. Update NodeGraph::evaluateNode
    start_idx = content.find('juce::Image NodeGraph::evaluateNode(int nodeId, double time)')
    end_idx = content.find('void NodeGraph::renderOutput(', start_idx)
    
    evaluate_node_new = """juce::Image NodeGraph::evaluateNode(int nodeId, double time)
{
    auto node = getNode(nodeId);
    if (!node) return juce::Image();

    // Gather inputs for this node
    int numInputs = node->getNumInputPins();
    std::vector<juce::Image> inputs(numInputs);
    for (const auto& link : links)
    {
        if (link.toNodeId == nodeId && link.toPinIndex < numInputs)
        {
            inputs[link.toPinIndex] = evaluateNode(link.fromNodeId, time);
        }
    }

    // Create intermediate image for this node
    juce::Image result(juce::Image::ARGB, node->resolutionX, node->resolutionY, true);
    result.clear(result.getBounds(), juce::Colours::transparentBlack);

    node->process(result, inputs, time);

    return result;
}"""

    if start_idx != -1 and end_idx != -1:
        content = content[:start_idx] + evaluate_node_new + "\n\n" + content[end_idx:]

    with open('Source/NodeGraph.cpp', 'w') as f:
        f.write(content)

refactor_nodegraph()
