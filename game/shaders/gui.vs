uniform vec2 gScreenSize;

layout(location = 0) in vec2 position;
layout(location = 1) in vec2 texCoord;
layout(location = 2) in vec4 color;

out vec2 texCoord0;
out vec4 color0;

void main() {
    color0 = color;
    texCoord0 = texCoord;
    gl_Position = vec4(position * 2.0f / gScreenSize - 1.0f, 0.0f, 1.0f);
}
