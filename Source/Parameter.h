#pragma once
#include <JuceHeader.h>
#include <vector>
#include <algorithm>

struct EnvelopePoint
{
    double time;
    float value;
    
    bool operator<(const EnvelopePoint& other) const {
        return time < other.time;
    }
};

class Envelope
{
public:
    Envelope() = default;

    std::vector<EnvelopePoint> points;
    bool isEnabled = false;
    double duration = 10.0;

    void addPoint(double time, float value)
    {
        points.push_back({time, value});
        std::sort(points.begin(), points.end());
    }

    float getValueAtTime(double time) const
    {
        if (points.empty()) return 0.0f;
        if (points.size() == 1) return points.front().value;
        
        // Wrap time to duration
        double t = std::fmod(time, duration);
        if (t < 0) t += duration;

        if (t <= points.front().time) return points.front().value;
        if (t >= points.back().time) return points.back().value;

        for (size_t i = 0; i < points.size() - 1; ++i)
        {
            if (t >= points[i].time && t <= points[i + 1].time)
            {
                double range = points[i + 1].time - points[i].time;
                if (range <= 0.0) return points[i].value;
                
                double factor = (t - points[i].time) / range;
                return points[i].value + (float)(factor * (points[i + 1].value - points[i].value));
            }
        }

        return points.back().value;
    }
};

class Parameter
{
public:
    Parameter(float defaultVal) : baseValue(defaultVal) {}

    float baseValue;
    Envelope envelope;

    float eval(double time) const
    {
        if (envelope.isEnabled)
            return envelope.getValueAtTime(time);
        return baseValue;
    }
    
    // Implicit conversion for backwards compatibility
    operator float() const { return baseValue; }
    
    // Assignment operator
    Parameter& operator=(float val) {
        baseValue = val;
        return *this;
    }
};

struct ColourEnvelopePoint
{
    double time;
    juce::Colour value;
    
    bool operator<(const ColourEnvelopePoint& other) const {
        return time < other.time;
    }
};

class ColourEnvelope
{
public:
    ColourEnvelope() = default;

    std::vector<ColourEnvelopePoint> points;
    bool isEnabled = false;
    double duration = 10.0;

    void addPoint(double time, juce::Colour value)
    {
        points.push_back({time, value});
        std::sort(points.begin(), points.end());
    }

    juce::Colour getValueAtTime(double time) const
    {
        if (points.empty()) return juce::Colours::transparentBlack;
        if (points.size() == 1) return points.front().value;
        
        double t = std::fmod(time, duration);
        if (t < 0) t += duration;

        if (t <= points.front().time) return points.front().value;
        if (t >= points.back().time) return points.back().value;

        for (size_t i = 0; i < points.size() - 1; ++i)
        {
            if (t >= points[i].time && t <= points[i + 1].time)
            {
                double range = points[i + 1].time - points[i].time;
                if (range <= 0.0) return points[i].value;
                
                double factor = (t - points[i].time) / range;
                // Since interpolatedWith takes the OTHER color and the proportion of it
                return points[i].value.interpolatedWith(points[i + 1].value, (float)factor);
            }
        }

        return points.back().value;
    }
};

class ColourParameter
{
public:
    ColourParameter(juce::Colour defaultVal = juce::Colours::white) : baseValue(defaultVal) {}

    juce::Colour baseValue;
    ColourEnvelope envelope;

    juce::Colour eval(double time) const
    {
        if (envelope.isEnabled)
            return envelope.getValueAtTime(time);
        return baseValue;
    }
    
    operator juce::Colour() const { return baseValue; }
    
    ColourParameter& operator=(juce::Colour val) {
        baseValue = val;
        return *this;
    }
};
