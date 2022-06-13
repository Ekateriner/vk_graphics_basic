#ifndef SIMPLE_RENDER_H
#define SIMPLE_RENDER_H

#define VK_NO_PROTOTYPES

#include "../../render/scene_mgr.h"
#include "../../render/render_common.h"
#include "../../render/render_gui.h"
#include "../../../resources/shaders/common.h"
#include <geom/vk_mesh.h>
#include <vk_descriptor_sets.h>
#include <vk_fbuf_attachment.h>
#include <vk_images.h>
#include <vk_swapchain.h>
#include <string>
#include <iostream>

class SimpleRender : public IRender
{
public:
  const std::string VERTEX_SHADER_PATH_LAND = "../resources/shaders/landscape.vert";
  const std::string FRAGMENT_SHADER_PATH_LAND = "../resources/shaders/landscape.frag";
  const std::string TESSELATION_CONTROL_SHADER_PATH_LAND = "../resources/shaders/landscape.tesc";
  const std::string TESSELATION_EVALUATION_SHADER_PATH_LAND = "../resources/shaders/landscape.tese";
  
  const std::string COMPUTE_SHADER_PATH_GRASS = "../resources/shaders/grass_frustum_cul.comp";
  const std::string VERTEX_SHADER_PATH_GRASS = "../resources/shaders/grass.vert";
  const std::string TESSELATION_CONTROL_SHADER_PATH_GRASS = "../resources/shaders/grass.tesc";
  const std::string TESSELATION_EVALUATION_SHADER_PATH_GRASS = "../resources/shaders/grass.tese";
  const std::string FRAGMENT_SHADER_PATH_GRASS = "../resources/shaders/grass.frag";
  
  const std::string VERTEX_SHADER_PATH_FILL = "../resources/shaders/g_buffer_fill.vert";
  const std::string FRAGMENT_SHADER_PATH_FILL = "../resources/shaders/g_buffer_fill.frag";
  const std::string VERTEX_SHADER_PATH_RESOLVE = "../resources/shaders/g_buffer_resolve.vert";
  const std::string GEOMETRY_SHADER_PATH_RESOLVE = "../resources/shaders/g_buffer_resolve.geom";
  const std::string FRAGMENT_SHADER_PATH_RESOLVE = "../resources/shaders/g_buffer_resolve.frag";
  const std::string COMPUTE_SHADER_PATH = "../resources/shaders/frustum_cul.comp";
  const std::string GEOMETRY_SHADER_PATH = "../resources/shaders/hair.geom";
  
  const std::string VERTEX_SHADER_PATH_FOG = "../resources/shaders/quad3_vert.vert";
  const std::string FRAGMENT_SHADER_PATH_FOG = "../resources/shaders/fog.frag";
  
  const std::string VERTEX_SHADER_PATH_SSAO = "../resources/shaders/quad3_vert.vert";
  const std::string FRAGMENT_SHADER_PATH_SSAO = "../resources/shaders/ssao.frag";
  
  const std::string VERTEX_SHADER_PATH_POSTE = "../resources/shaders/quad3_vert.vert";
  const std::string FRAGMENT_SHADER_PATH_POSTE = "../resources/shaders/post_effects.frag";
  
  SimpleRender(uint32_t a_width, uint32_t a_height);
  ~SimpleRender()  { Cleanup(); };

  inline uint32_t     GetWidth()      const override { return m_width; }
  inline uint32_t     GetHeight()     const override { return m_height; }
  inline VkInstance   GetVkInstance() const override { return m_instance; }
  void InitVulkan(const char** a_instanceExtensions, uint32_t a_instanceExtensionsCount, uint32_t a_deviceId) override;

  void InitPresentation(VkSurfaceKHR& a_surface, bool initGUI) override;

  void ProcessInput(const AppInput& input) override;
  void UpdateCamera(const Camera* cams, uint32_t a_camsCount) override;
  Camera GetCurrentCamera() override {return m_cam;}
  void UpdateView();

  void LoadScene(const char *path, bool transpose_inst_matrices) override;
  void DrawFrame(float a_time, DrawMode a_mode) override;

  //////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

  // debugging utils
  //
  static VKAPI_ATTR VkBool32 VKAPI_CALL debugReportCallbackFn(
    VkDebugReportFlagsEXT                       flags,
    VkDebugReportObjectTypeEXT                  objectType,
    uint64_t                                    object,
    size_t                                      location,
    int32_t                                     messageCode,
    const char* pLayerPrefix,
    const char* pMessage,
    void* pUserData)
  {
    (void)flags;
    (void)objectType;
    (void)object;
    (void)location;
    (void)messageCode;
    (void)pUserData;
    std::cout << pLayerPrefix << ": " << pMessage << std::endl;
    return VK_FALSE;
  }

  VkDebugReportCallbackEXT m_debugReportCallback = nullptr;
protected:

  VkInstance       m_instance       = VK_NULL_HANDLE;
  VkCommandPool    m_commandPool    = VK_NULL_HANDLE;
  VkPhysicalDevice m_physicalDevice = VK_NULL_HANDLE;
  VkDevice         m_device         = VK_NULL_HANDLE;
  VkQueue          m_graphicsQueue  = VK_NULL_HANDLE;
  VkQueue          m_transferQueue  = VK_NULL_HANDLE;

  vk_utils::QueueFID_T m_queueFamilyIDXs {UINT32_MAX, UINT32_MAX, UINT32_MAX};

  struct
  {
    uint32_t    currentFrame      = 0u;
    VkQueue     queue             = VK_NULL_HANDLE;
    VkSemaphore imageAvailable    = VK_NULL_HANDLE;
    VkSemaphore renderingFinished = VK_NULL_HANDLE;
  } m_presentationResources;

  std::vector<VkFence> m_frameFences;
  std::vector<VkCommandBuffer> m_cmdBuffersDrawMain;

  struct
  {
    LiteMath::float4x4 Proj;
    LiteMath::float4x4 View;
  } resolveConst;

  struct
  {
      LiteMath::float4x4 projView;
      uint32_t instance_count;
  } comp_pushConst;

  VkBuffer iicommand_buffer;
  VkBuffer drawAtomic_buffer;
  VkBuffer drawMatrices_buffer;
  VkDeviceMemory m_buffers_memory;
  
  VkBuffer grass_iicommand_buffer;
  VkBuffer grassShifts_buffer;
  VkDeviceMemory m_grass_buffers_memory;
  VkDescriptorSet m_grass_comp_dSet = VK_NULL_HANDLE;
  VkDescriptorSetLayout m_grass_comp_dSetLayout = VK_NULL_HANDLE;
  VkDescriptorSet m_grass_dSet = VK_NULL_HANDLE;
  VkDescriptorSetLayout m_grass_dSetLayout = VK_NULL_HANDLE;
  
  
  int m_landscape_width = 128;
  int m_landscape_height = 128;
  
  int m_samples_count = 128;

  UniformParams m_uniforms {};
  VkBuffer m_ubo = VK_NULL_HANDLE;
  VkDeviceMemory m_uboAlloc = VK_NULL_HANDLE;
  void* m_uboMappedMem = nullptr;

  pipeline_data_t m_fillForwardPipeline {};
  pipeline_data_t m_resolveForwardPipeline {};
  pipeline_data_t m_computeForwardPipeline {};
  pipeline_data_t m_landscapePipeline {};
  pipeline_data_t m_grass_compPipeline {};
  pipeline_data_t m_grassPipeline {};
  pipeline_data_t m_fogPipeline {};
  pipeline_data_t m_ssaoPipeline {};
  pipeline_data_t m_postEPipeline {};

  VkDescriptorSet m_fill_dSet = VK_NULL_HANDLE;
  VkDescriptorSetLayout m_fill_dSetLayout = VK_NULL_HANDLE;
  VkDescriptorSet m_resolve_dSet = VK_NULL_HANDLE;
  VkDescriptorSetLayout m_resolve_dSetLayout = VK_NULL_HANDLE;
  VkDescriptorSet m_light_dSet = VK_NULL_HANDLE;
  VkDescriptorSetLayout m_light_dSetLayout = VK_NULL_HANDLE;
  VkDescriptorSet m_postE_dSet = VK_NULL_HANDLE;
  VkDescriptorSetLayout m_postE_dSetLayout = VK_NULL_HANDLE;
  VkRenderPass m_mainRenderPass = VK_NULL_HANDLE; // main renderpass
  VkFramebuffer m_mainFrameBuffer;
  VkRenderPass m_postRenderPass = VK_NULL_HANDLE; // post renderpass
  VkRenderPass m_fogRenderPass = VK_NULL_HANDLE; // fog renderpass
  VkFramebuffer m_fogFrameBuffer;
  VkRenderPass m_ssaoRenderPass = VK_NULL_HANDLE; // ssao renderpass
  VkFramebuffer m_ssaoFrameBuffer;
  
  VkDescriptorSet m_landscape_dSet = VK_NULL_HANDLE;
  VkDescriptorSetLayout m_landscape_dSetLayout = VK_NULL_HANDLE;
  
  VkDescriptorSet m_fog_dSet = VK_NULL_HANDLE;
  VkDescriptorSetLayout m_fog_dSetLayout = VK_NULL_HANDLE;
  uint32_t fog_scale_factor = 32;
  
  VkDescriptorSet m_ssao_dSet = VK_NULL_HANDLE;
  VkDescriptorSetLayout m_ssao_dSetLayout = VK_NULL_HANDLE;
  
  VkSampler m_simple_image_sampler = VK_NULL_HANDLE;
  
  VkDescriptorSet m_comp_dSet = VK_NULL_HANDLE;
  VkDescriptorSetLayout m_comp_dSetLayout = VK_NULL_HANDLE;
  bool make_geom = false;

  std::shared_ptr<vk_utils::DescriptorMaker> m_pBindings = nullptr;

  // *** presentation
  VkSurfaceKHR m_surface = VK_NULL_HANDLE;
  VulkanSwapChain m_swapchain;
  std::vector<VkFramebuffer> m_screenFrameBuffers;
  vk_utils::VulkanImageMem m_depthBuffer{};
  vk_utils::VulkanImageMem m_normalBuffer{};
  vk_utils::VulkanImageMem m_albedoBuffer{};
  vk_utils::VulkanImageMem m_tangentBuffer{};
  static constexpr uint32_t m_gbuf_size = 4;
  
  vk_utils::VulkanImageMem m_resultImageBuffer{};
  vk_utils::VulkanImageMem m_fogImageBuffer{};
  vk_utils::VulkanImageMem m_ssaoImageBuffer{};
  // ***

  // *** GUI
  std::shared_ptr<IRenderGUI> m_pGUIRender;
  virtual void SetupGUIElements();
  void DrawFrameWithGUI();
  //

  Camera   m_cam;
  uint32_t m_width  = 1024u;
  uint32_t m_height = 1024u;
  uint32_t m_framesInFlight  = 2u;
  bool m_vsync = false;

  VkPhysicalDeviceFeatures m_enabledDeviceFeatures = {};
  std::vector<const char*> m_deviceExtensions      = {};
  std::vector<const char*> m_instanceExtensions    = {};

  bool m_enableValidation;
  std::vector<const char*> m_validationLayers;

  std::shared_ptr<SceneManager> m_pScnMgr;

  void DrawFrameSimple();

  void CreateInstance();
  void CreateDevice(uint32_t a_deviceId);

  void BuildCommandBufferSimple(VkCommandBuffer cmdBuff, VkFramebuffer frameBuff);

  virtual void SetupSimplePipeline();
  void CleanupPipelineAndSwapchain();
  void RecreateSwapChain();

  void CreateRenderpass();
  void CreateFrameBuffer();

  void CreateBuffers();
  void UpdateUniformBuffer(float a_time);

  void Cleanup();

  void SetupDeviceFeatures();
  void SetupDeviceExtensions();
  void SetupValidationLayers();
};

#endif //SIMPLE_RENDER_H
