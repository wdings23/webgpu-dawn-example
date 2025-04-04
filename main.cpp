#include <GLFW/glfw3.h>
#include <webgpu/webgpu_cpp.h>
#include <webgpu/webgpu_cpp_print.h>
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

float3          gCameraLookAt;
float3          gCameraPosition;
float3          gCameraUp;
float           gfSpeed;

/*
**
*/
void configureSurface() 
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
    cameraInfo.mfNear = 0.3f;
    cameraInfo.mfViewWidth = (float)kWidth;
    cameraInfo.mfViewHeight = (float)kHeight;
    cameraInfo.mProjectionJitter = float2(0.0f, 0.0f);
    cameraInfo.mUp = float3(0.0f, 1.0f, 0.0f);

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
    Render::CRenderer::CreateDescriptor desc = {};
    desc.miScreenWidth = kWidth;
    desc.miScreenHeight = kHeight;
    desc.mpDevice = &device;
    desc.mMeshFilePath = "Vinci_SurfacePro11-triangles.bin";
    desc.mRenderJobPipelineFilePath = "render-jobs.json";
    gRenderer.setup(desc);
    
    configureSurface();
    createRenderPipeline();

    gCamera.setLookAt(gCameraLookAt);
    gCamera.setPosition(gCameraPosition);
}

/*
**
*/
void start() 
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

    gCameraLookAt = float3(0.0f, 0.0f, -100.0f);
    gCameraPosition = float3(0.0f, 0.0f, 1.5f);
    gCameraUp = float3(0.0f, 1.0f, 0.0f);
    gfSpeed = 0.01f;

    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    GLFWwindow* window =
        glfwCreateWindow(kWidth, kHeight, "WebGPU window", nullptr, nullptr);

    surface = wgpu::glfw::CreateSurfaceForWindow(instance, window);

    auto keyCallBack = [](GLFWwindow* window, int key, int scancode, int action, int mods)
    {
        switch(key)
        {
            case GLFW_KEY_W:
            {
                float3 viewDir = normalize(gCameraLookAt - gCameraPosition);
                gCameraPosition += viewDir * gfSpeed;
                gCameraLookAt += viewDir * gfSpeed;

                break;
            }

            case GLFW_KEY_S:
            {
                float3 viewDir = normalize(gCameraLookAt - gCameraPosition);
                gCameraPosition += viewDir * -gfSpeed;
                gCameraLookAt += viewDir * -gfSpeed;

                break;
            }

            case GLFW_KEY_A:
            {
                float3 viewDir = normalize(gCameraLookAt - gCameraPosition);
                float3 tangent = cross(gCameraUp, viewDir);
                float3 binormal = cross(viewDir, tangent);

                gCameraPosition += tangent * -gfSpeed;
                gCameraLookAt += tangent * -gfSpeed;

                break;
            }

            case GLFW_KEY_D:
            {
                float3 viewDir = normalize(gCameraLookAt - gCameraPosition);
                float3 tangent = cross(gCameraUp, viewDir);
                float3 binormal = cross(viewDir, tangent);

                gCameraPosition += tangent * gfSpeed;
                gCameraLookAt += tangent * gfSpeed;

                break;
            }
        }

        float3 viewDir = normalize(gCameraLookAt - gCameraPosition);
        if(fabsf(viewDir.y) >= 0.9f)
        {
            gCameraUp = float3(1.0f, 0.0f, 0.0f);
        }
    };

    glfwSetKeyCallback(window, keyCallBack);

#endif // __EMSCRIPTEN__

    initGraphics();

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

/*
**
*/
int main() 
{
    wgpu::InstanceDescriptor desc = {};
    desc.capabilities.timedWaitAnyEnable = true;
    instance = wgpu::CreateInstance(&desc);

    wgpu::RequestAdapterOptions adapterOptions = {};
    adapterOptions.backendType = wgpu::BackendType::Vulkan;
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
}
