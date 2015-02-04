in vec2 texCoord0;

uniform sampler2D gColorMap;

out vec4 fragColor;

void main() {
    fragColor = texture2D(gColorMap, texCoord0);
}
