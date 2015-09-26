# Developers
Documentation about engine components can be found in here

## Organization
The source code is organized into namespaces and a file name convention.
For example, anything prefixed with `u_` belongs to the *utility* namespace,
which is also named `u`. The following namespaces and file prefixes exist:

| Prefix | Namespace | Description |
|--------|-----------|-------------|
| u_     | u         | utility     |
| m_     | m         | math        |
| r_     | r         | renderer    |

## Utility library
Neothyne doesn't use the C++ standard library, instead it rolls its own
implementation of common containers. They're not meant to be compatible with the
standard in any way, even if they share a similar API. The following containers
exist.

* u::buffer
* u::lru
* u::map
* u::optional
* u::pair
* u::set
* u::stack
* u::string
* u::vector

As well as a plethora of type traits and algorithmic functions which can be
found in `u_traits.h` and `u_algorithm.h` respectively. Miscellaneous utilites can
be found in `u_misc.h`.

## Strings
String in Neothyne are allocated using a special string heap with a fixed size.
A custom allocator is used for allocating strings which allocates from this fixed
size heap. The allocation strategy is buddy allocation and will always make blocks
of memory for strings of size 8, 16, 32, 64, 128, 256, ...

## Shaders

Shaders in Neothyne are written in GLSL with a bit of syntax sugar.

Neothyne will emit a prelude to each shader containing the version of GLSL to
utilize as well as things like `HAS_TEXTURE_RECTANGLE` if the GPU supports that
feature. You can find a list of the macros it emits by searching for
`method::define` in the code-base.

Neothyne supports the `#include` directive in GLSL; similarly, Neothyne will emit
uniform guards to prevent declaring uniforms more than once. This is useful as
headers typically declare uniforms throughout.

## Rendering pipeline

Each frame is broken into five distinctive passes. The passes are only responsible
for rendering the scene. The scene does not include the UI. The UI pass is applied
as a layer on the final window back buffer. The passes are as follows:

#### Compile & Cull pass

The compile and cull pass is in charge of compiling the information required
to render the frame as well as culling anything not within the viewing frustum.

Currently this involves running through all spot and point lights in the scene
and checking if they're visible. When they are visible, they are hashed and
compared against their previous hashed value to see if any changes were made
to them. If there are changes present and the light is set to cast shadows, the
world geometry is traversed to calculate which indices are responsible for the
vertices that are within that light's contributing light sphere. These indices
are used when rendering the geometry for the world into the shadow map. When
the light is a point light, the vertices are checked against a shadow frustum
to determine which faces they are present in. Six separate indices counts are
kept per frustum side in this case.

To prevent calculating the transforms required to render the shadow map every frame,
this pass also calculates the world-view-projection matrix to render the shadow
map only when the light's properties have changed.

#### Geometry pass

The geometry pass renders into a frame buffer with two texture attachments and
a depth-stencil attachment which are used to store diffuse, normal and spec information.

All geometry is rendered here including map models.

Ambient occlusion also happens during this pass on a separate FBO.

Some things shouldn't contribute to ambient occlusion so we draw those elements
to our geometry buffer as well by way of stencil replace.

Special care was taken to keep the geometry buffer small in size to reduce overall
bandwidth.

The geometry buffer is setup as follows (both attachments are RGBA8)
```
 R          G          B          A
[diffuse R][diffuse G][diffuse B][spec power]
[normals R][normals G][normals B][spec intensity]
```

Position is calculated in later stages from depth by using an inverse
world-view-projection matrix.

#### Lighting pass

Lighting pass begins by outputting to a final composite FBO with blending enabled.

First the point lights are rendered with a sphere of appropriate radius for each
light, taking special care to change the culling order of whether the observer
is inside the sphere or not.

The precalculated world-view-projection matrix is used to render into a shadow
map the depth from the light's perspective. The precalculated indices list during
the compile and cull pass are used here to reduce the amount of geometry rendered.
Rendering of the shadow map itself uses polygon offset to do slope-dependent bias.

The result of the shadow map is used immediately after it's available during the
lighting pass. There is only one shadow map, this is to keep the memory requirements
low. No batching or caching of shadow maps is done.

To make use of throughput an unusual PCF algorithm is used when sampling shadow
maps which makes sure the distance between taps is 4 unique pixels and that
they're correctly weighted by the area they take up in the final average.
The filter is listed here:
```
vec2 scale = 1.0f / textureSize(gShadowMap, 0);
shadowCoord.xy *= textureSize(gShadowMap, 0);
vec2 offset = fract(shadowCoord.xy - 0.5f);
shadowCoord.xy -= offset*0.5f;
vec4 size = vec4(offset + 1.0f, 2.0f - offset);
return (1.0f / 9.0f) * dot(size.zxzx*size.wwyy,
    vec4(texture(gShadowMap, vec3(scale * (shadowCoord.xy + vec2(-0.5f, -0.5f)), shadowCoord.z)),
         texture(gShadowMap, vec3(scale * (shadowCoord.xy + vec2(1.0f, -0.5f)), shadowCoord.z)),
         texture(gShadowMap, vec3(scale * (shadowCoord.xy + vec2(-0.5f, 1.0f)), shadowCoord.z)),
         texture(gShadowMap, vec3(scale * (shadowCoord.xy + vec2(1.0f, 1.0f)), shadowCoord.z))));
```

The same technique is used for rendering the spot lights as well.

Finally the directional lighting is rendered in two passes of its own. The first
pass does directional lighting without stencil test. The second pass uses stencil
so that the things specifically rendered without AO don't get AO applied to them.

#### Forward pass

The forward pass is responsible for rendering everything else that simply cannot
be rendered with the deferred lighting design. This includes the skybox, billboards
and particles.

First the skybox is rendered. Special care is kept to ignore the background color
in blending since it contains fog contribution from directional lighting. Instead
the skybox is independently fogged with a gradient to provide a coherent effect
of world geometry reaching into the skybox.

Editing aids (bounding boxes, billboards, etc) are rendered next. We take
special care to ensure anything with transparency during this has premultipled alpha
to prevent strange blending errors around transparent edges.

Particles are rendered next using an unusual vertex format void of texture
coordinates to reduce bandwidth costs of uploading them every frame. Since particles
in Neothyne are rendered as billboarded quads, the texture coordinates are always
```
[0, 0]
[1, 0]
[1, 1]
[0, 1]
```
The coordinates are thus encoded as a single integer constant where each byte
represents a coordinate pair (each nibble represents U and V respectively.)
On little endian GPUs this constant is `0x0FFFF00`.

Thus the coordinates can be calculated by using `gl_VertexID & 3` as a byte
index into the constant. The code is listed here for posterity's sake:
```
int uv = (0x0FFFF000 >> (8*(gl_VertexID & 3))) & 0xFF;
texCoord0 = vec2((uv & 0x0F) & 1, ((uv & 0xF0) >> 4) & 1);
```

Each vertex has its own exponent for the power-term in soft-particle rendering.
The depth buffer from the geometry pass is bound during particle rendering and
sampled to make soft-particles possible.
#### Composite pass

The composite pass is responsible for applying color grading to the final result
as well as optional anti-aliasing. It outputs to the window back buffer.
