#version 430 core

layout(lines) in;
layout(triangle_strip, max_vertices = 4) out;

// 1.0 / screen size 
uniform vec2  f_screenToWorld;
uniform float  o_thickness;

void main() {
        
    // Normal in screen space
    vec2 n0_ss = 
                (gl_in[1].gl_Position.xy/gl_in[1].gl_Position.w) - 
                (gl_in[0].gl_Position.xy/gl_in[0].gl_Position.w);

    n0_ss = normalize((n0_ss.xy / f_screenToWorld.xy));
    n0_ss = vec2(-n0_ss.y, n0_ss.x);

    float f = o_thickness * gl_in[0].gl_Position.w;

    gl_Position = gl_in[0].gl_Position + f * vec4(n0_ss * f_screenToWorld, 0.0, 0.0);
    EmitVertex();

    gl_Position = gl_in[0].gl_Position - f * vec4(n0_ss * f_screenToWorld, 0.0, 0.0);
    EmitVertex();

    f = o_thickness * gl_in[1].gl_Position.w;

    gl_Position = gl_in[1].gl_Position + f * vec4(n0_ss * f_screenToWorld, 0.0, 0.0);
    EmitVertex();

    gl_Position = gl_in[1].gl_Position - f * vec4(n0_ss * f_screenToWorld, 0.0, 0.0);
    EmitVertex();

    EndPrimitive();
}