#version 430 core

//! #include "../Include/UboDefs.glsl"

layout(location = 0) in vec3 position;
layout(location = 1) in vec3 normal;
layout(location = 2) in vec2 textureCoordinates;

out VS_OUT
{
    vec3 worldNormal;
    vec3 fragPosWorld;
    vec2 textureCoordinates;
}vs_out;

void main()
{
    
    vs_out.fragPosWorld = normalize(position);

    gl_Position = (f_projMat * o_modelMat * vec4(position.xyz , 1.0)).xyww;
}