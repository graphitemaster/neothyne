// Most shader paths share this shader (screen space quad-aligned)
in vec2 position;

out vec2 fragCoord;

void main() {
    gl_Position = vec4(position, 0.0f, 1.0f);
    fragCoord = position * 0.5f + 0.5f;
}

