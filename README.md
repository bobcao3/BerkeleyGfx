# Berkeley Gfx

Advanced real-time graphics boilerplate & libraries using Vulkan.

This is a work-in-progress.

## Purposes of Berkeley Gfx

Vulkan is released in 2016 as a modern and highly efficient cross-platform graphis API, while at the same time OpenGL is being slowly deprecated.

The graphics industry has began shifting away from OpenGL, favoring lower level abstractions such as Vulkan, DX12, and Metal. Since the release of OpenGL 1.0 in 1992, OpenGL has carried its way through multiple paradigm shift in graphics and GPU design. The singled threaded nature and state machine design makes OpenGL very inefficient, and the API of OpenGL no longer reflects how graphics hardware functions.

As the most of the graphics education is still based on OpenGL, this project is designed to provide an easy to learn framework that teaches you modern graphics programming paradigm for higher level students, and potentially grow into a framework that replaces OpenGL for future undergraduate level graphics courses.

## Build & Install

### Installation of Vulkan SDK

Since this is a vulkan project, you will need to download its SDK to build the project from source. LunarG Vulkan SDK is available on their website (https://www.lunarg.com/vulkan-sdk/) for all platforms (Mac, Windows, and Linux). For Linux users, it is usually possible to download the SDK through your package manager of choice. This might also be an option for Mac homebrew user.

After installing the SDK, some particular platforms such as Mac may require you to setup the environment by running `source setup-env.sh` after you installed the SDK. This needs to be run before you build the project. If you don't want to run it everytime, you can add this to the `.bashrc` or `.profile` file.

### Downloading / Cloneing the project

Clone this project **recursively** as it comes with many submodules.

If you forgot to clone it recursively, run `git submodule update --init --recursive` in the repo.

### Building the project

This project will build like any other CMake project. In addition, all dependencies are all built-in as submodules. Therefore, you only need to have a valid Visual Studio / XCode install and a valid Vulkan SDK install. (You don't need to set vcpkg toolchain file for Windows)

## Samples with Comments

The project comes with 4 different samples aimed for different scenerios. The 3rd and final one of them might be especially useful if you want to explore shaders while do not plan to deal with the graphics API itself.

**Ignore Sample 0 / 1 / 2 if you do not wish to know the details of Vulkan and the lower level API of Berkeley Gfx**

This project exports multiple CMake targets. Once you built the project, there will be multiple Sample*** executables. If you are using an IDE, when you select the run target, you will have multiple choices. You will need to select the one you want to run.

### Sample 0: SampleHelloTriangle

This is the Hello World of graphics: displaying a triangle on screen.

For the init stage of the sample:
1. Allocate a buffer on the GPU, and then we copy the vertices of a triangle onto this GPU buffer.
2. Create a Pipeline (a collection of pipeline, pipeline layout, and render pass in Vulkan parlance). A Pipeline defines how you want to render some geometry. Therefore, we specify the fragment & vertex shaders we want to use, set the viewport of the render, set the vertex input format (which will guide the GPU to read data from our vertex buffer), and finally specify the framebuffer format that we will render to.

For the render stage of the sample (the render function is called per frame):
1. Begin the command buffer. A command buffer records graphics and compute commands, and this buffer is later submitted to the GPU. The GPU will execute these commands in order. This is a common feature for all modern APIs and GPUs (command buffer for vulkan, command list for DX, and encoders for Metal). Using a prerecorded command buffer reduces the communication between CPU and GPU, which is very costly and very slow.
2. Setup a scoped render with our pipeline. We specify that we want this pipeline to render into the current frame.
3. Bind the pipeline
4. Bind the vertex buffer as the vertex input
5. Draw 3 vertices from the vertex buffer
6. Ends the command buffer. After we call `End()`, this command buffer can be now submitted to the GPU.

For more details, checkout the comments in the source code.

### Sample 1: SampleGLTFViewer

This sample demostrates how you can load & parse a GLTF model using `tinygltf`, and renders it with our library. In order for this to run, you will need to run the `get_assets.sh` in the source directory to download the GLTF sample models.

The details of GLTF format can be found here: https://www.khronos.org/gltf/

One notable difference from the last sample is that we now introduce the concept of Uniform buffer and Descriptor Set. A uniform buffer is a buffer that holds parameters for the shaders. In this case, we will pass in the camera view & projection matrix to the shaders in order for the vertex shaders to correctly transform the vertices. Descriptor Sets describes all the bindings for a shaders. Since we will be binding a Uniform Buffer and a texture, we will create a descriptor set of these two, and later bind the descriptor set to the shaders. This is another common feature of modern graphics API, where instead all of the inputs / parameters of a shader is bound at once with a single state object.

### Sample 2: SampleTerrain

This sample creates a mesh grid & loads a height map image. The shaders can read the height map image and offset the vertices to render a terrain. This is very similar to dispalcement mapping.

### Sample 3: SampleShaderGraph

ShaderGraph is a higher level API of Berkeley Gfx that provides very similar functionality to ShaderToy, while providing more control and easier UI to control the input parameters of shaders.

Each shader graph is a Graph of shaders that consumes some input images and parameters, and creates some output images. The input images can be a image loaded from disc, or it can be an output from previous shaders. This gives you the possibility to create any DAG of shaders that can achieve many post effects. With the support of using previous frame's output image as an input, you can create states and therefore perform complex simulations on GPU with this API.

Each shader graph is described as an json file with accompanying glsl shaders. We provided two samples, and by default we will load the `1_gameOfLife` shader graph. If you want it to load a different shader graph, modify the `graphFile` path in `sample/3_shaderGraph/shaderGraph.cpp`.

In the JSON file, `images` section defines a series of custom images that can be loaded from file, or specify the attributes of some shader output images.

The `stages` section defines a series of stages (nodes) on the shader graph. For each stage, you need to define the shader file it will use and the output image. If the output image is `framebuffer`, then that means this node will output to the framebuffer. (Usually the final output node of a shader graph). You can additionally provide the `textures` section that will serve as the input texture to the shaders. If the input texture name starts with `previous_**`, that means this is the texture from previous frame. We used this as the state for game of life. You can also provide the `parameters` section that will add custom parameters to your shaders. Two possible parameter types right now are `float` and `vec3`. These parameters will be user controllable and will displays an GUI for it automatically.

When you run the shader graph sample, you will see an "ShaderGraph" window, containing dropdowns for each shader stage you defined. This will be where you can control the parameters you defined. Try chaning the value for `colorFilter` to change the output colors of the game of life demo.

You can do sand sim or even fluid sim with this tool, as both of those can be described as a state machine. For fluid sim you may want to change the output format as `r32f` or `rgba32f` (32 bit floating point states instead of the default 8 bit fixed point state). Check the `2_customTexture` example on how to achieve custom formats.
