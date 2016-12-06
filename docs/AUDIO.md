# Sound mixing in Neothyne

Sound mixing in Neothyne occurs on its own thread independent from the
rest of the engine. The engine exposes functionality for constructing
"fire and forget" audio sources which can then be manipulated by reference.

# Definitions

## Sources
Audio sources contain the information related to "sound". This typically
includes things like the sample data, stream duration, etc. Think of this
as the engine's representation of the "source asset". Hence the name.

## Instance
Audio instances are "voices". You can have a maximum of 64 instances
playing at any given time. Instances reference audio source(s). Note the
plural, an instance can sample many sources at once. This is to allow
grouping of audio for synchronization purposes.

## Post clip scalar
Since volume is applied before clipping, it may be hard to eliminate
sound distortion by lowering volume. The post clip scalar is basically
a way to control volume after clipping has occurred. Take note that this
is potentially dangerous and must be used with caution since most uses
of it will completely distort the final samples. The scale is not limited
to `0..1` and negative values will produce very strange behavior.

# Options and Features

There are a variety of options that can be configured for sound instances
or globally as a whole these include

- volume
- panning
- relative play speed
- filters
- faders

## Faders
Faders are a convenient way to perform tasks per-mix. Their name comes
from fading volume in or out. But they can be used to do much more.
There are faders to do the following

- Fade volume in or out over specified time
- Fade panning from left/right channel in or out over specified time
- Fade relative play speed over specified time
- Fade global volume over specified time
- After specified time stop a channel of audio
- After specified time pause a channel of audio
- Oscillate volume at specified frequency
- Oscillate panning at specified frequency
- Oscillate relative play speed at specified frequency
- Oscillate global volume at specified frequency

## Filters
Filters are used to modify sound in some fashion or another. Typical
uses for them include creating environmental effects such as echoing.

There are a variety of filters

- Biquadratic Resonance
  - Low pass filter
  - High pass filter
  - Band pass filter
- Echo filter
- DC blocking filter
- *More to come*

### Biquadratic Resonance Filters
Biquadratic Resonance filters are a cheap approximate solution to achieving
the common low-pass, hi-pass and band-pass filters.

All Biquadratic resonance filters have three parameters that affect the
effect they have on the samples

- sample rate
- cut off frequency
- cut off resonance

### Echo Filter
The echo filter is, as the name suggests, a way to produce an echo.
At its core, it simply records and replays samples based on two parameters

- delay
- decay


### DC Blocking Filter
The DC blocking filter is used to fix "constant voltage offset" issues
with cheap audio recordings or audio which has been processed so much
that the waveform is no longer centered about the zero point.

#### Problem
Here's what a sine wave looks like with a constant offset

![Constant DC Offset](https://cdn.rawgit.com/graphitemaster/neothyne/master/docs/img/dc_offset.svg)

This is of importance especially when adding several sounds together as
signals with a constant offset waste the range and can lead to clipping
more easily. This is what it should look like with no DC component

![Proper Waveform Centered on 0](https://cdn.rawgit.com/graphitemaster/neothyne/master/docs/img/dc_proper.svg)

#### Solution
It's easy to just subtract a constant amount from every sample, however
a more generalized solution would work in more situations, that solution
is to filter out low frequencies.

Neothyne uses the easiest DC blocking filter for efficiency reasons, it
looks like this

![DC Blocking Filter](https://cdn.rawgit.com/graphitemaster/neothyne/master/docs/img/dc_blocking_filter.svg)

The flow graph above describes what happens to the input signal as it is
filtered. There are three paths the input and output to the DC blocking
filter make when passing through the filter

![DC Blocking Filter Paths](https://cdn.rawgit.com/graphitemaster/neothyne/master/docs/img/dc_blocking_filter_paths.svg)

Represented as a difference equation

```
y[n] = x[n] - x[n - 1] + a * y[n - 1]
```

Where `y[n]` is the output at the current time `n`, and `x[n]` is the
input at the current time `n` respectively.

If you're curious about numerical stability, consider the approach
taken here

##### Difference Equation
![Difference Equation](https://cdn.rawgit.com/graphitemaster/neothyne/master/docs/img/difference_eq.svg)

##### Z-transform
![Z-transform](https://cdn.rawgit.com/graphitemaster/neothyne/master/docs/img/ztransform_eq.svg)

##### Transfer Function
![Transfer Function](https://cdn.rawgit.com/graphitemaster/neothyne/master/docs/img/transfer_function_eq.svg)

##### Frequency Response
![Frequency Response](https://cdn.rawgit.com/graphitemaster/neothyne/master/docs/img/frequency_response_eq.svg)

Where the frequency response for `a` is `~1.0`. Nothing too bad should
happen. Neothyne in particular averages samples over a relatively long
period and subtracts this from the output.
