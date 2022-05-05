#include <map>
#include <array>
#include "scene_mgr.h"
#include "vk_utils.h"
#include "vk_buffers.h"
#include "brown_gen.h"
#include "blue_gen.h"
//#include <random>
#include "../loader_utils/hydraxml.h"


VkTransformMatrixKHR transformMatrixFromFloat4x4(const LiteMath::float4x4 &m)
{
  VkTransformMatrixKHR transformMatrix;
  for(int i = 0; i < 3; ++i)
  {
    for(int j = 0; j < 4; ++j)
    {
      transformMatrix.matrix[i][j] = m(i, j);
    }
  }

  return transformMatrix;
}

SceneManager::SceneManager(VkDevice a_device, VkPhysicalDevice a_physDevice,
  uint32_t a_transferQId, uint32_t a_graphicsQId, bool debug) : m_device(a_device), m_physDevice(a_physDevice),
                 m_transferQId(a_transferQId), m_graphicsQId(a_graphicsQId), m_debug(debug)
{
  vkGetDeviceQueue(m_device, m_transferQId, 0, &m_transferQ);
  vkGetDeviceQueue(m_device, m_graphicsQId, 0, &m_graphicsQ);
  VkDeviceSize scratchMemSize = 64 * 1024 * 1024;
  m_pCopyHelper = std::make_shared<vk_utils::PingPongCopyHelper>(m_physDevice, m_device, m_transferQ, m_transferQId, scratchMemSize);
  m_pMeshData   = std::make_shared<Mesh8F>();



  m_lightInfos.push_back({float4(1.0, 0.7, 0.4, 0.3),
                          float3(50.0, 50.0, 50.0),
                          float(100.0) });

  for (uint32_t i = 0; i < lightGridSize; i++)
    for (uint32_t j = 0; j < lightGridSize; j++)
      m_lightInfos.push_back({float4(abs(cos(i)) / 2.0, abs(cos(j)) / 2.0, 0.5, 0.3f),
                              float3(20 * i, 5.0, j * 20),
                              float(15.5 + i) });

  //lightCount = m_lightInfos.size();
}

bool SceneManager::LoadSceneXML(const std::string &scenePath, bool transpose)
{
  auto hscene_main = std::make_shared<hydra_xml::HydraScene>();
  auto res         = hscene_main->LoadState(scenePath);

  if(res < 0)
  {
    RUN_TIME_ERROR("LoadSceneXML error");
    return false;
  }

  for(auto loc : hscene_main->MeshFiles())
  {
    auto meshId    = AddMeshFromFile(loc);
    auto instances = hscene_main->GetAllInstancesOfMeshLoc(loc); 
    for(size_t j = 0; j < instances.size(); ++j)
    {
      if(transpose)
        InstanceMesh(meshId, LiteMath::transpose(instances[j]));
      else
        InstanceMesh(meshId, instances[j]);
    }
  }

  for(auto cam : hscene_main->Cameras())
  {
    m_sceneCameras.push_back(cam);
  }

  LoadGeoDataOnGPU();
  hscene_main = nullptr;

  return true;
}

hydra_xml::Camera SceneManager::GetCamera(uint32_t camId) const
{
  if(camId >= m_sceneCameras.size())
  {
    std::stringstream ss;
    ss << "[SceneManager::GetCamera] camera with id = " << camId << " was not loaded, using default camera.";
    vk_utils::logWarning(ss.str());

    hydra_xml::Camera res = {};
    res.fov = 60;
    res.nearPlane = 0.1f;
    res.farPlane  = 1000.0f;
    res.pos[0] = 0.0f; res.pos[1] = 0.0f; res.pos[2] = 15.0f;
    res.up[0] = 0.0f; res.up[1] = 1.0f; res.up[2] = 0.0f;
    res.lookAt[0] = 0.0f; res.lookAt[1] = 0.0f; res.lookAt[2] = 0.0f;

    return res;
  }

  return m_sceneCameras[camId];
}

void SceneManager::LoadSingleTriangle()
{
  std::vector<Vertex> vertices =
  {
    { {  1.0f,  1.0f, 0.0f } },
    { { -1.0f,  1.0f, 0.0f } },
    { {  0.0f, -1.0f, 0.0f } }
  };

  std::vector<uint32_t> indices = { 0, 1, 2 };
  m_totalIndices = static_cast<uint32_t>(indices.size());

  VkDeviceSize vertexBufSize = sizeof(Vertex) * vertices.size();
  VkDeviceSize indexBufSize  = sizeof(uint32_t) * indices.size();
  
  VkMemoryRequirements vertMemReq, idxMemReq; 
  m_geoVertBuf = vk_utils::createBuffer(m_device, vertexBufSize, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT, &vertMemReq);
  m_geoIdxBuf  = vk_utils::createBuffer(m_device, indexBufSize,  VK_BUFFER_USAGE_INDEX_BUFFER_BIT  | VK_BUFFER_USAGE_TRANSFER_DST_BIT, &idxMemReq);

  size_t pad = vk_utils::getPaddedSize(vertMemReq.size, idxMemReq.alignment);

  VkMemoryAllocateInfo allocateInfo = {};
  allocateInfo.sType           = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
  allocateInfo.pNext           = nullptr;
  allocateInfo.allocationSize  = pad + idxMemReq.size;
  allocateInfo.memoryTypeIndex = vk_utils::findMemoryType(vertMemReq.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, m_physDevice);


  VK_CHECK_RESULT(vkAllocateMemory(m_device, &allocateInfo, nullptr, &m_geoMemAlloc));

  VK_CHECK_RESULT(vkBindBufferMemory(m_device, m_geoVertBuf, m_geoMemAlloc, 0));
  VK_CHECK_RESULT(vkBindBufferMemory(m_device, m_geoIdxBuf,  m_geoMemAlloc, pad));
  m_pCopyHelper->UpdateBuffer(m_geoVertBuf, 0, vertices.data(),  vertexBufSize);
  m_pCopyHelper->UpdateBuffer(m_geoIdxBuf,  0, indices.data(), indexBufSize);
}



uint32_t SceneManager::AddMeshFromFile(const std::string& meshPath)
{
  //@TODO: other file formats
  auto data = cmesh::LoadMeshFromVSGF(meshPath.c_str());

  if(data.VerticesNum() == 0)
    RUN_TIME_ERROR(("can't load mesh at " + meshPath).c_str());

  return AddMeshFromData(data);
}

uint32_t SceneManager::AddMeshFromData(cmesh::SimpleMesh &meshData)
{
  assert(meshData.VerticesNum() > 0);
  assert(meshData.IndicesNum() > 0);

  m_pMeshData->Append(meshData);

  MeshInfoWithBbox info;
  info.m_indNum  = (uint32_t)meshData.IndicesNum();
  info.m_vertexOffset = m_totalVertices;
  info.m_indexOffset  = m_totalIndices;

  m_totalVertices += (uint32_t)meshData.VerticesNum();
  m_totalIndices  += (uint32_t)meshData.IndicesNum();

  Box4f meshBox;
  for (uint32_t i = 0; i < meshData.VerticesNum(); ++i) {
    meshBox.include(reinterpret_cast<float4*>(meshData.vPos4f.data())[i]);
  }
  info.m_meshBbox = meshBox;
  m_meshInfos.push_back(info);

  return (uint32_t)m_meshInfos.size() - 1;
}

uint32_t SceneManager::InstanceMesh(const uint32_t meshId, const LiteMath::float4x4 &matrix, bool markForRender)
{
  assert(meshId < m_meshInfos.size());

  //@TODO: maybe move
  //m_instanceMatrices.push_back(matrix);

  InstanceInfoWithMatrix info;
  info.inst_id       = (uint32_t)m_instanceInfos.size();
  info.mesh_id       = meshId;
  info.Matrix = matrix;

  m_instanceInfos.push_back(info);

  Box4f instBox;
  for (uint32_t i = 0; i < 8; ++i) {
    float4 corner = float4(
      (i & 1) == 0 ? m_meshInfos[meshId].m_meshBbox.boxMin.x : m_meshInfos[meshId].m_meshBbox.boxMax.x,
      (i & 2) == 0 ? m_meshInfos[meshId].m_meshBbox.boxMin.y : m_meshInfos[meshId].m_meshBbox.boxMax.y,
      (i & 4) == 0 ? m_meshInfos[meshId].m_meshBbox.boxMin.z : m_meshInfos[meshId].m_meshBbox.boxMax.z,
      1
    );
    instBox.include(matrix * corner);
  }
  sceneBbox.include(instBox);
  //m_instanceBboxes.push_back(instBox);

  return info.inst_id;
}

/*void SceneManager::MarkInstance(const uint32_t instId)
{
  assert(instId < m_instanceInfos.size());
  m_instanceInfos[instId].renderMark = true;
}

void SceneManager::UnmarkInstance(const uint32_t instId)
{
  assert(instId < m_instanceInfos.size());
  m_instanceInfos[instId].renderMark = false;
}*/

void SceneManager::LoadGeoDataOnGPU()
{
  VkDeviceSize vertexBufSize = m_pMeshData->VertexDataSize();
  VkDeviceSize indexBufSize = m_pMeshData->IndexDataSize();
  VkDeviceSize minfoBufSize = m_meshInfos.size() * sizeof(MeshInfoWithBbox);
  VkDeviceSize iinfoBufSize = m_instanceInfos.size() * sizeof(InstanceInfoWithMatrix);

  m_geoVertBuf  = vk_utils::createBuffer(m_device, vertexBufSize, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT);
  m_geoIdxBuf   = vk_utils::createBuffer(m_device, indexBufSize,  VK_BUFFER_USAGE_INDEX_BUFFER_BIT  | VK_BUFFER_USAGE_TRANSFER_DST_BIT);
  m_meshInfoBuf = vk_utils::createBuffer(m_device, minfoBufSize,   VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT);
  m_instanceInfoBuf = vk_utils::createBuffer(m_device, iinfoBufSize,   VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT);
  
  VkMemoryAllocateFlags allocFlags {};

  m_geoMemAlloc = vk_utils::allocateAndBindWithPadding(m_device, m_physDevice, {m_geoVertBuf, m_geoIdxBuf, m_meshInfoBuf, m_instanceInfoBuf}, allocFlags);
  
//  std::vector<LiteMath::uint2> mesh_info_tmp;
//  for(const auto& m : m_meshInfos)
//  {
//    mesh_info_tmp.emplace_back(m.m_indexOffset, m.m_vertexOffset);
//  }
//  if(!mesh_info_tmp.empty())
//    m_pCopyHelper->UpdateBuffer(m_meshInfoBuf,  0, mesh_info_tmp.data(), mesh_info_tmp.size() * sizeof(mesh_info_tmp[0]));

  m_pCopyHelper->UpdateBuffer(m_geoVertBuf, 0, m_pMeshData->VertexData(), vertexBufSize);
  m_pCopyHelper->UpdateBuffer(m_geoIdxBuf,  0, m_pMeshData->IndexData(), indexBufSize);
  m_pCopyHelper->UpdateBuffer(m_meshInfoBuf, 0, m_meshInfos.data(), minfoBufSize);
  m_pCopyHelper->UpdateBuffer(m_instanceInfoBuf, 0, m_instanceInfos.data(), iinfoBufSize);
  
  if (m_lightInfos.size() > 0) {
    VkDeviceSize linfoBufSize = m_lightInfos.size() * sizeof(LightInfo);
    m_lightInfoBuf = vk_utils::createBuffer(m_device, linfoBufSize,
                                            VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT);
    m_lightMemAlloc = vk_utils::allocateAndBindWithPadding(m_device, m_physDevice, {m_lightInfoBuf}, allocFlags);
    m_pCopyHelper->UpdateBuffer(m_lightInfoBuf, 0, m_lightInfos.data(), linfoBufSize);
  }
}

void SceneManager::UpdateGeoDataOnGPU()
{
  DestroyBuffers();
  LoadGeoDataOnGPU();
}


void SceneManager::DrawMarkedInstances()
{

}

void SceneManager::DestroyBuffers() {
  if(m_geoVertBuf != VK_NULL_HANDLE)
  {
    vkDestroyBuffer(m_device, m_geoVertBuf, nullptr);
    m_geoVertBuf = VK_NULL_HANDLE;
  }

  if(m_geoIdxBuf != VK_NULL_HANDLE)
  {
    vkDestroyBuffer(m_device, m_geoIdxBuf, nullptr);
    m_geoIdxBuf = VK_NULL_HANDLE;
  }

  if(m_meshInfoBuf != VK_NULL_HANDLE)
  {
    vkDestroyBuffer(m_device, m_meshInfoBuf, nullptr);
    m_meshInfoBuf = VK_NULL_HANDLE;
  }

  if(m_instanceInfoBuf != VK_NULL_HANDLE)
  {
    vkDestroyBuffer(m_device, m_instanceInfoBuf, nullptr);
    m_instanceInfoBuf = VK_NULL_HANDLE;
  }

  if(m_lightInfoBuf != VK_NULL_HANDLE)
  {
    vkDestroyBuffer(m_device, m_lightInfoBuf, nullptr);
    m_lightInfoBuf = VK_NULL_HANDLE;
  }
  
  if(m_landInfoBuf != VK_NULL_HANDLE)
  {
    vkDestroyBuffer(m_device, m_landInfoBuf, nullptr);
    m_landInfoBuf = VK_NULL_HANDLE;
  }
  
  if(m_landMemAlloc != VK_NULL_HANDLE)
  {
    vkFreeMemory(m_device, m_landMemAlloc, nullptr);
    m_landMemAlloc = VK_NULL_HANDLE;
  }
  
  if(m_grassInfoBuf != VK_NULL_HANDLE)
  {
    vkDestroyBuffer(m_device, m_grassInfoBuf, nullptr);
    m_grassInfoBuf = VK_NULL_HANDLE;
  }
  
  if(m_grassMemAlloc != VK_NULL_HANDLE)
  {
    vkFreeMemory(m_device, m_grassMemAlloc, nullptr);
    m_grassMemAlloc = VK_NULL_HANDLE;
  }
  
  if(m_grassVertBuf != VK_NULL_HANDLE)
  {
    vkDestroyBuffer(m_device, m_grassVertBuf, nullptr);
    m_grassVertBuf = VK_NULL_HANDLE;
  }
  
  if(m_grassIdxBuf != VK_NULL_HANDLE)
  {
    vkDestroyBuffer(m_device, m_grassIdxBuf, nullptr);
    m_grassIdxBuf = VK_NULL_HANDLE;
  }
  
  if(m_grassMeshMemAlloc != VK_NULL_HANDLE)
  {
    vkFreeMemory(m_device, m_grassMeshMemAlloc, nullptr);
    m_grassMeshMemAlloc = VK_NULL_HANDLE;
  }

  if(m_geoMemAlloc != VK_NULL_HANDLE)
  {
    vkFreeMemory(m_device, m_geoMemAlloc, nullptr);
    m_geoMemAlloc = VK_NULL_HANDLE;
  }
  
  if(m_lightMemAlloc != VK_NULL_HANDLE)
  {
    vkFreeMemory(m_device, m_lightMemAlloc, nullptr);
    m_lightMemAlloc = VK_NULL_HANDLE;
  }
}

void SceneManager::DestroyScene()
{
  DestroyBuffers();

  m_pCopyHelper = nullptr;
  vk_utils::deleteImg(m_device, &m_height_map);
  vk_utils::deleteImg(m_device, &m_grass_map);

  m_meshInfos.clear();
  m_pMeshData = nullptr;
  m_instanceInfos.clear();
  m_lightInfos.clear();
}

void SceneManager::CleanScene()
{
  if(m_geoVertBuf != VK_NULL_HANDLE)
  {
    vkDestroyBuffer(m_device, m_geoVertBuf, nullptr);
    m_geoVertBuf = VK_NULL_HANDLE;
  }
  
  if(m_geoIdxBuf != VK_NULL_HANDLE)
  {
    vkDestroyBuffer(m_device, m_geoIdxBuf, nullptr);
    m_geoIdxBuf = VK_NULL_HANDLE;
  }
  
  if(m_meshInfoBuf != VK_NULL_HANDLE)
  {
    vkDestroyBuffer(m_device, m_meshInfoBuf, nullptr);
    m_meshInfoBuf = VK_NULL_HANDLE;
  }
  
  if(m_instanceInfoBuf != VK_NULL_HANDLE)
  {
    vkDestroyBuffer(m_device, m_instanceInfoBuf, nullptr);
    m_instanceInfoBuf = VK_NULL_HANDLE;
  }
  
  if(m_geoMemAlloc != VK_NULL_HANDLE)
  {
    vkFreeMemory(m_device, m_geoMemAlloc, nullptr);
    m_geoMemAlloc = VK_NULL_HANDLE;
  }
  
  m_meshInfos.clear();
  m_instanceInfos.clear();
};

void SceneManager::GenerateLandscapeTex(int width, int height) {
  std::vector<float> tex;
  float scale = 3.0;
  for (int i = 0; i < height; i++) {
    for (int j = 0; j < width; j++) {
      LiteMath::float2 st = LiteMath::float2(float(i)/width, float(j) / height);
      st.x *= float(width)/height;
      
      tex.push_back(BrownGenerator::fbm( st * scale));
    }
  }
  m_height_map = vk_utils::allocateColorTextureFromDataLDR(m_device, m_physDevice,
                                            reinterpret_cast<const unsigned char*>(tex.data()), width, height,
                                            1, VK_FORMAT_R32_SFLOAT, m_pCopyHelper,
                                            VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT);
  
  {
    VkMemoryRequirements memReq;
    m_landInfoBuf = vk_utils::createBuffer(m_device, sizeof(LandscapeInfo), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
                                           &memReq);
    VkMemoryAllocateInfo allocateInfo = {};
    allocateInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocateInfo.pNext = nullptr;
    allocateInfo.allocationSize = memReq.size;
    allocateInfo.memoryTypeIndex = vk_utils::findMemoryType(memReq.memoryTypeBits,
                                                            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                                                            VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                                                            m_physDevice);
    VK_CHECK_RESULT(vkAllocateMemory(m_device, &allocateInfo, nullptr, &m_landMemAlloc))
    
    VK_CHECK_RESULT(vkBindBufferMemory(m_device, m_landInfoBuf, m_landMemAlloc, 0));
    
    vkMapMemory(m_device, m_landMemAlloc, 0, sizeof(LandscapeInfo), 0, &m_landMappedMem);
    m_land_info.height = height;
    m_land_info.width = width;
    m_land_info.scale = float4(width, 40.0, height, 0.0);
    m_land_info.trans = float4(-10.0f, -10.0, -10.0, 0.0);
    //m_land_info.trans = float3(-width / 4, -1.0, -height/4);
    m_land_info.sun_position = float4(100.0, 90.0, 100.0, 1.0);
    m_lightInfos.push_back({float4(1.0),
                            LiteMath::to_float3(m_land_info.sun_position),
                            float(0.0)});
    memcpy(m_landMappedMem, &m_land_info, sizeof(LandscapeInfo));
  }
  
  
  {
    std::vector<unsigned int> grass_tex;
    for (int i = 0; i < 256; i++) {
      for (int j = 0; j < 256; j++) {
        grass_tex.push_back(BlueGenerator::get_noise(LiteMath::uint2(i, j)));
      }
    }
    m_grass_map = vk_utils::allocateColorTextureFromDataLDR(m_device, m_physDevice,
                                                            reinterpret_cast<const unsigned char*>(grass_tex.data()), 256, 256,
                                                            1, VK_FORMAT_R8G8B8A8_UNORM, m_pCopyHelper,
                                                            VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT);
    
    VkMemoryRequirements memReq;
    m_grassInfoBuf = vk_utils::createBuffer(m_device, sizeof(GrassInfo), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
                                            &memReq);
    VkMemoryAllocateInfo allocateInfo = {};
    allocateInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocateInfo.pNext = nullptr;
    allocateInfo.allocationSize = memReq.size;
    allocateInfo.memoryTypeIndex = vk_utils::findMemoryType(memReq.memoryTypeBits,
                                                            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                                                            VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                                                            m_physDevice);
    VK_CHECK_RESULT(vkAllocateMemory(m_device, &allocateInfo, nullptr, &m_grassMemAlloc))
    
    VK_CHECK_RESULT(vkBindBufferMemory(m_device, m_grassInfoBuf, m_grassMemAlloc, 0));
    
    vkMapMemory(m_device, m_grassMemAlloc, 0, sizeof(GrassInfo), 0, &m_grassMappedMem);
    m_grass_info.tile_count = uint2(32, 32);
    m_grass_info.near_far = float2(20.0, 50.0);
    m_grass_info.freq_min = uint2(10, 10);
    m_grass_info.freq_max = uint2(20, 20);
    
    memcpy(m_grassMappedMem, &m_grass_info, sizeof(GrassInfo));
  }
  {
    // single grass mesh
    std::vector<Vertex> vertices =
    {
            { {  0.01f,  0.0f, 0.0f } },
            { { -0.01f,  0.0f, 0.0f } },
            { {  0.0f,  0.1f, 0.0f } }
    };
  
    std::vector<uint32_t> indices = { 0, 1, 2 };
    
    VkDeviceSize vertexBufSize = sizeof(Vertex) * vertices.size();
    VkDeviceSize indexBufSize  = sizeof(uint32_t) * indices.size();
  
    VkMemoryRequirements vertMemReq, idxMemReq;
    m_grassVertBuf = vk_utils::createBuffer(m_device, vertexBufSize, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT, &vertMemReq);
    m_grassIdxBuf  = vk_utils::createBuffer(m_device, indexBufSize,  VK_BUFFER_USAGE_INDEX_BUFFER_BIT  | VK_BUFFER_USAGE_TRANSFER_DST_BIT, &idxMemReq);
  
    size_t pad = vk_utils::getPaddedSize(vertMemReq.size, idxMemReq.alignment);
  
    VkMemoryAllocateInfo allocateInfo = {};
    allocateInfo.sType           = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocateInfo.pNext           = nullptr;
    allocateInfo.allocationSize  = pad + idxMemReq.size;
    allocateInfo.memoryTypeIndex = vk_utils::findMemoryType(vertMemReq.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, m_physDevice);
  
  
    VK_CHECK_RESULT(vkAllocateMemory(m_device, &allocateInfo, nullptr, &m_grassMeshMemAlloc));
  
    VK_CHECK_RESULT(vkBindBufferMemory(m_device, m_grassVertBuf, m_grassMeshMemAlloc, 0));
    VK_CHECK_RESULT(vkBindBufferMemory(m_device, m_grassIdxBuf,  m_grassMeshMemAlloc, pad));
    m_pCopyHelper->UpdateBuffer(m_grassVertBuf, 0, vertices.data(),  vertexBufSize);
    m_pCopyHelper->UpdateBuffer(m_grassIdxBuf,  0, indices.data(), indexBufSize);
    
    m_grass_inputBinding.binding   = 0;
    m_grass_inputBinding.stride    = sizeof(Vertex);
    m_grass_inputBinding.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
    
    m_grass_inputAttributes.binding  = 0;
    m_grass_inputAttributes.location = 0;
    m_grass_inputAttributes.format   = VK_FORMAT_R32G32B32_SFLOAT;
    m_grass_inputAttributes.offset   = 0;
  
    m_grass_vertexInputInfo.sType                           = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    m_grass_vertexInputInfo.vertexBindingDescriptionCount   = 1;
    m_grass_vertexInputInfo.vertexAttributeDescriptionCount = 1;
    m_grass_vertexInputInfo.pVertexBindingDescriptions      = &m_grass_inputBinding;
    m_grass_vertexInputInfo.pVertexAttributeDescriptions    = &m_grass_inputAttributes;
  }
  
  if(m_lightInfoBuf != VK_NULL_HANDLE)
  {
    vkDestroyBuffer(m_device, m_lightInfoBuf, nullptr);
    m_lightInfoBuf = VK_NULL_HANDLE;
  }
  if(m_lightMemAlloc != VK_NULL_HANDLE)
  {
    vkFreeMemory(m_device, m_lightMemAlloc, nullptr);
    m_lightMemAlloc = VK_NULL_HANDLE;
  }
  
  VkDeviceSize linfoBufSize = m_lightInfos.size() * sizeof(LightInfo);
  m_lightInfoBuf = vk_utils::createBuffer(m_device, linfoBufSize,   VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT);
  VkMemoryAllocateFlags allocFlags {};
  m_lightMemAlloc = vk_utils::allocateAndBindWithPadding(m_device, m_physDevice, {m_lightInfoBuf}, allocFlags);
  m_pCopyHelper->UpdateBuffer(m_lightInfoBuf, 0, m_lightInfos.data(), linfoBufSize);
}