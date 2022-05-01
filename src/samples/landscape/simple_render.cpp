#include "simple_render.h"
#include "../../utils/input_definitions.h"

#include <geom/vk_mesh.h>
#include <vk_pipeline.h>
#include <vk_buffers.h>
#include <array>

SimpleRender::SimpleRender(uint32_t a_width, uint32_t a_height) : m_width(a_width), m_height(a_height)
{
#ifdef NDEBUG
  m_enableValidation = false;
#else
  m_enableValidation = true;
#endif
}

void SimpleRender::SetupDeviceFeatures()
{
  // m_enabledDeviceFeatures.fillModeNonSolid = VK_TRUE;
  m_enabledDeviceFeatures.drawIndirectFirstInstance = VK_TRUE;
  m_enabledDeviceFeatures.multiDrawIndirect = VK_TRUE;
  m_enabledDeviceFeatures.geometryShader = VK_TRUE;
  m_enabledDeviceFeatures.tessellationShader = VK_TRUE;
}

void SimpleRender::SetupDeviceExtensions()
{
  m_deviceExtensions.push_back(VK_KHR_SWAPCHAIN_EXTENSION_NAME);
}

void SimpleRender::SetupValidationLayers()
{
  m_validationLayers.push_back("VK_LAYER_KHRONOS_validation");
  m_validationLayers.push_back("VK_LAYER_LUNARG_monitor");
}

void SimpleRender::InitVulkan(const char** a_instanceExtensions, uint32_t a_instanceExtensionsCount, uint32_t a_deviceId)
{
  for(size_t i = 0; i < a_instanceExtensionsCount; ++i)
  {
    m_instanceExtensions.push_back(a_instanceExtensions[i]);
  }

  SetupValidationLayers();
  VK_CHECK_RESULT(volkInitialize());
  CreateInstance();
  volkLoadInstance(m_instance);

  CreateDevice(a_deviceId);
  volkLoadDevice(m_device);

  m_commandPool = vk_utils::createCommandPool(m_device, m_queueFamilyIDXs.graphics,
                                              VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT);

  m_cmdBuffersDrawMain.reserve(m_framesInFlight);
  m_cmdBuffersDrawMain = vk_utils::createCommandBuffers(m_device, m_commandPool, m_framesInFlight);

  m_frameFences.resize(m_framesInFlight);
  VkFenceCreateInfo fenceInfo = {};
  fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
  fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;
  for (size_t i = 0; i < m_framesInFlight; i++)
  {
    VK_CHECK_RESULT(vkCreateFence(m_device, &fenceInfo, nullptr, &m_frameFences[i]));
  }

  m_pScnMgr = std::make_shared<SceneManager>(m_device, m_physicalDevice, m_queueFamilyIDXs.transfer,
                                             m_queueFamilyIDXs.graphics, false);
}

void SimpleRender::InitPresentation(VkSurfaceKHR &a_surface, bool initGUI)
{
  m_surface = a_surface;

  m_presentationResources.queue = m_swapchain.CreateSwapChain(m_physicalDevice, m_device, m_surface,
                                                              m_width, m_height, m_framesInFlight, m_vsync);
  m_presentationResources.currentFrame = 0;

  VkSemaphoreCreateInfo semaphoreInfo = {};
  semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
  VK_CHECK_RESULT(vkCreateSemaphore(m_device, &semaphoreInfo, nullptr, &m_presentationResources.imageAvailable));
  VK_CHECK_RESULT(vkCreateSemaphore(m_device, &semaphoreInfo, nullptr, &m_presentationResources.renderingFinished));

  std::vector<VkFormat> depthFormats = {
      VK_FORMAT_D32_SFLOAT,
      VK_FORMAT_D32_SFLOAT_S8_UINT,
      VK_FORMAT_D24_UNORM_S8_UINT,
      VK_FORMAT_D16_UNORM_S8_UINT,
      VK_FORMAT_D16_UNORM
  };
  vk_utils::getSupportedDepthFormat(m_physicalDevice, depthFormats, &m_depthBuffer.format);
  m_depthBuffer.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
  m_normalBuffer.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
  m_albedoBuffer.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
  m_tangentBuffer.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
  m_resultImageBuffer.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
  vk_utils::createImgAllocAndBind(m_device, m_physicalDevice, m_width, m_height,
                                  m_depthBuffer.format,
                                  VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT, &m_depthBuffer);
  vk_utils::createImgAllocAndBind(m_device, m_physicalDevice, m_width, m_height,
                                  VK_FORMAT_R16G16B16A16_SFLOAT,
                                  VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT, &m_normalBuffer);
  vk_utils::createImgAllocAndBind(m_device, m_physicalDevice, m_width, m_height,
                                  VK_FORMAT_R8G8B8A8_UNORM,
                                  VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT, &m_albedoBuffer);
  vk_utils::createImgAllocAndBind(m_device, m_physicalDevice, m_width, m_height,
                                  VK_FORMAT_R16G16B16A16_SFLOAT,
                                  VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT, &m_tangentBuffer);
  vk_utils::createImgAllocAndBind(m_device, m_physicalDevice, m_width, m_height,
                                  VK_FORMAT_R16G16B16A16_SFLOAT,
                                  VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT, &m_resultImageBuffer);
  
  CreateRenderpass();
  CreateFrameBuffer();
  
  if(initGUI)
    m_pGUIRender = std::make_shared<ImGuiRender>(m_instance, m_device, m_physicalDevice, m_queueFamilyIDXs.graphics, m_graphicsQueue, m_swapchain);
  
  m_simple_image_sampler = vk_utils::createSampler(
          m_device, VK_FILTER_LINEAR, VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER,
          VK_BORDER_COLOR_FLOAT_TRANSPARENT_BLACK);
}

void SimpleRender::CreateRenderpass() {
    // main renderpass
  {
    std::vector<VkAttachmentDescription> attachmentDescriptions;
    attachmentDescriptions.push_back(
            VkAttachmentDescription{
                    .flags = VK_ATTACHMENT_DESCRIPTION_MAY_ALIAS_BIT,
                    .format = m_resultImageBuffer.format,//m_swapchain.GetFormat(),
                    .samples = VK_SAMPLE_COUNT_1_BIT,
                    .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
                    .storeOp = VK_ATTACHMENT_STORE_OP_STORE,
                    .stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
                    .stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
                    .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
                    .finalLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL});
    attachmentDescriptions.push_back(
            VkAttachmentDescription{
                    .flags = VK_ATTACHMENT_DESCRIPTION_MAY_ALIAS_BIT,
                    .format = m_depthBuffer.format,
                    .samples = VK_SAMPLE_COUNT_1_BIT,
                    .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
                    .storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
                    .stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
                    .stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
                    .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
                    .finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL});
    for (auto format: {m_normalBuffer.format, m_albedoBuffer.format, m_tangentBuffer.format}) {
      attachmentDescriptions.push_back(
              VkAttachmentDescription{
                      .flags = VK_ATTACHMENT_DESCRIPTION_MAY_ALIAS_BIT,
                      .format = format,
                      .samples = VK_SAMPLE_COUNT_1_BIT,
                      .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
                      .storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
                      .stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
                      .stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
                      .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
                      .finalLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL});
    }
    
    std::array<VkSubpassDescription, 2> subpasses{}; // create and resolve
    
    std::vector<VkAttachmentReference> colorAttachmentRefs;
    //colorAttachmentRefs.push_back(VkAttachmentReference{0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL});
    for (uint32_t i = 2; i < m_gbuf_size + 1; i++)
      colorAttachmentRefs.push_back(VkAttachmentReference{i, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL});
    
    VkAttachmentReference depthAttachmentRef = {};
    depthAttachmentRef.attachment = 1;
    depthAttachmentRef.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
    subpasses[0].pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpasses[0].colorAttachmentCount = colorAttachmentRefs.size();
    subpasses[0].pColorAttachments = colorAttachmentRefs.data();
    subpasses[0].pDepthStencilAttachment = &depthAttachmentRef;
    
    std::vector<VkAttachmentReference> inputAttachmentRefs;
    for (uint32_t i = 1; i < m_gbuf_size + 1; i++)
      inputAttachmentRefs.push_back(VkAttachmentReference{i, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL});
    VkAttachmentReference presentAttachmentRef = {};
    presentAttachmentRef.attachment = 0;
    presentAttachmentRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    
    subpasses[1].pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpasses[1].inputAttachmentCount = inputAttachmentRefs.size();
    subpasses[1].pInputAttachments = inputAttachmentRefs.data();
    subpasses[1].colorAttachmentCount = 1;
    subpasses[1].pColorAttachments = &presentAttachmentRef;
    
    std::array<VkSubpassDependency, 2> dependencies{};
    
    dependencies[0].srcSubpass = VK_SUBPASS_EXTERNAL;
    dependencies[0].dstSubpass = 1;
    dependencies[0].srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    dependencies[0].dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    dependencies[0].srcAccessMask = 0;
    dependencies[0].dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
    dependencies[0].dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;
    
    dependencies[1].srcSubpass = 0;
    dependencies[1].dstSubpass = 1;
    dependencies[1].srcStageMask =
            VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
    dependencies[1].dstStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
    dependencies[1].srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
    dependencies[1].dstAccessMask = VK_ACCESS_INPUT_ATTACHMENT_READ_BIT;
    dependencies[1].dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;
  
//    dependencies[2].srcSubpass = 1;
//    dependencies[2].dstSubpass = VK_SUBPASS_EXTERNAL;
//    dependencies[2].srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
//    dependencies[2].dstStageMask = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
//    dependencies[2].srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
//    dependencies[2].dstAccessMask = VK_ACCESS_MEMORY_READ_BIT;
//    dependencies[2].dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;
    
    VkRenderPassCreateInfo renderPassInfo = {};
    renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    renderPassInfo.attachmentCount = attachmentDescriptions.size();
    renderPassInfo.pAttachments = attachmentDescriptions.data();
    renderPassInfo.subpassCount = subpasses.size();
    renderPassInfo.pSubpasses = subpasses.data();
    renderPassInfo.dependencyCount = dependencies.size();
    renderPassInfo.pDependencies = dependencies.data();
    
    VK_CHECK_RESULT(vkCreateRenderPass(m_device, &renderPassInfo, nullptr, &m_mainRenderPass));
  }
  
  // post render pass
  {
    std::vector<VkAttachmentDescription> attachmentDescriptions;
    attachmentDescriptions.push_back(
            VkAttachmentDescription{
                    .flags = VK_ATTACHMENT_DESCRIPTION_MAY_ALIAS_BIT,
                    .format = m_swapchain.GetFormat(),
                    .samples = VK_SAMPLE_COUNT_1_BIT,
                    .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
                    .storeOp = VK_ATTACHMENT_STORE_OP_STORE,
                    .stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
                    .stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
                    .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
                    .finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR});
  
    std::array<VkSubpassDescription, 1> subpasses{};
  
    std::vector<VkAttachmentReference> colorAttachmentRefs;
    colorAttachmentRefs.push_back(VkAttachmentReference{0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL});
    
    subpasses[0].pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpasses[0].colorAttachmentCount = colorAttachmentRefs.size();
    subpasses[0].pColorAttachments = colorAttachmentRefs.data();
  
    std::array<VkSubpassDependency, 1> dependencies{};
  
    dependencies[0].srcSubpass = VK_SUBPASS_EXTERNAL;
    dependencies[0].dstSubpass = 0;
    dependencies[0].srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    dependencies[0].dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    dependencies[0].srcAccessMask = {};
    dependencies[0].dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
    dependencies[0].dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;
  
//    dependencies[1].srcSubpass = 0;
//    dependencies[1].dstSubpass = VK_SUBPASS_EXTERNAL;
//    dependencies[1].srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
//    dependencies[1].dstStageMask = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
//    dependencies[1].srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
//    dependencies[1].dstAccessMask = VK_ACCESS_MEMORY_READ_BIT;
//    dependencies[1].dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;
  
    VkRenderPassCreateInfo renderPassInfo = {};
    renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    renderPassInfo.attachmentCount = attachmentDescriptions.size();
    renderPassInfo.pAttachments = attachmentDescriptions.data();
    renderPassInfo.subpassCount = subpasses.size();
    renderPassInfo.pSubpasses = subpasses.data();
    renderPassInfo.dependencyCount = dependencies.size();
    renderPassInfo.pDependencies = dependencies.data();
  
    VK_CHECK_RESULT(vkCreateRenderPass(m_device, &renderPassInfo, nullptr, &m_postRenderPass));
  }
}

void SimpleRender::CreateFrameBuffer()
{
  // main frame buffer
  {
    std::array<VkImageView, m_gbuf_size + 1> attachments{};
  
    VkFramebufferCreateInfo framebufferInfo = {};
    framebufferInfo.sType                   = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
    framebufferInfo.renderPass              = m_mainRenderPass;
    framebufferInfo.attachmentCount         = attachments.size();
    framebufferInfo.pAttachments            = attachments.data();
    framebufferInfo.width                   = m_swapchain.GetExtent().width;
    framebufferInfo.height                  = m_swapchain.GetExtent().height;
    framebufferInfo.layers                  = 1;
  
    attachments[0] = m_resultImageBuffer.view;
    attachments[1] = m_depthBuffer.view;
    attachments[2] = m_normalBuffer.view;
    attachments[3] = m_albedoBuffer.view;
    attachments[4] = m_tangentBuffer.view;
    vkCreateFramebuffer(m_device, &framebufferInfo, nullptr, &m_mainFrameBuffer);
  }
  
  // post to screen
  {
    m_screenFrameBuffers.resize(m_swapchain.GetImageCount());
  
    std::array<VkImageView, 1> attachments{};
  
    VkFramebufferCreateInfo framebufferInfo = {};
    framebufferInfo.sType                   = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
    framebufferInfo.renderPass              = m_postRenderPass;
    framebufferInfo.attachmentCount         = attachments.size();
    framebufferInfo.pAttachments            = attachments.data();
    framebufferInfo.width                   = m_swapchain.GetExtent().width;
    framebufferInfo.height                  = m_swapchain.GetExtent().height;
    framebufferInfo.layers                  = 1;
  
    for (uint32_t i = 0; i < m_screenFrameBuffers.size(); i++) {
      attachments[0] = m_swapchain.GetAttachment(i).view;
      vkCreateFramebuffer(m_device, &framebufferInfo, nullptr, &m_screenFrameBuffers[i]);
    }
  }
}

void SimpleRender::CreateInstance()
{
  VkApplicationInfo appInfo = {};
  appInfo.sType              = VK_STRUCTURE_TYPE_APPLICATION_INFO;
  appInfo.pNext              = nullptr;
  appInfo.pApplicationName   = "VkRender";
  appInfo.applicationVersion = VK_MAKE_VERSION(0, 1, 0);
  appInfo.pEngineName        = "SimpleForward";
  appInfo.engineVersion      = VK_MAKE_VERSION(0, 1, 0);
  appInfo.apiVersion         = VK_MAKE_VERSION(1, 1, 0);

  m_instance = vk_utils::createInstance(m_enableValidation, m_validationLayers, m_instanceExtensions, &appInfo);

  if (m_enableValidation)
    vk_utils::initDebugReportCallback(m_instance, &debugReportCallbackFn, &m_debugReportCallback);
}

void SimpleRender::CreateDevice(uint32_t a_deviceId)
{
  SetupDeviceExtensions();
  m_physicalDevice = vk_utils::findPhysicalDevice(m_instance, true, a_deviceId, m_deviceExtensions);

  SetupDeviceFeatures();
  m_device = vk_utils::createLogicalDevice(m_physicalDevice, m_validationLayers, m_deviceExtensions,
                                           m_enabledDeviceFeatures, m_queueFamilyIDXs,
                                           VK_QUEUE_GRAPHICS_BIT | VK_QUEUE_TRANSFER_BIT);

  vkGetDeviceQueue(m_device, m_queueFamilyIDXs.graphics, 0, &m_graphicsQueue);
  vkGetDeviceQueue(m_device, m_queueFamilyIDXs.transfer, 0, &m_transferQueue);
}


void SimpleRender::SetupSimplePipeline()
{
  // if we are recreating pipeline (for example, to reload shaders)
  // we need to cleanup old pipeline
  if(m_fillForwardPipeline.layout != VK_NULL_HANDLE)
  {
    vkDestroyPipelineLayout(m_device, m_fillForwardPipeline.layout, nullptr);
    m_fillForwardPipeline.layout = VK_NULL_HANDLE;
  }
  if(m_fillForwardPipeline.pipeline != VK_NULL_HANDLE)
  {
    vkDestroyPipeline(m_device, m_fillForwardPipeline.pipeline, nullptr);
    m_fillForwardPipeline.pipeline = VK_NULL_HANDLE;
  }
  if(m_resolveForwardPipeline.layout != VK_NULL_HANDLE)
  {
    vkDestroyPipelineLayout(m_device, m_resolveForwardPipeline.layout, nullptr);
    m_resolveForwardPipeline.layout = VK_NULL_HANDLE;
  }
  if(m_resolveForwardPipeline.pipeline != VK_NULL_HANDLE)
  {
    vkDestroyPipeline(m_device, m_resolveForwardPipeline.pipeline, nullptr);
    m_resolveForwardPipeline.pipeline = VK_NULL_HANDLE;
  }
  if (m_computeForwardPipeline.pipeline != VK_NULL_HANDLE)
  {
    vkDestroyPipeline(m_device, m_computeForwardPipeline.pipeline, nullptr);
    m_computeForwardPipeline.pipeline = VK_NULL_HANDLE;
  }
  if (m_computeForwardPipeline.layout != VK_NULL_HANDLE)
  {
    vkDestroyPipelineLayout(m_device, m_computeForwardPipeline.layout, nullptr);
    m_computeForwardPipeline.layout = VK_NULL_HANDLE;
  }
  if(m_landscapePipeline.layout != VK_NULL_HANDLE)
  {
    vkDestroyPipelineLayout(m_device, m_landscapePipeline.layout, nullptr);
    m_landscapePipeline.layout = VK_NULL_HANDLE;
  }
  if(m_landscapePipeline.pipeline != VK_NULL_HANDLE)
  {
    vkDestroyPipeline(m_device, m_landscapePipeline.pipeline, nullptr);
    m_landscapePipeline.pipeline = VK_NULL_HANDLE;
  }
  if(m_postEPipeline.layout != VK_NULL_HANDLE)
  {
    vkDestroyPipelineLayout(m_device, m_postEPipeline.layout, nullptr);
    m_postEPipeline.layout = VK_NULL_HANDLE;
  }
  if(m_postEPipeline.pipeline != VK_NULL_HANDLE)
  {
    vkDestroyPipeline(m_device, m_postEPipeline.pipeline, nullptr);
    m_postEPipeline.pipeline = VK_NULL_HANDLE;
  }
  
  if(m_pBindings == nullptr) {
      std::vector<std::pair<VkDescriptorType, uint32_t> > dtypes = {
              {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 4},
              {VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 7},
              {VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1},
              {VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, 5}
      };
      m_pBindings = std::make_shared<vk_utils::DescriptorMaker>(m_device, dtypes, 5);
  }
  
  if (m_pScnMgr->InstancesNum() > 0) {
    m_pBindings->BindBegin(VK_SHADER_STAGE_COMPUTE_BIT);
    m_pBindings->BindBuffer(0, iicommand_buffer);
    m_pBindings->BindBuffer(1, m_pScnMgr->GetMeshInfoBuffer());
    m_pBindings->BindBuffer(2, m_pScnMgr->GetInstanceInfoBuffer());
    m_pBindings->BindBuffer(3, drawAtomic_buffer);
    m_pBindings->BindBuffer(4, drawMatrices_buffer);
    m_pBindings->BindEnd(&m_comp_dSet, &m_comp_dSetLayout);
  }
  
  {
    m_pBindings->BindBegin(VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT | VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT |
                           VK_SHADER_STAGE_FRAGMENT_BIT);
    m_pBindings->BindBuffer(0, m_ubo, VK_NULL_HANDLE, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER);
    m_pBindings->BindBuffer(1, m_pScnMgr->GetLandInfoBuffer(), VK_NULL_HANDLE, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER);
    m_pBindings->BindImage(2, m_pScnMgr->GetHeightMap().view, m_simple_image_sampler,
                           VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);
    m_pBindings->BindEnd(&m_landscape_dSet, &m_landscape_dSetLayout);
  }
  
  if (m_pScnMgr->InstancesNum() > 0) {
    m_pBindings->BindBegin(VK_SHADER_STAGE_FRAGMENT_BIT | VK_SHADER_STAGE_VERTEX_BIT);
    m_pBindings->BindBuffer(0, m_ubo, VK_NULL_HANDLE, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER);
    m_pBindings->BindBuffer(1, drawMatrices_buffer);
    m_pBindings->BindEnd(&m_fill_dSet, &m_fill_dSetLayout);
  }
  
  {
    m_pBindings->BindBegin(VK_SHADER_STAGE_FRAGMENT_BIT);
    m_pBindings->BindImage(0, m_depthBuffer.view, VK_NULL_HANDLE, VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT);
    m_pBindings->BindImage(1, m_normalBuffer.view, VK_NULL_HANDLE, VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT);
    m_pBindings->BindImage(2, m_albedoBuffer.view, VK_NULL_HANDLE, VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT);
    m_pBindings->BindImage(3, m_tangentBuffer.view, VK_NULL_HANDLE, VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT);
    m_pBindings->BindEnd(&m_resolve_dSet, &m_resolve_dSetLayout);
  }
  
  {
    m_pBindings->BindBegin(VK_SHADER_STAGE_FRAGMENT_BIT | VK_SHADER_STAGE_GEOMETRY_BIT);
    m_pBindings->BindBuffer(0, m_ubo, VK_NULL_HANDLE, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER);
    m_pBindings->BindBuffer(1, m_pScnMgr->GetLightInfoBuffer());
    m_pBindings->BindEnd(&m_light_dSet, &m_light_dSetLayout);
  }
  
  {
    m_pBindings->BindBegin(VK_SHADER_STAGE_FRAGMENT_BIT);
    //m_pBindings->BindBuffer(0, m_ubo, VK_NULL_HANDLE, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER);
    m_pBindings->BindImage(0, m_resultImageBuffer.view, m_simple_image_sampler, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);
    m_pBindings->BindEnd(&m_postE_dSet, &m_postE_dSetLayout);
  }
  
  {
    vk_utils::GraphicsPipelineMaker maker;
    
    std::unordered_map<VkShaderStageFlagBits, std::string> shader_paths;
    shader_paths[VK_SHADER_STAGE_FRAGMENT_BIT] = FRAGMENT_SHADER_PATH_LAND + ".spv";
    shader_paths[VK_SHADER_STAGE_VERTEX_BIT] = VERTEX_SHADER_PATH_LAND + ".spv";
    shader_paths[VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT] = TESSELATION_CONTROL_SHADER_PATH_LAND + ".spv";
    shader_paths[VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT] = TESSELATION_EVALUATION_SHADER_PATH_LAND + ".spv";
    maker.LoadShaders(m_device, shader_paths);
    
    m_landscapePipeline.layout = maker.MakeLayout(m_device, {m_landscape_dSetLayout}, sizeof(resolveConst));
    
    maker.SetDefaultState(m_width, m_height);
    std::array<VkPipelineColorBlendAttachmentState, m_gbuf_size - 1> blendStates{};
    VkPipelineColorBlendAttachmentState defaultState{};
    defaultState.blendEnable = VK_FALSE;
    defaultState.colorWriteMask =
            VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
    blendStates.fill(defaultState);
    
    maker.colorBlending.attachmentCount = blendStates.size();
    maker.colorBlending.pAttachments = blendStates.data();
    
    std::array dynStates{VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR};
    
    VkPipelineDynamicStateCreateInfo dynamicState = {};
    dynamicState.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
    dynamicState.dynamicStateCount = (uint32_t) dynStates.size();
    dynamicState.pDynamicStates = dynStates.data();
    
    VkPipelineVertexInputStateCreateInfo vertexLayout = {};
    vertexLayout.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    vertexLayout.vertexBindingDescriptionCount = 0;
    vertexLayout.vertexAttributeDescriptionCount = 0;
    
    VkPipelineInputAssemblyStateCreateInfo inputAssembly = {};
    inputAssembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_PATCH_LIST;
    inputAssembly.primitiveRestartEnable = false;
    
    VkPipelineTessellationStateCreateInfo tessState = {};
    tessState.sType = VK_STRUCTURE_TYPE_PIPELINE_TESSELLATION_STATE_CREATE_INFO;
    tessState.patchControlPoints = 4;
    
    VkGraphicsPipelineCreateInfo pipelineInfo = {};
    pipelineInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    pipelineInfo.flags = 0;
    pipelineInfo.stageCount = static_cast<uint32_t>(shader_paths.size());
    pipelineInfo.pStages = maker.shaderStageInfos;
    pipelineInfo.pVertexInputState = &vertexLayout;
    pipelineInfo.pInputAssemblyState = &inputAssembly;
    pipelineInfo.pViewportState = &maker.viewportState;
    pipelineInfo.pRasterizationState = &maker.rasterizer;
    pipelineInfo.pMultisampleState = &maker.multisampling;
    pipelineInfo.pColorBlendState = &maker.colorBlending;
    pipelineInfo.pTessellationState = &tessState;
    pipelineInfo.layout = m_landscapePipeline.layout;
    pipelineInfo.renderPass = m_mainRenderPass;
    pipelineInfo.subpass = 0;
    pipelineInfo.pDynamicState = &dynamicState;
    pipelineInfo.basePipelineHandle = VK_NULL_HANDLE;
    pipelineInfo.pDepthStencilState = &maker.depthStencilTest;
    
    VK_CHECK_RESULT(vkCreateGraphicsPipelines(m_device, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr,
                                              &m_landscapePipeline.pipeline));
    
    for (size_t i = 0; i < shader_paths.size(); ++i) {
      if (maker.shaderModules[i] != VK_NULL_HANDLE)
        vkDestroyShaderModule(m_device, maker.shaderModules[i], VK_NULL_HANDLE);
      maker.shaderModules[i] = VK_NULL_HANDLE;
    }
  }
  
  if (m_pScnMgr->InstancesNum() > 0) {
    vk_utils::GraphicsPipelineMaker maker;
  
    std::unordered_map<VkShaderStageFlagBits, std::string> shader_paths;
    shader_paths[VK_SHADER_STAGE_FRAGMENT_BIT] = FRAGMENT_SHADER_PATH_FILL + ".spv";
    if (make_geom) {
      shader_paths[VK_SHADER_STAGE_GEOMETRY_BIT] = GEOMETRY_SHADER_PATH + ".spv";
    }
    shader_paths[VK_SHADER_STAGE_VERTEX_BIT] = VERTEX_SHADER_PATH_FILL + ".spv";
    maker.LoadShaders(m_device, shader_paths);
    
    m_fillForwardPipeline.layout = maker.MakeLayout(m_device, {m_fill_dSetLayout}, sizeof(resolveConst));
    maker.SetDefaultState(m_width, m_height);


    std::array<VkPipelineColorBlendAttachmentState, m_gbuf_size - 1> blendStates{};
    VkPipelineColorBlendAttachmentState defaultState{};
    defaultState.blendEnable = VK_FALSE;
    defaultState.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
    blendStates.fill(defaultState);
    
    maker.colorBlending.attachmentCount = blendStates.size();
    maker.colorBlending.pAttachments = blendStates.data();
    
    m_fillForwardPipeline.pipeline = maker.MakePipeline(m_device, m_pScnMgr->GetPipelineVertexInputStateCreateInfo(),
                                                        m_mainRenderPass,
                                                        {VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR});
  }
  
  {
    vk_utils::GraphicsPipelineMaker maker;
  
    std::unordered_map<VkShaderStageFlagBits, std::string> shader_paths;
    shader_paths[VK_SHADER_STAGE_FRAGMENT_BIT] = FRAGMENT_SHADER_PATH_RESOLVE + ".spv";
    shader_paths[VK_SHADER_STAGE_GEOMETRY_BIT] = GEOMETRY_SHADER_PATH_RESOLVE + ".spv";
    shader_paths[VK_SHADER_STAGE_VERTEX_BIT] = VERTEX_SHADER_PATH_RESOLVE + ".spv";
    maker.LoadShaders(m_device, shader_paths);
    
    m_resolveForwardPipeline.layout = maker.MakeLayout(m_device, {m_light_dSetLayout, m_resolve_dSetLayout},
                                                       sizeof(resolveConst));
    maker.SetDefaultState(m_width, m_height);
    maker.depthStencilTest.depthTestEnable = false;
    maker.colorBlendAttachments = {VkPipelineColorBlendAttachmentState{.blendEnable = VK_TRUE,
            .srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA,
            .dstColorBlendFactor = VK_BLEND_FACTOR_ONE,
            .colorBlendOp = VK_BLEND_OP_ADD,
            .colorWriteMask = VK_COLOR_COMPONENT_R_BIT |
                              VK_COLOR_COMPONENT_G_BIT |
                              VK_COLOR_COMPONENT_B_BIT}};
  
    VkPipelineVertexInputStateCreateInfo vertexLayout = {};
    vertexLayout.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    vertexLayout.vertexBindingDescriptionCount = 0;
    vertexLayout.vertexAttributeDescriptionCount = 0;
    
    m_resolveForwardPipeline.pipeline = maker.MakePipeline(m_device, vertexLayout,
                                                           m_mainRenderPass,
                                                           {VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR},
                                                           vk_utils::IA_PList(), 1);
  }
  
  if (m_pScnMgr->InstancesNum() > 0) {
    vk_utils::ComputePipelineMaker comp_maker;
    comp_maker.LoadShader(m_device, COMPUTE_SHADER_PATH + ".spv");
    
    m_computeForwardPipeline.layout = comp_maker.MakeLayout(m_device, {m_comp_dSetLayout}, sizeof(comp_pushConst));
    m_computeForwardPipeline.pipeline = comp_maker.MakePipeline(m_device);
  }
  
  {
    vk_utils::GraphicsPipelineMaker maker;
  
    std::unordered_map<VkShaderStageFlagBits, std::string> shader_paths;
    shader_paths[VK_SHADER_STAGE_FRAGMENT_BIT] = FRAGMENT_SHADER_PATH_POSTE + ".spv";
    shader_paths[VK_SHADER_STAGE_VERTEX_BIT] = VERTEX_SHADER_PATH_POSTE + ".spv";
    maker.LoadShaders(m_device, shader_paths);
  
    m_postEPipeline.layout = maker.MakeLayout(m_device, {m_postE_dSetLayout}, sizeof(resolveConst));
  
    maker.SetDefaultState(m_width, m_height);
    maker.depthStencilTest.depthTestEnable = false;
//    maker.colorBlendAttachments = {VkPipelineColorBlendAttachmentState{.blendEnable = VK_TRUE,
//            .srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA,
//            .dstColorBlendFactor = VK_BLEND_FACTOR_ONE,
//            .colorBlendOp = VK_BLEND_OP_ADD,
//            .colorWriteMask = VK_COLOR_COMPONENT_R_BIT |
//                              VK_COLOR_COMPONENT_G_BIT |
//                              VK_COLOR_COMPONENT_B_BIT}};
  
    VkPipelineVertexInputStateCreateInfo vertexLayout = {};
    vertexLayout.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    vertexLayout.vertexBindingDescriptionCount = 0;
    vertexLayout.vertexAttributeDescriptionCount = 0;
  
    m_postEPipeline.pipeline = maker.MakePipeline(m_device, vertexLayout,
                                                  m_postRenderPass,
                                                  {VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR});
  }
}

void SimpleRender::CreateBuffers()
{
  if (m_pScnMgr->InstancesNum() > 0) {
    iicommand_buffer = vk_utils::createBuffer(m_device,
                                              m_pScnMgr->MeshesNum() * sizeof(VkDrawIndexedIndirectCommand),
                                              VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT);
    drawAtomic_buffer = vk_utils::createBuffer(m_device,
                                               sizeof(uint32_t),
                                               VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT);
    drawMatrices_buffer = vk_utils::createBuffer(m_device,
                                                 m_pScnMgr->InstancesNum() * sizeof(LiteMath::float4x4),
                                                 VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT);
    m_buffers_memory = vk_utils::allocateAndBindWithPadding(m_device, m_physicalDevice,
                                                            {iicommand_buffer, drawAtomic_buffer, drawMatrices_buffer},
                                                            0);
  }

  VkMemoryRequirements memReq;
  m_ubo = vk_utils::createBuffer(m_device, sizeof(m_uniforms), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, &memReq);

  VkMemoryAllocateInfo allocateInfo = {};
  allocateInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
  allocateInfo.pNext = nullptr;
  allocateInfo.allocationSize = memReq.size;
  allocateInfo.memoryTypeIndex = vk_utils::findMemoryType(memReq.memoryTypeBits,
                                                          VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                                                          VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                                                          m_physicalDevice);
  VK_CHECK_RESULT(vkAllocateMemory(m_device, &allocateInfo, nullptr, &m_uboAlloc))

  VK_CHECK_RESULT(vkBindBufferMemory(m_device, m_ubo, m_uboAlloc, 0));

  vkMapMemory(m_device, m_uboAlloc, 0, sizeof(m_uniforms), 0, &m_uboMappedMem);

  //m_uniforms.lightPos = LiteMath::float3(0.0f, 1.0f, 1.0f);
  m_uniforms.baseColor = LiteMath::float3(0.9f, 0.92f, 1.0f);
  m_uniforms.baseColor = LiteMath::float3(0.9f, 0.92f, 1.0f);
  m_uniforms.animateLightColor = true;

  UpdateUniformBuffer(0.0f);
}

void SimpleRender::UpdateUniformBuffer(float a_time)
{
// most uniforms are updated in GUI -> SetupGUIElements()
  m_uniforms.time = a_time;
  m_uniforms.screenHeight = m_height;
  m_uniforms.screenWidth = m_width;
  memcpy(m_uboMappedMem, &m_uniforms, sizeof(m_uniforms));
}

void SimpleRender::BuildCommandBufferSimple(VkCommandBuffer a_cmdBuff, VkFramebuffer a_frameBuff)
{
  vkResetCommandBuffer(a_cmdBuff, 0);

  VkCommandBufferBeginInfo beginInfo = {};
  beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
  beginInfo.flags = VK_COMMAND_BUFFER_USAGE_SIMULTANEOUS_USE_BIT;

  VK_CHECK_RESULT(vkBeginCommandBuffer(a_cmdBuff, &beginInfo));
  
  if (m_pScnMgr->InstancesNum() > 0) {
    vkCmdFillBuffer(a_cmdBuff, drawAtomic_buffer, 0, sizeof(uint), 0);
    
    VkBufferMemoryBarrier barrier_atomic = {};
    barrier_atomic.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER;
    barrier_atomic.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    barrier_atomic.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
    barrier_atomic.buffer = drawAtomic_buffer;
    barrier_atomic.offset = 0;
    barrier_atomic.size = VK_WHOLE_SIZE;
    
    vkCmdPipelineBarrier(a_cmdBuff, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                         {}, 0, nullptr, 1, &barrier_atomic, 0, nullptr);
    
    vkCmdBindPipeline(a_cmdBuff, VK_PIPELINE_BIND_POINT_COMPUTE, m_computeForwardPipeline.pipeline);
    vkCmdBindDescriptorSets(a_cmdBuff, VK_PIPELINE_BIND_POINT_COMPUTE, m_computeForwardPipeline.layout, 0, 1,
                            &m_comp_dSet, 0, VK_NULL_HANDLE);
    comp_pushConst.instance_count = m_pScnMgr->InstancesNum();
    vkCmdPushConstants(a_cmdBuff, m_computeForwardPipeline.layout, VK_SHADER_STAGE_COMPUTE_BIT, 0,
                       sizeof(comp_pushConst),
                       &comp_pushConst);
    vkCmdDispatch(a_cmdBuff, m_pScnMgr->MeshesNum(), 1, 1);
    
    VkBufferMemoryBarrier barrier_command = {};
    barrier_command.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER;
    barrier_command.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
    barrier_command.dstAccessMask = VK_ACCESS_INDIRECT_COMMAND_READ_BIT;
    barrier_command.buffer = iicommand_buffer;
    barrier_command.offset = 0;
    barrier_command.size = VK_WHOLE_SIZE;
    
    VkBufferMemoryBarrier barrier_matrix = {};
    barrier_matrix.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER;
    barrier_matrix.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
    barrier_matrix.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
    barrier_matrix.buffer = drawMatrices_buffer;
    barrier_matrix.offset = 0;
    barrier_matrix.size = VK_WHOLE_SIZE;
    
    std::array barriers = {barrier_command, barrier_matrix};
    
    vkCmdPipelineBarrier(a_cmdBuff, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                         VK_PIPELINE_STAGE_DRAW_INDIRECT_BIT | VK_PIPELINE_STAGE_VERTEX_SHADER_BIT,
                         {}, 0, nullptr, barriers.size(), barriers.data(), 0, nullptr);
  }

  vk_utils::setDefaultViewport(a_cmdBuff, static_cast<float>(m_width), static_cast<float>(m_height));
  vk_utils::setDefaultScissor(a_cmdBuff, m_width, m_height);

  ///// draw main pass
  {
    VkRenderPassBeginInfo renderPassInfo = {};
    renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    renderPassInfo.renderPass = m_mainRenderPass;
    renderPassInfo.framebuffer = m_mainFrameBuffer;//a_frameBuff;
    renderPassInfo.renderArea.offset = {0, 0};
    renderPassInfo.renderArea.extent = m_swapchain.GetExtent();

    std::array clearValues {
       VkClearValue { .color = {{0.0f, 0.0f, 0.0f, 1.0f}} },
       VkClearValue { .depthStencil = {1.0f, 0} },
       VkClearValue { .color = {{0.0f, 0.0f, 0.0f, 1.0f}} },
       VkClearValue { .color = {{0.0f, 0.0f, 0.0f, 1.0f}} },
       VkClearValue { .color = {{0.0f, 0.0f, 0.0f, 1.0f}} },
    };
    renderPassInfo.clearValueCount = clearValues.size();
    renderPassInfo.pClearValues = clearValues.data();

    vkCmdBeginRenderPass(a_cmdBuff, &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);
  
    vkCmdBindPipeline(a_cmdBuff, VK_PIPELINE_BIND_POINT_GRAPHICS, m_landscapePipeline.pipeline);
    vkCmdBindDescriptorSets(a_cmdBuff, VK_PIPELINE_BIND_POINT_GRAPHICS, m_landscapePipeline.layout, 0, 1,
                            &m_landscape_dSet, 0, VK_NULL_HANDLE);
    
    VkShaderStageFlags stageFlags = (VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT |
            VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT | VK_SHADER_STAGE_FRAGMENT_BIT);
    vkCmdPushConstants(a_cmdBuff, m_landscapePipeline.layout, stageFlags, 0, sizeof(resolveConst), &resolveConst);
  
    vkCmdDraw(a_cmdBuff, 4, 1, 0, 0);
  
    if (m_pScnMgr->InstancesNum() > 0) {
      vkCmdBindPipeline(a_cmdBuff, VK_PIPELINE_BIND_POINT_GRAPHICS, m_fillForwardPipeline.pipeline);
      vkCmdBindDescriptorSets(a_cmdBuff, VK_PIPELINE_BIND_POINT_GRAPHICS, m_fillForwardPipeline.layout, 0, 1,
                              &m_fill_dSet, 0, VK_NULL_HANDLE);
    
      stageFlags = (VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT);
      if (make_geom)
        stageFlags = (stageFlags | VK_SHADER_STAGE_GEOMETRY_BIT);
    
      VkDeviceSize zero_offset = 0u;
      VkBuffer vertexBuf = m_pScnMgr->GetVertexBuffer();
      VkBuffer indexBuf = m_pScnMgr->GetIndexBuffer();
    
      vkCmdBindVertexBuffers(a_cmdBuff, 0, 1, &vertexBuf, &zero_offset);
      vkCmdBindIndexBuffer(a_cmdBuff, indexBuf, 0, VK_INDEX_TYPE_UINT32);
    
      vkCmdPushConstants(a_cmdBuff, m_fillForwardPipeline.layout, stageFlags, 0,
                         sizeof(resolveConst), &resolveConst);
    
      vkCmdDrawIndexedIndirect(a_cmdBuff, iicommand_buffer, 0,
                               m_pScnMgr->MeshesNum(),
                               sizeof(VkDrawIndexedIndirectCommand));
    }
    vkCmdNextSubpass(a_cmdBuff, VK_SUBPASS_CONTENTS_INLINE);

    vkCmdBindPipeline(a_cmdBuff, VK_PIPELINE_BIND_POINT_GRAPHICS, m_resolveForwardPipeline.pipeline);

    std::array<VkDescriptorSet, 2> dsets = {m_light_dSet, m_resolve_dSet};
    vkCmdBindDescriptorSets(a_cmdBuff, VK_PIPELINE_BIND_POINT_GRAPHICS, m_resolveForwardPipeline.layout, 0, dsets.size(),
                            dsets.data(), 0, VK_NULL_HANDLE);

    stageFlags = (VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_GEOMETRY_BIT | VK_SHADER_STAGE_FRAGMENT_BIT);

    vkCmdPushConstants(a_cmdBuff, m_resolveForwardPipeline.layout, stageFlags, 0,
                       sizeof(resolveConst), &resolveConst);

    vkCmdDraw(a_cmdBuff, 1, m_pScnMgr->LightsNum(), 0, 0);

    vkCmdEndRenderPass(a_cmdBuff);
  }
  
  VkImageMemoryBarrier image_barrier = {};
  image_barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
  image_barrier.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
  image_barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
  image_barrier.image = m_resultImageBuffer.image;
  image_barrier.oldLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
  image_barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
  image_barrier.subresourceRange = VkImageSubresourceRange({.aspectMask = m_resultImageBuffer.aspectMask,
                                                            .baseMipLevel = 0,
                                                            .levelCount = m_resultImageBuffer.mipLvls,
                                                            .baseArrayLayer = 0,
                                                            .layerCount = 1});
  
  vkCmdPipelineBarrier(a_cmdBuff, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
                       {}, 0, nullptr, 0, nullptr, 1, &image_barrier);
  
  //// draw post effects pass
  {
    VkRenderPassBeginInfo renderPassInfo = {};
    renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    renderPassInfo.renderPass = m_postRenderPass;
    renderPassInfo.framebuffer = a_frameBuff;
    renderPassInfo.renderArea.offset = {0, 0};
    renderPassInfo.renderArea.extent = m_swapchain.GetExtent();
  
    std::array clearValues {
            VkClearValue { .color = {{0.0f, 0.0f, 0.0f, 1.0f}} },
    };
    renderPassInfo.clearValueCount = clearValues.size();
    renderPassInfo.pClearValues = clearValues.data();
  
    vkCmdBeginRenderPass(a_cmdBuff, &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);
  
    vkCmdBindPipeline(a_cmdBuff, VK_PIPELINE_BIND_POINT_GRAPHICS, m_postEPipeline.pipeline);
    vkCmdBindDescriptorSets(a_cmdBuff, VK_PIPELINE_BIND_POINT_GRAPHICS, m_postEPipeline.layout, 0, 1,
                            &m_postE_dSet, 0, VK_NULL_HANDLE);
  
    VkShaderStageFlags stageFlags = (VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT);
    vkCmdPushConstants(a_cmdBuff, m_postEPipeline.layout, stageFlags, 0, sizeof(resolveConst), &resolveConst);
  
    vkCmdDraw(a_cmdBuff, 4, 1, 0, 0);
    
    vkCmdEndRenderPass(a_cmdBuff);
  }

  VK_CHECK_RESULT(vkEndCommandBuffer(a_cmdBuff));
}


void SimpleRender::CleanupPipelineAndSwapchain()
{
  vkDestroyBuffer(m_device, iicommand_buffer, nullptr);
  vkDestroyBuffer(m_device, drawAtomic_buffer, nullptr);
  vkDestroyBuffer(m_device, drawMatrices_buffer, nullptr);

  vkFreeMemory(m_device, m_buffers_memory, nullptr);

  if (!m_cmdBuffersDrawMain.empty())
  {
    vkFreeCommandBuffers(m_device, m_commandPool, static_cast<uint32_t>(m_cmdBuffersDrawMain.size()),
                         m_cmdBuffersDrawMain.data());
    m_cmdBuffersDrawMain.clear();
  }

  for (size_t i = 0; i < m_frameFences.size(); i++)
  {
    vkDestroyFence(m_device, m_frameFences[i], nullptr);
  }
  m_frameFences.clear();

  vk_utils::deleteImg(m_device, &m_depthBuffer);
  if(m_depthBuffer.mem != VK_NULL_HANDLE)
  {
    vkFreeMemory(m_device, m_depthBuffer.mem, nullptr);
    m_depthBuffer.mem = VK_NULL_HANDLE;
  }

  vk_utils::deleteImg(m_device, &m_normalBuffer);
  if(m_normalBuffer.mem != VK_NULL_HANDLE)
  {
    vkFreeMemory(m_device, m_normalBuffer.mem, nullptr);
    m_normalBuffer.mem = VK_NULL_HANDLE;
  }

  vk_utils::deleteImg(m_device, &m_albedoBuffer);
  if(m_albedoBuffer.mem != VK_NULL_HANDLE)
  {
    vkFreeMemory(m_device, m_albedoBuffer.mem, nullptr);
    m_albedoBuffer.mem = VK_NULL_HANDLE;
  }

  vk_utils::deleteImg(m_device, &m_tangentBuffer);
  if(m_tangentBuffer.mem != VK_NULL_HANDLE)
  {
    vkFreeMemory(m_device, m_tangentBuffer.mem, nullptr);
    m_tangentBuffer.mem = VK_NULL_HANDLE;
  }
  
  vk_utils::deleteImg(m_device, &m_resultImageBuffer);
  if(m_resultImageBuffer.mem != VK_NULL_HANDLE)
  {
    vkFreeMemory(m_device, m_resultImageBuffer.mem, nullptr);
    m_resultImageBuffer.mem = VK_NULL_HANDLE;
  }

  for (size_t i = 0; i < m_screenFrameBuffers.size(); i++)
  {
    vkDestroyFramebuffer(m_device, m_screenFrameBuffers[i], nullptr);
  }
  m_screenFrameBuffers.clear();
  
  vkDestroyFramebuffer(m_device, m_mainFrameBuffer, nullptr);
  
  if(m_mainRenderPass != VK_NULL_HANDLE)
  {
    vkDestroyRenderPass(m_device, m_mainRenderPass, nullptr);
    m_mainRenderPass = VK_NULL_HANDLE;
  }
  
  if(m_postRenderPass != VK_NULL_HANDLE)
  {
    vkDestroyRenderPass(m_device, m_postRenderPass, nullptr);
    m_postRenderPass = VK_NULL_HANDLE;
  }
  
  m_swapchain.Cleanup();
}

void SimpleRender::RecreateSwapChain()
{
  vkDeviceWaitIdle(m_device);

  CleanupPipelineAndSwapchain();
  auto oldImagesNum = m_swapchain.GetImageCount();
  m_presentationResources.queue = m_swapchain.CreateSwapChain(m_physicalDevice, m_device, m_surface, m_width, m_height,
    oldImagesNum, m_vsync);

  std::vector<VkFormat> depthFormats = {
    VK_FORMAT_D32_SFLOAT,
    VK_FORMAT_D32_SFLOAT_S8_UINT,
    VK_FORMAT_D24_UNORM_S8_UINT,
    VK_FORMAT_D16_UNORM_S8_UINT,
    VK_FORMAT_D16_UNORM
  };
  vk_utils::getSupportedDepthFormat(m_physicalDevice, depthFormats, &m_depthBuffer.format);
  m_depthBuffer.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
  m_normalBuffer.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
  m_albedoBuffer.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
  m_tangentBuffer.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
  m_resultImageBuffer.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
  vk_utils::createImgAllocAndBind(m_device, m_physicalDevice, m_width, m_height,
                                  m_depthBuffer.format,
                                  VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT, &m_depthBuffer);
  vk_utils::createImgAllocAndBind(m_device, m_physicalDevice, m_width, m_height,
                                  VK_FORMAT_R16G16B16A16_SFLOAT,
                                  VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT, &m_normalBuffer);
  vk_utils::createImgAllocAndBind(m_device, m_physicalDevice, m_width, m_height,
                                  VK_FORMAT_R8G8B8A8_UNORM,
                                  VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT, &m_albedoBuffer);
  vk_utils::createImgAllocAndBind(m_device, m_physicalDevice, m_width, m_height,
                                  VK_FORMAT_R16G16B16A16_SFLOAT,
                                  VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT, &m_tangentBuffer);
  vk_utils::createImgAllocAndBind(m_device, m_physicalDevice, m_width, m_height,
                                  VK_FORMAT_R16G16B16A16_SFLOAT,
                                  VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT, &m_resultImageBuffer);
  CreateRenderpass();
  CreateFrameBuffer();

  m_frameFences.resize(m_framesInFlight);
  VkFenceCreateInfo fenceInfo = {};
  fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
  fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;
  for (size_t i = 0; i < m_framesInFlight; i++)
  {
    VK_CHECK_RESULT(vkCreateFence(m_device, &fenceInfo, nullptr, &m_frameFences[i]));
  }

  m_cmdBuffersDrawMain = vk_utils::createCommandBuffers(m_device, m_commandPool, m_framesInFlight);
  for (uint32_t i = 0; i < m_swapchain.GetImageCount(); ++i)
  {
    BuildCommandBufferSimple(m_cmdBuffersDrawMain[i], m_screenFrameBuffers[i]);
  }

  m_pGUIRender->OnSwapchainChanged(m_swapchain);
}

void SimpleRender::Cleanup()
{
  if (m_pGUIRender) {
    m_pGUIRender = nullptr;
    ImGui::DestroyContext();
  }
  CleanupPipelineAndSwapchain();
  if(m_surface != VK_NULL_HANDLE)
  {
    vkDestroySurfaceKHR(m_instance, m_surface, nullptr);
    m_surface = VK_NULL_HANDLE;
  }

  if(m_fillForwardPipeline.layout != VK_NULL_HANDLE)
  {
    vkDestroyPipelineLayout(m_device, m_fillForwardPipeline.layout, nullptr);
    m_fillForwardPipeline.layout = VK_NULL_HANDLE;
  }
  if(m_fillForwardPipeline.pipeline != VK_NULL_HANDLE)
  {
    vkDestroyPipeline(m_device, m_fillForwardPipeline.pipeline, nullptr);
    m_fillForwardPipeline.pipeline = VK_NULL_HANDLE;
  }
  if(m_resolveForwardPipeline.layout != VK_NULL_HANDLE)
  {
    vkDestroyPipelineLayout(m_device, m_resolveForwardPipeline.layout, nullptr);
    m_resolveForwardPipeline.layout = VK_NULL_HANDLE;
  }
  if(m_resolveForwardPipeline.pipeline != VK_NULL_HANDLE)
  {
    vkDestroyPipeline(m_device, m_resolveForwardPipeline.pipeline, nullptr);
    m_resolveForwardPipeline.pipeline = VK_NULL_HANDLE;
  }
  if (m_computeForwardPipeline.pipeline != VK_NULL_HANDLE)
  {
    vkDestroyPipeline(m_device, m_computeForwardPipeline.pipeline, nullptr);
    m_computeForwardPipeline.pipeline = VK_NULL_HANDLE;
  }
  if (m_computeForwardPipeline.layout != VK_NULL_HANDLE)
  {
    vkDestroyPipelineLayout(m_device, m_computeForwardPipeline.layout, nullptr);
    m_computeForwardPipeline.layout = VK_NULL_HANDLE;
  }
  
  if (m_simple_image_sampler != VK_NULL_HANDLE)
  {
    vkDestroySampler(m_device, m_simple_image_sampler, nullptr);
    m_simple_image_sampler = VK_NULL_HANDLE;
  }
  
  if(m_landscapePipeline.layout != VK_NULL_HANDLE)
  {
    vkDestroyPipelineLayout(m_device, m_landscapePipeline.layout, nullptr);
    m_landscapePipeline.layout = VK_NULL_HANDLE;
  }
  if(m_landscapePipeline.pipeline != VK_NULL_HANDLE)
  {
    vkDestroyPipeline(m_device, m_landscapePipeline.pipeline, nullptr);
    m_landscapePipeline.pipeline = VK_NULL_HANDLE;
  }
  
  if(m_postEPipeline.layout != VK_NULL_HANDLE)
  {
    vkDestroyPipelineLayout(m_device, m_postEPipeline.layout, nullptr);
    m_postEPipeline.layout = VK_NULL_HANDLE;
  }
  if(m_postEPipeline.pipeline != VK_NULL_HANDLE)
  {
    vkDestroyPipeline(m_device, m_postEPipeline.pipeline, nullptr);
    m_postEPipeline.pipeline = VK_NULL_HANDLE;
  }
  
  if (m_presentationResources.imageAvailable != VK_NULL_HANDLE)
  {
    vkDestroySemaphore(m_device, m_presentationResources.imageAvailable, nullptr);
    m_presentationResources.imageAvailable = VK_NULL_HANDLE;
  }
  if (m_presentationResources.renderingFinished != VK_NULL_HANDLE)
  {
    vkDestroySemaphore(m_device, m_presentationResources.renderingFinished, nullptr);
    m_presentationResources.renderingFinished = VK_NULL_HANDLE;
  }

  if (m_commandPool != VK_NULL_HANDLE)
  {
    vkDestroyCommandPool(m_device, m_commandPool, nullptr);
    m_commandPool = VK_NULL_HANDLE;
  }

  if(m_ubo != VK_NULL_HANDLE)
  {
    vkDestroyBuffer(m_device, m_ubo, nullptr);
    m_ubo = VK_NULL_HANDLE;
  }

  if(m_uboAlloc != VK_NULL_HANDLE)
  {
    vkFreeMemory(m_device, m_uboAlloc, nullptr);
    m_uboAlloc = VK_NULL_HANDLE;
  }

  m_pBindings = nullptr;
  m_pScnMgr   = nullptr;

  if(m_device != VK_NULL_HANDLE)
  {
    vkDestroyDevice(m_device, nullptr);
    m_device = VK_NULL_HANDLE;
  }

  if(m_debugReportCallback != VK_NULL_HANDLE)
  {
    vkDestroyDebugReportCallbackEXT(m_instance, m_debugReportCallback, nullptr);
    m_debugReportCallback = VK_NULL_HANDLE;
  }

  if(m_instance != VK_NULL_HANDLE)
  {
    vkDestroyInstance(m_instance, nullptr);
    m_instance = VK_NULL_HANDLE;
  }
}

void SimpleRender::ProcessInput(const AppInput &input)
{
  // add keyboard controls here
  // camera movement is processed separately

  // recreate pipeline to reload shaders
  if(input.keyPressed[GLFW_KEY_B])
  {
#ifdef WIN32
    std::system("cd ../resources/shaders && python compile_simple_render_shaders.py");
#else
    std::system("cd ../resources/shaders && python3 compile_landscape_shaders.py");
#endif

    SetupSimplePipeline();

    for (uint32_t i = 0; i < m_framesInFlight; ++i)
    {
      BuildCommandBufferSimple(m_cmdBuffersDrawMain[i], m_screenFrameBuffers[i]);
    }
  }

  if(input.keyPressed[GLFW_KEY_H])
  {
    make_geom = !make_geom;
    SetupSimplePipeline();
  }
}

void SimpleRender::UpdateCamera(const Camera* cams, uint32_t a_camsCount)
{
  assert(a_camsCount > 0);
  m_cam = cams[0];
  UpdateView();
}

void SimpleRender::UpdateView()
{
  const float aspect   = float(m_width) / float(m_height);
  auto mProjFix        = OpenglToVulkanProjectionMatrixFix();
  auto mProj           = projectionMatrix(m_cam.fov, aspect, 0.1f, 1000.0f);
  auto mLookAt         = LiteMath::lookAt(m_cam.pos, m_cam.lookAt, m_cam.up);
  auto mWorldViewProj  = mProjFix * mProj * mLookAt;
  comp_pushConst.projView = mWorldViewProj;
  resolveConst.Proj = mProjFix * mProj;
  resolveConst.View = mLookAt;
}

void SimpleRender::LoadScene(const char* path, bool transpose_inst_matrices)
{
  m_pScnMgr->LoadSceneXML(path, transpose_inst_matrices);
  m_pScnMgr->CleanScene();
  m_pScnMgr->GenerateLandscapeTex(m_landscape_width, m_landscape_height);

  CreateBuffers();
  SetupSimplePipeline();

  auto loadedCam = m_pScnMgr->GetCamera(0);
  m_cam.fov = loadedCam.fov;
  m_cam.pos = float3(loadedCam.pos);
  m_cam.up  = float3(loadedCam.up);
  m_cam.lookAt = float3(loadedCam.lookAt);
  m_cam.tdist  = loadedCam.farPlane;

  UpdateView();

  for (uint32_t i = 0; i < m_framesInFlight; ++i)
  {
    BuildCommandBufferSimple(m_cmdBuffersDrawMain[i], m_screenFrameBuffers[i]);
  }
}

void SimpleRender::DrawFrameSimple()
{
  vkWaitForFences(m_device, 1, &m_frameFences[m_presentationResources.currentFrame], VK_TRUE, UINT64_MAX);
  vkResetFences(m_device, 1, &m_frameFences[m_presentationResources.currentFrame]);

  uint32_t imageIdx;
  m_swapchain.AcquireNextImage(m_presentationResources.imageAvailable, &imageIdx);

  auto currentCmdBuf = m_cmdBuffersDrawMain[m_presentationResources.currentFrame];

  VkSemaphore waitSemaphores[] = {m_presentationResources.imageAvailable};
  VkPipelineStageFlags waitStages[] = {VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT};

  BuildCommandBufferSimple(currentCmdBuf, m_screenFrameBuffers[imageIdx]);

  VkSubmitInfo submitInfo = {};
  submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
  submitInfo.waitSemaphoreCount = 1;
  submitInfo.pWaitSemaphores = waitSemaphores;
  submitInfo.pWaitDstStageMask = waitStages;
  submitInfo.commandBufferCount = 1;
  submitInfo.pCommandBuffers = &currentCmdBuf;

  VkSemaphore signalSemaphores[] = {m_presentationResources.renderingFinished};
  submitInfo.signalSemaphoreCount = 1;
  submitInfo.pSignalSemaphores = signalSemaphores;

  VK_CHECK_RESULT(vkQueueSubmit(m_graphicsQueue, 1, &submitInfo, m_frameFences[m_presentationResources.currentFrame]));

  VkResult presentRes = m_swapchain.QueuePresent(m_presentationResources.queue, imageIdx,
                                                 m_presentationResources.renderingFinished);

  if (presentRes == VK_ERROR_OUT_OF_DATE_KHR || presentRes == VK_SUBOPTIMAL_KHR)
  {
    RecreateSwapChain();
  }
  else if (presentRes != VK_SUCCESS)
  {
    RUN_TIME_ERROR("Failed to present swapchain image");
  }

  m_presentationResources.currentFrame = (m_presentationResources.currentFrame + 1) % m_framesInFlight;

  vkQueueWaitIdle(m_presentationResources.queue);
}

void SimpleRender::DrawFrame(float a_time, DrawMode a_mode)
{
  UpdateUniformBuffer(a_time);
  switch (a_mode)
  {
  case DrawMode::WITH_GUI:
    SetupGUIElements();
    DrawFrameWithGUI();
    break;
  case DrawMode::NO_GUI:
    DrawFrameSimple();
    break;
  default:
    DrawFrameSimple();
  }
}


/////////////////////////////////

void SimpleRender::SetupGUIElements()
{
  ImGui_ImplVulkan_NewFrame();
  ImGui_ImplGlfw_NewFrame();
  ImGui::NewFrame();
  {
//    ImGui::ShowDemoWindow();
    ImGui::Begin("Simple render settings");

    ImGui::ColorEdit3("Meshes base color", m_uniforms.baseColor.M, ImGuiColorEditFlags_PickerHueWheel | ImGuiColorEditFlags_NoInputs);
    ImGui::Checkbox("Animate light source color", &m_uniforms.animateLightColor);
    //ImGui::SliderFloat3("Light source position", m_uniforms.lightPos.M, -10.f, 10.f);
    ImGui::Text("Press H to enable/disable hair in geometry shader");
    //ImGui::Checkbox("Make hair in geometry shader", &make_geom);

    ImGui::Text("Application average %.3f ms/frame (%.1f FPS)", 1000.0f / ImGui::GetIO().Framerate, ImGui::GetIO().Framerate);

    ImGui::NewLine();

    ImGui::TextColored(ImVec4(1.0f, 1.0f, 0.0f, 1.0f),"Press 'B' to recompile and reload shaders");
    ImGui::Text("Changing bindings is not supported.");
    //ImGui::Text("Vertex shader path: %s", VERTEX_SHADER_PATH.c_str());
    //ImGui::Text("Fragment shader path: %s", FRAGMENT_SHADER_PATH.c_str());
    ImGui::End();
  }

  // Rendering
  ImGui::Render();
}

void SimpleRender::DrawFrameWithGUI()
{
  vkWaitForFences(m_device, 1, &m_frameFences[m_presentationResources.currentFrame], VK_TRUE, UINT64_MAX);
  vkResetFences(m_device, 1, &m_frameFences[m_presentationResources.currentFrame]);

  uint32_t imageIdx;
  auto result = m_swapchain.AcquireNextImage(m_presentationResources.imageAvailable, &imageIdx);
  if (result == VK_ERROR_OUT_OF_DATE_KHR)
  {
    RecreateSwapChain();
    return;
  }
  else if (result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR)
  {
    RUN_TIME_ERROR("Failed to acquire the next swapchain image!");
  }

  auto currentCmdBuf = m_cmdBuffersDrawMain[m_presentationResources.currentFrame];

  VkSemaphore waitSemaphores[] = {m_presentationResources.imageAvailable};
  VkPipelineStageFlags waitStages[] = {VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT};

  BuildCommandBufferSimple(currentCmdBuf, m_screenFrameBuffers[imageIdx]);

  ImDrawData* pDrawData = ImGui::GetDrawData();
  auto currentGUICmdBuf = m_pGUIRender->BuildGUIRenderCommand(imageIdx, pDrawData);

  std::vector<VkCommandBuffer> submitCmdBufs = { currentCmdBuf, currentGUICmdBuf};

  VkSubmitInfo submitInfo = {};
  submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
  submitInfo.waitSemaphoreCount = 1;
  submitInfo.pWaitSemaphores = waitSemaphores;
  submitInfo.pWaitDstStageMask = waitStages;
  submitInfo.commandBufferCount = (uint32_t)submitCmdBufs.size();
  submitInfo.pCommandBuffers = submitCmdBufs.data();

  VkSemaphore signalSemaphores[] = {m_presentationResources.renderingFinished};
  submitInfo.signalSemaphoreCount = 1;
  submitInfo.pSignalSemaphores = signalSemaphores;

  VK_CHECK_RESULT(vkQueueSubmit(m_graphicsQueue, 1, &submitInfo, m_frameFences[m_presentationResources.currentFrame]));

  VkResult presentRes = m_swapchain.QueuePresent(m_presentationResources.queue, imageIdx,
    m_presentationResources.renderingFinished);

  if (presentRes == VK_ERROR_OUT_OF_DATE_KHR || presentRes == VK_SUBOPTIMAL_KHR)
  {
    RecreateSwapChain();
  }
  else if (presentRes != VK_SUCCESS)
  {
    RUN_TIME_ERROR("Failed to present swapchain image");
  }

  m_presentationResources.currentFrame = (m_presentationResources.currentFrame + 1) % m_framesInFlight;

  vkQueueWaitIdle(m_presentationResources.queue);
}
