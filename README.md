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

## Supported texture formats

Neothyne rolls it's own texture loaders, the support of each loader is specified
below:

* JPG (baseline)
  * Baseline only (not progressive nor lossless)
  * Supports 8-bit greyscale colorspace
  * Supports YCbCr colorspace
  * Supports any power-of-two chroma subsampling ratio
  * Supports standard centered chroma samples as well as EXIF cosited positioning
  * Supports restart markers
  * Supports the following chrominance upsampling filters
    * Bicubic (looks nicer than bilinear)
    * Fast pixel repetition (faster loads)

* PNG (fully supported)
  * Supports greyscale (with or without alpha) (at the following bitdepths [<= 8, 8, 16])
  * Supports RGB24
  * Supports palette (indexed color) (at bitdepths <= 8)
  * Supports RGBA32
  * Supports RGBA64

* TGA (only the following image types [1, 2, 9, 10])
  * Supports uncompressed color (paletted)
  * Supports uncompressed (BGRA)
  * Supports RLE compressed color (paletted)
  * Supports RLE compressed (BGRA)

In addition to those supported texture formats. Neothyne also builds BPTC, DXT1,
DXT5 and RGTC compressed textures at runtime and caches them to disk. It will
prefer the cached textures if the hardware supports BPTC, S3TC or RGTC texture
compression.

## Organization
The source code is organized into namespaces and a file name convention.
For example, anything prefixed with `u_` belongs to the *utility* namespace,
which is also named `u`. The following namespaces and file prefixes exist:

| Prefix | Namespace | Description |
|--------|-----------|-------------|
| u_     | u         | utility     |
| m_     | m         | math        |
| r_     | r         | renderer    |
| c_     | c         | console     |

## Utility library
Neothyne doesn't use the C++ standard library, instead it rolls it's own
implementation of common containers. They're not meant to be compatible with the
standard in any way, even if they share a similar API. The following containers
exist.

* u::pair
* u::set
* u::map
* u::string
* u::vector
* u::buffer

As well as a plethora of type traits and algorithmic functions which can be
found in `u_traits.h` and `u_algorithm.h` respectively. Miscellaneous utilites can
be found in `u_misc.h`.

## Goals

* Particles
* Shadow mapping
* 3D models (iqm?)
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
