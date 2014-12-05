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
