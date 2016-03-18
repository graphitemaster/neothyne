in vec3 position;
in vec2 texCoord;

uniform mat4 gWVP;
uniform mat4 gWorld;

out vec2 texCoord0;
out vec3 position0;
out vec3 worldPos0;

void main() {
    position0 = normalize(position);
    texCoord0 = texCoord;
    gl_Position = (gWVP * vec4(position, 1.0f)).xyww;
    worldPos0 = (gWorld * vec4(position, 1.0f)).xyz;
}

