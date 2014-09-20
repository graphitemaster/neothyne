layout (points) in;
layout (triangle_strip) out;
layout (max_vertices = 4) out;

uniform mat4 gVP;
uniform vec3 gCameraPosition;
uniform vec2 gSize;

out vec2 texCoord0;

void main(void) {
    vec3 position = gl_in[0].gl_Position.xyz;
    vec3 toCamera = normalize(gCameraPosition - position);
    vec3 up = vec3(0.0f, 1.0f, 0.0f);
    vec3 right = cross(toCamera, up) * gSize.x;

    position -= right / 2.0f;
    position.y -= gSize.y / 2.0f;
    gl_Position = gVP * vec4(position, 1.0f);
    texCoord0 = vec2(0.0f, 1.0f);
    EmitVertex();

    position.y += gSize.y;
    gl_Position = gVP * vec4(position, 1.0f);
    texCoord0 = vec2(0.0f, 0.0f);
    EmitVertex();

    position.y -= gSize.y;
    position += right;
    gl_Position = gVP * vec4(position, 1.0f);
    texCoord0 = vec2(1.0f, 1.0f);
    EmitVertex();

    position.y += gSize.y;
    gl_Position = gVP * vec4(position, 1.0f);
    texCoord0 = vec2(1.0f, 0.0f);
    EmitVertex();

    EndPrimitive();
}
