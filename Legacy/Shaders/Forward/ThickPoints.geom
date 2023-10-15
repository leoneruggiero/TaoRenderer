#version 430 core

layout(points) in;
layout(triangle_strip, max_vertices = 4) out;

// 1.0 / screen size S
uniform vec2    f_screenToWorld;
uniform float    o_thickness;

void main() {

    float f = o_thickness;
    f *= gl_in[0].gl_Position.w;        

    // Top - Right
    gl_Position = gl_in[0].gl_Position + f * vec4(f_screenToWorld, 0.0, 0.0);
    EmitVertex();

    // Bottom - Right
    gl_Position = gl_in[0].gl_Position + f * vec4(f_screenToWorld.x, - f_screenToWorld.y, 0.0, 0.0);
    EmitVertex();

    // Top - Left
    gl_Position = gl_in[0].gl_Position + f * vec4(-f_screenToWorld.x, f_screenToWorld.y, 0.0, 0.0);
    EmitVertex();

    // Bottom - Left          
    gl_Position = gl_in[0].gl_Position + f * vec4(-f_screenToWorld, 0.0, 0.0);
    EmitVertex();

    EndPrimitive();
}