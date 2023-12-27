#include "acp_gltf_vulkan.h"
#include <inttypes.h>
#include <ctype.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#include <vector>

#define TOKENS_WITH_DIFFERENT_REPRESENATATION(X) \
	X(open_curly, {)\
	X(none, ¦)\
	X(close_curly, })\
	X(open_bracket, [)\
	X(closed_bracket, ])\
	X(colon, :) \

#define TOKENS(X) \
	X(accessors)\
	X(bufferView)\
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
	X(SCALAR)

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

struct token {
	token_types type;
	acp_vulkan::gltf_data::string_view view;
	union value
	{
		double		as_real;
		int64_t		as_int;
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
							.data = state->data + state->next_char,
							.data_length = token_length
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
						.data = state->data + state->next_char,
						.data_length = (last_location + 1) - state->next_char
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
		if (expect(value, token_types::is_string) == return_value::error_value)
			return return_value::error_value;
		
		*target = { value.view.data + 1, value.view.data_length - 2 };
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

std::pair<acp_vulkan::gltf_data::asset_type, return_value> parse_asset(tokenizer_state* state)
{
	acp_vulkan::gltf_data::asset_type out{};

	if (expect_ordered_and_discard_tokens(state, { token_types::colon, token_types::open_curly }) == return_value::error_value)
		return { {}, return_value::error_value };

	while(true)
	{
		{
			return_value r = try_read_string_property_in_to(state, token_types::generator, &out.generator);
			if (r == return_value::error_value)
				return { {}, return_value::error_value };
			else if (r == return_value::true_value)
				continue;
		}
		{
			return_value r = try_read_string_property_in_to(state, token_types::version, &out.version);
			if (r == return_value::error_value)
				return { {}, return_value::error_value };
			else if (r == return_value::true_value)
				continue;
		}

		{
			return_value r = discard_next_or_return_if_token(state, token_types::close_curly);
			if (r == return_value::error_value)
				return { {}, return_value::error_value };
			else if (r == return_value::true_value)
				break;
		}
	}

	if (expect(next_token(state), token_types::close_curly) == return_value::error_value)
		return { {}, return_value::error_value };

	return { out, return_value::true_value };
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
			case token_types::asset:
			{
				auto asset_data = parse_asset(&state);
				if (asset_data.second == return_value::error_value)
					return { .gltf_state = acp_vulkan::gltf_data::parsing_error, .parsing_error_location = state.next_char };
				out.asset = std::move(asset_data.first);
				break;
			}
		}

		if (t.type == token_types::eof)
			break;
	}

	return out;
}
