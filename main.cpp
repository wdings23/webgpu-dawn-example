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

wgpu::Instance instance;
wgpu::Adapter adapter;
wgpu::Device device;
wgpu::RenderPipeline pipeline;

wgpu::Surface surface;
wgpu::TextureFormat format;
const uint32_t kWidth = 512;
const uint32_t kHeight = 512;

CCamera gCamera;
Render::CRenderer gRenderer;
wgpu::Sampler gSampler;
wgpu::BindGroup gBindGroup;
wgpu::BindGroupLayout gBindGroupLayout;
float4x4                                gPrevViewProjectionMatrix;

/*
**
*/
void ConfigureSurface() 
{
    wgpu::SurfaceCapabilities capabilities;
    surface.GetCapabilities(adapter, &capabilities);
    format = capabilities.formats[0];

    wgpu::SurfaceConfiguration config{
        .device = device,
        .format = format,
        .width = kWidth,
        .height = kHeight,
        .presentMode = wgpu::PresentMode::Immediate
    };
    surface.Configure(&config);

    wgpu::SurfaceTexture surfaceTexture;
    surface.GetCurrentTexture(&surfaceTexture);
    int iDebug = 1;
}

/*
**
*/
void GetAdapter(void (*callback)(wgpu::Adapter))
{
    auto createdAdapterCallBack = [](
        wgpu::RequestAdapterStatus status, 
        wgpu::Adapter cAdapter, 
        const char* message,
        void* userData)
        {
            if(message)
            {
                DEBUG_PRINTF("!!! RequestAdapter: %s !!!!\n", message);
            }
            if(status == wgpu::RequestAdapterStatus::Success)
            {
                wgpu::Adapter adapter = wgpu::Adapter::Acquire(cAdapter.Get());
                reinterpret_cast<void (*)(wgpu::Adapter)>(userData)(adapter);
            }
            
        };

    wgpu::RequestAdapterOptions options = {};
    options.backendType = wgpu::BackendType::Vulkan;
    //options.featureLevel = wgpu::FeatureLevel::Compatibility;
    //options.forceFallbackAdapter = true;
    //options.powerPreference = wgpu::PowerPreference::HighPerformance;
    wgpu::Future future = instance.RequestAdapter(
        &options,
        wgpu::CallbackMode::WaitAnyOnly,
        createdAdapterCallBack,
        (void*)callback
    );

    instance.WaitAny(future, 1000000000);

#if 0
    wgpu::RequestAdapterOptions option = {};
    option.backendType = wgpu::BackendType::Vulkan;
    instance.RequestAdapter(
        &option,
        callBack,
        &userData
    );

    instance.RequestAdapter(
        &option,
        wgpu::CallbackMode::WaitAnyOnly,
        // TODO(https://bugs.chromium.org/p/dawn/issues/detail?id=1892): Use
        // wgpu::RequestAdapterStatus and wgpu::Adapter.
        [](WGPURequestAdapterStatus status, 
            WGPUAdapter cAdapter,
            char const* message, 
            void* userdata) 
        {
                if(message) 
                {
                    printf("RequestAdapter: %s\n", message);
                }
                if(status != WGPURequestAdapterStatus_Success) 
                {
                    exit(0);
                }
                wgpu::Adapter adapter = wgpu::Adapter::Acquire(cAdapter);
                reinterpret_cast<void (*)(wgpu::Adapter)>(userdata)(adapter);
        },
        callback);
#endif // #if 0
}

/*
**
*/
void GetDevice(void (*callback)(wgpu::Device))
{
    auto callBackUponDeviceCreation = [](
        wgpu::RequestDeviceStatus status,
        wgpu::Device cDevice,
        wgpu::StringView message,
        void* userData)
    {
        if(message.length)
        {
            DEBUG_PRINTF("RequestDevice: %s\n", message);
        }
        
#if 0
        auto cb = [](
            wgpu::LoggingType type, const char* message)
            {

            };

        device.SetLoggingCallback(
            &cb
        );

        device.SetLoggingCallback(
            &cb,
            nullptr
       );
#endif // #if 0

        //device.SetUncapturedErrorCallback(
        //    [](WGPUErrorType type, const char* message, void* userData)
        //    {
        //        DEBUG_PRINTF("Error: %d - message: \"%s\"\n",
        //            type,
        //            message);
        //        assert(0);
        //    },
        //    nullptr);
        reinterpret_cast<void (*)(wgpu::Device)>(userData)(cDevice);
    };

    auto cb2 = [](
        WGPURequestDeviceStatus status, 
        WGPUDevice device, 
        WGPUStringView message, 
        void* callback_param, 
        void* userdata_param
    )
    {

    };

    // be able to set user given labels for objects
    char const* aszToggleNames[] =
    {
        "use_user_defined_labels_in_backend"
    };
    wgpu::DeviceDescriptor deviceDesc = {};
    wgpu::DawnTogglesDescriptor toggleDesc = {};
    toggleDesc.enabledToggles = (const char* const*)&aszToggleNames;
    toggleDesc.enabledToggleCount = 1;
    deviceDesc.nextInChain = &toggleDesc;

    wgpu::Future future = adapter.RequestDevice(
        &deviceDesc,
        wgpu::CallbackMode::WaitAnyOnly,
        callBackUponDeviceCreation,
        (void*)callback
    );

    instance.WaitAny(future, 10000000000);

    auto callBack = [](
        WGPURequestDeviceStatus status, 
        WGPUDevice device, 
        WGPUStringView message, 
        void* callback_param, 
        void* userData)
    {

    };
#if 0
    adapter.RequestDevice(
        &deviceDesc,
        wgpu::CallbackMode::AllowProcessEvents,
        callback
    );
#endif // #if 0

#if 0
    adapter.RequestDevice(
        &deviceDesc,
        // TODO(https://bugs.chromium.org/p/dawn/issues/detail?id=1892): Use
        // wgpu::RequestDeviceStatus and wgpu::Device.
        [](WGPURequestDeviceStatus status, WGPUDevice cDevice,
            const char* message, void* userdata) 
        {
                if(message) 
                {
                    printf("RequestDevice: %s\n", message);
                }
                wgpu::Device device = wgpu::Device::Acquire(cDevice);
                device.SetUncapturedErrorCallback(
                    [](WGPUErrorType type, const char* message, void* userdata) 
                    {
                        DEBUG_PRINTF("Error: %d - message: \"%s\"\n",
                            type,
                            message);
                        assert(0);
                    },
                    nullptr);
                reinterpret_cast<void (*)(wgpu::Device)>(userdata)(device);
        }, reinterpret_cast<void*>(callback));
#endif // #if 0
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
void CreateRenderPipeline() 
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

    wgpu::SamplerDescriptor samplerDesc = {};
    gSampler = device.CreateSampler(&samplerDesc);

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
    cameraInfo.mfNear = 1.0f;
    cameraInfo.mfViewWidth = (float)kWidth;
    cameraInfo.mfViewHeight = (float)kHeight;
    cameraInfo.mProjectionJitter = float2(0.0f, 0.0f);
    cameraInfo.mUp = float3(0.0f, 1.0f, 0.0f);
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
void InitGraphics() 
{
    Render::CRenderer::CreateDescriptor desc = {};
    desc.miScreenWidth = kWidth;
    desc.miScreenHeight = kHeight;
    desc.mpDevice = &device;
    desc.mMeshFilePath = "train6-triangles.bin";
    desc.mRenderJobPipelineFilePath = "render-jobs.json";
    gRenderer.setup(desc);
    
    ConfigureSurface();
    CreateRenderPipeline();

    gCamera.setFar(100.0f);
    gCamera.setNear(1.0f);
    gCamera.setLookAt(float3(0.0f, 0.0f, -100.0f));
    gCamera.setPosition(float3(0.0f, 0.0f, 4.0f));

    
}

/*
**
*/
void Start() 
{
#if defined(__EMSCRIPTEN__)
    wgpu::EmscriptenSurfaceSourceCanvasHTMLSelector canvasDesc{};
    wgpu::SurfaceDescriptor surfaceDesc{.nextInChain = &canvasDesc};
    surface = instance.CreateSurface(&surfaceDesc);
#else

    if(!glfwInit()) 
    {
        return;
    }

    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    GLFWwindow* window =
        glfwCreateWindow(kWidth, kHeight, "WebGPU window", nullptr, nullptr);

    surface = wgpu::glfw::CreateSurfaceForWindow(instance, window);
#endif // __EMSCRIPTEN__

    InitGraphics();

#if defined(__EMSCRIPTEN__)
    emscripten_set_main_loop(Render, 0, false);
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

int main() 
{
    wgpu::InstanceDescriptor desc = {};
    desc.capabilities.timedWaitAnyEnable = true;
    instance = wgpu::CreateInstance(&desc);

    wgpu::Future future = instance.RequestAdapter(
        nullptr,
        wgpu::CallbackMode::WaitAnyOnly,
        [](wgpu::RequestAdapterStatus status,
            wgpu::Adapter a,
            wgpu::StringView message)
        {
            if(status != wgpu::RequestAdapterStatus::Success)
            {
                assert(0);
            }

            adapter = std::move(a);
        }
    );
    instance.WaitAny(future, UINT64_MAX);

    wgpu::DeviceDescriptor deviceDesc = {};
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
                assert(0);
            }
            device = std::move(d);
        }
    );

    instance.WaitAny(future2, UINT64_MAX);

    Start();

    
}
