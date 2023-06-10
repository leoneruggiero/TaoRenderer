#version 430 core

#define GIZOMS_MESH
//! #include "./Include/UboDefs.glsl"

in VS_OUT
{
    vec3 v_position;
    vec3 v_normal;
    vec4 v_color;
    vec2 v_texCoord;
}
fs_in;

out vec4 FragColor;

void main()
{
    vec3 eyePos  = f_viewPos.xyz;
    float att    = 0.5;
    float fr     = 1.0 - dot( normalize(fs_in.v_normal) ,normalize(eyePos - fs_in.v_position));
    FragColor    = att * vec4(fr,fr, fr, 1.0) + fs_in.v_color;
}