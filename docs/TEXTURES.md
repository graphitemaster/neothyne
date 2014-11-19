## Texture
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
