uniform sampler2D gSplashTexture;

in vec2 texCoord0;
out vec4 fragColor;

uniform float gTime;
uniform vec2 gScreenSize;

void main(void) {
    vec2 pixelCoord = (gl_FragCoord.xy / gScreenSize);
    vec3 pixelColor = vec3(0.0f, 0.0f, 0.0f);

    for (int i = 0; i < 25; i++) {
        vec2 particlePosition = vec2(0.5 + 0.30 * sin(gTime + float(i) / 25.0f) * 1.8f,
                                     0.5 + 0.30 * cos(gTime + float(i) / 25.0f) * 1.8f);
        float particleSize = 0.0002f * float(i);
        pixelColor.r += particleSize * (0.5f / length(particlePosition - pixelCoord));
        pixelColor.g += particleSize * (1.5f / length(particlePosition - pixelCoord) * float(i) / 25.0f);
        pixelColor.b += 2.5f * particleSize * (1.5f / length(particlePosition - pixelCoord) * float(i) / 25.0f);
    }

    vec2 scaleCoord = texCoord0.xy + gScreenSize;

    vec4 t0 = texture(gSplashTexture, scaleCoord);
    vec4 t1 = vec4(pixelColor, 1.0f);

    fragColor = mix(t1, t0, t0.a);
}
