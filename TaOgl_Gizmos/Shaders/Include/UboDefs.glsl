//? #version 430 core

#ifdef GIZMOS_POINTS
    layout (std140) uniform blk_PerObjectData
    {
        uniform uint    o_size;         // 4 byte
        uniform bool    o_snap;         // 4 byte
        uniform bool    o_hasTexture;   // 4 byte
                                        // TOTAL => 12 byte
    };
#endif

#ifdef GIZMOS_LINES
    layout (std140) uniform blk_PerObjectData
    {
        uniform uint   o_size;          // 4 byte
        uniform bool   o_hasTexture;    // 4 byte
        uniform uint   o_patternSize;   // 4 byte
                                        // TOTAL => 12 byte
    };
#endif

#ifdef GIZMOS_LINE_STRIP
    layout (std140) uniform blk_PerObjectData
    {
        uniform uint   o_size;          // 4 byte
        uniform bool   o_hasTexture;    // 4 byte
        uniform uint   o_patternSize;   // 4 byte
        uniform uint   o_vertCount;     // 4 byte
                                        // TOTAL => 16 byte
    };
#endif

#ifdef GIZMOS_MESH
    layout (std140) uniform blk_PerObjectData
    {
        uniform vec4   o_color;         // 16 byte
        uniform bool   o_hasTexture;    // 4  byte
                                        // TOTAL => 20 byte
    };
#endif

layout (std140) uniform blk_PerFrameData
{
    uniform mat4    f_viewMat;          // 64  byte
    uniform mat4    f_projMat;          // 64  byte
    uniform vec2    f_viewportSize;     // 8   byte
    uniform vec2    f_invViewportSize;  // 8   byte
    uniform vec2    f_nearFar;          // 4   byte
                                        // TOTAL => 148 byte
};
