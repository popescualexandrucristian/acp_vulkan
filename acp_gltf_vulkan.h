#pragma once
#include <vulkan/vulkan.h>
#include <inttypes.h>
#include <vector>

namespace acp_vulkan
{
	struct gltf_data
	{
		enum gltf_state_type
		{
			valid,
			parsing_error,
			missing_asset_section
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

		struct buffer_view
		{
			uint32_t buffer;
			uint32_t byte_offset;
			uint32_t byte_length;
		};
		std::vector<buffer_view> buffer_views;

		struct buffer
		{
			string_view uri;
			string_view name;
			uint32_t byte_length;
		};
		std::vector<buffer> buffers;

		struct image
		{
			string_view uri;
			uint32_t buffer_view;
			string_view mime_type;
		};
		std::vector<image> images;

		struct accesor
		{
			uint32_t buffer_view;
			uint32_t byte_offset;
			uint32_t component_type;
			bool	 normalized;
			uint32_t count;
			string_view type;
			float max[16];
			float min[16];

			struct sparse_type
			{
				uint32_t count;
				struct indices_type
				{
					uint32_t buffer_view;
					uint32_t byte_offset;
					uint32_t component_type;
				};
				indices_type indices;
				struct values_type
				{
					uint32_t buffer_view;
					uint32_t byte_offset;
				};
				values_type values;
			};
			bool is_sparse;
			sparse_type sparse;

			string_view name;
		};
		std::vector<accesor> accesors;

		void* gltf_data;
	};

	gltf_data gltf_data_from_memory(const char* data, size_t data_size);
};