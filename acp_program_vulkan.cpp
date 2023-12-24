#include "acp_program_vulkan.h"

#include <vector>
#include <string>
#include <map>
#include <spirv-headers/spirv.h>
#include <assert.h>

#ifdef ENABLE_VULKAN_DEBUG_MARKERS
#include <acp_debug_vulkan.h>
#endif

struct shader_id
{
    uint32_t opcode;
    uint32_t typeId;
    uint32_t storageClass;
    uint32_t binding;
    uint32_t location;
    uint32_t set;
    uint32_t constant;
    uint32_t component_count;
    uint32_t width;
    uint32_t signedness;
    bool input_or_output;
};

static acp_vulkan::shader::meta::type to_meta_type(uint32_t num_components, SpvOp element_type_op_code, uint32_t width, uint32_t signedness)
{
    switch (element_type_op_code)
    {
    case SpvOpTypeVoid:
        return acp_vulkan::shader::meta::type::void_type;
    case SpvOpTypeFloat:
    {
        if (width == 32)
        {
            switch (num_components)
            {
            case 1: return acp_vulkan::shader::meta::type::float_type;
            case 2: return acp_vulkan::shader::meta::type::vec2_type;
            case 3: return acp_vulkan::shader::meta::type::vec3_type;
            case 4: return acp_vulkan::shader::meta::type::vec4_type;
            }
        }
        else if (width == 64)
        {
            switch (num_components)
            {
            case 1: return acp_vulkan::shader::meta::type::double_type;
            case 2: return acp_vulkan::shader::meta::type::dvec2_type;
            case 3: return acp_vulkan::shader::meta::type::dvec3_type;
            case 4: return acp_vulkan::shader::meta::type::dvec4_type;
            }
        }
    }
    break;
    case SpvOpTypeBool:
    {
        switch (num_components)
        {
        case 1: return acp_vulkan::shader::meta::type::bool_type;
        case 2: return acp_vulkan::shader::meta::type::bvec2_type;
        case 3: return acp_vulkan::shader::meta::type::bvec3_type;
        case 4: return acp_vulkan::shader::meta::type::bvec4_type;
        }
    }
    case SpvOpTypeInt:
    {
        if (signedness == 1)
        {
            switch (num_components)
            {
            case 1: return acp_vulkan::shader::meta::type::int_type;
            case 2: return acp_vulkan::shader::meta::type::ivec2_type;
            case 3: return acp_vulkan::shader::meta::type::ivec3_type;
            case 4: return acp_vulkan::shader::meta::type::ivec4_type;
            }
        }
        else if (signedness == 0)
        {
            switch (num_components)
            {
            case 1: return acp_vulkan::shader::meta::type::uint_type;
            case 2: return acp_vulkan::shader::meta::type::uvec2_type;
            case 3: return acp_vulkan::shader::meta::type::uvec3_type;
            case 4: return acp_vulkan::shader::meta::type::uvec4_type;
            }
        }
    }
    break;
    }

    return acp_vulkan::shader::meta::type::types_count;
}

static VkShaderStageFlagBits execution_model_to_shader_stage(SpvExecutionModel executionModel)
{
    switch (executionModel)
    {
    case SpvExecutionModelVertex:
        return VK_SHADER_STAGE_VERTEX_BIT;
    case SpvExecutionModelFragment:
        return VK_SHADER_STAGE_FRAGMENT_BIT;
    case SpvExecutionModelGLCompute:
        return VK_SHADER_STAGE_COMPUTE_BIT;
    case SpvExecutionModelTaskEXT:
        return VK_SHADER_STAGE_TASK_BIT_EXT;
    case SpvExecutionModelMeshEXT:
        return VK_SHADER_STAGE_MESH_BIT_EXT;

    default:
        return VkShaderStageFlagBits(0);
    }
}

static VkDescriptorType spv_op_to_descriptor_type(SpvOp op, SpvStorageClass type)
{
    switch (op)
    {
    case SpvOpTypeStruct:
        if (type == SpvStorageClassUniform)
            return VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        if (type == SpvStorageClassStorageBuffer)
            return VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    case SpvOpTypeImage:
        return VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
    case SpvOpTypeSampler:
        return VK_DESCRIPTOR_TYPE_SAMPLER;
    case SpvOpTypeSampledImage:
        return VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    default:
        return VK_DESCRIPTOR_TYPE_MAX_ENUM;
    }
}

static bool update_shader_metadata(acp_vulkan::shader* shader, const uint32_t* const data, size_t data_size)
{
    if (data[0] != SpvMagicNumber)
        return false;

    uint32_t num_ids = data[3];
    std::vector<shader_id> ids(num_ids);

    const uint32_t* data_ptr = data + 5;

    while (data_ptr != data + data_size)
    {
        uint16_t opcode = uint16_t(data_ptr[0]);
        uint16_t word_count = uint16_t(data_ptr[0] >> 16);

        switch (opcode)
        {
        case SpvOpEntryPoint:
        {
            if (word_count < 4)
            {
                assert(false && "Invalid number of words in SpvOpEntryPoint");
                return false;
            }
            shader->type = execution_model_to_shader_stage(SpvExecutionModel(data_ptr[1]));

            uint32_t word_offset = 1;
            const char* entry_point_name = reinterpret_cast<const char*>(data_ptr + 3);
            if (entry_point_name[0] != '\0')
            {
                shader->metadata.entry_point = entry_point_name;
                if (!shader->metadata.entry_point.empty())
                    word_offset = uint32_t(shader->metadata.entry_point.size() / 4 + 1);
            }
            for (size_t i = 3 + word_offset; i < word_count; ++i)
            {
                ids[data_ptr[i]].input_or_output = true;
            }

        } break;
        case SpvOpExecutionMode:
        {
            if (word_count < 3)
            {
                assert(false && "Invalid number of words in SpvOpExecutionMode");
                return false;
            }

            uint32_t mode = data_ptr[2];

            switch (mode)
            {
            case SpvExecutionModeLocalSize:

                if (word_count != 6)
                {
                    assert(false && "Invalid number of words in SpvExecutionModeLocalSize");
                    return false;
                }

                shader->metadata.group_size_x = data_ptr[3];
                shader->metadata.group_size_y = data_ptr[4];
                shader->metadata.group_size_z = data_ptr[5];
                break;
            }
        } break;
        case SpvOpExecutionModeId:
        {
            if (word_count < 3)
            {
                assert(false && "Invalid number of words in SpvOpExecutionModeId");
                return false;
            }

            uint32_t mode = data_ptr[2];

            switch (mode)
            {
            case SpvExecutionModeLocalSizeId:
                if (word_count != 6)
                {
                    assert(false && "Invalid number of words in SpvExecutionModeLocalSizeId");
                    return false;
                }
                shader->metadata.group_size_x_id = int32_t(data_ptr[3]);
                shader->metadata.group_size_y_id = int32_t(data_ptr[4]);
                shader->metadata.group_size_z_id = int32_t(data_ptr[5]);
                break;
            }
        } break;
        case SpvOpDecorate:
        {
            if (word_count < 3)
            {
                assert(false && "Invalid number of words in SpvOpDecorate");
                return false;
            }

            uint32_t id = data_ptr[1];
            if (id >= num_ids)
            {
                assert(false && "Invalid spv op id");
                return false;
            }

            switch (data_ptr[2])
            {
            case SpvDecorationDescriptorSet:
                if (word_count != 4)
                {
                    assert(false && "Invalid number of words in SpvDecorationDescriptorSet");
                    return false;
                }
                ids[id].set = data_ptr[3];
                break;
            case SpvDecorationBinding:
                if (word_count != 4)
                {
                    assert(false && "Invalid number of words in SpvDecorationBinding");
                    return false;
                }
                ids[id].binding = data_ptr[3];
                break;
            case SpvDecorationLocation:
                if (word_count != 4)
                {
                    assert(false && "Invalid number of words in SpvDecorationLocation");
                    return false;
                }
                ids[id].location = data_ptr[3];
                break;
            }
        } break;
        case SpvOpTypeVector:
        {
            if (word_count != 4)
            {
                assert(false && "Invalid number of words in spv type");
                return false;
            }
            uint32_t id = data_ptr[1];
            if (id >= num_ids)
            {
                assert(false && "Invalid spv op id");
                return false;
            }
            if (ids[id].opcode != 0)
            {
                assert(false && "Spv type id is used");
                return false;
            }

            ids[id].opcode = opcode;
            ids[id].typeId = data_ptr[2];
            ids[id].component_count = data_ptr[3];

        } break;
        case SpvOpTypeBool:
        case SpvOpTypeVoid:
        {
            if (word_count != 2)
            {
                assert(false && "Invalid number of words in spv type");
                return false;
            }
            uint32_t id = data_ptr[1];
            if (id >= num_ids)
            {
                assert(false && "Invalid spv op id");
                return false;
            }
            if (ids[id].opcode != 0)
            {
                assert(false && "Spv type id is used");
                return false;
            }

            ids[id].opcode = opcode;
        } break;
        case SpvOpTypeFloat:
        {
            if (word_count != 3)
            {
                assert(false && "Invalid number of words in spv type");
                return false;
            }
            uint32_t id = data_ptr[1];
            if (id >= num_ids)
            {
                assert(false && "Invalid spv op id");
                return false;
            }
            if (ids[id].opcode != 0)
            {
                assert(false && "Spv type id is used");
                return false;
            }

            ids[id].opcode = opcode;
            ids[id].width = data_ptr[2];

        } break;
        case SpvOpTypeInt:
        {
            if (word_count != 4)
            {
                assert(false && "Invalid number of words in spv type");
                return false;
            }
            uint32_t id = data_ptr[1];
            if (id >= num_ids)
            {
                assert(false && "Invalid spv op id");
                return false;
            }
            if (ids[id].opcode != 0)
            {
                assert(false && "Spv type id is used");
                return false;
            }

            ids[id].opcode = opcode;
            ids[id].width = data_ptr[2];
            ids[id].signedness = data_ptr[3];

        } break;
        case SpvOpTypeStruct:
        case SpvOpTypeImage:
        case SpvOpTypeSampler:
        case SpvOpTypeSampledImage:
        {
            if (word_count < 2)
            {
                assert(false && "Invalid number of words in spv type");
                return false;
            }

            uint32_t id = data_ptr[1];
            if (id >= num_ids)
            {
                assert(false && "Invalid spv op id");
                return false;
            }

            if (ids[id].opcode != 0)
            {
                assert(false && "Spv type id is used");
                return false;
            }
            ids[id].opcode = opcode;
        } break;
        case SpvOpTypePointer:
        {
            if (word_count != 4)
            {
                assert(false && "Invalid number of words in SpvOpTypePointer");
                return false;
            }

            uint32_t id = data_ptr[1];
            if (id >= num_ids)
            {
                assert(false && "Invalid spv op id");
                return false;
            }

            if (ids[id].opcode != 0)
            {
                assert(false && "Spv type id is used");
                return false;
            }
            ids[id].opcode = opcode;
            ids[id].typeId = data_ptr[3];
            ids[id].storageClass = data_ptr[2];
        } break;
        case SpvOpConstant:
        {
            if (word_count < 4)
            {
                assert(false && "Invalid number of words in SpvOpConstant");
                return false;
            }

            uint32_t id = data_ptr[2];
            if (ids[id].opcode != 0)
            {
                assert(false && "Spv type id is used");
                return false;
            }

            ids[id].opcode = opcode;
            ids[id].typeId = data_ptr[1];
            ids[id].constant = data_ptr[3];
        } break;
        case SpvOpVariable:
        {
            if (word_count < 4)
            {
                assert(false && "Invalid number of words in SpvOpVariable");
                return false;
            }

            uint32_t id = data_ptr[2];
            if (id >= num_ids)
            {
                assert(false && "Invalid spv op id");
                return false;
            }

            if (ids[id].opcode != 0)
            {
                assert(false && "Spv type id is used");
                return false;
            }
            ids[id].opcode = opcode;
            ids[id].typeId = data_ptr[1];
            ids[id].storageClass = data_ptr[3];
        } break;
        }

        if (data_ptr + word_count > data + data_size)
        {
            assert(false && "Incomplete spv data");
            return false;
        }
        data_ptr += word_count;
    }

    if (shader->metadata.group_size_x_id != -1)
    {
        if (ids.size() < shader->metadata.group_size_x_id)
        {
            assert(false && "Invalid group size id");
            return false;
        }
        shader->metadata.group_size_x = ids[shader->metadata.group_size_x_id].constant;
    }

    if (shader->metadata.group_size_y_id != -1)
    {
        if (ids.size() < shader->metadata.group_size_y_id)
        {
            assert(false && "Invalid group size id");
            return false;
        }
        shader->metadata.group_size_y = ids[shader->metadata.group_size_y_id].constant;
    }

    if (shader->metadata.group_size_y_id != -1)
    {
        if (ids.size() < shader->metadata.group_size_z_id)
        {
            assert(false && "Invalid group size id");
            return false;
        }
        shader->metadata.group_size_z = ids[shader->metadata.group_size_z_id].constant;
    }

    for (auto& id : ids)
    {
        if (id.input_or_output)
        {
            if (id.typeId > ids.size())
            {
                assert(false && "Invalid type id in output parameter");
                return false;
            }
            if (ids[id.typeId].typeId > ids.size())
            {
                assert(false && "Invalid type id in type of output parameter");
                return false;
            }
            if (ids[ids[id.typeId].typeId].typeId > ids.size())
            {
                assert(false && "Invalid type id in type of type of output parameter");
                return false;
            }

            SpvStorageClass ioType = SpvStorageClass(ids[id.typeId].storageClass);
            uint32_t num_components = ids[ids[id.typeId].typeId].component_count;
            uint32_t width = ids[ids[ids[id.typeId].typeId].typeId].width;
            uint32_t signedness = ids[ids[id.typeId].typeId].signedness;
            SpvOp element_type_op_code = SpvOp(ids[ids[ids[id.typeId].typeId].typeId].opcode);
            acp_vulkan::shader::meta::type field_type = to_meta_type(num_components, element_type_op_code, width, signedness);
            if (field_type != acp_vulkan::shader::meta::type::types_count)
            {
                if (ioType == SpvStorageClassOutput)
                    shader->metadata.outputs.push_back({ field_type, id.location });
                else if (ioType == SpvStorageClassInput)
                    shader->metadata.inputs.push_back({ field_type, id.location });
            }
        }

        if (id.opcode == SpvOpVariable && (id.storageClass == SpvStorageClassUniform || id.storageClass == SpvStorageClassUniformConstant || id.storageClass == SpvStorageClassStorageBuffer))
        {
            if (id.typeId > ids.size())
            {
                assert(false && "Invalid type id in output parameter");
                return false;
            }
            if (ids[id.typeId].typeId > ids.size())
            {
                assert(false && "Invalid type id in type of output parameter");
                return false;
            }

            if (ids[id.typeId].opcode != SpvOpTypePointer)
            {
                assert(false && "Should have a valid pointer for a type");
                return false;
            }

            SpvStorageClass type = SpvStorageClass(ids[id.typeId].storageClass);

            uint32_t type_kind = ids[ids[id.typeId].typeId].opcode;
            VkDescriptorType resource_type = spv_op_to_descriptor_type(SpvOp(type_kind), type);

            if (resource_type == VK_DESCRIPTOR_TYPE_MAX_ENUM)
            {
                assert(false && "Unknown descriptor type");
                return false;
            }

            shader->metadata.resource_bindings.push_back({ resource_type, id.binding, id.set });
        }

        if (id.opcode == SpvOpVariable && id.storageClass == SpvStorageClassPushConstant)
        {
            shader->uses_constants = true;
        }
    }

    return true;
}

acp_vulkan::shader* acp_vulkan::shader_init(VkDevice logical_device, VkAllocationCallbacks* host_allocator, const uint32_t* const data, size_t data_size, const char* name)
{
    VkShaderModule module = VK_NULL_HANDLE;
    VkShaderModuleCreateInfo create_info{ VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO };
    create_info.codeSize = uint32_t(data_size * sizeof(uint32_t));
    create_info.pCode = data;
    if (vkCreateShaderModule(logical_device, &create_info, host_allocator, &module) != VK_SUCCESS)
        return nullptr;

#ifdef ENABLE_VULKAN_DEBUG_MARKERS
    acp_vulkan::debug_set_object_name(logical_device, module, VK_OBJECT_TYPE_SHADER_MODULE, name);
#endif

    if (module == VK_NULL_HANDLE)
        return nullptr;

    shader* out = new shader();
    out->module = module;
    if (!update_shader_metadata(out, data, data_size))
    {
        vkDestroyShaderModule(logical_device, out->module, host_allocator);
        delete out;
        return nullptr;
    }
    return out;
}

acp_vulkan::shader* acp_vulkan::shader_init(VkDevice logical_device, VkAllocationCallbacks* host_allocator, const char* path)
{
    FILE* shader_bytes = fopen(path, "rb");
    fseek(shader_bytes, 0, SEEK_END);
    long shader_size = ftell(shader_bytes);
    fseek(shader_bytes, 0, SEEK_SET);
    uint32_t* shader_data = new uint32_t[shader_size];
    fread(shader_data, sizeof(uint32_t), shader_size / sizeof(uint32_t), shader_bytes);
    fclose(shader_bytes);
    shader* out = shader_init(logical_device, host_allocator, shader_data, shader_size / sizeof(uint32_t), path);
    delete shader_data;
    return out;
}

void acp_vulkan::shader_destroy(VkDevice logical_device, VkAllocationCallbacks* host_allocator, shader* shader)
{
    if (!shader)
        return;

    if (shader->module != VK_NULL_HANDLE)
    {
        vkDestroyShaderModule(logical_device, shader->module, host_allocator);
        shader->module = VK_NULL_HANDLE;
    }

    delete shader;
}

static std::vector<acp_vulkan::program::layout> createDescriptorLayoutsAssumeSharedSets(VkDevice logical_device, VkAllocationCallbacks* host_allocator, acp_vulkan::shaders shaders)
{
    std::vector<acp_vulkan::program::layout> out;
    std::map<uint32_t, uint32_t> set_ids_to_num_bindings;
    for (const acp_vulkan::shader* const shader : shaders)
    {
        for (auto& binding : shader->metadata.resource_bindings)
        {
            set_ids_to_num_bindings[binding.set] += 1;
        }
    }

    for (const auto& current_set_id : set_ids_to_num_bindings)
    {
        std::vector<VkDescriptorSetLayoutBinding> bindings;
        for (const acp_vulkan::shader* const shader : shaders)
        {
            for (auto& binding : shader->metadata.resource_bindings)
            {
                if (binding.set != current_set_id.first)
                    continue;

                VkDescriptorSetLayoutBinding* currentBinding = nullptr;
                auto sameBinding = std::find_if(begin(bindings), end(bindings), [&binding](const VkDescriptorSetLayoutBinding oldBinding) {return oldBinding.binding == binding.binding; });
                if (sameBinding == bindings.end())
                {
                    VkDescriptorSetLayoutBinding new_binding{};
                    new_binding.binding = binding.binding;
                    new_binding.descriptorType = binding.resource_type;
                    new_binding.stageFlags = shader->type;
                    new_binding.descriptorCount = 1;// todo(alex) : Parse this in the shader.
                    bindings.emplace_back(std::move(new_binding));
                    currentBinding = &bindings[bindings.size() - 1];
                }
                else
                    currentBinding = &*sameBinding;

                if (!currentBinding)
                    return out;

                currentBinding->stageFlags |= shader->type;

                //todo(alex) : Set the descriptor count to the highest value of the bindings.

                if (currentBinding->descriptorType != binding.resource_type)
                    return out;
            }
        }

        VkDescriptorSetLayoutCreateInfo decriptor_layout_create_info{ VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO };
        decriptor_layout_create_info.pBindings = bindings.data();
        decriptor_layout_create_info.bindingCount = uint32_t(bindings.size());

        VkDescriptorSetLayout descriptor_layout = VK_NULL_HANDLE;
        vkCreateDescriptorSetLayout(logical_device, &decriptor_layout_create_info, host_allocator, &descriptor_layout);
#ifdef ENABLE_VULKAN_DEBUG_MARKERS
        acp_vulkan::debug_set_object_name(logical_device, descriptor_layout, VK_OBJECT_TYPE_DESCRIPTOR_SET_LAYOUT, "acp_descriptor_layout");
#endif
        if (descriptor_layout == VK_NULL_HANDLE)
        {
            for (auto l : out)
                vkDestroyDescriptorSetLayout(logical_device, l.first, host_allocator);
            out.clear();
            return out;
        }
        out.push_back({ descriptor_layout, std::move(bindings) });
    }

    return out;
}

static std::vector<acp_vulkan::program::layout> createDescriptorLayoutsNoSharedSets(VkDevice logical_device, VkAllocationCallbacks* host_allocator, acp_vulkan::shaders shaders)
{
    std::vector<acp_vulkan::program::layout> out;
    for (const acp_vulkan::shader* const shader : shaders)
    {
        std::map<uint32_t, uint32_t> set_ids_to_num_bindings;
        for (auto& binding : shader->metadata.resource_bindings)
        {
            set_ids_to_num_bindings[binding.set] += 1;
        }
        std::vector<VkDescriptorSetLayoutBinding> bindings;
        for (const auto& id_to_count : set_ids_to_num_bindings)
        {
            bindings.clear();
            for (auto& binding : shader->metadata.resource_bindings)
            {
                if (binding.set == id_to_count.first)
                {
                    VkDescriptorSetLayoutBinding new_binding{};
                    new_binding.binding = binding.binding;
                    new_binding.descriptorType = binding.resource_type;
                    new_binding.stageFlags = shader->type;
                    new_binding.descriptorCount = 1;// todo(alex) : Parse this in the shader.
                    bindings.emplace_back(std::move(new_binding));
                }
            }

            VkDescriptorSetLayoutCreateInfo decriptor_layout_create_info{ VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO };
            decriptor_layout_create_info.pBindings = bindings.data();
            decriptor_layout_create_info.bindingCount = uint32_t(bindings.size());

            VkDescriptorSetLayout descriptor_layout = VK_NULL_HANDLE;
            vkCreateDescriptorSetLayout(logical_device, &decriptor_layout_create_info, host_allocator, &descriptor_layout);
#ifdef ENABLE_VULKAN_DEBUG_MARKERS
            acp_vulkan::debug_set_object_name(logical_device, descriptor_layout, VK_OBJECT_TYPE_DESCRIPTOR_SET_LAYOUT, "acp_descriptor_layout");
#endif
            if (descriptor_layout == VK_NULL_HANDLE)
            {
                for (auto l : out)
                    vkDestroyDescriptorSetLayout(logical_device, l.first, host_allocator);
                out.clear();
                return out;
            }
            out.push_back({ descriptor_layout, std::move(bindings) });
        }
    }
    return out;
}

struct pipeline_layout_data
{
    const std::vector<acp_vulkan::program::layout> descriptor_layouts;
    VkPipelineLayout pipeline_layout;
};
static pipeline_layout_data create_pipeline_layout(VkDevice logical_device, VkAllocationCallbacks* host_allocator, acp_vulkan::shaders shaders, size_t push_constant_size, bool sharedDescriptorSets)
{
    VkPipelineLayout pipeline_layout = VK_NULL_HANDLE;
    VkPipelineLayoutCreateInfo layout_create_info{ VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO };

    const std::vector<acp_vulkan::program::layout> descriptor_layouts =
        sharedDescriptorSets ? createDescriptorLayoutsAssumeSharedSets(logical_device, host_allocator, shaders) :
        createDescriptorLayoutsNoSharedSets(logical_device, host_allocator, shaders);

    std::vector<VkDescriptorSetLayout> raw_layouts;
    raw_layouts.reserve(descriptor_layouts.size());
    for (auto ii : descriptor_layouts)
        raw_layouts.push_back(ii.first);

    layout_create_info.pSetLayouts = raw_layouts.data();
    layout_create_info.setLayoutCount = uint32_t(raw_layouts.size());

    VkPushConstantRange push_constant_range{};
    push_constant_range.offset = 0;
    push_constant_range.size = uint32_t(push_constant_size);
    push_constant_range.stageFlags = 0;
    for (const acp_vulkan::shader* const shader : shaders)
    {
        if (shader->uses_constants)
            push_constant_range.stageFlags |= shader->type;
    }
    layout_create_info.pPushConstantRanges = &push_constant_range;
    layout_create_info.pushConstantRangeCount = 1;

    if (push_constant_range.stageFlags == 0)
        layout_create_info.pushConstantRangeCount = 0;

    if (vkCreatePipelineLayout(logical_device, &layout_create_info, host_allocator, &pipeline_layout) != VK_SUCCESS)
        return { {}, VK_NULL_HANDLE };

#ifdef ENABLE_VULKAN_DEBUG_MARKERS
    acp_vulkan::debug_set_object_name(logical_device, pipeline_layout, VK_OBJECT_TYPE_PIPELINE_LAYOUT, "acp_pipeline_layout");
#endif

    return {std::move(descriptor_layouts), pipeline_layout};
}

acp_vulkan::program* acp_vulkan::compute_program_init(VkDevice logical_device, VkAllocationCallbacks* host_allocator, const shader* shader, size_t push_constant_size, bool sharedDescriptorSets, const char* name)
{
    VkComputePipelineCreateInfo create_info{ VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO };
    //todo(alex) : Use this and the pipeline cache.
    create_info.basePipelineHandle = VK_NULL_HANDLE;
    create_info.basePipelineIndex = 0;
    create_info.flags = 0;

    pipeline_layout_data pipeline_layout_data = create_pipeline_layout(logical_device, host_allocator, acp_vulkan::shaders{shader}, push_constant_size, sharedDescriptorSets);
    if (pipeline_layout_data.pipeline_layout == VK_NULL_HANDLE)
        return nullptr;

    create_info.layout = pipeline_layout_data.pipeline_layout;
    
    create_info.stage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    create_info.stage.stage = shader->type;
    create_info.stage.module = shader->module;
    create_info.stage.pName = shader->metadata.entry_point.c_str();

    VkPipeline output_pipeline = VK_NULL_HANDLE;
    VkPipelineCache pipeline_cache = VK_NULL_HANDLE;
    if (vkCreateComputePipelines(logical_device, pipeline_cache, 1, &create_info, host_allocator, &output_pipeline) != VK_SUCCESS)
        return nullptr;

#ifdef ENABLE_VULKAN_DEBUG_MARKERS
    acp_vulkan::debug_set_object_name(logical_device, output_pipeline, VK_OBJECT_TYPE_PIPELINE, name);
#endif
    if (output_pipeline == VK_NULL_HANDLE)
    {
        vkDestroyPipelineLayout(logical_device, pipeline_layout_data.pipeline_layout, host_allocator);
        return nullptr;
    }

    program* out = new program();
    out->pipeline = output_pipeline;
    out->pipeline_layout = pipeline_layout_data.pipeline_layout;
    out->descriptor_layouts = std::move(pipeline_layout_data.descriptor_layouts);
    return out;
}

acp_vulkan::program* acp_vulkan::graphics_program_init(VkDevice logical_device, VkAllocationCallbacks* host_allocator, shaders shaders, input_attributes vertex_input_attributes, size_t push_constant_size, bool use_depth, bool write_to_depth, bool sharedDescriptorSets, uint32_t color_attachment_count, const VkFormat* color_attachment_formats, VkFormat depth_attachment_format, VkFormat stencil_attachment_format, const char* name)
{
    VkGraphicsPipelineCreateInfo create_info{ VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO };
    create_info.stageCount = uint32_t(shaders.size());
    std::vector<VkPipelineShaderStageCreateInfo> shader_stages;
    for (const shader* const shader : shaders)
    {
        VkPipelineShaderStageCreateInfo shader_create_info{ VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO };
        shader_create_info.stage = shader->type;
        shader_create_info.module = shader->module;
        shader_create_info.pName = shader->metadata.entry_point.c_str();
        shader_stages.push_back(shader_create_info);
    }
    create_info.pStages = shader_stages.data();
    create_info.stageCount = uint32_t(shader_stages.size());

    std::vector<VkVertexInputAttributeDescription> attribute_descriptions;
    std::vector<VkVertexInputBindingDescription> attribute_bindings;

    for (const input_attribute_data& vertex_input_attribute : vertex_input_attributes)
    {
        VkVertexInputBindingDescription binding_description;
        binding_description.binding = vertex_input_attribute.binding;
        binding_description.inputRate = vertex_input_attribute.input_rate;
        binding_description.stride = vertex_input_attribute.stride;
        attribute_bindings.emplace_back(std::move(binding_description));

        for (const shader* const shader : shaders)
        {
            if (shader->type == VK_SHADER_STAGE_VERTEX_BIT)
            {
                for (auto& shader_usage_attribute : shader->metadata.inputs)
                {
                    for (uint32_t ii = 0; ii < vertex_input_attribute.locations.size(); ++ii)
                    {
                        //todo(alex): Check that the user provided locations are valid.
                        if (shader_usage_attribute.location == vertex_input_attribute.locations[ii])
                        {
                            VkVertexInputAttributeDescription attribute_description{};
                            attribute_description.binding = vertex_input_attribute.binding;
                            attribute_description.offset = vertex_input_attribute.offsets[ii];
                            attribute_description.location = vertex_input_attribute.locations[ii];
                            switch (shader_usage_attribute.field_type)
                            {
                            //todo(alex) : Check that bools are actualy transfered as signed ints, this is what I remember.
                            case shader::meta::type::bool_type:
                                attribute_description.format = VK_FORMAT_R32_UINT;
                                break;
                            case shader::meta::type::bvec2_type:
                                attribute_description.format = VK_FORMAT_R32G32_UINT;
                                break;
                            case shader::meta::type::bvec3_type:
                                attribute_description.format = VK_FORMAT_R32G32B32_UINT;
                                break;
                            case shader::meta::type::bvec4_type:
                                attribute_description.format = VK_FORMAT_R32G32B32A32_UINT;
                                break;

                            case shader::meta::type::double_type:
                                attribute_description.format = VK_FORMAT_R64_SFLOAT;
                                break;
                            case shader::meta::type::dvec2_type:
                                attribute_description.format = VK_FORMAT_R64G64_SFLOAT;
                                break;
                            case shader::meta::type::dvec3_type:
                                attribute_description.format = VK_FORMAT_R64G64B64_SFLOAT;
                                break;
                            case shader::meta::type::dvec4_type:
                                attribute_description.format = VK_FORMAT_R64G64B64A64_SFLOAT;
                                break;


                            case shader::meta::type::float_type:
                                attribute_description.format = VK_FORMAT_R32_SFLOAT;
                                break;
                            case shader::meta::type::vec2_type:
                                attribute_description.format = VK_FORMAT_R32G32_SFLOAT;
                                break;
                            case shader::meta::type::vec3_type:
                                attribute_description.format = VK_FORMAT_R32G32B32_SFLOAT;
                                break;
                            case shader::meta::type::vec4_type:
                                attribute_description.format = VK_FORMAT_R32G32B32A32_SFLOAT;
                                break;


                            case shader::meta::type::int_type:
                                attribute_description.format = VK_FORMAT_R32_SINT;
                                break;
                            case shader::meta::type::ivec2_type:
                                attribute_description.format = VK_FORMAT_R32G32_SINT;
                                break;
                            case shader::meta::type::ivec3_type:
                                attribute_description.format = VK_FORMAT_R32G32B32_SINT;
                                break;
                            case shader::meta::type::ivec4_type:
                                attribute_description.format = VK_FORMAT_R32G32B32A32_SINT;
                                break;


                            case shader::meta::type::uint_type:
                                attribute_description.format = VK_FORMAT_R32_UINT;
                                break;
                            case shader::meta::type::uvec2_type:
                                attribute_description.format = VK_FORMAT_R32G32_UINT;
                                break;
                            case shader::meta::type::uvec3_type:
                                attribute_description.format = VK_FORMAT_R32G32B32_UINT;
                                break;
                            case shader::meta::type::uvec4_type:
                                attribute_description.format = VK_FORMAT_R32G32B32A32_UINT;
                                break;

                            }
                            attribute_descriptions.push_back(std::move(attribute_description));
                        }
                    }
                }
            }
        }
    }

    VkPipelineVertexInputStateCreateInfo vertex_input_state_create_info{ VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO };
    vertex_input_state_create_info.pVertexAttributeDescriptions = attribute_descriptions.data();
    vertex_input_state_create_info.vertexAttributeDescriptionCount = uint32_t(attribute_descriptions.size());

    vertex_input_state_create_info.pVertexBindingDescriptions = attribute_bindings.data();
    vertex_input_state_create_info.vertexBindingDescriptionCount = uint32_t(attribute_bindings.size());

    create_info.pVertexInputState = &vertex_input_state_create_info;

    VkPipelineInputAssemblyStateCreateInfo input_assembly_create_info{ VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO };
    input_assembly_create_info.primitiveRestartEnable = false;
    //todo(alex): Add an option to support other primitive topology types.
    input_assembly_create_info.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    create_info.pInputAssemblyState = &input_assembly_create_info;

    create_info.pTessellationState = VK_NULL_HANDLE;

    //Using dynamic state, this is here so we can pass validation.
    VkPipelineViewportStateCreateInfo viewport_create_info{ VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO };
    viewport_create_info.viewportCount = 1;
    VkViewport viewport{};
    create_info.pViewportState = &viewport_create_info;
    viewport_create_info.scissorCount = 1;

    VkPipelineRasterizationStateCreateInfo resterization_create_info{ VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO };
    resterization_create_info.cullMode = VK_CULL_MODE_BACK_BIT;
    resterization_create_info.frontFace = VK_FRONT_FACE_CLOCKWISE;
    resterization_create_info.lineWidth = 1.0f;
    resterization_create_info.polygonMode = VK_POLYGON_MODE_FILL;
    create_info.pRasterizationState = &resterization_create_info;

    VkPipelineMultisampleStateCreateInfo multisample_create_info{ VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO };
    multisample_create_info.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;//todo(alex) : AA as options.
    create_info.pMultisampleState = &multisample_create_info;

    VkPipelineDepthStencilStateCreateInfo depth_create_info{ VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO };
    depth_create_info.depthTestEnable = use_depth;
    depth_create_info.depthWriteEnable = write_to_depth;
    depth_create_info.depthCompareOp = VK_COMPARE_OP_LESS;
    depth_create_info.depthBoundsTestEnable = false;
    depth_create_info.stencilTestEnable = false;
    create_info.pDepthStencilState = &depth_create_info;

    VkPipelineColorBlendAttachmentState color_blend_attachment{};
    color_blend_attachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
    color_blend_attachment.blendEnable = VK_FALSE;

    VkPipelineColorBlendStateCreateInfo color_blend_create_info{ VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO };
    color_blend_create_info.logicOpEnable = VK_FALSE;
    color_blend_create_info.logicOp = VK_LOGIC_OP_COPY;
    color_blend_create_info.attachmentCount = 1;
    color_blend_create_info.pAttachments = &color_blend_attachment;
    color_blend_create_info.blendConstants[0] = 0.0f;
    color_blend_create_info.blendConstants[1] = 0.0f;
    color_blend_create_info.blendConstants[2] = 0.0f;
    color_blend_create_info.blendConstants[3] = 0.0f;
    create_info.pColorBlendState = &color_blend_create_info;

    VkDynamicState dynamic_states[] = { VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR };
    VkPipelineDynamicStateCreateInfo dynamic_state_create_info{ VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO };
    dynamic_state_create_info.pDynamicStates = dynamic_states;
    dynamic_state_create_info.dynamicStateCount = uint32_t(sizeof(dynamic_states) / sizeof(dynamic_states[0]));
    create_info.pDynamicState = &dynamic_state_create_info;

    VkPipelineRenderingCreateInfo rendering_info = { VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO };
    rendering_info.colorAttachmentCount = color_attachment_count;
    rendering_info.pColorAttachmentFormats = color_attachment_formats;
    rendering_info.depthAttachmentFormat = depth_attachment_format;
    rendering_info.stencilAttachmentFormat = stencil_attachment_format;
    create_info.pNext = &rendering_info;

    pipeline_layout_data pipeline_layout_data = create_pipeline_layout(logical_device, host_allocator, shaders, push_constant_size, sharedDescriptorSets);
    if (pipeline_layout_data.pipeline_layout == VK_NULL_HANDLE)
        return nullptr;

    create_info.layout = pipeline_layout_data.pipeline_layout;

    //todo(alex) : Use this and the pipeline cache.
    create_info.renderPass = VK_NULL_HANDLE;
    create_info.subpass = 0;
    create_info.basePipelineHandle = VK_NULL_HANDLE;
    create_info.basePipelineIndex = 0;

    VkPipeline output_pipeline = VK_NULL_HANDLE;
    VkPipelineCache pipeline_cache = VK_NULL_HANDLE;
    if (vkCreateGraphicsPipelines(logical_device, pipeline_cache, 1, &create_info, host_allocator, &output_pipeline) != VK_SUCCESS)
        return nullptr;
    if (output_pipeline == VK_NULL_HANDLE)
    {
        vkDestroyPipelineLayout(logical_device, pipeline_layout_data.pipeline_layout, host_allocator);
        return nullptr;
    }

#ifdef ENABLE_VULKAN_DEBUG_MARKERS
    acp_vulkan::debug_set_object_name(logical_device, output_pipeline, VK_OBJECT_TYPE_PIPELINE, name);
#endif

    program* out = new program();
    out->pipeline = output_pipeline;
    out->pipeline_layout = pipeline_layout_data.pipeline_layout;
    out->descriptor_layouts = std::move(pipeline_layout_data.descriptor_layouts);
    return out;
}

void acp_vulkan::program_destroy(VkDevice logical_device, VkAllocationCallbacks* host_allocator, program* program)
{
    if (program->pipeline)
    {
        vkDestroyPipeline(logical_device, program->pipeline, host_allocator);
        program->pipeline = VK_NULL_HANDLE;
    }
    for (auto descriptor_layout : program->descriptor_layouts)
    {
        vkDestroyDescriptorSetLayout(logical_device, descriptor_layout.first, host_allocator);
    }
    program->descriptor_layouts.clear();
    if (program->pipeline_layout)
    {
        vkDestroyPipelineLayout(logical_device, program->pipeline_layout, host_allocator);
        program->pipeline_layout = VK_NULL_HANDLE;
    }
    delete program;
}