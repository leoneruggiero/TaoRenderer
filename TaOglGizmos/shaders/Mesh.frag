#version 430 core

#define GIZMOS_MESH
//! #include "UboDefs.glsl"

in VS_OUT
{
    vec3 v_position;
    vec3 v_normal;
    vec4 v_color;
    vec2 v_texCoord;
}
fs_in;

out vec4 FragColor;

//[CUSTOM_SHADER]

#ifndef USE_CUSTOM_SHADER
void main()
{
    vec3 eyePos  = f_viewPos.xyz;
    float att    = 0.5;
    float fr     = 1.0 - dot( normalize(fs_in.v_normal) ,normalize(eyePos - fs_in.v_position));

    FragColor    = 
    #ifndef SELECTION
                    vec4(att * vec3(fr) + fs_in.v_color.rgb, fs_in.v_color.a);
    #else
                    fs_in.v_color;
    #endif
}
#endif