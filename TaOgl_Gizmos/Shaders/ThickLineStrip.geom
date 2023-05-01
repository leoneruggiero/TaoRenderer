#version 430 core
#define GIZMOS_LINES
//! #include "./Include/UboDefs.glsl"

layout(lines_adjacency) in;
layout(triangle_strip, max_vertices = 4) out;

out GS_OUT
{
    vec4 v_color;
    vec2 v_texCoord;
}gs_out;

void main() {

    gs_out.v_color    = vec4(0.0);
    gs_out.v_texCoord = vec2(0.0);

    float f = o_size * gl_in[1].gl_Position.w;
        
    // Normal to the previous segment (adj)
    vec2 n0_ss = 
                (gl_in[1].gl_Position.xy/gl_in[1].gl_Position.w) - 
                (gl_in[0].gl_Position.xy/gl_in[0].gl_Position.w);

    n0_ss = normalize((n0_ss.xy / f_invViewportSize.xy));
    n0_ss = vec2(-n0_ss.y, n0_ss.x);

    // Normal to the segment
    vec2 n1_ss = 
                (gl_in[2].gl_Position.xy/gl_in[2].gl_Position.w) - 
                (gl_in[1].gl_Position.xy/gl_in[1].gl_Position.w);

    n1_ss = normalize((n1_ss.xy / f_invViewportSize.xy));
    n1_ss = vec2(-n1_ss.y, n1_ss.x);

    // Normal to the next segment (adj)
    vec2 n2_ss = 
                (gl_in[3].gl_Position.xy/gl_in[3].gl_Position.w) - 
                (gl_in[2].gl_Position.xy/gl_in[2].gl_Position.w);

    n2_ss = normalize((n2_ss.xy / f_invViewportSize.xy));
    n2_ss = vec2(-n2_ss.y, n2_ss.x);

    // miter screen space
    vec2 m0_ss = normalize(n0_ss + n1_ss);
    vec2 m1_ss = normalize(n1_ss + n2_ss);

    // lower limit for the dot product to prevent miters from being too long
    float l1_ss = o_size/max(dot(m0_ss, n0_ss), 0.5);
    float l2_ss = o_size/max(dot(m1_ss, n1_ss), 0.5);


    vec4 m0_ndc = gl_in[1].gl_Position + l1_ss * (vec4(m0_ss * f_invViewportSize, 0, 0) * gl_in[1].gl_Position.w);
    vec4 m1_ndc = gl_in[1].gl_Position - l1_ss * (vec4(m0_ss * f_invViewportSize, 0, 0) * gl_in[1].gl_Position.w);
    vec4 m2_ndc = gl_in[2].gl_Position + l2_ss * (vec4(m1_ss * f_invViewportSize, 0, 0) * gl_in[2].gl_Position.w);
    vec4 m3_ndc = gl_in[2].gl_Position - l2_ss * (vec4(m1_ss * f_invViewportSize, 0, 0) * gl_in[2].gl_Position.w);

    gl_Position = l1_ss > 0
                    ? m0_ndc
                    : m1_ndc;
    EmitVertex();

    gl_Position = l1_ss > 0
                    ? m1_ndc
                    : m0_ndc;
    EmitVertex();
        
    gl_Position = l2_ss > 0
                    ? m2_ndc
                    : m3_ndc;
    EmitVertex();
   
    gl_Position = l2_ss > 0
                    ? m3_ndc
                    : m2_ndc;
    EmitVertex();

    EndPrimitive();
}