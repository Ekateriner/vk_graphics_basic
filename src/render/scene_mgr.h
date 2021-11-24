#ifndef CHIMERA_SCENE_MGR_H
#define CHIMERA_SCENE_MGR_H

#include <vector>

#include <geom/vk_mesh.h>
#include "LiteMath.h"
#include <vk_copy.h>

#include "../loader_utils/hydraxml.h"
#include "../resources/shaders/common.h"

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

  VkPipelineVertexInputStateCreateInfo GetPipelineVertexInputStateCreateInfo() { return m_pMeshData->VertexInputLayout();}

  VkBuffer GetVertexBuffer() const { return m_geoVertBuf; }
  VkBuffer GetIndexBuffer()  const { return m_geoIdxBuf; }
  VkBuffer GetMeshInfoBuffer()  const { return m_meshInfoBuf; }
  VkBuffer GetInstanceInfoBuffer()  const { return m_instanceInfoBuf; }
  VkBuffer GetLightInfoBuffer()  const { return m_lightInfoBuf; }
  std::shared_ptr<vk_utils::ICopyEngine> GetCopyHelper() { return  m_pCopyHelper; }

  uint32_t MeshesNum() const {return (uint32_t)m_meshInfos.size();}
  uint32_t InstancesNum() const {return (uint32_t)m_instanceInfos.size();}
  uint32_t LightsNum() const {return lightCount;}

  hydra_xml::Camera GetCamera(uint32_t camId) const;
  MeshInfoWithBbox GetMeshInfo(uint32_t meshId) const {assert(meshId < m_meshInfos.size()); return m_meshInfos[meshId];}
  //const std::vector<MeshInfoWithBbox>& GetMeshInfos() const {return m_meshInfos;}
  LiteMath::Box4f GetMeshBbox(uint32_t meshId) const {assert(meshId < m_meshInfos.size()); return m_meshInfos[meshId].m_meshBbox;}
  InstanceInfoWithMatrix GetInstanceInfo(uint32_t instId) const {assert(instId < m_instanceInfos.size()); return m_instanceInfos[instId];}
  //const std::vector<InstanceInfoWithMatrix> GetInstanceInfos() const {return m_instanceInfos;}
  //LiteMath::Box4f GetInstanceBbox(uint32_t instId) const {assert(instId < m_instanceBboxes.size()); return m_instanceBboxes[instId];}
  LiteMath::float4x4 GetInstanceMatrix(uint32_t instId) const {assert(instId < m_instanceInfos.size()); return m_instanceInfos[instId].Matrix;}
  LiteMath::Box4f GetSceneBbox() const {return sceneBbox;}

private:
  void LoadGeoDataOnGPU();

  std::vector<MeshInfoWithBbox> m_meshInfos = {};
  std::shared_ptr<IMeshData> m_pMeshData = nullptr;

  std::vector<InstanceInfoWithMatrix> m_instanceInfos = {};
  //std::vector<LiteMath::Box4f> m_instanceBboxes = {};
  //std::vector<LiteMath::float4x4> m_instanceMatrices = {};

  std::vector<hydra_xml::Camera> m_sceneCameras = {};
  std::vector<LightInfo> m_lightInfos = {};
  uint32_t lightCount = 10;
  LiteMath::Box4f sceneBbox;

  uint32_t m_totalVertices = 0u;
  uint32_t m_totalIndices  = 0u;

  VkBuffer m_geoVertBuf = VK_NULL_HANDLE;
  VkBuffer m_geoIdxBuf  = VK_NULL_HANDLE;
  VkBuffer m_meshInfoBuf  = VK_NULL_HANDLE;
  VkBuffer m_instanceInfoBuf  = VK_NULL_HANDLE;
  VkBuffer m_lightInfoBuf  = VK_NULL_HANDLE;
  //VkBuffer m_instanceMatricesBuffer = VK_NULL_HANDLE;
  VkDeviceMemory m_geoMemAlloc = VK_NULL_HANDLE;

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
