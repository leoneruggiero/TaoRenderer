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
    gl_Position = f_projMat * f_viewMat * o_modelMat * vec4(position, 1.0);

    vs_out.fragPosWorld = (o_modelMat * vec4(position, 1.0)).xyz;
    vs_out.worldNormal = normalize((o_normalMat * vec4(normal, 0.0f)).xyz);
    vs_out.textureCoordinates = textureCoordinates;
}