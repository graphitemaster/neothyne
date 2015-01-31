# Variables

The following console variables can be set via init.cfg.

The init.cfg file can be found in one of the following places depending on your
operating system

* Linux
    * $XDG_DATA_HOME/Neothyne
    * $HOME/.local/share/Neothyne
* Windows
    * %CSIDL_APPDATA%\\Neothyne
* Mac OS X
    * ~/Library/Application Support/Neothyne


## Video
##### vid_fullscreen
* 0 = windowed mode
* 1 = fullscreen

##### vid_height
resolution in height or if 0 and vid_width is also 0 engine uses desktop resolution

##### vid_width
resolution in width or if 0 and vid_height is also 0 engine uses desktop resolution

#### vid_vsync
* -1 = late swap tearing (instant buffer swap if vertical retrace is missed)
* 0 = disabled
* 1 = enabled
* 2 = cap framerate to refresh rate

#### vid_maxfps
value to cap framerate to or 0 for no framerate capping

#### vid_driver
the driver to use for rendering enclosed in quotes


## Renderer

##### r_aniso
Anisotropic texture filtering

* 0 = disable
* 1 = enable (if the hardware supports it)

##### r_bilinear
Bilinear texture filtering

* 0 = disable
* 1 = enable

##### r_trilinear
Trilinear texture mipmap filtering

* 0 = disable
* 1 = enable

##### r_mipmaps
Generate mipmaps for textures

* 0 = disable
* 1 = enable

##### r_texcomp
Use texture compression if the hardware supports it

* 0 = disable
* 1 = enable

##### r_texcompcache
Cache compressed textures to disk for use later. This is useful for systems which have slow on-driver texture compression like Intel.

* 0 = disable
* 1 = enable

##### r_texquality
Adjust texture quality

* any value in the range [0.0, 1.0]

##### r_dxt_optimize
Optimize DXT endpoints such that they have a consistent encoding, this helps some hardware and improves on disk compression ratio.

* 0 = disable
* 1 = enable

##### r_fxaa
Fast approximate anti-aliasing

* 0 = disable
* 1 = enable

##### r_ssao
Screen space ambient occlusion

* 0 = disable
* 1 = enable

##### r_parallax
Displacement mapping

* 0 = disable
* 1 = enable


## Client
##### cl_farp
Far clipping plane distance

* any value in the range [128.0, 4096.0]

##### cl_nearp
Near clipping plane distance

* any value in the range [0.0, 10.0]

##### cl_fov
Field of view

* any value in the range [45.0, 270.0]

##### cl_mouse_invert
Invert mouse movements

* 0 = disable
* 1 = enabled

##### cl_mouse_sens
Sensitivity of mouse

* any value in range [0.01, 1.0]


## Texture
##### tex_jpeg_chroma
Chroma upsampling filter for decoding JPEGs

* 0 = bicubic (slow)
* 1 = pixel repetition (fast)

## UI
##### ui_scroll_speed
Mouse scroll speed for UI sliders

* an value in range [1, 10]
