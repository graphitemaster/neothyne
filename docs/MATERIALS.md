# Materials
Material files contain space-delimited key-value lines which represent a material.
The easiest material contains just a diffuse texture. More sophisticated materials
are possible with the use of the appropriate key-value associations.

## Keys

##### diffuse
A diffuse texture

##### normal
A tangent space normal map texture. Tangent space normals assume 0.5 is a
null component, 0.0 is a -1 component and 1.0 is a +1 component. Where
RGB is XYZ respectively. The engine ignores the blue component since it can
be derived from R and G.

##### spec
A local space specularity map. Local space specularity maps assume that the
R component consists of the power term for specularity, while the G component
consists of the intensity term.

##### specparams
The following key cannot be used in conjunction with spec. This gives the entire
material the following specular power and intensity for all pixels.

##### displacement
A height map used for displacement mapping.

##### parallax
Scale and bias used for displacement mapping. Bias is optional.

##### animation
Materials can be animated by having the textures contain any number of frames
both horizontally and vertically in the textures. The animate key takes four
arguments with two optional arguments: Width of a frame, height of a
frame, rate at which to cycle through the frames (in fps) and the total amount of
frames to read from the textures to form an animation.

If the math doesn't work out the engine will fail to load the animated material.

The two optional arguments are used to flip the texture coordinates along the
S and T axis respectively.

For instance if you have a texture with a total size of 1024x1024 you could
store four frames each 256x256 in size horizontally and vertically. Giving you
a total of 16 frames. The engine does not care how many frames you have
horizontally or vertically, so long as the width of a frame is modulo the
full texture width. When it reaches the end of a row it will move to the next
row in the texture.

For good looking animations with high framerates you'll need larger textures,
but keep in mind the engine will not load textures larger than 4096x4096 in size.
With a 4096x4096 texture you could store 1024 128x128 frames. At a playback rate
of 24fps that gives you a total of 42 seconds.

Lower resolution textures don't look that great if they're going to be used on
large surfaces so you may want your frames to be no less than 256x256 in which
case your theoretical maximum duration would be 10 seconds at 24fps.

You can experiment with lower or higher playback rates however 24fps is just
fast enough to be acceptably convincing when played back and maximizes the
duration of your animations.

Animated materials are not meant to be videos so don't use them as such. They're
meant to make interesting looking textures like the jump pads in Q3A.

##### colorize
Hexadecimal RGBA color to colorize (modulate) the texture by.

##### nodebug
The following flag disables the rendering of debug information into the
textures associated with the material for when `r_debug_tex` is enabled.

## Tags
The texture value for a material key can be prefixed with a series of tags. A
tag is a way of indicating a task for which the engine will interpret and carry
out on the texture before handing it off to the GPU.

The following tags exist:

##### normal
Eliminate the blue component of the texture file, preserving only red and
green (for normal maps.) This tag is implicit with the use of the `normal'
material key.

##### grey
Reduce the texture to greyscale. This tag is implicit with the use of the
`displacement' material key.

##### nocompress
Prevents the engine from compressing the texture. This may be desired when working
with small detailed textures of which you want to preserve the detail.

##### premul
Premultiply alpha.
