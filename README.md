# acp_vulkan
#### version: 0.3
---
For examples see : https://github.com/popescualexandrucristian/engine6
---
Utility tools that help with some of the Vulkan's boilerplate.

### acp_program_vulkan.h
Utility that parses shader data to generate modules with metadata.

Parse Vulkan shader files and return a shader structure that contains the shader code and some metadata.
```
    shader* shader_init(
    VkDevice logical_device, 
    VkAllocationCallbacks* host_allocator, 
    const uint32_t* const data, 
    size_t data_size,
	const char* name);
``` 
```
    shader* shader_init(VkDevice logical_device, VkAllocationCallbacks* host_allocator, const char* path);
```

Deallocate the shader data.
```
    void shader_destroy(VkDevice logical_device, VkAllocationCallbacks* host_allocator, shader* shader);
```

```
struct shader
    {
        VkShaderStageFlagBits type; // shader type.
        VkShaderModule module; // loaded shader module.
        bool uses_constants; // true if it uses constant buffers.
        struct meta
        {
            struct binding
            {
                VkDescriptorType resource_type;
                uint32_t binding;
                uint32_t set;
            };
            struct attribute
            {
                shader::meta::type field_type; // type of the variable.
                uint32_t location; // binding location for the variable.
            };

            uint32_t group_size_x; //group size if applyes for this shader.
            uint32_t group_size_y;
            uint32_t group_size_z;
            int32_t group_size_x_id = -1;
            int32_t group_size_y_id = -1;
            int32_t group_size_z_id = -1;
            std::vector<binding> resource_bindings;
            std::string entry_point; //name of the main for this shader.
            std::vector<attribute> inputs; //inputs consumed by this shader.
            std::vector<attribute> outputs; //results for the next stage from this shader.
        } metadata;
    };
```

Create a pipeline, pipeline layout and descriptor layouts based on the metadata from the shaders.
Note :
 * This system uses the new dynamic render pass instance as I am a frame-buffer/render pass hater.
 * Uses dynamic states for view-ports and scissors.
 * Limitations:
	 * It uses c++ primitives, will the host_allocator for everything in the future.
	 * Only for the standard graphics pipelines for now.
	 * Only for VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST for now, will add others later.
	 * More depth options should be added.
	 * Blending options should be added.
	 * AA options should be added.
	 * Pipeline caches should be added.
  	 * Layout cache shoul be added. (probably the last 2 should be one object or something idk) 
```
//If vertex streams are used we need some extra information that can't be deduced from shaders.
//This info comes in a list of input_attribute_data, this is optional and only needed if the input attributes are used in the vertex shader.
struct input_attribute_data
{
    uint32_t			binding;
    std::vector<uint32_t>	offsets;
    std::vector<uint32_t>	locations;
    uint32_t			stride;
    VkVertexInputRate		input_rate;
};
```

```
struct program
{
    VkPipelineLayout pipeline_layout;
    VkPipeline pipeline;
    typedef std::pair<VkDescriptorSetLayout, std::vector<VkDescriptorSetLayoutBinding>> layout;
    std::vector<layout> descriptor_layouts;
};
```

```
program* graphics_program_init(
        VkDevice logical_device, 
        VkAllocationCallbacks* host_allocator, 
        shaders shaders, 
        input_attributes vertex_input_attributes,
        size_t push_constant_size, 
        bool use_depth, 
        bool write_to_depth, 
        bool sharedDescriptorSets,
        uint32_t color_attachment_count, 
        const VkFormat* color_attachment_formats,
        VkFormat depth_attachment_format, 
        VkFormat stencil_attachment_format,
		const char* name);
```

```
program* compute_program_init(
		VkDevice logical_device, 
		VkAllocationCallbacks* host_allocator, 
		const shader* shader, 
        size_t push_constant_size, 
		bool sharedDescriptorSets, 
		const char* name);
```

Deallocate a pipeline, it's layout and it's descriptor layouts + the metadata.
```
    void program_destroy(
    VkDevice logical_device, 
    VkAllocationCallbacks* host_allocator, program* program);
```

### acp_dds_vulkan.h
Utility that parses dds files or data to generate Vulkan ready image data.

```
	struct image_mip_data
	{
		VkExtent3D extents{}; // size of the mip level.
		uint8_t* data{ nullptr }; // data for the mip level.
		size_t data_size{ 0 }; // size in bytes for the mip level.
	};

	struct dds_data
	{
	    // VkImageCreateInfo with all the proper flags for this kind of dds
	    VkImageCreateInfo image_create_info; 
	    size_t width{ 0 };
	    size_t height{ 0 };
	    image_mip_data image_mip_data[16] = {};
	    size_t num_mips{ 0 };
	    unsigned char* dss_buffer_data{ nullptr };
	};
```

Load DDS data from file or from memory.
Note :
 * Parameters:
	* data - bytes that point to DDS data includeing the headers.
	* data_size - size of data.
	* will_own_data - the call will allocate a copy of the data and dds_data_free will have to be called to free that memory.
	* host_allocator - standard Vulkan allocator, if null, the default allocator will be used. Allocations are made on the heap only if will_own_data is true.
	* the file version of the call always owns the memory.
 * Limitations:
	 * Does not support paletted versions of DDS.
	 * Does not support/was not tested with the new versions of files.

```
	dds_data dds_data_from_memory(void* data, size_t data_size, bool will_own_data, VkAllocationCallbacks* host_allocator);
	
	dds_data dds_data_from_file(const char* path, VkAllocationCallbacks* host_allocator);
```
Free DDS data.
Note :
	* Only needs to be called if the dds_data owns the memory.
```
	void dds_data_free(dds_data* dds_data, VkAllocationCallbacks* host_allocator);
```
Note:
* This lib is in it's initial form and it will take some time until it is battle ready.
* The lib is licensed using the MIT license.
* Examples/tests will come at some point in the future in a sister repo.
* A build system is not provided as one is not necessary, just include the .cpp and the .h files.
* Might move to a header only mode in the future.
* I am a fan of Ortodox C++ so please don't create pull requests with things that are not necessary such as encapsulation directives, proper classes or other c++ 'features'.

### acp_context/*

Boilerplate far initializeing the vulakn context, swapchain and depth buffers + utils for standard primitives.

Note:
	* This system uses the new dynamic render pass instance as I am a frame-buffer/render pass hater.
