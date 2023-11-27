# Candela

Candela accepts only one argument to the config file. If no arguments are provided, the default path **Assets/config.json** is used.

## Scene

There is only one scene type 'Scene'. Need to provide Name of the new instance and Type. Lights may also be added. These are considered as external lights. These lights are usually added by the scene loaders but can also be manually added here. Non-external lights are triangles in the mesh with an emissive material. These can only be Area lights.


```json
"Scenes": [
  {
    "Type": "Scene",
    "Name": "Scene"
    "Lights": [
      {
          "LightType": "Directional",
          "Position": { "x": 0, "y": 0, "z": 0 },
          "Direction": {
            "x": -0.678103089,
            "y": -0.731488943,
            "z": 0.0714155212
          },
          "Diffuse": { "x": 1, "y": 1, "z": 1 },
          "InnerConeAngle": 0.588
        }
    ]
  }
]
```

### Lights (External)

- Type = "Directional", "Point", "Spot", "Ambient", "Area". Ambient is not really supported in the shaders and may waste samples if used.
- Position = Vector3
- Direction = Vector3
- Up = Vector3
- Attenuation = Vector3 - x = constant, y = linear, z = quadratic. 1.f / (x + y*d + z*d^2) - d is distance. Default is (1,1,1)
- Diffuse = Vector3 - Radiance for Area Lights, Intensity for Point and Spot, Irradiance (perpendicular) for Directional
- Specular = Vector3 - Unused
- InnerConeAngle = float - Used for spot. Angle in radiance is the angle from normal to spherical cap.
- OuterConeAngle = float **UNUSED** - Used for spot. Angle in radiance is the angle from normal to spherical cap.


## Scene Loaders

```json
{
    "Type": "AssImpSceneLoader",
    "Scene": "Scene",
    "FilePath": "Assets/caustic-glass/geometry/mesh_00002.ply"
}
```

- Type = "AssImpSceneLoader" or "WavefrontSceneLoader"
- Scene = The scene instance name
- FilePath = Path to OBJ, gltf, FBX, etc
- AlwaysComputeNormals - if `true` computes the normals eventhough the model has vertex normals defined.

### AssImpSceneLoader

- LoadLights - bool


## Scene Modifiers

- Type = "SceneModifier"
- Scene = The scene instance name
- Materials - List of Materials to modify. Each material modifier object contains the property name to modify and value. Names is a list of Material names to modify with these properties. Alternatively, the Ids List can be provided.

Valid Poroperty names for Materials are: 

- Diffuse, Emissive, Specular, TransmissiveFilter - `Vector3`
- Dissolve, Refractive Index - These are `float`s

```json
"SceneModifiers": [
    {
      "Type": "SceneModifier",
      "Scene": "Scene",
      "Materials": [
        {
          "Names": ["Glass", "Other"],
          "RefractiveIndex": 1.5,
          "Dissolve": 0
        }
      ]
    }
  ]
  ```

- Transforms - List of transform modifiers for named meshes (or by Id) (similar to Materials)

Valid Property Names:

- Translation, Scale, Rotation - `Vector3`
- UseCentrePosition - bool : Use the Mesh's computed Center so that rotation,
- TranslationAbsolute - bool : Add this translation without adding the CenterPosition
- RelativeComponents - bool : Add the provided values with the node's transform

```json
"SceneModifiers": [
    {
      "Type": "SceneModifier",
      "Scene": "Scene",
      "Transforms": [
        {
          "Names": ["Mesh000"],
          "Translation": { "x": 0, "y": -2.6, "z": 0 }
        }
      ]
    }
]
```

## Cameras

The Camera configuration object is shown below

- Type - Camera - This is a perspective camera with the following properties
- Position, Direction, Up - `Vector3`
- SensorDimensions - `Vector2` - The sensor size onto which the image is projected. Nikon sensor use 0.0235,0.0156 (Units always in metres, though it does not matter)
- Distance - nearZ. Also this is the distance from the pinhole to the sensor
- MaxDistance - farZ. 

```json
"Cameras": [
   {
     "Type": "Camera",
     "Name": "Camera",
     "Position": {
       "x": 0,
       "y": 1,
       "z": 3.5
     },
     "Direction": {
       "x": 0,
       "y": 0,
       "z": -1
     },
     "SensorDimensions": {
       "x": 0.0235,
       "y": 0.0156
     },
     "Distance": 0.018,
     "MaxDistance": 125
   }
],
```

## Chains

A chain is a pipeline executed by each renderer when a frame is finalised. In Candela, these provide tone mapping features as well as a way to save files to PNG and OpenEXR. The output of each chain element is fed to the input of the next chain element.

- Name = A new name for this Chain
- Type = Chain
- Chain = A list of Chain Elements


The following chain elements are available:

### Tone Mapping

- Type = ToneMapping

This tone mapper maps the radiance values using the following formula:

```
R_m = R_in / (1 + R_in)
```

### Alpha Correction (aka Gamma)

- Type = AlphaCorrection
- Gamma = float value representing the gamma. Default is 2.4

This AlphaCorrection chain maps the radiance values using the following formula:

```c++
const float encodingGamma = 1.f / gamma;
x = x <= 0.0031308f ? x * 12.92f : 1.055f * pow(x, encodingGamma) - 0.055f;
```

### File Output

- Type = FileOutput
- FilePath - Directory where to save the file
- FileType = PPM, PNG, RAW, EXR
- CompressionType = NONE, RLE, ZIPS, ZIP, PIZ. Only applies to EXR. PIZ is recommended with noisy data. See [here](https://openexr.com/en/latest/TechnicalIntroduction.html#data-compression) for more info.

### Typical Chain

```json
"Chains": [
  {
    "Name": "Chain",
    "Type": "Chain",
    "Chain": [
      {
        "Type": "FileOutput",
        "FileType": "EXR"
      },
      {
        "Type": "ToneMapping"
      },
      {
        "Type": "AlphaCorrection"
      },
      {
        "Type": "FileOutput",
        "FileType": "PNG"
      }
    ]
  }
]
```

## Drawables (Shaders)

Drawables are shaders that run in a pipeline fashion when attached to a renderer. 

### Raster

This is a basic rasterizer that can be used to compute GBuffer, etc. It is also used to draw emissive primitives i.e. mesh with emissive materials.

```json
"Drawables": [
  {
    "Type": "RasterDrawable",
    "Name": "Raster",
    "ComputeGBuffer": true,
    "ComputeRadiance": false,
    "ComputeEmissiveIfRadOff": true
  }
]
```

- Name - required
- ComputeGBuffer = `bool` - Outputs the gBuffer. The GBuffer generates the following resources that can be used by other Shaders. gPos, gNorm, gAlb, gMeshInfo and gOut. gPos (float4) is the position buffer, gNorm (float4) is the normal buffer, gAlb (float4) is the albedo buffer, gMeshInfo (uint2) is a buffer that stores the materialId and instanceId associated with a pixel. Finally, gOut (float4) is the radiance buffer.
- ComputeRadiance = `bool` - Outputs the radiance.
- ComputeEmissiveIfRadOff = `bool` - Outputs the emissive radiance even if ComputeRadiance is off.

### Raster Ray-Traced Shadows

Texture resources created by this shader are prefixed with `"rrt_"`. This shader requires ray tracing support. This shader works only with Area Lights (emissive) at the moment. It works by estimating the area light via rasterisation and then subtracting the occlusion term (negative radiance). Ray tracing is therefore used to compute shadows (Ambient Occlusion).

```json
{
  "Type": "RasterRTShadowsDrawable",
  "Name": "RasterRT",
  "Sampler": { 
    "Seed": 123 
  }
}
```

- Name - Required
- Sampler - Used to set a seed (for predictable debugging)

### Path Tracer

A path tracer.

- Name - Required
- Sampler - Used to set a seed (for predictable debugging)
- SpecularOnly = `bool`. If true, the path tracer will trace specular materials only.

```json
{
  "Type": "PathTracingDrawable",
  "Name": "PT",
  "SpecularOnly": true
}
```

### Light Tracer

A light tracer.

- Name - Required
- Sampler - Used to set a seed (for predictable debugging)
- LightSamples - `Vector2` - Size of the samples per frame
- Components - There are three variants of the light tracer. Normal, Optimised and Importance. These are all enabled by default so that during rendering one can switch from one to the other. However, sometimes it is beneficial to turn a component off. Components is an object with the key as the component name, and the value being a bool signifying wether the component will load at all.

```json
{
  "Type": "LightTracingDrawable",
  "Name": "LT",
  "LightSamples": {
    "x": 900,
    "y": 600
  },
  "Components": {
    "Importance": false
  }
}
```

### Denoiser

Most options are configurable during rendering in imgui.
The denoiser depends on the Rasteriser (with gBuffer computation) and the Light Tracer or Path Tracer.

```json
{
  "Type": "DenoiserDrawable",
  "Name": "Den"
}
```

## Animation

The animation system works by defining a number of states and a number of transitions that move the system from one state to the other.

- Name - Required
- InitialMeshState - The name of the initial state of the system
- TranslationAbsolute - `bool` - True if translations should be absolute (not relative to parent)
- States - A list of states. Each state must have a `Name` and should define a transform using: `Translation`, `Scale` and `Rotation`.
- Transitions - A list of transitions. A transistion occurs linearly (linear interpolation) to its destination `State` during its `Duration`

```json
"Animations": [
  {
    "Type": "Animation",
    "Name": "Animation1",
    "InitialMeshState": "Initial",
    "TranslationAbsolute": false,
    "States": [
      {
        "Name": "Initial",
        "Translation": {
          "x": 0,
          "y": 0.25,
          "z": 0
        }
      },
      {
        "Name": "First",
        "Rotation": {
          "x": 0,
          "y": 6.28,
          "z": 0
        },
        "Translation": {
          "x": 0,
          "y": 0.25,
          "z": 0
        }
      }
    ],
    "Transitions": [
      {
        "State": "First",
        "Duration": 2000
      }
    ]
  }
]
```

## Renderer

There is only one renderer type.

- AdapterIndex - `uint` - In case of multiple GPUs, select one according to its index. The index is reported in Candela's CLI
- Debug - `bool` - Initialises DirectX Debugging runtime (the debug layer)
- Break - `bool` - If debug is enabled, the application breaks on warnings, etc. This should probably be turned off in production environments
- VSync - `bool` - True to sync with the monitor FPS, false for full speed
- Scene - The name of the scene to render 
- Camera - The name of the camera to use
- Chain - The chain to use to output to file, etc
- ShaderAccumulation - `bool` - Whether the radiance values are accumulated every frame or cleared with every new frame
- Drawables - A list of shaders to perform rendering with. The other is extremely important for shaders that depend on the output of other shaders (such as the denoiser)
- WindowDimensions - The window size. Ideally should be the same aspect ratio as the camera
- Animations - A list of objects that connect animations to scene graph nodes
- AnimationSequencer - `Enabled` is self explantory. `FramesPerAnimation` is the number of frames to render before the `TimeDeltaMs` of the animation is incremented. The chain is also executed at this point. The animation stops after `MaxTimeMs` is surpassed.
- ExitOnAnimationCompletion - `bool` - Whether to exit when the animation sequencer completes

```json
"Renderers": [
  {
    "Type": "Renderer",
    "AdapterIndex": 0,
    "Debug": true,
    "Break": true,
    "VSync": false,
    "Scene": "Scene",
    "Camera": "Camera",
    "Chain": "Chain",
    "ShaderAccumulation": false,
    "Drawables": [
      "Raster",
      "LT",
      "PT",
      "Den"
    ],
    "WindowDimensions": {
      "x": 900,
      "y": 600
    },
    "Animations": [
      {
        "Name": "Animation1",
        "Targets": [ "shortBox" ]
      }
    ],
    "AnimationSequencer": {
      "Enabled": false,
      "FramesPerAnimation": 1000,
      "TimeDeltaMs": 200,
      "MaxTimeMs": 1000
    },
    "ExitOnAnimationCompletion": true
  }
]
```
