# Tao Renderer

![Screenshot 2023-10-14 181635](https://github.com/leoneruggiero/TestApp_OpenGL/assets/55357743/8e63b128-2edb-429d-8bd3-d8de6a5c23a2)


# Features
## Pbr

![Screenshot 2023-10-14 184257](https://github.com/leoneruggiero/TestApp_OpenGL/assets/55357743/efc992a1-0b08-408a-bced-5eb8f33ac794)

### Deferred Shading

### Advanced materials

### Environment IBL

![IBLDemo](https://github.com/leoneruggiero/TestApp_OpenGL/assets/55357743/99e88b33-7deb-483f-9007-7f35d397a0bd)

### Area Lights

### Contact Hardening Soft Shadows (PCSS)

![PCSS](https://github.com/leoneruggiero/TestApp_OpenGL/assets/55357743/a65aa6d0-853b-4bd0-892c-42270b1b067d)

## Gizmos

![GizmoDemo](https://github.com/leoneruggiero/TestApp_OpenGL/assets/55357743/48a36730-dd48-4079-8a03-4b461706fb3b)

Lorem ipsum some descpriptium Lorem ipsum some descpriptium Lorem ipsum some descpriptium
Lorem ipsum some descpriptium Lorem ipsum some descpriptium Lorem ipsum some descpriptium 
Lorem ipsum some descpriptium

### Zoom Invariance
Lorem ipsum some descpriptium Lorem ipsum some descpriptium Lorem ipsum some descpriptium
![ZoomInvariance](https://github.com/leoneruggiero/TestApp_OpenGL/assets/55357743/e91d6ecc-acd0-4942-b89f-01e9d2387950)


### Constant Screen-Lenght Texture Mapping
![ConstantScreenLength](https://github.com/leoneruggiero/TestApp_OpenGL/assets/55357743/428af1d6-b9ae-4838-989d-563ade5e7450)


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

### Mouse Pick

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
