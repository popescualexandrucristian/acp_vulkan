#include "acp_gltf_vulkan.h"
#include <ctype.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#define MAX_TOKENS_PER_ENTITY (1024 * 1024)


#define TOKENS_WITH_DIFFERENT_REPRESENATATION(X) \
	X(open_curly, {)\
	X(none, ¦)\
	X(close_curly, })\
	X(open_bracket, [)\
	X(closed_bracket, ])\
	X(colon, :) \
	X(true_value, true) \
	X(false_value, false) \

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
	X(WEIGHTS_9)\
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
	X(normalized)

#define TO_ENUM_VALUE1(x, y) x,
#define TO_ENUM_VALUE2(x) x,
enum class token_types : uint32_t {
	TOKENS_WITH_DIFFERENT_REPRESENATATION(TO_ENUM_VALUE1)
	TOKENS(TO_ENUM_VALUE2)
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
	","
};
#undef TO_STRING_VALUE1
#undef TO_STRING_VALUE2

#define TO_STRING_VALUE1(x, y) false,
#define TO_STRING_VALUE2(x) true,
static const bool is_string_like[] = {
	TOKENS_WITH_DIFFERENT_REPRESENATATION(TO_STRING_VALUE1)
	TOKENS(TO_STRING_VALUE2)
	false,//comma
	false, //TOKENS_COUNT,
	false, //is_float,
	false, //is_int,
	true, //is_string,
	false //eof
};
#undef TO_STRING_VALUE1
#undef TO_STRING_VALUE2

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
			if ((state->next_char + token_length) < state->data_size)
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

return_value try_read_string_property_in_to(tokenizer_state* state, token_types type, acp_vulkan::gltf_data::string_view* target)
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
		
		*target = value.view;
		return return_value::true_value;
	}
	return return_value::false_value;
}

template<typename T>
return_value try_read_real_property_to(tokenizer_state* state, token_types type, T* target)
{
	peeked_token t = peek_token(state);
	if (t.token.type == type)
	{
		next_token(state);
		if (expect(next_token(state), token_types::colon) == return_value::error_value)
			return return_value::error_value;
		token value = next_token(state);
		if (value.type != token_types::is_float)
			return return_value::error_value;

		*target = T(value.value.as_real);
		return return_value::true_value;
	}
	return return_value::false_value;
}

template<typename T>
return_value try_read_bool_property_to(tokenizer_state* state, token_types type, T* target)
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
return_value try_read_int_property_to(tokenizer_state* state, token_types type, T* target)
{
	peeked_token t = peek_token(state);
	if (t.token.type == type)
	{
		next_token(state);
		if (expect(next_token(state), token_types::colon) == return_value::error_value)
			return return_value::error_value;
		token value = next_token(state);
		if (value.type != token_types::is_int)
			return return_value::error_value;

		*target = T(value.value.as_int);
		return return_value::true_value;
	}
	return return_value::false_value;
}

return_value expect_ordered_and_discard_tokens(tokenizer_state* state, std::initializer_list<token_types> expected)
{
	for (token_types t : expected)
		if (expect(next_token(state), t) == return_value::error_value)
			return return_value::error_value;

	return return_value::true_value;
}

return_value discard_next_or_return_if_token(tokenizer_state* state, token_types expected)
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

return_value return_if_token(tokenizer_state* state, token_types expected)
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

return_value skip_object_if_found(tokenizer_state* state)
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
			else if (t.type == token_types::open_curly)
			{
				ariety++;
				continue;
			}
			else if (t.type == token_types::close_curly)
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
		skip_object_if_found(state);														\
	}

#define BREAK_LOOP_ON_TOKRN_OR_ERROR(TOKEN)													\
	{																						\
		return_value r = return_if_token(state, TOKEN);										\
		if (r == return_value::error_value)													\
			return { {}, return_value::error_value };										\
		else if (r == return_value::true_value)												\
			break;																			\
	}

return_value discard_next_if_token(tokenizer_state* state, token_types expected)
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
		return_value r = try_read_string_property_in_to(state, TOKEN, TARGET);								\
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

#define DESCARD_IF_EXCPECTED_RETURN_OTHERWISE(TOKEN)						\
	{																		\
		auto r = discard_next_if_token(state, TOKEN);						\
		if (r == return_value::error_value)									\
			return { {}, return_value::error_value };						\
		else if (r == return_value::false_value)							\
			return { {}, return_value::false_value };						\
	}

#define ELEMENT_START \
	for(size_t element_max_count = 0; element_max_count < MAX_TOKENS_PER_ENTITY; ++element_max_count)	\
	{

#define ELEMENT_END \
		if (element_max_count == MAX_TOKENS_PER_ENTITY - 1)															\
			return { {}, return_value::error_value };													\
	}

std::pair<acp_vulkan::gltf_data::asset_type, return_value> parse_asset(tokenizer_state* state)
{
	acp_vulkan::gltf_data::asset_type out{};

	EXPECT_AND_DESCARD(token_types::colon, token_types::open_curly);

	ELEMENT_START
		TRY_READ_STRING_OR_REPORT_ERROR(token_types::generator, &out.generator);
		TRY_READ_STRING_OR_REPORT_ERROR(token_types::version, &out.version);

		BREAK_LOOP_ON_TOKRN_OR_ERROR_DISCARD_OTHERWISE(token_types::close_curly)
	ELEMENT_END

	EXPECT_AND_DESCARD(token_types::close_curly);

	return { out, return_value::true_value };
}

std::pair<acp_vulkan::gltf_data::buffer_view, return_value> parse_buffer_view(tokenizer_state* state)
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

std::pair<std::vector<acp_vulkan::gltf_data::buffer_view>, return_value> parse_buffer_views(tokenizer_state* state)
{
	std::vector<acp_vulkan::gltf_data::buffer_view> out{};

	EXPECT_AND_DESCARD(token_types::colon, token_types::open_bracket);

	ELEMENT_START
		GLTF_ELEMENT(parse_buffer_view);
		BREAK_LOOP_ON_TOKRN_OR_ERROR(token_types::closed_bracket);
	ELEMENT_END

	EXPECT_AND_DESCARD(token_types::closed_bracket);

	return { std::move(out), return_value::true_value };
}

std::pair<acp_vulkan::gltf_data::buffer, return_value> parse_buffer(tokenizer_state* state)
{
	acp_vulkan::gltf_data::buffer out{};

	DESCARD_IF_EXCPECTED_RETURN_OTHERWISE(token_types::open_curly);

	ELEMENT_START
		TRY_READ_STRING_OR_REPORT_ERROR(token_types::uri, &out.uri);
		TRY_READ_STRING_OR_REPORT_ERROR(token_types::name, &out.name);
		TRY_READ_INT_VALUE_OR_REPORT_ERROR(token_types::byteLength, &out.byte_length);
		BREAK_LOOP_ON_TOKRN_OR_ERROR_DISCARD_OTHERWISE(token_types::close_curly);
	ELEMENT_END

	DESCARD_IF_EXCPECTED(token_types::close_curly);

	return { std::move(out), return_value::true_value };
}

std::pair<std::vector<acp_vulkan::gltf_data::buffer>, return_value> parse_buffers(tokenizer_state* state)
{
	std::vector<acp_vulkan::gltf_data::buffer> out{};

	EXPECT_AND_DESCARD(token_types::colon, token_types::open_bracket);

	ELEMENT_START
		GLTF_ELEMENT(parse_buffer);
		BREAK_LOOP_ON_TOKRN_OR_ERROR(token_types::closed_bracket);
	ELEMENT_END

	EXPECT_AND_DESCARD(token_types::closed_bracket);

	return { std::move(out), return_value::true_value };
}

std::pair<acp_vulkan::gltf_data::image, return_value> parse_image(tokenizer_state* state)
{
	acp_vulkan::gltf_data::image out{};

	DESCARD_IF_EXCPECTED_RETURN_OTHERWISE(token_types::open_curly);

	ELEMENT_START
		TRY_READ_STRING_OR_REPORT_ERROR(token_types::uri, &out.uri);
		TRY_READ_INT_VALUE_OR_REPORT_ERROR(token_types::bufferView, &out.buffer_view);
		TRY_READ_STRING_OR_REPORT_ERROR(token_types::mimeType, &out.mime_type);
		BREAK_LOOP_ON_TOKRN_OR_ERROR_DISCARD_OTHERWISE(token_types::close_curly);
	ELEMENT_END

	DESCARD_IF_EXCPECTED(token_types::close_curly);

	return { std::move(out), return_value::true_value };
}

std::pair<std::vector<acp_vulkan::gltf_data::image>, return_value> parse_images(tokenizer_state* state)
{
	std::vector<acp_vulkan::gltf_data::image> out{};

	EXPECT_AND_DESCARD(token_types::colon, token_types::open_bracket);

	ELEMENT_START
		GLTF_ELEMENT(parse_image);
		BREAK_LOOP_ON_TOKRN_OR_ERROR(token_types::closed_bracket);
	ELEMENT_END

	EXPECT_AND_DESCARD(token_types::closed_bracket);

	return { std::move(out), return_value::true_value };
}


std::pair<std::vector<float>, return_value> parse_float_array(tokenizer_state* state)
{
	std::vector<float> out{};

	EXPECT_AND_DESCARD(token_types::open_bracket);

	ELEMENT_START
		peeked_token t = peek_token(state);
		if (t.token.type == token_types::is_float)
		{
			out.push_back(float(t.token.value.as_real));
			next_token(state);
		}
		DESCARD_IF_EXCPECTED(token_types::comma);
		BREAK_LOOP_ON_TOKRN_OR_ERROR(token_types::closed_bracket);
	ELEMENT_END

	EXPECT_AND_DESCARD(token_types::closed_bracket);

	return { std::move(out), return_value::true_value };
}

template<size_t MAX_TARGETS, typename V, typename T>
return_value try_read_float_array_property_to(tokenizer_state* state, token_types type, T target, V* found_values)
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
			if (value.first.size() <= ii)
				break;
			target[ii] = value.first[ii];
			if (found_values)
				found_values++;
		}
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

std::pair<acp_vulkan::gltf_data::accesor, return_value> parse_accesor(tokenizer_state* state)
{
	acp_vulkan::gltf_data::accesor out{};

	DESCARD_IF_EXCPECTED_RETURN_OTHERWISE(token_types::open_curly);


	/*
	* struct accesor
		{
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
	*/
	ELEMENT_START
		TRY_READ_INT_VALUE_OR_REPORT_ERROR(token_types::bufferView, &out.buffer_view);
		TRY_READ_INT_VALUE_OR_REPORT_ERROR(token_types::byteOffset, &out.byte_offset);
		TRY_READ_INT_VALUE_OR_REPORT_ERROR(token_types::componentType, &out.component_type);
		TRY_READ_BOOL_VALUE_OR_REPORT_ERROR(token_types::normalized, &out.normalized);
		TRY_READ_INT_VALUE_OR_REPORT_ERROR(token_types::count, &out.count);
		TRY_READ_STRING_OR_REPORT_ERROR(token_types::type, &out.type);
		TRY_READ_STRING_OR_REPORT_ERROR(token_types::name, &out.name);
		TRY_READ_FLOAT_ARRAY_OR_REPORT_ERROR(token_types::min, static_cast<float*>(out.min), sizeof(out.min) / sizeof(out.min[0]));
		TRY_READ_FLOAT_ARRAY_OR_REPORT_ERROR(token_types::max, static_cast<float*>(out.max), sizeof(out.max) / sizeof(out.max[0]));

		//todo(alex) : Implement sparse structure read. and the rest of the thing !

		BREAK_LOOP_ON_TOKRN_OR_ERROR_DISCARD_OTHERWISE(token_types::close_curly);
	ELEMENT_END

	DESCARD_IF_EXCPECTED(token_types::close_curly);

	return { std::move(out), return_value::true_value };
}

std::pair<std::vector<acp_vulkan::gltf_data::accesor>, return_value> parse_accesors(tokenizer_state* state)
{
	std::vector<acp_vulkan::gltf_data::accesor> out{};

	EXPECT_AND_DESCARD(token_types::colon, token_types::open_bracket);

	ELEMENT_START
		GLTF_ELEMENT(parse_accesor);
		BREAK_LOOP_ON_TOKRN_OR_ERROR(token_types::closed_bracket);
	ELEMENT_END

	EXPECT_AND_DESCARD(token_types::closed_bracket);

	return { std::move(out), return_value::true_value };
}

#define GLTF_SECTION(TARGET, CALL, DESTINSTION)																			\
	case TARGET:																										\
	{																													\
		auto asset_data = CALL(&state);																					\
		if (asset_data.second == return_value::error_value)																\
			return { .gltf_state = acp_vulkan::gltf_data::parsing_error, .parsing_error_location = state.next_char };	\
		out.DESTINSTION = std::move(asset_data.first);																	\
		found_sections.push_back(TARGET);																				\
		break;																											\
	}

acp_vulkan::gltf_data acp_vulkan::gltf_data_from_memory(const char* data, size_t data_size)
{
	acp_vulkan::gltf_data out{};

	tokenizer_state state{
		.data = data,
		.data_size = data_size
	};

	if (expect(next_token(&state), token_types::open_curly) == return_value::error_value)
		return {.gltf_state = acp_vulkan::gltf_data::parsing_error, .parsing_error_location = state.next_char};

	std::vector<token_types> found_sections;
	std::pair<token_types, acp_vulkan::gltf_data::gltf_state_type> mandatory_sections[] = {
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
			GLTF_SECTION(token_types::asset, parse_asset, asset);
			GLTF_SECTION(token_types::bufferViews, parse_buffer_views, buffer_views);
			GLTF_SECTION(token_types::buffers, parse_buffers, buffers);
			GLTF_SECTION(token_types::images, parse_images, images);
			GLTF_SECTION(token_types::accessors, parse_accesors, accesors);
		}

		if (t.type == token_types::eof)
			break;
	}

	for (auto ii : mandatory_sections)
	{
		if (std::find(found_sections.cbegin(), found_sections.cend(), ii.first) == found_sections.cend())
		{
			return { .gltf_state = ii.second, .parsing_error_location = 0 };
			assert(false);
		}
	}

	return out;
}
