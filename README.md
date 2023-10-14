# Tao Renderer
![Screenshot 2023-10-14 181635](https://github.com/leoneruggiero/TestApp_OpenGL/assets/55357743/8e63b128-2edb-429d-8bd3-d8de6a5c23a2)

**Tao** is an OpenGL renderer meant to be a playground to experiment with modern OpenGL concepts, computer graphics techniques and learn a bit of C++. 
The renderer is made of different components:
* **RenderContext**: creates a window, an OpenGL context, and provides abstractions for OpenGL.
* **PbrRenderer**: draws a 3D scene using physically-based rendering techniques.
* **GizmosRenderer**: draws all the useful _extras_ in a 3D application, such as debug visualization, lines, overlays, ...

The different components can communicate since they share the same underlying OpenGL abstraction provided by RenderContex.

## Pbr Renderer Features
### Pbr materials
![Screenshot 2023-10-14 184257](https://github.com/leoneruggiero/TestApp_OpenGL/assets/55357743/efc992a1-0b08-408a-bced-5eb8f33ac794)
**Cook-Torrance** has been chosen as the specular BRDF of choice for this renderer since it is considered the standard in a number of articles and technical publications.<br>
**Lambertian** diffuse has been used for the diffuse term.

### Deferred Shading

### Environment IBL

![IBLDemo](https://github.com/leoneruggiero/TestApp_OpenGL/assets/55357743/7841c4ee-90c0-4aa9-a875-d3993cf7becd)

To address environment lights, the split-sum approximation as described by Karis in [TODO] has been used.<br>
Loading an environment for a 3D scene using **Tao** is as simple as providing the renderer with a valid path to a .hdr image which contains an equirectangular projected environment capture.
The renderer takes care of processing the image, without the need to use specialized software.

### Area Lights
In addition to directional lights, **Tao** features spherical and rectangular lights:
* Sphere lights are rendered by using the _Most representative point_ technique as described by Karis in [TODO].
* Rectangular lights are achieved using a technique known as _Linearly transformed cosine_ (Article[TODO], Implementation[TODO]).

### Contact Hardening Soft Shadows
![PCSS](https://github.com/leoneruggiero/TestApp_OpenGL/assets/55357743/a65aa6d0-853b-4bd0-892c-42270b1b067d)

CHSS for directional and area lights implementation is based on the concepts presented in the article "Percentage-Closer Soft Shadows"[TODO]. 

## Gizmos Rendere Features
![GizmoDemo](https://github.com/leoneruggiero/TestApp_OpenGL/assets/55357743/48a36730-dd48-4079-8a03-4b461706fb3b)

**GizmosRenderer** is a flexible component that can be used to draw meshes, lines, line-strips and points which has been developed with overlay-drawing and debug visualization in mind.<br>

The GIF shows all the different elements drawn using this component:
* The XY-plane grid
* The small view-oriented cube on the top left corner
* The light icons
* The handles used to manipulate a light's transformation (the arrow triad for example).  

### Zoom Invariance
![ZoomInvariance](https://github.com/leoneruggiero/TestApp_OpenGL/assets/55357743/e91d6ecc-acd0-4942-b89f-01e9d2387950)

Gizmos can optionally be drawn in the 3D scene so that they always cover the same area on the screen. 

### Constant Screen-Lenght Texture Mapping
![ConstantScreenLength](https://github.com/leoneruggiero/TestApp_OpenGL/assets/55357743/428af1d6-b9ae-4838-989d-563ade5e7450)

For lines and line-strip gizmos, an option is available that maps textures that that...that what? eh?

### Mouse Pick

Gizmos selection/picking has been achieved with an off-screen drawing where entity IDs have been color encoded.<br>
By using a stack of [Pixel Buffer Objects](https://www.khronos.org/opengl/wiki/Pixel_Buffer_Object) and [Fences](https://www.khronos.org/opengl/wiki/Sync_Object), reading the result of the off-screen drawing doesn't stall the pipeline.


### Shader Graph

Cpp code:

```cpp
auto kHighlight   = SGConstVec(0.8f, 0.2f, 0.2f, 1.0f);
auto kMinOpacity  = SGConstVec(0.3f);
auto kMaxOpacity  = SGConstVec(0.8f);

auto vertCol = SGInputVertexColor();
auto vertPos = SGInputVertexPosition();
auto vertNrm = SGInputVertexNormal();
auto eyePos  = SGInputEyePosition();

auto NoV = SGDot(SGNorm(eyePos - vertPos), vertNrm);

auto opacity = SGMix(kMinOpacity, kMaxOpacity, NoV);
auto color   = SGMix(vertCol, kHighlight, NoV);

auto out = SGOutColor(SGVec(SGSwizzleX(color), SGSwizzleY(color), SGSwizzleZ(color), opacity));
```

Graph (exported as .tgf):

![shaderGraph](https://github.com/leoneruggiero/TestApp_OpenGL/assets/55357743/a614a43b-d054-4119-b622-534eb6a0f33b)


Auto-generated GLSL code:

```c
/* The following code was auto-generated */ 
void main()
{
  float sgVar_1 = dot( normalize( ( ( f_viewPos.xyz ) - ( fs_in.v_position ) ) ), fs_in.v_normal );
  vec4 sgVar_0 = mix( fs_in.v_color, const vec4(0.800000,0.200000,0.200000,1.000000), sgVar_1 );
  FragColor = vec4( ( (sgVar_0).x ), ( (sgVar_0).y ), ( (sgVar_0).z ), ( mix( const float(0.300000), const float(0.800000), sgVar_1 ) ) );
}
```

### Programmatic textures

_Example:_ rect light gizmo icon (upscaled)

<img src="https://github.com/leoneruggiero/TestApp_OpenGL/assets/55357743/f9113d30-ec26-4c54-aa99-596b876b98a1" width="256" height="256"/>

```cpp
auto sdfBody =
    SdfRoundedRect{vec2{rw, rh}, vec4{radius, 0.0f, radius, 0.0f}}
        .Translate(vec2{0.0f, (rw - rh) * 0.5f});

auto sdfMask =
    SdfRoundedRect{vec2{rw, rh}, vec4{radius, 0.0f, radius, 0.0f}}
        .Scale(vec2{0.8f})
        .Translate(vec2{0.0f, (rw - rh) * 0.5f});

auto sdfBase =
    SdfRoundedRect{vec2{rw, rw}, vec4{radius}}
        .Subtract(sdfBody)
        .Subtract(sdfBody
                      .Scale(vec2{2.0f, 1.0f})
                      .Translate(vec2{0.0f, -h_10}));

auto sdfLEDs =
    SdfRect{vec2(h_20)}
        .Translate(vec2{h_5 * 0.5f})
        .RepeatGrid(vec2{h_5})
        .Translate(vec2{h_5 * 0.5f})
        .Intersect(sdfMask);
```


### Configurable pipeline

## References
