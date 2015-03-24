uniform mat4 gVP;

in vec3 position;
in vec2 texCoord;

out vec2 texCoord0;

void main() {
    gl_Position = gVP * vec4(position, 1.0f);
    texCoord0 = texCoord;
}
