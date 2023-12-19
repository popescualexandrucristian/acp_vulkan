#pragma once
#include <vulkan/vulkan.h>
#include <vma/vk_mem_alloc.h>
#include <functional>

namespace acp_vulkan
{
	struct frame_sync
	{
		VkFence render_fence;
		VkSemaphore render_semaphore;
		VkSemaphore present_semaphore;
	};

	struct swapchain;

	struct renderer_context
	{
		VkInstance instance;
		VkAllocationCallbacks* host_allocator;
		PFN_vkDebugReportCallbackEXT debug_callback;
		VkDebugReportCallbackEXT callback_extension;
		VkPhysicalDevice physical_device;
		uint32_t graphics_family_index;
		VkDevice logical_device;
		VkSurfaceKHR surface;
		VkFormat swapchain_format;
		VkFormat depth_format;
		swapchain* swapchain;
		VmaAllocator gpu_allocator;

		std::vector<frame_sync> frame_syncs;
		size_t max_frames;
		size_t current_frame;
		uint32_t next_image_index;

		VkQueue graphics_queue;

		std::vector<VkCommandPool> imediate_commands_pools;
		std::vector<VkFence> imediate_commands_fences;

		bool vsync_state;
		bool depth_state;
		uint32_t width;
		uint32_t height;

		struct user_context_data
		{
			void* user_data{ nullptr };
			struct resize_context
			{
				const uint32_t width{ 0 };
				const uint32_t height{ 0 };
				const bool use_vsync{ false };
				const bool use_depth{ false };
			};
			const resize_context(*renderer_resize)(renderer_context* context, uint32_t new_width, uint32_t new_height);
			bool (*renderer_update)(renderer_context* context, size_t current_frame, VkRenderingAttachmentInfo color_attachment, VkRenderingAttachmentInfo depth_attachment, double delta_time_in_seconds);
			bool (*renderer_init)(renderer_context* context);
			void (*renderer_shutdown)(renderer_context* context);
		} user_context;
	};

	struct renderer_init_context
	{
		const uint32_t width{ 0 };
		const uint32_t height{ 0 };
		const bool use_vsync{ false };
		const bool use_depth{ false };
		const bool use_validation{ false };;
		const bool use_synchronization_validation{ false };
		const renderer_context::user_context_data user_context;
	};
	renderer_context* renderer_init(const renderer_init_context& init_context);
	bool renderer_resize(renderer_context* context, uint32_t width, uint32_t height);
	bool renderer_update(acp_vulkan::renderer_context* context, double delta_time);
	void renderer_start_main_pass(VkCommandBuffer command_buffer, acp_vulkan::renderer_context* context, VkRenderingAttachmentInfo color_attachment, VkRenderingAttachmentInfo depth_attachment);
	void renderer_end_main_pass(VkCommandBuffer command_buffer, acp_vulkan::renderer_context* context);

	void immediate_submit(renderer_context* context, std::function<void(VkCommandBuffer cmd)>&& function);
	uint32_t acquire_next_image(swapchain* swapchain, renderer_context* context, VkFence fence, VkSemaphore semaphore);

	void renderer_shutdown(renderer_context*);
}

//os interface

PFN_vkDebugReportCallbackEXT acp_vulkan_os_specific_get_log_callback();

const char** acp_vulkan_os_specific_get_renderer_debug_layers();

const char** acp_vulkan_os_specific_get_renderer_extensions();

bool acp_vulkan_os_specific_get_renderer_device_supports_presentation(VkPhysicalDevice physical_device, uint32_t family_index);

VkSurfaceKHR acp_vulkan_os_specific_create_renderer_surface(VkInstance instance);
void acp_vulkan_os_specific_destroy_renderer_surface(VkSurfaceKHR surface, VkInstance instance);

void acp_vulkan_os_specific_log(const char* format, ...);