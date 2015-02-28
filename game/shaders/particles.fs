in vec2 texCoord0;
in vec4 color0;

uniform sampler2D gColorMap;

out vec4 fragColor;

void main() {
    fragColor = color0 * texture(gColorMap, texCoord0);
}
