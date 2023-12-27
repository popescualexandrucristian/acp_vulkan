#pragma once
#include <vulkan/vulkan.h>

namespace acp_vulkan
{
	struct gltf_data
	{
		enum gltf_state_type
		{
			valid,
			parsing_error
		} gltf_state;
		size_t parsing_error_location;

		struct string_view
		{
			const char* data;
			size_t data_length;
		};

		struct asset_type
		{
			string_view generator;
			string_view version;
		} asset;
		void* gltf_data;
	};

	gltf_data gltf_data_from_memory(const char* data, size_t data_size);
};