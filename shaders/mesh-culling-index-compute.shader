struct DrawIndexParam
{
    miIndexCount: u32,
    miInstanceCount: u32,
    miFirstIndex: u32,
    miBaseVertex: i32,
    miFirstInstance: u32,
};

struct MeshExtent
{
    mMinPosition: vec4<f32>,
    mMaxPosition: vec4<f32>,
};

struct Range
{
    miStart: u32,
    miEnd: u32,
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

struct UniformData
{
    miNumMeshes: u32,
    mfExplodeMultiplier: f32,
};

@group(0) @binding(0) var<storage, read_write> aDrawCalls: array<DrawIndexParam>;
@group(0) @binding(1) var<storage, read_write> aNumDrawCalls: array<atomic<u32>>;
@group(0) @binding(2) var<storage, read_write> aiVisibleMeshID: array<u32>;
//@group(0) @binding(3) depthTexture0: texture_2d<f32>;
//@group(0) @binding(4) depthTexture1: texture_2d<f32>;
//@group(0) @binding(5) depthTexture2: texture_2d<f32>;
//@group(0) @binding(6) depthTexture3: texture_2d<f32>;
//@group(0) @binding(7) depthTexture4: texture_2d<f32>;
//@group(0) @binding(8) depthTexture5: texture_2d<f32>;
//@group(0) @binding(9) depthTexture6: texture_2d<f32>;
//@group(0) @binding(10) depthTexture7: texture_2d<f32>;

@group(1) @binding(0) var<uniform> uniformBuffer: UniformData;
@group(1) @binding(1) var<storage, read> aMeshTriangleIndexRanges: array<Range>;
@group(1) @binding(2) var<storage, read> aMeshExtents: array<MeshExtent>;
@group(1) @binding(3) var<storage, read> aiVisibleFlags: array<u32>;
@group(1) @binding(4) var<uniform> defaultUniformData: DefaultUniformData;

const iNumThreads = 256u;

@compute
@workgroup_size(iNumThreads)
fn cs_main(
    @builtin(num_workgroups) numWorkGroups: vec3<u32>,
    @builtin(local_invocation_index) iLocalThreadIndex: u32,
    @builtin(workgroup_id) workGroup: vec3<u32>)
{
    let iMesh: u32 = iLocalThreadIndex + workGroup.x * iNumThreads;
    if(iMesh >= uniformBuffer.miNumMeshes)
    {
        return;
    }

    if(aiVisibleFlags[iMesh] <= 0)
    {
        aDrawCalls[iMesh].miIndexCount = 0u;
        aDrawCalls[iMesh].miInstanceCount = 0u;
        aDrawCalls[iMesh].miFirstIndex = 0u;
        aDrawCalls[iMesh].miBaseVertex = 0u;
        aDrawCalls[iMesh].miFirstInstance = 0u;

        return;
    }
    

    let center: vec3f = (aMeshExtents[iMesh].mMaxPosition.xyz + aMeshExtents[iMesh].mMinPosition.xyz) * 0.5f;

    let uv: vec2f = vec2f(0.0f, 0.0f);
    
    let minPos: vec3f = aMeshExtents[iMesh].mMinPosition.xyz + center.z * max(uniformBuffer.mfExplodeMultiplier, 1.0f);
    let maxPos: vec3f = aMeshExtents[iMesh].mMaxPosition.xyz + center.z * max(uniformBuffer.mfExplodeMultiplier, 1.0f);

    // check bbox against the depth pyramid
    let bOccluded: bool = cullBBoxDepth(
        minPos,
        maxPos,
        iMesh
    );

    var bInside: bool = cullBBox(
        minPos,
        maxPos,
        iMesh);    

    var xformCenter: vec4f = vec4f(center.xyz, 1.0f) * defaultUniformData.mViewProjectionMatrix;
    xformCenter.x /= xformCenter.w;
    xformCenter.y /= xformCenter.w;
    xformCenter.z /= xformCenter.w;

    if(xformCenter.x >= -1.0f && xformCenter.x <= 1.0f && xformCenter.y >= -1.0f && xformCenter.x <= 1.0f)
    {
        bInside = true;
    }
    else 
    {
        bInside = false;
    }

bInside = true;

    aiVisibleMeshID[iMesh] = 0u;

    let iNumIndices: u32 = aMeshTriangleIndexRanges[iMesh].miEnd - aMeshTriangleIndexRanges[iMesh].miStart;
    let iIndexAddressOffset: u32 = aMeshTriangleIndexRanges[iMesh].miStart;
    //var iDrawCommandIndex: u32 = 0u;
    if(bInside && !bOccluded)
    {
        let iDrawCommandIndex: u32 = atomicAdd(&aNumDrawCalls[0], 1u);

        aDrawCalls[iMesh].miIndexCount = iNumIndices;
        aDrawCalls[iMesh].miInstanceCount = 1u;
        aDrawCalls[iMesh].miFirstIndex = iIndexAddressOffset;
        aDrawCalls[iMesh].miBaseVertex = 0;
        aDrawCalls[iMesh].miFirstInstance = 0u;
        

        // mark as visible
        aiVisibleMeshID[iMesh] = 1u;
    }

    atomicAdd(&aNumDrawCalls[1], 1u);  
}

/////
fn getFrustumPlane(
    iColumn: u32,
    fMult: f32) -> vec4f
{
    var plane: vec3f = vec3f(
        defaultUniformData.mViewProjectionMatrix[iColumn][0] * fMult + defaultUniformData.mViewProjectionMatrix[3][0],
        defaultUniformData.mViewProjectionMatrix[iColumn][1] * fMult + defaultUniformData.mViewProjectionMatrix[3][1],
        defaultUniformData.mViewProjectionMatrix[iColumn][2] * fMult + defaultUniformData.mViewProjectionMatrix[3][2]);
    let fPlaneW: f32 = defaultUniformData.mViewProjectionMatrix[iColumn][3] * fMult + defaultUniformData.mViewProjectionMatrix[3][3];
    let fLength: f32 = length(plane);
    plane = normalize(plane);
    
    let ret: vec4f = vec4f(
        plane.xyz,
        fPlaneW / (fLength + 0.00001f)
    );
    
    return ret;
}

//////
fn cullBBox(
    minPosition: vec3f,
    maxPosition: vec3f,
    iMesh: u32) -> bool
{
    // frustum planes
    let leftPlane: vec4f = getFrustumPlane(0u, 1.0f);
    let rightPlane: vec4f = getFrustumPlane(0u, -1.0f);
    let bottomPlane: vec4f = getFrustumPlane(1u, 1.0f);
    let upperPlane: vec4f = getFrustumPlane(1u, -1.0f);
    let nearPlane: vec4f = getFrustumPlane(2u, 1.0f);
    let farPlane: vec4f = getFrustumPlane(2u, -1.0f);

    let v0: vec3f = vec3f(minPosition.x, minPosition.y, minPosition.z);
    let v1: vec3f = vec3f(maxPosition.x, minPosition.y, minPosition.z);
    let v2: vec3f = vec3f(minPosition.x, minPosition.y, maxPosition.z);
    let v3: vec3f = vec3f(maxPosition.x, minPosition.y, maxPosition.z);

    let v4: vec3f = vec3f(minPosition.x, maxPosition.y, minPosition.z);
    let v5: vec3f = vec3f(maxPosition.x, maxPosition.y, minPosition.z);
    let v6: vec3f = vec3f(minPosition.x, maxPosition.y, maxPosition.z);
    let v7: vec3f = vec3f(maxPosition.x, maxPosition.y, maxPosition.z);

    var fCount0: f32 = 0.0f;
    fCount0 += sign(dot(leftPlane.xyz, v0) + leftPlane.w);
    fCount0 += sign(dot(leftPlane.xyz, v1) + leftPlane.w);
    fCount0 += sign(dot(leftPlane.xyz, v2) + leftPlane.w);
    fCount0 += sign(dot(leftPlane.xyz, v3) + leftPlane.w);
    fCount0 += sign(dot(leftPlane.xyz, v4) + leftPlane.w);
    fCount0 += sign(dot(leftPlane.xyz, v5) + leftPlane.w);
    fCount0 += sign(dot(leftPlane.xyz, v6) + leftPlane.w);
    fCount0 += sign(dot(leftPlane.xyz, v7) + leftPlane.w);

    var fCount1: f32 = 0.0f;
    fCount1 += sign(dot(rightPlane.xyz, v0) + rightPlane.w);
    fCount1 += sign(dot(rightPlane.xyz, v1) + rightPlane.w);
    fCount1 += sign(dot(rightPlane.xyz, v2) + rightPlane.w);
    fCount1 += sign(dot(rightPlane.xyz, v3) + rightPlane.w);
    fCount1 += sign(dot(rightPlane.xyz, v4) + rightPlane.w);
    fCount1 += sign(dot(rightPlane.xyz, v5) + rightPlane.w);
    fCount1 += sign(dot(rightPlane.xyz, v6) + rightPlane.w);
    fCount1 += sign(dot(rightPlane.xyz, v7) + rightPlane.w);

    var fCount2: f32 = 0.0f;
    fCount2 += sign(dot(upperPlane.xyz, v0) + upperPlane.w);
    fCount2 += sign(dot(upperPlane.xyz, v1) + upperPlane.w);
    fCount2 += sign(dot(upperPlane.xyz, v2) + upperPlane.w);
    fCount2 += sign(dot(upperPlane.xyz, v3) + upperPlane.w);
    fCount2 += sign(dot(upperPlane.xyz, v4) + upperPlane.w);
    fCount2 += sign(dot(upperPlane.xyz, v5) + upperPlane.w);
    fCount2 += sign(dot(upperPlane.xyz, v6) + upperPlane.w);
    fCount2 += sign(dot(upperPlane.xyz, v7) + upperPlane.w);

    var fCount3: f32 = 0.0f;
    fCount3 += sign(dot(bottomPlane.xyz, v0) + bottomPlane.w);
    fCount3 += sign(dot(bottomPlane.xyz, v1) + bottomPlane.w);
    fCount3 += sign(dot(bottomPlane.xyz, v2) + bottomPlane.w);
    fCount3 += sign(dot(bottomPlane.xyz, v3) + bottomPlane.w);
    fCount3 += sign(dot(bottomPlane.xyz, v4) + bottomPlane.w);
    fCount3 += sign(dot(bottomPlane.xyz, v5) + bottomPlane.w);
    fCount3 += sign(dot(bottomPlane.xyz, v6) + bottomPlane.w);
    fCount3 += sign(dot(bottomPlane.xyz, v7) + bottomPlane.w);

    var fCount4: f32 = 0.0f;
    fCount4 += sign(dot(nearPlane.xyz, v0) + nearPlane.w);
    fCount4 += sign(dot(nearPlane.xyz, v1) + nearPlane.w);
    fCount4 += sign(dot(nearPlane.xyz, v2) + nearPlane.w);
    fCount4 += sign(dot(nearPlane.xyz, v3) + nearPlane.w);
    fCount4 += sign(dot(nearPlane.xyz, v4) + nearPlane.w);
    fCount4 += sign(dot(nearPlane.xyz, v5) + nearPlane.w);
    fCount4 += sign(dot(nearPlane.xyz, v6) + nearPlane.w);
    fCount4 += sign(dot(nearPlane.xyz, v7) + nearPlane.w);

    return (fCount0 > -8.0f && fCount1 > -8.0f && fCount2 > -8.0f && fCount3 > -8.0f && fCount4 > -8.0f);  
}


//////
fn cullBBoxDepth(
    minPosition: vec3f,
    maxPosition: vec3f,
    iMesh: u32) -> bool
{
    return false;

    // NOTE: z pyramid textures didn't all in account of all pixels, just the 4 adjacent ones when down scaling
    // scale to pow(2) size to fix this?

    //let v0: vec3f = vec3f(minPosition.x, minPosition.y, minPosition.z);
    //let v1: vec3f = vec3f(maxPosition.x, minPosition.y, minPosition.z);
    //let v2: vec3f = vec3f(minPosition.x, minPosition.y, maxPosition.z);
    //let v3: vec3f = vec3f(maxPosition.x, minPosition.y, maxPosition.z);
//
    //let v4: vec3f = vec3f(minPosition.x, maxPosition.y, minPosition.z);
    //let v5: vec3f = vec3f(maxPosition.x, maxPosition.y, minPosition.z);
    //let v6: vec3f = vec3f(minPosition.x, maxPosition.y, maxPosition.z);
    //let v7: vec3f = vec3f(maxPosition.x, maxPosition.y, maxPosition.z);
//
    //let xform0: vec4f = vec4f(v0.xyz, 1.0f) * defaultUniformData.mJitteredViewProjectionMatrix;
    //let xform1: vec4f = vec4f(v1.xyz, 1.0f) * defaultUniformData.mJitteredViewProjectionMatrix;
    //let xform2: vec4f = vec4f(v2.xyz, 1.0f) * defaultUniformData.mJitteredViewProjectionMatrix;
    //let xform3: vec4f = vec4f(v3.xyz, 1.0f) * defaultUniformData.mJitteredViewProjectionMatrix;
//
    //let xform4: vec4f = vec4f(v4.xyz, 1.0f) * defaultUniformData.mJitteredViewProjectionMatrix;
    //let xform5: vec4f = vec4f(v5.xyz, 1.0f) * defaultUniformData.mJitteredViewProjectionMatrix;
    //let xform6: vec4f = vec4f(v6.xyz, 1.0f) * defaultUniformData.mJitteredViewProjectionMatrix;
    //let xform7: vec4f = vec4f(v7.xyz, 1.0f) * defaultUniformData.mJitteredViewProjectionMatrix;
//
    //let xyz0:vec3f  = xform0.xyz / xform0.w;
    //let xyz1:vec3f  = xform1.xyz / xform1.w;
    //let xyz2:vec3f  = xform2.xyz / xform2.w;
    //let xyz3:vec3f  = xform3.xyz / xform3.w;
//
    //let xyz4:vec3f  = xform4.xyz / xform4.w;
    //let xyz5:vec3f  = xform5.xyz / xform5.w;
    //let xyz6:vec3f  = xform6.xyz / xform6.w;
    //let xyz7:vec3f  = xform7.xyz / xform7.w;
//
    //var minXYZ: vec3f = min(min(min(min(min(min(min(xyz0, xyz1), xyz2), xyz3), xyz4), xyz5), xyz6), xyz7);
    //var maxXYZ: vec3f = max(max(max(max(max(max(max(xyz0, xyz1), xyz2), xyz3), xyz4), xyz5), xyz6), xyz7);
    //minXYZ = vec3f(minXYZ.x * 0.5f + 0.5f, minXYZ.y * 0.5f + 0.5f, minXYZ.z);
    //maxXYZ = vec3f(maxXYZ.x * 0.5f + 0.5f, maxXYZ.y * 0.5f + 0.5f, maxXYZ.z);
//
    //minXYZ.x = clamp(minXYZ.x, 0.0f, 1.0f);
    //minXYZ.y = clamp(minXYZ.y, 0.0f, 1.0f);
    //maxXYZ.x = clamp(maxXYZ.x, 0.0f, 1.0f);
    //maxXYZ.y = clamp(maxXYZ.y, 0.0f, 1.0f);
//
    //if (maxXYZ.z > 1.0f)
    //{
    //    minXYZ.z = 0.0f;
    //}
//
    //let fMinDepth: f32 = minXYZ.z;
    //let fAspectRatio: f32 = f32(defaultUniformData.miScreenHeight) / f32(defaultUniformData.miScreenWidth);
    //let diffXY: vec2f = abs(maxXYZ.xy - minXYZ.xy);
    //let fMaxComp: f32 = max(diffXY.x, diffXY.y * fAspectRatio) * 512.0f; // compute LOD from 1 to 8
    //let iLOD: u32 = min(u32(log2(fMaxComp)), 8u);
    //var uv: vec2f = vec2f(maxXYZ.xy + minXYZ.xy) * 0.5f;
    //uv.y = 1.0f - uv.y;
    //uv = clamp(uv, vec2f(0.0f, 0.0f), vec2f(1.0f, 1.0f));
    //var fSampleDepth: f32 = 0.0f;
//
    //if (iLOD == 0u)
    //{
    //    var textureSize: uint2;
    //    depthTexture0.GetDimensions(textureSize.x, textureSize.y);
    //    let screenCoord: uint2 = uint2(u32(uv.x * f32(textureSize.x)), u32(uv.y * f32(textureSize.y)));
    //    fSampleDepth = depthTexture0[screenCoord].x;
    //}
    //else if (iLOD == 1u)
    //{
    //    var textureSize: uint2;
    //    depthTexture1.GetDimensions(textureSize.x, textureSize.y);
    //    let screenCoord: uint2 = uint2(u32(uv.x * f32(textureSize.x)), u32(uv.y * f32(textureSize.y)));
    //    fSampleDepth = depthTexture1[screenCoord].x;
    //}
    //else if (iLOD == 2u)
    //{
    //    var textureSize: uint2;
    //    depthTexture2.GetDimensions(textureSize.x, textureSize.y);
    //    let screenCoord: uint2 = uint2(u32(uv.x * f32(textureSize.x)), u32(uv.y * f32(textureSize.y)));
    //    fSampleDepth = depthTexture2[screenCoord].x;
    //}
    //else if (iLOD == 3u)
    //{
    //    var textureSize: uint2;
    //    depthTexture3.GetDimensions(textureSize.x, textureSize.y);
    //    let screenCoord: uint2 = uint2(u32(uv.x * f32(textureSize.x)), u32(uv.y * f32(textureSize.y)));
    //    fSampleDepth = depthTexture3[screenCoord].x;
    //}
    //else if (iLOD == 4u)
    //{
    //    var textureSize: uint2;
    //    depthTexture4.GetDimensions(textureSize.x, textureSize.y);
    //    let screenCoord: uint2 = uint2(u32(uv.x * f32(textureSize.x)), u32(uv.y * f32(textureSize.y)));
    //    fSampleDepth = depthTexture4[screenCoord].x;
    //}
    //else if (iLOD == 5u)
    //{
    //    var textureSize: uint2;
    //    depthTexture5.GetDimensions(textureSize.x, textureSize.y);
    //    let screenCoord: uint2 = uint2(u32(uv.x * f32(textureSize.x)), u32(uv.y * f32(textureSize.y)));
    //    fSampleDepth = depthTexture5[screenCoord].x;
    //}
    //else if (iLOD == 6u)
    //{
    //    var textureSize: uint2;
    //    depthTexture6.GetDimensions(textureSize.x, textureSize.y);
    //    let screenCoord: uint2 = uint2(u32(uv.x * f32(textureSize.x)), u32(uv.y * f32(textureSize.y)));
    //    fSampleDepth = depthTexture6[screenCoord].x;
    //}
    //else
    //{
    //    var textureSize: uint2;
    //    depthTexture7.GetDimensions(textureSize.x, textureSize.y);
    //    let screenCoord: uint2 = uint2(u32(uv.x * f32(textureSize.x)), u32(uv.y * f32(textureSize.y)));
    //    fSampleDepth = depthTexture7[screenCoord].x;
    //}

    //return (fMinDepth > fSampleDepth);
}