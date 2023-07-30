#version 430 core

#define GBUFF_READ

//! #include "GPassHelper.glsl"

layout(location = 0) in vec2 v_position;

void main()
{
    gl_Position = vec4(v_position.xy, 0.0f,  1.0);
}