in vec3 position;
in vec3 normal;
in vec2 texCoord;
in vec3 tangent;
in float w;

uniform mat4 gWVP;
uniform mat4 gWorld;

out vec2 texCoord0;
out vec3 normal0;
out vec3 worldPosition0;

void main() {
    gl_Position = gWVP * vec4(position, 1.0f);
    texCoord0 = texCoord;
    normal0 = (gWorld * vec4(normal, 0.0f)).xyz;
    worldPosition0 = (gWorld * vec4(position, 1.0f)).xyz;
}
