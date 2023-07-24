#version 430 core
#define GIZMOS_LINES
//! #include "./UboDefs.glsl"
//! #include "./Helper.glsl"

layout(lines) in;
layout(triangle_strip, max_vertices = 4) out;

out GS_OUT
{
    vec4 v_color;
    vec2 v_texCoord;
}gs_out;

in VS_OUT
{
    vec4  v_color;
    bool  v_visible;
}
gs_in[];

void main() {
       
    if(!gs_in[0].v_visible) return;

    vec4 p0_clip = gl_in[0].gl_Position;
    vec4 p1_clip = gl_in[1].gl_Position;

    bool skip = false;

    // Clip against near plane, to avoid problems
    ClipLineOnNear(p0_clip, p1_clip, f_nearFar.x, skip);

    if(skip) return;
   

    // screen space positions
    vec2 p0_ss = p0_clip.xy/p0_clip.w;
    vec2 p1_ss = p1_clip.xy/p1_clip.w;
    p0_ss = (p0_ss * 0.5 + 0.5)*f_viewportSize.xy;
    p1_ss = (p1_ss * 0.5 + 0.5)*f_viewportSize.xy;

    // normal in screen space
    vec2  nrm_ss = p1_ss.xy - p0_ss.xy;
    float len_ss = length(nrm_ss);
    nrm_ss = nrm_ss.xy / len_ss;        // normalize
    nrm_ss = vec2(-nrm_ss.y, nrm_ss.x); // rotate pi/2

    // point0
    vec2 f = o_size * nrm_ss.xy * f_invViewportSize.xy * p0_clip.w;

    gs_out.v_color = gs_in[0].v_color;
    gs_out.v_texCoord = vec2(0.0, 1.0);
    gl_Position = p0_clip + vec4(f, 0.0, 0.0);
    EmitVertex();

    gs_out.v_color = gs_in[0].v_color;
    gs_out.v_texCoord = vec2(0.0, 0.0);
    gl_Position = p0_clip - vec4(f, 0.0, 0.0);;
    EmitVertex();

    // point1
    gs_out.v_texCoord = vec2(0.0);
    f = o_size * nrm_ss.xy * f_invViewportSize.xy * p1_clip.w;

    gs_out.v_color = gs_in[1].v_color;
    gs_out.v_texCoord = vec2(len_ss/o_patternSize, 1.0);
    gl_Position = p1_clip + vec4(f, 0.0, 0.0);
    EmitVertex();

    gs_out.v_color = gs_in[1].v_color;
    gs_out.v_texCoord = vec2(len_ss/o_patternSize, 0.0);
    gl_Position = p1_clip - vec4(f, 0.0, 0.0);
    EmitVertex();

    EndPrimitive();
}