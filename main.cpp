#include <GLFW/glfw3.h>
#include <webgpu/webgpu_cpp.h>
#include <iostream>
#if defined(__EMSCRIPTEN__)
#include <emscripten/emscripten.h>
#else
#include <webgpu/webgpu_glfw.h>
#endif

#include <render/camera.h>
#include <render/renderer.h>

#include <utils/LogPrint.h>

#define PI 3.14159f

wgpu::Instance instance;
wgpu::Adapter adapter;
wgpu::Device device;
wgpu::RenderPipeline pipeline;

wgpu::Surface surface;
wgpu::TextureFormat format;
const uint32_t kWidth = 1024;
const uint32_t kHeight = 1024;

struct UniformData
{
    uint32_t miNumMeshes;
    float mfExplodeMultiplier;
    int32_t miSelectionX;
    int32_t miSelectionY;
    int32_t miSelectedMesh;
};

enum State
{
    NORMAL = 0,
    ZOOM_TO_SELECTION,
};

CCamera gCamera;
Render::CRenderer gRenderer;
wgpu::Sampler gSampler;
wgpu::BindGroup gBindGroup;
wgpu::BindGroupLayout gBindGroupLayout;
float4x4                                gPrevViewProjectionMatrix;

float3          gCameraLookAt;
float3          gCameraPosition;
float3          gCameraUp;
float           gfSpeed;

uint32_t        giLeftButtonHeld;
uint32_t        giRightButtonHeld;

int32_t         giLastX = -1;
int32_t         giLastY = -1;
float           gfRotationSpeed = 0.3f;
float           gfExplodeMultiplier = 0.0f;

State           gState;

float2 gCameraAngle(0.0f, 0.0f);
float3 gInitialCameraPosition(0.0f, 0.0f, -3.0f);
float3 gInitialCameraLookAt(0.0f, 0.0f, 0.0f);

std::vector<int32_t> aiHiddenMeshes;
std::vector<uint32_t> aiVisibilityFlags;
std::vector<float2> gaHaltonSequence;

void handleCameraMouseRotate(
    int32_t iX,
    int32_t iY,
    int32_t iLastX,
    int32_t iLastY);

void handleCameraMousePan(
    int32_t iX,
    int32_t iY,
    int32_t iLastX,
    int32_t iLastY);

void zoomToSelection();

float2 get_jitter_offset(int frame_index, int width, int height);

/*
**
*/
void configureSurface() 
{
    wgpu::SurfaceCapabilities capabilities;
    surface.GetCapabilities(adapter, &capabilities);
    format = capabilities.formats[0];

    printf("%s : %d format = %d\n",
        __FILE__,
        __LINE__,
        (uint32_t)format);

    wgpu::TextureFormat viewFormat = wgpu::TextureFormat::BGRA8Unorm;
    wgpu::SurfaceConfiguration config = {};
    config.device = device;
    config.format = format;
    config.width = kWidth;
    config.height = kHeight;
    config.viewFormats = &viewFormat;
    config.viewFormatCount = 1;
    config.presentMode = wgpu::PresentMode::Fifo;

    surface.Configure(&config);

    wgpu::SurfaceTexture surfaceTexture;
    surface.GetCurrentTexture(&surfaceTexture);
}

const char shaderCode[] = R"(
    @group(0) @binding(0) var texture : texture_2d<f32>;
    @group(0) @binding(1) var textureSampler : sampler;

    struct VertexOutput 
    {
        @builtin(position) pos: vec4f,
        @location(0) uv: vec2f,
    };
    @vertex fn vertexMain(@builtin(vertex_index) i : u32) -> VertexOutput 
    {
        const pos = array(vec2f(-1, 3), vec2f(-1, -1), vec2f(3, -1));
        const uv = array(vec2f(0, -1), vec2f(0, 1), vec2f(2, 1));
        var output: VertexOutput;
        output.pos = vec4f(pos[i], 0.0f, 1.0f);
        output.uv = uv[i];        
        
        return output;
    }
    @fragment fn fragmentMain(in: VertexOutput) -> @location(0) vec4f 
    {
        let color: vec4f = textureSample(
            texture,
            textureSampler,
            in.uv);

        return color;
    }
)";

/*
**
*/
void createRenderPipeline() 
{
    wgpu::ShaderModuleWGSLDescriptor wgslDesc{};
    wgslDesc.code = shaderCode;

    wgpu::ShaderModuleDescriptor shaderModuleDescriptor
    {
        .nextInChain = &wgslDesc
    };
    wgpu::ShaderModule shaderModule =
        device.CreateShaderModule(&shaderModuleDescriptor);

    wgpu::ColorTargetState colorTargetState
    {
        .format = format
    };

    wgpu::FragmentState fragmentState
    {.module = shaderModule,
        .targetCount = 1,
        .targets = &colorTargetState
    };

    // swap chain binding layouts
    std::vector<wgpu::BindGroupLayoutEntry> aBindingLayouts;
    wgpu::BindGroupLayoutEntry textureLayout = {};

    // texture binding layout
    textureLayout.binding = (uint32_t)aBindingLayouts.size();
    textureLayout.visibility = wgpu::ShaderStage::Fragment;
    textureLayout.texture.sampleType = wgpu::TextureSampleType::UnfilterableFloat;
    textureLayout.texture.viewDimension = wgpu::TextureViewDimension::e2D;
    aBindingLayouts.push_back(textureLayout);

    // sampler binding layout
    wgpu::BindGroupLayoutEntry samplerLayout = {};
    samplerLayout.binding = (uint32_t)aBindingLayouts.size();
    samplerLayout.sampler.type = wgpu::SamplerBindingType::NonFiltering;
    samplerLayout.visibility = wgpu::ShaderStage::Fragment;
    aBindingLayouts.push_back(samplerLayout);

    // create binding group layout
    wgpu::BindGroupLayoutDescriptor bindGroupLayoutDesc = {};
    bindGroupLayoutDesc.entries = aBindingLayouts.data();
    bindGroupLayoutDesc.entryCount = (uint32_t)aBindingLayouts.size();
    gBindGroupLayout = device.CreateBindGroupLayout(&bindGroupLayoutDesc);

    // create bind group
    std::vector<wgpu::BindGroupEntry> aBindGroupEntries;

    // texture binding in group
    wgpu::BindGroupEntry bindGroupEntry = {};
    bindGroupEntry.binding = (uint32_t)aBindGroupEntries.size();
    bindGroupEntry.textureView = gRenderer.getSwapChainTexture().CreateView();
    bindGroupEntry.sampler = nullptr;
    aBindGroupEntries.push_back(bindGroupEntry);

    // sample binding in group
    bindGroupEntry = {};
    bindGroupEntry.binding = (uint32_t)aBindGroupEntries.size();
    bindGroupEntry.sampler = gSampler;
    aBindGroupEntries.push_back(bindGroupEntry);

    // create bind group
    wgpu::BindGroupDescriptor bindGroupDesc = {};
    bindGroupDesc.layout = gBindGroupLayout;
    bindGroupDesc.entries = aBindGroupEntries.data();
    bindGroupDesc.entryCount = (uint32_t)aBindGroupEntries.size();
    gBindGroup = device.CreateBindGroup(&bindGroupDesc);

    // layout for creating pipeline
    wgpu::PipelineLayoutDescriptor layoutDesc = {};
    layoutDesc.bindGroupLayoutCount = 1;
    layoutDesc.bindGroupLayouts = &gBindGroupLayout;
    wgpu::PipelineLayout pipelineLayout = device.CreatePipelineLayout(&layoutDesc);

    // set the expected layout for pipeline and create
    wgpu::RenderPipelineDescriptor renderPipelineDesc = {};
    wgpu::VertexState vertexState = {};
    vertexState.module = shaderModule;
    renderPipelineDesc.vertex = vertexState;
    renderPipelineDesc.fragment = &fragmentState;
    renderPipelineDesc.layout = pipelineLayout;
    pipeline = device.CreateRenderPipeline(&renderPipelineDesc);
}

/*
**
*/
void render() 
{
    CameraUpdateInfo cameraInfo = {};
    cameraInfo.mfFar = 100.0f;
    cameraInfo.mfFieldOfView = 3.14159f * 0.5f;
    cameraInfo.mfNear = 0.01f;
    cameraInfo.mfViewWidth = (float)kWidth;
    cameraInfo.mfViewHeight = (float)kHeight;
    cameraInfo.mProjectionJitter = float2(0.0f, 0.0f);
    cameraInfo.mUp = float3(0.0f, 1.0f, 0.0f);
    cameraInfo.mProjectionJitter = float2(
        gaHaltonSequence[gRenderer.getFrameIndex() % 64].x * 0.1f,
        gaHaltonSequence[gRenderer.getFrameIndex() % 64].y * 0.1f
    );

    gCamera.setLookAt(gCameraLookAt);
    gCamera.setPosition(gCameraPosition);
    gCamera.update(cameraInfo);

    Render::CRenderer::DrawUpdateDescriptor drawDesc = {};
    drawDesc.mpViewMatrix = &gCamera.getViewMatrix();
    drawDesc.mpProjectionMatrix = &gCamera.getProjectionMatrix();
    drawDesc.mpViewProjectionMatrix = &gCamera.getViewProjectionMatrix();
    drawDesc.mpPrevViewProjectionMatrix = &gPrevViewProjectionMatrix;
    drawDesc.mpCameraPosition = &gCamera.getPosition();
    drawDesc.mpCameraLookAt = &gCamera.getLookAt();
    gRenderer.draw(drawDesc);

    gPrevViewProjectionMatrix = gCamera.getViewProjectionMatrix();

    wgpu::SurfaceTexture surfaceTexture;
    surface.GetCurrentTexture(&surfaceTexture);

    wgpu::RenderPassColorAttachment attachment
    {
        .view = surfaceTexture.texture.CreateView(),
        .loadOp = wgpu::LoadOp::Clear,
        .storeOp = wgpu::StoreOp::Store
    };

    wgpu::RenderPassDescriptor renderpass{.colorAttachmentCount = 1,
        .colorAttachments = &attachment};

    wgpu::CommandEncoder encoder = device.CreateCommandEncoder();
    wgpu::RenderPassEncoder pass = encoder.BeginRenderPass(&renderpass);

    pass.SetBindGroup(0, gBindGroup);
    pass.SetPipeline(pipeline);
    pass.Draw(3);
    pass.End();
    wgpu::CommandBuffer commands = encoder.Finish();
    device.GetQueue().Submit(1, &commands);
}

/*
**
*/
void initGraphics() 
{
    configureSurface();
    
    wgpu::SamplerDescriptor samplerDesc = {};
    gSampler = device.CreateSampler(&samplerDesc);

    Render::CRenderer::CreateDescriptor desc = {};
    desc.miScreenWidth = kWidth;
    desc.miScreenHeight = kHeight;
    desc.mpDevice = &device;
    desc.mpInstance = &instance;
    desc.mMeshFilePath = "Vinci_SurfacePro11";
    desc.mRenderJobPipelineFilePath = "render-jobs.json";
    desc.mpSampler = &gSampler;
    gRenderer.setup(desc);
    
    createRenderPipeline();

    gCamera.setLookAt(gCameraLookAt);
    gCamera.setPosition(gCameraPosition);
}

/*
**
*/
void start() 
{
    if(!glfwInit()) 
    {
        return;
    }

    gaHaltonSequence.resize(64);
    for(uint32_t i = 0; i < 64; i++)
    {
        gaHaltonSequence[i] = get_jitter_offset(i, 512, 512);
    }

    gCameraLookAt = gInitialCameraLookAt;
    gCameraPosition = gInitialCameraPosition;
    gCameraUp = float3(0.0f, 1.0f, 0.0f);
    gfSpeed = 0.1f;
    giLeftButtonHeld = giRightButtonHeld = 0;

    gState = NORMAL;

    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    GLFWwindow* window =
        glfwCreateWindow(kWidth, kHeight, "WebGPU window", nullptr, nullptr);

#if defined(__EMSCRIPTEN__)
    wgpu::SurfaceDescriptorFromCanvasHTMLSelector canvasDesc{};
    canvasDesc.selector = "#canvas";
    wgpu::SurfaceDescriptor surfaceDesc{.nextInChain = &canvasDesc};
    surface = instance.CreateSurface(&surfaceDesc);
#else
    surface = wgpu::glfw::CreateSurfaceForWindow(instance, window);
#endif // __EMSCRIPTEN__

    auto keyCallBack = [](GLFWwindow* window, int key, int scancode, int action, int mods)
    {
        if(action == GLFW_RELEASE)
        {
            return;
        }

        switch(key)
        {
            case GLFW_KEY_W:
            {
                // move forward
                float3 viewDir = normalize(gCameraLookAt - gCameraPosition);
                gCameraPosition += viewDir * gfSpeed;
                gCameraLookAt += viewDir * gfSpeed;

                break;
            }

            case GLFW_KEY_S:
            {
                // move back
                float3 viewDir = normalize(gCameraLookAt - gCameraPosition);
                gCameraPosition += viewDir * -gfSpeed;
                gCameraLookAt += viewDir * -gfSpeed;

                break;
            }

            case GLFW_KEY_A:
            {
                // pan left
                float3 viewDir = normalize(gCameraLookAt - gCameraPosition);
                float3 tangent = cross(gCameraUp, viewDir);
                float3 binormal = cross(viewDir, tangent);

                gCameraPosition += tangent * -gfSpeed;
                gCameraLookAt += tangent * -gfSpeed;

                break;
            }

            case GLFW_KEY_D:
            {
                // pan right
                float3 viewDir = normalize(gCameraLookAt - gCameraPosition);
                float3 tangent = cross(gCameraUp, viewDir);
                float3 binormal = cross(viewDir, tangent);

                gCameraPosition += tangent * gfSpeed;
                gCameraLookAt += tangent * gfSpeed;

                break;
            }

            case GLFW_KEY_E:
            {
                // explode mesh 
                gfExplodeMultiplier += 1.0f;
                gRenderer.setExplosionMultiplier(gfExplodeMultiplier);

                break;
            }

            case GLFW_KEY_R:
            {
                // move meshes back from explosion
                gfExplodeMultiplier -= 1.0f;
                gfExplodeMultiplier = std::max(gfExplodeMultiplier, 0.0f);

                gRenderer.setExplosionMultiplier(gfExplodeMultiplier);

                break;
            }

            case GLFW_KEY_H:
            {
                // hide mesh
                uint32_t iFlag = 0;
                Render::CRenderer::SelectMeshInfo const& selectionInfo = gRenderer.getSelectionInfo();
                if(selectionInfo.miMeshID >= 0)
                {
                    aiVisibilityFlags[selectionInfo.miMeshID - 1] = 0;
                    gRenderer.setBufferData(
                        "visibilityFlags",
                        aiVisibilityFlags.data(),
                        0,
                        uint32_t(aiVisibilityFlags.size() * sizeof(uint32_t))
                    );
                    aiHiddenMeshes.push_back(selectionInfo.miMeshID - 1);
                }
                break;
            }

            case GLFW_KEY_J:
            {
                // show last hidden mesh
                uint32_t iFlag = 1;
                Render::CRenderer::SelectMeshInfo const& selectionInfo = gRenderer.getSelectionInfo();
                if(aiHiddenMeshes.size() > 0)
                {
                    uint32_t iMesh = aiHiddenMeshes.back();
                    aiVisibilityFlags[iMesh] = 1;
                    gRenderer.setBufferData(
                        "visibilityFlags",
                        aiVisibilityFlags.data(),
                        0,
                        uint32_t(aiVisibilityFlags.size() * sizeof(uint32_t))
                    );
                    aiHiddenMeshes.pop_back();
                    
                }
                break;
            }

            case GLFW_KEY_Z:
            {
                zoomToSelection();
                break;
            }

        }

        float3 viewDir = normalize(gCameraLookAt - gCameraPosition);
        if(fabsf(viewDir.y) >= 0.9f)
        {
            gCameraUp = float3(1.0f, 0.0f, 0.0f);
        }
    };

    auto mouseButtonCallback = [](GLFWwindow* window, int button, int action, int mods)
    {
        if(button == GLFW_MOUSE_BUTTON_LEFT)
        {
            if(action == GLFW_PRESS)
            {
                giLeftButtonHeld = 1;
            }
            else
            {
                giLeftButtonHeld = 0;
            }
        }
        else if(button == GLFW_MOUSE_BUTTON_RIGHT)
        {
            if(action == GLFW_PRESS)
            {
                giRightButtonHeld = 1;
            }
            else
            {
                giRightButtonHeld = 0;
            }
        }

        double xpos = 0.0, ypos = 0.0;
        glfwGetCursorPos(window, &xpos, &ypos);
        if(giLeftButtonHeld || giRightButtonHeld)
        {
            giLastX = (int32_t)xpos;
            giLastY = (int32_t)ypos;
        }
        
        if(giLeftButtonHeld)
        {
            gRenderer.highLightSelectedMesh(giLastX, giLastY);
        }

    };

    auto mouseMove = [](GLFWwindow* window, double xpos, double ypos)
    {
        if(giLeftButtonHeld)
        {
            handleCameraMouseRotate((int32_t)xpos, (int32_t)ypos, giLastX, giLastY);
            giLastX = (int32_t)xpos;
            giLastY = (int32_t)ypos;
        }
        else if(giRightButtonHeld)
        {
            if(giLastX == -1)
            {
                giLastX = (int32_t)xpos;
            }

            if(giLastY == -1)
            {
                giLastY = (int32_t)ypos;
            }

            handleCameraMousePan((int32_t)xpos, (int32_t)ypos, giLastX, giLastY);
            giLastX = (int32_t)xpos;
            giLastY = (int32_t)ypos;
        }
        else
        {
            giLastX = giLastY = -1;
        }
    };

    glfwSetKeyCallback(window, keyCallBack);

    glfwSetMouseButtonCallback(window, mouseButtonCallback);
    glfwSetCursorPosCallback(window, mouseMove);

    initGraphics();

    if(aiVisibilityFlags.size() <= 0)
    {
        uint32_t iNumMeshes = gRenderer.getNumMeshes();
        aiVisibilityFlags.resize(iNumMeshes);
        for(uint32_t i = 0; i < iNumMeshes; i++)
        {
            aiVisibilityFlags[i] = 1;
        }
    }
    gRenderer.setVisibilityFlags(aiVisibilityFlags.data());

    gRenderer.setBufferData(
        "visibilityFlags",
        aiVisibilityFlags.data(),
        0,
        (uint32_t)(aiVisibilityFlags.size() * sizeof(uint32_t))
    );

#if defined(__EMSCRIPTEN__)
    emscripten_set_main_loop(render, 0, false);
#else
    while(!glfwWindowShouldClose(window)) 
    {
        glfwPollEvents();
        render();
        surface.Present();
        instance.ProcessEvents();
    }
#endif
}

#if defined(__EMSCRIPTEN__)
void GetAdapter(void (*callback)(wgpu::Adapter)) {
    instance.RequestAdapter(
        nullptr,
        // TODO(https://bugs.chromium.org/p/dawn/issues/detail?id=1892): Use
        // wgpu::RequestAdapterStatus and wgpu::Adapter.
        [](WGPURequestAdapterStatus status, WGPUAdapter cAdapter,
            const char* message, void* userdata) {
                if(message) {
                    printf("RequestAdapter: %s\n", message);
                }
                if(status != WGPURequestAdapterStatus_Success) {
                    exit(0);
                }
                wgpu::Adapter adapter = wgpu::Adapter::Acquire(cAdapter);
                reinterpret_cast<void (*)(wgpu::Adapter)>(userdata)(adapter);
        }, reinterpret_cast<void*>(callback));
}

void GetDevice(void (*callback)(wgpu::Device)) {

    wgpu::RequiredLimits requiredLimits = {};
    requiredLimits.limits.maxBufferSize = 400000000;
    requiredLimits.limits.maxStorageBufferBindingSize = 400000000;
    wgpu::DeviceDescriptor deviceDesc = {};
    deviceDesc.requiredLimits = &requiredLimits;
    adapter.RequestDevice(
        &deviceDesc,
        // TODO(https://bugs.chromium.org/p/dawn/issues/detail?id=1892): Use
        // wgpu::RequestDeviceStatus and wgpu::Device.
        [](WGPURequestDeviceStatus status, WGPUDevice cDevice,
            const char* message, void* userdata) {
                if(message) {
                    printf("RequestDevice: %s\n", message);
                }
                wgpu::Device device = wgpu::Device::Acquire(cDevice);
                device.SetUncapturedErrorCallback(
                    [](WGPUErrorType type, const char* message, void* userdata) {
                        std::cout << "Error: " << type << " - message: " << message;
                    },
                    nullptr);
                reinterpret_cast<void (*)(wgpu::Device)>(userdata)(device);
        }, reinterpret_cast<void*>(callback));
}

#endif // __EMSCRIPTEN__

/*
**
*/
int main() 
{
#if defined(__EMSCRIPTEN__)
    instance = wgpu::CreateInstance();
#else 
    wgpu::InstanceDescriptor desc = {};
    desc.capabilities.timedWaitAnyEnable = true;
    instance = wgpu::CreateInstance(&desc);
#endif // __EMSCRIPTEN__

#if defined(__EMSCRIPTEN__)
    //GetAdapter([](wgpu::Adapter a) {
    //    adapter = a;
    //   GetDevice([](wgpu::Device d) {
    //       device = d;
    //       start();
    //       });
    //   
    //});

    static bool bGotAdapter;
    bGotAdapter = false;
    instance.RequestAdapter(
        nullptr,
        [](WGPURequestAdapterStatus status,
            WGPUAdapter cAdapter,
            const char* message,
            void* userdata)
        {
            adapter = wgpu::Adapter::Acquire(cAdapter);

            wgpu::AdapterInfo info = {};
            adapter.GetInfo(&info);
            printf("adapter: %d\n", info.deviceID);

            printf("got adapter\n");
            
            bGotAdapter = true; 
        },
        nullptr);

    while(bGotAdapter == false)
    {
        emscripten_sleep(10);
        printf("waiting...\n");
    }

    static bool bGotDevice;
    bGotDevice = false;
    wgpu::RequiredLimits requiredLimits = {};
    requiredLimits.limits.maxBufferSize = 400000000;
    requiredLimits.limits.maxStorageBufferBindingSize = 400000000;
    requiredLimits.limits.maxColorAttachmentBytesPerSample = 64;
    wgpu::DeviceDescriptor deviceDesc = {};
    deviceDesc.requiredLimits = &requiredLimits;
    adapter.RequestDevice(
        &deviceDesc,
        [](WGPURequestDeviceStatus status,
            WGPUDevice cDevice,
            const char* message,
            void* userdata)
        {
            device = wgpu::Device::Acquire(cDevice);
            bGotDevice = true;

            printf("got device\n");
        },
        nullptr
    );

    while(bGotDevice == false)
    {
        emscripten_sleep(10);
        printf("waiting for device...\n");
    }

    start();
    
#else
    wgpu::RequestAdapterOptions adapterOptions = {};
    adapterOptions.backendType = wgpu::BackendType::Vulkan;
    adapterOptions.powerPreference = wgpu::PowerPreference::HighPerformance;
    adapterOptions.featureLevel = wgpu::FeatureLevel::Core;

    wgpu::Future future = instance.RequestAdapter(
        &adapterOptions,
        wgpu::CallbackMode::WaitAnyOnly,
        [](wgpu::RequestAdapterStatus status,
            wgpu::Adapter a,
            wgpu::StringView message)
        {
            if(status != wgpu::RequestAdapterStatus::Success)
            {
                DEBUG_PRINTF("!!! error %d requesting adapter -- message: \"%s\"\n",
                    status,
                    message.data);
                assert(0);
            }

            adapter = std::move(a);
        }
    );
    instance.WaitAny(future, UINT64_MAX);

    wgpu::Bool bHasMultiDrawIndirect = adapter.HasFeature(wgpu::FeatureName::MultiDrawIndirect);

    // be able to set user given labels for objects
    char const* aszToggleNames[] =
    {
        "use_user_defined_labels_in_backend",
        "allow_unsafe_apis"
    };
    wgpu::FeatureName aFeatureNames[] =
    {
        wgpu::FeatureName::MultiDrawIndirect
    };
    wgpu::Limits requireLimits = {};
    requireLimits.maxBufferSize = 1000000000;
    requireLimits.maxStorageBufferBindingSize = 1000000000;
    requireLimits.maxColorAttachmentBytesPerSample = 64;

    wgpu::DawnTogglesDescriptor toggleDesc = {};
    toggleDesc.enabledToggles = (const char* const*)&aszToggleNames;
    toggleDesc.enabledToggleCount = sizeof(aszToggleNames) / sizeof(*aszToggleNames);
    wgpu::DeviceDescriptor deviceDesc = {};
    deviceDesc.nextInChain = &toggleDesc;
    deviceDesc.requiredFeatures = aFeatureNames;
    deviceDesc.requiredFeatureCount = sizeof(aFeatureNames) / sizeof(*aFeatureNames);
    deviceDesc.requiredLimits = &requireLimits;

    deviceDesc.SetUncapturedErrorCallback(
        [](wgpu::Device const& device,
            wgpu::ErrorType errorType,
            wgpu::StringView message)
        {
            DEBUG_PRINTF("!!! error %d -- message: \"%s\"\n",
                errorType,
                message.data
            );
            assert(0);
        }
    );

    wgpu::Future future2 = adapter.RequestDevice(
        &deviceDesc,
        wgpu::CallbackMode::WaitAnyOnly,
        [](wgpu::RequestDeviceStatus status,
            wgpu::Device d,
            wgpu::StringView message)
        {
            if(status != wgpu::RequestDeviceStatus::Success)
            {
                DEBUG_PRINTF("!!! error creating device %d -- message: \"%s\" !!!\n",
                    status,
                    message.data);
                assert(0);
            }
            device = std::move(d);
        }
    );

    instance.WaitAny(future2, UINT64_MAX);
    start();
#endif // __EMSCRIPTEN__

}



/*
**
*/
void handleCameraMouseRotate(
    int32_t iX,
    int32_t iY,
    int32_t iLastX,
    int32_t iLastY)
{
    if(giLastX < 0)
    {
        giLastX = iX;
    }

    if(giLastY < 0)
    {
        giLastY = iY;
    }

    float fDiffX = float(iX - giLastX) * -1.0f;
    float fDiffY = float(iY - giLastY);

    float fDeltaX = (2.0f * 3.14159f) / (float)kWidth;
    float fDeltaY = (2.0f * 3.14159f) / (float)kHeight;

    gCameraAngle.y += fDiffX * gfRotationSpeed * fDeltaY;
    gCameraAngle.x += fDiffY * gfRotationSpeed * fDeltaX;

    if(gCameraAngle.y < 0.0f)
    {
        gCameraAngle.y = 2.0f * 3.14159f + gCameraAngle.y;
    }
    if(gCameraAngle.y > 2.0f * 3.14159f)
    {
        gCameraAngle.y = gCameraAngle.y - 2.0f * 3.14159f;
    }

    if(gCameraAngle.x < -PI * 0.5f)
    {
        gCameraAngle.x = -PI * 0.5f;
    }
    if(gCameraAngle.x > PI * 0.5f)
    {
        gCameraAngle.x = PI * 0.5f;
    }


    float4x4 rotateX = rotateMatrixX(gCameraAngle.x);
    float4x4 rotateY = rotateMatrixY(gCameraAngle.y);
    float4x4 totalMatrix = rotateY * rotateX;

    float3 diff = gInitialCameraPosition - gInitialCameraLookAt;

    float4 xformEyePosition = totalMatrix * float4(diff, 1.0f);
    xformEyePosition.x += gCameraLookAt.x;
    xformEyePosition.y += gCameraLookAt.y;
    xformEyePosition.z += gCameraLookAt.z;
    gCameraPosition = xformEyePosition;

    giLastX = iX;
    giLastY = iY;
}

/*
**
*/
void handleCameraMousePan(
    int32_t iX,
    int32_t iY,
    int32_t iLastX,
    int32_t iLastY)
{
    float const fSpeed = 0.01f;

    float fDiffX = float(iX - iLastX);
    float fDiffY = float(iY - iLastY);

    float3 viewDir = gCameraLookAt - gCameraPosition;
    float3 normalizedViewDir = normalize(viewDir);

    float3 tangent = cross(gCameraUp, normalizedViewDir);
    float3 binormal = cross(tangent, normalizedViewDir);

    gCameraPosition = gCameraPosition + binormal * -fDiffY * fSpeed + tangent * -fDiffX * fSpeed;
    gCameraLookAt = gCameraLookAt + binormal * -fDiffY * fSpeed + tangent * -fDiffX * fSpeed;
}



/*
**
*/
void toggleOtherVisibilityFlags(uint32_t iMeshID, bool bVisible)
{
    printf("%s : %d set %d\n", __FUNCTION__, __LINE__, bVisible);

    uint32_t iDataSize = (uint32_t)sizeof(uint32_t) * (uint32_t)aiVisibilityFlags.size();
    if(bVisible)
    {
        for(uint32_t i = 0; i < (uint32_t)aiVisibilityFlags.size(); i++)
        {
            aiVisibilityFlags[i] = 1;
        }
    }
    else
    {
        memset(aiVisibilityFlags.data(), 0, sizeof(uint32_t) * aiVisibilityFlags.size());
    }

    for(uint32_t i = 0; i < (uint32_t)aiHiddenMeshes.size(); i++)
    {
        aiVisibilityFlags[aiHiddenMeshes[i]] = 0;
    }

    aiVisibilityFlags[iMeshID] = 1;

    gRenderer.setBufferData(
        "visibilityFlags",
        aiVisibilityFlags.data(),
        0,
        iDataSize
    );
    gRenderer.setVisibilityFlags(aiVisibilityFlags.data());

}

float3 gSavedInitialCameraPosition;
float3 gSavedInitialCameraLookAt;
float3 gSavedCameraPosition;
float3 gSavedCameraLookAt;
float2 gSavedCameraAngle;

/*
**
*/
void zoomToSelection()
{
    printf("%s : %d\n", __FUNCTION__, __LINE__);

    if(gState == ZOOM_TO_SELECTION)
    {
        gInitialCameraPosition = gSavedInitialCameraPosition;
        gInitialCameraLookAt = gSavedInitialCameraLookAt;

        gCameraPosition = gSavedCameraPosition;
        gCameraLookAt = gSavedCameraLookAt;
        gCameraAngle = gSavedCameraAngle;

        gState = NORMAL;
        toggleOtherVisibilityFlags(0, true);
        gRenderer.setExplosionMultiplier(gfExplodeMultiplier);
    }
    else
    {
        gState = ZOOM_TO_SELECTION;

        Render::CRenderer::SelectMeshInfo const& selectMeshInfo = gRenderer.getSelectionInfo();
        if(selectMeshInfo.miMeshID >= 0)
        {
            float3 totalMidPt = (gRenderer.mTotalMeshExtent.mMaxPosition + gRenderer.mTotalMeshExtent.mMinPosition) * 0.5f;
            float3 midPt = (selectMeshInfo.mMaxPosition + selectMeshInfo.mMinPosition) * 0.5f;

            //float fZ = (totalMidPt.z - midPt.z) * std::max(gfExplodeMultiplier, 0.0f);
            //midPt.z = midPt.z - fZ;

            // compute radius
            float3 diff = selectMeshInfo.mMaxPosition - selectMeshInfo.mMinPosition;
            float fRadius = length(diff) * 0.5f;

            gSavedCameraPosition = gCameraPosition;
            gSavedCameraLookAt = gCameraLookAt;

            gSavedInitialCameraPosition = gInitialCameraPosition;
            gSavedInitialCameraLookAt = gInitialCameraLookAt;

            gSavedCameraAngle = gCameraAngle;

            float fRadiusMult = 1.25f;

            //gCameraPosition = midPt + normalize(diff) * fRadius;
            gCameraPosition = midPt + float3(0.0f, 0.0f, 1.0f) * (fRadius * fRadiusMult);
            gCameraLookAt = midPt;

            gInitialCameraPosition = gCameraPosition;
            gInitialCameraLookAt = gCameraLookAt;

            gCameraAngle = float2(0.0f, 0.0f);

            toggleOtherVisibilityFlags(selectMeshInfo.miMeshID - 1, false);
            gRenderer.setExplosionMultiplier(0.0f);
        }
    }
}

float halton(int index, int base) 
{
    float f = 1.0f;
    float r = 0.0f;
    while(index > 0) {
        f = f / base;
        r = r + f * (index % base);
        index = index / base;
    }
    return r;
}
float2 halton_2d(int index) 
{
    return float2(halton(index, 2), halton(index, 3));
}

float2 get_jitter_offset(int frame_index, int width, int height) 
{
    float2 halton_sample = halton_2d(frame_index);
    // Scale and translate to center around pixel center and limit to pixel range
    float x = ((halton_sample.x - 0.5f) / width) * 2.0f;
    float y = ((halton_sample.y - 0.5f) / height) * 2.0f;
    return float2(x, y);
}