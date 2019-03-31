#pragma once

#include "../common/vkappbase.h"
#include "glm/glm.hpp"

class TriangleApp : public VulkanAppBase
{
public:
  TriangleApp() : VulkanAppBase() { }

  virtual void prepare() override;
  virtual void cleanup() override;

  virtual void makeCommand(VkCommandBuffer command) override;

  struct Vertex
  {
    glm::vec3 pos;
    glm::vec3 color;
  };
private:
  struct BufferObject
  {
    VkBuffer buffer;
    VkDeviceMemory  memory;
  };
  BufferObject createBuffer(uint32_t size, VkBufferUsageFlags usage);
  VkPipelineShaderStageCreateInfo loadShaderModule(const char* fileName, VkShaderStageFlagBits stage);
  

  BufferObject m_vertexBuffer;
  BufferObject m_indexBuffer;

  VkPipelineLayout m_pipelineLayout;
  VkPipeline   m_pipeline;
  uint32_t m_indexCount;
};