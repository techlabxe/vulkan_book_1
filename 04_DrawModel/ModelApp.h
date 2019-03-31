#pragma once

#include "../common/vkappbase.h"
#include "glm/glm.hpp"
#include "GLTFSDK/GLTF.h"

namespace Microsoft
{
  namespace glTF
  {
    class Document;
    class GLTFResourceReader;
  }
}

class ModelApp : public VulkanAppBase
{
public:
  ModelApp() : VulkanAppBase() { }

  virtual void prepare() override;
  virtual void cleanup() override;

  virtual void makeCommand(VkCommandBuffer command) override;

  struct Vertex
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

  struct ModelMesh
  {
    BufferObject vertexBuffer;
    BufferObject indexBuffer;
    uint32_t vertexCount;
    uint32_t indexCount;

    int materialIndex;

    std::vector<VkDescriptorSet> descriptorSet;
  };
  struct Material
  {
    TextureObject texture;
    Microsoft::glTF::AlphaMode alphaMode;
  };
  struct Model
  {
    std::vector<ModelMesh> meshes;
    std::vector<Material> materials;
  };
  
  void makeModelGeometry(const Microsoft::glTF::Document&, std::shared_ptr<Microsoft::glTF::GLTFResourceReader> reader);
  void makeModelMaterial(const Microsoft::glTF::Document&, std::shared_ptr<Microsoft::glTF::GLTFResourceReader> reader);

  void prepareUniformBuffers();
  void prepareDescriptorSetLayout();
  void prepareDescriptorPool();
  void prepareDescriptorSet();

  BufferObject createBuffer(uint32_t size, VkBufferUsageFlags usage, VkMemoryPropertyFlags flags, const void* initialData);
  VkPipelineShaderStageCreateInfo loadShaderModule(const char* fileName, VkShaderStageFlagBits stage);
  VkSampler createSampler();
  TextureObject createTextureFromMemory(const std::vector<char>& imageData);
  void setImageMemoryBarrier( VkCommandBuffer command, VkImage image, VkImageLayout oldLayout, VkImageLayout newLayout);

  Model m_model;

  std::vector<BufferObject> m_uniformBuffers;

  VkDescriptorSetLayout m_descriptorSetLayout;
  VkDescriptorPool  m_descriptorPool;

  VkSampler m_sampler;

  VkPipelineLayout m_pipelineLayout;
  VkPipeline  m_pipelineOpaque;
  VkPipeline  m_pipelineAlpha;
};