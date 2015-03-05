uniform sampler2D gColorMap;

in vec2 texCoord0;

out vec4 fragColor;

void main() {
    fragColor = texture(gColorMap, texCoord0);
}
