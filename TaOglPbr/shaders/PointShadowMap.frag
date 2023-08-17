#version 430 core

in float v_lightDstWorld;
out vec4 outCol;

void main()
{
    outCol = vec4(v_lightDstWorld);
}