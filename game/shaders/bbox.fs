layout (location = 0) out vec4 diffuseOut;

uniform vec3 gColor;

void main() {
    diffuseOut = vec4(gColor, 1.0f);
}
