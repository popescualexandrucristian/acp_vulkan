#include "acp_gltf_vulkan.h"
#include <vulkan/vulkan.h>
#include <ctype.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <stdio.h>
#include <map>

template<typename A, typename B>
using pair = acp_vulkan::gltf_data::pair<A,B>;

template<typename T>
using data_view = acp_vulkan::gltf_data::data_view<T>;

template<typename T>
static void free_gltf_buffer(acp_vulkan::gltf_data::data_view<T>& data_view, VkAllocationCallbacks* host_allocator)
{
	if (!data_view.data)
		return;

	if (host_allocator)
		host_allocator->pfnFree(host_allocator->pUserData, (void*)data_view.data);
	else
		delete[] data_view.data;

	data_view.data = nullptr;
	data_view.data_length = 0;
}

template<typename T, size_t INITIAL_CAPACITY = 16>
struct temp_data_view
{
	T* data;
	size_t data_length;
	size_t data_capacity;
	VkAllocationCallbacks* host_allocator{ nullptr };

	void free()
	{
		if (host_allocator)
			host_allocator->pfnFree(host_allocator->pUserData, data);
		else
			delete[] data;

		data = nullptr;
		data_capacity = 0;
		data_length = 0;
	}

	temp_data_view& operator=(temp_data_view&& other)
	{
		free();

		data = other.data;
		data_capacity = other.data_capacity;
		data_length = other.data_length;

		other.data = nullptr;
		other.data_capacity = 0;
		other.data_length = 0;
	}
	temp_data_view(temp_data_view&& other)
	{
		data = other.data;
		data_capacity = other.data_capacity;
		data_length = other.data_length;

		other.data = nullptr;
		other.data_capacity = 0;
		other.data_length = 0;
	}
	temp_data_view() :data(nullptr), data_length(0), data_capacity(0), host_allocator(nullptr)
	{
	}
	~temp_data_view()
	{
		free();
	}

	void reserve(size_t new_size)
	{
		if (data_capacity == 0)
		{
			data_capacity = INITIAL_CAPACITY > new_size ? INITIAL_CAPACITY : new_size;
			assert(data_length == 0);
			assert(!data);
		}

		T* old_data_ptr = nullptr;
		size_t old_data_capacity = 0;
		if (data_capacity == data_length || new_size > data_capacity)
		{
			old_data_ptr = data;
			old_data_capacity = data_capacity;
			data = nullptr;
			data_capacity = data_capacity >= new_size ? data_capacity : new_size;
		}

		if (!data)
		{
			data = host_allocator ?
				reinterpret_cast<T*>(host_allocator->pfnAllocation(host_allocator->pUserData, data_capacity, alignof(T), VK_SYSTEM_ALLOCATION_SCOPE_OBJECT))
				: new T[data_capacity];
		}

		if (old_data_ptr)
		{
			for (size_t ii = 0; ii < data_length; ++ii)
				data[ii] = old_data_ptr[ii];

			if (host_allocator)
				host_allocator->pfnFree(host_allocator->pUserData, old_data_ptr);
			else
				delete[] old_data_ptr;
		}
	}

	void emplace_back(T&& value)
	{
		if (data_capacity == 0)
			reserve(INITIAL_CAPACITY);
		else if (data_capacity == data_length)
			reserve(data_capacity * 2);

		data[data_length++] = std::move(value);
	}

	data_view<T> to()
	{
		data_view<T> out;
		out.data = data;
		out.data_length = data_length;
		data = nullptr;
		data_length = 0;
		data_capacity = 0;
		return out;
	}
};

using string_view = acp_vulkan::gltf_data::string_view;

static string_view copy(string_view from, VkAllocationCallbacks* host_allocator)
{
	char* data = host_allocator ?
		reinterpret_cast<char*>(host_allocator->pfnAllocation(host_allocator->pUserData, from.data_length, 1, VK_SYSTEM_ALLOCATION_SCOPE_OBJECT))
		: new char[from.data_length];
	memcpy(data, from.data, from.data_length);
	return { .data = data, .data_length = from.data_length };
}

#define MAX_TOKENS_PER_ENTITY (1024 * 1024)


#define TOKENS_WITH_DIFFERENT_REPRESENATATION(X) \
	X(open_curly, {)\
	X(none, ¦)\
	X(close_curly, })\
	X(open_bracket, [)\
	X(closed_bracket, ])\
	X(colon, :) \
	X(true_value, true) \
	X(false_value, false)

#define TOKENS_FOR_ALPHA_MODES(X) \
	X(OPAQUE) \
	X(MASK) \
	X(alphaCutoff) \
	X(BLEND)

#define TOKENS_FOR_ATTRIBUES(X) \
	X(TEXCOORD_0)\
	X(TEXCOORD_1)\
	X(TEXCOORD_2)\
	X(TEXCOORD_3)\
	X(TEXCOORD_4)\
	X(TEXCOORD_5)\
	X(TEXCOORD_6)\
	X(TEXCOORD_7)\
	X(TEXCOORD_8)\
	X(TEXCOORD_9)\
	X(POSITION)\
	X(NORMAL)\
	X(TANGENT)\
	X(COLOR_0)\
	X(COLOR_1)\
	X(COLOR_2)\
	X(COLOR_3)\
	X(COLOR_4)\
	X(COLOR_5)\
	X(COLOR_6)\
	X(COLOR_7)\
	X(COLOR_8)\
	X(COLOR_9)\
	X(JOINTS_0)\
	X(JOINTS_1)\
	X(JOINTS_2)\
	X(JOINTS_3)\
	X(JOINTS_4)\
	X(JOINTS_5)\
	X(JOINTS_6)\
	X(JOINTS_7)\
	X(JOINTS_8)\
	X(JOINTS_9)\
	X(WEIGHTS_0)\
	X(WEIGHTS_1)\
	X(WEIGHTS_2)\
	X(WEIGHTS_3)\
	X(WEIGHTS_4)\
	X(WEIGHTS_5)\
	X(WEIGHTS_6)\
	X(WEIGHTS_7)\
	X(WEIGHTS_8)\
	X(WEIGHTS_9)

#define TOKENS(X) \
	X(accessors)\
	X(bufferView)\
	X(bufferViews)\
	X(componentType)\
	X(count)\
	X(type)\
	X(VEC2)\
	X(VEC3)\
	X(VEC4)\
	X(max)\
	X(min)\
	X(asset)\
	X(generator)\
	X(version)\
	X(buffer)\
	X(byteLength)\
	X(byteOffset)\
	X(buffers)\
	X(uri)\
	X(images)\
	X(meshes)\
	X(primitives)\
	X(attributes)\
	X(indices)\
	X(material)\
	X(name)\
	X(materials)\
	X(pbrMetallicRoughness)\
	X(baseColorTexture)\
	X(index)\
	X(normalTexture)\
	X(nodes)\
	X(mesh)\
	X(translation)\
	X(rotation)\
	X(scale)\
	X(weights)\
	X(scene)\
	X(scenes)\
	X(textures)\
	X(source) \
	X(SCALAR) \
	X(mimeType) \
	X(normalized) \
	X(sparse) \
	X(values) \
	X(sampler) \
	X(mode) \
	X(targets) \
	X(texCoord) \
	X(baseColorFactor) \
	X(metallicFactor) \
	X(roughnessFactor) \
	X(metallicRoughnessTexture) \
	X(strength) \
	X(occlusionTexture) \
	X(emissiveTexture) \
	X(emissiveFactor) \
	X(alphaMode) \
	X(doubleSided) \
	X(skin) \
	X(matrix) \
	X(samplers) \
	X(magFilter) \
	X(minFilter) \
	X(wrapS) \
	X(wrapT) \
	X(skins) \
	X(joints) \
	X(inverseBindMatrices) \
	X(skeleton) \
	X(cameras) \
	X(xmag) \
	X(ymag) \
	X(zfar) \
	X(znear) \
	X(aspectRatio) \
	X(yfov) \
	X(perspective) \
	X(orthographic) \
	X(animations) \
	X(channels) \
	X(target) \
	X(node) \
	X(path) \
	X(input) \
	X(output) \
	X(interpolation) \
	X(extensions) \
	X(MSFT_texture_dds)

#define TO_ENUM_VALUE1(x, y) x,
#define TO_ENUM_VALUE2(x) x,
enum class token_types : uint32_t {
	TOKENS_WITH_DIFFERENT_REPRESENATATION(TO_ENUM_VALUE1)
	TOKENS(TO_ENUM_VALUE2)
	TOKENS_FOR_ATTRIBUES(TO_ENUM_VALUE2)
	TOKENS_FOR_ALPHA_MODES(TO_ENUM_VALUE2)
	comma,
	TOKENS_COUNT,
	is_float,
	is_int,
	is_string,
	eof
};
#undef TO_ENUM_VALUE1
#undef TO_ENUM_VALUE2

#define TO_STRING_VALUE1(x, y) #y,
#define TO_STRING_VALUE2(x) "\""#x"\"",
static const char* token_as_strings[] = {
	TOKENS_WITH_DIFFERENT_REPRESENATATION(TO_STRING_VALUE1)
	TOKENS(TO_STRING_VALUE2)
	TOKENS_FOR_ATTRIBUES(TO_STRING_VALUE2)
	TOKENS_FOR_ALPHA_MODES(TO_STRING_VALUE2)
	","
};
#undef TO_STRING_VALUE1
#undef TO_STRING_VALUE2

#define TO_STRING_VALUE1(x, y) false,
#define TO_STRING_VALUE2(x) true,
static const bool is_string_like[] = {
	TOKENS_WITH_DIFFERENT_REPRESENATATION(TO_STRING_VALUE1)
	TOKENS(TO_STRING_VALUE2)
	TOKENS_FOR_ATTRIBUES(TO_STRING_VALUE2)
	TOKENS_FOR_ALPHA_MODES(TO_STRING_VALUE2)
	false,//comma
	false, //TOKENS_COUNT,
	false, //is_float,
	false, //is_int,
	true, //is_string,
	false //eof
};
#undef TO_STRING_VALUE1
#undef TO_STRING_VALUE2

#define TOKEN_TO_ATTRIBUTE_DECLARATION(x) {token_types::x, acp_vulkan::gltf_data::attribute::x},
static const std::map<token_types, acp_vulkan::gltf_data::attribute> tokens_to_attribute
{
	TOKENS_FOR_ATTRIBUES(TOKEN_TO_ATTRIBUTE_DECLARATION)
};
#undef TOKEN_TO_ATTRIBUTE_DECLARATION

static const std::map<token_types, acp_vulkan::gltf_data::material::alpha_mode_type> tokens_to_alpha_mode
{
	{token_types::OPAQUE, acp_vulkan::gltf_data::material::alpha_mode_type::OPAQUE},
	{token_types::MASK, acp_vulkan::gltf_data::material::alpha_mode_type::MASK},
	{token_types::alphaCutoff, acp_vulkan::gltf_data::material::alpha_mode_type::ALPHA_CUTOFF},
	{token_types::BLEND, acp_vulkan::gltf_data::material::alpha_mode_type::BLEND}
};
#undef TOKEN_TO_ALPHA_MODE_DECLARATION

struct token {
	token_types type;
	acp_vulkan::gltf_data::string_view view;
	union value
	{
		double		as_real;
		int64_t		as_int;
		bool		as_bool;
	} value;
};

struct tokenizer_state {
	const char* data;
	size_t data_size;
	size_t next_char;
	VkAllocationCallbacks* host_allocator;
};

template<typename T>
struct on_scope_end
{
	T* target;
	typedef void(*scope_end_call_type)(T*, VkAllocationCallbacks* host_allocator);
	scope_end_call_type scope_end_call;
	VkAllocationCallbacks* host_allocator;
	bool droped{ true };

	on_scope_end(T* target, scope_end_call_type scope_end_call, VkAllocationCallbacks* host_allocator)
		: target(target)
		, scope_end_call(scope_end_call)
		, host_allocator(host_allocator)
		, droped(false)
	{}

	void drop()
	{
		droped = true;
	}

	~on_scope_end()
	{
		if (droped)
			return;

		(*scope_end_call)(target, host_allocator);
	}
};

struct peeked_token
{
	token token;
	size_t next;
};

enum class return_value {
	none,
	false_value,
	true_value,
	error_value
};

static peeked_token peek_token(tokenizer_state* state)
{
	while (state->data_size > state->next_char)
	{
		bool is_skipable = isspace(*(state->data + state->next_char));
		if (!is_skipable && *(state->data + state->next_char) == '\n')
			is_skipable = true;
		if (!is_skipable && *(state->data + state->next_char) == '\r')
			is_skipable = true;
		if (!is_skipable)
			break;
		state->next_char++;
	}
	if (state->data_size > state->next_char)
	{
		for (uint32_t ii = 0; ii < uint32_t(token_types::TOKENS_COUNT); ++ii)
		{
			size_t token_length = strlen(token_as_strings[size_t(ii)]);
			if ((state->next_char + token_length) <= state->data_size)
			{
				if (strncmp(state->data + state->next_char, token_as_strings[size_t(ii)], token_length) == 0)
				{
					token out{
						.type = token_types(ii),
						.view {
							.data = state->data + state->next_char + 1,
							.data_length = token_length - 2
						}
					};
					return { out, state->next_char + token_length };
				}
			}
		}
		if (*(state->data + state->next_char) == '\"')
		{
			size_t last_location = state->next_char + 1;
			while (last_location < state->data_size && *(state->data + last_location) != '\"')
			{
				++last_location;
			}
			if (*(state->data + last_location) == '\"')
			{
				token out{
					.type = token_types::is_string,
					.view {
						.data = state->data + state->next_char + 1,
						.data_length = (last_location + 1) - state->next_char - 2
					},
				};
				return { out, last_location + 1 };
			}
		}
		{
			char* end = nullptr;
			errno = 0;
			size_t token_length = 0;
			token number_token{};

			long long value_as_ll = strtoll(state->data + state->next_char, &end, 0);
			if ((errno == 0) && end != state->data + state->next_char && end != nullptr)
			{
				token_length = end - (state->data + state->next_char);
				number_token = {
					.type = token_types::is_int,
					.view {
						.data = state->data + state->next_char,
						.data_length = token_length
					},
					.value{.as_int = value_as_ll }
				};
			}

			end = nullptr;
			errno = 0;
			double value_as_d = strtod(state->data + state->next_char, &end);
			if ((errno == 0) && end != state->data + state->next_char && end != nullptr)
			{
				size_t token_length_as_double = end - (state->data + state->next_char);
				if (token_length < token_length_as_double)
				{
					token_length = token_length_as_double;
					number_token = {
						.type = token_types::is_float,
						.view {
							.data = state->data + state->next_char,
							.data_length = token_length_as_double
						},
						.value{.as_real = value_as_d }
					};
				}
			}

			if (number_token.view.data_length > 0)
				return { number_token, state->next_char + token_length };
			
		}
		
		if (state->next_char == state->data_size)
		{
			token out{
					.type = token_types::eof,
					.view {
						.data = state->data + state->next_char,
						.data_length = 0
					},
			};
			return { out, 0 };
		}

		token out{
				.type = token_types::none,
				.view {
					.data = state->data + state->next_char,
					.data_length = 0
				},
		};
		return { out , 0 };
	}

	token out{
		.type = token_types::eof,
		.view {
			.data = state->data + state->next_char,
			.data_length = 0
		},
	};
	return { out, 0 };
}

static return_value expect(const token& t, token_types type)
{
	assert(t.type == type);
	if (t.type == type)
		return return_value::true_value;
	return return_value::error_value;
}

static token next_token(tokenizer_state* state)
{
	peeked_token p = peek_token(state);
	if (p.next != 0)
		state->next_char = p.next;
	return p.token;
}

static return_value try_read_string_property_in_to(tokenizer_state* state, token_types type, acp_vulkan::gltf_data::string_view* target, bool allocate_new_data)
{
	peeked_token t = peek_token(state);
	if (t.token.type == type)
	{
		next_token(state);
		if (expect(next_token(state), token_types::colon) == return_value::error_value)
			return return_value::error_value;
		token value = next_token(state);
		if (!((size_t(value.type) > 0) && (size_t(value.type) < sizeof(is_string_like)/ sizeof(is_string_like[0])) && is_string_like[size_t(value.type)]))
			return return_value::error_value;
		
		if (allocate_new_data)
			*target = copy(value.view, state->host_allocator);
		else
			*target = value.view;
		
		return return_value::true_value;
	}
	return return_value::false_value;
}

template<typename T>
static return_value try_read_real_property_to(tokenizer_state* state, token_types type, T* target)
{
	peeked_token t = peek_token(state);
	if (t.token.type == type)
	{
		next_token(state);
		if (expect(next_token(state), token_types::colon) == return_value::error_value)
			return return_value::error_value;
		token value = next_token(state);
		if (value.type == token_types::is_float)
		{
			*target = T(value.value.as_real);
			return return_value::true_value;
		} 
		else if (value.type == token_types::is_int)
		{
			*target = T(value.value.as_int);
			return return_value::true_value;
		}

		return return_value::error_value;
	}
	return return_value::false_value;
}

template<typename T>
static return_value try_read_bool_property_to(tokenizer_state* state, token_types type, T* target)
{
	peeked_token t = peek_token(state);
	if (t.token.type == type)
	{
		next_token(state);
		if (expect(next_token(state), token_types::colon) == return_value::error_value)
			return return_value::error_value;
		token value = next_token(state);
		if (value.type == token_types::true_value)
		{
			*target = T(true);
			return return_value::true_value;
		}
		else if (value.type == token_types::false_value)
		{
			*target = T(false);
			return return_value::true_value;
		}

		return return_value::error_value;
	}
	return return_value::false_value;
}

template<typename T>
static return_value try_read_int_property_to(tokenizer_state* state, token_types type, T* target)
{
	peeked_token t = peek_token(state);
	if (t.token.type == type)
	{
		next_token(state);
		if (expect(next_token(state), token_types::colon) == return_value::error_value)
			return return_value::error_value;
		token value = next_token(state);
		if (value.type == token_types::is_int)
		{
			*target = T(value.value.as_int);
			return return_value::true_value;
		}

		return return_value::error_value;

	}
	return return_value::false_value;
}

static return_value expect_ordered_and_discard_tokens(tokenizer_state* state, std::initializer_list<token_types> expected)
{
	for (token_types t : expected)
		if (expect(next_token(state), t) == return_value::error_value)
			return return_value::error_value;

	return return_value::true_value;
}

static return_value discard_next_or_return_if_token(tokenizer_state* state, token_types expected)
{
	peeked_token p = peek_token(state);
	if (p.token.type == expected)
		return return_value::true_value;

	if (p.token.type == token_types::eof || p.token.type == token_types::none)
	{
		assert(false);
		return return_value::error_value;
	}

	next_token(state);
	return return_value::false_value;
}

static return_value return_if_token(tokenizer_state* state, token_types expected)
{
	peeked_token p = peek_token(state);
	if (p.token.type == expected)
		return return_value::true_value;

	if (p.token.type == token_types::eof || p.token.type == token_types::none)
	{
		assert(false);
		return return_value::error_value;
	}

	return return_value::false_value;
}

static return_value skip_object_and_arries_if_found(tokenizer_state* state)
{
	peeked_token p = peek_token(state);
	size_t ariety = 0;
	if (p.token.type == token_types::open_curly)
	{
		while (true)
		{
			token t = next_token(state);
			if (t.type == token_types::eof || t.type == token_types::none)
			{
				assert(false);
				return return_value::error_value;
			}
			else if (t.type == token_types::open_curly || t.type == token_types::open_bracket)
			{
				ariety++;
				continue;
			}
			else if (t.type == token_types::close_curly || t.type == token_types::closed_bracket)
			{
				ariety--;
				if (ariety == 0)
					return return_value::true_value;
				else
					continue;
			}
		}
	}

	return return_value::false_value;
}

#define BREAK_LOOP_ON_TOKRN_OR_ERROR_DISCARD_OTHERWISE(TOKEN)								\
	{																						\
		return_value r = discard_next_or_return_if_token(state, TOKEN);						\
		if (r == return_value::error_value)													\
			return { {}, return_value::error_value };										\
		else if (r == return_value::true_value)												\
			break;																			\
		skip_object_and_arries_if_found(state);												\
	}

#define BREAK_LOOP_ON_TOKRN_OR_ERROR_STATE_DISCARD_OTHERWISE(TOKEN)							\
	{																						\
		return_value r = discard_next_or_return_if_token(state, TOKEN);						\
		if (r == return_value::error_value)													\
			return return_value::error_value;												\
		else if (r == return_value::true_value)												\
			break;																			\
		skip_object_and_arries_if_found(state);												\
	}

#define BREAK_LOOP_ON_TOKRN_OR_ERROR(TOKEN)													\
	{																						\
		return_value r = return_if_token(state, TOKEN);										\
		if (r == return_value::error_value)													\
			return { {}, return_value::error_value };										\
		else if (r == return_value::true_value)												\
			break;																			\
	}

static return_value discard_next_if_token(tokenizer_state* state, token_types expected)
{
	peeked_token p = peek_token(state);
	if (p.token.type == expected)
	{
		next_token(state);
		return return_value::true_value;
	}

	if (p.token.type == token_types::eof || p.token.type == token_types::none)
	{
		assert(false);
		return return_value::error_value;
	}

	return return_value::false_value;
}

#define TRY_READ_STRING_OR_REPORT_ERROR(TOKEN, TARGET)														\
	{																										\
		return_value r = try_read_string_property_in_to(state, TOKEN, TARGET, true);						\
		if (r == return_value::error_value)																	\
			return { {}, return_value::error_value };														\
		else if (r == return_value::true_value)																\
			continue;																						\
	}

#define TRY_READ_STRING_OR_REPORT_ERROR_DONT_ALLOCATE(TOKEN, TARGET)										\
	{																										\
		return_value r = try_read_string_property_in_to(state, TOKEN, TARGET, false);						\
		if (r == return_value::error_value)																	\
			return { {}, return_value::error_value };														\
		else if (r == return_value::true_value)																\
			continue;																						\
	}

#define TRY_READ_INT_VALUE_OR_REPORT_ERROR(TOKEN, TARGET)													\
	{																										\
		return_value r = try_read_int_property_to(state, TOKEN, TARGET);									\
		if (r == return_value::error_value)																	\
			return { {}, return_value::error_value };														\
		else if (r == return_value::true_value)																\
			continue;																						\
	}

#define TRY_READ_FLOAT_VALUE_OR_REPORT_ERROR(TOKEN, TARGET)													\
	{																										\
		return_value r = try_read_real_property_to(state, TOKEN, TARGET);									\
		if (r == return_value::error_value)																	\
			return { {}, return_value::error_value };														\
		else if (r == return_value::true_value)																\
			continue;																						\
	}

#define TRY_READ_BOOL_VALUE_OR_REPORT_ERROR(TOKEN, TARGET)													\
	{																										\
		return_value r = try_read_bool_property_to(state, TOKEN, TARGET);									\
		if (r == return_value::error_value)																	\
			return { {}, return_value::error_value };														\
		else if (r == return_value::true_value)																\
			continue;																						\
	}


#define EXPECT_AND_DESCARD(...) \
	if (expect_ordered_and_discard_tokens(state, { __VA_ARGS__ }) == return_value::error_value) \
		return { {}, return_value::error_value }

#define DESCARD_IF_EXCPECTED(TOKEN) \
	if (discard_next_if_token(state, TOKEN) == return_value::error_value) \
		return { {}, return_value::error_value }

#define DESCARD_IF_EXCPECTED_STATE(TOKEN) \
	if (discard_next_if_token(state, TOKEN) == return_value::error_value) \
		return return_value::error_value

#define DESCARD_IF_EXCPECTED_RETURN_OTHERWISE(TOKEN)						\
	{																		\
		auto r = discard_next_if_token(state, TOKEN);						\
		if (r == return_value::error_value)									\
			return { {}, return_value::error_value };						\
		else if (r == return_value::false_value)							\
			return { {}, return_value::false_value };						\
	}

#define DESCARD_IF_EXCPECTED_RETURN_STATE_OTHERWISE(TOKEN)					\
	{																		\
		auto r = discard_next_if_token(state, TOKEN);						\
		if (r == return_value::error_value)									\
			return return_value::error_value;								\
		else if (r == return_value::false_value)							\
			return return_value::false_value;								\
	}

#define ELEMENT_START \
	for(size_t element_max_count = 0; element_max_count < MAX_TOKENS_PER_ENTITY; ++element_max_count)	\
	{

#define ELEMENT_END \
		if (element_max_count == MAX_TOKENS_PER_ENTITY - 1)												\
			return { {}, return_value::error_value };													\
	}

#define ELEMENT_END_STATE \
		if (element_max_count == MAX_TOKENS_PER_ENTITY - 1)												\
			return return_value::error_value;															\
	}


static void delete_asset_data(acp_vulkan::gltf_data::asset_type* in, VkAllocationCallbacks* host_allocator)
{
	free_gltf_buffer(in->generator, host_allocator);
	free_gltf_buffer(in->version, host_allocator);
}

static pair<acp_vulkan::gltf_data::asset_type, return_value> parse_asset(tokenizer_state* state)
{
	acp_vulkan::gltf_data::asset_type out{};
	on_scope_end<decltype(out)> delete_on_failure(&out, delete_asset_data, state->host_allocator);

	EXPECT_AND_DESCARD(token_types::colon, token_types::open_curly);

	ELEMENT_START
		TRY_READ_STRING_OR_REPORT_ERROR(token_types::generator, &out.generator);
		TRY_READ_STRING_OR_REPORT_ERROR(token_types::version, &out.version);

		BREAK_LOOP_ON_TOKRN_OR_ERROR_DISCARD_OTHERWISE(token_types::close_curly)
	ELEMENT_END

	EXPECT_AND_DESCARD(token_types::close_curly);

	delete_on_failure.drop();
	return { out, return_value::true_value };
}

static pair<acp_vulkan::gltf_data::buffer_view, return_value> parse_buffer_view(tokenizer_state* state)
{
	acp_vulkan::gltf_data::buffer_view out{};

	DESCARD_IF_EXCPECTED_RETURN_OTHERWISE(token_types::open_curly);

	ELEMENT_START
		TRY_READ_INT_VALUE_OR_REPORT_ERROR(token_types::buffer, &out.buffer);
		TRY_READ_INT_VALUE_OR_REPORT_ERROR(token_types::byteLength, &out.byte_length);
		TRY_READ_INT_VALUE_OR_REPORT_ERROR(token_types::byteOffset, &out.byte_offset);
		BREAK_LOOP_ON_TOKRN_OR_ERROR_DISCARD_OTHERWISE(token_types::close_curly);
	ELEMENT_END

	DESCARD_IF_EXCPECTED(token_types::close_curly);

	return { std::move(out), return_value::true_value };
}

#define GLTF_ELEMENT(CALL)																								\
	{																													\
		auto asset_data = CALL(state);																					\
		if (asset_data.second == return_value::error_value)																\
			return { {}, return_value::error_value };																	\
		else if (asset_data.second == return_value::true_value)															\
		{																												\
			out.emplace_back(std::move(asset_data.first));																\
			continue;																									\
		}																												\
		DESCARD_IF_EXCPECTED(token_types::comma);																		\
	}

static void delete_buffer_data(acp_vulkan::gltf_data::buffer* in, VkAllocationCallbacks* host_allocator)
{
	free_gltf_buffer(in->name, host_allocator);
	free_gltf_buffer(in->uri, host_allocator);
	free_gltf_buffer(in->embedded_bytes, host_allocator);
	free_gltf_buffer(in->embedded_mime, host_allocator);
}

static pair<acp_vulkan::gltf_data::buffer, return_value> parse_buffer(tokenizer_state* state)
{
	acp_vulkan::gltf_data::buffer out{};
	on_scope_end<decltype(out)> delete_on_failure(&out, delete_buffer_data, state->host_allocator);

	DESCARD_IF_EXCPECTED_RETURN_OTHERWISE(token_types::open_curly);

	ELEMENT_START
		TRY_READ_STRING_OR_REPORT_ERROR(token_types::uri, &out.uri);
		TRY_READ_STRING_OR_REPORT_ERROR(token_types::name, &out.name);
		TRY_READ_INT_VALUE_OR_REPORT_ERROR(token_types::byteLength, &out.byte_length);
		BREAK_LOOP_ON_TOKRN_OR_ERROR_DISCARD_OTHERWISE(token_types::close_curly);
	ELEMENT_END

	DESCARD_IF_EXCPECTED(token_types::close_curly);

	delete_on_failure.drop();
	return { std::move(out), return_value::true_value };
}

static void delete_image_data(acp_vulkan::gltf_data::image* in, VkAllocationCallbacks* host_allocator)
{
	free_gltf_buffer(in->uri, host_allocator);
	free_gltf_buffer(in->mime_type, host_allocator);
	free_gltf_buffer(in->embedded_bytes, host_allocator);
	free_gltf_buffer(in->embedded_mime, host_allocator);
}

static pair<acp_vulkan::gltf_data::image, return_value> parse_image(tokenizer_state* state)
{
	acp_vulkan::gltf_data::image out{};
	on_scope_end<decltype(out)> delete_on_failure(&out, delete_image_data, state->host_allocator);

	DESCARD_IF_EXCPECTED_RETURN_OTHERWISE(token_types::open_curly);

	ELEMENT_START
		TRY_READ_STRING_OR_REPORT_ERROR(token_types::uri, &out.uri);
		TRY_READ_INT_VALUE_OR_REPORT_ERROR(token_types::bufferView, &out.buffer_view);
		TRY_READ_STRING_OR_REPORT_ERROR(token_types::mimeType, &out.mime_type);
		BREAK_LOOP_ON_TOKRN_OR_ERROR_DISCARD_OTHERWISE(token_types::close_curly);
	ELEMENT_END

	DESCARD_IF_EXCPECTED(token_types::close_curly);

	delete_on_failure.drop();
	return { std::move(out), return_value::true_value };
}

static pair<temp_data_view<float>, return_value> parse_float_array(tokenizer_state* state)
{
	temp_data_view<float> out{};
	out.host_allocator = state->host_allocator;

	EXPECT_AND_DESCARD(token_types::open_bracket);

	ELEMENT_START
		peeked_token t = peek_token(state);
		if (t.token.type == token_types::is_float)
		{
			out.emplace_back(std::move(float(t.token.value.as_real)));
			next_token(state);
		}
		else if (t.token.type == token_types::is_int)
		{
			out.emplace_back(std::move(float(t.token.value.as_int)));
			next_token(state);
		}
		DESCARD_IF_EXCPECTED(token_types::comma);
		BREAK_LOOP_ON_TOKRN_OR_ERROR(token_types::closed_bracket);
	ELEMENT_END

	EXPECT_AND_DESCARD(token_types::closed_bracket);

	return { std::move(out), return_value::true_value };
}

template<size_t MAX_TARGETS, typename V, typename T>
static return_value try_read_float_array_property_to(tokenizer_state* state, token_types type, T target, V* found_values)
{
	peeked_token t = peek_token(state);
	if (t.token.type == type)
	{
		next_token(state);
		if (expect(next_token(state), token_types::colon) == return_value::error_value)
			return return_value::error_value;
		auto value = parse_float_array(state);
		if (value.second != return_value::true_value)
			return return_value::error_value;

		for (size_t ii = 0; ii < MAX_TARGETS; ++ii)
		{
			if (value.first.data_length <= ii)
				break;
			target[ii] = value.first.data[ii];
			if (found_values)
				found_values++;
		}
		return return_value::true_value;
	}
	return return_value::false_value;
}

template<size_t MAX_TARGETS, typename V, typename T>
static return_value try_read_float_array_property_to(tokenizer_state* state, token_types type, data_view<T>& target, V* found_values)
{
	peeked_token t = peek_token(state);
	if (t.token.type == type)
	{
		next_token(state);
		if (expect(next_token(state), token_types::colon) == return_value::error_value)
			return return_value::error_value;
		auto value = parse_float_array(state);
		if (value.second != return_value::true_value)
			return return_value::error_value;

		target = std::move(value.first.to());
		return return_value::true_value;
	}
	return return_value::false_value;
}

#define TRY_READ_FLOAT_ARRAY_OR_REPORT_ERROR(TOKEN, TARGET, MAX_TARGETS)											\
	{																												\
		return_value r = try_read_float_array_property_to<MAX_TARGETS, size_t>(state, TOKEN, TARGET, nullptr);		\
		if (r == return_value::error_value)																			\
			return { {}, return_value::error_value };																\
		else if (r == return_value::true_value)																		\
			continue;																								\
	}

#define TRY_READ_FLOAT_ARRAY_WITHS_SIZE_OR_REPORT_ERROR(TOKEN, TARGET, MAX_TARGETS, SET_TARGETS)			\
	{																										\
		return_value r = try_read_float_array_property_to<MAX_TARGETS>(state, TOKEN, TARGET, SET_TARGETS);	\
		if (r == return_value::error_value)																	\
			return { {}, return_value::error_value };														\
		else if (r == return_value::true_value)																\
			continue;																						\
	}

static pair<temp_data_view<uint32_t>, return_value> parse_int_array(tokenizer_state* state)
{
	temp_data_view<uint32_t> out{};
	out.host_allocator = state->host_allocator;

	EXPECT_AND_DESCARD(token_types::open_bracket);

	ELEMENT_START
		peeked_token t = peek_token(state);
		if (t.token.type == token_types::is_int)
		{
			out.emplace_back(std::move(uint32_t(t.token.value.as_int)));
			next_token(state);
		}

		DESCARD_IF_EXCPECTED(token_types::comma);
		BREAK_LOOP_ON_TOKRN_OR_ERROR(token_types::closed_bracket);
	ELEMENT_END

	EXPECT_AND_DESCARD(token_types::closed_bracket);

	return { std::move(out), return_value::true_value };
}

template<size_t MAX_TARGETS, typename V, typename T>
static return_value try_read_int_array_property_to(tokenizer_state* state, token_types type, T target, V* found_values)
{
	peeked_token t = peek_token(state);
	if (t.token.type == type)
	{
		next_token(state);
		if (expect(next_token(state), token_types::colon) == return_value::error_value)
			return return_value::error_value;
		auto value = parse_int_array(state);
		if (value.second != return_value::true_value)
			return return_value::error_value;

		for (size_t ii = 0; ii < MAX_TARGETS; ++ii)
		{
			if (value.first.data_length <= ii)
				break;
			target[ii] = value.first[ii];
			if (found_values)
				found_values++;
		}
		return return_value::true_value;
	}
	return return_value::false_value;
}

 template<size_t MAX_TARGETS, typename V, typename T>
static return_value try_read_int_array_property_to(tokenizer_state* state, token_types type, data_view<T>& target, V* found_values)
{
	peeked_token t = peek_token(state);
	if (t.token.type == type)
	{
		next_token(state);
		if (expect(next_token(state), token_types::colon) == return_value::error_value)
			return return_value::error_value;
		auto value = parse_int_array(state);
		if (value.second != return_value::true_value)
			return return_value::error_value;

		target = std::move(value.first.to());
		return return_value::true_value;
	}
	return return_value::false_value;
}

#define TRY_READ_INT_ARRAY_OR_REPORT_ERROR(TOKEN, TARGET, MAX_TARGETS)												\
	{																												\
		return_value r = try_read_int_array_property_to<MAX_TARGETS, size_t>(state, TOKEN, TARGET, nullptr);		\
		if (r == return_value::error_value)																			\
			return { {}, return_value::error_value };																\
		else if (r == return_value::true_value)																		\
			continue;																								\
	}

#define TRY_READ_INT_ARRAY_WITHS_SIZE_OR_REPORT_ERROR(TOKEN, TARGET, MAX_TARGETS, SET_TARGETS)				\
	{																										\
		return_value r = try_read_int_array_property_to<MAX_TARGETS>(state, TOKEN, TARGET, SET_TARGETS);	\
		if (r == return_value::error_value)																	\
			return { {}, return_value::error_value };														\
		else if (r == return_value::true_value)																\
			continue;																						\
	}


static pair<acp_vulkan::gltf_data::accesor::sparse_type::indices_type, return_value> parse_sparse_type_indices_type(tokenizer_state* state)
{
	acp_vulkan::gltf_data::accesor::sparse_type::indices_type out{};

	DESCARD_IF_EXCPECTED_RETURN_OTHERWISE(token_types::open_curly);

	ELEMENT_START
		TRY_READ_INT_VALUE_OR_REPORT_ERROR(token_types::bufferView, &out.buffer_view);
		TRY_READ_INT_VALUE_OR_REPORT_ERROR(token_types::byteOffset, &out.byte_offset);
		TRY_READ_INT_VALUE_OR_REPORT_ERROR(token_types::componentType, &out.component_type);
		BREAK_LOOP_ON_TOKRN_OR_ERROR_DISCARD_OTHERWISE(token_types::close_curly);
	ELEMENT_END

	DESCARD_IF_EXCPECTED(token_types::close_curly);

	return { std::move(out), return_value::true_value };
}

static pair<acp_vulkan::gltf_data::accesor::sparse_type::values_type, return_value> parse_sparse_type_values_type(tokenizer_state* state)
{
	acp_vulkan::gltf_data::accesor::sparse_type::values_type out{};

	DESCARD_IF_EXCPECTED_RETURN_OTHERWISE(token_types::open_curly);

	ELEMENT_START
		TRY_READ_INT_VALUE_OR_REPORT_ERROR(token_types::bufferView, &out.buffer_view);
		TRY_READ_INT_VALUE_OR_REPORT_ERROR(token_types::byteOffset, &out.byte_offset);
		BREAK_LOOP_ON_TOKRN_OR_ERROR_DISCARD_OTHERWISE(token_types::close_curly);
	ELEMENT_END

	DESCARD_IF_EXCPECTED(token_types::close_curly);

	return { std::move(out), return_value::true_value };
}

template<typename T, typename F>
static return_value try_read_subsection_to(tokenizer_state* state, token_types type, F subsection_parser, T target, bool* found_subsection)
{
	peeked_token t = peek_token(state);
	if (t.token.type == type)
	{
		next_token(state);
		if (expect(next_token(state), token_types::colon) == return_value::error_value)
			return return_value::error_value;
		auto v = subsection_parser(state);
		if (v.second != return_value::true_value)
		{
			if (found_subsection)
				*found_subsection = false;
			return return_value::error_value;
		}

		*target = std::move(v.first);
		if (found_subsection)
			*found_subsection = true;
		return return_value::true_value;
	}
	return return_value::false_value;
}

template<typename T, typename F>
static return_value try_read_subsection_array_to(tokenizer_state* state, token_types type, F subsection_parser, data_view<T>& target, bool* found_subsection)
{
	temp_data_view<T> out{};
	out.host_allocator = state->host_allocator;

	peeked_token t = peek_token(state);
	if (t.token.type == type)
	{
		next_token(state);
		if (expect(next_token(state), token_types::colon) == return_value::error_value)
			return return_value::error_value;

		DESCARD_IF_EXCPECTED_RETURN_STATE_OTHERWISE(token_types::open_bracket);

		ELEMENT_START
			auto v = subsection_parser(state);
			if (v.second == return_value::true_value)
			{
				out.emplace_back(std::move(v.first));
				if (found_subsection)
					*found_subsection = true;
			}
			BREAK_LOOP_ON_TOKRN_OR_ERROR_STATE_DISCARD_OTHERWISE(token_types::closed_bracket);
		ELEMENT_END_STATE

		DESCARD_IF_EXCPECTED_STATE(token_types::closed_bracket);

		target = std::move(out.to());
		return return_value::true_value;
	}

	return return_value::false_value;
}

#define TRY_READ_GLTF_SUB_SECTION_OR_REPORT_ERROR(TOKEN, TARGET, SUBSECTION_PARSER)							\
	{																										\
		return_value r = try_read_subsection_to(state, TOKEN, SUBSECTION_PARSER, TARGET, nullptr);			\
		if (r == return_value::error_value)																	\
			return { {}, return_value::error_value };														\
		else if (r == return_value::true_value)																\
			continue;																						\
	}

#define TRY_READ_GLTF_SUB_SECTION_AND_STATE_OR_REPORT_ERROR(TOKEN, TARGET, SUBSECTION_PARSER, FOUNT_SUB_SECTION)	\
	{																												\
		return_value r = try_read_subsection_to(state, TOKEN, SUBSECTION_PARSER, TARGET, FOUNT_SUB_SECTION);		\
		if (r == return_value::error_value)																			\
			return { {}, return_value::error_value };																\
		else if (r == return_value::true_value)																		\
			continue;																								\
	}

#define TRY_READ_GLTF_SUB_SECTION_ARRAY_OR_REPORT_ERROR(TOKEN, TARGET, SUBSECTION_PARSER)					\
	{																										\
		return_value r = try_read_subsection_array_to(state, TOKEN, SUBSECTION_PARSER, TARGET, nullptr);	\
		if (r == return_value::error_value)																	\
			return { {}, return_value::error_value };														\
		else if (r == return_value::true_value)																\
			continue;																						\
	}

#define TRY_READ_GLTF_SUB_SECTION_ARRAY_AND_STATE_OR_REPORT_ERROR(TOKEN, TARGET, SUBSECTION_PARSER, FOUNT_SUB_SECTION)	\
	{																													\
		return_value r = try_read_subsection_array_to(state, TOKEN, SUBSECTION_PARSER, TARGET, FOUNT_SUB_SECTION);		\
		if (r == return_value::error_value)																				\
			return { {}, return_value::error_value };																	\
		else if (r == return_value::true_value)																			\
			continue;																									\
	}

static pair<acp_vulkan::gltf_data::accesor::sparse_type, return_value> parse_sparse_type(tokenizer_state* state)
{
	acp_vulkan::gltf_data::accesor::sparse_type out{};

	DESCARD_IF_EXCPECTED_RETURN_OTHERWISE(token_types::open_curly);

	ELEMENT_START
		TRY_READ_INT_VALUE_OR_REPORT_ERROR(token_types::count, &out.count);
		TRY_READ_GLTF_SUB_SECTION_OR_REPORT_ERROR(token_types::indices, &out.indices, parse_sparse_type_indices_type);
		TRY_READ_GLTF_SUB_SECTION_OR_REPORT_ERROR(token_types::values, &out.values, parse_sparse_type_values_type);
		BREAK_LOOP_ON_TOKRN_OR_ERROR_DISCARD_OTHERWISE(token_types::close_curly);
	ELEMENT_END

		DESCARD_IF_EXCPECTED(token_types::close_curly);

	return { std::move(out), return_value::true_value };
}


static void delete_accesor_data(acp_vulkan::gltf_data::accesor* in, VkAllocationCallbacks* host_allocator)
{
	free_gltf_buffer(in->name, host_allocator);
}

static pair<acp_vulkan::gltf_data::accesor, return_value> parse_accesor(tokenizer_state* state)
{
	acp_vulkan::gltf_data::accesor out{};
	on_scope_end<decltype(out)> delete_on_failure(&out, delete_accesor_data, state->host_allocator);

	DESCARD_IF_EXCPECTED_RETURN_OTHERWISE(token_types::open_curly);

	string_view type_as_string{};

	ELEMENT_START
		TRY_READ_INT_VALUE_OR_REPORT_ERROR(token_types::bufferView, &out.buffer_view);
		TRY_READ_INT_VALUE_OR_REPORT_ERROR(token_types::byteOffset, &out.byte_offset);
		TRY_READ_INT_VALUE_OR_REPORT_ERROR(token_types::componentType, &out.component_type);
		TRY_READ_BOOL_VALUE_OR_REPORT_ERROR(token_types::normalized, &out.normalized);
		TRY_READ_INT_VALUE_OR_REPORT_ERROR(token_types::count, &out.count);
		TRY_READ_STRING_OR_REPORT_ERROR_DONT_ALLOCATE(token_types::type, &type_as_string);
		TRY_READ_STRING_OR_REPORT_ERROR(token_types::name, &out.name);
		TRY_READ_FLOAT_ARRAY_OR_REPORT_ERROR(token_types::min, out.min, sizeof(out.min) / sizeof(out.min[0]));
		TRY_READ_FLOAT_ARRAY_OR_REPORT_ERROR(token_types::max, out.max, sizeof(out.max) / sizeof(out.max[0]));
		TRY_READ_GLTF_SUB_SECTION_AND_STATE_OR_REPORT_ERROR(token_types::sparse, &out.sparse, parse_sparse_type, &out.is_sparse);
		BREAK_LOOP_ON_TOKRN_OR_ERROR_DISCARD_OTHERWISE(token_types::close_curly);
	ELEMENT_END

	DESCARD_IF_EXCPECTED(token_types::close_curly);

	if (!type_as_string.data)
		return { {}, return_value::error_value };

	const char* accesors_types[]{
		"SCALAR",
		"VEC2",
		"VEC3",
		"VEC4",
		"MAT2",
		"MAT3",
		"MAT4"
	};
	bool found_type = false;
	for (size_t ii = 0; ii < 7; ++ii)
	{
		if (strncmp(type_as_string.data, accesors_types[ii], strlen(accesors_types[ii])) == 0)
		{
			out.type = acp_vulkan::gltf_data::accesor::type_type(ii);
			found_type = true;
			break;
		}
	}

	if (!found_type)
		return { {}, return_value::error_value };

	delete_on_failure.drop();
	return { std::move(out), return_value::true_value };
}

template<typename T, typename F>
static pair<temp_data_view<T>, return_value> parse_elements(tokenizer_state* state, F parse_element)
{
	temp_data_view<T> out{};
	out.host_allocator = state->host_allocator;

	EXPECT_AND_DESCARD(token_types::colon, token_types::open_bracket);

	ELEMENT_START
		GLTF_ELEMENT(parse_element);
		BREAK_LOOP_ON_TOKRN_OR_ERROR(token_types::closed_bracket);
	ELEMENT_END

	EXPECT_AND_DESCARD(token_types::closed_bracket);

	return { std::move(out), return_value::true_value };
}

#define GLTF_SECTION(TARGET, DESTINSTION, DESTINATION_TYPE, ELEMENT_PARSER)												\
	case TARGET:																										\
	{																													\
		auto asset_data = parse_elements<DESTINATION_TYPE>(&state, ELEMENT_PARSER);										\
		if (asset_data.second == return_value::error_value)																\
		{																												\
			acp_vulkan::gltf_data_free(&out, state.host_allocator);														\
			return { .gltf_state = acp_vulkan::gltf_data::parsing_error, .parsing_error_location = state.next_char };	\
		}																												\
			out.DESTINSTION = std::move(asset_data.first.to());															\
			found_sections.emplace_back(TARGET);																		\
			break;																										\
	}

static void delete_texture_data(acp_vulkan::gltf_data::texture* in, VkAllocationCallbacks* host_allocator)
{
	free_gltf_buffer(in->name, host_allocator);
}

static pair<uint32_t, return_value> parse_texture_extensions_msft(tokenizer_state* state)
{
	uint32_t out{ UINT32_MAX };

	DESCARD_IF_EXCPECTED_RETURN_OTHERWISE(token_types::open_curly);

	ELEMENT_START
		TRY_READ_INT_VALUE_OR_REPORT_ERROR(token_types::source, &out);
		BREAK_LOOP_ON_TOKRN_OR_ERROR_DISCARD_OTHERWISE(token_types::close_curly);
	ELEMENT_END

	DESCARD_IF_EXCPECTED(token_types::close_curly);

	return { std::move(out), return_value::true_value };
}

static pair<uint32_t, return_value> parse_texture_extensions(tokenizer_state* state)
{
	uint32_t out{UINT32_MAX};

	DESCARD_IF_EXCPECTED_RETURN_OTHERWISE(token_types::open_curly);

	ELEMENT_START
		TRY_READ_GLTF_SUB_SECTION_OR_REPORT_ERROR(token_types::MSFT_texture_dds, &out, parse_texture_extensions_msft);
		BREAK_LOOP_ON_TOKRN_OR_ERROR_DISCARD_OTHERWISE(token_types::close_curly);
	ELEMENT_END

	DESCARD_IF_EXCPECTED(token_types::close_curly);

	return { std::move(out), return_value::true_value };
}

static pair<acp_vulkan::gltf_data::texture, return_value> parse_texture(tokenizer_state* state)
{
	acp_vulkan::gltf_data::texture out{};
	on_scope_end<decltype(out)> delete_on_failure(&out, delete_texture_data, state->host_allocator);

	DESCARD_IF_EXCPECTED_RETURN_OTHERWISE(token_types::open_curly);

	ELEMENT_START
		TRY_READ_STRING_OR_REPORT_ERROR(token_types::name, &out.name);
		TRY_READ_INT_VALUE_OR_REPORT_ERROR(token_types::sampler, &out.sampler);
		TRY_READ_INT_VALUE_OR_REPORT_ERROR(token_types::source, &out.source);
		TRY_READ_GLTF_SUB_SECTION_OR_REPORT_ERROR(token_types::extensions, &out.MSFT_source, parse_texture_extensions);
		BREAK_LOOP_ON_TOKRN_OR_ERROR_DISCARD_OTHERWISE(token_types::close_curly);
	ELEMENT_END

	DESCARD_IF_EXCPECTED(token_types::close_curly);

	if (out.sampler != UINT32_MAX)
		out.has_sampler = true;

	if (out.source != UINT32_MAX)
		out.has_source = true;

	if (out.MSFT_source != UINT32_MAX)
		out.has_MSFT_source = true;

	delete_on_failure.drop();
	return { std::move(out), return_value::true_value };
}

static pair<temp_data_view<pair<acp_vulkan::gltf_data::attribute, uint32_t>>, return_value> parse_attributes(tokenizer_state* state)
{
	temp_data_view<pair<acp_vulkan::gltf_data::attribute, uint32_t>> out{};
	out.host_allocator = state->host_allocator;

	EXPECT_AND_DESCARD(token_types::open_curly);

	ELEMENT_START
		peeked_token t = peek_token(state);
		if (auto iter = tokens_to_attribute.find(t.token.type); iter != tokens_to_attribute.cend())
		{
			next_token(state);
			pair<acp_vulkan::gltf_data::attribute, uint32_t> v{};
			v.first = iter->second;
			EXPECT_AND_DESCARD(token_types::colon);
			token accessor = next_token(state);
			if (accessor.type != token_types::is_int)
				return { {}, return_value::error_value };
			v.second = uint32_t(accessor.value.as_int);
			out.emplace_back(std::move(v));
		}
		DESCARD_IF_EXCPECTED(token_types::comma);
		BREAK_LOOP_ON_TOKRN_OR_ERROR(token_types::close_curly);
	ELEMENT_END

	EXPECT_AND_DESCARD(token_types::close_curly);

	return { std::move(out), return_value::true_value };
}

template<typename T>
static return_value try_read_attributes_property_to(tokenizer_state* state, token_types type, data_view<T>& target)
{
	peeked_token t = peek_token(state);
	if (t.token.type == type)
	{
		next_token(state);
		if (expect(next_token(state), token_types::colon) == return_value::error_value)
			return return_value::error_value;
		auto value = parse_attributes(state);
		if (value.second != return_value::true_value)
			return return_value::error_value;

		target = std::move(value.first.to());
		return return_value::true_value;
	}
	return return_value::false_value;
}

#define TRY_READ_ATTRIBUTE_ARRAY_OR_REPORT_ERROR(TOKEN, TARGET)														\
	{																												\
		return_value r = try_read_attributes_property_to(state, TOKEN, TARGET);										\
		if (r == return_value::error_value)																			\
			return { {}, return_value::error_value };																\
		else if (r == return_value::true_value)																		\
			continue;																								\
	}

static void delete_mesh_primitive_targets_data(acp_vulkan::gltf_data::mesh::primitive_type::target* in, VkAllocationCallbacks* host_allocator)
{
	free_gltf_buffer(in->attributes, host_allocator);
}

static pair<acp_vulkan::gltf_data::mesh::primitive_type::target, return_value> parse_mesh_primitive_targets(tokenizer_state* state)
{
	acp_vulkan::gltf_data::mesh::primitive_type::target out{};
	on_scope_end<decltype(out)> delete_on_failure(&out, delete_mesh_primitive_targets_data, state->host_allocator);

	DESCARD_IF_EXCPECTED_RETURN_OTHERWISE(token_types::open_curly);

	ELEMENT_START
		TRY_READ_ATTRIBUTE_ARRAY_OR_REPORT_ERROR(token_types::attributes, out.attributes);
		BREAK_LOOP_ON_TOKRN_OR_ERROR_DISCARD_OTHERWISE(token_types::close_curly);
	ELEMENT_END

	DESCARD_IF_EXCPECTED(token_types::close_curly);

	delete_on_failure.drop();
	return { std::move(out), return_value::true_value };
}

static void delete_mesh_primitive_data(acp_vulkan::gltf_data::mesh::primitive_type* in, VkAllocationCallbacks* host_allocator)
{
	free_gltf_buffer(in->attributes, host_allocator);
	for (size_t ii = 0; ii < in->targets.data_length; ++ii)
		delete_mesh_primitive_targets_data(&in->targets.data[ii], host_allocator);
	free_gltf_buffer(in->targets, host_allocator);
}

static pair<acp_vulkan::gltf_data::mesh::primitive_type, return_value> parse_mesh_primitive(tokenizer_state* state)
{
	acp_vulkan::gltf_data::mesh::primitive_type out{};
	on_scope_end<decltype(out)> delete_on_failure(&out, delete_mesh_primitive_data, state->host_allocator);

	DESCARD_IF_EXCPECTED_RETURN_OTHERWISE(token_types::open_curly);

	ELEMENT_START
		TRY_READ_ATTRIBUTE_ARRAY_OR_REPORT_ERROR(token_types::attributes, out.attributes);
		TRY_READ_INT_VALUE_OR_REPORT_ERROR(token_types::indices, &out.indices);
		TRY_READ_INT_VALUE_OR_REPORT_ERROR(token_types::material, &out.material);
		TRY_READ_INT_VALUE_OR_REPORT_ERROR(token_types::mode, &out.mode);
		TRY_READ_GLTF_SUB_SECTION_ARRAY_OR_REPORT_ERROR(token_types::targets, out.targets, parse_mesh_primitive_targets);
		BREAK_LOOP_ON_TOKRN_OR_ERROR_DISCARD_OTHERWISE(token_types::close_curly);
	ELEMENT_END

		DESCARD_IF_EXCPECTED(token_types::close_curly);

	delete_on_failure.drop();
	return { std::move(out), return_value::true_value };
}

static void delete_mesh_data(acp_vulkan::gltf_data::mesh* in, VkAllocationCallbacks* host_allocator)
{
	for (size_t jj = 0; jj < in->primitives.data_length; ++jj)
		delete_mesh_primitive_data(&in->primitives.data[jj], host_allocator);
	free_gltf_buffer(in->primitives, host_allocator);
	free_gltf_buffer(in->weights, host_allocator);
	free_gltf_buffer(in->name, host_allocator);
}

static pair<acp_vulkan::gltf_data::mesh, return_value> parse_mesh(tokenizer_state* state)
{
	acp_vulkan::gltf_data::mesh out{};
	on_scope_end<decltype(out)> delete_on_failure(&out, delete_mesh_data, state->host_allocator);

	DESCARD_IF_EXCPECTED_RETURN_OTHERWISE(token_types::open_curly);

	ELEMENT_START
		TRY_READ_GLTF_SUB_SECTION_ARRAY_OR_REPORT_ERROR(token_types::primitives, out.primitives, parse_mesh_primitive);
		TRY_READ_FLOAT_ARRAY_OR_REPORT_ERROR(token_types::weights, out.weights, SIZE_MAX);
		TRY_READ_STRING_OR_REPORT_ERROR(token_types::name, &out.name);
		BREAK_LOOP_ON_TOKRN_OR_ERROR_DISCARD_OTHERWISE(token_types::close_curly);
	ELEMENT_END

	DESCARD_IF_EXCPECTED(token_types::close_curly);

	delete_on_failure.drop();
	return { std::move(out), return_value::true_value };
}

static pair<acp_vulkan::gltf_data::material::texture_info, return_value> parse_material_texture_info(tokenizer_state* state)
{
	acp_vulkan::gltf_data::material::texture_info out{};

	DESCARD_IF_EXCPECTED_RETURN_OTHERWISE(token_types::open_curly);

	ELEMENT_START
		TRY_READ_INT_VALUE_OR_REPORT_ERROR(token_types::index, &out.index);
		TRY_READ_INT_VALUE_OR_REPORT_ERROR(token_types::texCoord, &out.tex_coord);
		BREAK_LOOP_ON_TOKRN_OR_ERROR_DISCARD_OTHERWISE(token_types::close_curly);
	ELEMENT_END

	DESCARD_IF_EXCPECTED(token_types::close_curly);

	return { std::move(out), return_value::true_value };
}

static pair<acp_vulkan::gltf_data::material::normal_texture_info, return_value> parse_material_normal_texture_info(tokenizer_state* state)
{
	acp_vulkan::gltf_data::material::normal_texture_info out{};

	DESCARD_IF_EXCPECTED_RETURN_OTHERWISE(token_types::open_curly);

	ELEMENT_START
		TRY_READ_INT_VALUE_OR_REPORT_ERROR(token_types::index, &out.index);
		TRY_READ_INT_VALUE_OR_REPORT_ERROR(token_types::texCoord, &out.tex_coord);
		TRY_READ_FLOAT_VALUE_OR_REPORT_ERROR(token_types::scale, &out.scale);
		BREAK_LOOP_ON_TOKRN_OR_ERROR_DISCARD_OTHERWISE(token_types::close_curly);
	ELEMENT_END

	DESCARD_IF_EXCPECTED(token_types::close_curly);

	return { std::move(out), return_value::true_value };
}

static pair<acp_vulkan::gltf_data::material::occlusion_texture_info, return_value> parse_material_occlusion_texture_info(tokenizer_state* state)
{
	acp_vulkan::gltf_data::material::occlusion_texture_info out{};

	DESCARD_IF_EXCPECTED_RETURN_OTHERWISE(token_types::open_curly);

	ELEMENT_START
		TRY_READ_INT_VALUE_OR_REPORT_ERROR(token_types::index, &out.index);
		TRY_READ_INT_VALUE_OR_REPORT_ERROR(token_types::texCoord, &out.tex_coord);
		TRY_READ_FLOAT_VALUE_OR_REPORT_ERROR(token_types::strength, &out.strength);
		BREAK_LOOP_ON_TOKRN_OR_ERROR_DISCARD_OTHERWISE(token_types::close_curly);
	ELEMENT_END

		DESCARD_IF_EXCPECTED(token_types::close_curly);

	return { std::move(out), return_value::true_value };
}

static pair<acp_vulkan::gltf_data::material::pbr_metallic_roughness_type, return_value> parse_material_pbr_metallic_roughness(tokenizer_state* state)
{
	acp_vulkan::gltf_data::material::pbr_metallic_roughness_type out{};

	DESCARD_IF_EXCPECTED_RETURN_OTHERWISE(token_types::open_curly);

	ELEMENT_START
		TRY_READ_FLOAT_ARRAY_OR_REPORT_ERROR(token_types::baseColorFactor, out.base_color_factor, 4);
		TRY_READ_GLTF_SUB_SECTION_AND_STATE_OR_REPORT_ERROR(token_types::baseColorTexture, &out.base_color_texture, parse_material_texture_info, &out.has_base_color_texture);
		TRY_READ_FLOAT_VALUE_OR_REPORT_ERROR(token_types::metallicFactor, &out.metallic_factor);
		TRY_READ_FLOAT_VALUE_OR_REPORT_ERROR(token_types::roughnessFactor, &out.roughness_factor);
		TRY_READ_GLTF_SUB_SECTION_AND_STATE_OR_REPORT_ERROR(token_types::metallicRoughnessTexture, &out.metallic_roughness_texture, parse_material_texture_info, &out.has_metallic_roughness_texture);
		BREAK_LOOP_ON_TOKRN_OR_ERROR_DISCARD_OTHERWISE(token_types::close_curly);
	ELEMENT_END

	DESCARD_IF_EXCPECTED(token_types::close_curly);

	return { std::move(out), return_value::true_value };
}

template<typename T>
static return_value try_read_alpha_mode_property_to(tokenizer_state* state, token_types type, T* target)
{
	peeked_token t = peek_token(state);
	if (t.token.type == type)
	{
		next_token(state);
		if (expect(next_token(state), token_types::colon) == return_value::error_value)
			return return_value::error_value;
		token value = next_token(state);
		if (auto iter = tokens_to_alpha_mode.find(value.type); iter != tokens_to_alpha_mode.end())
		{
			*target = T(iter->second);
			return return_value::true_value;
		}

		return return_value::error_value;
	}
	return return_value::false_value;
}

#define TRY_READ_ALPHA_MODE_VALUE_OR_REPORT_ERROR(TOKEN, TARGET)											\
	{																										\
		return_value r = try_read_alpha_mode_property_to(state, TOKEN, TARGET);								\
		if (r == return_value::error_value)																	\
			return { {}, return_value::error_value };														\
		else if (r == return_value::true_value)																\
			continue;																						\
	}

static void delete_material_data(acp_vulkan::gltf_data::material* in, VkAllocationCallbacks* host_allocator)
{
	free_gltf_buffer(in->name, host_allocator);
}

static pair<acp_vulkan::gltf_data::material, return_value> parse_material(tokenizer_state* state)
{
	acp_vulkan::gltf_data::material out{};
	on_scope_end<decltype(out)> delete_on_failure(&out, delete_material_data, state->host_allocator);

	DESCARD_IF_EXCPECTED_RETURN_OTHERWISE(token_types::open_curly);

	ELEMENT_START
		TRY_READ_GLTF_SUB_SECTION_AND_STATE_OR_REPORT_ERROR(token_types::pbrMetallicRoughness, &out.pbr_metallic_roughness, parse_material_pbr_metallic_roughness, &out.has_pbr_metallic_roughness);
		TRY_READ_GLTF_SUB_SECTION_AND_STATE_OR_REPORT_ERROR(token_types::normalTexture, &out.normal_texture, parse_material_normal_texture_info, &out.has_normal_texture);
		TRY_READ_GLTF_SUB_SECTION_AND_STATE_OR_REPORT_ERROR(token_types::occlusionTexture, &out.occlusion_texture, parse_material_occlusion_texture_info, &out.has_occlusion_texture);
		TRY_READ_GLTF_SUB_SECTION_AND_STATE_OR_REPORT_ERROR(token_types::emissiveTexture, &out.emissive_texture, parse_material_texture_info, &out.has_emissive_texture);
		TRY_READ_FLOAT_ARRAY_OR_REPORT_ERROR(token_types::emissiveFactor, out.emissive_factor, 3);
		TRY_READ_ALPHA_MODE_VALUE_OR_REPORT_ERROR(token_types::alphaMode, &out.alpha_mode);
		TRY_READ_FLOAT_VALUE_OR_REPORT_ERROR(token_types::alphaCutoff, &out.alpha_cutoff);
		TRY_READ_BOOL_VALUE_OR_REPORT_ERROR(token_types::doubleSided, &out.double_sided);
		TRY_READ_STRING_OR_REPORT_ERROR(token_types::name, &out.name);
		BREAK_LOOP_ON_TOKRN_OR_ERROR_DISCARD_OTHERWISE(token_types::close_curly);
	ELEMENT_END

	DESCARD_IF_EXCPECTED(token_types::close_curly);

	delete_on_failure.drop();
	return { std::move(out), return_value::true_value };
}

static void delete_node_data(acp_vulkan::gltf_data::node* in, VkAllocationCallbacks* host_allocator)
{
	free_gltf_buffer(in->children, host_allocator);
	free_gltf_buffer(in->name, host_allocator);
	free_gltf_buffer(in->weights, host_allocator);
}

static pair<acp_vulkan::gltf_data::node, return_value> parse_node(tokenizer_state* state)
{
	acp_vulkan::gltf_data::node out{};
	on_scope_end<decltype(out)> delete_on_failure(&out, delete_node_data, state->host_allocator);

	DESCARD_IF_EXCPECTED_RETURN_OTHERWISE(token_types::open_curly);

	ELEMENT_START
		TRY_READ_STRING_OR_REPORT_ERROR(token_types::name, &out.name);
		TRY_READ_INT_VALUE_OR_REPORT_ERROR(token_types::skin, &out.skin);
		TRY_READ_FLOAT_ARRAY_OR_REPORT_ERROR(token_types::matrix, out.matrix, 16);
		TRY_READ_INT_VALUE_OR_REPORT_ERROR(token_types::mesh, &out.mesh);
		TRY_READ_FLOAT_ARRAY_OR_REPORT_ERROR(token_types::rotation, out.rotation, 4);
		TRY_READ_FLOAT_ARRAY_OR_REPORT_ERROR(token_types::scale, out.scale, 3);
		TRY_READ_FLOAT_ARRAY_OR_REPORT_ERROR(token_types::translation, out.translation, 3);
		TRY_READ_FLOAT_ARRAY_OR_REPORT_ERROR(token_types::weights, out.weights, SIZE_MAX);
		BREAK_LOOP_ON_TOKRN_OR_ERROR_DISCARD_OTHERWISE(token_types::close_curly);
	ELEMENT_END

	DESCARD_IF_EXCPECTED(token_types::close_curly);

	if (out.camera != UINT32_MAX)
		out.has_camera = true;

	if (out.mesh != UINT32_MAX)
		out.has_mesh = true;

	if (out.skin != UINT32_MAX)
		out.has_skin = true;

	float default_matrix[16]{ 1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f , 0.0f, 0.0f, 0.0f, 0.0f, 1.0f };
	if (memcmp(out.matrix, default_matrix, sizeof(float) * 16) != 0)
		out.has_matrix = true;

	float default_rotation[4]{ 0.0f, 0.0f, 0.0f, 1.0f };
	if (memcmp(out.rotation, default_rotation, sizeof(float) * 4) != 0)
		out.has_rotation = true;

	float default_scale[3]{ 1.0f, 1.0f, 1.0f };
	if (memcmp(out.scale, default_scale, sizeof(float) * 3) != 0)
		out.has_scale = true;

	float default_translation[3]{ 0.0f, 0.0f, 0.0f };
	if (memcmp(out.translation, default_translation, sizeof(float) * 3) != 0)
		out.has_translation = true;

	delete_on_failure.drop();
	return { std::move(out), return_value::true_value };
}

static void delete_scene_data(acp_vulkan::gltf_data::scene* in, VkAllocationCallbacks* host_allocator)
{
	free_gltf_buffer(in->nodes, host_allocator);
	free_gltf_buffer(in->name, host_allocator);
}

static pair<acp_vulkan::gltf_data::scene, return_value> parse_scene(tokenizer_state* state)
{
	acp_vulkan::gltf_data::scene out{};
	on_scope_end<decltype(out)> delete_on_failure(&out, delete_scene_data, state->host_allocator);

	DESCARD_IF_EXCPECTED_RETURN_OTHERWISE(token_types::open_curly);

	ELEMENT_START
		TRY_READ_INT_ARRAY_OR_REPORT_ERROR(token_types::nodes, out.nodes, SIZE_MAX);
		TRY_READ_STRING_OR_REPORT_ERROR(token_types::name, &out.name);
		BREAK_LOOP_ON_TOKRN_OR_ERROR_DISCARD_OTHERWISE(token_types::close_curly);
	ELEMENT_END

	DESCARD_IF_EXCPECTED(token_types::close_curly);

	delete_on_failure.drop();
	return { std::move(out), return_value::true_value };
}

struct pars_sampler
{
	uint32_t magFilter{ UINT32_MAX };
	uint32_t minFilter{ UINT32_MAX };
	uint32_t wrapS{ 10497 };
	uint32_t wrapT{ 10497 };
	acp_vulkan::gltf_data::string_view name;
};

static void delete_sampler_data(acp_vulkan::gltf_data::sampler* in, VkAllocationCallbacks* host_allocator)
{
	free_gltf_buffer(in->name, host_allocator);
}

static void delete_sampler_data(pars_sampler* in, VkAllocationCallbacks* host_allocator)
{
	free_gltf_buffer(in->name, host_allocator);
}

static pair<acp_vulkan::gltf_data::sampler, return_value> parse_sampler(tokenizer_state* state)
{
	pars_sampler out{};
	on_scope_end<decltype(out)> delete_on_failure(&out, delete_sampler_data, state->host_allocator);

	DESCARD_IF_EXCPECTED_RETURN_OTHERWISE(token_types::open_curly);

	ELEMENT_START
		TRY_READ_STRING_OR_REPORT_ERROR(token_types::name, &out.name);
		TRY_READ_INT_VALUE_OR_REPORT_ERROR(token_types::magFilter, &out.magFilter);
		TRY_READ_INT_VALUE_OR_REPORT_ERROR(token_types::minFilter, &out.minFilter);
		TRY_READ_INT_VALUE_OR_REPORT_ERROR(token_types::wrapS, &out.wrapS);
		TRY_READ_INT_VALUE_OR_REPORT_ERROR(token_types::wrapT, &out.wrapT);
		BREAK_LOOP_ON_TOKRN_OR_ERROR_DISCARD_OTHERWISE(token_types::close_curly);
	ELEMENT_END

	DESCARD_IF_EXCPECTED(token_types::close_curly);

	delete_on_failure.drop();
	acp_vulkan::gltf_data::sampler final_out{
		.mag_filter = acp_vulkan::gltf_data::sampler::filter_type(out.magFilter),
		.min_filter = acp_vulkan::gltf_data::sampler::filter_type(out.minFilter),
		.wrap_s = acp_vulkan::gltf_data::sampler::wrap_type(out.wrapS),
		.wrap_t = acp_vulkan::gltf_data::sampler::wrap_type(out.wrapT),
		.name = out.name
	};

	return { std::move(final_out), return_value::true_value };
}

static void delete_skin_data(acp_vulkan::gltf_data::skin* in, VkAllocationCallbacks* host_allocator)
{
	free_gltf_buffer(in->name, host_allocator);
	free_gltf_buffer(in->joints, host_allocator);
}

static pair<acp_vulkan::gltf_data::skin, return_value> parse_skin(tokenizer_state* state)
{
	acp_vulkan::gltf_data::skin out{};
	on_scope_end<decltype(out)> delete_on_failure(&out, delete_skin_data, state->host_allocator);

	DESCARD_IF_EXCPECTED_RETURN_OTHERWISE(token_types::open_curly);

	ELEMENT_START
		TRY_READ_INT_VALUE_OR_REPORT_ERROR(token_types::inverseBindMatrices, &out.inverse_bind_matrices);
		TRY_READ_INT_VALUE_OR_REPORT_ERROR(token_types::skeleton, &out.skeleton);
		TRY_READ_INT_ARRAY_OR_REPORT_ERROR(token_types::joints, out.joints, SIZE_MAX);
		TRY_READ_STRING_OR_REPORT_ERROR(token_types::name, &out.name);
		BREAK_LOOP_ON_TOKRN_OR_ERROR_DISCARD_OTHERWISE(token_types::close_curly);
	ELEMENT_END

	DESCARD_IF_EXCPECTED(token_types::close_curly);

	if (out.inverse_bind_matrices != UINT32_MAX)
		out.has_inverse_bind_matrices = true;

	if (out.skeleton != UINT32_MAX)
		out.has_skeleton = true;

	delete_on_failure.drop();
	return { std::move(out), return_value::true_value };
}

static pair<acp_vulkan::gltf_data::camera::orthographic_properties_type, return_value> parse_camera_orthographic(tokenizer_state* state)
{
	acp_vulkan::gltf_data::camera::orthographic_properties_type out{};

	DESCARD_IF_EXCPECTED_RETURN_OTHERWISE(token_types::open_curly);

	ELEMENT_START
		TRY_READ_FLOAT_VALUE_OR_REPORT_ERROR(token_types::xmag, &out.x_mag);
		TRY_READ_FLOAT_VALUE_OR_REPORT_ERROR(token_types::ymag, &out.y_mag);
		TRY_READ_FLOAT_VALUE_OR_REPORT_ERROR(token_types::zfar, &out.z_far);
		TRY_READ_FLOAT_VALUE_OR_REPORT_ERROR(token_types::znear, &out.z_near);
		BREAK_LOOP_ON_TOKRN_OR_ERROR_DISCARD_OTHERWISE(token_types::close_curly);
	ELEMENT_END

	DESCARD_IF_EXCPECTED(token_types::close_curly);

	return { std::move(out), return_value::true_value };
}

static pair<acp_vulkan::gltf_data::camera::perspective_properties_type, return_value> parse_camera_perspective(tokenizer_state* state)
{
	acp_vulkan::gltf_data::camera::perspective_properties_type out{};

	DESCARD_IF_EXCPECTED_RETURN_OTHERWISE(token_types::open_curly);

	ELEMENT_START
		TRY_READ_FLOAT_VALUE_OR_REPORT_ERROR(token_types::aspectRatio, &out.aspect_ratio);
		TRY_READ_FLOAT_VALUE_OR_REPORT_ERROR(token_types::yfov, &out.y_fov);
		TRY_READ_FLOAT_VALUE_OR_REPORT_ERROR(token_types::zfar, &out.z_far);
		TRY_READ_FLOAT_VALUE_OR_REPORT_ERROR(token_types::znear, &out.z_near);
		BREAK_LOOP_ON_TOKRN_OR_ERROR_DISCARD_OTHERWISE(token_types::close_curly);
	ELEMENT_END

	DESCARD_IF_EXCPECTED(token_types::close_curly);

	return { std::move(out), return_value::true_value };
}

static void delete_camera_data(acp_vulkan::gltf_data::camera* in, VkAllocationCallbacks* host_allocator)
{
	free_gltf_buffer(in->name, host_allocator);
}

static pair<acp_vulkan::gltf_data::camera, return_value> parse_camera(tokenizer_state* state)
{
	acp_vulkan::gltf_data::camera out{};
	on_scope_end<decltype(out)> delete_on_failure(&out, delete_camera_data, state->host_allocator);

	DESCARD_IF_EXCPECTED_RETURN_OTHERWISE(token_types::open_curly);

	bool has_perspective_data = false;
	bool has_orthographic_data = false;
	string_view type_as_string;
	ELEMENT_START
		TRY_READ_STRING_OR_REPORT_ERROR(token_types::name, &out.name);
		TRY_READ_GLTF_SUB_SECTION_AND_STATE_OR_REPORT_ERROR(token_types::perspective, &out.perspective, parse_camera_perspective, &has_perspective_data);
		TRY_READ_GLTF_SUB_SECTION_AND_STATE_OR_REPORT_ERROR(token_types::orthographic, &out.orthographic, parse_camera_orthographic, &has_orthographic_data);
		TRY_READ_STRING_OR_REPORT_ERROR_DONT_ALLOCATE(token_types::type, &type_as_string);
		BREAK_LOOP_ON_TOKRN_OR_ERROR_DISCARD_OTHERWISE(token_types::close_curly);
	ELEMENT_END

	DESCARD_IF_EXCPECTED(token_types::close_curly);

	size_t perspective_len = strlen(token_as_strings[size_t(token_types::perspective)]);
	if ((perspective_len == type_as_string.data_length) && (strncmp(token_as_strings[size_t(token_types::perspective)], type_as_string.data, perspective_len) == 0))
	{
		out.type = acp_vulkan::gltf_data::camera::camera_type::perspective;
		if (has_orthographic_data)
			return { {}, return_value::false_value };
	}
	else
	{
		size_t orthographic_len = strlen(token_as_strings[size_t(token_types::orthographic)]);
		if ((orthographic_len == type_as_string.data_length) && (strncmp(token_as_strings[size_t(token_types::orthographic)], type_as_string.data, orthographic_len) == 0))
		{
			out.type = acp_vulkan::gltf_data::camera::camera_type::orthographic;
			if (has_perspective_data)
				return { {}, return_value::false_value };
		}
	}

	delete_on_failure.drop();
	return { std::move(out), return_value::true_value };
}

static pair<acp_vulkan::gltf_data::animation::channel::target_type, return_value> parse_animation_channel_target(tokenizer_state* state)
{
	acp_vulkan::gltf_data::animation::channel::target_type out{};

	DESCARD_IF_EXCPECTED_RETURN_OTHERWISE(token_types::open_curly);

	acp_vulkan::gltf_data::string_view target_as_string;

	ELEMENT_START
		TRY_READ_INT_VALUE_OR_REPORT_ERROR(token_types::node, &out.node);
		TRY_READ_STRING_OR_REPORT_ERROR_DONT_ALLOCATE(token_types::path, &target_as_string);
		BREAK_LOOP_ON_TOKRN_OR_ERROR_DISCARD_OTHERWISE(token_types::close_curly);
	ELEMENT_END

	DESCARD_IF_EXCPECTED(token_types::close_curly);

	if (!target_as_string.data)
		return { {}, return_value::error_value };
	
	const char* path_types_as_strings[] {
		"translation",
		"rotation",
		"scale",
		"weights"
	};
	bool found_path = false;
	for (size_t ii = 0; ii < 4; ++ii)
	{
		if (strncmp(target_as_string.data, path_types_as_strings[ii], strlen(path_types_as_strings[ii])) == 0)
		{
			out.path = acp_vulkan::gltf_data::animation::channel::target_type::path_type(ii);
			found_path = true;
			break;
		}
	}

	if(!found_path)
		return { {}, return_value::error_value };

	return { std::move(out), return_value::true_value };
}

static pair<acp_vulkan::gltf_data::animation::channel, return_value> parse_animation_channel(tokenizer_state* state)
{
	acp_vulkan::gltf_data::animation::channel out{};

	DESCARD_IF_EXCPECTED_RETURN_OTHERWISE(token_types::open_curly);

	ELEMENT_START
		TRY_READ_INT_VALUE_OR_REPORT_ERROR(token_types::sampler, &out.sampler);
		TRY_READ_GLTF_SUB_SECTION_OR_REPORT_ERROR(token_types::target, &out.target, parse_animation_channel_target);
		BREAK_LOOP_ON_TOKRN_OR_ERROR_DISCARD_OTHERWISE(token_types::close_curly);
	ELEMENT_END

	DESCARD_IF_EXCPECTED(token_types::close_curly);

	return { std::move(out), return_value::true_value };
}

static pair<acp_vulkan::gltf_data::animation::sampler, return_value> parse_animation_sampler(tokenizer_state* state)
{
	acp_vulkan::gltf_data::animation::sampler out{};

	DESCARD_IF_EXCPECTED_RETURN_OTHERWISE(token_types::open_curly);

	acp_vulkan::gltf_data::string_view interpolation_as_string{};

	ELEMENT_START
		TRY_READ_INT_VALUE_OR_REPORT_ERROR(token_types::input, &out.input);
		TRY_READ_STRING_OR_REPORT_ERROR_DONT_ALLOCATE(token_types::interpolation, &interpolation_as_string);
		TRY_READ_INT_VALUE_OR_REPORT_ERROR(token_types::output, &out.output);
		BREAK_LOOP_ON_TOKRN_OR_ERROR_DISCARD_OTHERWISE(token_types::close_curly);
	ELEMENT_END

	if (interpolation_as_string.data)
	{
		const char* interpolation_types_as_strings[]{
			"LINEAR",
			"STEP",
			"CUBICSPLINE",
		};
		for (size_t ii = 0; ii < 3; ++ii)
		{
			if (strncmp(interpolation_as_string.data, interpolation_types_as_strings[ii], strlen(interpolation_types_as_strings[ii])) == 0)
			{
				out.interpolation = acp_vulkan::gltf_data::animation::sampler::interpolation_type(ii);
				break;
			}
		}
	}

	DESCARD_IF_EXCPECTED(token_types::close_curly);

	return { std::move(out), return_value::true_value };
}

static void delete_animation_data(acp_vulkan::gltf_data::animation* in, VkAllocationCallbacks* host_allocator)
{
	free_gltf_buffer(in->name, host_allocator);
}

static pair<acp_vulkan::gltf_data::animation, return_value> parse_animation(tokenizer_state* state)
{
	acp_vulkan::gltf_data::animation out{};
	on_scope_end<decltype(out)> delete_on_failure(&out, delete_animation_data, state->host_allocator);

	DESCARD_IF_EXCPECTED_RETURN_OTHERWISE(token_types::open_curly);

	ELEMENT_START
		TRY_READ_GLTF_SUB_SECTION_ARRAY_OR_REPORT_ERROR(token_types::channels, out.channels, parse_animation_channel);
		TRY_READ_GLTF_SUB_SECTION_ARRAY_OR_REPORT_ERROR(token_types::samplers, out.samplers, parse_animation_sampler);
		TRY_READ_STRING_OR_REPORT_ERROR(token_types::name, &out.name);
		BREAK_LOOP_ON_TOKRN_OR_ERROR_DISCARD_OTHERWISE(token_types::close_curly);
	ELEMENT_END

	DESCARD_IF_EXCPECTED(token_types::close_curly);

	delete_on_failure.drop();
	return { std::move(out), return_value::true_value };
}

static constexpr const char  table[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
static constexpr const int   BASE64_INPUT_SIZE = 57;

bool is_base_64(char c)
{
	return c && strchr(table, c) != NULL;
}

inline char base64_value(char c)
{
	const char* p = strchr(table, c);
	if (p) {
		return p - table;
	}
	else {
		return 0;
	}
}

static constexpr const unsigned char decoding_table[256] = {
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x3e, 0x00, 0x00, 0x00, 0x3f,
	0x34, 0x35, 0x36, 0x37, 0x38, 0x39, 0x3a, 0x3b, 0x3c, 0x3d, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0a, 0x0b, 0x0c, 0x0d, 0x0e,
	0x0f, 0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17, 0x18, 0x19, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x1a, 0x1b, 0x1c, 0x1d, 0x1e, 0x1f, 0x20, 0x21, 0x22, 0x23, 0x24, 0x25, 0x26, 0x27, 0x28,
	0x29, 0x2a, 0x2b, 0x2c, 0x2d, 0x2e, 0x2f, 0x30, 0x31, 0x32, 0x33, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 };

data_view<uint8_t> base64_decode(string_view input)
{
	if (input.data_length % 4 != 0)
		return {};

	size_t output_length = input.data_length / 4 * 3 + 2;

	if (input.data[input.data_length - 1] == '=') (output_length)--;
	if (input.data[input.data_length - 2] == '=') (output_length)--;

	temp_data_view<uint8_t> decoded_data;
	decoded_data.reserve(output_length);

	if (!decoded_data.data)
		return {};

	for (int i = 0, j = 0; i < input.data_length;)
	{

		uint32_t sextet_a = input.data[i] == '=' ? 0 & i++ : decoding_table[input.data[i++]];
		uint32_t sextet_b = input.data[i] == '=' ? 0 & i++ : decoding_table[input.data[i++]];
		uint32_t sextet_c = input.data[i] == '=' ? 0 & i++ : decoding_table[input.data[i++]];
		uint32_t sextet_d = input.data[i] == '=' ? 0 & i++ : decoding_table[input.data[i++]];

		uint32_t triple = (sextet_a << 3 * 6)
			+ (sextet_b << 2 * 6)
			+ (sextet_c << 1 * 6)
			+ (sextet_d << 0 * 6);

		if (j < output_length) decoded_data.emplace_back((triple >> 2 * 8) & 0xFF);
		if (j < output_length) decoded_data.emplace_back((triple >> 1 * 8) & 0xFF);
		if (j < output_length) decoded_data.emplace_back((triple >> 0 * 8) & 0xFF);

	}

	return decoded_data.to();
};

static std::pair<string_view, string_view> get_embedded_data(string_view uri)
{
	static constexpr const char base_64_data_magic_start[] = "data:";
	static constexpr size_t base_64_data_magic_len_start = 5;

	static constexpr const char base_64_data_magic_end[] = ";base64,";
	static constexpr size_t base_64_data_magic_len_end = 7;

	if (!uri.data)
		return {};

	if (uri.data_length < base_64_data_magic_len_start)
		return {};

	if (strncmp(uri.data, base_64_data_magic_start, base_64_data_magic_len_start) == 0)
	{
		const char* data = uri.data + base_64_data_magic_len_start;
		size_t data_length = uri.data_length - base_64_data_magic_len_start;
		for (size_t jj = 0; jj < data_length; ++jj)
		{
			if (data[jj] != ';')
				continue;

			if ((data_length - jj) > base_64_data_magic_len_end)
			{
				if (strncmp(data + jj, base_64_data_magic_end, base_64_data_magic_len_end) == 0)
				{
					const char* base_64_encoded_data_start = data + jj + base_64_data_magic_len_end;
					string_view base_64_encoded_data{ .data = data + jj + base_64_data_magic_len_end + 1, .data_length = data_length - jj - base_64_data_magic_len_end - 1 };
					string_view mime{ .data = data , .data_length = uri.data_length - base_64_data_magic_len_start - base_64_encoded_data.data_length - base_64_data_magic_len_end - 1 };
					return { mime, base_64_encoded_data };
				}
			}
		}
	}

	return {};
}

acp_vulkan::gltf_data acp_vulkan::gltf_data_from_memory(const char* data, size_t data_size, VkAllocationCallbacks* host_allocator)
{
	acp_vulkan::gltf_data out{};


	tokenizer_state state{
		.data = data,
		.data_size = data_size,
		.host_allocator = host_allocator,
	};

	if (expect(next_token(&state), token_types::open_curly) == return_value::error_value)
		return {.gltf_state = acp_vulkan::gltf_data::parsing_error, .parsing_error_location = state.next_char};

	temp_data_view<token_types> found_sections{};
	found_sections.host_allocator = host_allocator;
	pair<token_types, acp_vulkan::gltf_data::gltf_state_type> mandatory_sections[] = {
		{token_types::asset, acp_vulkan::gltf_data::missing_asset_section }
	};
	while (true)
	{
		token t = next_token(&state);
		if (t.type == token_types::none)
		{
			return { .gltf_state = acp_vulkan::gltf_data::parsing_error, .parsing_error_location = state.next_char };
			assert(false);
		}

		switch (t.type)
		{
			GLTF_SECTION(token_types::bufferViews, buffer_views, acp_vulkan::gltf_data::buffer_view, parse_buffer_view);
			GLTF_SECTION(token_types::buffers, buffers, acp_vulkan::gltf_data::buffer, parse_buffer);
			GLTF_SECTION(token_types::images, images, acp_vulkan::gltf_data::image, parse_image);
			GLTF_SECTION(token_types::accessors, accesors, acp_vulkan::gltf_data::accesor, parse_accesor);
			GLTF_SECTION(token_types::textures, textures, acp_vulkan::gltf_data::texture, parse_texture);
			GLTF_SECTION(token_types::meshes, meshes, acp_vulkan::gltf_data::mesh, parse_mesh);
			GLTF_SECTION(token_types::materials, materials, acp_vulkan::gltf_data::material, parse_material);
			GLTF_SECTION(token_types::nodes, nodes, acp_vulkan::gltf_data::node, parse_node);
			GLTF_SECTION(token_types::scenes, scenes, acp_vulkan::gltf_data::scene, parse_scene);
			GLTF_SECTION(token_types::samplers, samplers, acp_vulkan::gltf_data::sampler, parse_sampler);
			GLTF_SECTION(token_types::skins, skins, acp_vulkan::gltf_data::skin, parse_skin);
			GLTF_SECTION(token_types::cameras, cameras, acp_vulkan::gltf_data::camera, parse_camera);
			GLTF_SECTION(token_types::animations, animations, acp_vulkan::gltf_data::animation, parse_animation);		
			case token_types::asset:
			{
				auto asset = parse_asset(&state);
				if(asset.second != return_value::true_value)
					return { .gltf_state = acp_vulkan::gltf_data::parsing_error, .parsing_error_location = state.next_char };
				out.asset = asset.first;
				found_sections.emplace_back(token_types::asset);
				break;
			}
			case token_types::scene:
			{
				if (expect_ordered_and_discard_tokens(&state, { token_types::colon }) == return_value::error_value)
					return { .gltf_state = acp_vulkan::gltf_data::parsing_error, .parsing_error_location = state.next_char };

				token default_scene = next_token(&state);
				if(default_scene.type != token_types::is_int)
					return { .gltf_state = acp_vulkan::gltf_data::parsing_error, .parsing_error_location = state.next_char };
				out.default_scene = uint32_t(default_scene.value.as_int);
				out.has_defautl_scene = true;
				found_sections.emplace_back(token_types::scene);
				break;
			}
			default:
				skip_object_and_arries_if_found(&state);
		}

		if (t.type == token_types::eof)
			break;
	}

	for (auto ii : mandatory_sections)
	{
		bool found_section = false;
		for (size_t jj = 0; jj < found_sections.data_length; ++jj)
		{
			if (found_sections.data[jj] == ii.first)
			{
				found_section = true;
				break;
			}
		}
		if (!found_section)
		{
			return { .gltf_state = ii.second, .parsing_error_location = 0 };
			assert(false);
		}
	}

	for (size_t ii = 0; ii < out.buffers.data_length; ++ii)
	{
		auto embaded_data = get_embedded_data(out.buffers.data[ii].uri);
		if (embaded_data.first.data && embaded_data.second.data)
		{
			out.buffers.data[ii].embedded_bytes = base64_decode(embaded_data.second);
			if (out.buffers.data[ii].embedded_bytes.data && out.buffers.data[ii].embedded_bytes.data_length != 0)
				out.buffers.data[ii].embedded_mime = copy(embaded_data.first, host_allocator);
		}
	}

	for (size_t ii = 0; ii < out.images.data_length; ++ii)
	{
		auto embaded_data = get_embedded_data(out.images.data[ii].uri);
		if (embaded_data.first.data && embaded_data.second.data)
		{
			out.images.data[ii].embedded_bytes = base64_decode(embaded_data.second);
			if (out.images.data[ii].embedded_bytes.data && out.images.data[ii].embedded_bytes.data_length != 0)
				out.images.data[ii].embedded_mime = copy(embaded_data.first, host_allocator);
		}
	}
	out.gltf_state = acp_vulkan::gltf_data::gltf_state_type::valid;

	return out;
}

acp_vulkan::gltf_data acp_vulkan::gltf_data_from_file(const char* path, VkAllocationCallbacks* host_allocator)
{
	FILE* gltf_bytes = fopen(path, "rb");
	if (!gltf_bytes)
		return {};

	fseek(gltf_bytes, 0, SEEK_END);
	long gltf_size = ftell(gltf_bytes);
	fseek(gltf_bytes, 0, SEEK_SET);

	char* gltf_data = host_allocator ?
		reinterpret_cast<char*>(host_allocator->pfnAllocation(host_allocator->pUserData, gltf_size, 1, VK_SYSTEM_ALLOCATION_SCOPE_OBJECT))
		: new char[gltf_size];

	if (!gltf_data)
	{
		fclose(gltf_bytes);
		return {.gltf_state = acp_vulkan::gltf_data::gltf_state_type::unable_to_read_file, .parsing_error_location = 0 };
	}

	size_t bytes_to_read = gltf_size;
	size_t offset = 0;
	while (size_t bytes_read = fread(gltf_data + offset, 1, bytes_to_read, gltf_bytes))
	{
		offset += bytes_read;
		bytes_to_read -= bytes_read;
	}

	fclose(gltf_bytes);

	if (gltf_size < offset)
	{
		if (host_allocator)
			host_allocator->pfnFree(host_allocator->pUserData, gltf_data);
		else
			delete[] gltf_data;

		return {};
	}

	acp_vulkan::gltf_data out = acp_vulkan::gltf_data_from_memory(gltf_data, gltf_size, host_allocator);

	if (host_allocator)
		host_allocator->pfnFree(host_allocator->pUserData, gltf_data);
	else
		delete[] gltf_data;

	return out;
}

void acp_vulkan::gltf_data_free(gltf_data* gltf_data, VkAllocationCallbacks* host_allocator)
{
	gltf_data->gltf_state = acp_vulkan::gltf_data::gltf_state_type::deleted;

	delete_asset_data(&gltf_data->asset, host_allocator);

	//buffer views
	free_gltf_buffer(gltf_data->buffer_views, host_allocator);

	//buffers
	for (size_t ii = 0; ii < gltf_data->buffers.data_length; ++ii)
		delete_buffer_data(&gltf_data->buffers.data[ii], host_allocator);
	free_gltf_buffer(gltf_data->buffers, host_allocator);

	//images
	for (size_t ii = 0; ii < gltf_data->images.data_length; ++ii)
		delete_image_data(&gltf_data->images.data[ii], host_allocator);
	free_gltf_buffer(gltf_data->images, host_allocator);

	//accesors
	for (size_t ii = 0; ii < gltf_data->accesors.data_length; ++ii)
		delete_accesor_data(&gltf_data->accesors.data[ii], host_allocator);
	free_gltf_buffer(gltf_data->accesors, host_allocator);

	//textures
	for (size_t ii = 0; ii < gltf_data->textures.data_length; ++ii)
		delete_texture_data(&gltf_data->textures.data[ii], host_allocator);
	free_gltf_buffer(gltf_data->textures, host_allocator);

	//meshes
	for (size_t ii = 0; ii < gltf_data->meshes.data_length; ++ii)
		delete_mesh_data(&gltf_data->meshes.data[ii], host_allocator);
	free_gltf_buffer(gltf_data->meshes, host_allocator);

	//materials
	for (size_t ii = 0; ii < gltf_data->materials.data_length; ++ii)
		delete_material_data(&gltf_data->materials.data[ii], host_allocator);
	free_gltf_buffer(gltf_data->materials, host_allocator);

	//nodes
	for (size_t ii = 0; ii < gltf_data->nodes.data_length; ++ii)
		delete_node_data(&gltf_data->nodes.data[ii], host_allocator);
	free_gltf_buffer(gltf_data->nodes, host_allocator);

	//scenes
	for (size_t ii = 0; ii < gltf_data->scenes.data_length; ++ii)
		delete_scene_data(&gltf_data->scenes.data[ii], host_allocator);
	free_gltf_buffer(gltf_data->scenes, host_allocator);

	//samplers
	for (size_t ii = 0; ii < gltf_data->samplers.data_length; ++ii)
		delete_sampler_data(&gltf_data->samplers.data[ii], host_allocator);
	free_gltf_buffer(gltf_data->samplers, host_allocator);

	//skins
	for (size_t ii = 0; ii < gltf_data->skins.data_length; ++ii)
		delete_skin_data(&gltf_data->skins.data[ii], host_allocator);
	free_gltf_buffer(gltf_data->skins, host_allocator);

	//cameras
	for (size_t ii = 0; ii < gltf_data->cameras.data_length; ++ii)
		delete_camera_data(&gltf_data->cameras.data[ii], host_allocator);
	free_gltf_buffer(gltf_data->cameras, host_allocator);

	//animations
	for (size_t ii = 0; ii < gltf_data->animations.data_length; ++ii)
		delete_animation_data(&gltf_data->animations.data[ii], host_allocator);
	free_gltf_buffer(gltf_data->animations, host_allocator);
}