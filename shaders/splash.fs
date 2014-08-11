uniform sampler2D gSplashTexture;

in vec2 texCoord0;

out vec4 fragColor;

void main(void) {
    fragColor = texture(gSplashTexture, texCoord0);
}
