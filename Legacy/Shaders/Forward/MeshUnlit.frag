#version 430 core
    
//! #include "../Include/UboDefs.glsl"

in VS_OUT
{
    vec3 worldNormal;
    vec3 fragPosWorld;
    vec2 textureCoordinates;
}fs_in;

out vec4 FragColor;

void main()
{
    FragColor = o_material.Albedo;
}