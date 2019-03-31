#pragma once
#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#define VK_USE_PLATFORM_WIN32_KHR
#define GLFW_INCLUDE_VULKAN
#define GLFW_EXPOSE_NATIVE_WIN32
#include <GLFW/glfw3.h>
#include <GLFW/glfw3native.h>
#include <vulkan/vk_layer.h>
#include <vulkan/vulkan_win32.h>

#include <vector>

class VulkanAppBase
{
public:
  VulkanAppBase();
  virtual ~VulkanAppBase() { }
  void initialize(GLFWwindow* window, const char* appName);
  void terminate();

  virtual void render();

  virtual void prepare() { }
  virtual void cleanup() { }
  virtual void makeCommand(VkCommandBuffer command) { }
protected:
  static void checkResult(VkResult);

  void initializeInstance(const char* appName);
  void selectPhysicalDevice();
  uint32_t searchGraphicsQueueIndex();
  void createDevice();
  void prepareCommandPool();
  void selectSurfaceFormat(VkFormat format);
  void createSwapchain(GLFWwindow* window);
  void createDepthBuffer();
  void createViews();

  void createRenderPass();
  void createFramebuffer();

  void prepareCommandBuffers();
  void prepareSemaphores();

  uint32_t getMemoryTypeIndex(uint32_t requestBits, VkMemoryPropertyFlags requestProps)const;
  
  void enableDebugReport();
  void disableDebugReport();


  VkInstance  m_instance;
  VkDevice    m_device;
  VkPhysicalDevice  m_physDev;

  VkSurfaceKHR        m_surface;
  VkSurfaceFormatKHR  m_surfaceFormat;
  VkSurfaceCapabilitiesKHR  m_surfaceCaps;

  VkPhysicalDeviceMemoryProperties m_physMemProps;

  uint32_t m_graphicsQueueIndex;
  VkQueue m_deviceQueue;

  VkCommandPool m_commandPool;
  VkPresentModeKHR m_presentMode;
  VkSwapchainKHR  m_swapchain;
  VkExtent2D    m_swapchainExtent;
  std::vector<VkImage> m_swapchainImages;
  std::vector<VkImageView> m_swapchainViews;

  VkImage         m_depthBuffer;
  VkDeviceMemory  m_depthBufferMemory;
  VkImageView     m_depthBufferView;

  VkRenderPass      m_renderPass;
  std::vector<VkFramebuffer>    m_framebuffers;

  std::vector<VkFence>          m_fences;
  VkSemaphore   m_renderCompletedSem, m_presentCompletedSem;

  // デバッグレポート関連
  PFN_vkCreateDebugReportCallbackEXT	m_vkCreateDebugReportCallbackEXT;
  PFN_vkDebugReportMessageEXT	m_vkDebugReportMessageEXT;
  PFN_vkDestroyDebugReportCallbackEXT m_vkDestroyDebugReportCallbackEXT;
  VkDebugReportCallbackEXT  m_debugReport;

  std::vector<VkCommandBuffer> m_commands;

  uint32_t  m_imageIndex;
};
