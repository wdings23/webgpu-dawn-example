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
    @location(0) mAmbientOcclusion: vec4<f32>,
    @location(1) mIndirectLighting: vec4<f32>,
};

struct UniformData
{
    mfSampleRadius: f32,
    mfNumSlices: f32,
    mfNumSections: f32,
    mfThickness: f32,
    mfMinAOPct: f32,
    mfMaxAOPct: f32,
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
var textureCoordAndClipSpaceTexture: texture_2d<f32>;

@group(0) @binding(3)
var lightingTexture: texture_2d<f32>;

@group(1) @binding(0)
var<uniform> uniformData: UniformData;

@group(1) @binding(1)
var<storage, read> blueNoiseBuffer: array<vec2f>;

@group(1) @binding(2)
var<uniform> defaultUniformBuffer: DefaultUniformData;

@group(1) @binding(3)
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
    var out: FragmentOutput = ao(in);
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

/////
fn ao(in: VertexOutput) -> FragmentOutput
{
    var out: FragmentOutput;

    let fDirection: f32 = 1.0f;

    let kiNumSections: u32 = 16u;
    let kiNumSlices: u32 = 16u;
    var kfThickness: f32 = 0.01f;

    let screenCoord: vec2i = vec2i(
        i32(in.uv.x * f32(defaultUniformBuffer.miScreenWidth)),
        i32(in.uv.y * f32(defaultUniformBuffer.miScreenHeight))
    );

    //var worldPosition: vec4f = textureSample(
    //    worldPositionTexture, 
    //    textureSampler, 
    //    in.uv);

    var worldPosition: vec4f = textureLoad(
        worldPositionTexture, 
        screenCoord,
        0);

    if(worldPosition.w <= 0.0f)
    {
        out.mAmbientOcclusion = vec4f(1.0f, 1.0f, 1.0f, 0.0f);
        return out;
    }

    //let normal: vec4f = textureSample(
    //    normalTexture, 
    //    textureSampler, 
    //    in.uv);
    let normal: vec4f = textureLoad(
        normalTexture, 
        screenCoord, 
        0);

    var viewMatrix: mat4x4<f32> = defaultUniformBuffer.mViewMatrix;
    viewMatrix[0] = vec4f(viewMatrix[0].xyz, 0.0f);
    viewMatrix[1] = vec4f(viewMatrix[1].xyz, 0.0f);
    viewMatrix[2] = vec4f(viewMatrix[2].xyz, 0.0f);
    
    let cameraPosition: vec3f = defaultUniformBuffer.mCameraPosition.xyz;
    var viewSpaceNormal: vec4f = vec4f(normal.xyz, 1.0f) * viewMatrix;
    viewSpaceNormal.z = -viewSpaceNormal.z;
    let fSampleRadius: f32 = uniformData.mfSampleRadius;

    var iBlueNoiseIndex: i32 = (screenCoord.y * defaultUniformBuffer.miScreenWidth + screenCoord.x/* + defaultUniformBuffer.miFrame*/) % 128;
    let fRadius: f32 = blueNoiseBuffer[iBlueNoiseIndex].x * fSampleRadius;

    var textureSize: vec2u = textureDimensions(worldPositionTexture);
    let uvStep: vec2f = vec2<f32>(
        1.0f / f32(textureSize.x), 
        1.0f / f32(textureSize.y)) * fSampleRadius;

    let viewPosition: vec3f = (cameraPosition - worldPosition.xyz) * fDirection;
    var viewDirection: vec3f = normalize(viewPosition * -1.0f);

    // sample multiple slices around the view direction
    var iTotalBits: u32 = 0u;
    var iCountedBits: u32 = 0u;
    var totalIndirectLighting: vec3f = vec3f(0.0f, 0.0f, 0.0f);
    for(var iSlice: u32 = 0; iSlice < kiNumSlices; iSlice++)
    {
        // These steps are essentially for patitioning the middle of the slide to integrate over
        // Get the cosine of projected view space normal to the slice and view direction by:
        // 1) 2d direction in screen space of the slice
        // 2) slice plane normal is thecross product of screen space direction and view direction
        // 3) get the projected length of the view space normal to the slice plane normal to apply to the plane normal 
        // 4) subtract the view space normal with projected plane normal to get the projected view space to slice normal
        // 5) cosine of the view direction on the slice plane is the dot product of projected normal and view direction
        iBlueNoiseIndex = (iBlueNoiseIndex + i32(iSlice)) % 128;
        let fPhi: f32 = f32(iSlice) * ((2.0f * PI) / f32(kiNumSlices));
        //let fSampleLength: f32 = sqrt(blueNoiseBuffer[iBlueNoiseIndex].x * blueNoiseBuffer[iBlueNoiseIndex].x + blueNoiseBuffer[iBlueNoiseIndex].y + blueNoiseBuffer[iBlueNoiseIndex].y);
        //var fPhi: f32 = atan2(blueNoiseBuffer[iBlueNoiseIndex].y, blueNoiseBuffer[iBlueNoiseIndex].x);
        //if(fPhi < 0.0f)
        //{
        //    fPhi = 2.0f * PI - fPhi;
        //}
        let omega: vec2f = vec2f(cos(fPhi), sin(fPhi));
        let screenSpaceDirection: vec3f =  vec3f(omega.x, omega.y, 0.0f);
        let orthoDirection: vec3f = screenSpaceDirection - (viewDirection * dot(screenSpaceDirection, viewDirection));
        let axis: vec3f = cross(viewDirection, orthoDirection);
        let fProjectedLength: f32 = dot(viewSpaceNormal.xyz, axis);
        let projectedAxis: vec3f = axis * fProjectedLength;
        let projectedNormal: vec3f = viewSpaceNormal.xyz - projectedAxis;
        let fProjectedNormalLength: f32 = length(projectedNormal);
        
        // angle between view vector and projected normal vector
        let fCosineProjectedNormal: f32 = clamp(dot(projectedNormal, viewDirection) / (fProjectedNormalLength + 0.0001f), 0.0f, 1.0f);
        let sliceTangent: vec3f = cross(viewDirection, axis);
        let fViewToProjectedNormalAngle: f32 = -sign(dot(projectedNormal, orthoDirection)) * acos(fCosineProjectedNormal);

        // sections on the slice
        var iAOBitMask: u32 = 0u;
        var fSampleDirection: f32 = 1.0f;
        var iOccludedBits: u32 = 0u;
        var sectionLighting: vec3f = vec3f(0.0f, 0.0f, 0.0f);
        for(var iDirection: u32 = 0u; iDirection < 2u; iDirection++)
        {
            fSampleDirection = 1.0f;
            if(iDirection > 0)
            {
                fSampleDirection = -1.0f;
            }

            for(var iSection: u32 = 0u; iSection < kiNumSections; iSection++)
            {
                // sample view clip space position and normal
                let sampleUV: vec2f = in.uv.xy + omega * uvStep * fSampleDirection * f32(iSection);
                let sampleScreenCoord: vec2i = vec2i(
                    i32(sampleUV.x * f32(defaultUniformBuffer.miScreenWidth)),
                    i32(sampleUV.y * f32(defaultUniformBuffer.miScreenHeight))
                );
                var sampleWorldPosition: vec4f = textureLoad(
                    worldPositionTexture,
                    sampleScreenCoord,
                    0
                );
                if(sampleWorldPosition.w <= 0.0f)
                {
                    continue;
                }
                var sampleNormal: vec4f = textureLoad(
                    normalTexture,
                    sampleScreenCoord,
                    0
                );
                var sampleLighting: vec4f = textureLoad(
                    lightingTexture,
                    sampleScreenCoord,
                    0
                );

                let sampleViewPosition: vec3f = (cameraPosition - sampleWorldPosition.xyz) * fDirection;

                // sample view position - current view position
                let deltaViewSpacePosition: vec3f = (viewPosition.xyz - sampleViewPosition.xyz) * fDirection;
                let fDeltaViewSpaceLength: f32 = dot(deltaViewSpacePosition, deltaViewSpacePosition);
                
                // front and back horizon angle
                let backDeltaViewPosition: vec3f = deltaViewSpacePosition - viewDirection * uniformData.mfThickness;
                var fHorizonAngleFront: f32 = dot(deltaViewSpacePosition / (fDeltaViewSpaceLength + 0.0001f), viewDirection);
                var fHorizonAngleBack: f32 = dot(normalize(backDeltaViewPosition), viewDirection);

                fHorizonAngleFront = acos(fHorizonAngleFront);
                fHorizonAngleBack = acos(fHorizonAngleBack);

                // convert to percentage relative projected normal angle as the middle angle
                let fMinAngle: f32 = fViewToProjectedNormalAngle - PI * 0.5f;
                let fMaxAngle: f32 = fViewToProjectedNormalAngle + PI * 0.5f;
                let fPct0: f32 = clamp((fHorizonAngleFront - fMinAngle) / PI, 0.0f, 1.0f);
                let fPct1: f32 = clamp((fHorizonAngleBack - fMinAngle) / PI, 0.0f, 1.0f);
                var horizonAngle: vec2f = vec2f(fPct1, fPct0);
                //if(fSampleDirection < 0.0f)
                //{
                //    horizonAngle = vec2f(fPct0, fPct1);
                //}
                
                // set the section bit for this sample
                let iStartHorizon: u32 = u32(horizonAngle.x * f32(kiNumSections));
                let fHorizonAngle: f32 = ceil((horizonAngle.x - horizonAngle.y) * f32(kiNumSections));
                var iAngleHorizon: u32 = 0u;
                if(fHorizonAngle > 0.0f) 
                {
                    iAngleHorizon = 1u;
                }
                if(iAngleHorizon > 0u)
                {
                    iOccludedBits |= (1u << iStartHorizon);

                    sectionLighting += sampleLighting.xyz * 
                        max(dot(normal.xyz, normalize(deltaViewSpacePosition.xyz)), 0.0f) * 
                        max(dot(sampleNormal.xyz, normalize(deltaViewSpacePosition.xyz * -1.0f)), 0.0f);
                }
            
            }   // for slice

            let iNumBits: u32 = CountBits(iOccludedBits);
            iCountedBits += iNumBits;

            sectionLighting /= (f32(iNumBits) + 0.001f);

        }   // for direction

        iTotalBits += kiNumSections;

        totalIndirectLighting += sectionLighting;

    }   // for slice

    var fAO: f32 = 1.0f - f32(iCountedBits) / (f32(iTotalBits) + 0.001f);
    let fMin: f32 = 0.0f;
    let fMax: f32 = 0.7f;
    fAO = (clamp(fAO, fMin, fMax) - fMin) / (fMax - fMin);
    
    let kfReflectivity: f32 = 0.9f;

    //fAO = smoothstep(0.0f, 1.0f, smoothstep(0.0f, 1.0f, smoothstep(0.0f, 1.0f, fAO)));
    out.mAmbientOcclusion = vec4f(fAO, fAO, fAO, 1.0f);
    out.mIndirectLighting = vec4f((totalIndirectLighting.xyz / f32(kiNumSlices)) * kfReflectivity, 1.0f);

    return out;
}