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

## Screen capture

##### scr_info
embed system information in screenshots

* 0 = disable
* 1 = enable

##### scr_format
screenshot output format

* 0 = BMP
* 1 = TGA
* 2 = PNG (default)

##### scr_quality
adjust screenshot resolution

* any value in the range [0.0, 1.0]


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

##### r_tex_compress
Use texture compression if the hardware supports it

* 0 = disable
* 1 = enable

##### r_tex_compess_cache
Cache compressed textures to disk for use later. This is useful for systems which
have slow on-driver texture compression like Intel.

* 0 = disable
* 1 = enable

##### r_tex_compress_cache_zlib
When caching of compressed textures to disk is enabled, this option will compress
the texture data with deflate for smaller files on disk.

* 0 = disable
* 1 = enable

##### r_texquality
Adjust texture quality

* any value in the range [0.0, 1.0]

##### r_dxt_optimize
Optimize DXT endpoints such that they have a consistent encoding, this helps some
hardware and improves on disk compression ratio.

* 0 = disable
* 1 = enable

##### r_fxaa
Fast approximate anti-aliasing

* 0 = disable
* 1 = enable

##### r_parallax
Displacement mapping

* 0 = disable
* 1 = enable

##### r_ssao
Screen space ambient occlusion

* 0 = disable
* 1 = enable

##### r_spec
Specular highlights

* 0 = disable
* 1 = enable

##### r_fog
Fog

* 0 = disable
* 1 = enable

##### r_smsize
Shadow map resolution

* any value in range [16, 4096]

##### r_smborder
Shadow map border

* any value in range [0, 8]

##### r_vignette
Vignette

* 0 = disable
* 1 = enable

##### r_vignette_radius
Radius of vignette

* any value in range [0.25, 1.0f]

##### r_vignette_softness
Softness factor for vignette

* any value in range [0.0, 1.0]

##### r_vignette_opacity
Opacity for vignette

* any value in range [0.0, 1.0]

##### r_smbias
Shadow map bias

* any value in range [-10.0,  10.0]

##### r_smpolyfactor
Shadow map polygon offset factor (for slope dependent bias)

* any value in range [-1000.0, 1000.0]

##### r_smpolyoffset
Shadow map polygon offset units (for slope dependent bias)

* any value in range [-1000.0, 1000.0]

##### r_debug
Debug visualizations of various renderer buffers

* 0 = no visualization
* 1 = depth buffer
* 2 = normal buffer
* 3 = position buffer
* 4 = ambient occlusion buffer

##### r_stats
Debug memory statistics for the world

* 0 = disable
* 1 = enable

##### r_debug_tex
The engine is allowed free reign to resize, reformat and adjust
textures at runtime to be suitable for rendering. This information becomes
visible in the top right hand corner of the respective texture when enabled.

* 0 = disable
* 1 = enable

The texture size presented in debug mode is an estimate of how much
video memory the texture takes. The actual usage may vary depending on
padding, alignment and compression. When texture compression is enabled
and supported by the system the actual cost can be as little as 1/16th
of what is reported.

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
##### tex_jpg_chroma
Chroma upsampling filter for decoding JPEGs

* 0 = bicubic (slow)
* 1 = pixel repetition (fast)

##### tex_tga_compress
RLE compression for saving TGAs

* 0 = disabled
* 1 = enabled

##### tex_png_compress_quality
Quality of PNG file size. This adjusts the deflate algorithm to allow for more
opportunities to compress further at the cost of speed. Higher values produce
smaller files.

* any value in range [5, 128]


## UI
##### ui_scroll_speed
Mouse scroll speed for UI sliders

* any value in range [1, 10]


## Map
##### map_fog_density
Map fog density

* any value in range [0, 1]

##### map_fog_color
Map fog color

* any value in range [0, 0x00FFFFFF]

##### map_fog_equation
Map fog equation

* 0 = Linear
* 1 = Exp
* 2 = Exp2

##### map_fog_range_start
Map fog range start (for linear only)

* any value in range [0, 1]

##### map_fog_range_end
Map fog range end (for linear only)

* any value in range [0, 1]

##### map_dlight_color
Map directional light color

* any value in range [0, 0x00FFFFFF]

##### map_dlight_ambient
Map directional light ambient term

* any value in range [0, 1]

##### map_dlight_diffuse
Map directional light diffuse term

* any value in range [0, 1]
