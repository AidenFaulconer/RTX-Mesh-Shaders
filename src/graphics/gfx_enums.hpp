/*!
 *  \author    Viktor Zoutman
 *  \date      2019-2020
 *  \copyright GNU General Public License v3.0
 */

#pragma once

#include <vector>
#include "../util/log.hpp"
#include "vulkan/vulkan.h"

namespace gfx::enums
{

	enum class ShaderType
	{
		VERTEX,
		PIXEL,
		COMPUTE,
		GEOMETRY,
		MESH,
		TASK,
		RT_MISS,
		RT_RAYGEN,
		RT_CLOSEST,
		RT_ANY
	};

	enum class TextureFilter
	{
		FILTER_LINEAR,
		FILTER_POINT,
		FILTER_LINEAR_POINT,
		FILTER_ANISOTROPIC
	};

	enum class TextureAddressMode
	{
		TAM_MIRROR_ONCE = (int)VK_SAMPLER_ADDRESS_MODE_MIRROR_CLAMP_TO_EDGE,
		TAM_MIRROR = (int)VK_SAMPLER_ADDRESS_MODE_MIRRORED_REPEAT,
		TAM_CLAMP = (int)VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
		TAM_BORDER = (int)VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER,
		TAM_WRAP = (int)VK_SAMPLER_ADDRESS_MODE_REPEAT,
	};

	enum class BorderColor
	{
		BORDER_TRANSPARENT = (int)VK_BORDER_COLOR_FLOAT_TRANSPARENT_BLACK,
		BORDER_BLACK = (int)VK_BORDER_COLOR_FLOAT_OPAQUE_BLACK,
		BORDER_WHITE = (int)VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE,
	};

	enum class BufferDescType
	{
		STORAGE = (int)VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
		TEXEL_STORAGE = (int)VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER,
		UNIFORM = (int)VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
		TEXEL_UNIFORM = (int)VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER,
	};

	enum BufferUsageFlag
	{
		INDEX_BUFFER = (int)VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
		VERTEX_BUFFER = (int)VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
		STORAGE_DST_VERTEX_BUFFER = (int)VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
		TEXEL_UNIFORM_DST_VERTEX_BUFFER = (int)VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER,
		STORAGE_DST_INDEX_BUFFER = (int)VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
		CONSTANT_BUFFER = (int)VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
		TRANSFER_SRC = (int)VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
		TRANSFER_DST = (int)VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
		STORAGE = (int)VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
		RAYTRACING = (int)VK_BUFFER_USAGE_RAY_TRACING_BIT_NV,
		RAYTRACING_CB = (int)VK_BUFFER_USAGE_RAY_TRACING_BIT_NV | VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
		RAYTRACING_STORAGE = (int)VK_BUFFER_USAGE_RAY_TRACING_BIT_NV | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
	};

	enum class PipelineType
	{
		COMPUTE_PIPE,
		GRAPHICS_PIPE,
		RAYTRACING_PIPE,
	};

	inline bool FormatHasStencilComponent(VkFormat format)
	{
		return format == VK_FORMAT_D32_SFLOAT_S8_UINT || format == VK_FORMAT_D24_UNORM_S8_UINT;
	}

	inline std::size_t BitsPerPixel(VkFormat format)
	{
		switch(format)
		{
			case VK_FORMAT_R32G32B32A32_SFLOAT:
			case VK_FORMAT_R32G32B32A32_SINT:
			case VK_FORMAT_R32G32B32A32_UINT:
				return 128;
			case VK_FORMAT_R32G32B32_SFLOAT:
			case VK_FORMAT_R32G32B32_SINT:
			case VK_FORMAT_R32G32B32_UINT:
				return 96;
			case VK_FORMAT_R8G8B8A8_UNORM:
			case VK_FORMAT_B8G8R8A8_UNORM:
			case VK_FORMAT_B8G8R8A8_SRGB:
			case VK_FORMAT_R8G8B8A8_SRGB:
				return 32;

			default:
				LOGW("Unsupported format in bytes per pixel returning 4");
				return 4;
		}
	}

	inline std::size_t BytesPerPixel(VkFormat format)
	{
		return BitsPerPixel(format) / 8;
	}

}