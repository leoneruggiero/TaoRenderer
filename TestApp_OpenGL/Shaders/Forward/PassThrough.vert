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
    gl_Position =  o_modelMat * vec4(position, 1.0);

}