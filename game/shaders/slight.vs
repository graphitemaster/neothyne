in vec3 position;

uniform mat4 gWVP;

#ifdef USE_SHADOWMAP
uniform mat4 gLightWVP;

out vec4 lightSpacePosition0;
#endif

void main() {
    gl_Position = gWVP * vec4(position, 1.0f);
#ifdef USE_SHADOWMAP
    lightSpacePosition0 = gLightWVP * vec4(position, 1.0f);
#endif
}

