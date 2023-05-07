#version 430 core
#define GIZMOS_POINTS
//! #include "./Include/UboDefs.glsl"

layout(points) in;
layout(triangle_strip, max_vertices = 4) out;

in VS_OUT
{
    vec4  v_color;
    vec4  v_texCoord; // rect, .xy = bl, .zw = tr 
}
vs_out[];

out GS_OUT
{
    vec4 v_color;
    vec2 v_texCoord;
}gs_out;

void main() {

    vec2 f = vec2(o_size);
    f = 2.0* f * f_invViewportSize.xy * gl_in[0].gl_Position.w;        
    
    vec4 c = gl_in[0].gl_Position;
    if(o_snap) // snap point center to nearest pixel
    {
        c.xy = round((c.xy/c.w*0.5+0.5)*f_viewportSize.xy);
        c.xy*= f_invViewportSize.xy;
        c.xy = c.xy*2-1;
        c.xy*= c.w;
    }

    gs_out.v_color     = vs_out[0].v_color;

    // Top - Right
    gl_Position = c + vec4(f.xy, 0.0, 0.0);
    gs_out.v_texCoord  = vs_out[0].v_texCoord.zw; 
    EmitVertex();

    // Bottom - Right
    gl_Position = c + vec4(f.x, -f.y, 0.0, 0.0);
    gs_out.v_texCoord  = vs_out[0].v_texCoord.zy; 
    EmitVertex();

    // Top - Left
    gl_Position = c + vec4(-f.x, f.y, 0.0, 0.0);
    gs_out.v_texCoord  = vs_out[0].v_texCoord.xw; 
    EmitVertex();

    // Bottom - Left          
    gl_Position = c + vec4(-f.xy, 0.0, 0.0);
    gs_out.v_texCoord  = vs_out[0].v_texCoord.xy; 
    EmitVertex();

    EndPrimitive();
}