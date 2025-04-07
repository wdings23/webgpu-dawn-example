@group(0) @binding(0) var depthTexture : texture_2d<f32>;
@group(0) @binding(1) var textureSampler : sampler;

struct VertexOutput 
{
    @builtin(position) pos: vec4f,
    @location(0) uv: vec2f,
};

struct PSOutput
{
    @location(0) mDepthOutput : vec4f;
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
    var out: PSOutput;

    let depth: vec4f = depthTexture.Gather(
        sampler,
        in.texCoord.xy
    );

    let fMaxDepth: f32 = max(max(max(depth.x, depth.y), depth.z), depth.w);
    out.mDepthOutput = vec4f(fMaxDepth, fMaxDepth, fMaxDepth, 1.0f);

    return out;
}

ls
