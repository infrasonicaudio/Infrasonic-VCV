# Warp Core

<p align="center">
<img src="res/warp-core.png" width="240">
</p>

Warp Core is a dual-algorithm phase distortion oscillator with internal and external phase 
modulation with support for polyphony. This module started as a firmware prototype for a
Eurorack DSP platform and is being released as a free VCV Rack module to test the idea and 
controls, while a hardware version is tentatively in development for a future release.

## Overview

Inspired by the phase distortion techniques pioneered by Casio's CZ synthesizers, Warp Core
starts with a simple carrier phasor - a ramp increasing from 0 to 1 over the course of one
period at the fundamental frequency. This phasor is then warped using two sequential
non-linear phase distortion algorithms **WARP A** and **WARP B** as indicated by the LEDs
and with amounts dialed in by the corresponding knobs and CV inputs.

The phasor is also modulated either pre- or post-distortion by an internal sinusoidal modulation
oscillator with a configurable fixed ratio to the carrier and a modulation index (amount) controlled
by **INT PM** and **PM CV**. This modulation oscillator is also summed with the **EXT PM** input
before being applied to the phasor. Applying phase modulation pre-phase-distortion tends to produce
much harsher and more intense timbres than applying post-phase-distortion.

The phasor is used to index a sine wave, producing a complex oscillator.
As a result of the distortion and/or modulation, the output produces complex harmonics and
shapes that would be difficult to achieve with the same fluidity using different techniques.

Finally, the output is optionally "windowed" using a unipolar sawtooth or triangle shape at
the fundamental frequency of the carrier phasor. This technique was used in the Casio CZ series 
to mitigate discontinuities at the start or end of the phase-distorted wave when using specific
algorithms. It can be used similarly in Warp Core, but it is also an interesting way to add
additional color to the sound, as a form of simple amplitude modulation.

A second output is provided which defaults to a 90-degree shifted (cosine) lookup using the same
phasor and applying the same windowing. This does *not* produce a 90-degree shifted copy of the 
first output, but rather a similar-but-different complex output waveform, since the phasor wrapping 
applies differently to the cosine than it does to the sine. (Hint: this sounds pretty cool when 
used as a "right" output for stereo processing). The second output may also be optionally configured
to produce a pure wave at the same fundamental frequency as the carrier, a sine sub-oscillator one 
octave below the carrier, or to direclty output the distorted and modulated phasor.

## Controls

<p align="center">
<img src="res/controls.png" width="240">
</p>

1. **TUNE (Knob)** – Carrier frequency tuning. 5 Octave range from C1 (32.7 Hz) to C6 (2646.4 Hz)
2. **INT PM Amount (Knob)** – Internal phase modulation amount. While in **Edit Internal PM Ratio** mode, selects internal PM oscillator ratio.
3. **WARP A Selection (Button)** – Cycle phase distortion algorithm for WARP A
4. **WARP B Selection (Button)** – Cycle phase distortion algorithm for WARP B
5. **WARP A Amount (Knob)** – Phase distortion amount for WARP A algorithm
6. **WARP B Amount (Knob)** – Phase distortion amount for WARP B algorithm
7. **Indicator LEDs** – Indicates selected phase distortion algorithm for WARP A (Blue) and WARP B (Red). While in **Edit Internal PM Ratio** mode, indicates PM ratio.
8. **WARP A CV Attenuverter (Knob)** – Attenuverter for **WARP A CV**
9. **WARP B CV Attenuverter (Knob)** – Attenuverter for **WARP B CV**
10. **PM Pre/Post (Switch)** – Switches between phase modulation being applied before (pre) or after (post) phase distortion.
11. **Windowing Selection (Switch)** – Switches between windowing options for final waveform: OFF (no windowing), Saw, or Triangle.
12. **WARP A CV (Input)** – CV input for **WARP A** amount, attenu-verted by **WARP A CV Attenuverter**. Acts as offset for **WARP A Amount** knob.
13. **WARP B CV (Input)** – CV input for **WARP B** amount, attenu-verted by **WARP B CV Attenuverter**. Acts as offset for **WARP B Amount** knob.
14. **V/Oct (Input)** – Standard volt-per-octave frequency input for the carrier phasor. 
15. **PM CV (Input)** – CV input for internal PM amount. Acts as offset for **INT PM Amount** knob. 
16. **EXT PM (Input)** – Audio-rate external phase modulation input. Sums scaled incoming signal directly with (attenuated) Internal PM oscillator before modulating the phasor.<sup>*</sup>
17. **Main Output 0° (Output)** – Main oscillator audio output
18. **Aux Output 90° (Output)** – Auxiliary audio output. Defaults to "90°" oscillator output as described in the overview. Can be configured to output a pure sine tracking the carrier, a sub-octave sine tracking the carrier, or the distorted and modulated phasor.

_<sup>*</sup> For purposes of SIMD performance optimization, Warp Core processes its DSP in blocks
of samples rather than one at a time. The External PM input is internally buffered to maintain 
true audio rate processing, however as a result of the buffering, there is slight latency on the output. Therefore PM feedback patches will not necessarily sound great. A low-latency mode may be added in a future update to enble improved feedback patching._

## Phase Distortion Algorithms

There are currently four selectable phase distortion algorithms, one of which may be chosen
for each of **WARP A** and **WARP B**. The phase distortion is non-linear and applied in series, 
so using the same algorithm for both will produce interesting results, as will changing the order
of the algorithms.

_More algorithms may be added in future updates!_

<div>
<img src="res/bend.svg" width="28" align="left"><h3>Bend</h3>
</div>

Applies a "tension" style curve to the phasor. Amount controls the level of tension.
This generally results in a narrowing of the waveform to one side, which  turns a sine 
wave to something a little closer to a narrow pulse wave.

<div>
<img src="res/sync.svg" width="28" align="left"><h3>Sync</h3>
</div>

Emulation of oscillator hard-sync by wrapping the phasor back to zero at a frequency
greater than the fundamental. Try this by itself with sawtooth windowing for a sound
resembling a sweeping resonant filter.

<div>
<img src="res/pinch.svg" width="28" align="left"><h3>Pinch</h3>
</div>

"Pinches" the phasor toward the center of its period, leaving the beginning and end at the
minimum and maximum point. This has sort of a formant-shifting effect on the sound.

<div>
<img src="res/fold.svg" width="28" align="left"><h3>Fold</h3>
</div>

Wavefolds the phasor by progressively mirroring it when it reaches the max/min level. This
is similar to sync but with a wholly different sonic character.

## Context Menu

Right click the module panel for a few additional options:

### Editing Internal PM Ratio

The internal phase modulation ratio can be adjusted by enabling this option and turning the
**INT PM** knob. While this option is enabled, the LEDs indicate the ratio numerator and
denominator in binary format. The numbers on the right side of the LEDs are a guide: blue
represents the numerator and red the denominator. Pink means that digit applies to both.

Above about 12-o-clock the ratios are integer multiples of the carrier. Below that
are other small-integer ratios and divisions of the carrier.

_Example:_

* Blue LEDs 1 and 2 – `Numerator = 1 + 2 = 3`
* Red LED 1 – `Denominator = 1`
* Ratio = `3:1` (third harmonic)

<img src="res/pm-ratio-ex.png">

_As this module is intended to be a prototype for a future hardware module, the UI for this
setting is meant to be something feasible to implement in hardware , e.g. by holding one 
of the buttons and turning the **INT PM** knob._

### Phase Distortion Algorithm Selection

Instead of using the buttons on the panel you can also directly select the algorithms
used for each of the **WARP A** and **WARP B** from a context menu.

### Auxiliary Output Mode

The **Aux Output** (labeled 90° on the panel) may be configured to a few alternate modes:

* **90°** – Uses the same phasor as the main output to index a cosine instead of a sine, producing an alternate pseudo-phase-shifted output. Try this as the right side of a stereo pair, with the main output as left!
* **Sine (Unison)** – Outputs a pure sine wave at the same frequency as the carrier phasor. Useful for mixing in some additional oomph to thicken up timbres that reduce fundamental harmonic energy.
* **Sine (Sub)** – Outputs a pure sine wave one octave below the carrier phasor. Also useful for mixing with the main output to thicken up the sound.
* **Phasor** – Outputs the distorted and modulated phasor directly. Useful for visualizing  and understanding the effect of each algorithm, and possibly for other creative patching!

### Oversampling

As phase distortion is inherently a nonlinear technique, this module is internally oversampled
to mitigate aliasing. The default is 4x oversampling but you may use the context menu to configure
the level of oversampling from 1x (none) to 16x. The CPU usage will increase the higher you go,
especially when using the module for polyphony.
