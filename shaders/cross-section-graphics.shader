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

struct MeshExtent
{
    mMinPosition: vec4<f32>,
    mMaxPosition: vec4<f32>,
};

@group(0) @binding(0)
var worldPositionTexture: texture_2d<f32>;

@group(0) @binding(1)
var normalTexture: texture_2d<f32>;

@group(0) @binding(2)
var depthTexture: texture_2d<f32>;

@group(1) @binding(0)
var<uniform> defaultUniformBuffer: DefaultUniformData;

@group(1) @binding(1)
var textureSampler: sampler;

struct VertexOutput 
{
    @builtin(position) pos: vec4f,
    @location(0) uv: vec2f,
};

struct FragmentOutput 
{
    @location(0) mOutput : vec4<f32>,
    @location(1) mDebug0 : vec4<f32>,
    @location(2) mDebug1 : vec4<f32>,
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

    out.mOutput = vec4f(0.0f, 0.0f, 0.0f, 0.0f);

    var iMesh: i32 = i32(ceil(worldPosition.w - 0.5f));
    if(worldPosition.w == 0.0f)
    {
        return out;
    }

    let normal: vec4f = textureLoad(
        normalTexture,
        screenCoord,
        0
    );

    let sampleScreenCoordX0: vec2i = vec2i(
        i32(f32(defaultUniformBuffer.miScreenWidth) * in.uv.x - 1),
        i32(f32(defaultUniformBuffer.miScreenHeight) * in.uv.y) 
    );
    let sampleScreenCoordX1: vec2i = vec2i(
        i32(f32(defaultUniformBuffer.miScreenWidth) * in.uv.x + 1),
        i32(f32(defaultUniformBuffer.miScreenHeight) * in.uv.y) 
    );

    let sampleScreenCoordY0: vec2i = vec2i(
        i32(f32(defaultUniformBuffer.miScreenWidth) * in.uv.x),
        i32(f32(defaultUniformBuffer.miScreenHeight) * in.uv.y - 1) 
    );
    let sampleScreenCoordY1: vec2i = vec2i(
        i32(f32(defaultUniformBuffer.miScreenWidth) * in.uv.x),
        i32(f32(defaultUniformBuffer.miScreenHeight) * in.uv.y + 1) 
    );

    let fDepth0: f32 = textureLoad(
        depthTexture,
        sampleScreenCoordX0,
        0
    ).x;
    let fDepth1: f32 = textureLoad(
        depthTexture,
        sampleScreenCoordX1,
        0
    ).x;

    let fDepth2: f32 = textureLoad(
        depthTexture,
        sampleScreenCoordY0,
        0
    ).x;
    let fDepth3: f32 = textureLoad(
        depthTexture,
        sampleScreenCoordY1,
        0
    ).x;

    let d: vec3f = vec3f((fDepth1 - fDepth0) * 0.5f, (fDepth3 - fDepth2) * 0.5f, 1.0f);
    let n: vec3f = normalize(d);

    let worldPositionToEye: vec3f = normalize(worldPosition.xyz - defaultUniformBuffer.mCameraPosition.xyz);
    var up: vec3f = vec3f(0.0f, -1.0f, 0.0f);
    if(abs(worldPositionToEye.y) >= 0.99f)
    {
        up = vec3f(1.0f, 0.0f, 0.0f);
    }

    let tangent: vec3f = cross(up, worldPositionToEye);
    let binormal: vec3f = cross(worldPositionToEye, tangent);
    var viewSpaceNormal: vec3f = vec3f(
        dot(tangent.xyz, normal.xyz),
        dot(binormal.xyz, normal.xyz),
        dot(worldPositionToEye.xyz, normal.xyz)
    );

    let eyeDir: vec3f = normalize(defaultUniformBuffer.mCameraLookAt.xyz - defaultUniformBuffer.mCameraPosition.xyz);
    let fDP: f32 = dot(eyeDir, viewSpaceNormal.xyz);

    out.mDebug0 = vec4f(viewSpaceNormal.xyz, 1.0f);

    if(viewSpaceNormal.z <= 0.0f)
    {
        out.mOutput = vec4f(1.0f, 0.0f, 0.0f, 1.0f);
    }
    
    return out;
}

