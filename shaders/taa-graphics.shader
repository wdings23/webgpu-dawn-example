const FLT_MAX: f32 = 1000000.0f;
const NUM_HISTORY: f32 = 60.0f;

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
var radianceTexture: texture_2d<f32>;

@group(0) @binding(2) 
var prevRadianceTexture: texture_2d<f32>;

@group(0) @binding(3) 
var motionVectorTexture: texture_2d<f32>;

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

    let textureSize: vec2<u32> = textureDimensions(radianceTexture);
    let fOneOverWidth: f32 = 1.0f / f32(textureSize.x);
    let fOneOverHeight: f32 = 1.0f / f32(textureSize.y);

    let uv: vec2<f32> = vec2<f32>(
        in.uv.x,
        in.uv.y
    );

    var motionVector: vec4<f32> = textureSample(
        motionVectorTexture,
        textureSampler,
        uv
    );
    //motionVector = motionVector * 2.0f - 1.0f;
    let prevUV: vec2<f32> = uv.xy - motionVector.xy;

    var iSampleSize: i32 = 1;
    var minRadiance: vec3<f32> = vec3<f32>(FLT_MAX, FLT_MAX, FLT_MAX);
    var maxRadiance: vec3<f32> = vec3<f32>(-FLT_MAX, -FLT_MAX, -FLT_MAX);
    var totalRadiance: vec3<f32> = vec3<f32>(0.0f, 0.0f, 0.0f);
    var totalPrevRadiance: vec3<f32> = vec3<f32>(0.0f, 0.0f, 0.0f);
    var fTotalPrevWeight: f32 = 0.0f;
    for(var iY: i32 = -iSampleSize; iY <= iSampleSize; iY++)
    {
        for(var iX: i32 = -iSampleSize; iX <= iSampleSize; iX++)
        {
            let offset: vec2<f32> = vec2<f32>(
                f32(iX) * fOneOverWidth,
                f32(iY) * fOneOverHeight
            );

            let sampleUV: vec2<f32> = uv + offset;
            let sampleRadiance: vec3<f32> = textureSample(
                radianceTexture, 
                textureSampler,
                sampleUV
            ).xyz;

            totalRadiance += sampleRadiance;

            minRadiance = min(sampleRadiance, minRadiance);
            maxRadiance = max(sampleRadiance, maxRadiance);

            //var fWeight: f32 = exp(-3.0f * f32(iX * iX + iY * iY) / f32(3 * 3));
            var fWeight: f32 = 1.0f;

            let samplePrevUV: vec2<f32> = prevUV + offset;
            let samplePrevRadiance: vec4<f32> = textureSample(
                prevRadianceTexture,
                textureSampler,
                samplePrevUV
            );
            totalPrevRadiance += samplePrevRadiance.xyz * fWeight;
            fTotalPrevWeight += fWeight;
        }
    }

    let radiance: vec4<f32> = textureSample(
        radianceTexture,
        textureSampler,
        uv
    );
    let prevRadiance: vec3<f32> = textureSample(
        prevRadianceTexture,
        textureSampler,
        prevUV
    ).xyz;
    var lerpOutput: vec3<f32> = mix(
        //clamp(prevRadiance, minRadiance, maxRadiance),
        clamp(totalPrevRadiance.xyz / fTotalPrevWeight, minRadiance, maxRadiance),
        //radiance.xyz,
        totalRadiance.xyz / fTotalPrevWeight,
        (1.0f / NUM_HISTORY)
    );

    out.mOutput.x = lerpOutput.x;
    out.mOutput.y = lerpOutput.y;
    out.mOutput.z = lerpOutput.z;
    out.mOutput.w = radiance.w;

    return out;
}