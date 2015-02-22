# Neothyne

Neothyne is an attempt at getting back to the roots of good old twitch shooting
akin to that of Quake World.

## Philosphy

Neothyne is a game/engine with a focus on matching that of idTech3/Q3A in terms
of visual fidelity. At its core, Neothyne is a relatively simple engine with a
focus on providing a minimum set of features to make a twitch shooter.

## Status

As of late Neothyne is just a piece of tech which, while primitive in nature,
embodies the following:

* Efficent KD-tree (which allows for)
  * Efficent static and sweeping collision detection
  * Efficent scene management
  * Smooth world traces for accurate and efficent collision response

* Deferred renderer (which can do)
  * Directional lighting (with ambient and diffuse terms)
  * Specular lighting (with power and intensity terms)
  * Point lighting
  * Spot lighting
  * Sky meshes (skybox, skydome, etc, any mesh will suffice).
  * Normal mapping (dot3 bump mapping)
  * Displacement mapping (steep parallax and relief mapping)
  * Sky fog gradient
  * Fast approximate anti-aliasing (FXAA)
  * Screen space ambient occlusion (SSAO)
  * View-frustum and occlusion-query culling

## Goals

* Particles
* Shadow mapping
* Networking (client/server model)
* Build the game

## Screenshots
An imgur album of screenshots showing the engine and development of it can
be found [here](http://imgur.com/a/Y3Rfi)

## Build
Neothyne depends on the following libraries:

* SDL2

Verify that you have this installed, then run `make'.

## System requirements
Neothyne requires a GPU which is capable of GL 3.3. Modern low profile GPUs may
function poorly. (Note: Some cards are advertised as being GL 2 capable, but with newer drivers, can become GL 3 capable. This is the case for some cards in the GeForce 8 series, for example.)

## Documentation
Documentation may be found in the `docs' directory.
