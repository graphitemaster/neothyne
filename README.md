# Neothyne

Neothyne is an attempt at getting back to the roots of good old twitch shooting
akin to that of Quake World.

Here's what it looks like currently
![](http://i.imgur.com/TokBM3y.png)

## Philosphy

A modern dependency free game engine to make a twitch shooter with

## Status

As of late Neothyne is just an engine offering

* Efficent KD-tree (which allows for)
  * Efficent static and sweeping collision detection
  * Efficent scene management
  * Smooth world traces for accurate and efficent collision response

* Deferred renderer (which can do)
  * Directional lighting (with ambient and diffuse terms)
  * Specular lighting (with power and intensity terms)
  * Point lighting (with omnidirectional shadow mapping)
  * Spot lighting (with shadow mapping)
  * Skybox
  * Soft particles
  * Normal mapping (dot3 bump mapping)
  * Displacement mapping (steep parallax and relief mapping)
  * Fog (linear, exp and exp2)
  * Fast approximate anti-aliasing (FXAA)
  * Screen space ambient occlusion (SSAO)
  * Vignette
  * View frustum culling
  * Skeletal model animation (IQM)
  * Variable color grading (supporting)
    * Color balance (shadows, midtones and highlights)
    * Hue, lightness and saturation
    * Brightness and contrast

* Scripting language (Neo language)
  * Heavily inspired by Javascript, C++, Lua, Ruby, C#, Java (In order of inspiration)
  * Dynamically typed
  * Structured
  * First class values
  * Lexically scoped
  * Type rich (function, method, bool, int, float, string, array, object)
  * Object oriented (Single inheritence multiple-interface)
  * Garbage collected

* Fire and forget audio mixer (which can do)
  * Multiple sound sources (global and local)
  * Faders (adjust volume, speed, panning over time)
  * Dynamic audio control
    * Fade volume, speed, panning, etc.
    * Panning
    * Speed
    * Filters
      * Echo
      * Biquadratic Resonance Filters
        * Low pass
        * Hi pass
        * Band pass
      * DC Blocking
    * Audio grouping

* Immediate-mode graphical user interface (which can do)
  * Buttons
  * Items
  * Check boxes
  * Radio buttons
  * Windows
  * Collapsible areas
  * Labels (left-justified text)
  * Values (right-justified text)
  * Sliders (horizontal and vertical)
  * Indentation
  * Headers (vertical separations and lines)
  * A variety of raw rendering primitives (such as)
    * Lines
    * Rectangles
    * Text
    * Images
    * Models

* Console (which allows for)
  * Global configuration
  * History and tab completion
  * Reactive changes to various engine components (including renderer)

* Asset optimization (which can do)
  * Online texture compression
  * Online linear-speed vertex cache optimization
  * Online half-precision float conversion
  * DXT end-point optimization (helps old hardware DXT decode fetches)

## Goals

* Networking (client/server model)
* Build the game
* Scripting

## Etymology
- Neo (Old English from Proto-Germanic: `nawiz`, `nawaz`, i.e: *"corpse"*)
- Thyne (Old English: `thyn`, `þyn`. Cognate to German: `dein`, i.e: *"yours"*)

In modern English: *"your corpse"*

## Screenshots
An imgur album of screenshots showing the engine and development of it can
be found [here](http://imgur.com/a/Y3Rfi)

## Build
Please check the [build documentation](docs/BUILDING.md)

## System requirements
Neothyne requires a GPU which is capable of GL 3.0. Modern low profile GPUs may
function poorly. (Note: Some cards are advertised as being GL 2 capable, but
with newer drivers, can become GL 3 capable. This is the case for some cards in
the GeForce 8 series, for example.)

## Documentation
Documentation may be found in the `docs' directory.
