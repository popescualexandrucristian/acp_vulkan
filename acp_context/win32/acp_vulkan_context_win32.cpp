#include <Windows.h>
#include <vulkan/vulkan.h>
#include <vulkan/vulkan_win32.h>
#include <acp_context/acp_vulkan_context.h>

extern "C"
{
	__declspec(dllexport) DWORD NvOptimusEnablement = 0x00000001;
	__declspec(dllexport) int AmdPowerXpressRequestHighPerformance = 1;
}

const char* object_type(VkDebugReportObjectTypeEXT s)
{
	switch (s) {
	case VK_DEBUG_REPORT_OBJECT_TYPE_UNKNOWN_EXT:
		return "UNKNOWN";
	case VK_DEBUG_REPORT_OBJECT_TYPE_INSTANCE_EXT:
		return "INSTANCE";
	case VK_DEBUG_REPORT_OBJECT_TYPE_PHYSICAL_DEVICE_EXT:
		return "PHYSICAL_DEVICE";
	case VK_DEBUG_REPORT_OBJECT_TYPE_DEVICE_EXT:
		return "DEVICE";
	case VK_DEBUG_REPORT_OBJECT_TYPE_QUEUE_EXT:
		return "QUEUE";
	case VK_DEBUG_REPORT_OBJECT_TYPE_SEMAPHORE_EXT:
		return "SEMAPHORE";
	case VK_DEBUG_REPORT_OBJECT_TYPE_COMMAND_BUFFER_EXT:
		return "COMMAND_BUFFER";
	case VK_DEBUG_REPORT_OBJECT_TYPE_FENCE_EXT:
		return "FENCE";
	case VK_DEBUG_REPORT_OBJECT_TYPE_DEVICE_MEMORY_EXT:
		return "DEVICE_MEMORY";
	case VK_DEBUG_REPORT_OBJECT_TYPE_BUFFER_EXT:
		return "BUFFER";
	case VK_DEBUG_REPORT_OBJECT_TYPE_IMAGE_EXT:
		return "IMAGE";
	case VK_DEBUG_REPORT_OBJECT_TYPE_EVENT_EXT:
		return "EVENT";
	case VK_DEBUG_REPORT_OBJECT_TYPE_QUERY_POOL_EXT:
		return "QUERY_POOL";
	case VK_DEBUG_REPORT_OBJECT_TYPE_BUFFER_VIEW_EXT:
		return "BUFFER_VIEW";
	case VK_DEBUG_REPORT_OBJECT_TYPE_IMAGE_VIEW_EXT:
		return "IMAGE_VIEW";
	case VK_DEBUG_REPORT_OBJECT_TYPE_SHADER_MODULE_EXT:
		return "SHADER_MODULE";
	case VK_DEBUG_REPORT_OBJECT_TYPE_PIPELINE_CACHE_EXT:
		return "PIPELINE_CACHE";
	case VK_DEBUG_REPORT_OBJECT_TYPE_PIPELINE_LAYOUT_EXT:
		return "PIPELINE_LAYOUT";
	case VK_DEBUG_REPORT_OBJECT_TYPE_RENDER_PASS_EXT:
		return "RENDER_PASS";
	case VK_DEBUG_REPORT_OBJECT_TYPE_PIPELINE_EXT:
		return "PIPELINE";
	case VK_DEBUG_REPORT_OBJECT_TYPE_DESCRIPTOR_SET_LAYOUT_EXT:
		return "DESCRIPTOR_SET_LAYOUT";
	case VK_DEBUG_REPORT_OBJECT_TYPE_SAMPLER_EXT:
		return "SAMPLER";
	case VK_DEBUG_REPORT_OBJECT_TYPE_DESCRIPTOR_POOL_EXT:
		return "DESCRIPTOR_POOL";
	case VK_DEBUG_REPORT_OBJECT_TYPE_DESCRIPTOR_SET_EXT:
		return "DESCRIPTOR_SET";
	case VK_DEBUG_REPORT_OBJECT_TYPE_FRAMEBUFFER_EXT:
		return "FRAMEBUFFER";
	case VK_DEBUG_REPORT_OBJECT_TYPE_COMMAND_POOL_EXT:
		return "COMMAND_POOL";
	case VK_DEBUG_REPORT_OBJECT_TYPE_SURFACE_KHR_EXT:
		return "SURFACE_KHR";
	case VK_DEBUG_REPORT_OBJECT_TYPE_SWAPCHAIN_KHR_EXT:
		return "SWAPCHAIN_KHR";
	case VK_DEBUG_REPORT_OBJECT_TYPE_DEBUG_REPORT_CALLBACK_EXT_EXT:
		return "DEBUG_REPORT_CALLBACK_EXT";
	case VK_DEBUG_REPORT_OBJECT_TYPE_DISPLAY_KHR_EXT:
		return "DISPLAY_KHR";
	case VK_DEBUG_REPORT_OBJECT_TYPE_DISPLAY_MODE_KHR_EXT:
		return "DISPLAY_MODE_KHR";
	case VK_DEBUG_REPORT_OBJECT_TYPE_VALIDATION_CACHE_EXT_EXT:
		return "VALIDATION_CACHE_EXT";
	case VK_DEBUG_REPORT_OBJECT_TYPE_SAMPLER_YCBCR_CONVERSION_EXT:
		return "SAMPLER_YCBCR_CONVERSION";
	case VK_DEBUG_REPORT_OBJECT_TYPE_DESCRIPTOR_UPDATE_TEMPLATE_EXT:
		return "DESCRIPTOR_UPDATE_TEMPLATE";
	case VK_DEBUG_REPORT_OBJECT_TYPE_CU_MODULE_NVX_EXT:
		return "CU_MODULE_NVX";
	case VK_DEBUG_REPORT_OBJECT_TYPE_CU_FUNCTION_NVX_EXT:
		return "CU_FUNCTION_NVX";
	case VK_DEBUG_REPORT_OBJECT_TYPE_ACCELERATION_STRUCTURE_KHR_EXT:
		return "ACCELERATION_STRUCTURE_KHR";
	case VK_DEBUG_REPORT_OBJECT_TYPE_ACCELERATION_STRUCTURE_NV_EXT:
		return "ACCELERATION_STRUCTURE_NV";
	case VK_DEBUG_REPORT_OBJECT_TYPE_BUFFER_COLLECTION_FUCHSIA_EXT:
		return "BUFFER_COLLECTION_FUCHSIA";
	default:
		return "UNKNOWN";
	}
}

const char* severity(VkDebugReportFlagsEXT s)
{
	switch (s) {
	case VK_DEBUG_REPORT_INFORMATION_BIT_EXT:
		return "info";
	case VK_DEBUG_REPORT_WARNING_BIT_EXT:
		return "warning";
	case VK_DEBUG_REPORT_PERFORMANCE_WARNING_BIT_EXT:
		return "performace warning";
	case VK_DEBUG_REPORT_ERROR_BIT_EXT:
		return "error";
	case VK_DEBUG_REPORT_DEBUG_BIT_EXT:
		return "debug";
	default:
		return "unknown";
	}
}

VkBool32 debug_callback(
	VkDebugReportFlagsEXT                       flags,
	VkDebugReportObjectTypeEXT                  objectType,
	uint64_t                                    object,
	size_t                                      location,
	int32_t                                     messageCode,
	const char*,
	const char* pMessage,
	void*)
{
	auto o = object_type(objectType);
	auto s = severity(flags);
	acp_vulkan_os_specific_log("[%s(%d): %s(%Id)]\n%s:%zd\n", s, messageCode, o, object, pMessage, location);

	return VK_FALSE;
}

PFN_vkDebugReportCallbackEXT acp_vulkan_os_specific_get_log_callback()
{
#ifdef ENABLE_DEBUG_CONSOLE
	return &debug_callback;
#else
	return nullptr;
#endif
}

const char* debug_layers[] =
{
	"VK_LAYER_KHRONOS_validation",
	nullptr
};

const char** acp_vulkan_os_specific_get_renderer_debug_layers()
{
#ifdef ENABLE_VULKAN_VALIDATION_LAYERS
	return debug_layers;
#else
	return nullptr;
#endif
}

const char* extensions[] =
{
	VK_KHR_SURFACE_EXTENSION_NAME,
	VK_KHR_WIN32_SURFACE_EXTENSION_NAME,
#ifdef ENABLE_DEBUG_CONSOLE
	VK_EXT_DEBUG_REPORT_EXTENSION_NAME,
#endif
	nullptr
};

const char** acp_vulkan_os_specific_get_renderer_extensions()
{
	return extensions;
}

bool acp_vulkan_os_specific_get_renderer_device_supports_presentation(VkPhysicalDevice physical_device, uint32_t family_index)
{
	return vkGetPhysicalDeviceWin32PresentationSupportKHR(physical_device, family_index);
}

struct handle_data {
	unsigned long process_id;
	HWND window_handle;
};

BOOL is_main_window(HWND handle)
{
	return GetWindow(handle, GW_OWNER) == (HWND)0 && IsWindowVisible(handle);
}

BOOL CALLBACK enum_windows_callback(HWND handle, LPARAM lParam)
{
	handle_data& data = *(handle_data*)lParam;
	unsigned long process_id = 0;
	GetWindowThreadProcessId(handle, &process_id);
	if (data.process_id != process_id || !is_main_window(handle))
		return TRUE;
	data.window_handle = handle;
	return FALSE;
}

handle_data find_main_window(unsigned long process_id)
{
	handle_data data;
	data.process_id = process_id;
	data.window_handle = 0;
	EnumWindows(enum_windows_callback, (LPARAM)&data);
	return data;
}

VkSurfaceKHR acp_vulkan_os_specific_create_renderer_surface(VkInstance instance)
{
	handle_data main_window = find_main_window(GetProcessId(GetCurrentProcess()));
	VkWin32SurfaceCreateInfoKHR createInfo = { VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR };
	createInfo.hinstance = GetModuleHandle(0);
	createInfo.hwnd = main_window.window_handle;

	VkSurfaceKHR surface = 0;
	if (vkCreateWin32SurfaceKHR(instance, &createInfo, nullptr, &surface) != VK_SUCCESS)
		return VK_NULL_HANDLE;
	return surface;
}

acp_vulkan_os_specific_width_and_height acp_vulkan_os_specific_get_width_and_height()
{
	handle_data main_window = find_main_window(GetProcessId(GetCurrentProcess()));
	RECT rect{};
	GetClientRect(main_window.window_handle, &rect);
	return { uint32_t(rect.right - rect.left), uint32_t(rect.bottom - rect.top) };
}

void* acp_vulkan_os_specific_get_main_window_handle()
{
	handle_data main_window = find_main_window(GetProcessId(GetCurrentProcess()));
	return main_window.window_handle;
}

void acp_vulkan_os_specific_destroy_renderer_surface(VkSurfaceKHR surface, VkInstance instance)
{
	vkDestroySurfaceKHR(instance, surface, nullptr);
}
