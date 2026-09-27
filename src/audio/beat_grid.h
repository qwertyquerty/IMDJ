#pragma once

#include <algorithm>
#include <cmath>

namespace imdj {

struct BeatGrid {
    double bpm = 0.0;
    double anchorFrame = 0.0;
    double sampleRate = 48000.0;

    bool valid() const { return bpm > 0.0 && sampleRate > 0.0; }
    double beatSeconds() const { return 60.0 / bpm; }
    double beatFrames() const { return beatSeconds() * sampleRate; }

    double beatAt(double frame) const { return valid() ? (frame - anchorFrame) / beatFrames() : 0.0; }
    double frameAt(double beat) const { return anchorFrame + beat * beatFrames(); }

    double quantize(double frame, double periodBeats = 1.0) const
    {
        if (!valid() || periodBeats <= 0.0) {
            return frame;
        }

        double nearest = std::round(beatAt(frame) / periodBeats) * periodBeats;
        return std::max(0.0, frameAt(nearest));
    }

    double phaseAt(double frame, double periodBeats = 1.0) const
    {
        if (!valid() || periodBeats <= 0.0) {
            return 0.0;
        }

        double phase = std::fmod(beatAt(frame), periodBeats);
        return phase < 0.0 ? phase + periodBeats : phase;
    }
};

inline double NearestBeatShift(double sinceBeat, double beatLength, double otherSinceBeat, double otherBeatLength)
{
    const double wrap = std::min(beatLength, otherBeatLength);
    if (wrap <= 0.0) {
        return 0.0;
    }

    double shift = std::fmod(otherSinceBeat - sinceBeat, wrap);
    if (shift > wrap * 0.5) {
        shift -= wrap;
    }
    else if (shift <= -wrap * 0.5) {
        shift += wrap;
    }

    return shift;
}

} // namespace imdj
