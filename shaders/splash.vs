layout (location = 0) in vec4 position;
layout (location = 1) in vec2 texCoord;

out vec2 texCoord0;

void main(void) {
    texCoord0 = texCoord;
    gl_Position = position;
}
