## Texture
Neothyne rolls its own texture loaders, the support of each loader is specified
below:

* JPEG (baseline)
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

* PNM (fully supported)
  * Supports ASCII (P1) and binary (P4) PBM (Portable BitMap)
  * Supports ASCII (P2) and binary (P5) PGM (Portable GrayMap)
  * Supports ASCII (P3) and binary (P6) PPM (Portable PixMap)

* PCX (8-bit indexed or 24-bit RGB (RLE only))
  * Supports 8-bit indexed
  * Supports 24-bit RGB

* DDS
  * Supports the following block-compressed formats only
    * DXT1
    * DXT3
    * DXT5
    * BC4U
    * BC4S
    * BC5U
    * BC5S
    * ATI1
    * ATI2

In addition to those supported texture formats. Neothyne also builds BPTC, DXT1,
DXT5 and RGTC compressed textures at runtime and caches them to disk. It will
prefer the cached textures if the hardware supports BPTC, S3TC or RGTC texture
compression.
