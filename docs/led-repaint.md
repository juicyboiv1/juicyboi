# Pedal LEDs and partial repaints (Rev3.2.1 fix)

**Report.** On an M3 MacBook Air (arm64) Standalone the tuner and gate LEDs did
not light. The same build worked on an Intel Mac Mini. Boost, EQ and Cab LEDs
may be affected too.

**What the code did.**
- **Gate and the other pedal LEDs.** They are painted by `Pedal::paint`, but they
  were only *invalidated* by `Pedal::pollLed()`. That asked for a 40×40 rectangle
  around the LED.
- **Tuner LED.** `TunerPedal::timerCallback()` invalidated only the display window
  when the tuner switched on. The LED sits below that window, so nothing ever
  asked for it to be repainted.
- **Gate LED.** Also required `isGateActive()`, which is only set once the audio
  callback has processed a block with the gate enabled.

The LEDs therefore appeared only when something else happened to repaint them.
What else repaints depends on how macOS CoreGraphics merges and clips the dirty
rectangles of the single window-sized NSView. That behaviour varies with the OS
release and the machine ("different dirty region repaint behavior on similar
machines", JUCE forum; `setNeedsDisplayInRect` merging changed in macOS 14).
The Mini (an older macOS, different compositor) was over-invalidating enough to
cover the LEDs; the M3 honoured the exact, too-small rectangles.

This was proven only up to the code defect (see below). The macOS-side merging
behaviour is the best-evidence explanation for why the two machines differed. It
is not reproducible on Linux.

**Fix.**
- A pedal LED change repaints the whole pedal (`Pedal::ledRepaintArea()`).
- The tuner repaints the whole pedal when it is switched on or off, and only
  the display window for readings.
- Footswitch changes (boost, EQ, gate, cab, tuner) refresh their LED immediately;
  the 30 Hz poll remains as a safety net.
- A pedal that becomes visible or is reparented forgets what it last drew
  (`invalidateLed`), so it always repaints.
- The gate LED is lit whenever the gate parameter is on (colour shows
  open/closed), independent of whether audio has run. A NaN open amount shows as open.
- `Tests/EditorTest.cpp` section 9 snapshots the LED pixels: dark off, red tuner
  LED on engage, green gate LED when enabled with no audio running, dark again
  after release, and the repaint area covers the LED glow.
