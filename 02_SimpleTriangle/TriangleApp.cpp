#include "TriangleApp.h"

#include <fstream>
#include <array>

using namespace glm;
using namespace std;

void TriangleApp::prepare()
{
  const vec3 red(1.0f, 0.0f, 0.0f);
  const vec3 green(0.0f, 1.0f, 0.0f);
  const vec3 blue(0.0f, 0.0f, 1.0f);
  Vertex vertices[] = {
    { vec3(-1.0f, 0.0f, 0.0f), red },
    { vec3(+1.0f, 0.0f, 0.0f), blue },
    { vec3(0.0f, 1.0f, 0.0f), green },
  };
  uint32_t indices[] = { 0, 1, 2 };

  m_vertexBuffer = createBuffer(sizeof(vertices), VK_BUFFER_USAGE_VERTEX_BUFFER_BIT);
  m_indexBuffer = createBuffer(sizeof(indices), VK_BUFFER_USAGE_INDEX_BUFFER_BIT);

  // 頂点データの書き込み
  {
    void* p;
    vkMapMemory(m_device, m_vertexBuffer.memory, 0, VK_WHOLE_SIZE, 0, &p);
    memcpy(p, vertices, sizeof(vertices));
    vkUnmapMemory(m_device, m_vertexBuffer.memory);
  }
  // インデックスデータの書き込み
  {
    void* p;
    vkMapMemory(m_device, m_indexBuffer.memory, 0, VK_WHOLE_SIZE, 0, &p);
    memcpy(p, indices, sizeof(indices));
    vkUnmapMemory(m_device, m_indexBuffer.memory);
  }
  m_indexCount = _countof(indices);

  // 頂点の入力設定
  VkVertexInputBindingDescription inputBinding{
    0,                          // binding
    sizeof(Vertex),          // stride
    VK_VERTEX_INPUT_RATE_VERTEX // inputRate
  };
  array<VkVertexInputAttributeDescription, 2> inputAttribs{
    {
      { 0, 0, VK_FORMAT_R32G32B32_SFLOAT, offsetof(Vertex, pos)},
      { 1, 0, VK_FORMAT_R32G32B32_SFLOAT, offsetof(Vertex, color)},
    }
  };
  VkPipelineVertexInputStateCreateInfo vertexInputCI{};
  vertexInputCI.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
  vertexInputCI.vertexBindingDescriptionCount = 1;
  vertexInputCI.pVertexBindingDescriptions = &inputBinding;
  vertexInputCI.vertexAttributeDescriptionCount = uint32_t(inputAttribs.size());
  vertexInputCI.pVertexAttributeDescriptions = inputAttribs.data();

  // ブレンディングの設定
  const auto colorWriteAll = \
    VK_COLOR_COMPONENT_R_BIT | \
    VK_COLOR_COMPONENT_G_BIT | \
    VK_COLOR_COMPONENT_B_BIT | \
    VK_COLOR_COMPONENT_A_BIT;
  VkPipelineColorBlendAttachmentState blendAttachment{};
  blendAttachment.blendEnable = VK_TRUE;
  blendAttachment.srcColorBlendFactor = VK_BLEND_FACTOR_ONE;
  blendAttachment.dstColorBlendFactor = VK_BLEND_FACTOR_ZERO;
  blendAttachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
  blendAttachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
  blendAttachment.colorBlendOp = VK_BLEND_OP_ADD;
  blendAttachment.alphaBlendOp = VK_BLEND_OP_ADD;
  blendAttachment.colorWriteMask = colorWriteAll;
  VkPipelineColorBlendStateCreateInfo cbCI{};
  cbCI.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
  cbCI.attachmentCount = 1;
  cbCI.pAttachments = &blendAttachment;

  // ビューポートの設定
  VkViewport viewport;
  {
    viewport.x = 0.0f;
    viewport.y = float(m_swapchainExtent.height);
    viewport.width = float(m_swapchainExtent.width);
    viewport.height = -1.0f * float(m_swapchainExtent.height);
    viewport.minDepth = 0.0f;
    viewport.maxDepth = 1.0f;
  }
  VkRect2D scissor = {
    { 0,0},// offset
    m_swapchainExtent
  };
  VkPipelineViewportStateCreateInfo viewportCI{};
  viewportCI.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
  viewportCI.viewportCount = 1;
  viewportCI.pViewports = &viewport;
  viewportCI.scissorCount = 1;
  viewportCI.pScissors = &scissor;

  // プリミティブトポロジー設定
  VkPipelineInputAssemblyStateCreateInfo inputAssemblyCI{};
  inputAssemblyCI.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
  inputAssemblyCI.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;


  // ラスタライザーステート設定
  VkPipelineRasterizationStateCreateInfo rasterizerCI{};
  rasterizerCI.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
  rasterizerCI.polygonMode = VK_POLYGON_MODE_FILL;
  rasterizerCI.cullMode = VK_CULL_MODE_NONE;
  rasterizerCI.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
  rasterizerCI.lineWidth = 1.0f;

  // マルチサンプル設定
  VkPipelineMultisampleStateCreateInfo multisampleCI{};
  multisampleCI.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
  multisampleCI.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

  // デプスステンシルステート設定
  VkPipelineDepthStencilStateCreateInfo depthStencilCI{};
  depthStencilCI.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
  depthStencilCI.depthTestEnable = VK_TRUE;
  depthStencilCI.depthCompareOp = VK_COMPARE_OP_LESS_OR_EQUAL;
  depthStencilCI.depthWriteEnable = VK_TRUE;
  depthStencilCI.stencilTestEnable = VK_FALSE;

  // シェーダーバイナリの読み込み
  vector<VkPipelineShaderStageCreateInfo> shaderStages
  {
    loadShaderModule("shader.vert.spv", VK_SHADER_STAGE_VERTEX_BIT),
    loadShaderModule("shader.frag.spv", VK_SHADER_STAGE_FRAGMENT_BIT)
  };

  // パイプラインレイアウト
  VkPipelineLayoutCreateInfo pipelineLayoutCI{};
  pipelineLayoutCI.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
  vkCreatePipelineLayout(m_device, &pipelineLayoutCI, nullptr, &m_pipelineLayout);

  // パイプラインの構築
  VkGraphicsPipelineCreateInfo ci{};
  ci.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
  ci.stageCount = uint32_t(shaderStages.size());
  ci.pStages = shaderStages.data();
  ci.pInputAssemblyState = &inputAssemblyCI;
  ci.pVertexInputState = &vertexInputCI;
  ci.pRasterizationState = &rasterizerCI;
  ci.pDepthStencilState = &depthStencilCI;
  ci.pMultisampleState = &multisampleCI;
  ci.pViewportState = &viewportCI;
  ci.pColorBlendState = &cbCI;
  ci.renderPass = m_renderPass;
  ci.layout = m_pipelineLayout;
  vkCreateGraphicsPipelines(m_device, VK_NULL_HANDLE, 1, &ci, nullptr, &m_pipeline);

  // ShaderModule はもう不要のため破棄
  for (const auto& v : shaderStages)
  {
    vkDestroyShaderModule(m_device, v.module, nullptr);
  }
}
void TriangleApp::cleanup()
{
  vkDestroyPipelineLayout(m_device, m_pipelineLayout, nullptr);
  vkDestroyPipeline(m_device, m_pipeline, nullptr);

  vkFreeMemory(m_device, m_vertexBuffer.memory, nullptr);
  vkFreeMemory(m_device, m_indexBuffer.memory, nullptr);
  vkDestroyBuffer(m_device, m_vertexBuffer.buffer, nullptr);
  vkDestroyBuffer(m_device, m_indexBuffer.buffer, nullptr);
}

void TriangleApp::makeCommand(VkCommandBuffer command)
{
  // 作成したパイプラインをセット
  vkCmdBindPipeline(command, VK_PIPELINE_BIND_POINT_GRAPHICS, m_pipeline);

  // 各バッファオブジェクトのセット
  VkDeviceSize offset = 0;
  vkCmdBindVertexBuffers(command, 0, 1, &m_vertexBuffer.buffer, &offset);
  vkCmdBindIndexBuffer(command, m_indexBuffer.buffer, offset, VK_INDEX_TYPE_UINT32);

  // 3角形描画
  vkCmdDrawIndexed(command, m_indexCount, 1, 0, 0, 0);
}


TriangleApp::BufferObject TriangleApp::createBuffer(uint32_t size, VkBufferUsageFlags usage)
{
  BufferObject obj;
  VkBufferCreateInfo ci{};
  ci.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
  ci.usage = usage;
  ci.size = size;
  auto result = vkCreateBuffer(m_device, &ci, nullptr, &obj.buffer);
  checkResult(result);

  // メモリ量の算出
  VkMemoryRequirements reqs;
  vkGetBufferMemoryRequirements(m_device, obj.buffer, &reqs);
  VkMemoryAllocateInfo info{};
  info.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
  info.allocationSize = reqs.size;
  // メモリタイプの判定
  auto flags = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT;
  info.memoryTypeIndex = getMemoryTypeIndex(reqs.memoryTypeBits, flags);
  // メモリの確保
  vkAllocateMemory(m_device, &info, nullptr, &obj.memory);

  // メモリのバインド
  vkBindBufferMemory(m_device, obj.buffer, obj.memory, 0);
  return obj;
}

VkPipelineShaderStageCreateInfo TriangleApp::loadShaderModule(const char* fileName, VkShaderStageFlagBits stage)
{
  ifstream infile(fileName, std::ios::binary);
  if (!infile)
  {
    OutputDebugStringA("file not found.\n");
    DebugBreak();
  }
  vector<char> filedata;
  filedata.resize(uint32_t(infile.seekg(0, ifstream::end).tellg()));
  infile.seekg(0, ifstream::beg).read(filedata.data(), filedata.size());

  VkShaderModule shaderModule;
  VkShaderModuleCreateInfo ci{};
  ci.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
  ci.pCode = reinterpret_cast<uint32_t*>(filedata.data());
  ci.codeSize = filedata.size();
  vkCreateShaderModule(m_device, &ci, nullptr, &shaderModule);

  VkPipelineShaderStageCreateInfo shaderStageCI{};
  shaderStageCI.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
  shaderStageCI.stage = stage;
  shaderStageCI.module = shaderModule;
  shaderStageCI.pName = "main";
  return shaderStageCI;
}