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

struct MSDFInfo
{
    glyphIdx: i32,
    left_bearing: i32,
    advance: i32,
    rgb: array<f32, 3>,
    width: i32,
    height: i32,
    yOffset: i32,
};

struct OutputGlyphInfo
{
    mSDFResult: MSDFInfo,
    miAtlasX: u32,
    miAtlasY: u32,
    miASCII: u32,
};

struct Coord
{
    miX: i32,
    miY: i32,
    miGlyphIndex: i32,
};

@group(0) @binding(0)
var fontAtlasTexture: texture_2d<f32>;

@group(0) @binding(1)
var<storage, read> aGlyphInfo: array<OutputGlyphInfo>;

@group(0) @binding(2)
var<storage, read> aDrawTextCoordinate: array<Coord>;

@group(0) @binding(3)
var<uniform> defaultUniformBuffer: DefaultUniformData;

@group(0) @binding(4)
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
fn vs_main(
    @builtin(vertex_index) i : u32, 
    @builtin(instance_index) iInstanceIndex: u32) -> VertexOutput 
{
    var drawCoordinateInfo: Coord = aDrawTextCoordinate[iInstanceIndex];
    var glyphInfo: OutputGlyphInfo = aGlyphInfo[drawCoordinateInfo.miGlyphIndex];

    let fOneOverWidth: f32 = 1.0f / f32(defaultUniformBuffer.miScreenWidth);
    let fOneOverHeight: f32 = 1.0f / f32(defaultUniformBuffer.miScreenHeight);

    var atlasSize: vec2u = textureDimensions(fontAtlasTexture);

    let fOneOverAtlasWidth: f32 = 1.0f / f32(atlasSize.x);
    let fOneOverAtlasHeight: f32 = 1.0f / f32(atlasSize.y);

    // center position of the glyph on the screen in (0, 1)
    var centerPos: vec2f = vec2f(
        f32(drawCoordinateInfo.miX + glyphInfo.mSDFResult.width / 2) * fOneOverWidth,
        f32(drawCoordinateInfo.miY + glyphInfo.mSDFResult.height / 2) * fOneOverHeight
    );

    // default quad position and uv 
    const aPos = array(
        vec2f(-1.0f, 1.0f),
        vec2f(-1.0f, -1.0f),
        vec2f(1.0f, -1.0f),
        vec2f(1.0f, 1.0f)
    );
    const aUV = array(
        vec2f(0.0f, 0.0f),
        vec2f(0.0f, 1.0f),
        vec2f(1.0f, 1.0f),
        vec2f(1.0f, 0.0f)
    );

    // uv of the glyph within the texture atlas
    let fStartU: f32 = f32(glyphInfo.miAtlasX) * fOneOverAtlasWidth;
    let fStartV: f32 = f32(glyphInfo.miAtlasY) * fOneOverAtlasHeight; 
    let iGlyphWidth: i32 = glyphInfo.mSDFResult.width;
    let iGlyphHeight: i32 = glyphInfo.mSDFResult.height;
    let glyphAtlasScale: vec2f = vec2f(
        f32(iGlyphWidth) * fOneOverAtlasWidth,
        f32(iGlyphHeight) * fOneOverAtlasHeight
    );
    let glyphAtlasUV: vec2f = vec2f(
        fStartU + aUV[i].x * glyphAtlasScale.x,
        fStartV + aUV[i].y * glyphAtlasScale.y
    );

    // position within the output image
    let glyphOutputScale: vec2f = vec2f(
        f32(iGlyphWidth) * fOneOverWidth,
        f32(iGlyphHeight) * fOneOverHeight
    );
    let glyphOutputPosition: vec2f = vec2f(
        centerPos.x + aPos[i].x * glyphOutputScale.x,
        centerPos.y + aPos[i].y * glyphOutputScale.y
    );
    
    var output: VertexOutput;
    output.pos = vec4f(glyphOutputPosition, 0.0f, 1.0f);
    output.uv = glyphAtlasUV;        

    return output;
}

@fragment
fn fs_main(in: VertexOutput) -> FragmentOutput 
{
    var output: FragmentOutput;

    var glyphImage: vec4f = textureSample(
        fontAtlasTexture,
        textureSampler,
        in.uv.xy
    );

    output.mOutput = glyphImage;

    return output;
}