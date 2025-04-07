const PI: f32 = 3.14159f;

struct UniformData
{
    miNumMeshes: u32,
    mfExplodeMultiplier: f32,
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

struct Material
{
    mDiffuse: vec4<f32>,
    mSpecular: vec4<f32>,
    mEmissive: vec4<f32>,

    miID: u32,
    miAlbedoTextureID: u32,
    miNormalTextureID: u32,
    miEmissiveTextureID: u32
};

struct Range
{
    miStart: i32,
    miEnd: i32
};

struct MeshExtent
{
    mMinPosition: vec4<f32>,
    mMaxPosition: vec4<f32>,
};

struct SelectMeshInfo
{
    miMeshID: u32,
    miSelectionX: i32,
    miSelectionY: i32,
    mMinPosition: vec3<f32>,
    mMaxPosition: vec3<f32>,
};

@group(1) @binding(0)
var<uniform> uniformBuffer: UniformData;

@group(1) @binding(1)
var<storage> aMaterials: array<Material>;

@group(1) @binding(2)
var<storage> aMeshMaterialID: array<u32>;

@group(1) @binding(3)
var<storage, read> aMeshTriangleIndexRanges: array<Range>;

@group(1) @binding(4)
var<storage, read> aMeshExtents: array<MeshExtent>;

@group(1) @binding(5)
var<uniform> defaultUniformBuffer: DefaultUniformData;

@group(1) @binding(6)
var textureSampler: sampler;

struct VertexInput {
    @location(0) worldPosition : vec4<f32>,
    @location(1) texCoord: vec4<f32>,
    @location(2) normal : vec4<f32>
};
struct VertexOutput {
    @builtin(position) pos: vec4<f32>,
    @location(0) worldPosition: vec4<f32>,
    @location(1) texCoord: vec4<f32>,
    @location(2) normal: vec4<f32>
};
struct FragmentOutput {
    @location(0) worldPosition : vec4<f32>,
    //@location(1) texCoord : vec4<f32>,
    @location(1) normal: vec4<f32>,
    //@location(3) motionVector: vec4<f32>,
    //@location(2) clipSpace: vec4<f32>,
    @location(2) mMaterial: vec4<f32>,
};


@vertex
fn vs_main(in: VertexInput,
    @builtin(vertex_index) iVertexIndex: u32) -> VertexOutput 
{
    var out: VertexOutput;
    
    //let iMesh: u32 = u32(ceil(in.worldPosition.w - 0.5f));
    let iMesh: u32 = u32(ceil(in.texCoord.z - 0.5f));
    let midPt: vec3f = (aMeshExtents[iMesh].mMaxPosition.xyz + aMeshExtents[iMesh].mMinPosition.xyz) * 0.5f;

    // total mesh extent is at the very end of list
    let totalMeshExtent: MeshExtent = aMeshExtents[defaultUniformBuffer.miNumMeshes];
    let totalCenter: vec3f = (totalMeshExtent.mMaxPosition.xyz + totalMeshExtent.mMinPosition.xyz) * 0.5f;

    var worldPosition: vec4<f32> = vec4<f32>(
        in.worldPosition.x,
        in.worldPosition.y,
        in.worldPosition.z + (totalCenter.z - midPt.z) * max(uniformBuffer.mfExplodeMultiplier, 0.0f),
        1.0f
    );
    out.pos = worldPosition * defaultUniformBuffer.mJitteredViewProjectionMatrix;
    out.worldPosition = vec4f(worldPosition.xyz, f32(iMesh));
    out.texCoord = vec4f(in.texCoord.x, in.texCoord.y, f32(iMesh), 1.0f);
    out.normal = in.normal;
    return out;
}

@fragment
fn fs_main(in: VertexOutput) -> FragmentOutput 
{
    var out: FragmentOutput;
    
    //let iMesh: u32 = u32(ceil(in.worldPosition.w - 0.5f));
    let iMesh: u32 = u32(ceil(in.texCoord.z - 0.5f));
    
    out.worldPosition = vec4<f32>(in.worldPosition.xyz, f32(iMesh));
    //out.texCoord = vec4<f32>(in.texCoord.x, in.texCoord.y, 0.0f, 1.0f);
    let normalXYZ: vec3<f32> = normalize(in.normal.xyz);
    out.normal.x = normalXYZ.x;
    out.normal.y = normalXYZ.y;
    out.normal.z = normalXYZ.z;
    out.normal.w = 1.0f;

    var texCoord: vec4<f32> = vec4<f32>(
        in.texCoord.xy,
        0.0f,
        0.0f);
    if(texCoord.x < 0.0f)
    {
        texCoord.x = fract(abs(1.0f - texCoord.x));
    }
    else if(texCoord.x > 1.0f)
    {
        texCoord.x = fract(texCoord.x);
    }

    if(texCoord.y < 0.0f)
    {
        texCoord.y = fract(abs(1.0f - texCoord.y));
    }
    else if(texCoord.y > 1.0f)
    {
        texCoord.y = fract(texCoord.y);
    }

    //out.texCoord = vec4<f32>(texCoord.x, texCoord.y, 0.0f, 0.0f);

    // store depth and mesh id in worldPosition.w
    out.worldPosition.w = clamp(in.pos.z, 0.0f, 0.999f) + floor(in.worldPosition.w + 0.5f);

    let currClipSpace: vec4<f32> = vec4<f32>(in.worldPosition.xyz, 1.0f) * defaultUniformBuffer.mJitteredViewProjectionMatrix;
    let prevClipSpace: vec4<f32> = vec4<f32>(in.worldPosition.xyz, 1.0f) * defaultUniformBuffer.mPrevViewProjectionMatrix;

    var currClipSpacePos: vec3<f32> = vec3<f32>(
        currClipSpace.x / currClipSpace.w,
        currClipSpace.y / currClipSpace.w,
        currClipSpace.z / currClipSpace.w
    ) * 0.5f + 
    vec3<f32>(0.5f);
    currClipSpacePos.y = 1.0f - currClipSpacePos.y;

    var prevClipSpacePos: vec3<f32> = vec3<f32>(
        prevClipSpace.x / prevClipSpace.w,
        prevClipSpace.y / prevClipSpace.w,
        prevClipSpace.z / prevClipSpace.w
    ) * 0.5f + 
    vec3<f32>(0.5f);
    prevClipSpacePos.y = 1.0f - prevClipSpacePos.y;

    //out.motionVector.x = (currClipSpacePos.x - prevClipSpacePos.x);
    //out.motionVector.y = (currClipSpacePos.y - prevClipSpacePos.y);
    //out.motionVector.z = floor(in.worldPosition.w + 0.5f);      // mesh id
    //out.motionVector.w = currClipSpacePos.z;                    // depth

    var xform: vec4<f32> = vec4<f32>(in.worldPosition.xyz, 1.0f) * defaultUniformBuffer.mJitteredViewProjectionMatrix;
    var fDepth: f32 = xform.z / xform.w;
    //out.clipSpace = vec4<f32>(currClipSpacePos.xyz, 1.0f);
    out.normal = vec4<f32>(normalXYZ, 1.0f);
    
    let lightDir: vec3f = normalize(vec3f(1.0f, -1.0f, 1.0f));
    let fDP: f32 = max(dot(normalXYZ, lightDir), 0.3f);
    out.mMaterial = vec4f(aMaterials[iMesh].mDiffuse.xyz * fDP, 1.0f);

    // encode clip space 
    out.worldPosition.w += currClipSpacePos.z;
    out.normal.w = currClipSpacePos.x;
    out.mMaterial.w = currClipSpacePos.y;

    //out.texCoord.z = f32(iMesh);

    return out;
}

