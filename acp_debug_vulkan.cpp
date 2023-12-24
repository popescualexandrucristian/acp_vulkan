#include "acp_debug_vulkan.h"
#include <inttypes.h>
#include <string.h>

static PFN_vkSetDebugUtilsObjectNameEXT		local_vkSetDebugUtilsObjectNameEXT{ nullptr };
static PFN_vkSetDebugUtilsObjectTagEXT		local_vkSetDebugUtilsObjectTagEXT{ nullptr };
static PFN_vkQueueBeginDebugUtilsLabelEXT	local_vkQueueBeginDebugUtilsLabelEXT{ nullptr };
static PFN_vkQueueEndDebugUtilsLabelEXT		local_vkQueueEndDebugUtilsLabelEXT{ nullptr };
static PFN_vkQueueInsertDebugUtilsLabelEXT	local_vkQueueInsertDebugUtilsLabelEXT{ nullptr };
static PFN_vkCmdBeginDebugUtilsLabelEXT		local_vkCmdBeginDebugUtilsLabelEXT{ nullptr };
static PFN_vkCmdEndDebugUtilsLabelEXT		local_vkCmdEndDebugUtilsLabelEXT{ nullptr };
static PFN_vkCmdInsertDebugUtilsLabelEXT	local_vkCmdInsertDebugUtilsLabelEXT{ nullptr };

void acp_vulkan::debug_init(VkDevice device)
{
#ifndef ENABLE_VULKAN_DEBUG_MARKERS
	(void)device;
	return;
#else
	local_vkSetDebugUtilsObjectNameEXT = (PFN_vkSetDebugUtilsObjectNameEXT)vkGetDeviceProcAddr(device, "vkSetDebugUtilsObjectNameEXT");
	local_vkSetDebugUtilsObjectTagEXT = (PFN_vkSetDebugUtilsObjectTagEXT)vkGetDeviceProcAddr(device, "vkSetDebugUtilsObjectTagEXT");
	local_vkQueueBeginDebugUtilsLabelEXT = (PFN_vkQueueBeginDebugUtilsLabelEXT)vkGetDeviceProcAddr(device, "vkQueueBeginDebugUtilsLabelEXT");
	local_vkQueueEndDebugUtilsLabelEXT = (PFN_vkQueueEndDebugUtilsLabelEXT)vkGetDeviceProcAddr(device, "vkQueueEndDebugUtilsLabelEXT");
	local_vkQueueInsertDebugUtilsLabelEXT = (PFN_vkQueueInsertDebugUtilsLabelEXT)vkGetDeviceProcAddr(device, "vkQueueInsertDebugUtilsLabelEXT");
	local_vkCmdBeginDebugUtilsLabelEXT = (PFN_vkCmdBeginDebugUtilsLabelEXT)vkGetDeviceProcAddr(device, "vkCmdBeginDebugUtilsLabelEXT");
	local_vkCmdEndDebugUtilsLabelEXT = (PFN_vkCmdEndDebugUtilsLabelEXT)vkGetDeviceProcAddr(device, "vkCmdEndDebugUtilsLabelEXT");
	local_vkCmdInsertDebugUtilsLabelEXT = (PFN_vkCmdInsertDebugUtilsLabelEXT)vkGetDeviceProcAddr(device, "vkCmdInsertDebugUtilsLabelEXT");
#endif
}

void acp_vulkan::debug_set_object_name(VkDevice device, void* object, VkObjectType object_type, const char* name)
{
	if (!local_vkSetDebugUtilsObjectNameEXT)
		return;

	VkDebugUtilsObjectNameInfoEXT name_info{};
	name_info.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT;
	name_info.pObjectName = name;
	name_info.objectHandle = reinterpret_cast<uint64_t>(object);
	name_info.objectType = object_type;
	local_vkSetDebugUtilsObjectNameEXT(device, &name_info);
}

void acp_vulkan::debug_set_object_tag(VkDevice device, void* object, VkObjectType object_type, uint64_t name, size_t tag_size, const void* tag)
{
	if (!local_vkSetDebugUtilsObjectTagEXT)
		return;

	VkDebugUtilsObjectTagInfoEXT tag_info{};
	tag_info.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_TAG_INFO_EXT;
	tag_info.tagName = name;
	tag_info.tagSize = tag_size;
	tag_info.pTag = tag;
	tag_info.objectHandle = reinterpret_cast<uint64_t>(object);
	tag_info.objectType = object_type;
	local_vkSetDebugUtilsObjectTagEXT(device, &tag_info);
}

void acp_vulkan::debug_begin_region(VkQueue queue, const char* marker_name, acp_vulkan::debug_color color)
{
	if (!local_vkQueueBeginDebugUtilsLabelEXT)
		return;

	VkDebugUtilsLabelEXT queue_info{};
	queue_info.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_LABEL_EXT;
	queue_info.pLabelName = marker_name;
	memcpy(queue_info.color, &color, sizeof(acp_vulkan::debug_color));

	local_vkQueueBeginDebugUtilsLabelEXT(queue, &queue_info);
}

void acp_vulkan::debug_end_region(VkQueue queue)
{
	if (!local_vkQueueEndDebugUtilsLabelEXT)
		return;

	local_vkQueueEndDebugUtilsLabelEXT(queue);
}

void acp_vulkan::debug_insert(VkQueue queue, const char* marker_name, acp_vulkan::debug_color color)
{
	if (!local_vkQueueInsertDebugUtilsLabelEXT)
		return;

	VkDebugUtilsLabelEXT queue_info{};
	queue_info.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_LABEL_EXT;
	queue_info.pLabelName = marker_name;
	memcpy(queue_info.color, &color, sizeof(acp_vulkan::debug_color));

	local_vkQueueInsertDebugUtilsLabelEXT(queue, &queue_info);
}

void acp_vulkan::debug_begin_region(VkCommandBuffer command_buffer, const char* marker_name, acp_vulkan::debug_color color)
{
	if (!local_vkCmdBeginDebugUtilsLabelEXT)
		return;

	VkDebugUtilsLabelEXT cmd_info{};
	cmd_info.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_LABEL_EXT;
	cmd_info.pLabelName = marker_name;
	memcpy(cmd_info.color, &color, sizeof(acp_vulkan::debug_color));

	local_vkCmdBeginDebugUtilsLabelEXT(command_buffer, &cmd_info);
}

void acp_vulkan::debug_end_region(VkCommandBuffer command_buffer)
{
	if (!local_vkCmdEndDebugUtilsLabelEXT)
		return;

	local_vkCmdEndDebugUtilsLabelEXT(command_buffer);
}

void acp_vulkan::debug_insert(VkCommandBuffer command_buffer, const char* marker_name, acp_vulkan::debug_color color)
{
	if (!local_vkCmdInsertDebugUtilsLabelEXT)
		return;

	VkDebugUtilsLabelEXT cmd_info{};
	cmd_info.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_LABEL_EXT;
	cmd_info.pLabelName = marker_name;
	memcpy(cmd_info.color, &color, sizeof(acp_vulkan::debug_color));

	local_vkCmdInsertDebugUtilsLabelEXT(command_buffer, &cmd_info);
}