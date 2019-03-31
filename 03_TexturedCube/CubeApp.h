#pragma once

#include "../common/vkappbase.h"
#include "glm/glm.hpp"

class CubeApp : public VulkanAppBase
{
public:
  CubeApp() : VulkanAppBase() { }

  virtual void prepare() override;
  virtual void cleanup() override;

  virtual void makeCommand(VkCommandBuffer command) override;

  struct CubeVertex
  {
    glm::vec3 pos;
    glm::vec3 color;
    glm::vec2 uv;
  };
private:
  struct BufferObject
  {
    VkBuffer buffer;
    VkDeviceMemory  memory;
  };
  struct TextureObject
  {
    VkImage image;
    VkDeviceMemory memory;
    VkImageView view;
  };
  struct ShaderParameters
  {
    glm::mat4 mtxWorld;
    glm::mat4 mtxView;
    glm::mat4 mtxProj;
  };
  void makeCubeGeometry();
  void prepareUniformBuffers();
  void prepareDescriptorSetLayout();
  void prepareDescriptorPool();
  void prepareDescriptorSet();

  BufferObject createBuffer(uint32_t size, VkBufferUsageFlags usage, VkMemoryPropertyFlags flags = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT);
  VkPipelineShaderStageCreateInfo loadShaderModule(const char* fileName, VkShaderStageFlagBits stage);
  VkSampler createSampler();
  TextureObject createTexture(const char* fileName);
  void setImageMemoryBarrier( VkCommandBuffer command, VkImage image, VkImageLayout oldLayout, VkImageLayout newLayout);

  BufferObject m_vertexBuffer;
  BufferObject m_indexBuffer;
  std::vector<BufferObject> m_uniformBuffers;
  TextureObject m_texture;

  VkDescriptorSetLayout m_descriptorSetLayout;
  VkDescriptorPool  m_descriptorPool;
  std::vector<VkDescriptorSet> m_descriptorSet;

  VkSampler m_sampler;

  VkPipelineLayout m_pipelineLayout;
  VkPipeline   m_pipeline;
  uint32_t m_indexCount;
};