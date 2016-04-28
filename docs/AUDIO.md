# Sound mixing in Neothyne

Sound mixing in Neothyne occurs on its own thread independent from the
rest of the engine. The engine exposes functionality for constructing
"fire and forget" audio sources which can then be manipulated by reference.

The engine exposes a rich interface for querying and manipulating sound
sources.

## Separation
The concept of sound is broken into two layers: global and local.
Local options affect the sound locally while global options are applied
after all sound sources (and their respective options) have been mixed
into the final output.

## Global options
Global options include the ability to manipulate volume, post clipping
scale factor, and a filter.

## Local options
Local options include the ability to manipulate volume, relative play
speed, sampling rate, panning, and a filter. Similarly, local sources
can be manipulated with faders.

## Faders
Faders are a way to schedule a change repeatedly, starting from an
initial value and interpolating the change to a maximal value. For example
they can be used to increase or decrease volume over time, among other
things.

## Schedulers
Schedulers are a more immediate form of faders. They can be used to
schedule an action to occur in the future. For instance "stop this
sound in 5 seconds."

## Filters
You can attach a filter to any sound source or globally. Filters are
executed on a per-sample basis and currently the following filters are
supported.

### Biquadratic Resonance
- LowPass
- HighPass
- BandPass

### Miscellaneous
- Echo
