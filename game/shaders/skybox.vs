in vec3 position;

uniform mat4 gWVP;
uniform mat4 gWorld;

out vec3 texCoord0; // 3d texel fetch
out vec3 worldPos0;

void main() {
    texCoord0 = normalize(position);
    gl_Position = (gWVP * vec4(position, 1.0f)).xyww;
    worldPos0 = (gWorld * vec4(position, 1.0f)).xyz;
}

