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

struct OutputGlyphInfo
{
    width: i32,
    height: i32,
    yOffset: i32,
    miAtlasX: i32,
    miAtlasY: i32,
    miASCII: i32,
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
    @location(1) mfGlyph: f32,
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
    let kGlyphSize: f32 = 64.0f;

    var drawCoordinateInfo: Coord = aDrawTextCoordinate[iInstanceIndex];
    var glyphInfo: OutputGlyphInfo = aGlyphInfo[drawCoordinateInfo.miGlyphIndex];

    let fOneOverWidth: f32 = 1.0f / f32(defaultUniformBuffer.miScreenWidth);
    let fOneOverHeight: f32 = 1.0f / f32(defaultUniformBuffer.miScreenHeight);

    var atlasSize: vec2u = textureDimensions(fontAtlasTexture);

    let fOneOverAtlasWidth: f32 = 1.0f / f32(atlasSize.x);
    let fOneOverAtlasHeight: f32 = 1.0f / f32(atlasSize.y);

    let iHalfWidth: i32 = defaultUniformBuffer.miScreenWidth / 2;
    let iHalfHeight: i32 = defaultUniformBuffer.miScreenHeight / 2;

    // center position of the glyph on the screen in (0, 1)
    //var centerPos: vec2f = vec2f(
    //    f32(drawCoordinateInfo.miX) * fOneOverWidth,
    //    f32(drawCoordinateInfo.miY) * fOneOverHeight
    //);
    var centerPos: vec2f = vec2f(
        f32(drawCoordinateInfo.miX - iHalfWidth) / f32(iHalfWidth),
        (f32(drawCoordinateInfo.miY - iHalfHeight) / f32(iHalfHeight)) * -1.0f
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

    let glyphScale: vec2f = vec2f(
        f32(glyphInfo.width) / kGlyphSize,
        f32(glyphInfo.height) / kGlyphSize
    );

    // uv of the glyph within the texture atlas
    let fStartU: f32 = f32(glyphInfo.miAtlasX) * fOneOverAtlasWidth;
    let fStartV: f32 = f32(glyphInfo.miAtlasY) * fOneOverAtlasHeight; 
    let iGlyphWidth: i32 = glyphInfo.width;
    let iGlyphHeight: i32 = glyphInfo.height;
    let glyphAtlasScale: vec2f = vec2f(
        f32(iGlyphWidth) * fOneOverAtlasWidth,
        f32(iGlyphHeight) * fOneOverAtlasHeight
    );
    let glyphAtlasUV: vec2f = vec2f(
        fStartU + aUV[i].x * glyphAtlasScale.x,
        fStartV + aUV[i].y * glyphAtlasScale.y
    );

    // position within the output image
    // offset for the glyph corner positions
    const aPosMult = array(
        vec2i(-1, -1),
        vec2i(-1, 1),
        vec2i(1, 1),
        vec2i(1, -1)
    );
    let iX: i32 = drawCoordinateInfo.miX + (glyphInfo.width / 2) * aPosMult[i].x;
    let iY: i32 = drawCoordinateInfo.miY + (glyphInfo.height / 2) * aPosMult[i].y;
    let iDiffY: i32 = i32(kGlyphSize) - iGlyphHeight;
    var glyphOutputPosition: vec2f = vec2f(
        f32(iX) / f32(defaultUniformBuffer.miScreenWidth),
        f32(iY + iDiffY + glyphInfo.yOffset * -1) / f32(defaultUniformBuffer.miScreenHeight)
    );

    // convert (0, 1) to (-1, 1), making sure to invert y 
    glyphOutputPosition.x = glyphOutputPosition.x * 2.0f - 1.0f;
    glyphOutputPosition.y = (glyphOutputPosition.y * 2.0f - 1.0f) * -1.0f;

    var output: VertexOutput;
    output.pos = vec4f(glyphOutputPosition, 0.0f, 1.0f);
    output.uv = vec2(glyphAtlasUV.x, glyphAtlasUV.y);        

    output.mfGlyph = f32(drawCoordinateInfo.miGlyphIndex);

    return output;
}

@fragment
fn fs_main(in: VertexOutput) -> FragmentOutput 
{
    var output: FragmentOutput;

    var sample: vec3f = textureSample(
        fontAtlasTexture,
        textureSampler,
        in.uv.xy
    ).xyz;

    let iGlyph: i32 = i32(ceil(in.mfGlyph - 0.5f));
    var glyphInfo: OutputGlyphInfo = aGlyphInfo[iGlyph];

    let sz: vec2f = vec2f(f32(glyphInfo.width), f32(glyphInfo.height));
    let dx: f32 = dpdx(in.uv.x) * sz.x; 
    let dy: f32 = dpdy(in.uv.y) * sz.y;
    let toPixels: f32 = 8.0f * inverseSqrt(dx * dx + dy * dy);
    let sigDist: f32 = max(min(sample.r, sample.g), min(max(sample.r, sample.g), sample.b));
    let w: f32 = fwidth(sigDist);
    let fOpacity: f32 = smoothstep(0.5 - w, 0.5 + w, sigDist);
    
    output.mOutput = vec4f(mix(vec3f(0.0f, 0.0f, 0.0f), vec3f(1.0f, 1.0f, 1.0f), fOpacity), fOpacity);
    return output;
}