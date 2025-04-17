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

//#define TINYEXR_IMPLEMENTATION
//#include <tinyexr/tinyexr.h>

#include <utils/halton.h>
#include <utils/blue_noise.h>

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
std::vector<float2> gaBlueNoise;

float3 gMeshMidPt;
float gfMeshRadius;
uint32_t giCameraMode = PROJECTION_PERSPECTIVE;

struct AOUniformData
{
    float mfSampleRadius;
    float mfNumSlices;
    float mfNumSections;
    float mfThickness;
    float mfMinAOPct;
    float mfMaxAOPct;
};
AOUniformData gAOUniformData;

struct DeferredIndirectUniformData
{
    float               mfExplosionMultiplier;
    float               mfCrossSectionPlaneD;
};
DeferredIndirectUniformData gDeferredIndirectUniformData;



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

#if defined(__APPLE__) && !defined(EMSCRIPTEN)
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
        const pos = array(vec2f(-1, -3), vec2f(-1, 1), vec2f(3, 1));
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
#else 
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
#endif // __APPLE__

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
    cameraInfo.mfNear = 0.1f;
    cameraInfo.mfViewWidth = (float)kWidth;
    cameraInfo.mfViewHeight = (float)kHeight;
    cameraInfo.mUp = float3(0.0f, 1.0f, 0.0f);
    cameraInfo.mProjectionJitter = float2(
        gaHaltonSequence[gRenderer.getFrameIndex() % 64].x * 0.001f,
        gaHaltonSequence[gRenderer.getFrameIndex() % 64].y * 0.001f
    );

    if(giCameraMode == ProjectionType::PROJECTION_ORTHOGRAPHIC)
    {
        cameraInfo.mfViewWidth = gfMeshRadius * 2.5f;
        cameraInfo.mfViewHeight = gfMeshRadius * 2.5f;
        cameraInfo.mfNear = gfMeshRadius * 4.0f;
        cameraInfo.mfFar = -gfMeshRadius * 4.0f;
        gCamera.setProjectionType(ProjectionType::PROJECTION_ORTHOGRAPHIC);
    }
    else
    {
        gCamera.setProjectionType(ProjectionType::PROJECTION_PERSPECTIVE);
    }

    gCamera.setLookAt(gCameraLookAt);
    gCamera.setPosition(gCameraPosition);
    gCamera.update(cameraInfo);

    gRenderer.mCameraLookAt = gCameraLookAt;
    gRenderer.mCameraPosition = gCameraPosition;

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
    //desc.mMeshFilePath = "Vinci_SurfacePro11";
    //desc.mMeshFilePath = "bistro-total";
    //desc.mMeshFilePath = "little-tokyo";
    desc.mMeshFilePath = "ICE1";
    desc.mRenderJobPipelineFilePath = "render-jobs.json";
    desc.mpSampler = &gSampler;
    gRenderer.setup(desc);
    
    createRenderPipeline();

    gMeshMidPt = (float3(gRenderer.mTotalMeshExtent.mMaxPosition) + float3(gRenderer.mTotalMeshExtent.mMinPosition)) * 0.5f;
    gfMeshRadius = length(float3(gRenderer.mTotalMeshExtent.mMaxPosition) - float3(gRenderer.mTotalMeshExtent.mMinPosition)) * 0.5f;

    if(gfMeshRadius <= 10.0f)
    {
        gInitialCameraLookAt = gMeshMidPt;
        gInitialCameraPosition = gMeshMidPt + float3(0.0f, 0.0f, 1.0f) * gfMeshRadius * 1.25f;

        gCameraLookAt = gInitialCameraLookAt;
        gCameraPosition = gInitialCameraPosition;

        gCamera.setLookAt(gCameraLookAt);
        gCamera.setPosition(gCameraPosition);
    }
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

    // halton sequence for camera jitters
    gaHaltonSequence.resize(64);
    for(uint32_t i = 0; i < 64; i++)
    {
        gaHaltonSequence[i] = Utils::get_jitter_offset(i, 512, 512);
    }

    // blue noise 
    uint32_t iBlueNoiseSize = 32;
    std::vector<std::pair<float, float>> aBlueNoisePts = Utils::generatePoints(
        2.0f,
        iBlueNoiseSize,
        iBlueNoiseSize
    );
    gaBlueNoise.resize(aBlueNoisePts.size());
    for(uint32_t i = 0; i < (uint32_t)aBlueNoisePts.size(); i++)
    {
        aBlueNoisePts[i].first /= float(iBlueNoiseSize);
        aBlueNoisePts[i].second /= float(iBlueNoiseSize);

        gaBlueNoise[i].x = aBlueNoisePts[i].first - 0.5f;
        gaBlueNoise[i].y = aBlueNoisePts[i].second - 0.5f;
    }

    gCameraLookAt = gInitialCameraLookAt;
    gCameraPosition = gInitialCameraPosition;
    gCameraUp = float3(0.0f, 1.0f, 0.0f);
    gfSpeed = 0.1f;
    giLeftButtonHeld = giRightButtonHeld = 0;

    gRenderer.setCameraPositionAndLookAt(gCameraPosition, gCameraLookAt);

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
                //gfExplodeMultiplier += 1.0f;
                //gRenderer.setExplosionMultiplier(gfExplodeMultiplier);

                gDeferredIndirectUniformData.mfExplosionMultiplier += 1.0f;

                Render::CRenderer::QueueData data;
                data.mJobName = "Deferred Indirect Graphics";
                data.mShaderResourceName = "indirectUniformData";
                data.miStart = 0;
                data.miSize = (uint32_t)sizeof(DeferredIndirectUniformData);
                data.mpData = &gDeferredIndirectUniformData;
                gRenderer.addQueueData(data);

                break;
            }

            case GLFW_KEY_R:
            {
                // move meshes back from explosion
                //gfExplodeMultiplier -= 1.0f;
                //gfExplodeMultiplier = std::max(gfExplodeMultiplier, 0.0f);
                //gRenderer.setExplosionMultiplier(gfExplodeMultiplier);

                gDeferredIndirectUniformData.mfExplosionMultiplier = std::max(gDeferredIndirectUniformData.mfExplosionMultiplier - 1.0f, 0.0f);

                Render::CRenderer::QueueData data;
                data.mJobName = "Deferred Indirect Graphics";
                data.mShaderResourceName = "indirectUniformData";
                data.miStart = 0;
                data.miSize = (uint32_t)sizeof(DeferredIndirectUniformData);
                data.mpData = &gDeferredIndirectUniformData;
                gRenderer.addQueueData(data);

                break;
            }

            case GLFW_KEY_H:
            {
                // hide mesh
                uint32_t iFlag = 0;
                Render::CRenderer::SelectMeshInfo const& selectionInfo = gRenderer.getSelectionInfo();
                DEBUG_PRINTF("selected mesh %d\n", selectionInfo.miMeshID);
                if(selectionInfo.miMeshID >= 0)
                {
                    aiVisibilityFlags[selectionInfo.miMeshID] = 0;
                    gRenderer.setBufferData(
                        "visibilityFlags",
                        aiVisibilityFlags.data(),
                        0,
                        uint32_t(aiVisibilityFlags.size() * sizeof(uint32_t))
                    );
                    aiHiddenMeshes.push_back(selectionInfo.miMeshID);
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

            case GLFW_KEY_N:
            {
                gAOUniformData.mfSampleRadius = std::max(gAOUniformData.mfSampleRadius - 0.1f, 0.0f);

                Render::CRenderer::QueueData data;
                data.mJobName = "Ambient Occlusion Graphics";
                data.mShaderResourceName = "uniformData";
                data.miStart = 0;
                data.miSize = (uint32_t)sizeof(AOUniformData);
                data.mpData = &gAOUniformData;

                gRenderer.addQueueData(data);
                break;
            }

            case GLFW_KEY_M:
            {
                gAOUniformData.mfSampleRadius = std::max(gAOUniformData.mfSampleRadius + 0.1f, 0.0f);

                Render::CRenderer::QueueData data;
                data.mJobName = "Ambient Occlusion Graphics";
                data.mShaderResourceName = "uniformData";
                data.miStart = 0;
                data.miSize = (uint32_t)sizeof(AOUniformData);
                data.mpData = &gAOUniformData;

                gRenderer.addQueueData(data);
                break;
            }

            case GLFW_KEY_K:
            {
                gAOUniformData.mfThickness = std::max(gAOUniformData.mfThickness - 0.0001f, 0.0f);

                Render::CRenderer::QueueData data;
                data.mJobName = "Ambient Occlusion Graphics";
                data.mShaderResourceName = "uniformData";
                data.miStart = 0;
                data.miSize = (uint32_t)sizeof(AOUniformData);
                data.mpData = &gAOUniformData;

                gRenderer.addQueueData(data);
                break;
            }

            case GLFW_KEY_L:
            {
                gAOUniformData.mfThickness = std::max(gAOUniformData.mfThickness + 0.0001f, 0.0f);

                Render::CRenderer::QueueData data;
                data.mJobName = "Ambient Occlusion Graphics";
                data.mShaderResourceName = "uniformData";
                data.miStart = 0;
                data.miSize = (uint32_t)sizeof(AOUniformData);
                data.mpData = &gAOUniformData;

                gRenderer.addQueueData(data);
                break;
            }

            case GLFW_KEY_O:
            {
                if(gDeferredIndirectUniformData.mfCrossSectionPlaneD > 1000.0f)
                {
                    gDeferredIndirectUniformData.mfCrossSectionPlaneD = gfMeshRadius * 1.25f;
                }

                gDeferredIndirectUniformData.mfCrossSectionPlaneD -= ((gfMeshRadius * 2.0f) / 100.0f);

                Render::CRenderer::QueueData data;
                data.mJobName = "Deferred Indirect Graphics";
                data.mShaderResourceName = "indirectUniformData";
                data.miStart = 0;
                data.miSize = (uint32_t)sizeof(DeferredIndirectUniformData);
                data.mpData = &gDeferredIndirectUniformData;
                gRenderer.addQueueData(data);

                break;
            }

            case GLFW_KEY_P:
            {
                if(gDeferredIndirectUniformData.mfCrossSectionPlaneD > 1000.0f)
                {
                    gDeferredIndirectUniformData.mfCrossSectionPlaneD = gfMeshRadius * 1.25f;
                }

                gDeferredIndirectUniformData.mfCrossSectionPlaneD += ((gfMeshRadius * 2.0f) / 100.0f);

                Render::CRenderer::QueueData data;
                data.mJobName = "Deferred Indirect Graphics";
                data.mShaderResourceName = "indirectUniformData";
                data.miStart = 0;
                data.miSize = (uint32_t)sizeof(DeferredIndirectUniformData);
                data.mpData = &gDeferredIndirectUniformData;
                gRenderer.addQueueData(data);

                break;
            }

            case GLFW_KEY_C:
            {
                if(giCameraMode == PROJECTION_PERSPECTIVE)
                {
                    giCameraMode = PROJECTION_ORTHOGRAPHIC;
                    
                }
                else
                {
                    giCameraMode = PROJECTION_PERSPECTIVE;
                }


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
#if defined(__APPLE__) && !defined(EMSCRIPTEN)
        ypos = (double)kHeight - ypos;
#endif // __APPLE__
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

    gRenderer.setBufferData(
        "blueNoiseBuffer",
        gaBlueNoise.data(),
        0,
        (uint32_t)(sizeof(float2)* gaBlueNoise.size())
    );

    {
        gAOUniformData.mfSampleRadius = 2.0f;
        gAOUniformData.mfMaxAOPct = 0.7f;
        gAOUniformData.mfMinAOPct = 0.0f;
        gAOUniformData.mfNumSections = 16.0f;
        gAOUniformData.mfNumSlices = 16.0f;
        gAOUniformData.mfThickness = 0.0015f;
        Render::CRenderer::QueueData data;
        data.mJobName = "Ambient Occlusion Graphics";
        data.mShaderResourceName = "uniformData";
        data.miStart = 0;
        data.miSize = (uint32_t)sizeof(AOUniformData);
        data.mpData = &gAOUniformData;
        gRenderer.addQueueData(data);

        gDeferredIndirectUniformData.mfCrossSectionPlaneD = 100000.0f;
        gDeferredIndirectUniformData.mfExplosionMultiplier = 0.0f;

        data.mJobName = "Deferred Indirect Graphics";
        data.mShaderResourceName = "indirectUniformData";
        data.miStart = 0;
        data.miSize = (uint32_t)sizeof(DeferredIndirectUniformData);
        data.mpData = &gDeferredIndirectUniformData;
        gRenderer.addQueueData(data);

    }

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

float sign(float fVal)
{
    return (fVal < 0.0f) ? -1.0f : 1.0f;
}

/*
**
*/
void verifyTest()
{
#if 0
    std::vector<float4> worldPositionImage;
    std::vector<float4> normalImage;
    std::vector<float4> texAndClipSpaceImage;
    std::vector<float4> depthImage;
    std::vector<float4> viewImage;

    float* pafWorldPositionImageData = nullptr;
    char const* error = nullptr;
    int32_t iImageWidth = 0, iImageHeight = 0;
    LoadEXR(&pafWorldPositionImageData, &iImageWidth, &iImageHeight, "d:\\Downloads\\render-doc-image-outputs\\world-position.exr", &error);

    worldPositionImage.resize(iImageWidth * iImageHeight);
    normalImage.resize(iImageWidth * iImageHeight);
    texAndClipSpaceImage.resize(iImageWidth * iImageHeight);
    depthImage.resize(iImageWidth * iImageHeight);
    viewImage.resize(iImageWidth * iImageHeight);

    float* pafNormalImageData = nullptr;
    LoadEXR(&pafNormalImageData, &iImageWidth, &iImageHeight, "d:\\Downloads\\render-doc-image-outputs\\normal.exr", &error);

    float* pafTexCoordAndClipSpaceImageData = nullptr;
    LoadEXR(&pafTexCoordAndClipSpaceImageData, &iImageWidth, &iImageHeight, "d:\\Downloads\\render-doc-image-outputs\\texture-and-clipspace.exr", &error);

    float* pafDepthImageData = nullptr;
    LoadEXR(&pafDepthImageData, &iImageWidth, &iImageHeight, "d:\\Downloads\\render-doc-image-outputs\\depth.exr", &error);

    float* pafViewImageData = nullptr;
    LoadEXR(&pafViewImageData, &iImageWidth, &iImageHeight, "d:\\Downloads\\render-doc-image-outputs\\view.exr", &error);

    memcpy(worldPositionImage.data(), pafWorldPositionImageData, sizeof(float4) * iImageWidth * iImageHeight);
    memcpy(normalImage.data(), pafNormalImageData, sizeof(float4) * iImageWidth * iImageHeight);
    memcpy(texAndClipSpaceImage.data(), pafTexCoordAndClipSpaceImageData, sizeof(float4) * iImageWidth * iImageHeight);
    memcpy(depthImage.data(), pafDepthImageData, sizeof(float4) * iImageWidth * iImageHeight);
    memcpy(viewImage.data(), pafViewImageData, sizeof(float4) * iImageWidth * iImageHeight);

    free(pafWorldPositionImageData);
    free(pafNormalImageData);
    free(pafTexCoordAndClipSpaceImageData);
    free(pafDepthImageData);
    free(pafViewImageData);

    uint32_t textureSampler = 1;
    float4 const* worldPositionTexture = worldPositionImage.data();
    float4 const* normalTexture = normalImage.data();
    float4 const* textureCoordAndClipSpaceTexture = texAndClipSpaceImage.data();
    float4 const* depthTexture = depthImage.data();
    float4 const* viewTexture = viewImage.data();

    //uint2 inputCoord = uint2(845, 661);
    uint2 inputCoord = uint2(783, 855);
    float2 inputUV = float2(
        inputCoord.x / float(iImageWidth),
        inputCoord.y / float(iImageHeight)
    );

    auto textureSample = [iImageWidth, iImageHeight](float4 const* texture, uint32_t iTextureSampler, float2 uv)
        {
            uint32_t iX = clamp(uint32_t(uv.x * float(iImageWidth)), 0, iImageWidth - 1);
            uint32_t iY = clamp(uint32_t(uv.y * float(iImageHeight)), 0, iImageWidth - 1);

            uint32_t iIndex = iY * iImageWidth + iX;
            return texture[iIndex];
        };

    auto countBits = [](uint32_t iVal)
        {
            //Counts the number of 1:s
            //https://www.baeldung.com/cs/integer-bitcount
            iVal = (iVal & 0x55555555) + ((iVal >> 1) & 0x55555555);
            iVal = (iVal & 0x33333333) + ((iVal >> 2) & 0x33333333);
            iVal = (iVal & 0x0F0F0F0F) + ((iVal >> 4) & 0x0F0F0F0F);
            iVal = (iVal & 0x00FF00FF) + ((iVal >> 8) & 0x00FF00FF);
            iVal = (iVal & 0x0000FFFF) + ((iVal >> 16) & 0x0000FFFF);
            return iVal;
        };

    uint32_t iImageIndex = inputCoord.y * iImageWidth + inputCoord.x;
    float4 const& worldPositionOutput = worldPositionImage[iImageIndex];
    float4 const& normalOutput = normalImage[iImageIndex];
    float4 const& clipSpaceOutput = texAndClipSpaceImage[iImageIndex];
    float4 const& depth = depthImage[iImageIndex];
    float4 const& viewOutput = viewImage[iImageIndex];

    float3 cameraPosition = float3(0.0f, 6.0f, -3.0f);
    float3 cameraLookAt = float3(0.0f, 6.0f, 0.0f);

    float3 viewPosition = float3(viewOutput.x, viewOutput.y, viewOutput.z);


    uint32_t const kiNumSlices = 16;
    uint32_t const kiNumSections = 32;
    float const kfThickness = 0.001f;

    float fSampleRadius = 4.0f;

    uint2 textureSize = uint2(iImageWidth, iImageHeight);
    float2 uvStep = float2(
        1.0f / float(textureSize.x),
        1.0f / float(textureSize.y)) * fSampleRadius;

    float3 viewSpaceNormal = float3(normalOutput.x, normalOutput.y, normalOutput.z);
    

    //viewDirection = vec3(0.0f, 0.0f, -1.0f);
    //viewSpaceNormal = vec3(0.0f, 1.0f, 0.0f);

    
    CameraUpdateInfo cameraInfo = {};
    cameraInfo.mfFar = 100.0f;
    cameraInfo.mfFieldOfView = 3.14159f * 0.5f;
    cameraInfo.mfNear = 0.01f;
    cameraInfo.mfViewWidth = (float)kWidth;
    cameraInfo.mfViewHeight = (float)kHeight;
    cameraInfo.mProjectionJitter = float2(0.0f, 0.0f);
    cameraInfo.mUp = float3(0.0f, 1.0f, 0.0f);
    cameraInfo.mProjectionJitter = float2(0.0f, 0.0f);
    CCamera camera;
    camera.setLookAt(cameraLookAt);
    camera.setPosition(cameraPosition);
    camera.update(cameraInfo);
    float4x4 viewMatrix = camera.getViewMatrix();
    viewMatrix.mafEntries[3] = 0.0f;
    viewMatrix.mafEntries[7] = 0.0f;
    viewMatrix.mafEntries[11] = 0.0f;

    std::vector<float4> paOutputImage(iImageWidth * iImageHeight);
    for(int32_t iY = 0; iY < iImageWidth; iY++)
    //for(int32_t iY = 660; iY <= 660; iY++)
    {
        for(int32_t iX = 0; iX < iImageWidth; iX++)
        //for(int32_t iX = 843; iX <= 843; iX++)
        {
            float2 uv = float2((float)iX / (float)iImageWidth, (float)iY / (float)iImageHeight);

            float4 worldPosition = textureSample(
                worldPositionTexture,
                textureSampler,
                uv
            );
            float3 viewPosition = float3(worldPosition.x, worldPosition.y, worldPosition.z) - cameraPosition;
            float3 viewDirection = normalize(viewPosition * -1.0f);
            
            float4 normal = textureSample(
                normalTexture,
                textureSampler,
                uv
            );
            viewSpaceNormal = viewMatrix * normal;

            uint32_t iTotalBits = 0;
            uint32_t iCountedBits = 0;
            for(uint32_t iSlice = 0; iSlice < kiNumSlices; iSlice++)
            {
                float fPhi = float(iSlice) * ((2.0f * PI) / float(kiNumSlices));
                float2 omega = float2(cos(fPhi), -sin(fPhi));
                float3 screenSpaceDirection = float3(omega.x, omega.y, 0.0f);
                float3 orthoDirection = screenSpaceDirection - (viewDirection * dot(screenSpaceDirection, viewDirection));
                float3 axis = cross(viewDirection, orthoDirection);
                float fProjectedLength = dot(viewSpaceNormal, axis);
                float3 projectedAxis = axis * fProjectedLength;
                float3 projectedNormal = viewSpaceNormal - projectedAxis;
                float fProjectedNormalLength = length(projectedNormal);
                if(fProjectedNormalLength == 0.0f)
                {
                    continue;
                }
                
                float fCosineProjectedNormal = clamp(dot(projectedNormal, viewDirection) / fProjectedNormalLength, 0.0f, 1.0f);
                
                // angle between view vector and projected normal vector
                float3 sliceTangent = cross(viewDirection, axis);
                //float fViewToProjectedNormalAngle = sign(dot(projectedNormal, orthoDirection)) * acos(fCosineProjectedNormal);

                float fViewToProjectedNormalAngle = acos(fCosineProjectedNormal);

                // sections on the slice
                uint32_t iAOBitMask = 0u;
                float fSampleDirection = 1.0f;
                uint32_t iOccludedBits = 0;
                for(int32_t iDirection = 0; iDirection < 2; iDirection++)
                {
                    fSampleDirection = (iDirection > 0) ? -1.0f : 1.0f;
                    for(int32_t iSection = 0; iSection < kiNumSections / 2; iSection++)
                    {
                        // sample view clip space position and normal
                        float2 sampleUV = uv + omega * uvStep * fSampleDirection * float(iSection);
                        sampleUV.x = clamp(sampleUV.x, 0.0f, 1.0f);
                        sampleUV.y = clamp(sampleUV.y, 0.0f, 1.0f);

                        uint32_t iSampleX = uint32_t(float(sampleUV.x) * float(iImageWidth));
                        uint32_t iSampleY = uint32_t(float(sampleUV.y) * float(iImageHeight));

                        if(iSampleX == inputCoord.x && iSampleY == inputCoord.y)
                        {
                            continue;
                        }

                        // sample depth and clip space
                        float4 sampleWorldPosition = textureSample(
                            worldPositionTexture,
                            textureSampler,
                            sampleUV
                        );
                        float4 sampleNormal = textureSample(
                            normalTexture,
                            textureSampler,
                            sampleUV
                        );

                        float3 sampleViewPosition = float3(sampleWorldPosition.x, sampleWorldPosition.y, sampleWorldPosition.z) - cameraPosition;
                        
                        // sample view position - current view position
                        float3 deltaViewSpacePosition = float3(sampleViewPosition) - viewPosition;
                        float fDeltaViewSpaceLength = dot(deltaViewSpacePosition, deltaViewSpacePosition);
                        if(fDeltaViewSpaceLength <= 0.0f)
                        {
                            continue;
                        }

                        // front and back horizon angle
                        float3 backDeltaViewPosition = deltaViewSpacePosition - viewDirection * kfThickness;
                        float fHorizonAngleFront = dot(normalize(deltaViewSpacePosition), viewDirection);
                        float fHorizonAngleBack = dot(normalize(backDeltaViewPosition), viewDirection);
                        
                        fHorizonAngleFront = acos(fHorizonAngleFront);
                        fHorizonAngleBack = acos(fHorizonAngleBack);

                        // convert to percentage relative projected normal angle as the middle angle
                        float fMinAngle = fViewToProjectedNormalAngle - PI * 0.5f;
                        float fMaxAngle = fViewToProjectedNormalAngle + PI * 0.5f;
                        float fPct0 = clamp((fHorizonAngleFront - fMinAngle) / PI, 0.0f, 1.0f);
                        float fPct1 = clamp((fHorizonAngleBack - fMinAngle) / PI, 0.0f, 1.0f);
                        float2 horizonAngle = float2(fPct1, fPct0);
                        if(fSampleDirection < 0.0f)
                        {
                            horizonAngle = float2(fPct0, fPct1);
                        }

                        // set the section bit for this sample
                        uint32_t iStartHorizon = uint32_t(horizonAngle.x * (float)kiNumSections);
                        float fHorizonAngle = ceil((horizonAngle.x - horizonAngle.y) * float(kiNumSections));
                        uint32_t iAngleHorizon = (fHorizonAngle > 0.0f) ? 1 : 0;
                        if(iAngleHorizon > 0)
                        {
                            iOccludedBits |= (1 << iStartHorizon);
                        }

#if 0
                        DEBUG_PRINTF("%d view delta (%.4f, %.4f, %.4f) angle %.4f mid angle %.4f pct %.4f angle bit %d\n\trange(%.4f, %4f)\n\n",
                            iSection,
                            deltaViewSpacePosition.x,
                            deltaViewSpacePosition.y,
                            deltaViewSpacePosition.z,
                            fHorizonAngleFront,
                            fViewToProjectedNormalAngle,
                            fPct0,
                            iAngleHorizon,
                            fMinAngle,
                            fMaxAngle);
                        
                        int iDebug = 1;
#endif // #if 0

                    }   // for sections

                    uint32_t iNumBits = countBits(iOccludedBits);
                    iCountedBits += iNumBits;

                }   // for directions

                iTotalBits += kiNumSections;

            }   // for slice 

            float fPct = 1.0f - float(iCountedBits) / float(iTotalBits);
            
            uint32_t iImageIndex = iY * iImageWidth + iX;
            paOutputImage[iImageIndex] = float4(fPct, fPct, fPct, 1.0f);

        }   // for x

    }   // for y

    SaveEXR((float const*)paOutputImage.data(), iImageWidth, iImageHeight, 4, 0, "D:\\Downloads\\render-doc-image-outputs\\output-ao.exr", &error);
    int iDebug = 1;
#endif // #if 0
}

/*
**
*/
int main() 
{
    //verifyTest();

    

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
#if defined(_MSC_VER)
    adapterOptions.backendType = wgpu::BackendType::Vulkan;
#endif // _MSC_VER
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
        "allow_unsafe_apis",
        "disable_symbol_renaming"
    };
    wgpu::FeatureName aFeatureNames[] =
    {
    #if defined(_MSC_VER)
        wgpu::FeatureName::MultiDrawIndirect
    #endif // _MSC_VER
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
    float const fSpeed = gfMeshRadius * 0.001f;

    float fDiffX = float(iX - iLastX);
    float fDiffY = float(iY - iLastY);

    float3 viewDir = gCameraLookAt - gCameraPosition;
    float3 normalizedViewDir = normalize(viewDir);

    float3 tangent = cross(gCameraUp, normalizedViewDir);
    float3 binormal = cross(tangent, normalizedViewDir);

    gCameraPosition = gCameraPosition + binormal * fDiffY * fSpeed + tangent * fDiffX * fSpeed;
    gCameraLookAt = gCameraLookAt + binormal * fDiffY * fSpeed + tangent * fDiffX * fSpeed;
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
        }
    }
}
