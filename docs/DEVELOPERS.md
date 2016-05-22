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
| a_     | a         | audio       |

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

Neothyne supports the `#include` directive in GLSL.

If supported, Neothyne will fetch compiled program binaries and cache them disk
for subsequent launches to reduce startup costs associated with processing,
compiling and linking source.

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
are used when rendering the geometry for the world into the shadow map.

When the light is a point light, each triangle in the light sphere is checked to
see which plane the triangle is on in the viewing frustum. The triangles are then
outputted to six separate indices lists. This is done using masks though as
triangles can be in multiple frustum planes at once. The code for this is listed
here to give the reader a better understanding of what is involved:
```
static uint8_t calcTriangleSideMask(const m::vec3 &p1,
                                    const m::vec3 &p2,
                                    const m::vec3 &p3,
                                    float bias)
{
    // p1, p2, p3 are in the cubemap's local coordinate system
    // bias = border/(size - border)
    uint8_t mask = 0x3F;
    float dp1 = p1.x + p1.y, dn1 = p1.x - p1.y, ap1 = m::abs(dp1), an1 = m::abs(dn1),
          dp2 = p2.x + p2.y, dn2 = p2.x - p2.y, ap2 = m::abs(dp2), an2 = m::abs(dn2),
          dp3 = p3.x + p3.y, dn3 = p3.x - p3.y, ap3 = m::abs(dp3), an3 = m::abs(dn3);
    if (ap1 > bias*an1 && ap2 > bias*an2 && ap3 > bias*an3)
        mask &= (3<<4)
            | (dp1 < 0 ? (1<<0)|(1<<2) : (2<<0)|(2<<2))
            | (dp2 < 0 ? (1<<0)|(1<<2) : (2<<0)|(2<<2))
            | (dp3 < 0 ? (1<<0)|(1<<2) : (2<<0)|(2<<2));
    if (an1 > bias*ap1 && an2 > bias*ap2 && an3 > bias*ap3)
        mask &= (3<<4)
            | (dn1 < 0 ? (1<<0)|(2<<2) : (2<<0)|(1<<2))
            | (dn2 < 0 ? (1<<0)|(2<<2) : (2<<0)|(1<<2))
            | (dn3 < 0 ? (1<<0)|(2<<2) : (2<<0)|(1<<2));

    dp1 = p1.y + p1.z, dn1 = p1.y - p1.z, ap1 = fabs(dp1), an1 = fabs(dn1),
    dp2 = p2.y + p2.z, dn2 = p2.y - p2.z, ap2 = fabs(dp2), an2 = fabs(dn2),
    dp3 = p3.y + p3.z, dn3 = p3.y - p3.z, ap3 = fabs(dp3), an3 = fabs(dn3);
    if (ap1 > bias*an1 && ap2 > bias*an2 && ap3 > bias*an3)
        mask &= (3<<0)
            | (dp1 < 0 ? (1<<2)|(1<<4) : (2<<2)|(2<<4))
            | (dp2 < 0 ? (1<<2)|(1<<4) : (2<<2)|(2<<4))
            | (dp3 < 0 ? (1<<2)|(1<<4) : (2<<2)|(2<<4));
    if (an1 > bias*ap1 && an2 > bias*ap2 && an3 > bias*ap3)
        mask &= (3<<0)
            | (dn1 < 0 ? (1<<2)|(2<<4) : (2<<2)|(1<<4))
            | (dn2 < 0 ? (1<<2)|(2<<4) : (2<<2)|(1<<4))
            | (dn3 < 0 ? (1<<2)|(2<<4) : (2<<2)|(1<<4));

    dp1 = p1.z + p1.x, dn1 = p1.z - p1.x, ap1 = fabs(dp1), an1 = fabs(dn1),
    dp2 = p2.z + p2.x, dn2 = p2.z - p2.x, ap2 = fabs(dp2), an2 = fabs(dn2),
    dp3 = p3.z + p3.x, dn3 = p3.z - p3.x, ap3 = fabs(dp3), an3 = fabs(dn3);
    if (ap1 > bias*an1 && ap2 > bias*an2 && ap3 > bias*an3)
        mask &= (3<<2)
            | (dp1 < 0 ? (1<<4)|(1<<0) : (2<<4)|(2<<0))
            | (dp2 < 0 ? (1<<4)|(1<<0) : (2<<4)|(2<<0))
            | (dp3 < 0 ? (1<<4)|(1<<0) : (2<<4)|(2<<0));
    if (an1 > bias*ap1 && an2 > bias*ap2 && an3 > bias*ap3)
        mask &= (3<<2)
            | (dn1 < 0 ? (1<<4)|(2<<0) : (2<<4)|(1<<0))
            | (dn2 < 0 ? (1<<4)|(2<<0) : (2<<4)|(1<<0))
            | (dn3 < 0 ? (1<<4)|(2<<0) : (2<<4)|(1<<0));

    return mask;
}
```

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
in Neothyne are rendered as billboarded quads, the texture coordinates are always:
```
[0, 0]
[1, 0]
[1, 1]
[0, 1]
```

To do this `fract` is used to modulate `gl_VertexID` into four possible values:
`0, 0.25, 0.50, 0.75`.

For `u`, we only need a value of `1` when the result of the modulation is either
`0.25` or `0.50`. For `v` we only need a value of `1` when the value of the
modulation is `>= 0.50`.

Thus, just `round` (defined as `floor(value + 0.5)`) will give the right value
for `v`. To get `u` the process is slightly more involved.

Through various experimenting it was observed that `floor(fract(value + 0.25) + 0.25)`
has the following properties:
* `0.75` gets turned into `1.00`, which `fract` turns to `0.00`.
* `0.00` gets turned into `0.25`, which `round` turns to `0.00`.
* `0.25` gets turned into `0.50`, which `round` turns to `1.00`.
* `0.50` gets turned into `0.75`, whcih `round` turns to `1.00`.

These properties match that of how we need to generate the texture coordinates,
leaving us with the following:
```
vec2(floor(fract(value + 0.25) + 0.50), floor(value + 0.50))
```

Or more appropriately:
```
vec2(round(fract(value + 0.25)), round(value))
```

However, due to strange rounding errors, the result of this operation does not
exactly produces the correct coordinates. So to compensate, we bias the
result of the `fract` by `0.125`, leaving us with this slightly scarier, albeit
shorter way of calculating the texture coordinates:
```
round(fract(gl_VertexID * 0.25 + vec2(0.125, 0.375)))
```

Each vertex has its own exponent for the power-term in soft-particle rendering.
The depth buffer from the geometry pass is bound during particle rendering and
sampled to make soft-particles possible.

#### Composite pass

The composite pass is responsible for applying color grading to the final result
as well as optional anti-aliasing. It outputs to the window back buffer.
