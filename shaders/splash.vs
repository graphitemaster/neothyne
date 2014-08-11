layout (location = 0) in vec4 position;
layout (location = 1) in vec2 texCoord;

out vec2 texCoord0;

uniform float gAspectRatio;

void main(void) {
    texCoord0 = texCoord;

    // preserve aspect ratio
    mat3 scale = mat3(
        vec3(gAspectRatio, 0.0f, 0.0f),
        vec3(0.0f,         1.0f, 0.0f),
        vec3(0.0f,         0.0f, 1.0f)
    );

    gl_Position = vec4(scale * vec3(position.xy, 0.0f), 1.0f);
}
