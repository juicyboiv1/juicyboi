// juicyboi_pitchprobe: runs only the Rev3.3 PITCH validation (Tests/PitchShiftTest.cpp),
// including its latency and CPU measurements.
#include <juce_events/juce_events.h>
int pitchShiftValidation();
int main()
{
    juce::ScopedJuceInitialiser_GUI init;
    return pitchShiftValidation() == 0 ? 0 : 1;
}
