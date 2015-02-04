layout (location = 0) in vec3 position;
layout (location = 1) in vec3 normal;
layout (location = 2) in vec2 texCoord;
layout (location = 3) in vec3 tangent;
layout (location = 4) in float w;

uniform mat4 gWVP;

out vec2 texCoord0;

void main() {
    gl_Position = gWVP * vec4(position, 1.0f);
    texCoord0 = texCoord;
}
