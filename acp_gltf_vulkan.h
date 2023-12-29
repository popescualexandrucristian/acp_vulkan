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

		struct texture
		{
			uint32_t sampler;
			uint32_t source;
			string_view name;
		};
		std::vector<texture> textures;

		enum class attribute {
			TEXCOORD_0,
			TEXCOORD_1,
			TEXCOORD_2,
			TEXCOORD_3,
			TEXCOORD_4,
			TEXCOORD_5,
			TEXCOORD_6,
			TEXCOORD_7,
			TEXCOORD_8,
			TEXCOORD_9,
			POSITION,
			NORMAL,
			TANGENT,
			COLOR_0,
			COLOR_1,
			COLOR_2,
			COLOR_3,
			COLOR_4,
			COLOR_5,
			COLOR_6,
			COLOR_7,
			COLOR_8,
			COLOR_9,
			JOINTS_0,
			JOINTS_1,
			JOINTS_2,
			JOINTS_3,
			JOINTS_4,
			JOINTS_5,
			JOINTS_6,
			JOINTS_7,
			JOINTS_8,
			JOINTS_9,
			WEIGHTS_0,
			WEIGHTS_1,
			WEIGHTS_2,
			WEIGHTS_3,
			WEIGHTS_4,
			WEIGHTS_5,
			WEIGHTS_6,
			WEIGHTS_7,
			WEIGHTS_8,
			WEIGHTS_9
		};

		struct mesh
		{
			enum class mode
			{
				POINTS,
				LINES,
				LINE_LOOP,
				LINE_STRIP,
				TRIANGLES,
				TRIANGLE_STRIP,
				TRIANGLE_FAN
			};
			struct primitive_type
			{
				std::vector<std::pair<attribute, uint32_t>> attributes;
				uint32_t indices;
				uint32_t material;
				mode mode{ mode::TRIANGLES };
				struct target
				{
					std::vector<std::pair<attribute, uint32_t>> attributes;
				};
				std::vector<target> targets;
			};
			std::vector<primitive_type> primitives;
			std::vector<float> weights;
			string_view name;
		};
		std::vector<mesh> meshes;

		void* gltf_data;
	};

	gltf_data gltf_data_from_memory(const char* data, size_t data_size);
};