uniform mat4 gVP;

in vec3 position;
in vec2 texCoord;
in vec4 color;

out vec2 texCoord0;
out vec4 color0;

void main() {
    color0 = color;
    texCoord0 = texCoord;
    gl_Position = gVP * vec4(position, 1.0f);
}
