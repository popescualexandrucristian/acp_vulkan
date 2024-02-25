#include <acp_context/acp_vulkan_context.h>
#include <version.h>
#include <stdio.h>
#include <vector>
#include <log.h>
#include <acp_context/acp_vulkan_context_swapchain.h>
#include <acp_context/acp_vulkan_context_utils.h>
#include <acp_program_vulkan.h>

#ifdef ENABLE_VULKAN_DEBUG_MARKERS
#include <acp_debug_vulkan.h>
#endif

#define VMA_STATIC_VULKAN_FUNCTIONS 1
#define VMA_DYNAMIC_VULKAN_FUNCTIONS 0
#define VMA_IMPLEMENTATION
#pragma warning( push )
#pragma warning( disable : 4100 )
#pragma warning( disable : 4127 )
#pragma warning( disable : 4189 )
#include <vma/vk_mem_alloc.h>
#pragma warning( pop )

static bool register_debug_callback(acp_vulkan::renderer_context* context)
{
	if (!context->debug_callback)
		return false;

	VkDebugReportCallbackCreateInfoEXT create_info = { VK_STRUCTURE_TYPE_DEBUG_REPORT_CREATE_INFO_EXT };
	create_info.flags = VK_DEBUG_REPORT_WARNING_BIT_EXT | VK_DEBUG_REPORT_PERFORMANCE_WARNING_BIT_EXT | VK_DEBUG_REPORT_ERROR_BIT_EXT;
	create_info.pfnCallback = context->debug_callback;
	
	PFN_vkCreateDebugReportCallbackEXT create = reinterpret_cast<PFN_vkCreateDebugReportCallbackEXT>(vkGetInstanceProcAddr(context->instance, "vkCreateDebugReportCallbackEXT"));

	ACP_VK_CHECK(create(context->instance, &create_info, 0, &context->callback_extension), context);
	return context->callback_extension != VK_NULL_HANDLE;
}

static void unregister_debug_callback(acp_vulkan::renderer_context* context)
{
	if (context->callback_extension == VK_NULL_HANDLE)
		return;

	PFN_vkDestroyDebugReportCallbackEXT destroy = reinterpret_cast<PFN_vkDestroyDebugReportCallbackEXT>(vkGetInstanceProcAddr(context->instance, "vkDestroyDebugReportCallbackEXT"));
	destroy(context->instance, context->callback_extension, context->host_allocator);
	context->callback_extension = VK_NULL_HANDLE;
}

static bool create_instance(acp_vulkan::renderer_context* context, bool use_validation, bool use_synchronization_validation)
{
	VkInstanceCreateInfo instance_create_info{ VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO };
	VkApplicationInfo application_info{ VK_STRUCTURE_TYPE_APPLICATION_INFO };
	application_info.apiVersion = VK_API_VERSION_1_3;
	application_info.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
	application_info.engineVersion = VK_MAKE_VERSION(1, 0, 0);
	application_info.pApplicationName = PROJECT_NAME " " PROJECT_VERSION;
	application_info.pEngineName = PROJECT_NAME " " PROJECT_VERSION;
	instance_create_info.pApplicationInfo = &application_info;

	uint32_t debug_layer_count = 0;
	const char** debug_layers = acp_vulkan_os_specific_get_renderer_debug_layers();
	if (debug_layers)
	{
		instance_create_info.ppEnabledLayerNames = debug_layers;
		while (debug_layers != nullptr && *debug_layers)
		{
			++debug_layers;
			++debug_layer_count;
		}

		instance_create_info.enabledLayerCount = debug_layer_count;
	}

	VkValidationFeaturesEXT validationFeatures = { VK_STRUCTURE_TYPE_VALIDATION_FEATURES_EXT };
	VkValidationFeatureEnableEXT validationFeaturesAndSyncExt[] =
	{
		VK_VALIDATION_FEATURE_ENABLE_BEST_PRACTICES_EXT,
		VK_VALIDATION_FEATURE_ENABLE_SYNCHRONIZATION_VALIDATION_EXT
	};
	VkValidationFeatureEnableEXT validationFeaturesExt[] =
	{
		VK_VALIDATION_FEATURE_ENABLE_BEST_PRACTICES_EXT
	};
	
	if (use_validation)
	{
		if (use_synchronization_validation)
		{
			validationFeatures.enabledValidationFeatureCount = 2;
			validationFeatures.pEnabledValidationFeatures = validationFeaturesAndSyncExt;
		}
		else
		{
			validationFeatures.enabledValidationFeatureCount = 1;
			validationFeatures.pEnabledValidationFeatures = validationFeaturesExt;
		}
		
		instance_create_info.pNext = &validationFeatures;
	}

	const char** renderer_extensions = acp_vulkan_os_specific_get_renderer_extensions();
	instance_create_info.ppEnabledExtensionNames = renderer_extensions;
	uint32_t renderer_extensions_count = 0;
	while (renderer_extensions != nullptr && *renderer_extensions)
	{
		++renderer_extensions;
		++renderer_extensions_count;
	}
	instance_create_info.enabledExtensionCount = renderer_extensions_count;

	ACP_VK_CHECK(vkCreateInstance(&instance_create_info, context->host_allocator, &context->instance), context);

	return context->instance != VK_NULL_HANDLE;
}

static uint32_t get_queue_family_index(VkPhysicalDevice physical_device, VkQueueFlagBits type, std::initializer_list<uint32_t> ignore_queues)
{
	uint32_t queue_count = 0;
	vkGetPhysicalDeviceQueueFamilyProperties(physical_device, &queue_count, 0);

	std::vector<VkQueueFamilyProperties> queues(queue_count);
	vkGetPhysicalDeviceQueueFamilyProperties(physical_device, &queue_count, queues.data());

	for (uint32_t i = 0; i < queue_count; ++i)
	{
		if(std::find(ignore_queues.begin(), ignore_queues.end(), i) != ignore_queues.end())
			continue;

		if (queues[i].queueFlags & type)
			return i;
	}

	return VK_QUEUE_FAMILY_IGNORED;
}

static bool pick_physical_device(acp_vulkan::renderer_context* context, VkPhysicalDevice* physical_devices, uint32_t physical_devices_count)
{
	VkPhysicalDevice preferred = 0;
	uint32_t preferred_graphics_family_index = 0;
	uint32_t preferred_compute_family_index = 0;
	VkPhysicalDevice fallback = 0;
	uint32_t fallback_graphics_family_index = 0;
	uint32_t fallback_compute_family_index = 0;

	for (uint32_t i = 0; i < physical_devices_count; ++i)
	{
		VkPhysicalDeviceProperties props;
		vkGetPhysicalDeviceProperties(physical_devices[i], &props);

		uint32_t graphics_family_index = get_queue_family_index(physical_devices[i], VK_QUEUE_GRAPHICS_BIT, {});
		if (graphics_family_index == VK_QUEUE_FAMILY_IGNORED)
			continue;

		uint32_t compute_family_index = get_queue_family_index(physical_devices[i], VK_QUEUE_COMPUTE_BIT, { graphics_family_index, fallback ? fallback_graphics_family_index : UINT32_MAX });
		if (compute_family_index == VK_QUEUE_FAMILY_IGNORED)
			compute_family_index = get_queue_family_index(physical_devices[i], VK_QUEUE_COMPUTE_BIT, {});
		
		if (compute_family_index == VK_QUEUE_FAMILY_IGNORED)
			continue;

		if (!acp_vulkan_os_specific_get_renderer_device_supports_presentation(physical_devices[i], graphics_family_index))
			continue;

		if (props.apiVersion < VK_API_VERSION_1_3)
			continue;

		if (!preferred && props.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU)
		{
			preferred = physical_devices[i];
			preferred_graphics_family_index = graphics_family_index;
			preferred_compute_family_index = compute_family_index;
		}

		if (!fallback)
		{
			fallback = physical_devices[i];
			fallback_graphics_family_index = graphics_family_index;
			fallback_compute_family_index = compute_family_index;
		}
	}

	if (!fallback && !preferred)
		return false;

	context->physical_device = preferred ? preferred : fallback;
	context->graphics_family_index = preferred ? preferred_graphics_family_index : fallback_graphics_family_index;
	context->compute_family_index = preferred ? preferred_compute_family_index : fallback_compute_family_index;

	VkPhysicalDeviceProperties props;
	vkGetPhysicalDeviceProperties(context->physical_device, &props);
	return true;
}

static bool select_physical_device(acp_vulkan::renderer_context* context)
{
	VkPhysicalDevice physical_devices[16] = {};
	uint32_t physical_device_count = sizeof(physical_devices) / sizeof(physical_devices[0]);
	ACP_VK_CHECK(vkEnumeratePhysicalDevices(context->instance, &physical_device_count, physical_devices), context);

	if (!pick_physical_device(context, physical_devices, physical_device_count))
		return false;

	return true;
}

static bool create_logical_device(acp_vulkan::renderer_context* context)
{
	float queuePriorities[] = { 1.0f };

	VkDeviceQueueCreateInfo graphics_queue_info = { VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO };
	graphics_queue_info.queueFamilyIndex = context->graphics_family_index;
	graphics_queue_info.queueCount = 1;
	graphics_queue_info.pQueuePriorities = queuePriorities;

	VkDeviceQueueCreateInfo compute_queue_info = { VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO };
	compute_queue_info.queueFamilyIndex = context->compute_family_index;
	compute_queue_info.queueCount = 1;
	compute_queue_info.pQueuePriorities = queuePriorities;

	const char* extensions[] =
	{
		VK_KHR_SWAPCHAIN_EXTENSION_NAME,
	};

	VkPhysicalDeviceFeatures2 features = { VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2 };
	features.features.multiDrawIndirect = true;
	//features.features.pipelineStatisticsQuery = true;
	features.features.shaderInt16 = true;
	features.features.shaderInt64 = true;

	VkPhysicalDeviceVulkan11Features features11 = { VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_1_FEATURES };
	features11.storageBuffer16BitAccess = true;
	features11.shaderDrawParameters = true;

	VkPhysicalDeviceVulkan12Features features12 = { VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES };
	features12.drawIndirectCount = true;
	features12.storageBuffer8BitAccess = true;
	features12.uniformAndStorageBuffer8BitAccess = true;
	features12.shaderFloat16 = true;
	features12.shaderInt8 = true;
	features12.samplerFilterMinmax = true;
	features12.scalarBlockLayout = true;

	VkPhysicalDeviceVulkan13Features features13 = { VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES };
	features13.dynamicRendering = true;
	features13.synchronization2 = true;

	VkDeviceCreateInfo create_info = { VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO };
	VkDeviceQueueCreateInfo queue_info[] = {graphics_queue_info, compute_queue_info};
	if (graphics_queue_info.queueFamilyIndex != compute_queue_info.queueFamilyIndex)
	{
		create_info.queueCreateInfoCount = 2;
		create_info.pQueueCreateInfos = queue_info;
	}
	else
	{
		create_info.queueCreateInfoCount = 1;
		create_info.pQueueCreateInfos = &graphics_queue_info;
	}

	create_info.ppEnabledExtensionNames = extensions;
	create_info.enabledExtensionCount = sizeof(extensions)/sizeof(extensions[0]);

	create_info.pNext = &features;
	features.pNext = &features11;
	features11.pNext = &features12;
	features12.pNext = &features13;

	ACP_VK_CHECK(vkCreateDevice(context->physical_device, &create_info, 0, &context->logical_device), context);

#ifdef ENABLE_VULKAN_DEBUG_MARKERS
	acp_vulkan::debug_init(context->logical_device);
#endif

	return context->logical_device != VK_NULL_HANDLE;
}

static VkFormat select_format_from_available(acp_vulkan::renderer_context* context, const std::initializer_list<VkFormat>& candidates, VkImageTiling tiling, VkFormatFeatureFlags features) {
	for (VkFormat format : candidates) {
		VkFormatProperties props;
		vkGetPhysicalDeviceFormatProperties(context->physical_device, format, &props);

		if (tiling == VK_IMAGE_TILING_LINEAR && (props.linearTilingFeatures & features) == features) {
			return format;
		}
		else if (tiling == VK_IMAGE_TILING_OPTIMAL && (props.optimalTilingFeatures & features) == features) {
			return format;
		}
	}

	return VK_FORMAT_UNDEFINED;
}

static bool select_swapchain_format(acp_vulkan::renderer_context* context)
{
	context->swapchain_format = VK_FORMAT_UNDEFINED;
	context->depth_format = VK_FORMAT_UNDEFINED;

	uint32_t format_count = 0;
	ACP_VK_CHECK(vkGetPhysicalDeviceSurfaceFormatsKHR(context->physical_device, context->surface, &format_count, 0), context);
	if (format_count == 0)
		return false;

	std::vector<VkSurfaceFormatKHR> formats(format_count);
	ACP_VK_CHECK(vkGetPhysicalDeviceSurfaceFormatsKHR(context->physical_device, context->surface, &format_count, formats.data()), context);

	if (format_count == 1 && formats[0].format == VK_FORMAT_UNDEFINED)
	{
		context->swapchain_format = VK_FORMAT_R8G8B8A8_UNORM;
	}
	else
	{
		for (uint32_t i = 0; i < format_count; ++i)
			if (formats[i].format == VK_FORMAT_R8G8B8A8_UNORM || formats[i].format == VK_FORMAT_B8G8R8A8_UNORM)
			{
				context->swapchain_format = formats[i].format;
			}
	}

	if (context->swapchain_format == VK_FORMAT_UNDEFINED)
		context->swapchain_format = formats[0].format;

	context->depth_format = select_format_from_available(context, { VK_FORMAT_D32_SFLOAT, VK_FORMAT_D32_SFLOAT_S8_UINT, VK_FORMAT_D24_UNORM_S8_UINT },
		VK_IMAGE_TILING_OPTIMAL,
		VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT);

	return true;
}

static void create_frame_sync_data(acp_vulkan::renderer_context* context)
{
	context->frame_syncs.clear();
	for (size_t i = 0; i < context->swapchain->images.size(); ++i)
	{
		acp_vulkan::frame_sync sync{};
		sync.present_semaphore = acp_vulkan::semaphore_create(context, "present_semaphore");
		sync.render_semaphore = acp_vulkan::semaphore_create(context, "render_semaphore");
		sync.render_fence = acp_vulkan::fence_create(context, true, "render_fence");

		context->frame_syncs.emplace_back(std::move(sync));
	}

	context->compute_frame_syncs.clear();
	for (size_t i = 0; i < context->swapchain->images.size(); ++i)
	{
		acp_vulkan::compute_frame_sync sync{};
		sync.compute_semaphore = acp_vulkan::semaphore_create(context, "compute_semaphore");
		sync.compute_fence = acp_vulkan::fence_create(context, true, "compute_fence");

		context->compute_frame_syncs.emplace_back(std::move(sync));
	}
}

static void destroy_frame_sync_data(acp_vulkan::renderer_context* context)
{
	for (size_t i = 0; i < context->frame_syncs.size(); ++i)
	{
		acp_vulkan::semaphore_destroy(context, context->frame_syncs[i].present_semaphore);
		acp_vulkan::semaphore_destroy(context, context->frame_syncs[i].render_semaphore);
		acp_vulkan::fence_destroy(context, context->frame_syncs[i].render_fence);
	}
	context->frame_syncs.clear();

	for (size_t i = 0; i < context->compute_frame_syncs.size(); ++i)
	{
		acp_vulkan::semaphore_destroy(context, context->compute_frame_syncs[i].compute_semaphore);
		acp_vulkan::fence_destroy(context, context->compute_frame_syncs[i].compute_fence);
	}
	context->compute_frame_syncs.clear();
}

acp_vulkan::renderer_context* acp_vulkan::renderer_init(const renderer_init_context& init_context)
{
	acp_vulkan::renderer_context* out = new acp_vulkan::renderer_context();
	out->debug_callback = acp_vulkan_os_specific_get_log_callback();
	out->user_context = init_context.user_context;
	out->depth_state = init_context.use_depth;
	out->vsync_state = init_context.use_vsync;
	out->width = init_context.width;
	out->height = init_context.height;
	out->is_minimized = false;

	if (!create_instance(out, init_context.use_validation, init_context.use_synchronization_validation))
		goto ERROR;

	if (out->debug_callback && init_context.use_validation)
	{
		if (!register_debug_callback(out))
			goto ERROR;
	}

	if (!select_physical_device(out))
		goto ERROR;

	if (!create_logical_device(out))
		goto ERROR;

	vkGetDeviceQueue(out->logical_device, out->graphics_family_index, 0, &out->graphics_queue);
	vkGetDeviceQueue(out->logical_device, out->compute_family_index, 0, &out->compute_queue);

	out->surface = acp_vulkan_os_specific_create_renderer_surface(out->instance);
	if (out->surface == VK_NULL_HANDLE)
		goto ERROR;

	{
		VkBool32 present_supported = 0;
		ACP_VK_CHECK(vkGetPhysicalDeviceSurfaceSupportKHR(out->physical_device, out->graphics_family_index, out->surface, &present_supported), out);
		if (present_supported == 0)
			goto ERROR;
	}

	if (!select_swapchain_format(out))
		goto ERROR;

	{
		VmaAllocatorCreateInfo info = {};
		info.vulkanApiVersion = VK_API_VERSION_1_3;
		info.physicalDevice = out->physical_device;
		info.device = out->logical_device;
		info.instance = out->instance;
		out->gpu_allocator = {};
		ACP_VK_CHECK(vmaCreateAllocator(&info, &out->gpu_allocator), out);
	}

	out->swapchain = swapchain_init(out, out->width, out->height, out->vsync_state, out->depth_state);
	if (!out->swapchain)
		goto ERROR;

	create_frame_sync_data(out);
	out->current_frame = 0;
	out->max_frames = out->frame_syncs.size();

	//todo(alex) : Maybe also think about using the transfer queue, maybe it is different idk.
	out->imediate_commands_pools.push_back(commands_pool_crate(out, out->graphics_family_index, "imediate_commands_pool"));
	out->imediate_commands_fences.push_back(fence_create(out, false, "imediate_commands_fences"));

	if (!out->user_context.renderer_init(out))
		goto ERROR;

	return out;

ERROR:
	delete out;
	return nullptr;
}

bool acp_vulkan::renderer_resize(acp_vulkan::renderer_context* context, uint32_t width, uint32_t height)
{
	context->is_minimized = false;

	if (width == 0 || height == 0)
	{
		context->is_minimized = true;
		context->swapchain->width = 0;
		context->swapchain->height = 0;
		return true;
	}

	acp_vulkan::renderer_context::user_context_data::resize_context resize_context = context->user_context.renderer_resize(context, width, height);
	if (resize_context.width == 0 && 
		resize_context.height == 0 && 
		resize_context.use_depth == false && 
		resize_context.use_vsync == false)
		return false;

	if (resize_context.width == 0 || resize_context.height == 0)
	{
		context->is_minimized = true;
		return true;
	}

	if (swapchian_update(context->swapchain, context, resize_context.width, resize_context.height, resize_context.use_vsync, resize_context.use_depth))
	{
		assert(context->swapchain->images.size() == context->frame_syncs.size());
		context->current_frame = 0;
		context->next_image_index = 0;
		return true;
	}
	return false;
}

bool acp_vulkan::renderer_update(acp_vulkan::renderer_context* context, double delta_time)
{
	if (context->is_minimized)
		return true;

	size_t current_frame = context->current_frame % context->max_frames;
	acp_vulkan::frame_sync& sync = context->frame_syncs[current_frame];

	//aquire free image
	ACP_VK_CHECK(vkWaitForFences(context->logical_device, 1, &sync.render_fence, true, 1000000000), context);
	ACP_VK_CHECK(vkResetFences(context->logical_device, 1, &sync.render_fence), context);

	context->next_image_index = acp_vulkan::acquire_next_image(context->swapchain, context, VK_NULL_HANDLE, sync.present_semaphore);

	VkRenderingAttachmentInfo color_attachment = { VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO };
	color_attachment.imageView = context->swapchain->views[context->next_image_index];
	color_attachment.imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
	color_attachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
	color_attachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;

	VkRenderingAttachmentInfo depth_attachment = { VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO };
	if (context->depth_state)
	{
		depth_attachment.imageView = context->swapchain->depth_views[context->next_image_index];
		depth_attachment.imageLayout = VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL;
		depth_attachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
		depth_attachment.storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
	}

	acp_vulkan::compute_frame_sync& compute_sync = context->compute_frame_syncs[current_frame];
	vkWaitForFences(context->logical_device, 1, &compute_sync.compute_fence, VK_TRUE, UINT64_MAX);

	return context->user_context.renderer_update(context, current_frame, color_attachment, depth_attachment, delta_time);
}

void acp_vulkan::renderer_start_main_pass(VkCommandBuffer command_buffer, renderer_context* context, VkRenderingAttachmentInfo color_attachment, VkRenderingAttachmentInfo depth_attachment)
{
	size_t current_frame = context->current_frame % context->max_frames;

	VkRenderingInfo pass_info = { VK_STRUCTURE_TYPE_RENDERING_INFO };
	pass_info.renderArea.extent.width = context->swapchain->width;
	pass_info.renderArea.extent.height = context->swapchain->height;
	pass_info.layerCount = 1;
	pass_info.colorAttachmentCount = 1;
	pass_info.pColorAttachments = &color_attachment;

	VkImageMemoryBarrier2 render_begin_bariers[2] = {};

	//color_barreir
	render_begin_bariers[0] = {VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2};
	render_begin_bariers[0].pNext = nullptr;
	render_begin_bariers[0].srcStageMask = VK_PIPELINE_STAGE_2_BOTTOM_OF_PIPE_BIT;
	render_begin_bariers[0].srcAccessMask = 0;
	render_begin_bariers[0].oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	render_begin_bariers[0].dstStageMask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT;
	render_begin_bariers[0].dstAccessMask = VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT;
	render_begin_bariers[0].newLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
	render_begin_bariers[0].srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	render_begin_bariers[0].dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	render_begin_bariers[0].image = context->swapchain->images[current_frame];
	render_begin_bariers[0].subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	render_begin_bariers[0].subresourceRange.baseMipLevel = 0;
	render_begin_bariers[0].subresourceRange.levelCount = VK_REMAINING_MIP_LEVELS;
	render_begin_bariers[0].subresourceRange.layerCount = VK_REMAINING_ARRAY_LAYERS;

	if (context->depth_state)
	{
		pass_info.pDepthAttachment = context->depth_state ? &depth_attachment : nullptr;


		if (!context->swapchain->depth_images[current_frame].second)
		{
			//depth_barreir
			render_begin_bariers[1] = { VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2 };
			render_begin_bariers[1].pNext = nullptr;
			render_begin_bariers[1].srcStageMask = VK_PIPELINE_STAGE_2_NONE;
			render_begin_bariers[1].srcAccessMask = VK_ACCESS_2_NONE;
			render_begin_bariers[1].dstStageMask = VK_PIPELINE_STAGE_2_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_2_LATE_FRAGMENT_TESTS_BIT;
			render_begin_bariers[1].dstAccessMask = VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_READ_BIT | VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
			render_begin_bariers[1].oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
			render_begin_bariers[1].newLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
			render_begin_bariers[1].srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
			render_begin_bariers[1].dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
			render_begin_bariers[1].image = context->swapchain->depth_images[current_frame].first.image;
			render_begin_bariers[1].subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
			render_begin_bariers[1].subresourceRange.baseMipLevel = 0;
			render_begin_bariers[1].subresourceRange.levelCount = VK_REMAINING_MIP_LEVELS;
			render_begin_bariers[1].subresourceRange.layerCount = VK_REMAINING_ARRAY_LAYERS;

			acp_vulkan::push_pipeline_barrier(command_buffer, 0, 0, nullptr, 2, render_begin_bariers);
			context->swapchain->depth_images[current_frame].second = true;
		}
		else
		{
			acp_vulkan::push_pipeline_barrier(command_buffer, 0, 0, nullptr, 1, render_begin_bariers);
		}
	}
	else
	{
		acp_vulkan::push_pipeline_barrier(command_buffer, 0, 0, nullptr, 1, render_begin_bariers);
	}
	vkCmdBeginRendering(command_buffer, &pass_info);
}

void acp_vulkan::renderer_end_main_compute_pass(VkCommandBuffer command_buffer, acp_vulkan::renderer_context* context)
{
	size_t current_frame = context->current_frame % context->max_frames;
	acp_vulkan::compute_frame_sync& sync = context->compute_frame_syncs[current_frame];
	vkResetFences(context->logical_device, 1, &sync.compute_fence);

	VkSubmitInfo2 submit_info = { VK_STRUCTURE_TYPE_SUBMIT_INFO_2 };

	VkSemaphoreSubmitInfo signal_semaphore_info = { VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO };
	signal_semaphore_info.deviceIndex = 0;
	signal_semaphore_info.pNext = nullptr;
	signal_semaphore_info.semaphore = sync.compute_semaphore;
	signal_semaphore_info.stageMask = VK_PIPELINE_STAGE_2_BOTTOM_OF_PIPE_BIT;
	signal_semaphore_info.value = 1;

	submit_info.pSignalSemaphoreInfos = &signal_semaphore_info;
	submit_info.signalSemaphoreInfoCount = 1;

	VkCommandBufferSubmitInfo command_buffer_submit_info = { VK_STRUCTURE_TYPE_COMMAND_BUFFER_SUBMIT_INFO };
	command_buffer_submit_info.pNext = nullptr;
	command_buffer_submit_info.commandBuffer = command_buffer;
	command_buffer_submit_info.deviceMask = 0;
	submit_info.pCommandBufferInfos = &command_buffer_submit_info;
	submit_info.commandBufferInfoCount = 1;

	vkQueueSubmit2(context->compute_queue, 1, &submit_info, sync.compute_fence);
}

void acp_vulkan::renderer_end_main_pass(VkCommandBuffer command_buffer, acp_vulkan::renderer_context* context, bool wait_for_compute)
{
	size_t current_frame = context->current_frame % context->max_frames;
	acp_vulkan::frame_sync& sync = context->frame_syncs[current_frame];

	vkCmdEndRendering(command_buffer);

	VkImageMemoryBarrier2 present_color_barreir = { VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2 };

	present_color_barreir.image = context->swapchain->images[current_frame];
	present_color_barreir.srcStageMask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT;
	present_color_barreir.srcAccessMask = VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT;
	present_color_barreir.oldLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
	present_color_barreir.newLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
	present_color_barreir.dstStageMask = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
	present_color_barreir.dstAccessMask = VK_ACCESS_2_NONE;
	present_color_barreir.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	present_color_barreir.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	present_color_barreir.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	present_color_barreir.subresourceRange.baseMipLevel = 0;
	present_color_barreir.subresourceRange.levelCount = VK_REMAINING_MIP_LEVELS;
	present_color_barreir.subresourceRange.layerCount = VK_REMAINING_ARRAY_LAYERS;

	acp_vulkan::push_pipeline_barrier(command_buffer, 0, 0, nullptr, 1, &present_color_barreir);

	ACP_VK_CHECK(vkEndCommandBuffer(command_buffer), context);

	//submit

	VkSubmitInfo2 submit = {};

	submit.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO_2;
	submit.pNext = nullptr;

	acp_vulkan::compute_frame_sync& compute_sync = context->compute_frame_syncs[current_frame];

	VkSemaphoreSubmitInfo wait_semaphore_infos[2] = {}; 
	wait_semaphore_infos[0] = { VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO };
	wait_semaphore_infos[0].deviceIndex = 0;
	wait_semaphore_infos[0].pNext = nullptr;
	wait_semaphore_infos[0].semaphore = sync.present_semaphore;
	wait_semaphore_infos[0].stageMask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT;
	wait_semaphore_infos[0].value = 1;
	submit.pWaitSemaphoreInfos = wait_semaphore_infos;
	if (wait_for_compute)
	{
		wait_semaphore_infos[1] = { VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO };
		wait_semaphore_infos[1].deviceIndex = 0;
		wait_semaphore_infos[1].pNext = nullptr;
		wait_semaphore_infos[1].semaphore = compute_sync.compute_semaphore;
		wait_semaphore_infos[1].stageMask = VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT;
		wait_semaphore_infos[1].value = 1;

		submit.waitSemaphoreInfoCount = 2;
	}
	else
	{
		submit.waitSemaphoreInfoCount = 1;
	}

	VkSemaphoreSubmitInfo signal_semaphore_info = { VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO };
	signal_semaphore_info.deviceIndex = 0;
	signal_semaphore_info.pNext = nullptr;
	signal_semaphore_info.semaphore = sync.render_semaphore;
	signal_semaphore_info.stageMask = VK_PIPELINE_STAGE_2_BOTTOM_OF_PIPE_BIT;
	signal_semaphore_info.value = 1;

	submit.pSignalSemaphoreInfos = &signal_semaphore_info;
	submit.signalSemaphoreInfoCount = 1;

	VkCommandBufferSubmitInfo command_buffer_submit_info = { VK_STRUCTURE_TYPE_COMMAND_BUFFER_SUBMIT_INFO };
	command_buffer_submit_info.pNext = nullptr;
	command_buffer_submit_info.commandBuffer = command_buffer;
	command_buffer_submit_info.deviceMask = 0;
	submit.pCommandBufferInfos = &command_buffer_submit_info;
	submit.commandBufferInfoCount = 1;

	ACP_VK_CHECK(vkQueueSubmit2(context->graphics_queue, 1, &submit, sync.render_fence), context);

	//present

	VkPresentInfoKHR present_info = {};
	present_info.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
	present_info.pNext = nullptr;

	present_info.pSwapchains = &context->swapchain->swapchain;
	present_info.swapchainCount = 1;

	present_info.pWaitSemaphores = &sync.render_semaphore;
	present_info.waitSemaphoreCount = 1;

	present_info.pImageIndices = &context->next_image_index;

	context->current_frame++;
	if (VkResult submit_state = vkQueuePresentKHR(context->graphics_queue, &present_info); submit_state != VK_SUCCESS)
	{
		acp_vulkan::renderer_resize(context, context->width, context->height);
	}
}

void acp_vulkan::renderer_shutdown(acp_vulkan::renderer_context* context)
{
	if (context->instance)
	{
		if (context->logical_device)
			ACP_VK_CHECK(vkDeviceWaitIdle(context->logical_device), context);

		context->user_context.renderer_shutdown(context);

		destroy_frame_sync_data(context);

		for (int i = 0; i < context->imediate_commands_pools.size(); ++i)
			commands_pool_destroy(context, context->imediate_commands_pools[i]);
		context->imediate_commands_pools.clear();

		for (int i = 0; i < context->imediate_commands_fences.size(); ++i)
			fence_destroy(context, context->imediate_commands_fences[i]);
		context->imediate_commands_fences.clear();

		if (context->swapchain)
		{
			swapchian_destroy(context->swapchain, context);
			context->swapchain = nullptr;
		}
		if (context->surface)
		{
			acp_vulkan_os_specific_destroy_renderer_surface(context->surface, context->instance);
			context->surface = VK_NULL_HANDLE;
		}

		if (context->gpu_allocator)
		{
			vmaDestroyAllocator(context->gpu_allocator);
			context->gpu_allocator = nullptr;
		}

		if (context->logical_device)
		{
			vkDestroyDevice(context->logical_device, context->host_allocator);
			context->logical_device = VK_NULL_HANDLE;
		}
		
		unregister_debug_callback(context);
		vkDestroyInstance(context->instance, context->host_allocator);
		context->instance = VK_NULL_HANDLE;
	}
	delete context;
}

void acp_vulkan::immediate_submit(renderer_context* context, std::function<void(VkCommandBuffer cmd)>&& function)
{
	VkCommandPool imediate_cmd_pool = context->imediate_commands_pools[0];
	
	VkCommandBuffer command_buffer = VK_NULL_HANDLE;
	VkCommandBufferAllocateInfo command_allocate{ VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO };
	command_allocate.commandPool = imediate_cmd_pool;
	command_allocate.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
	command_allocate.commandBufferCount = 1;

	ACP_VK_CHECK(vkAllocateCommandBuffers(context->logical_device, &command_allocate, &command_buffer), context);

	VkCommandBufferBeginInfo info{ VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO };
	info.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
	ACP_VK_CHECK(vkBeginCommandBuffer(command_buffer, &info), context);
	{
		function(command_buffer);
	}
	ACP_VK_CHECK(vkEndCommandBuffer(command_buffer), context);

	VkSubmitInfo submit{ VK_STRUCTURE_TYPE_SUBMIT_INFO };
	submit.commandBufferCount = 1;
	submit.pCommandBuffers = &command_buffer;
	ACP_VK_CHECK(vkQueueSubmit(context->graphics_queue, 1, &submit, context->imediate_commands_fences[0]), context);

	vkWaitForFences(context->logical_device, 1, &context->imediate_commands_fences[0], true, UINT64_MAX);
	vkResetFences(context->logical_device, 1, &context->imediate_commands_fences[0]);
	ACP_VK_CHECK(vkResetCommandPool(context->logical_device, imediate_cmd_pool, 0), context);
}