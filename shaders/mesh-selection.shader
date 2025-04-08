struct UniformData
{
    miSelectedMesh: i32,
    miSelectionX: i32,
    miSelectionY: i32,
};

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
    mCameraLookDir: vec4<f32>,

    mLightRadiance: vec4<f32>,
    mLightDirection: vec4<f32>,
};

struct MeshExtent
{
    mMinPosition: vec4<f32>,
    mMaxPosition: vec4<f32>,
};

struct SelectMeshInfo
{
    miMeshID: i32,
    miSelectionX: i32,
    miSelectionY: i32,
    miPadding: i32,
    mMinMeshPosition: vec4f,
    mMaxMeshPosition: vec4f,
};

@group(0) @binding(0)
var worldPositionTexture: texture_2d<f32>;

@group(0) @binding(1)
var materialOutputTexture: texture_2d<f32>;

@group(1) @binding(0)
var<uniform> uniformBuffer: UniformData;

@group(1) @binding(1)
var<storage, read_write> aSelectedMeshes: array<SelectMeshInfo>;

@group(1) @binding(2)
var<storage, read> aMeshExtents: array<MeshExtent>;

@group(1) @binding(3)
var<uniform> defaultUniformBuffer: DefaultUniformData;

@group(1) @binding(4)
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
    out.mOutput = textureSample(
        materialOutputTexture,
        textureSampler,
        in.uv
    );

    let worldPosition: vec4f = textureSample(
        worldPositionTexture,
        textureSampler,
        in.uv
    );

    var iMesh: i32 = i32(ceil(worldPosition.w - 0.5f));
    if(worldPosition.w == 0.0f)
    {
        iMesh = -1;
    }

    if(uniformBuffer.miSelectionX >= 0 && uniformBuffer.miSelectionY >= 0 && uniformBuffer.miSelectedMesh < 0)
    {
        let iScreenCoordX: i32 = i32(in.uv.x * f32(defaultUniformBuffer.miScreenWidth));
        let iScreenCoordY: i32 = i32(in.uv.y * f32(defaultUniformBuffer.miScreenHeight));

        if(iScreenCoordX == uniformBuffer.miSelectionX && iScreenCoordY == uniformBuffer.miSelectionY)
        {
            aSelectedMeshes[0].miMeshID = i32(iMesh + 1);
            aSelectedMeshes[0].miSelectionX = iScreenCoordX;
            aSelectedMeshes[0].miSelectionY = iScreenCoordY;
            aSelectedMeshes[0].mMinMeshPosition = aMeshExtents[iMesh-1].mMinPosition;
            aSelectedMeshes[0].mMaxMeshPosition = aMeshExtents[iMesh-1].mMaxPosition;
        }
    }

    if(uniformBuffer.miSelectedMesh == i32(iMesh) && iMesh > 0)
    {
        out.mOutput = vec4f(1.0f, 1.0f, 0.0f, 1.0f);
    }

    if(iMesh == -1)
    {
        let backGroundColor: vec3f = mix(vec3f(0.0f, 0.5f, 0.8f), vec3f(0.8f, 0.8f, 0.8f), in.uv.y);
        out.mOutput = vec4f(backGroundColor, 0.0f);
    }

    return out;
}