

#include <cstdint>
#include <string>
#include <vector>
#include <cassert>
#include <sstream>
#include <mutex>
#include <map>

#include <filesystem>

#include <math/vec.h>
#include <utils/LogPrint.h>

#define STB_IMAGE_IMPLEMENTATION
#include <stb_image/stb_image.h>

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include <stb_image/stb_image_write.h>

#define TINYOBJLOADER_IMPLEMENTATION
#include <tiny_obj_loader/tiny_obj_loader.h>

#define POSITION_MULT 10.0

#if defined(__APPLE__)
#define FLT_MAX __FLT_MAX__
#endif // __APPLE__ 

std::mutex gMutex;

struct Face
{
    uint32_t                                    miIndex = UINT32_MAX;

    std::vector<uint32_t>                       maiPositions;
    std::vector<uint32_t>                       maiUVs;
    std::vector<uint32_t>                       maiNormals;

    bool operator == (Face const& face)
    {
        return miIndex == face.miIndex;
    }
};

struct OBJMaterialInfo
{
    uint32_t            miID = 0;
    float4              mDiffuse = float4(1.0f, 1.0f, 1.0f, 1.0f);
    float4              mSpecular = float4(0.0f, 0.0f, 0.0f, 0.0f);
    float4              mEmissive = float4(0.0f, 0.0f, 0.0f, 0.0f);;
    std::string         mName = "";
    std::string         mAlbedoTexturePath = "";
    std::string         mNormalTexturePath = "";
    std::string         mSpecularTexturePath = "";
    std::string         mEmissiveTexturePath = "";
};

struct OutputMaterialInfo
{
    float4              mDiffuse;
    float4              mSpecular;
    float4              mEmissive;
    uint32_t            miID;
    uint32_t            miAlbedoTextureID;
    uint32_t            miNormalTextureID;
    uint32_t            miSpecularTextureID;
};

struct MeshRange
{
    uint32_t            miStart;
    uint32_t            miEnd;
};

struct Vertex
{
    vec4        mPosition;
    vec4        mUV;
    vec4        mNormal;
};


struct MeshExtent
{
    vec4            mMinPosition;
    vec4            mMaxPosition;
};


void outputVerticesAndTriangles(
    std::vector<Vertex> const& aTotalVertices,
    std::vector<std::vector<uint32_t>> const& aaiTriangleVertexIndices,
    std::vector<MeshExtent> const& aMeshExtents,
    std::string const& directory,
    std::string const& baseName);

void outputTrianglePositionsAndTriangles(
    std::vector<float4> const& aTrianglePositions,
    std::vector<std::vector<uint32_t>> const& aaiTriangleVertexIndices,
    std::vector<MeshExtent> const& aMeshExtents,
    std::string const& directory,
    std::string const& baseName);

void test(
    std::vector<Vertex>& aTotalVertices,
    std::vector<std::vector<uint32_t>>& aaiTriangleIndices,
    std::vector<MeshRange>& aMeshRanges,
    std::vector<MeshExtent>& aMeshExtents,
    std::string const& fullPath);

void saveOBJ(
    std::string const& fullPath,
    std::vector<Vertex> const& aTotalVertices,
    std::vector<std::vector<uint32_t>> const& aaiTriangleVertexIndices,
    std::vector<uint32_t> const& aiMeshes);

bool readMaterialFile(
    std::vector<OBJMaterialInfo>& aMaterials,
    std::string const& fullPath,
    std::string const& directory,
    std::string const& baseName);

void outputMeshMaterialIDs(
    std::vector<uint32_t> const& aiMeshMaterialIDs,
    std::string const& directory,
    std::string const& baseName);

void convertNormalImages(std::string const& directory);


int main(int argc, char* argv[])
{
    std::string fullPath = argv[1];
    auto iter = fullPath.rfind("\\");
    if(iter == std::string::npos)
    {
        iter = fullPath.rfind("/");
    }
    std::string directory = fullPath.substr(0, iter);
    std::string fileName = fullPath.substr(iter + 1);
    auto extensionIter = fileName.rfind(".obj");
    
    std::string baseName = ""; 
    if(extensionIter == std::string::npos)
    {
        if(iter == fullPath.size() - 1)
        {
            iter = fullPath.rfind("\\", iter - 1);
        }
        
        baseName = fileName.substr(0, iter);
        directory = fullPath;
    }
    else
    {
        baseName = fileName.substr(0, extensionIter);
    }
    
    auto encodeToMapString = [&](Vertex const& v, uint32_t iMeshIndex)
        {
            Vertex copy = v;
            copy.mPosition.x *= 100.0f;
            copy.mPosition.y *= 100.0f;
            copy.mPosition.z *= 100.0f;

            std::ostringstream oss;
            oss << copy.mPosition.x << "_" << copy.mPosition.y << "_" << copy.mPosition.z << "_" <<
                copy.mNormal.x << "_" << copy.mNormal.y << "_" << copy.mNormal.z << "_" <<
                copy.mUV.x << "_" << copy.mUV.y << "_" << iMeshIndex;

            return oss.str();
        };

    std::map<std::string, std::vector<uint32_t>> aMeshInstanceIndices;

    std::vector<Vertex> aTotalVertices;
    std::map<std::string, uint32_t> aTotalVertexMap;
    std::vector<std::vector<uint32_t>> aaiTriangleVertexIndices;
    std::vector<MeshExtent> aMeshExtents;
    std::vector<float3> aMeshCenters;
    std::vector<float3> aMeshBBoxes;
    std::vector<std::string> aMeshNames;
    std::vector<uint32_t> aiMeshMaterialIDs;
    std::vector<OBJMaterialInfo> aMeshMaterials;

    float3 totalMinPos = float3(FLT_MAX, FLT_MAX, FLT_MAX);
    float3 totalMaxPos = float3(-FLT_MAX, -FLT_MAX, -FLT_MAX);

    for(auto const& entry : std::filesystem::directory_iterator(directory))
    {
        std::string path = entry.path().string().c_str();

        std::string fileName = path.substr(iter + 1);
        auto extensionIter = fileName.rfind(".obj");
        if(extensionIter == std::string::npos)
        {
            continue;
        }

        std::string baseName = fileName.substr(0, extensionIter);

        tinyobj::attrib_t attrib;
        std::vector<tinyobj::shape_t> shapes;
        std::vector<tinyobj::material_t> materials;
        std::string warn, err;

        bool ret = tinyobj::LoadObj(
            &attrib, 
            &shapes, 
            &materials, 
            &warn, 
            &err, 
            path.c_str(),
            directory.c_str());

        // Access loaded data
        for(size_t s = 0; s < shapes.size(); s++)
        {
            std::ostringstream partNameStringStream;
            partNameStringStream << baseName << "-" << "shape" << s;
            aMeshNames.push_back(partNameStringStream.str());

            OBJMaterialInfo material = {};
            if(materials.size() > 0)
            {
                int32_t iMaterialID = shapes[s].mesh.material_ids[0];

                material.mDiffuse = float4(
                    (float)materials[iMaterialID].diffuse[0], 
                    (float)materials[iMaterialID].diffuse[0], 
                    (float)materials[iMaterialID].diffuse[0], 1.0f);

                material.mSpecular = float4(
                    (float)materials[iMaterialID].specular[0],
                    (float)materials[iMaterialID].specular[0],
                    (float)materials[iMaterialID].specular[0], 1.0f);

                material.mEmissive = float4(
                    (float)materials[iMaterialID].emission[0],
                    (float)materials[iMaterialID].emission[0],
                    (float)materials[iMaterialID].emission[0], 1.0f);

                material.mAlbedoTexturePath = materials[iMaterialID].diffuse_texname;
                material.mEmissiveTexturePath = materials[iMaterialID].emissive_texname;
                material.mSpecularTexturePath = materials[iMaterialID].specular_texname;
                material.mNormalTexturePath = materials[iMaterialID].normal_texname;
            }
            else 
            {
                float fRed = float(rand() % 255) / 255.0f;
                float fGreen = float(rand() % 255) / 255.0f;
                float fBlue = float(rand() % 255) / 255.0f;
                material.mDiffuse = float4(fRed, fGreen, fBlue, 1.0f);
            }

            aMeshMaterials.push_back(material);
            aiMeshMaterialIDs.push_back((uint32_t)aMeshMaterials.size() - 1);

            std::vector<Vertex> aVertices;
            std::vector<uint32_t> aiVertexIndices;

            float3 maxPosition = float3(-FLT_MAX, -FLT_MAX, -FLT_MAX);
            float3 minPosition = float3(FLT_MAX, FLT_MAX, FLT_MAX);

            // Loop over faces(polygon)
            size_t index_offset = 0;
            for(size_t f = 0; f < shapes[s].mesh.num_face_vertices.size(); f++)
            {
                int fv = shapes[s].mesh.num_face_vertices[f];

                // Loop over vertices in the face.
                for(size_t v = 0; v < fv; v++)
                {
                    // access to vertex
                    tinyobj::index_t idx = shapes[s].mesh.indices[index_offset + v];

                    float vx = float(attrib.vertices[3 * idx.vertex_index + 0] * POSITION_MULT);
                    float vy = float(attrib.vertices[3 * idx.vertex_index + 1] * POSITION_MULT);
                    float vz = float(attrib.vertices[3 * idx.vertex_index + 2] * POSITION_MULT);

                    float nx = 0.0f;
                    float ny = 0.0f;
                    float nz = 0.0f;

                    float tx = 0.0f;
                    float ty = 0.0f;

                    // Check if normals and texcoords are loaded
                    if(idx.normal_index >= 0)
                    {
                        nx = attrib.normals[3 * idx.normal_index + 0];
                        ny = attrib.normals[3 * idx.normal_index + 1];
                        nz = attrib.normals[3 * idx.normal_index + 2];
                        // Use normal data
                    }
                    if(idx.texcoord_index >= 0)
                    {
                        tx = attrib.texcoords[2 * idx.texcoord_index + 0];
                        ty = attrib.texcoords[2 * idx.texcoord_index + 1];
                        // Use texture coordinate data
                    }

                    // Process vertex data (e.g., store in your data structures
                    Vertex vertex;
                    vertex.mPosition = float4(vx, vy, vz, (float)aMeshExtents.size());
                    vertex.mNormal = float4(nx, ny, nz, 1.0f);
                    vertex.mUV = float4(tx, ty, (float)aMeshExtents.size(), 1.0f);

                    totalMinPos = fminf(totalMinPos, float3(vx, vy, vz));
                    totalMaxPos = fmaxf(totalMaxPos, float3(vx, vy, vz));

                    std::string mapString = encodeToMapString(vertex, (uint32_t)s);
                    auto iter = aTotalVertexMap.find(mapString);
                    if(iter == aTotalVertexMap.end())
                    {
                        aVertices.push_back(vertex);
                        uint32_t iVertexIndex = (uint32_t)aTotalVertices.size();
                        aTotalVertexMap[mapString] = iVertexIndex;
                        aiVertexIndices.push_back(iVertexIndex);

                        aTotalVertices.push_back(vertex);
                    }
                    else
                    {
                        uint32_t iVertexIndex = aTotalVertexMap[mapString];

#if defined(_DEBUG)
                        Vertex const& checkV = aTotalVertices[iVertexIndex];
                        float3 diffPos = checkV.mPosition - vertex.mPosition;
                        float3 diffNorm = checkV.mNormal - vertex.mNormal;
                        float fDP0 = dot(diffPos, diffPos);
                        float fDP1 = dot(diffNorm, diffNorm);
                        assert(fDP0 <= 0.0001f && fDP1 <= 0.0001f);
#endif // #if 0

                        aiVertexIndices.push_back(iVertexIndex);
                    }
                    
                    minPosition = fminf(float3(vx, vy, vz), minPosition);
                    maxPosition = fmaxf(float3(vx, vy, vz), maxPosition);

                }   // for vertex

                index_offset += fv;

            }   // for face

            assert(aiVertexIndices.size() % 3 == 0);
            aaiTriangleVertexIndices.push_back(aiVertexIndices);

            MeshExtent meshExtent;
            meshExtent.mMinPosition = float4(minPosition.x, minPosition.y, minPosition.z, 1.0f);
            meshExtent.mMaxPosition = float4(maxPosition.x, maxPosition.y, maxPosition.z, 1.0f);

            aMeshExtents.push_back(meshExtent);

            float3 center = (maxPosition + minPosition) * 0.5f;
            aMeshCenters.push_back(center);

            float3 bbox = maxPosition - minPosition;
            aMeshBBoxes.push_back(bbox);

            {
                std::stringstream meshInstanceStringStream;
                meshInstanceStringStream << bbox.x << "_" << bbox.y << "_" << bbox.z << "_" << aiVertexIndices.size();
                std::string mapEntryName = meshInstanceStringStream.str();
                aMeshInstanceIndices[mapEntryName].push_back((uint32_t)aMeshBBoxes.size() - 1);
                
            }

//if(s <= 10)
//{
//    break;
//}

        }   // for shape = 0 to num shapes
    
        DEBUG_PRINTF("added \"%s\" num meshes %d total num meshes: %d\n", 
            baseName.c_str(), 
            shapes.size(),
            aMeshBBoxes.size());

//break;

    }   // tiny obj

    // total mesh extent
    MeshExtent meshExtent;
    meshExtent.mMinPosition = float4(totalMinPos, 1.0f);
    meshExtent.mMaxPosition = float4(totalMaxPos, 1.0f);
    aMeshExtents.push_back(meshExtent);

    DEBUG_PRINTF("num total meshes: %d\n", aMeshBBoxes.size());

    std::map<uint32_t, std::vector<uint32_t>> aMeshInstances;
    for(auto const& keyValue : aMeshInstanceIndices)
    {
        uint32_t iMeshIndex = keyValue.second[0];
        for(uint32_t i = 1; i < (uint32_t)keyValue.second.size(); i++)
        {
            aMeshInstances[iMeshIndex].push_back(keyValue.second[i]);
        }
    }

    for(auto const& keyValue : aMeshInstances)
    {
        DEBUG_PRINTF("mesh %d\t ", keyValue.second[0]);
        for(uint32_t i = 1; i < keyValue.second.size(); i++)
        {
            DEBUG_PRINTF("%d ", keyValue.second[i]);
        }
        DEBUG_PRINTF("\n");
    }

    // output materials
    {
        std::vector<OutputMaterialInfo> aOutputMaterialInfos(aMeshMaterials.size());
        for(uint32_t i = 0; i < (uint32_t)aOutputMaterialInfos.size(); i++)
        {
            aOutputMaterialInfos[i].mDiffuse = aMeshMaterials[i].mDiffuse;
            aOutputMaterialInfos[i].mEmissive = aMeshMaterials[i].mEmissive;
            aOutputMaterialInfos[i].mSpecular = aMeshMaterials[i].mSpecular;

            // TODO: get texture id from saved texture names
            aOutputMaterialInfos[i].miAlbedoTextureID = 0;
            aOutputMaterialInfos[i].miNormalTextureID = 0;
            aOutputMaterialInfos[i].miSpecularTextureID = 0;
        }

        std::string outputMaterialFilePath = directory + "/" + baseName + ".mat";
        FILE* fp = fopen(outputMaterialFilePath.c_str(), "wb");
        uint32_t iNumMaterials = (uint32_t)aOutputMaterialInfos.size();
        //fwrite(&iNumMaterials, sizeof(uint32_t), 1, fp);
        fwrite(aOutputMaterialInfos.data(), sizeof(OutputMaterialInfo), iNumMaterials, fp);
        fclose(fp);
    }

    std::string outputPath = directory + "/" + baseName + "-mesh-instance-ids.bin";
    FILE* fp = fopen(outputPath.c_str(), "wb");
    uint32_t iNumValidMeshes = (uint32_t)aMeshInstances.size();
    fwrite(&iNumValidMeshes, sizeof(uint32_t), 1, fp);
    for(auto const& keyValue: aMeshInstances)
    {
        uint32_t iNumMeshInstances = (uint32_t)keyValue.second.size();
        fwrite(&keyValue.first, sizeof(uint32_t), 1, fp);
        fwrite(&iNumMeshInstances, sizeof(uint32_t), 1, fp);
        fwrite(keyValue.second.data(), sizeof(uint32_t), keyValue.second.size(), fp);
    }
    fclose(fp);

    uint32_t iNumMeshes = (uint32_t)aMeshExtents.size();
    outputPath = directory + "/" + baseName + "-mesh-instance-positions.bin";
    fp = fopen(outputPath.c_str(), "wb");
    fwrite(&iNumMeshes, sizeof(uint32_t), 1, fp);
    fwrite(aMeshCenters.data(), sizeof(float3), aMeshCenters.size(), fp);
    fclose(fp);

    outputPath = directory + "/" + baseName + "-mesh-instance-bboxes.bin";
    fp = fopen(outputPath.c_str(), "wb");
    fwrite(&iNumMeshes, sizeof(uint32_t), 1, fp);
    fwrite(aMeshBBoxes.data(), sizeof(float3), aMeshBBoxes.size(), fp);
    fclose(fp);

    outputVerticesAndTriangles(
        aTotalVertices,
        aaiTriangleVertexIndices,
        aMeshExtents,
        directory,
        baseName);

    std::vector<float4> aTotalTrianglePositions(aTotalVertices.size());
    for(uint32_t i = 0; i < (uint32_t)aTotalVertices.size(); i++)
    {
        aTotalTrianglePositions[i] = float4(aTotalVertices[i].mPosition, 1.0f);
    }
    outputTrianglePositionsAndTriangles(
        aTotalTrianglePositions,
        aaiTriangleVertexIndices,
        aMeshExtents,
        directory,
        baseName
    );

    outputMeshMaterialIDs(
        aiMeshMaterialIDs,
        directory,
        baseName);

    std::string loadFullPath = directory + "/" + baseName + "-triangles.bin";
    std::vector<Vertex> aTestTotalVertices;
    std::vector<std::vector<uint32_t>> aaiTriangleIndices;
    std::vector<MeshRange> aMeshRanges;
    std::vector<MeshExtent> aTestMeshExtents;
    test(
        aTestTotalVertices,
        aaiTriangleIndices,
        aMeshRanges,
        aTestMeshExtents,
        loadFullPath);
}

/*
**
*/
void outputMeshMaterialIDs(
    std::vector<uint32_t> const& aiMeshMaterialIDs,
    std::string const& directory,
    std::string const& baseName)
{
    std::string fullPath = directory + "/" + baseName + ".mid";
    FILE* fp = fopen(fullPath.c_str(), "wb");
    fwrite(aiMeshMaterialIDs.data(), sizeof(uint32_t), aiMeshMaterialIDs.size(), fp);
    fclose(fp);
}

/*
**
*/
std::vector<std::string> split(const char* str, char c = ' ')
{
    std::vector<std::string> result;
    do
    {
        const char* begin = str;

        while(*str != c && *str)
            str++;

        result.push_back(std::string(begin, str));
    } while(0 != *str++);

    return result;
}

/*
**
*/
void getPositionRange(
    std::vector<uint32_t>& ret,
    std::vector<std::vector<vec3>> const& aaPositions)
{
    uint32_t iPositionLength = (uint32_t)aaPositions.size();
    ret.resize(2);
    ret[0] = ret[1] = 0;

    uint32_t iNumPositions = 0;
    for(uint32_t i = 0; i < iPositionLength; i++)
    {
        iNumPositions += (uint32_t)aaPositions[i].size();
        if(i == iPositionLength - 2)
        {
            ret[0] = iNumPositions;
        }
    }

    ret[1] = iNumPositions;
}

/*
**
*/
void outputVerticesAndTriangles(
    std::vector<Vertex> const& aTotalVertices,
    std::vector<std::vector<uint32_t>> const& aaiTriangleVertexIndices,
    std::vector<MeshExtent> const& aMeshExtents,
    std::string const& directory,
    std::string const& baseName)
{
    std::string fullPath = directory + "/" + baseName + "-triangles.bin";

    uint32_t iNumMeshes = (uint32_t)aaiTriangleVertexIndices.size();
    std::vector<MeshRange> aMeshTriangleRanges(iNumMeshes);
    uint32_t iCurrStart = 0;
    for(uint32_t i = 0; i < iNumMeshes; i++)
    {
        MeshRange range;
        range.miStart = iCurrStart;
        iCurrStart += (uint32_t)aaiTriangleVertexIndices[i].size();
        range.miEnd = iCurrStart;
        aMeshTriangleRanges[i] = range;
    }
    uint32_t iTriangleRangeSize = (uint32_t)aMeshTriangleRanges.size() * sizeof(MeshRange);

    uint32_t iNumTotalVertices = (uint32_t)aTotalVertices.size();
    uint32_t iVertexSize = (uint32_t)sizeof(Vertex);

    uint32_t iNumTotalTriangles = 0;
    for(auto const& aiTriangleVertexIndices : aaiTriangleVertexIndices)
    {
        assert(aiTriangleVertexIndices.size() % 3 == 0);
        uint32_t iNumTriangles = (uint32_t)aiTriangleVertexIndices.size() / 3;
        iNumTotalTriangles += iNumTriangles;
    }

    uint32_t iTriangleStartOffset = iNumTotalVertices * iVertexSize + iTriangleRangeSize + sizeof(uint32_t) * 5 + iNumMeshes * sizeof(MeshExtent);

    FILE* fp = fopen(fullPath.c_str(), "wb");
    fwrite(&iNumMeshes, sizeof(uint32_t), 1, fp);
    fwrite(&iNumTotalVertices, sizeof(uint32_t), 1, fp);
    fwrite(&iNumTotalTriangles, sizeof(uint32_t), 1, fp);
    fwrite(&iVertexSize, sizeof(uint32_t), 1, fp);
    fwrite(&iTriangleStartOffset, sizeof(uint32_t), 1, fp);
    fwrite(aMeshTriangleRanges.data(), sizeof(MeshRange), aMeshTriangleRanges.size(), fp);

    // mesh extents
    assert(aMeshExtents.size() == iNumMeshes + 1);
    fwrite(aMeshExtents.data(), sizeof(MeshExtent), iNumMeshes + 1, fp);

    // vertices and triangle indices
    assert(aaiTriangleVertexIndices.size() == iNumMeshes);
    fwrite(aTotalVertices.data(), sizeof(Vertex), aTotalVertices.size(), fp);

    for(uint32_t i = 0; i < aaiTriangleVertexIndices.size(); i++)
    {
        fwrite(aaiTriangleVertexIndices[i].data(), sizeof(uint32_t), aaiTriangleVertexIndices[i].size(), fp);
    }

    fclose(fp);

    DEBUG_PRINTF("wrote to %s num meshes: %d\n", fullPath.c_str(), (int32_t)aaiTriangleVertexIndices.size());
}

/*
**
*/
void outputTrianglePositionsAndTriangles(
    std::vector<float4> const& aTrianglePositions,
    std::vector<std::vector<uint32_t>> const& aaiTriangleVertexIndices,
    std::vector<MeshExtent> const& aMeshExtents,
    std::string const& directory,
    std::string const& baseName)
{
    std::string fullPath = directory + "/" + baseName + "-triangle-positions.bin";

    uint32_t iNumMeshes = (uint32_t)aaiTriangleVertexIndices.size();
    std::vector<MeshRange> aMeshTriangleRanges(iNumMeshes);
    uint32_t iCurrStart = 0;
    for(uint32_t i = 0; i < iNumMeshes; i++)
    {
        MeshRange range;
        range.miStart = iCurrStart;
        iCurrStart += (uint32_t)aaiTriangleVertexIndices[i].size();
        range.miEnd = iCurrStart;
        aMeshTriangleRanges[i] = range;
    }
    uint32_t iTriangleRangeSize = (uint32_t)aMeshTriangleRanges.size() * sizeof(MeshRange);

    uint32_t iNumTotalVertices = (uint32_t)aTrianglePositions.size();
    uint32_t iVertexSize = (uint32_t)sizeof(Vertex);

    uint32_t iNumTotalTriangles = 0;
    for(auto const& aiTriangleVertexIndices : aaiTriangleVertexIndices)
    {
        assert(aiTriangleVertexIndices.size() % 3 == 0);
        uint32_t iNumTriangles = (uint32_t)aiTriangleVertexIndices.size() / 3;
        iNumTotalTriangles += iNumTriangles;
    }

    uint32_t iTriangleStartOffset = iNumTotalVertices * iVertexSize + iTriangleRangeSize + sizeof(uint32_t) * 5 + iNumMeshes * sizeof(MeshExtent);

    FILE* fp = fopen(fullPath.c_str(), "wb");
    fwrite(&iNumMeshes, sizeof(uint32_t), 1, fp);
    fwrite(&iNumTotalVertices, sizeof(uint32_t), 1, fp);
    fwrite(&iNumTotalTriangles, sizeof(uint32_t), 1, fp);
    fwrite(&iVertexSize, sizeof(uint32_t), 1, fp);
    fwrite(&iTriangleStartOffset, sizeof(uint32_t), 1, fp);
    fwrite(aMeshTriangleRanges.data(), sizeof(MeshRange), aMeshTriangleRanges.size(), fp);

    // mesh extents
    assert(aMeshExtents.size() == iNumMeshes + 1);
    fwrite(aMeshExtents.data(), sizeof(MeshExtent), iNumMeshes, fp);

    // vertices and triangle indices
    assert(aaiTriangleVertexIndices.size() == iNumMeshes);
    fwrite(aTrianglePositions.data(), sizeof(float4), aTrianglePositions.size(), fp);

    for(uint32_t i = 0; i < aaiTriangleVertexIndices.size(); i++)
    {
        fwrite(aaiTriangleVertexIndices[i].data(), sizeof(uint32_t), aaiTriangleVertexIndices[i].size(), fp);
    }

    fclose(fp);

    DEBUG_PRINTF("wrote to %s num meshes: %d\n", fullPath.c_str(), (int32_t)aaiTriangleVertexIndices.size());
}

/*
**
*/
void test(
    std::vector<Vertex>& aTotalVertices,
    std::vector<std::vector<uint32_t>>& aaiTriangleVertexIndices,
    std::vector<MeshRange>& aMeshRanges,
    std::vector<MeshExtent>& aMeshExtents,
    std::string const& fullPath)
{
    uint32_t iNumMeshes = 0;
    uint32_t iNumTotalVertices = 0;
    uint32_t iNumTotalTriangles = 0;
    uint32_t iVertexSize = 0;
    uint32_t iTriangleStartOffset = 0;

    FILE* fp = fopen(fullPath.c_str(), "rb");
    auto directoryEnd = fullPath.find_last_of("/");
    if(directoryEnd == std::string::npos)
    {
        directoryEnd = fullPath.find_last_of("\\");
    }
    std::string directory = fullPath.substr(0, directoryEnd);
    std::string fileName = fullPath.substr(directoryEnd + 1);
    auto baseNameEnd = fileName.find_last_of(".");
    std::string baseName = fileName.substr(0, baseNameEnd);

    uint64_t iFileSize = (uint64_t)fseek(fp, 0, SEEK_END);
    fseek(fp, (long)iFileSize - 8, SEEK_SET);
    float fTest = 0.0f;
    fread(&fTest, sizeof(float), 1, fp);
    fseek(fp, 0, SEEK_SET);

    fread(&iNumMeshes, sizeof(uint32_t), 1, fp);
    fread(&iNumTotalVertices, sizeof(uint32_t), 1, fp);
    fread(&iNumTotalTriangles, sizeof(uint32_t), 1, fp);
    fread(&iVertexSize, sizeof(uint32_t), 1, fp);
    fread(&iTriangleStartOffset, sizeof(uint32_t), 1, fp);

    aMeshRanges.resize(iNumMeshes);
    fread(aMeshRanges.data(), sizeof(MeshRange), iNumMeshes, fp);

    aMeshExtents.resize(iNumMeshes + 1);            // last mesh extent is the overall mesh
    fread(aMeshExtents.data(), sizeof(MeshExtent), iNumMeshes + 1, fp);

    aTotalVertices.resize(iNumTotalVertices);
    fread(aTotalVertices.data(), sizeof(Vertex), iNumTotalVertices, fp);

    for(uint32_t i = 0; i < iNumMeshes; i++)
    {
        MeshRange const& range = aMeshRanges[i];
        uint32_t iNumTriangleIndices = range.miEnd - range.miStart;
        std::vector<uint32_t> aiTriangles(iNumTriangleIndices);
        fread(aiTriangles.data(), sizeof(uint32_t), iNumTriangleIndices, fp);
        aaiTriangleVertexIndices.push_back(aiTriangles);
    }

    fclose(fp);

    DEBUG_PRINTF("output verifcation obj meshes: \"%s\"\n, num meshes: %d\n", fullPath.c_str(), iNumMeshes);

    std::vector<uint32_t> aiMeshes;
    for(uint32_t i = 0; i < iNumMeshes; i++)
    {
        aiMeshes.push_back(i);
    }

    char szTestOutputDirectory[256];
    sprintf(szTestOutputDirectory, "%s/test-output", directory.c_str());
    std::filesystem::create_directories(szTestOutputDirectory);

    std::stringstream oss;
    oss << directory << "/test-output/test-" << baseName << "-part-read-from-file.obj";
    saveOBJ(
        oss.str().c_str(),
        aTotalVertices,
        aaiTriangleVertexIndices,
        aiMeshes);

    DEBUG_PRINTF("save debug parts to %s\n", oss.str().c_str());
}

/*
**
*/
void saveOBJ(
    std::string const& fullPath,
    std::vector<Vertex> const& aTotalVertices,
    std::vector<std::vector<uint32_t>> const& aaiTriangleVertexIndices,
    std::vector<uint32_t> const& aiMeshes)
{
    FILE* fp = fopen(fullPath.c_str(), "wb");

    for(auto const& vertex : aTotalVertices)
    {
        fprintf(fp, "v %.4f %.4f %.4f\n",
            vertex.mPosition.x,
            vertex.mPosition.y,
            vertex.mPosition.z);
    }

    for(auto const& vertex : aTotalVertices)
    {
        fprintf(fp, "vt %.4f %.4f\n",
            vertex.mUV.x,
            vertex.mUV.y);
    }

    for(auto const& vertex : aTotalVertices)
    {
        fprintf(fp, "vn %.4f %.4f %.4f\n",
            vertex.mNormal.x,
            vertex.mNormal.y,
            vertex.mNormal.z);
    }

    for(auto const& iMesh : aiMeshes)
    {
        fprintf(fp, "o object\n");
        std::vector<uint32_t> const& aiTriangleVertexIndices = aaiTriangleVertexIndices[iMesh];
        assert(aiTriangleVertexIndices.size() % 3 == 0);
        uint32_t iNumTriangles = (uint32_t)aiTriangleVertexIndices.size() / 3;
        for(uint32_t iTriangle = 0; iTriangle < iNumTriangles; iTriangle++)
        {
            fprintf(fp, "f %d/%d/%d %d/%d/%d %d/%d/%d\n",
                aiTriangleVertexIndices[iTriangle * 3] + 1,
                aiTriangleVertexIndices[iTriangle * 3] + 1,
                aiTriangleVertexIndices[iTriangle * 3] + 1,
                aiTriangleVertexIndices[iTriangle * 3 + 1] + 1,
                aiTriangleVertexIndices[iTriangle * 3 + 1] + 1,
                aiTriangleVertexIndices[iTriangle * 3 + 1] + 1,
                aiTriangleVertexIndices[iTriangle * 3 + 2] + 1,
                aiTriangleVertexIndices[iTriangle * 3 + 2] + 1,
                aiTriangleVertexIndices[iTriangle * 3 + 2] + 1);
        }
    }

    fclose(fp);
}

/*
**
*/
bool readMaterialFile(
    std::vector<OBJMaterialInfo>& aMaterials,
    std::string const& fullPath,
    std::string const& directory,
    std::string const& baseName)
{
    FILE* fp = fopen(fullPath.c_str(), "rb");
    if(fp == nullptr)
    {
        return false;
    }

    fseek(fp, 0, SEEK_END);
    uint64_t iFileSize = (uint64_t)ftell(fp);
    fseek(fp, 0, SEEK_SET);
    std::vector<char> acFileContent((size_t)iFileSize + 1);
    acFileContent[(size_t)iFileSize] = 0;
    fread(acFileContent.data(), sizeof(char), (size_t)iFileSize, fp);
    fclose(fp);

    std::map<std::string, uint32_t> aTotalAlbedoTextureMap;
    std::map<std::string, uint32_t> aTotalNormalTextureMap;
    std::map<std::string, uint32_t> aTotalSpecularTextureMap;
    std::map<std::string, uint32_t> aTotalEmissiveTextureMap;

    int64_t iCurrPosition = 0;
    std::string fileContent = acFileContent.data();
    OBJMaterialInfo* pMaterial = nullptr;
    while(true)
    {
        int64_t iEnd = fileContent.find('\n', (uint32_t)iCurrPosition);
        if(iEnd < 0 || iEnd > (int64_t)fileContent.size() || iCurrPosition >= (int64_t)fileContent.size())
        {
            break;
        }

        std::string line = fileContent.substr((uint32_t)iCurrPosition, (uint32_t)(iEnd - iCurrPosition));
        iCurrPosition = iEnd + 1;

        std::vector<std::string> aTokens = split(line.c_str(), ' ');
        if(aTokens[0] == "newmtl")
        {
            aMaterials.resize(aMaterials.size() + 1);
            pMaterial = &aMaterials[aMaterials.size() - 1];
            pMaterial->miID = (uint32_t)aMaterials.size();
            pMaterial->mName = aTokens[1];
        }
        else if(aTokens[0] == "Kd")
        {
            pMaterial->mDiffuse = float4(
                (float)atof(aTokens[1].c_str()),
                (float)atof(aTokens[2].c_str()),
                (float)atof(aTokens[3].c_str()),
                1.0f);
        }
        else if(aTokens[0] == "Ks")
        {
            pMaterial->mSpecular = float4(
                (float)atof(aTokens[1].c_str()),
                (float)atof(aTokens[2].c_str()),
                (float)atof(aTokens[3].c_str()),
                1.0f);
        }
        else if(aTokens[0] == "Ke")
        {
            pMaterial->mEmissive = float4(
                (float)atof(aTokens[1].c_str()),
                (float)atof(aTokens[2].c_str()),
                (float)atof(aTokens[3].c_str()),
                1.0f);
        }
        else if(aTokens[0] == "map_Bump")
        {
            pMaterial->mNormalTexturePath = aTokens[aTokens.size() - 1];
            aTotalNormalTextureMap[std::string(aTokens[aTokens.size() - 1])] = 1;
        }
        else if(aTokens[0] == "map_Kd")
        {
            pMaterial->mAlbedoTexturePath = aTokens[aTokens.size() - 1];
            aTotalAlbedoTextureMap[std::string(aTokens[aTokens.size() - 1])] = 1;
        }
        else if(aTokens[0] == "map_Ks")
        {
            pMaterial->mSpecularTexturePath = aTokens[aTokens.size() - 1];
            aTotalSpecularTextureMap[std::string(aTokens[aTokens.size() - 1])] = 1;
        }
        else if(aTokens[0] == "map_Ke")
        {
            pMaterial->mEmissiveTexturePath = aTokens[aTokens.size() - 1];
            aTotalEmissiveTextureMap[std::string(aTokens[aTokens.size() - 1])] = 1;

        }
    }

    std::vector<OutputMaterialInfo> aOutputMaterials;
    aOutputMaterials.resize(aMaterials.size());
    for(uint32_t i = 0; i < aMaterials.size(); i++)
    {
        aOutputMaterials[i].miID = i + 1;
        aOutputMaterials[i].mDiffuse = aMaterials[i].mDiffuse;
        aOutputMaterials[i].mEmissive = aMaterials[i].mEmissive;
        aOutputMaterials[i].mSpecular = aMaterials[i].mSpecular;

        auto albedoIter = aTotalAlbedoTextureMap.find(aMaterials[i].mAlbedoTexturePath);
        aOutputMaterials[i].miAlbedoTextureID = (uint32_t)std::distance(aTotalAlbedoTextureMap.begin(), albedoIter);

        auto normalIter = aTotalNormalTextureMap.find(aMaterials[i].mNormalTexturePath);
        aOutputMaterials[i].miNormalTextureID = (uint32_t)std::distance(aTotalNormalTextureMap.begin(), normalIter);

        auto specularIter = aTotalSpecularTextureMap.find(aMaterials[i].mSpecularTexturePath);
        aOutputMaterials[i].miSpecularTextureID = (uint32_t)std::distance(aTotalSpecularTextureMap.begin(), specularIter);

        auto emissiveIter = aTotalEmissiveTextureMap.find(aMaterials[i].mEmissiveTexturePath);
        uint32_t iEmissiveIndex = (uint32_t)std::distance(aTotalEmissiveTextureMap.begin(), emissiveIter);
        aOutputMaterials[i].miSpecularTextureID = aOutputMaterials[i].miSpecularTextureID | (uint32_t(iEmissiveIndex) << 16);
    }

    // read from image and just set the specular color from the image
    for(auto const& keyValue : aTotalSpecularTextureMap)
    {
        auto start = keyValue.first.find_last_of("\\");
        if(start == std::string::npos)
        {
            start = keyValue.first.find_last_of("/");
        }
        start += 1;
        auto end = keyValue.first.find_last_of(".");
        auto textureBaseName = keyValue.first.substr(start, end - start);
        std::string fullPath = "d:\\Downloads\\Bistro_v4\\converted-textures\\" + textureBaseName + ".png";
        int32_t iWidth = 0, iHeight = 0, iComp = 0;
        stbi_uc* pacImageData = stbi_load(fullPath.c_str(), &iWidth, &iHeight, &iComp, 4);
        float fRed = (float)(*pacImageData) / 255.0f;
        float fRoughness = (float)(*(pacImageData + 1)) / 255.0f;
        float fMetalness = (float)(*(pacImageData + 2)) / 255.0f;
        stbi_image_free(pacImageData);
        
        auto specularIter = aTotalSpecularTextureMap.find(keyValue.first);
        uint32_t iIndex = (uint32_t)std::distance(aTotalSpecularTextureMap.begin(), specularIter);

        for(uint32_t i = 0; i < aOutputMaterials.size(); i++)
        {
            uint32_t iSpecularTextureID = (aOutputMaterials[i].miSpecularTextureID & 0xffff);
            if(iSpecularTextureID == iIndex)
            {
                aOutputMaterials[i].mSpecular = float4(fRoughness, fMetalness, 0.0f, 0.0f);
            }
        }
    }

    for(auto const& keyValue : aTotalEmissiveTextureMap)
    {
        auto start = keyValue.first.find_last_of("\\");
        if(start == std::string::npos)
        {
            start = keyValue.first.find_last_of("/");
        }
        start += 1;
        auto end = keyValue.first.find_last_of(".");
        auto textureBaseName = keyValue.first.substr(start, end - start);
        std::string fullPath = "d:\\Downloads\\Bistro_v4\\converted-textures\\" + textureBaseName + ".png";
        int32_t iWidth = 0, iHeight = 0, iComp = 0;
        stbi_uc* pacImageData = stbi_load(fullPath.c_str(), &iWidth, &iHeight, &iComp, 4);
        float fRed = (float)(*pacImageData) / 255.0f;
        float fGreen = (float)(*(pacImageData + 1)) / 255.0f;
        float fBlue = (float)(*(pacImageData + 2)) / 255.0f;
        stbi_image_free(pacImageData);

        auto specularIter = aTotalEmissiveTextureMap.find(keyValue.first);
        uint32_t iIndex = (uint32_t)std::distance(aTotalEmissiveTextureMap.begin(), specularIter);

        for(uint32_t i = 0; i < aOutputMaterials.size(); i++)
        {
            uint32_t iEmissiveTextureID = (aOutputMaterials[i].miSpecularTextureID & 0xffff0000) >> 16;
            if(iEmissiveTextureID == iIndex)
            {
                aOutputMaterials[i].mEmissive = float4(fRed, fGreen, fBlue, 1.0f);
            }
        }
    }

    OutputMaterialInfo endMaterial;
    endMaterial.miID = 99999;
    endMaterial.mDiffuse.x = FLT_MAX;
    aOutputMaterials.push_back(endMaterial);

    uint32_t iNumMaterials = (uint32_t)aOutputMaterials.size();
    std::string outputFullPath = directory + "/" + baseName + ".mat";
    fp = fopen(outputFullPath.c_str(), "wb");
    //fwrite(&iNumMaterials, sizeof(uint32_t), 1, fp);
    fwrite(aOutputMaterials.data(), sizeof(OutputMaterialInfo), aOutputMaterials.size(), fp);

    char cNewLine = '\n';
    uint32_t iNumAlbedoTextures = (uint32_t)aTotalAlbedoTextureMap.size();
    fwrite(&iNumAlbedoTextures, sizeof(uint32_t), 1, fp);
    for(auto const& keyValue : aTotalAlbedoTextureMap)
    {
        fwrite(keyValue.first.c_str(), 1, keyValue.first.length(), fp);
        fwrite(&cNewLine, sizeof(char), 1, fp);
    }

    uint32_t iNumNormalTextures = (uint32_t)aTotalNormalTextureMap.size();
    fwrite(&iNumNormalTextures, sizeof(uint32_t), 1, fp);
    for(auto const& keyValue : aTotalNormalTextureMap)
    {
        fwrite(keyValue.first.c_str(), 1, keyValue.first.length(), fp);
        fwrite(&cNewLine, sizeof(char), 1, fp);
    }

    uint32_t iNumSpecularTextures = (uint32_t)aTotalSpecularTextureMap.size();
    fwrite(&iNumSpecularTextures, sizeof(uint32_t), 1, fp);
    for(auto const& keyValue : aTotalSpecularTextureMap)
    {
        fwrite(keyValue.first.c_str(), 1, keyValue.first.length(), fp);
        fwrite(&cNewLine, sizeof(char), 1, fp);
    }

    uint32_t iNumEmissiveTextures = (uint32_t)aTotalEmissiveTextureMap.size();
    fwrite(&iNumEmissiveTextures, sizeof(uint32_t), 1, fp);
    for(auto const& keyValue : aTotalEmissiveTextureMap)
    {
        fwrite(keyValue.first.c_str(), 1, keyValue.first.length(), fp);
        fwrite(&cNewLine, sizeof(char), 1, fp);
    }

    fclose(fp);

    DEBUG_PRINTF("wrote to %s\n", outputFullPath.c_str());

    // output texture selection from shader
    char szOutputShaderDirectory[256];
    sprintf(szOutputShaderDirectory, "%s\\shaders\\", directory.c_str());
    std::filesystem::create_directories(szOutputShaderDirectory);
    char szOutputShaderFilePath[256];
    sprintf(szOutputShaderFilePath, "%s\\%s-albedo.shader", szOutputShaderDirectory, baseName.c_str());
    fp = fopen(szOutputShaderFilePath, "w");
    uint32_t iLastGroup = 0;
    for(uint32_t iPart = 0;; iPart++)
    {
        uint32_t iStartTextureIndex = iPart * 100;
        if(iStartTextureIndex >= iNumAlbedoTextures)
        {
            break;
        }
        uint32_t iEndTextureIndex = iStartTextureIndex + 100;
        if(iEndTextureIndex > iNumAlbedoTextures)
        {
            iEndTextureIndex = iNumAlbedoTextures;
        }

        uint32_t iGroup = iPart + 2;
        for(uint32_t i = iStartTextureIndex; i < iEndTextureIndex; i++)
        {
            fprintf(fp, "@group(%d) @binding(%d)\nvar texture%d: texture_2d<f32>;\n", iGroup, i, i);
        }
        fprintf(fp, "\n\n//////\nfn sampleTexture%d(\n    iTextureID: u32,\n    uv: vec2<f32>) -> vec4<f32>\n{\n", iPart);
        for(uint32_t i = iStartTextureIndex; i < iEndTextureIndex; i++)
        {
            fprintf(fp, "    if(iTextureID == %du)\n    {\n    ret = textureSample(texture%d, linearTextureSampler, uv);\n    }\n", i, i);
        }
        fprintf(fp, "}\n\n");

        iLastGroup = iGroup;
    }
    fclose(fp);

    sprintf(szOutputShaderFilePath, "%s\\%s-normal.shader", szOutputShaderDirectory, baseName.c_str());
    fp = fopen(szOutputShaderFilePath, "w");
    for(uint32_t iPart = 0;; iPart++)
    {
        uint32_t iStartTextureIndex = iPart * 100;
        if(iStartTextureIndex >= iNumNormalTextures)
        {
            break;
        }
        uint32_t iEndTextureIndex = iStartTextureIndex + 100;
        if(iEndTextureIndex > iNumNormalTextures)
        {
            iEndTextureIndex = iNumNormalTextures;
        }

        uint32_t iGroup = iPart + iLastGroup;
        for(uint32_t i = iStartTextureIndex; i < iEndTextureIndex; i++)
        {
            fprintf(fp, "@group(%d) @binding(%d)\nvar normalTexture%d: texture_2d<f32>;\n", iGroup, i, i);
        }
        fprintf(fp, "\n\n//////\nfn sampleNormalTexture%d(\n    iTextureID: u32,\n    uv: vec2<f32>) -> vec4<f32>\n{\n", iPart);
        for(uint32_t i = iStartTextureIndex; i < iEndTextureIndex; i++)
        {
            fprintf(fp, "    if(iTextureID == %du)\n    {\n    ret = textureSample(normalTexture%d, linearTextureSampler, uv);\n    }\n", i, i);
        }
        fprintf(fp, "}\n\n");
    }
    fclose(fp);

    return true;
}

/*
**
*/
void convertNormalImages(std::string const& directory)
{
    for(const auto& entry : std::filesystem::recursive_directory_iterator(directory)) {
        if(std::filesystem::is_regular_file(entry))
        {
            std::string convertedPath = entry.path().string().c_str();
            std::string baseName = convertedPath.substr(convertedPath.find_last_of("\\") + 1);
            std::string outputPath = directory + "/../normals-rgba/" + baseName;

            int iWidth = 0, iHeight = 0, iComp = 0;
            stbi_uc* pData = stbi_load(convertedPath.c_str(), &iWidth, &iHeight, &iComp, 3);
            std::vector<unsigned char> acImageData(iWidth * iHeight * 4);
            for(int32_t iY = 0; iY < iHeight; iY++)
            {
                for(int32_t iX = 0; iX < iWidth; iX++)
                {
                    uint32_t iIndex = (iY * iWidth + iX) * 3;

                    unsigned char iGreen = (unsigned char)((float(pData[iIndex] >> 4) / 15.0f) * 255.0f);
                    unsigned char iBlue = (unsigned char)((float(pData[iIndex] & 0x0f) / 15.0f) * 255.0f);
                    unsigned char iRed = (unsigned char)((float(pData[iIndex + 1] >> 4) / 15.0f) * 255.0f);

                    uint32_t iConvertedIndex = (iY * iWidth + iX) * 4;
                    acImageData[iConvertedIndex] = iRed;
                    acImageData[iConvertedIndex + 1] = iGreen;
                    acImageData[iConvertedIndex + 2] = iBlue;
                    acImageData[iConvertedIndex + 3] = 255;
                }
            }

            stbi_image_free(pData);
            stbi_write_png(outputPath.c_str(), iWidth, iHeight, 4, acImageData.data(), iWidth * 4 * sizeof(char));
            int iDebug = 1;
        }

    }
}