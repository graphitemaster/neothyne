# Models
Model files contain space-delimited key-value lines which represent a model.
The easiest model contains just a model and diffuse texture. More sophisticated
models are possible with the use of the appropriate key-value associations as
defined in here and `MATERIALS.md'.

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
