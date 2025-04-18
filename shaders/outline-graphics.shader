const PI: f32 = 3.14159f;

struct DefaultUniformData
{
    miScreenWidth: i32,
    miScreenHeight: i32,
    miFrame: i32,
    miNumMeshes: u32,

    mfRand0: f32,
    mfRand1: f32,
    mfRand2: f32,
    mfRand3: f32,

    mViewProjectionMatrix: mat4x4<f32>,
    mPrevViewProjectionMatrix: mat4x4<f32>,
    mViewMatrix: mat4x4<f32>,
    mProjectionMatrix: mat4x4<f32>,

    mJitteredViewProjectionMatrix: mat4x4<f32>,
    mPrevJitteredViewProjectionMatrix: mat4x4<f32>,

    mCameraPosition: vec4<f32>,
    mCameraLookAt: vec4<f32>,

    mLightRadiance: vec4<f32>,
    mLightDirection: vec4<f32>,
};

struct UniformData
{
    mfDepthThreshold: f32,
    mfNormalThreshold: f32,
};

@group(0) @binding(0)
var worldPositionTexture: texture_2d<f32>;

@group(0) @binding(1)
var normalTexture: texture_2d<f32>;

@group(0) @binding(2)
var depthTexture: texture_2d<f32>;

@group(1) @binding(0)
var<uniform> uniformBuffer: UniformData;

@group(1) @binding(1)
var<uniform> defaultUniformBuffer: DefaultUniformData;

@group(1) @binding(2)
var textureSampler: sampler;

struct VertexOutput 
{
    @builtin(position) pos: vec4f,
    @location(0) uv: vec2f,
};

struct FragmentOutput 
{
    @location(0) mOutput : vec4<f32>,
};

@vertex
fn vs_main(@builtin(vertex_index) i : u32) -> VertexOutput 
{
    const pos = array(vec2f(-1, 3), vec2f(-1, -1), vec2f(3, -1));
    const uv = array(vec2f(0, -1), vec2f(0, 1), vec2f(2, 1));
    var output: VertexOutput;
    output.pos = vec4f(pos[i], 0.0f, 1.0f);
    output.uv = uv[i];        

    return output;
}

@fragment
fn fs_main(in: VertexOutput) -> FragmentOutput 
{
    var out: FragmentOutput;

    let screenCoord: vec2i = vec2i(
        i32(f32(defaultUniformBuffer.miScreenWidth) * in.uv.x),
        i32(f32(defaultUniformBuffer.miScreenHeight) * in.uv.y) 
    );
    let worldPosition: vec4f = textureLoad(
        worldPositionTexture,
        screenCoord,
        0
    );

    var iMesh: i32 = i32(ceil((worldPosition.w - fract(worldPosition.w)) - 0.5f));
    if(worldPosition.w == 0.0f)
    {
        out.mOutput = vec4f(1.0f, 1.0f, 1.0f, 0.0f);
        return out;
    }

    let normal: vec4f = textureLoad(
        normalTexture,
        screenCoord,
        0
    );

    let fDepth: f32 = textureLoad(
        depthTexture,
        screenCoord,
        0
    ).x;

    var bIsLine: bool = false;
    for(var iY: i32 = -1; iY <= 1; iY++)
    {
        for(var iX: i32 = -1; iX <= 1; iX++)
        {
            let sampleScreenCoord: vec2i = vec2i(
                i32(f32(defaultUniformBuffer.miScreenWidth) * in.uv.x) + iX,
                i32(f32(defaultUniformBuffer.miScreenHeight) * in.uv.y) + iY 
            );

            let fSampleDepth: f32 = textureLoad(
                depthTexture,
                sampleScreenCoord,
                0
            ).x;

            let fDepthDiff: f32 = abs(fDepth - fSampleDepth);
            if(fDepthDiff >= uniformBuffer.mfDepthThreshold)
            {
                bIsLine = true;
                break;
            }

            let sampleNormal: vec3f = textureLoad(
                normalTexture,
                sampleScreenCoord,
                0
            ).xyz;

            let normalDiff: vec3f = abs(sampleNormal - normal.xyz);
            let fNormalLengthSquared: f32 = dot(normalDiff, normalDiff);
            if(fNormalLengthSquared >= uniformBuffer.mfNormalThreshold)
            {
                bIsLine = true;
                break;
            }

            let sampleWorldPosition: vec4f = textureLoad(
                worldPositionTexture,
                sampleScreenCoord,
                0
            );
            let iSampleMesh: i32 = i32(ceil((sampleWorldPosition.w - fract(sampleWorldPosition.w) - 0.5f)));
            if(iSampleMesh != iMesh && uniformBuffer.mfNormalThreshold < 1.0f)
            {
                bIsLine = true;
                break;
            }
        }
    }

    out.mOutput = vec4f(1.0f, 1.0f, 1.0f, 1.0f);
    if(bIsLine)
    {
        out.mOutput = vec4f(0.0f, 0.0f, 0.0f, 1.0f);
    }

    return out;
}
