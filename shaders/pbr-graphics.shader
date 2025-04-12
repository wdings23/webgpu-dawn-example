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
    mCameraLookDir: vec4<f32>,

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
var materialOutputTexture: texture_2d<f32>;

@group(1) @binding(0)
var<storage, read> aMeshExtents: array<MeshExtent>;

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

    var iMesh: i32 = i32(ceil(worldPosition.w - 0.5f));
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

    var color = pbr(
        worldPosition.xyz,
        normal.xyz,
        0.2f,
        0.4f);
    color = clamp(color + vec3f(0.4f, 0.4f, 0.4f), vec3f(0.0f, 0.0f, 0.0f), vec3f(1.0f, 1.0f, 1.0f));

    out.mOutput = vec4f(color.x, color.y, color.z, 1.0f);
    return out;
}

/*
**
*/
fn pbr(worldPosition: vec3f,
       normal: vec3f, 
       roughness: f32,
       metallic: f32) -> vec3f
{
    let albedo: vec3f = vec3f(0.7f, 0.7f, 0.7f);

    let totalMeshExtent: MeshExtent = aMeshExtents[defaultUniformBuffer.miNumMeshes];
    let totalCenter: vec3f = (totalMeshExtent.mMaxPosition.xyz + totalMeshExtent.mMinPosition.xyz) * 0.5f;
    let totalSize: f32 = length(totalMeshExtent.mMaxPosition.xyz - totalMeshExtent.mMinPosition.xyz);

    var lightPositions: array<vec3f, 4>;
    lightPositions[0] = totalCenter + vec3f(0.0f, 1.0f * totalSize * 0.25f, 1.0f * totalSize * 0.25f);
    lightPositions[1] = totalCenter + vec3f(1.0f * totalSize * 0.25f, 1.0f * totalSize * 0.25f, 1.0f * totalSize * 0.25f);
    lightPositions[2] = totalCenter + vec3f(-1.0f * totalSize * 0.25f, 1.0f * totalSize * 0.25f, -1.0f * totalSize * 0.25f);
    lightPositions[3] = totalCenter + vec3f(0.0f, 1.0f * totalSize * 0.5f, -1.0f * totalSize * 0.25f);

    var lightColors: array<vec3f, 4>;
    lightColors[0] = vec3f(100.0f, 100.0f, 100.0f);
    lightColors[1] = vec3f(100.0f, 100.0f, 100.0f);
    lightColors[2] = vec3f(100.0f, 100.0f, 100.0f);
    lightColors[3] = vec3f(100.0f, 100.0f, 100.0f);

    let view: vec3f = normalize(defaultUniformBuffer.mCameraPosition.xyz - worldPosition);

    // calculate reflectance at normal incidence; if dia-electric (like plastic) use F0 
    // of 0.04 and if it's a metal, use the albedo color as F0 (metallic workflow)    
    var F0: f32 = 0.04f; 
    F0 = mix(F0, 0.6f, metallic);

    // reflectance equation
    var Lo: vec3f = vec3f(0.0f, 0.0f, 0.0f);
    for(var i: i32 = 0; i < 4; i += 1) 
    {
        // calculate per-light radiance
        let L: vec3f = normalize(lightPositions[i] - worldPosition);
        let H: vec3f = normalize(view + L);
        let distance: f32 = length(lightPositions[i] - worldPosition);
        let attenuation: f32 = 1.0f / (distance * distance);
        let radiance: vec3f = lightColors[i] * attenuation;

        // Cook-Torrance BRDF
        let NDF: f32 = DistributionGGX(normal, H, roughness);   
        let G: f32   = GeometrySmith(normal, view, L, roughness);      
        let F: vec3f    = fresnelSchlick(max(dot(H, view), 0.0f), vec3f(F0, F0, F0));
           
        let numerator: vec3f    = NDF * G * F; 
        let denominator: f32 = 4.0f * max(dot(normal, view), 0.0) * max(dot(normal, L), 0.0) + 0.0001f; // + 0.0001 to prevent divide by zero
        let specular: vec3f = numerator / denominator;
        
        // kS is equal to Fresnel
        let kS: vec3f = F;
        // for energy conservation, the diffuse and specular light can't
        // be above 1.0 (unless the surface emits light); to preserve this
        // relationship the diffuse component (kD) should equal 1.0 - kS.
        var kD: vec3f = vec3f(1.0f, 1.0f, 1.0f) - kS;
        // multiply kD by the inverse metalness such that only non-metals 
        // have diffuse lighting, or a linear blend if partly metal (pure metals
        // have no diffuse light).
        kD *= 1.0 - metallic;	  

        // scale light by NdotL
        let NdotL: f32 = max(dot(normal, L), 0.0f);        

        // add to outgoing radiance Lo
        Lo += (kD * albedo / PI + specular) * radiance * NdotL;  // note that we already multiplied the BRDF by the Fresnel (kS) so we won't multiply by kS again
    }   
    
    // ambient lighting (note that the next IBL tutorial will replace 
    // this ambient lighting with environment lighting).
    let ambient: vec3f = vec3f(0.03f, 0.03f, 0.03f) * albedo;
    
    var color: vec3f = ambient + Lo;

    // HDR tonemapping
    color = color / (color + vec3f(1.0f, 1.0f, 1.0f));
    // gamma correct
    color = pow(color, vec3f(1.0f/2.2f, 1.0f / 2.2f, 1.0f / 2.2f)); 

    return color;
}

fn DistributionGGX(
    N: vec3f, 
    H: vec3f, 
    roughness: f32) -> f32
{
    let a: f32 = roughness*roughness;
    let a2: f32 = a*a;
    let NdotH: f32 = max(dot(N, H), 0.0);
    let NdotH2: f32 = NdotH*NdotH;

    let nom: f32   = a2;
    var denom: f32 = (NdotH2 * (a2 - 1.0) + 1.0);
    denom = PI * denom * denom;

    return nom / denom;
}
// ----------------------------------------------------------------------------
fn GeometrySchlickGGX(
    NdotV: f32, 
    roughness: f32) -> f32
{
    let r: f32 = (roughness + 1.0f);
    let k: f32 = (r*r) / 8.0f;

    let nom: f32   = NdotV;
    let denom: f32 = NdotV * (1.0f - k) + k;

    return nom / denom;
}
// ----------------------------------------------------------------------------
fn GeometrySmith(
    N: vec3f, 
    V: vec3f, 
    L: vec3f, 
    roughness: f32) -> f32
{
    let NdotV: f32 = max(dot(N, V), 0.0f);
    let NdotL: f32 = max(dot(N, L), 0.0f);
    let ggx2: f32 = GeometrySchlickGGX(NdotV, roughness);
    let ggx1: f32 = GeometrySchlickGGX(NdotL, roughness);

    return ggx1 * ggx2;
}
// ----------------------------------------------------------------------------
fn fresnelSchlick(
    cosTheta: f32, 
    F0: vec3f) -> vec3f
{
    return F0 + (1.0f - F0) * pow(clamp(1.0f - cosTheta, 0.0f, 1.0f), 5.0f);
}