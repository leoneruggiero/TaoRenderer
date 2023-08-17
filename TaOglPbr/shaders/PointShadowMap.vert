#version 430 core

#define SHADOWPASS
//! #include "UboDefs.glsl"

layout(location = 0) in vec3 v_position;

void main()
{
    vec4 fragPosWorld = o_modelMat * vec4(v_position, 1.0);

    gl_Position = fragPosWorld; // the view-proj transform is applied in the geometry shader
}