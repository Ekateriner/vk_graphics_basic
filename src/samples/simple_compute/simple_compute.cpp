#include "simple_compute.h"

#include <vk_pipeline.h>
#include <vk_buffers.h>
#include <vk_utils.h>

SimpleCompute::SimpleCompute(uint32_t a_length) : m_length(a_length)
{
#ifdef NDEBUG
  m_enableValidation = false;
#else
  m_enableValidation = true;
#endif
}

void SimpleCompute::SetupValidationLayers()
{
  m_validationLayers.push_back("VK_LAYER_KHRONOS_validation");
  m_validationLayers.push_back("VK_LAYER_LUNARG_monitor");
}

void SimpleCompute::InitVulkan(const char** a_instanceExtensions, uint32_t a_instanceExtensionsCount, uint32_t a_deviceId)
{
  m_instanceExtensions.clear();
  for (uint32_t i = 0; i < a_instanceExtensionsCount; ++i) {
    m_instanceExtensions.push_back(a_instanceExtensions[i]);
  }
  SetupValidationLayers();
  VK_CHECK_RESULT(volkInitialize());
  CreateInstance();
  volkLoadInstance(m_instance);

  CreateDevice(a_deviceId);
  volkLoadDevice(m_device);

  m_commandPool = vk_utils::createCommandPool(m_device, m_queueFamilyIDXs.compute, VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT);

  m_cmdBufferCompute = vk_utils::createCommandBuffers(m_device, m_commandPool, 1)[0];
  
  m_pCopyHelper = std::make_shared<vk_utils::SimpleCopyHelper>(m_physicalDevice, m_device, m_transferQueue, m_queueFamilyIDXs.compute, 8*1024*1024);
}


void SimpleCompute::CreateInstance()
{
  VkApplicationInfo appInfo = {};
  appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
  appInfo.pNext = nullptr;
  appInfo.pApplicationName = "VkRender";
  appInfo.applicationVersion = VK_MAKE_VERSION(0, 1, 0);
  appInfo.pEngineName = "SimpleCompute";
  appInfo.engineVersion = VK_MAKE_VERSION(0, 1, 0);
  appInfo.apiVersion = VK_MAKE_VERSION(1, 1, 0);

  m_instance = vk_utils::createInstance(m_enableValidation, m_validationLayers, m_instanceExtensions, &appInfo);
  if (m_enableValidation)
    vk_utils::initDebugReportCallback(m_instance, &debugReportCallbackFn, &m_debugReportCallback);
}

void SimpleCompute::CreateDevice(uint32_t a_deviceId)
{
  m_physicalDevice = vk_utils::findPhysicalDevice(m_instance, true, a_deviceId, m_deviceExtensions);

  m_device = vk_utils::createLogicalDevice(m_physicalDevice, m_validationLayers, m_deviceExtensions,
                                           m_enabledDeviceFeatures, m_queueFamilyIDXs,
                                           VK_QUEUE_COMPUTE_BIT | VK_QUEUE_TRANSFER_BIT);

  vkGetDeviceQueue(m_device, m_queueFamilyIDXs.compute, 0, &m_computeQueue);
  vkGetDeviceQueue(m_device, m_queueFamilyIDXs.transfer, 0, &m_transferQueue);
}


void SimpleCompute::SetupSimplePipeline()
{
  std::vector<std::pair<VkDescriptorType, uint32_t> > dtypes = {
      {VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,             8}
  };

  // Создание и аллокация буферов
  m_Arr = vk_utils::createBuffer(m_device, sizeof(float) * m_length, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT |
                                                                       VK_BUFFER_USAGE_TRANSFER_DST_BIT);
  m_Result = vk_utils::createBuffer(m_device, sizeof(float) * m_length, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT |
                                                                       VK_BUFFER_USAGE_TRANSFER_SRC_BIT);
  m_Sums = vk_utils::createBuffer(m_device, sizeof(float) * m_block_size, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT |
                                                                       VK_BUFFER_USAGE_TRANSFER_SRC_BIT);
  vk_utils::allocateAndBindWithPadding(m_device, m_physicalDevice, {m_Arr, m_Result, m_Sums}, 0);

  m_pBindings = std::make_shared<vk_utils::DescriptorMaker>(m_device, dtypes, 3);
  // Создание descriptor set для передачи буферов в шейдер
  m_pBindings->BindBegin(VK_SHADER_STAGE_COMPUTE_BIT);
  m_pBindings->BindBuffer(0, m_Arr);
  m_pBindings->BindBuffer(1, m_Result);
  m_pBindings->BindBuffer(2, m_Sums);
  m_pBindings->BindEnd(&m_first_scanDS, &m_scanDSLayout);

  // Создание descriptor set для передачи буферов в шейдер
  m_pBindings->BindBegin(VK_SHADER_STAGE_COMPUTE_BIT);
  m_pBindings->BindBuffer(0, m_Sums);
  m_pBindings->BindBuffer(1, m_Sums);
  m_pBindings->BindBuffer(2, m_Sums);
  m_pBindings->BindEnd(&m_second_scanDS, &m_scanDSLayout);

  // Создание descriptor set для передачи буферов в шейдер
  m_pBindings->BindBegin(VK_SHADER_STAGE_COMPUTE_BIT);
  m_pBindings->BindBuffer(0, m_Result);
  m_pBindings->BindBuffer(1, m_Sums);
  m_pBindings->BindEnd(&m_add_shiftDS, &m_add_shiftDSLayout);


  // Заполнение буферов
  std::vector<float> values(m_length);
  for (uint32_t i = 0; i < values.size(); ++i) {
    values[i] = 1.0;
  }
  m_pCopyHelper->UpdateBuffer(m_Arr, 0, values.data(), sizeof(float) * values.size());
}

void SimpleCompute::BuildCommandBufferSimple(VkCommandBuffer a_cmdBuff, VkPipeline a_pipeline)
{
  vkResetCommandBuffer(a_cmdBuff, 0);

  VkCommandBufferBeginInfo beginInfo = {};
  beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
  beginInfo.flags = VK_COMMAND_BUFFER_USAGE_SIMULTANEOUS_USE_BIT;

  // Заполняем буфер команд
  VK_CHECK_RESULT(vkBeginCommandBuffer(a_cmdBuff, &beginInfo));

  vkCmdBindPipeline      (a_cmdBuff, VK_PIPELINE_BIND_POINT_COMPUTE, m_scan_pipeline);
  vkCmdBindDescriptorSets(a_cmdBuff, VK_PIPELINE_BIND_POINT_COMPUTE, m_scan_layout, 0, 1, &m_first_scanDS, 0, NULL);

  m_scan_pushConst.len = m_length;
  m_scan_pushConst.stage = 0;
  vkCmdPushConstants(a_cmdBuff, m_scan_layout, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(m_scan_pushConst), &m_scan_pushConst);

  vkCmdDispatch(a_cmdBuff, (m_scan_pushConst.len / m_block_size) + (m_scan_pushConst.len % m_block_size != 0), 1, 1);

  VkBufferMemoryBarrier barrier = {};
  barrier.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER;
  barrier.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
  barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
  barrier.buffer = m_Sums;
  barrier.offset = 0;
  barrier.size = m_block_size * sizeof(float);

  vkCmdPipelineBarrier(a_cmdBuff, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                       {}, 0, nullptr, 1, &barrier, 0, nullptr);

  //vkCmdBindPipeline      (a_cmdBuff, VK_PIPELINE_BIND_POINT_COMPUTE, m_scan_pipeline);
  vkCmdBindDescriptorSets(a_cmdBuff, VK_PIPELINE_BIND_POINT_COMPUTE, m_scan_layout, 0, 1, &m_second_scanDS, 0, NULL);

  m_scan_pushConst.len = (m_length / m_block_size) + (m_length % m_block_size != 0);
  m_scan_pushConst.stage = 1;
  vkCmdPushConstants(a_cmdBuff, m_scan_layout, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(m_scan_pushConst), &m_scan_pushConst);

  vkCmdDispatch(a_cmdBuff, 1, 1, 1);

  vkCmdPipelineBarrier(a_cmdBuff, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                       {}, 0, nullptr, 1, &barrier, 0, nullptr);

  vkCmdBindPipeline      (a_cmdBuff, VK_PIPELINE_BIND_POINT_COMPUTE, m_add_shift_pipeline);
  vkCmdBindDescriptorSets(a_cmdBuff, VK_PIPELINE_BIND_POINT_COMPUTE, m_add_shift_layout, 0, 1, &m_add_shiftDS, 0, NULL);

  vkCmdPushConstants(a_cmdBuff, m_scan_layout, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(m_length), &m_length);

  vkCmdDispatch(a_cmdBuff, (m_length / m_block_size) + (m_length % m_block_size != 0), 1, 1);
  VK_CHECK_RESULT(vkEndCommandBuffer(a_cmdBuff));
}


void SimpleCompute::CleanupPipeline()
{
  if (m_cmdBufferCompute)
  {
    vkFreeCommandBuffers(m_device, m_commandPool, 1, &m_cmdBufferCompute);
  }

  vkDestroyBuffer(m_device, m_Arr, nullptr);
  vkDestroyBuffer(m_device, m_Result, nullptr);
  vkDestroyBuffer(m_device, m_Sums, nullptr);

  auto destroy = [this] (VkPipelineLayout& layout, VkPipeline& pipeline) {
      vkDestroyPipelineLayout(m_device, layout, nullptr);
      vkDestroyPipeline(m_device, pipeline, nullptr);
  };

  destroy(m_scan_layout, m_scan_pipeline);
  destroy(m_add_shift_layout, m_add_shift_pipeline);
}


void SimpleCompute::Cleanup()
{
  CleanupPipeline();

  if (m_commandPool != VK_NULL_HANDLE)
  {
    vkDestroyCommandPool(m_device, m_commandPool, nullptr);
  }
}

void SimpleCompute::CreateComputePipeline()
{
  auto create = [this] (const char *shader_path, size_t push_const_size, VkDescriptorSetLayout& DSLayout,
                         VkPipelineLayout& layout, VkPipeline& pipeline) {
      // Загружаем шейдер
      std::vector<uint32_t> code = vk_utils::readSPVFile(shader_path);
      VkShaderModuleCreateInfo createInfo = {};
      createInfo.sType    = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
      createInfo.pCode    = code.data();
      createInfo.codeSize = code.size()*sizeof(uint32_t);

      VkShaderModule shaderModule;
      // Создаём шейдер в вулкане
      VK_CHECK_RESULT(vkCreateShaderModule(m_device, &createInfo, NULL, &shaderModule));

      VkPipelineShaderStageCreateInfo shaderStageCreateInfo = {};
      shaderStageCreateInfo.sType  = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
      shaderStageCreateInfo.stage  = VK_SHADER_STAGE_COMPUTE_BIT;
      shaderStageCreateInfo.module = shaderModule;
      shaderStageCreateInfo.pName  = "main";

      VkPushConstantRange pcRange = {};
      pcRange.offset = 0;
      pcRange.size = push_const_size;
      pcRange.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;

      // Создаём layout для pipeline
      VkPipelineLayoutCreateInfo pipelineLayoutCreateInfo = {};
      pipelineLayoutCreateInfo.sType          = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
      pipelineLayoutCreateInfo.setLayoutCount = 1;
      pipelineLayoutCreateInfo.pSetLayouts    = &DSLayout;
      pipelineLayoutCreateInfo.pushConstantRangeCount = 1;
      pipelineLayoutCreateInfo.pPushConstantRanges = &pcRange;
      VK_CHECK_RESULT(vkCreatePipelineLayout(m_device, &pipelineLayoutCreateInfo, NULL, &layout));

      VkComputePipelineCreateInfo pipelineCreateInfo = {};
      pipelineCreateInfo.sType  = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
      pipelineCreateInfo.stage  = shaderStageCreateInfo;
      pipelineCreateInfo.layout = layout;

      // Создаём pipeline - объект, который выставляет шейдер и его параметры
      VK_CHECK_RESULT(vkCreateComputePipelines(m_device, VK_NULL_HANDLE, 1, &pipelineCreateInfo, NULL, &pipeline));

      vkDestroyShaderModule(m_device, shaderModule, nullptr);
  };

  create("../resources/shaders/local_scan.comp.spv", 2*sizeof(m_length),
         m_scanDSLayout, m_scan_layout, m_scan_pipeline);
  create("../resources/shaders/shift_add.comp.spv", sizeof(m_length),
         m_add_shiftDSLayout, m_add_shift_layout, m_add_shift_pipeline);
}


void SimpleCompute::Execute()
{
  SetupSimplePipeline();
  CreateComputePipeline();

  BuildCommandBufferSimple(m_cmdBufferCompute, nullptr);

  VkSubmitInfo submitInfo = {};
  submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
  submitInfo.commandBufferCount = 1;
  submitInfo.pCommandBuffers = &m_cmdBufferCompute;

  VkFenceCreateInfo fenceCreateInfo = {};
  fenceCreateInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
  fenceCreateInfo.flags = 0;
  VK_CHECK_RESULT(vkCreateFence(m_device, &fenceCreateInfo, NULL, &m_fence));

  // Отправляем буфер команд на выполнение
  VK_CHECK_RESULT(vkQueueSubmit(m_computeQueue, 1, &submitInfo, m_fence));

  //Ждём конца выполнения команд
  VK_CHECK_RESULT(vkWaitForFences(m_device, 1, &m_fence, VK_TRUE, 100000000000));

  std::vector<float> values(m_length);
  m_pCopyHelper->ReadBuffer(m_Result, 0, values.data(), sizeof(float) * values.size());
  for (auto v: values) {
      std::cout << v << ' ';
  }
  std::cout << std::endl;
}
