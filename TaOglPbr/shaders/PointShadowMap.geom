#version 430 core

layout(triangles) in;
layout(triangle_strip, max_vertices = 18) out;

uniform vec3 u_lightWorldPos;
uniform mat4 u_viewProjMat[6];

out float v_lightDstWorld;

void main()
{
    for(int f = 0; f<6; f++)
    {
        gl_Layer = f;

        // gl_Position[0-2] is the world-space triangle vertex position.
        vec4 p0w = gl_in[0].gl_Position;
        vec4 p1w = gl_in[1].gl_Position;
        vec4 p2w = gl_in[2].gl_Position;
        vec4 p0 = u_viewProjMat[f] * p0w;
        vec4 p1 = u_viewProjMat[f] * p1w;
        vec4 p2 = u_viewProjMat[f] * p2w;

        gl_Position = p0; v_lightDstWorld = length(p0w.xyz-u_lightWorldPos); EmitVertex();
        gl_Position = p1; v_lightDstWorld = length(p1w.xyz-u_lightWorldPos); EmitVertex();
        gl_Position = p2; v_lightDstWorld = length(p2w.xyz-u_lightWorldPos); EmitVertex();

        EndPrimitive();
    }
}