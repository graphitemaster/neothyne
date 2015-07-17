# Models
Model files contain space-delimited key-value lines which represent a model.
The easiest model contains just a model and diffuse texture. More sophisticated
models are possible with the use of the appropriate key-value associations as
defined in here.

Models can have one material per mesh, or they can have just one material. In the
former case the use of the `material` key must be used to associate a material
with a mesh. The material key can be repeated for how ever many materials the
model requires. In the latter case the use of the material key should be omitted
and instead, the keys for defining a material (as described in `MATERIALS.md`)
should be used directly inline with the model's configuration.

## Keys

##### model
The model file.

##### scale
Scaling of the model. Accepts up to three float values where each value is `x`,
`y` and `z` respectively.

##### rotate
Rotation of the model. Accepts up to three float values where each value is `x`,
`y` and `z` respectively.

##### half
Use half-precision floating-point vertex data. This is useful if the model's
vertices are within applicable range. The engine will load-time convert the
data to half-precision for you. This option is silently ignored if half-precision
is not supported.

##### material
Bind a material file with a mesh in the model. Requires two arguments where the
first one is the mesh name in the model to bind a material to and the last is
the material itself. See `MATERIALS.md` for more information.

##### anim
Animated models may be split up into multiple files to make reusing animations
across multiple models sharing the same skeleton possible. To load those files
this tag must be used. It expects the name of the animated file to load.
Animations are loaded in the order this tag is used.
