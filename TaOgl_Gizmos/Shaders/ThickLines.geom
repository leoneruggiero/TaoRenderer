#version 430 core
#define GIZMOS_LINES
//! #include "./Include/UboDefs.glsl"

layout(lines) in;
layout(triangle_strip, max_vertices = 4) out;

out GS_OUT
{
    vec4 v_color;
}
gs_out;

in VS_OUT
{
    vec4  v_color;
}
gs_in[];

void main() {
        
    gs_out.v_color = gs_in[0].v_color;

    // Normal in screen space
    vec2 n0_ss = 
                (gl_in[1].gl_Position.xy/gl_in[1].gl_Position.w) - 
                (gl_in[0].gl_Position.xy/gl_in[0].gl_Position.w);

    n0_ss = normalize((n0_ss.xy / f_invViewportSize.xy));
    n0_ss = vec2(-n0_ss.y, n0_ss.x);

    float f = o_size * gl_in[0].gl_Position.w;

    gl_Position = gl_in[0].gl_Position + f * vec4(n0_ss * f_invViewportSize, 0.0, 0.0);
    EmitVertex();

    gl_Position = gl_in[0].gl_Position - f * vec4(n0_ss * f_invViewportSize, 0.0, 0.0);
    EmitVertex();

    f = o_size * gl_in[1].gl_Position.w;

    gl_Position = gl_in[1].gl_Position + f * vec4(n0_ss * f_invViewportSize, 0.0, 0.0);
    EmitVertex();

    gl_Position = gl_in[1].gl_Position - f * vec4(n0_ss * f_invViewportSize, 0.0, 0.0);
    EmitVertex();

    EndPrimitive();
}