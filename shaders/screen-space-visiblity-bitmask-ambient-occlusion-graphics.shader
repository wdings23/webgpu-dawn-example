const UINT32_MAX: u32 = 1000000;
const FLT_MAX: f32 = 1.0e+10;
const PI: f32 = 3.14159f;
const kfOneOverMaxBlendFrames: f32 = 1.0f / 10.0f;

struct VertexInput 
{
    @location(0) pos : vec4<f32>,
    @location(1) texCoord: vec2<f32>,
    @location(2) normal : vec4<f32>
};
struct VertexOutput 
{
    @builtin(position) pos: vec4f,
    @location(0) uv: vec2f,
};
struct FragmentOutput 
{
    @location(0) ambientOcclusion: vec4<f32>,
};

struct UniformData
{
    miFrame: u32,
    miScreenWidth: u32,
    miScreenHeight: u32,
    mfRand: f32,

    mViewProjectionMatrix: mat4x4<f32>,
    
    miStep: u32,

};

struct RandomResult 
{
    mfNum: f32,
    miSeed: u32,
};

struct SVGFFilterResult
{
    mRadiance: vec3<f32>,
    mMoments: vec3<f32>,
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

@group(0) @binding(0)
var worldPositionTexture: texture_2d<f32>;

@group(0) @binding(1)
var normalTexture: texture_2d<f32>;

@group(0) @binding(2)
var materialTexture: texture_2d<f32>;

@group(1) @binding(0)
var<uniform> uniformData: UniformData;

@group(1) @binding(1)
var<uniform> defaultUniformData: DefaultUniformData;

@group(1) @binding(2)
var textureSampler: sampler;

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
    let kiNumSlices: i32 = 16;
    let kfThickness: f32 = 0.1f;
    let kiNumSectors: i32 = 32;
    let kfMaxAOLength: f32 = 2.0f;

    let aspect: vec2<f32> = vec2<f32>(f32(defaultUniformData.miScreenHeight), f32(defaultUniformData.miScreenWidth)) / f32(defaultUniformData.miScreenWidth);

    var out: FragmentOutput;

    let worldPosition: vec4<f32> = textureSample(
        worldPositionTexture, 
        textureSampler, 
        in.uv);

    let position: vec3<f32> = worldPosition.xyz;
    if(worldPosition.w <= 0.0f)
    {
        out.ambientOcclusion = vec4<f32>(0.0f, 0.0f, 0.0f, 0.0f);
    }

    let normal: vec4<f32> = textureSample(
        normalTexture, 
        textureSampler, 
        in.uv);
    
    ///////////////////////////////////////////////////////////

    var fVisibility: f32 = 0.0f;

    let material: vec4<f32> = textureSample(
        materialTexture,
        textureSampler,
        in.uv
    );

    let clipSpace: vec3f = vec3f(normal.w, material.w, worldPosition.w);

    // transform world position and normal to view space
    let direction: vec3<f32> = normalize(-clipSpace.xyz);
    var up: vec3<f32> = vec3<f32>(0.0f, 1.0f, 0.0f);
    if(abs(direction.y) > 0.98f)
    {
        up = vec3<f32>(0.0f, 0.0f, 1.0f);
    }
    let tangent: vec3<f32> = normalize(cross(up, direction));
    let binormal: vec3<f32> = normalize(cross(direction, tangent));
    let viewClipSpace: vec3<f32> = vec3<f32>(
        dot(clipSpace.xyz, tangent),
        dot(clipSpace.xyz, binormal),
        dot(clipSpace.xyz, direction),
    );
    let viewNormal: vec3<f32> = vec3<f32>(
        dot(normal.xyz, tangent),
        dot(normal.xyz, binormal),
        dot(normal.xyz, direction)
    );     

    let fSampleRadius: f32 = 24.0f; // f32(kiNumSlices) * 0.5f;
    let uvStep: vec2<f32> = vec2<f32>(
        1.0f / f32(defaultUniformData.miScreenWidth), 
        1.0f / f32(defaultUniformData.miScreenHeight)) * fSampleRadius;

    let fAngleInc: f32 = PI / f32(kiNumSectors);
    
    var fNumHitSamples: f32 = 0.0f;
    var fSampleCount: f32 = 0.0f;

    var fAO: f32 = 0.0f;

    let fAngleDiff: f32 = 2.0f;

    var totalRadiance: vec3<f32> = vec3<f32>(0.0f, 0.0f, 0.0f);

    // sample multiple hemispheres
    for(var iSlice: i32 = 0; iSlice < kiNumSlices; iSlice++)
    {
        let fPhi: f32 = (f32(iSlice) / f32(kiNumSlices)) * PI;
        let omega: vec2<f32> = vec2<f32>(cos(fPhi), sin(fPhi));

        var iBitMask: u32 = 0u;
        var iAOBitMask: u32 = 0u;
        var iRadianceBitMask: u32 = 0u;
        for(var iSector: i32 = -kiNumSectors / 2; iSector < kiNumSectors / 2; iSector++)
        {
            // sample view clip space position and normal
            let sampleUV: vec2<f32> = in.uv.xy + omega * uvStep * (f32(iSector) / f32(kiNumSectors / 2));
            let sampleNormal: vec4<f32> = textureSample(
                normalTexture,
                textureSampler,
                sampleUV
            );
            let sampleWorldPosition: vec4<f32> = textureSample(
                worldPositionTexture,
                textureSampler,
                sampleUV
            );
            let sampleMaterial: vec4<f32> = textureSample(
                materialTexture,
                textureSampler,
                sampleUV
            );

            var sampleClipSpace: vec3<f32> = vec3<f32>(
                sampleNormal.w, sampleMaterial.w, sampleWorldPosition.w  
            );

            let sampleViewClipSpace: vec3<f32> = vec3<f32>(
                dot(sampleClipSpace, tangent),
                dot(sampleClipSpace, binormal),
                dot(sampleClipSpace, direction),
            );
            let sampleViewNormal: vec3<f32> = vec3<f32>(
                dot(sampleNormal.xyz, tangent),
                dot(sampleNormal.xyz, binormal),
                dot(sampleNormal.xyz, direction)
            );
            
            var iAOMult: u32 = 1u;
            if(length(sampleWorldPosition.xyz - worldPosition.xyz) > kfMaxAOLength)
            {
                iAOMult = 0u;
            }

            let fClipSpaceLength: f32 = length(sampleViewClipSpace - viewClipSpace);
            let sampleViewDiff: vec3<f32> = normalize((sampleViewClipSpace - viewClipSpace) + vec3<f32>(0.00001f, 0.0f, 0.0f));
            let fWorldDiffLength: f32 = max(length(sampleWorldPosition.xyz - worldPosition.xyz), 1.0f);

            let dir: vec3<f32> = vec3<f32>(0.0f, 0.0f, 1.0f);
            var viewDir: vec3<f32> = vec3<f32>(0.0f, 0.0f, 1.0f);
            
            // front and back angles
            let fAngle0: f32 = getAngle(
                viewClipSpace, 
                sampleViewClipSpace,
                viewDir);
            let fAngle1: f32 = clamp(fAngle0 - fAngleInc * kfThickness, 0.0f, PI);

            let iAngle0: f32 = clamp(ceil((fAngle0 / PI) * f32(kiNumSectors)), 0.0f, 32.0f);
            let iAngle1: f32 = clamp(iAngle0 - fAngleDiff, 0.0f, 32.0f);

            let iSampleBitMask: u32 = ((u32(pow(2.0f, f32(iAngle0 - iAngle1))) - 1u) << u32(iAngle1));
            let iSampleBitMaskAO: u32 = iSampleBitMask * iAOMult;

            let origNormalDP: f32 = max(dot(viewNormal, sampleViewDiff), 0.0f);
            let sampleNormalDP: f32 = max(dot(sampleViewNormal, -sampleViewDiff), 0.0f);

            iAOBitMask = iAOBitMask | iSampleBitMaskAO;
            iRadianceBitMask = iRadianceBitMask | iSampleBitMask;

            fSampleCount += 1.0f;
        }

        fAO += 1.0f - f32(CountBits(iAOBitMask)) / f32(kiNumSectors);
    }

    fAO /= f32(kiNumSlices);
    //fAO = smoothstep(0.0f, 1.0f, smoothstep(0.0f, 1.0f, fAO));
    fAO = smoothstep(0.0f, 1.0f, fAO);

    out.ambientOcclusion = vec4<f32>(fAO, fAO, fAO, 1.0f);
    
    return out;
}

//////
fn getAngle(
    viewClipSpace: vec3<f32>, 
    sampleViewClipSpace: vec3<f32>,
    viewDir: vec3<f32>) -> f32
{
    //let viewDir: vec3<f32> = vec3<f32>(0.0f, 0.0f, -1.0f);

    // project the view vector (0, 0, 1) to the position difference
    let positionDiff: vec3<f32> = sampleViewClipSpace - viewClipSpace;
    let fViewDP: f32 = dot(positionDiff, viewDir);
    let projectedView: vec3<f32> = vec3<f32>(0.0f, 0.0f, fViewDP);

    // get the x direction length
    let diff: vec3<f32> = positionDiff - projectedView;
    let fDiffSign: f32 = sign(diff.x);
    let fDiffLength: f32 = length(diff);

    // atan2 the y direction length (view direction length) x direction length (position difference to view direction)
    let fAngle: f32 = clamp(atan2(fDiffLength * fDiffSign, fViewDP) + PI * 0.5f, 0.0f, PI);     // -x is at -PI/2, apply that for 0.0

    return fAngle;
}

/////
fn CountBits(val: u32) -> u32 
{
    var iVal: u32 = val;

    //Counts the number of 1:s
    //https://www.baeldung.com/cs/integer-bitcount
    iVal = (iVal & 0x55555555)+((iVal >> 1) & 0x55555555);
    iVal = (iVal & 0x33333333)+((iVal >> 2) & 0x33333333);
    iVal = (iVal & 0x0F0F0F0F)+((iVal >> 4) & 0x0F0F0F0F);
    iVal = (iVal & 0x00FF00FF)+((iVal >> 8) & 0x00FF00FF);
    iVal = (iVal & 0x0000FFFF)+((iVal >> 16) & 0x0000FFFF);
    return iVal;
}

