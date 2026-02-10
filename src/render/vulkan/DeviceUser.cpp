#include "DeviceUser.hpp"
#include <vulkan/vulkan_core.h>

IDeviceUser::IDeviceUser(WP<CHyprVulkanDevice> device) : m_device(device) {}

VkDevice IDeviceUser::vkDevice() {
    return m_device.valid() ? m_device->vkDevice() : VK_NULL_HANDLE;
}
