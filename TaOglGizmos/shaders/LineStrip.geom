#version 430 core
#define GIZMOS_LINE_STRIP
//! #include "UboDefs.glsl"
//! #include "Helper.glsl"

layout(lines_adjacency) in;
layout(triangle_strip, max_vertices = 12) out;

in VS_OUT
{
    vec4  v_color;
    vec2  v_texCoord;
    bool  v_visible;
}
gs_in[];

out GS_OUT
{
    vec4 v_color;
    vec2 v_texCoord;
}gs_out;

struct VERT
{
    vec4 clipPos;
    vec4 color;
    vec2 texCoord;
};

void Emit(VERT v)
{
    gs_out.v_color    = v.color;
    gs_out.v_texCoord = v.texCoord;
    gl_Position         = v.clipPos;
    EmitVertex();
}

void main() {

    if(!gs_in[0].v_visible) return;

    float f = o_size * gl_in[1].gl_Position.w;
     
    bool skip0=false;
    bool skip1=false;
    bool skip2=false; 
    vec4 p0_clip = gl_in[0].gl_Position;
    vec4 p1_clip = gl_in[1].gl_Position;
    vec4 p2_clip = gl_in[2].gl_Position;
    vec4 p3_clip = gl_in[3].gl_Position;

    ClipLineOnNear(p1_clip, p2_clip, f_nearFar.x, skip1);

    // the line is behind the near plane
    // and won't be drawn
    if(skip1) return; 

    // Normal to the segment
    vec2 t1_ss = 
                (p2_clip.xy/p2_clip.w) - 
                (p1_clip.xy/p1_clip.w);

    t1_ss = ((t1_ss.xy / f_invViewportSize.xy));
    
    float t1_ss_len = length(t1_ss);

    t1_ss/=t1_ss_len;

    vec2 n1_ss = vec2(-t1_ss.y, t1_ss.x);


    ClipLineOnNear(p0_clip, p1_clip, f_nearFar.x, skip0);

    // Normal to the previous segment (adj)
    vec2 t0_ss;
   
    if(skip0) t0_ss = t1_ss;
    else
    {
        t0_ss = 
                    (p1_clip.xy/p1_clip.w) - 
                    (p0_clip.xy/p0_clip.w);

        t0_ss = normalize((t0_ss.xy / f_invViewportSize.xy));
    }
    vec2 n0_ss = vec2(-t0_ss.y, t0_ss.x);

    ClipLineOnNear(p2_clip, p3_clip, f_nearFar.x, skip2);

    // Normal to the next segment (adj)
    vec2 t2_ss;

    if(skip2) t2_ss = t1_ss;
    else
    {
        t2_ss = 
                    (p3_clip.xy/p3_clip.w) - 
                    (p2_clip.xy/p2_clip.w);

        t2_ss = normalize((t2_ss.xy / f_invViewportSize.xy));
    }
     vec2 n2_ss = vec2(-t2_ss.y, t2_ss.x);


    // bisectrix screen space
    vec2 bst0_ss = normalize(n0_ss + n1_ss);
    vec2 bst1_ss = normalize(n1_ss + n2_ss);
    
    float a0 = max(0.1, abs(cross(vec3(t0_ss, 0),vec3(-bst0_ss, 0)).z));
    float a1 = max(0.1, abs(cross(vec3(t1_ss, 0),vec3(-bst1_ss, 0)).z));
    bst0_ss /= a0;
    bst1_ss /= a1;

    ///////////////////
    //// Triangulation
    ///////////////////////////////////////////////
    //                                           //
    //            m1 O --------- O m2            //
    //               |      __ / |               //
    // p0(adj)       | __ /      |       p3(adj) //
    //    O ----- p1 O --------- O p2 ----- O    //
    //               |      __ / |               //
    //               | __ /      |               //
    //            m0 O --------- O m3            //
    
    VERT m0;      
    VERT m1;      
    VERT m2;      
    VERT m3;
    VERT p1;      
    VERT p2;  

    p1.clipPos  = p1_clip;
    p1.color    = gs_in[1].v_color;
    p1.texCoord = vec2(gs_in[1].v_texCoord.x, 0.5);

    p2.clipPos  = p2_clip;
    p2.color    = gs_in[2].v_color;
    p2.texCoord = vec2(gs_in[2].v_texCoord.x, 0.5);

    m0.color    = gs_in[1].v_color;
    m1.color    = gs_in[1].v_color;
    m2.color    = gs_in[2].v_color;
    m3.color    = gs_in[2].v_color;


    vec4 incr0_clip, incr0_ss; // increment from p1 to m0 
    vec4 incr1_clip, incr1_ss; // '' '' ''  from p1 to m1 
    vec4 incr2_clip, incr2_ss; // '' '' ''  from p2 to m2 
    vec4 incr3_clip, incr3_ss; // '' '' ''  from p2 to m3 

    //      O p2
    //     /
    //    /
    //   O p1    ^     (t0 x t1).z<0
    //   |       |t0  
    //   |       O --> t1
    bool cw0 = cross(vec3(t0_ss, 0), vec3(t1_ss, 0)).z<0;
    bool cw1 = cross(vec3(t1_ss, 0), vec3(t2_ss, 0)).z<0;

    /// Vertex m0 and m1
    //////////////////////////////
    if(cw0)
    {
        incr0_ss = o_size*(vec4(-bst0_ss, 0, 0));
        incr1_ss = o_size*(vec4( n1_ss  , 0, 0));
    }
    else
    {
        incr0_ss = o_size*(vec4(-n1_ss , 0, 0));
        incr1_ss = o_size*(vec4(bst0_ss, 0, 0));
    }

    incr0_clip = incr0_ss*vec4(f_invViewportSize.xy, 1.0, 1.0)* p1_clip.w;
    incr1_clip = incr1_ss*vec4(f_invViewportSize.xy, 1.0, 1.0)* p1_clip.w;

     m0.clipPos = p1_clip + incr0_clip; m0.texCoord = vec2( gs_in[1].v_texCoord.x + (gs_in[2].v_texCoord.x - gs_in[1].v_texCoord.x) * dot(incr0_ss.xy, t1_ss)/t1_ss_len, 0);
     m1.clipPos = p1_clip + incr1_clip; m1.texCoord = vec2( gs_in[1].v_texCoord.x + (gs_in[2].v_texCoord.x - gs_in[1].v_texCoord.x) * dot(incr1_ss.xy, t1_ss)/t1_ss_len, 1);
    
    /// Vertex m2 and m3
    //////////////////////////////
    if(cw1)
    {
        incr2_ss = o_size*(vec4( n1_ss   , 0, 0));
        incr3_ss = o_size*(vec4(-bst1_ss , 0, 0));
    }
    else
    {
        incr2_ss = o_size*(vec4( bst1_ss, 0, 0));
        incr3_ss = o_size*(vec4(-n1_ss  , 0, 0));
    }

    incr2_clip = incr2_ss*vec4(f_invViewportSize.xy, 1.0, 1.0)* p2_clip.w;
    incr3_clip = incr3_ss*vec4(f_invViewportSize.xy, 1.0, 1.0)* p2_clip.w;

    m2.clipPos = p2_clip + incr2_clip; m2.texCoord = vec2( gs_in[2].v_texCoord.x + (gs_in[1].v_texCoord.x - gs_in[2].v_texCoord.x) * dot(incr2_ss.xy, -t1_ss)/t1_ss_len, 1);
    m3.clipPos = p2_clip + incr3_clip; m3.texCoord = vec2( gs_in[2].v_texCoord.x + (gs_in[1].v_texCoord.x - gs_in[2].v_texCoord.x) * dot(incr3_ss.xy, -t1_ss)/t1_ss_len, 0);

    Emit(m1);
    Emit(m2);
    Emit(p1);
    Emit(p2);
    Emit(m0);
    Emit(m3);
    
    EndPrimitive();

    // We also need two additional triangles
    // to connect the lines toghether
    //
    //      O             m2 O_
    //       \               | \O f1
    //        \              | /
    //         \  p1     p2  |/
    //          O ---------- O 
    //        / |             \
    //       /  |              \
    //   f0 O   |               \
    //       \_ O m0             O

    VERT f0; 
    VERT f1; 
    VERT t0;
    VERT t1;

    if(cw0) 
    {
        f0.clipPos  = p1_clip-incr0_clip*a0;
        f0.texCoord = m1.texCoord;
        f0.color    = m1.color;
        t0          = m1;
    }
    else
    {
        f0.clipPos  = p1_clip-incr1_clip*a0;
        f0.texCoord = m0.texCoord;
        f0.color    = m0.color;
        t0          = m0;
    }
    

    if(cw1) 
    {
        f1.clipPos  = p2_clip-incr3_clip*a1;
        f1.texCoord = m2.texCoord;
        f1.color    = m2.color;
        t1          = m2;
    }
    else
    {
        f1.clipPos  = p2_clip-incr2_clip*a1;
        f1.texCoord = m3.texCoord;
        f1.color    = m3.color;
        t1          = m3;
    }

    Emit(t1); Emit(f1); Emit(p2);
    EndPrimitive();

    Emit(p1); Emit(f0); Emit(t0);
    EndPrimitive();
}