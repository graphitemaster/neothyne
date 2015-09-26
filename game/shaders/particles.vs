uniform mat4 gVP;

in vec3 position;
in vec4 color;
in float power;

out vec2 texCoord0;
out vec4 color0;
out float power0;

void main() {
    power0 = power;
    color0 = color;
    int uv = (0x0FFFF000 >> (8*(gl_VertexID & 3))) & 0xFF;
    texCoord0 = vec2((uv & 0x0F) & 1, ((uv & 0xF0) >> 4) & 1);
    gl_Position = gVP * vec4(position, 1.0f);
}
