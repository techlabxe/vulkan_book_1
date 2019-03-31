#include "ModelApp.h"

#include <fstream>
#include <array>
#include <glm/gtc/matrix_transform.hpp>

#define STB_IMAGE_IMPLEMENTATION
#include "../common/stb_image.h"

#include "streamreader.h"

using namespace glm;
using namespace std;

void ModelApp::prepare()
{
  // モデルデータの読み込み
  auto modelFilePath = experimental::filesystem::path("alicia-solid.vrm");
  if (modelFilePath.is_relative())
  {
    auto current = experimental::filesystem::current_path();
    current /= modelFilePath;
    current.swap(modelFilePath);
  }

  auto reader = make_unique<StreamReader>(modelFilePath.parent_path());
  auto glbStream = reader->GetInputStream(modelFilePath.filename().u8string());
  auto glbResourceReader = make_shared<Microsoft::glTF::GLBResourceReader>(std::move(reader), std::move(glbStream));
  auto document = Microsoft::glTF::Deserialize(glbResourceReader->GetJson());

  makeModelGeometry(document, glbResourceReader);
  makeModelMaterial(document, glbResourceReader);

  prepareUniformBuffers();
  prepareDescriptorSetLayout();
  prepareDescriptorPool();
 
  m_sampler = createSampler();
  prepareDescriptorSet();

  // 頂点の入力設定
  VkVertexInputBindingDescription inputBinding{
    0,                          // binding
    sizeof(Vertex),         // stride
    VK_VERTEX_INPUT_RATE_VERTEX // inputRate
  };
  array<VkVertexInputAttributeDescription, 3> inputAttribs{
    {
      { 0, 0, VK_FORMAT_R32G32B32_SFLOAT, offsetof(Vertex, pos)},
      { 1, 0, VK_FORMAT_R32G32B32_SFLOAT, offsetof(Vertex, color)},
      { 2, 0, VK_FORMAT_R32G32B32_SFLOAT, offsetof(Vertex, uv)},
    }
  };
  VkPipelineVertexInputStateCreateInfo vertexInputCI{};
  vertexInputCI.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
  vertexInputCI.vertexBindingDescriptionCount = 1;
  vertexInputCI.pVertexBindingDescriptions = &inputBinding;
  vertexInputCI.vertexAttributeDescriptionCount = uint32_t(inputAttribs.size());
  vertexInputCI.pVertexAttributeDescriptions = inputAttribs.data();


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

  // パイプラインレイアウト
  VkPipelineLayoutCreateInfo pipelineLayoutCI{};
  pipelineLayoutCI.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
  pipelineLayoutCI.setLayoutCount = 1;
  pipelineLayoutCI.pSetLayouts = &m_descriptorSetLayout;
  vkCreatePipelineLayout(m_device, &pipelineLayoutCI, nullptr, &m_pipelineLayout);

  // 不透明用: パイプラインの構築
  {
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
      loadShaderModule("shaderOpaque.frag.spv", VK_SHADER_STAGE_FRAGMENT_BIT)
    };
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
    vkCreateGraphicsPipelines(m_device, VK_NULL_HANDLE, 1, &ci, nullptr, &m_pipelineOpaque);

    // ShaderModule はもう不要のため破棄
    for (const auto& v : shaderStages)
    {
      vkDestroyShaderModule(m_device, v.module, nullptr);
    }
  }

  // 半透明用: パイプラインの構築
  {
    // ブレンディングの設定
    const auto colorWriteAll = \
      VK_COLOR_COMPONENT_R_BIT | \
      VK_COLOR_COMPONENT_G_BIT | \
      VK_COLOR_COMPONENT_B_BIT | \
      VK_COLOR_COMPONENT_A_BIT;
    VkPipelineColorBlendAttachmentState blendAttachment{};
    blendAttachment.blendEnable = VK_TRUE;
    blendAttachment.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
    blendAttachment.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
    blendAttachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
    blendAttachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
    blendAttachment.colorBlendOp = VK_BLEND_OP_ADD;
    blendAttachment.alphaBlendOp = VK_BLEND_OP_ADD;
    blendAttachment.colorWriteMask = colorWriteAll;
    VkPipelineColorBlendStateCreateInfo cbCI{};
    cbCI.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    cbCI.attachmentCount = 1;
    cbCI.pAttachments = &blendAttachment;

    // デプスステンシルステート設定
    VkPipelineDepthStencilStateCreateInfo depthStencilCI{};
    depthStencilCI.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
    depthStencilCI.depthTestEnable = VK_TRUE;
    depthStencilCI.depthCompareOp = VK_COMPARE_OP_LESS_OR_EQUAL;
    depthStencilCI.depthWriteEnable = VK_FALSE;
    depthStencilCI.stencilTestEnable = VK_FALSE;

    // シェーダーバイナリの読み込み
    vector<VkPipelineShaderStageCreateInfo> shaderStages
    {
      loadShaderModule("shader.vert.spv", VK_SHADER_STAGE_VERTEX_BIT),
      loadShaderModule("shaderAlpha.frag.spv", VK_SHADER_STAGE_FRAGMENT_BIT)
    };
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
    vkCreateGraphicsPipelines(m_device, VK_NULL_HANDLE, 1, &ci, nullptr, &m_pipelineAlpha);

    // ShaderModule はもう不要のため破棄
    for (const auto& v : shaderStages)
    {
      vkDestroyShaderModule(m_device, v.module, nullptr);
    }
  }
}
void ModelApp::cleanup()
{
  for (auto& v : m_uniformBuffers)
  {
    vkDestroyBuffer(m_device, v.buffer, nullptr);
    vkFreeMemory(m_device, v.memory, nullptr);
  }
  vkDestroySampler(m_device, m_sampler, nullptr);

  vkDestroyPipelineLayout(m_device, m_pipelineLayout, nullptr);
  vkDestroyPipeline(m_device, m_pipelineOpaque, nullptr);
  vkDestroyPipeline(m_device, m_pipelineAlpha, nullptr);

  for (auto& mesh : m_model.meshes)
  {
    vkFreeMemory(m_device, mesh.vertexBuffer.memory, nullptr);
    vkFreeMemory(m_device, mesh.indexBuffer.memory, nullptr);
    vkDestroyBuffer(m_device, mesh.vertexBuffer.buffer, nullptr);
    vkDestroyBuffer(m_device, mesh.indexBuffer.buffer, nullptr);
    uint32_t count = uint32_t(mesh.descriptorSet.size());
    mesh.descriptorSet.clear();
  }
  for (auto& material : m_model.materials)
  {
    vkFreeMemory(m_device, material.texture.memory, nullptr);
    vkDestroyImage(m_device, material.texture.image, nullptr);
    vkDestroyImageView(m_device, material.texture.view, nullptr);
  }
  vkDestroyDescriptorPool(m_device, m_descriptorPool, nullptr);
  vkDestroyDescriptorSetLayout(m_device, m_descriptorSetLayout, nullptr);
}

void ModelApp::makeCommand(VkCommandBuffer command)
{
  using namespace Microsoft::glTF;

  // ユニフォームバッファの中身を更新する.
  ShaderParameters shaderParam{};
  shaderParam.mtxWorld = glm::identity<glm::mat4>();
  shaderParam.mtxView = lookAtRH(vec3(0.0f, 1.5f, -1.0f), vec3(0.0f, 1.25f, 0.0f), vec3(0.0f, 1.0f, 0.0f));
  shaderParam.mtxProj = perspective(glm::radians(45.0f), 640.0f / 480, 0.01f, 100.0f);
  {
    auto memory = m_uniformBuffers[m_imageIndex].memory;
    void* p;
    vkMapMemory(m_device, memory, 0, VK_WHOLE_SIZE, 0, &p);
    memcpy(p, &shaderParam, sizeof(shaderParam));
    vkUnmapMemory(m_device, memory);
  }

  for (auto mode : { ALPHA_OPAQUE, ALPHA_MASK, ALPHA_BLEND })
  {
    for (const auto& mesh : m_model.meshes)
    {
      // 対応するポリゴンメッシュのみを描画する.
      if (m_model.materials[mesh.materialIndex].alphaMode != mode)
      {
        continue;
      }

      // モードに応じて使用するパイプラインを変える.
      switch (mode)
      {
      case ALPHA_OPAQUE:
        vkCmdBindPipeline(command, VK_PIPELINE_BIND_POINT_GRAPHICS, m_pipelineOpaque);
        break;
      case ALPHA_MASK:
        vkCmdBindPipeline(command, VK_PIPELINE_BIND_POINT_GRAPHICS, m_pipelineOpaque);
        break;
      case ALPHA_BLEND:
        vkCmdBindPipeline(command, VK_PIPELINE_BIND_POINT_GRAPHICS, m_pipelineAlpha);
        break;
      }

      // 各バッファオブジェクトのセット
      VkDeviceSize offset = 0;
      vkCmdBindVertexBuffers(command, 0, 1, &mesh.vertexBuffer.buffer, &offset);
      vkCmdBindIndexBuffer(command, mesh.indexBuffer.buffer, offset, VK_INDEX_TYPE_UINT32);

      // ディスクリプタセットをセット
      VkDescriptorSet descriptorSets[] = {
        mesh.descriptorSet[m_imageIndex]
      };
      vkCmdBindDescriptorSets(command, VK_PIPELINE_BIND_POINT_GRAPHICS, m_pipelineLayout, 0, 1, descriptorSets, 0, nullptr);

      // このメッシュを描画
      vkCmdDrawIndexed(command, mesh.indexCount, 1, 0, 0, 0);
    }
  }
}

void ModelApp::makeModelGeometry(const Microsoft::glTF::Document& doc, std::shared_ptr<Microsoft::glTF::GLTFResourceReader> reader )
{
  using namespace Microsoft::glTF;
  for (const auto& mesh : doc.meshes.Elements())
  {
    for (const auto& meshPrimitive : mesh.primitives)
    {
      std::vector<Vertex> vertices;
      std::vector<uint32_t> indices;

      // 頂点位置情報アクセッサの取得
      auto& idPos = meshPrimitive.GetAttributeAccessorId(ACCESSOR_POSITION);
      auto& accPos = doc.accessors.Get(idPos);
      // 法線情報アクセッサの取得
      auto& idNrm = meshPrimitive.GetAttributeAccessorId(ACCESSOR_NORMAL);
      auto& accNrm = doc.accessors.Get(idNrm);
      // テクスチャ座標情報アクセッサの取得
      auto& idUV = meshPrimitive.GetAttributeAccessorId(ACCESSOR_TEXCOORD_0);
      auto& accUV = doc.accessors.Get(idUV);
      // 頂点インデックス用アクセッサの取得
      auto& idIndex = meshPrimitive.indicesAccessorId;
      auto& accIndex = doc.accessors.Get(idIndex);

      // アクセッサからデータ列を取得
      auto vertPos = reader->ReadBinaryData<float>(doc, accPos);
      auto vertNrm = reader->ReadBinaryData<float>(doc, accNrm);
      auto vertUV = reader->ReadBinaryData<float>(doc, accUV);

      auto vertexCount = accPos.count;
      for (uint32_t i = 0; i < vertexCount; ++i)
      {
        // 頂点データの構築
        int vid0 = 3*i, vid1 = 3*i+1, vid2 = 3*i+2;
        int tid0 = 2*i, tid1 = 2*i+1;
        vertices.emplace_back(
          Vertex{
            vec3(vertPos[vid0], vertPos[vid1],vertPos[vid2]),
            vec3(vertNrm[vid0], vertNrm[vid1],vertNrm[vid2]),
            vec2(vertUV[tid0],vertUV[tid1])
          }
        );
      }
      // インデックスデータ
      indices = reader->ReadBinaryData<uint32_t>(doc, accIndex);

      auto vbSize = sizeof(Vertex)*vertices.size();
      auto ibSize = sizeof(uint32_t)*indices.size();
      ModelMesh modelMesh;
      modelMesh.vertexBuffer = createBuffer(vbSize, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT, vertices.data());
      modelMesh.indexBuffer = createBuffer(ibSize,VK_BUFFER_USAGE_INDEX_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT, indices.data());
      modelMesh.vertexCount = vertices.size();
      modelMesh.indexCount = indices.size();
      modelMesh.materialIndex = doc.materials.GetIndex(meshPrimitive.materialId);
      m_model.meshes.push_back(modelMesh);
    }
  }
}
void ModelApp::makeModelMaterial(const Microsoft::glTF::Document& doc, std::shared_ptr<Microsoft::glTF::GLTFResourceReader> reader)
{
  for (auto& m : doc.materials.Elements())
  {
    auto textureId = m.metallicRoughness.baseColorTexture.textureId;
    if (textureId.empty())
    {
      textureId = m.normalTexture.textureId;
    }
    auto& texture = doc.textures.Get(textureId);
    auto& image = doc.images.Get(texture.imageId);
    auto imageBufferView = doc.bufferViews.Get(image.bufferViewId);
    auto imageData = reader->ReadBinaryData<char>(doc, imageBufferView);

    // imageData が画像データ.
    Material material{};
    material.alphaMode = m.alphaMode;
    material.texture = createTextureFromMemory(imageData);
    m_model.materials.push_back(material);
  }
}

void ModelApp::prepareUniformBuffers()
{
  m_uniformBuffers.resize(m_swapchainViews.size());
  for (auto& v : m_uniformBuffers)
  {
    VkMemoryPropertyFlags uboFlags = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
    v = createBuffer(sizeof(ShaderParameters), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, uboFlags, nullptr );
  }
}
void ModelApp::prepareDescriptorSetLayout()
{
  vector<VkDescriptorSetLayoutBinding> bindings;
  VkDescriptorSetLayoutBinding bindingUBO{}, bindingTex{};
  bindingUBO.binding = 0;
  bindingUBO.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
  bindingUBO.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
  bindingUBO.descriptorCount = 1;
  bindings.push_back(bindingUBO);
  
  bindingTex.binding = 1;
  bindingTex.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
  bindingTex.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
  bindingTex.descriptorCount = 1;
  bindings.push_back(bindingTex);

  VkDescriptorSetLayoutCreateInfo ci{};
  ci.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
  ci.bindingCount = uint32_t(bindings.size());
  ci.pBindings = bindings.data();
  vkCreateDescriptorSetLayout(m_device, &ci, nullptr, &m_descriptorSetLayout);
}

void ModelApp::prepareDescriptorPool()
{
  array<VkDescriptorPoolSize, 2> descPoolSize;
  descPoolSize[0].descriptorCount = 1;
  descPoolSize[0].type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
  descPoolSize[1].descriptorCount = 1;
  descPoolSize[1].type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;

  uint32_t maxDescriptorCount = m_swapchainImages.size() * m_model.meshes.size();
  VkDescriptorPoolCreateInfo ci{};
  ci.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
  ci.maxSets = maxDescriptorCount;
  ci.poolSizeCount = uint32_t(descPoolSize.size());
  ci.pPoolSizes = descPoolSize.data();
  vkCreateDescriptorPool(m_device, &ci, nullptr, &m_descriptorPool);
}

void ModelApp::prepareDescriptorSet()
{
  vector<VkDescriptorSetLayout> layouts;
  for (int i = 0; i<int(m_uniformBuffers.size()); ++i)
  {
    layouts.push_back(m_descriptorSetLayout);
  }

  for (auto& mesh : m_model.meshes)
  {
    // ディスクリプタセットの確保
    VkDescriptorSetAllocateInfo ai{};
    ai.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    ai.descriptorPool = m_descriptorPool;
    ai.descriptorSetCount = m_uniformBuffers.size();
    ai.pSetLayouts = layouts.data();
    mesh.descriptorSet.resize(m_uniformBuffers.size());
    vkAllocateDescriptorSets(m_device, &ai, mesh.descriptorSet.data());

    // ディスクリプタセットへ書き込み.
    auto material = m_model.materials[mesh.materialIndex];
    for (int i = 0; i<int(m_uniformBuffers.size()); ++i)
    {
      VkDescriptorBufferInfo descUBO{};
      descUBO.buffer = m_uniformBuffers[i].buffer;
      descUBO.offset = 0;
      descUBO.range = VK_WHOLE_SIZE;

      VkDescriptorImageInfo  descImage{};
      descImage.imageView = material.texture.view;
      descImage.sampler = m_sampler;
      descImage.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

      VkWriteDescriptorSet ubo{};
      ubo.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
      ubo.dstBinding = 0;
      ubo.descriptorCount = 1;
      ubo.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
      ubo.pBufferInfo = &descUBO;
      ubo.dstSet = mesh.descriptorSet[i];

      VkWriteDescriptorSet tex{};
      tex.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
      tex.dstBinding = 1;
      tex.descriptorCount = 1;
      tex.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
      tex.pImageInfo = &descImage;
      tex.dstSet = mesh.descriptorSet[i];

      vector<VkWriteDescriptorSet> writeSets = {
        ubo, tex
      };
      vkUpdateDescriptorSets(m_device, uint32_t(writeSets.size()), writeSets.data(), 0, nullptr);
    }
  }
}

ModelApp::BufferObject ModelApp::createBuffer(uint32_t size, VkBufferUsageFlags usage, VkMemoryPropertyFlags flags, const void* initialData)
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
  info.memoryTypeIndex = getMemoryTypeIndex(reqs.memoryTypeBits, flags);
  // メモリの確保
  vkAllocateMemory(m_device, &info, nullptr, &obj.memory);

  // メモリのバインド
  vkBindBufferMemory(m_device, obj.buffer, obj.memory, 0);

  if ((flags & VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT) != 0 &&
    initialData != nullptr)
  {
    void* p;
    vkMapMemory(m_device, obj.memory, 0, VK_WHOLE_SIZE, 0, &p);
    memcpy(p, initialData, size);
    vkUnmapMemory(m_device, obj.memory);
  }
  return obj;
}

VkPipelineShaderStageCreateInfo ModelApp::loadShaderModule(const char* fileName, VkShaderStageFlagBits stage)
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

VkSampler ModelApp::createSampler()
{
  VkSampler sampler;
  VkSamplerCreateInfo ci{};
  ci.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
  ci.minFilter = VK_FILTER_LINEAR;
  ci.magFilter = VK_FILTER_LINEAR;
  ci.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
  ci.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
  ci.maxAnisotropy = 1.0f;
  ci.borderColor = VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE;
  vkCreateSampler(m_device, &ci, nullptr, &sampler);
  return sampler;
}

ModelApp::TextureObject ModelApp::createTextureFromMemory(const std::vector<char>& imageData)
{
  BufferObject stagingBuffer;
  TextureObject texture{};
  int width, height, channels;
  auto* pImage = stbi_load_from_memory(
    reinterpret_cast<const uint8_t*>(imageData.data()),
    imageData.size(), &width, &height, &channels, 0);

  auto format = VK_FORMAT_R8G8B8A8_UNORM;

  {
    // テクスチャのVkImage を生成
    VkImageCreateInfo ci{};
    ci.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    ci.extent = { uint32_t(width), uint32_t(height), 1 };
    ci.format = format;
    ci.imageType = VK_IMAGE_TYPE_2D;
    ci.arrayLayers = 1;
    ci.mipLevels = 1;
    ci.samples = VK_SAMPLE_COUNT_1_BIT;
    ci.usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
    vkCreateImage(m_device, &ci, nullptr, &texture.image);

    // メモリ量の算出
    VkMemoryRequirements reqs;
    vkGetImageMemoryRequirements(m_device, texture.image, &reqs);
    VkMemoryAllocateInfo info{};
    info.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    info.allocationSize = reqs.size;
    // メモリタイプの判定
    info.memoryTypeIndex = getMemoryTypeIndex(reqs.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
    // メモリの確保
    vkAllocateMemory(m_device, &info, nullptr, &texture.memory);
    // メモリのバインド
    vkBindImageMemory(m_device, texture.image, texture.memory, 0);
  }

  {
    uint32_t imageSize = width * height * sizeof(uint32_t);
    // ステージングバッファを用意.
    stagingBuffer = createBuffer(imageSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT, pImage);
  }

  VkBufferImageCopy copyRegion{};
  copyRegion.imageExtent = { uint32_t(width), uint32_t(height), 1 };
  copyRegion.imageSubresource = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1 };
  VkCommandBuffer command;
  {
    VkCommandBufferAllocateInfo ai{};
    ai.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    ai.commandBufferCount = 1;
    ai.commandPool = m_commandPool;
    ai.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    vkAllocateCommandBuffers(m_device, &ai, &command);
  }

  VkCommandBufferBeginInfo commandBI{};
  commandBI.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
  vkBeginCommandBuffer(command, &commandBI);
  setImageMemoryBarrier(command, texture.image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
  vkCmdCopyBufferToImage(command, stagingBuffer.buffer, texture.image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &copyRegion);

  setImageMemoryBarrier(command, texture.image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
  vkEndCommandBuffer(command);

  VkSubmitInfo submitInfo{};
  submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
  submitInfo.commandBufferCount = 1;
  submitInfo.pCommandBuffers = &command;
  vkQueueSubmit(m_deviceQueue, 1, &submitInfo, VK_NULL_HANDLE);
  {
    // テクスチャ参照用のビューを生成
    VkImageViewCreateInfo ci{};
    ci.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    ci.viewType = VK_IMAGE_VIEW_TYPE_2D;
    ci.image = texture.image;
    ci.format = format;
    ci.components = {
      VK_COMPONENT_SWIZZLE_R,
      VK_COMPONENT_SWIZZLE_G,
      VK_COMPONENT_SWIZZLE_B,
      VK_COMPONENT_SWIZZLE_A,
    };
    ci.subresourceRange = {
      VK_IMAGE_ASPECT_COLOR_BIT,0,1,0,1
    };
    vkCreateImageView(m_device, &ci, nullptr, &texture.view);
  }

  vkDeviceWaitIdle(m_device);
  vkFreeCommandBuffers(m_device, m_commandPool, 1, &command);

  // ステージングバッファ解放.
  vkFreeMemory(m_device, stagingBuffer.memory, nullptr);
  vkDestroyBuffer(m_device, stagingBuffer.buffer, nullptr);

  return texture;
}

void ModelApp::setImageMemoryBarrier(
  VkCommandBuffer command,
  VkImage image,
  VkImageLayout oldLayout, VkImageLayout newLayout)
{
  VkImageMemoryBarrier imb{};
  imb.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
  imb.oldLayout = oldLayout;
  imb.newLayout = newLayout;
  imb.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
  imb.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
  imb.subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 };
  imb.image = image;

  VkPipelineStageFlags srcStage = VK_PIPELINE_STAGE_ALL_COMMANDS_BIT;
  VkPipelineStageFlags dstStage = VK_PIPELINE_STAGE_ALL_COMMANDS_BIT;

  switch (oldLayout)
  {
  case VK_IMAGE_LAYOUT_UNDEFINED:
    imb.srcAccessMask = 0;
    break;
  case VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL:
    imb.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    srcStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
    break;
  }

  switch (newLayout)
  {
  case VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL:
    imb.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
    dstStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
    break;
  case VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL:
    imb.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    dstStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
    break;
  case VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL:
    imb.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
    dstStage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
    break;
  }

  //srcStage = VK_PIPELINE_STAGE_ALL_COMMANDS_BIT;  // パイプライン中でリソースへの書込み最終のステージ.
  //dstStage = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;  // パイプライン中で次にリソースに書き込むステージ.

  vkCmdPipelineBarrier(
    command,
    srcStage,
    dstStage,
    0,
    0,  // memoryBarrierCount
    nullptr,
    0,  // bufferMemoryBarrierCount
    nullptr,
    1,  // imageMemoryBarrierCount
    &imb);
}
