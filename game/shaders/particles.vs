uniform mat4 gVP;

in vec3 position;
in vec4 color;

out vec2 texCoord0;
out vec4 color0;

void main() {
    color0 = color;
    //
    // The idea here is to generate one of the following texture coordinates based
    // on the vertex id. Generally we want to generate the following coordinates
    // for every quad: [0,0] [1,0], [1,1], [0,1]. To do this we utilize fract
    // to modulate the vertex id into four possible values "0, 0.25, 0.50, 0.75".
    //
    // Thus for `u', we only need a value of 1 when the value of the modulation is
    // either 0.25 or 0.50. For the `v' case we only need a value of 1 when the value
    // is >= 0.50.
    //
    // Thus, just round (defined as floor(value + 0.5)) will give the right
    // value for `v'. For `u' the process is slightly more involved.
    //
    // Through various experimenting it was observed that floor(fract(value + 0.25) + 0.25)
    // has the following properties:
    //  - 0.75 gets turned into 1.00, which fracts to 0.
    //  - 0.00 gets turned into 0.25, which rounds to 0.
    //  - 0.25 gets turned into 0.50, which rounds to 1.
    //  - 0.50 gets turned into 0.75, which rounds to 1.
    //
    // These properties match that of how we need to generate texture coordinates.
    //
    // This leaves us with:
    //  vec2(floor(fract(val + 0.25) + 0.5), floor(val + 0.5));
    //
    // Or more appropriately:
    //  vec2(round(fract(val + 0.25f)), round(val));
    //
    // Due to strange rounding errors though, the result of this operating does
    // not exactly produce the correct coordinates. So instead we bias the result
    // of the fract by 0.125, leaving us with this slightly scarier, albeit shorter
    // way of calculating texture coordinates:
    //  round(fract(gl_VertexID * 0.25f + vec2(0.125f, 0.375f)))
    //
    texCoord0 = round(fract(gl_VertexID * 0.25f + vec2(0.125f, 0.375f)));
    gl_Position = gVP * vec4(position, 1.0f);
}
