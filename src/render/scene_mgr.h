#ifndef CHIMERA_SCENE_MGR_H
#define CHIMERA_SCENE_MGR_H

#include <vector>

#include <geom/vk_mesh.h>
#include "LiteMath.h"
#include <vk_copy.h>
#include "vk_images.h"

#include "../loader_utils/hydraxml.h"
//#include "../resources/shaders/common.h"

/*struct InstanceInfo
{
  uint32_t inst_id = 0u;
  uint32_t mesh_id = 0u;
  VkDeviceSize instBufOffset = 0u;
  bool renderMark = false;
};*/

struct InstanceInfoWithMatrix
{
    LiteMath::float4x4 Matrix;
    uint32_t inst_id = 0u;
    uint32_t mesh_id = 0u;
    //VkDeviceSize instBufOffset = 0u;
    //int renderMark = 0;

    char padding[8];
};

struct MeshInfoWithBbox
{
    LiteMath::Box4f m_meshBbox;
    //uint32_t m_vertNum = 0;
    uint32_t m_indNum  = 0;
    uint32_t m_vertexOffset = 0u;
    uint32_t m_indexOffset  = 0u;

    //VkDeviceSize m_vertexBufOffset = 0u;
    //VkDeviceSize m_indexBufOffset  = 0u;
    char padding[4];
};

struct LightInfo
{
    float4 color;
    float3 pos;
    float radius;
};

struct LandscapeInfo
{
    float4 scale;
    float4 trans;
    float4 sun_position;
    int width;
    int height;
};

struct GrassInfo
{
  uint2 tile_count;
  float2 near_far;
  uint2 freq_min;
  uint2 freq_max;
};

struct SceneManager
{
  SceneManager(VkDevice a_device, VkPhysicalDevice a_physDevice, uint32_t a_transferQId, uint32_t a_graphicsQId,
    bool debug = false);
  ~SceneManager() { DestroyScene(); }

  bool LoadSceneXML(const std::string &scenePath, bool transpose = true);
  void LoadSingleTriangle();

  uint32_t AddMeshFromFile(const std::string& meshPath);
  uint32_t AddMeshFromData(cmesh::SimpleMesh &meshData);

  uint32_t InstanceMesh(uint32_t meshId, const LiteMath::float4x4 &matrix, bool markForRender = true);

  void MarkInstance(uint32_t instId);
  void UnmarkInstance(uint32_t instId);

  void DrawMarkedInstances();

  void DestroyScene();
  void CleanScene();
  void GenerateLandscapeTex(int width, int height);
  void GenerateSphereSamples(int count);
  void UpdateGeoDataOnGPU();

  VkPipelineVertexInputStateCreateInfo GetPipelineVertexInputStateCreateInfo() { return m_pMeshData->VertexInputLayout();}
  VkPipelineVertexInputStateCreateInfo GetGrassPipelineVertexInputStateCreateInfo() { return m_grass_vertexInputInfo;}

  VkBuffer GetVertexBuffer() const { return m_geoVertBuf; }
  VkBuffer GetIndexBuffer()  const { return m_geoIdxBuf; }
  VkBuffer GetGrassVertexBuffer() const { return m_grassVertBuf; }
  VkBuffer GetGrassIndexBuffer()  const { return m_grassIdxBuf; }
  VkBuffer GetMeshInfoBuffer()  const { return m_meshInfoBuf; }
  VkBuffer GetInstanceInfoBuffer()  const { return m_instanceInfoBuf; }
  VkBuffer GetLightInfoBuffer()  const { return m_lightInfoBuf; }
  VkBuffer GetLandInfoBuffer()  const { return m_landInfoBuf; }
  VkBuffer GetGrassInfoBuffer()  const { return m_grassInfoBuf; }
  VkBuffer GetSampleBuffer()  const { return m_sampleBuf; }
  uint32_t GrassTilesNum() const {return (uint32_t)m_grass_info.tile_count.x * m_grass_info.tile_count.y;}
  uint32_t GrassMaxCount() const {return (uint32_t)m_grass_info.freq_max.x * m_grass_info.freq_max.y;}
  std::shared_ptr<vk_utils::ICopyEngine> GetCopyHelper() { return  m_pCopyHelper; }

  uint32_t MeshesNum() const {return (uint32_t)m_meshInfos.size();}
  uint32_t InstancesNum() const {return (uint32_t)m_instanceInfos.size();}
  uint32_t LightsNum() const {return m_lightInfos.size();}

  hydra_xml::Camera GetCamera(uint32_t camId) const;
  MeshInfoWithBbox GetMeshInfo(uint32_t meshId) const {assert(meshId < m_meshInfos.size()); return m_meshInfos[meshId];}
  //const std::vector<MeshInfoWithBbox>& GetMeshInfos() const {return m_meshInfos;}
  LiteMath::Box4f GetMeshBbox(uint32_t meshId) const {assert(meshId < m_meshInfos.size()); return m_meshInfos[meshId].m_meshBbox;}
  InstanceInfoWithMatrix GetInstanceInfo(uint32_t instId) const {assert(instId < m_instanceInfos.size()); return m_instanceInfos[instId];}
  //const std::vector<InstanceInfoWithMatrix> GetInstanceInfos() const {return m_instanceInfos;}
  //LiteMath::Box4f GetInstanceBbox(uint32_t instId) const {assert(instId < m_instanceBboxes.size()); return m_instanceBboxes[instId];}
  LiteMath::float4x4 GetInstanceMatrix(uint32_t instId) const {assert(instId < m_instanceInfos.size()); return m_instanceInfos[instId].Matrix;}
  LiteMath::Box4f GetSceneBbox() const {return sceneBbox;}
  vk_utils::VulkanImageMem GetHeightMap() const { return m_height_map; }
  vk_utils::VulkanImageMem GetGrassMap() const { return m_grass_map; }

private:
  void LoadGeoDataOnGPU();
  void DestroyBuffers();

  std::vector<MeshInfoWithBbox> m_meshInfos = {};
  std::shared_ptr<IMeshData> m_pMeshData = nullptr;

  std::vector<InstanceInfoWithMatrix> m_instanceInfos = {};
  //std::vector<LiteMath::Box4f> m_instanceBboxes = {};
  //std::vector<LiteMath::float4x4> m_instanceMatrices = {};

  std::vector<hydra_xml::Camera> m_sceneCameras = {};
  std::vector<LightInfo> m_lightInfos = {};
  //uint32_t lightCount;
  uint32_t lightGridSize = 10;
  LiteMath::Box4f sceneBbox;
  
  vk_utils::VulkanImageMem m_height_map;
  LandscapeInfo m_land_info = {};
  VkBuffer m_landInfoBuf = VK_NULL_HANDLE;
  void* m_landMappedMem = nullptr;
  VkDeviceMemory m_landMemAlloc = VK_NULL_HANDLE;
  
  VkBuffer m_sampleBuf = VK_NULL_HANDLE;
  VkDeviceMemory m_sampleMemAlloc = VK_NULL_HANDLE;
  GrassInfo m_grass_info = {};
  VkBuffer m_grassInfoBuf = VK_NULL_HANDLE;
  void* m_grassMappedMem = nullptr;
  VkDeviceMemory m_grassMemAlloc = VK_NULL_HANDLE;
  VkBuffer m_grassVertBuf = VK_NULL_HANDLE;
  VkBuffer m_grassIdxBuf  = VK_NULL_HANDLE;
  VkDeviceMemory m_grassMeshMemAlloc = VK_NULL_HANDLE;
  VkVertexInputBindingDescription   m_grass_inputBinding {};
  VkVertexInputAttributeDescription m_grass_inputAttributes {};
  VkPipelineVertexInputStateCreateInfo m_grass_vertexInputInfo = {};
  vk_utils::VulkanImageMem m_grass_map;

  uint32_t m_totalVertices = 0u;
  uint32_t m_totalIndices  = 0u;

  VkBuffer m_geoVertBuf = VK_NULL_HANDLE;
  VkBuffer m_geoIdxBuf  = VK_NULL_HANDLE;
  VkBuffer m_meshInfoBuf  = VK_NULL_HANDLE;
  VkBuffer m_instanceInfoBuf  = VK_NULL_HANDLE;
  VkBuffer m_lightInfoBuf  = VK_NULL_HANDLE;
  //VkBuffer m_instanceMatricesBuffer = VK_NULL_HANDLE;
  VkDeviceMemory m_geoMemAlloc = VK_NULL_HANDLE;
  VkDeviceMemory m_lightMemAlloc = VK_NULL_HANDLE;
  
  VkDevice m_device = VK_NULL_HANDLE;
  VkPhysicalDevice m_physDevice = VK_NULL_HANDLE;
  uint32_t m_transferQId = UINT32_MAX;
  VkQueue m_transferQ = VK_NULL_HANDLE;

  uint32_t m_graphicsQId = UINT32_MAX;
  VkQueue m_graphicsQ = VK_NULL_HANDLE;
  std::shared_ptr<vk_utils::ICopyEngine> m_pCopyHelper;

  bool m_debug = false;
  // for debugging
  struct Vertex
  {
    float pos[3];
  };
};

#endif//CHIMERA_SCENE_MGR_H
