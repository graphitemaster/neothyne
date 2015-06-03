#include <shaders/smaa.h>

in vec3 position;
in vec2 texCoord;

out vec2 texCoord0;
out vec4 offset[2];

out vec4 dummy2; // unused

void main() {
    texCoord0 = texCoord;
    vec4 dummy1 = vec4(0.0f);
    SMAANeighborhoodBlendingVS(dummy1, dummy2, texCoord0, offset);
    gl_Position = mat4(1) * vec4(position, 1.0f);
}
