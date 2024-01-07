#pragma once
#include <vulkan/vulkan.h>
#include <inttypes.h>

namespace acp_vulkan
{
	struct gltf_data
	{
		enum gltf_state_type
		{
			valid,
			parsing_error,
			missing_asset_section,
			unable_to_read_file,
			deleted
		} gltf_state;
		size_t parsing_error_location;

		template<typename T>
		struct data_view
		{
			T* data{ nullptr };
			size_t data_length{ 0 };
		};

		template<typename A, typename B>
		struct pair
		{
			A first;
			B second;
		};

		typedef data_view<const char> string_view;

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
		data_view<buffer_view> buffer_views;

		struct buffer
		{
			string_view uri;
			string_view name;
			uint32_t byte_length;
			data_view<uint8_t> embedded_bytes;
			string_view embedded_mime;
		};
		data_view<buffer> buffers;

		struct image
		{
			string_view uri;
			uint32_t buffer_view;
			string_view mime_type;
			data_view<uint8_t> embedded_bytes;
			string_view embedded_mime;
		};
		data_view<image> images;

		struct accesor
		{
			uint32_t buffer_view;
			uint32_t byte_offset;
			enum class component_type_type {
				UNSIGNED_BYTE = 5121,
				SHORT = 5122,
				UNSIGNED_SHORT = 5123,
				UNSIGNED_INT = 5125,
				FLOAT = 5126
			};
			component_type_type component_type;
			bool	 normalized;
			uint32_t count;
			enum class type_type
			{
				SCALAR,
				VEC2,
				VEC3,
				VEC4,
				MAT2,
				MAT3,
				MAT4
			};
			type_type type;
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
		data_view<accesor> accesors;

		struct texture
		{
			uint32_t sampler;
			uint32_t source;
			string_view name;
		};
		data_view<texture> textures;

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
				data_view<pair<attribute, uint32_t>> attributes;
				uint32_t indices;
				uint32_t material;
				mode mode{ mode::TRIANGLES };
				struct target
				{
					data_view<pair<attribute, uint32_t>> attributes;
				};
				data_view<target> targets;
			};
			data_view<primitive_type> primitives;
			data_view<float> weights;
			string_view name;
		};
		data_view<mesh> meshes;

		struct material
		{
			struct texture_info
			{
				uint32_t index;
				uint32_t tex_coord{ 0 };
			};
			struct pbr_metallic_roughness_type
			{
				float base_color_factor[4]{ 1.0f, 1.0f, 1.0f, 1.0f };
				bool has_base_color_texture{ false };
				texture_info base_color_texture;
				float metallic_factor{ 1.0f };
				float roughness_factor{ 1.0f };
				bool has_metallic_roughness_texture{ false };
				texture_info metallic_roughness_texture;
			};
			bool has_pbr_metallic_roughness{ false };
			pbr_metallic_roughness_type pbr_metallic_roughness;
			struct normal_texture_info
			{
				uint32_t index;
				uint32_t tex_coord{ 0 };
				float scale{ 1.0f };
			};
			bool has_normal_texture{ false };
			normal_texture_info normal_texture;
			struct occlusion_texture_info
			{
				uint32_t index;
				uint32_t tex_coord{ 0 };
				float strength{ 1.0f };
			};
			bool has_occlusion_texture{ false };
			occlusion_texture_info occlusion_texture;
			bool has_emissive_texture{ false };
			texture_info emissive_texture;
			float emissive_factor[3] = { 0.0f, 0.0f, 0.0f };
			enum class alpha_mode_type
			{
				OPAQUE,
				MASK,
				ALPHA_CUTOFF,
				BLEND
			};
			alpha_mode_type alpha_mode{ alpha_mode_type::OPAQUE };
			float alpha_cutoff{ 0.5f };
			bool double_sided{ false };
			string_view name;
		};
		data_view<material> materials;

		struct node
		{
			bool has_camera{ false };
			uint32_t camera{ UINT32_MAX };
			data_view<uint32_t> children;
			bool has_skin{ false };
			uint32_t skin{ UINT32_MAX };
			bool has_mesh{ false };
			uint32_t mesh{ UINT32_MAX };
			bool has_matrix{ false };
			float matrix[16]{ 1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f , 0.0f, 0.0f, 0.0f, 0.0f, 1.0f };
			bool has_rotation{ false };
			float rotation[4]{ 0.0f, 0.0f, 0.0f, 1.0f };
			bool has_scale{ false };
			float scale[3]{ 1.0f, 1.0f, 1.0f };
			bool has_translation{ false };
			float translation[3]{ 0.0f, 0.0f, 0.0f };
			data_view<float> weights;
			string_view name;
		};
		data_view<node> nodes;

		struct scene {
			data_view<uint32_t> nodes;
			string_view name;
		};
		data_view<scene> scenes;
		uint32_t default_scene{ UINT32_MAX };
		bool has_defautl_scene{ false };

		struct sampler
		{
			enum class filter_type
			{
				NONE = 0,
				NEAREST = 9728,
				LINEAR = 9729,
				NEAREST_MIPMAP_NEAREST = 9984,
				LINEAR_MIPMAP_NEAREST = 9985,
				NEAREST_MIPMAP_LINEAR = 9986,
				LINEAR_MIPMAP_LINEAR = 9987
			};
			enum class wrap_type
			{
				REPEAT = 10497,
				CLAMP_TO_EDGE = 33071,
				MIRRORED_REPEAT = 33648,
			};
			filter_type mag_filter{ filter_type::NONE };
			filter_type min_filter{ filter_type::NONE };
			wrap_type wrap_s{ wrap_type::REPEAT };
			wrap_type wrap_t{ wrap_type::REPEAT };
			string_view name;
		};
		data_view<sampler> samplers;

		struct skin
		{
			bool has_inverse_bind_matrices{ false };
			uint32_t inverse_bind_matrices{ UINT32_MAX };
			bool has_skeleton{ false };
			uint32_t skeleton{ UINT32_MAX };
			data_view<uint32_t> joints;
			string_view name;
		};
		data_view<skin> skins;

		struct camera
		{
			enum class camera_type
			{
				perspective,
				orthographic
			};
			camera_type type;


			struct orthographic_properties_type
			{
				float x_mag;
				float y_mag;
				float z_far;
				float z_near;
			};
			orthographic_properties_type orthographic;
			struct perspective_properties_type
			{
				float aspect_ratio;
				float y_fov;
				float z_far;
				float z_near;
			};
			perspective_properties_type perspective;
			string_view name;
		};
		data_view<camera> cameras;

		struct animation
		{
			struct channel
			{
				uint32_t sampler;
				struct target_type {
					uint32_t node;
					enum class path_type {
						translation,
						rotation,
						scale,
						weights
					};
					path_type path;
				};
				target_type target;
			};
			data_view<channel> channels;
			struct sampler {
				uint32_t input;
				enum class interpolation_type
				{
					LINEAR,
					STEP,
					CUBICSPLINE
				};
				interpolation_type interpolation{ interpolation_type::LINEAR };
				uint32_t output;
			};
			data_view<sampler> samplers;
			string_view name;
		};
		data_view<animation> animations;
	};

	//todo(alex) : Parse the binary version !
	//todo(alex) : Investigate how to turn this in to Vulkan friendly data.
	gltf_data gltf_data_from_memory(const char* data, size_t data_size, VkAllocationCallbacks* host_allocator);
	gltf_data gltf_data_from_file(const char* path, VkAllocationCallbacks* host_allocator);
	void gltf_data_free(gltf_data* gltf_data, VkAllocationCallbacks* host_allocator);
};