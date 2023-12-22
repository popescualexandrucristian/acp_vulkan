#include "acp_dds_vulkan.h"
#include <inttypes.h>
#include <algorithm>
#include <stdio.h>
#include <malloc.h>

// header dwFlags
typedef enum DDSD_FLAGS {
	DDSD_CAPS = 0x1,
	DDSD_HEIGHT = 0x2,
	DDSD_WIDTH = 0x4,
	DDSD_PITCH = 0x8,
	DDSD_PIXELFORMAT = 0x1000,
	DDSD_MIPMAPCOUNT = 0x20000,
	DDSD_LINEARSIZE = 0x80000,
	DDSD_DEPTH = 0x800000
} DDSD_FLAGS;

// header dwCaps
typedef enum DDSC_FLAGS {
	DDSCAPS_COMPLEX = 0x8,
	DDSCAPS_TEXTURE = 0x1000,
	DDSCAPS_MIPMAP = 0x400000
} DDSC_FLAGS;

// header dwCaps2
typedef enum DDSC_FLAGS2 {
	DDSCAPS2_CUBEMAP = 0x200,
	DDSCAPS2_CUBEMAP_POSITIVEX = 0x400,
	DDSCAPS2_CUBEMAP_NEGATIVEX = 0x800,
	DDSCAPS2_CUBEMAP_POSITIVEY = 0x1000,
	DDSCAPS2_CUBEMAP_NEGATIVEY = 0x2000,
	DDSCAPS2_CUBEMAP_POSITIVEZ = 0x4000,
	DDSCAPS2_CUBEMAP_NEGATIVEZ = 0x8000,
	DDSCAPS2_VOLUME = 0x200000,
	DDSCAPS2_CUBEMAP_ALLFACES = (DDSCAPS2_CUBEMAP_POSITIVEX | DDSCAPS2_CUBEMAP_NEGATIVEX | DDSCAPS2_CUBEMAP_POSITIVEY | DDSCAPS2_CUBEMAP_NEGATIVEY | DDSCAPS2_CUBEMAP_POSITIVEZ | DDSCAPS2_CUBEMAP_NEGATIVEZ)
} DDSC_FLAGS2;



// pixel format dwFlags
typedef enum DDPF_FLAGS {
	DDPF_ALPHAPIXELS = 0x1,
	DDPF_ALPHA = 0x2,
	DDPF_FOURCC = 0x4,
	DDPF_RGB = 0x40,
	DDPF_YUV = 0x200,
	DDPF_LUMINANCE = 0x20000
} DDPF_FLAGS;


// DX10 dxgiFormat value
typedef enum DXGI_FORMAT {
	DXGI_FORMAT_UNKNOWN = 0,
	DXGI_FORMAT_R32G32B32A32_TYPELESS = 1,
	DXGI_FORMAT_R32G32B32A32_FLOAT = 2,
	DXGI_FORMAT_R32G32B32A32_UINT = 3,
	DXGI_FORMAT_R32G32B32A32_SINT = 4,
	DXGI_FORMAT_R32G32B32_TYPELESS = 5,
	DXGI_FORMAT_R32G32B32_FLOAT = 6,
	DXGI_FORMAT_R32G32B32_UINT = 7,
	DXGI_FORMAT_R32G32B32_SINT = 8,
	DXGI_FORMAT_R16G16B16A16_TYPELESS = 9,
	DXGI_FORMAT_R16G16B16A16_FLOAT = 10,
	DXGI_FORMAT_R16G16B16A16_UNORM = 11,
	DXGI_FORMAT_R16G16B16A16_UINT = 12,
	DXGI_FORMAT_R16G16B16A16_SNORM = 13,
	DXGI_FORMAT_R16G16B16A16_SINT = 14,
	DXGI_FORMAT_R32G32_TYPELESS = 15,
	DXGI_FORMAT_R32G32_FLOAT = 16,
	DXGI_FORMAT_R32G32_UINT = 17,
	DXGI_FORMAT_R32G32_SINT = 18,
	DXGI_FORMAT_R32G8X24_TYPELESS = 19,
	DXGI_FORMAT_D32_FLOAT_S8X24_UINT = 20,
	DXGI_FORMAT_R32_FLOAT_X8X24_TYPELESS = 21,
	DXGI_FORMAT_X32_TYPELESS_G8X24_UINT = 22,
	DXGI_FORMAT_R10G10B10A2_TYPELESS = 23,
	DXGI_FORMAT_R10G10B10A2_UNORM = 24,
	DXGI_FORMAT_R10G10B10A2_UINT = 25,
	DXGI_FORMAT_R11G11B10_FLOAT = 26,
	DXGI_FORMAT_R8G8B8A8_TYPELESS = 27,
	DXGI_FORMAT_R8G8B8A8_UNORM = 28,
	DXGI_FORMAT_R8G8B8A8_UNORM_SRGB = 29,
	DXGI_FORMAT_R8G8B8A8_UINT = 30,
	DXGI_FORMAT_R8G8B8A8_SNORM = 31,
	DXGI_FORMAT_R8G8B8A8_SINT = 32,
	DXGI_FORMAT_R16G16_TYPELESS = 33,
	DXGI_FORMAT_R16G16_FLOAT = 34,
	DXGI_FORMAT_R16G16_UNORM = 35,
	DXGI_FORMAT_R16G16_UINT = 36,
	DXGI_FORMAT_R16G16_SNORM = 37,
	DXGI_FORMAT_R16G16_SINT = 38,
	DXGI_FORMAT_R32_TYPELESS = 39,
	DXGI_FORMAT_D32_FLOAT = 40,
	DXGI_FORMAT_R32_FLOAT = 41,
	DXGI_FORMAT_R32_UINT = 42,
	DXGI_FORMAT_R32_SINT = 43,
	DXGI_FORMAT_R24G8_TYPELESS = 44,
	DXGI_FORMAT_D24_UNORM_S8_UINT = 45,
	DXGI_FORMAT_R24_UNORM_X8_TYPELESS = 46,
	DXGI_FORMAT_X24_TYPELESS_G8_UINT = 47,
	DXGI_FORMAT_R8G8_TYPELESS = 48,
	DXGI_FORMAT_R8G8_UNORM = 49,
	DXGI_FORMAT_R8G8_UINT = 50,
	DXGI_FORMAT_R8G8_SNORM = 51,
	DXGI_FORMAT_R8G8_SINT = 52,
	DXGI_FORMAT_R16_TYPELESS = 53,
	DXGI_FORMAT_R16_FLOAT = 54,
	DXGI_FORMAT_D16_UNORM = 55,
	DXGI_FORMAT_R16_UNORM = 56,
	DXGI_FORMAT_R16_UINT = 57,
	DXGI_FORMAT_R16_SNORM = 58,
	DXGI_FORMAT_R16_SINT = 59,
	DXGI_FORMAT_R8_TYPELESS = 60,
	DXGI_FORMAT_R8_UNORM = 61,
	DXGI_FORMAT_R8_UINT = 62,
	DXGI_FORMAT_R8_SNORM = 63,
	DXGI_FORMAT_R8_SINT = 64,
	DXGI_FORMAT_A8_UNORM = 65,
	DXGI_FORMAT_R1_UNORM = 66,
	DXGI_FORMAT_R9G9B9E5_SHAREDEXP = 67,
	DXGI_FORMAT_R8G8_B8G8_UNORM = 68,
	DXGI_FORMAT_G8R8_G8B8_UNORM = 69,
	DXGI_FORMAT_BC1_TYPELESS = 70,
	DXGI_FORMAT_BC1_UNORM = 71,
	DXGI_FORMAT_BC1_UNORM_SRGB = 72,
	DXGI_FORMAT_BC2_TYPELESS = 73,
	DXGI_FORMAT_BC2_UNORM = 74,
	DXGI_FORMAT_BC2_UNORM_SRGB = 75,
	DXGI_FORMAT_BC3_TYPELESS = 76,
	DXGI_FORMAT_BC3_UNORM = 77,
	DXGI_FORMAT_BC3_UNORM_SRGB = 78,
	DXGI_FORMAT_BC4_TYPELESS = 79,
	DXGI_FORMAT_BC4_UNORM = 80,
	DXGI_FORMAT_BC4_SNORM = 81,
	DXGI_FORMAT_BC5_TYPELESS = 82,
	DXGI_FORMAT_BC5_UNORM = 83,
	DXGI_FORMAT_BC5_SNORM = 84,
	DXGI_FORMAT_B5G6R5_UNORM = 85,
	DXGI_FORMAT_B5G5R5A1_UNORM = 86,
	DXGI_FORMAT_B8G8R8A8_UNORM = 87,
	DXGI_FORMAT_B8G8R8X8_UNORM = 88,
	DXGI_FORMAT_R10G10B10_XR_BIAS_A2_UNORM = 89,
	DXGI_FORMAT_B8G8R8A8_TYPELESS = 90,
	DXGI_FORMAT_B8G8R8A8_UNORM_SRGB = 91,
	DXGI_FORMAT_B8G8R8X8_TYPELESS = 92,
	DXGI_FORMAT_B8G8R8X8_UNORM_SRGB = 93,
	DXGI_FORMAT_BC6H_TYPELESS = 94,
	DXGI_FORMAT_BC6H_UF16 = 95,
	DXGI_FORMAT_BC6H_SF16 = 96,
	DXGI_FORMAT_BC7_TYPELESS = 97,
	DXGI_FORMAT_BC7_UNORM = 98,
	DXGI_FORMAT_BC7_UNORM_SRGB = 99,
	DXGI_FORMAT_AYUV = 100,
	DXGI_FORMAT_Y410 = 101,
	DXGI_FORMAT_Y416 = 102,
	DXGI_FORMAT_NV12 = 103,
	DXGI_FORMAT_P010 = 104,
	DXGI_FORMAT_P016 = 105,
	DXGI_FORMAT_420_OPAQUE = 106,
	DXGI_FORMAT_YUY2 = 107,
	DXGI_FORMAT_Y210 = 108,
	DXGI_FORMAT_Y216 = 109,
	DXGI_FORMAT_NV11 = 110,
	DXGI_FORMAT_AI44 = 111,
	DXGI_FORMAT_IA44 = 112,
	DXGI_FORMAT_P8 = 113,
	DXGI_FORMAT_A8P8 = 114,
	DXGI_FORMAT_B4G4R4A4_UNORM = 115,
	DXGI_FORMAT_P208 = 130,
	DXGI_FORMAT_V208 = 131,
	DXGI_FORMAT_V408 = 132,
	DXGI_FORMAT_FORCE_UINT = 0xffffffff
} DXGI_FORMAT;

// DX10 resourceDimension value
typedef enum D3D10_RESOURCE_DIMENSION {
	D3D10_RESOURCE_DIMENSION_UNKNOWN = 0,
	D3D10_RESOURCE_DIMENSION_BUFFER = 1,
	D3D10_RESOURCE_DIMENSION_TEXTURE1D = 2,
	D3D10_RESOURCE_DIMENSION_TEXTURE2D = 3,
	D3D10_RESOURCE_DIMENSION_TEXTURE3D = 4
} D3D10_RESOURCE_DIMENSION;

// DX10 miscFlag flags
typedef enum D3D10_RESOURCE_MISC_FLAG {
	D3D10_RESOURCE_MISC_GENERATE_MIPS = 0x1,
	D3D10_RESOURCE_MISC_SHARED = 0x2,
	D3D10_RESOURCE_MISC_TEXTURECUBE = 0x4,
	D3D10_RESOURCE_MISC_SHARED_KEYEDMUTEX = 0x10,
	D3D10_RESOURCE_MISC_GDI_COMPATIBLE = 0x20
} D3D10_RESOURCE_MISC_FLAG;

// DX10 miscFlag flags
typedef enum D3D11_RESOURCE_MISC_FLAG {
	D3D11_RESOURCE_MISC_GENERATE_MIPS = 0x1,
	D3D11_RESOURCE_MISC_SHARED = 0x2,
	D3D11_RESOURCE_MISC_TEXTURECUBE = 0x4,
	D3D11_RESOURCE_MISC_DRAWINDIRECT_ARGS = 0x10,
	D3D11_RESOURCE_MISC_BUFFER_ALLOW_RAW_VIEWS = 0x20,
	D3D11_RESOURCE_MISC_BUFFER_STRUCTURED = 0x40,
	D3D11_RESOURCE_MISC_RESOURCE_CLAMP = 0x80,
	D3D11_RESOURCE_MISC_SHARED_KEYEDMUTEX = 0x100,
	D3D11_RESOURCE_MISC_GDI_COMPATIBLE = 0x200,
	D3D11_RESOURCE_MISC_SHARED_NTHANDLE = 0x800,
	D3D11_RESOURCE_MISC_RESTRICTED_CONTENT = 0x1000,
	D3D11_RESOURCE_MISC_RESTRICT_SHARED_RESOURCE = 0x2000,
	D3D11_RESOURCE_MISC_RESTRICT_SHARED_RESOURCE_DRIVER = 0x4000,
	D3D11_RESOURCE_MISC_GUARDED = 0x8000,
	D3D11_RESOURCE_MISC_TILE_POOL = 0x20000,
	D3D11_RESOURCE_MISC_TILED = 0x40000,
	D3D11_RESOURCE_MISC_HW_PROTECTED = 0x80000
} D3D11_RESOURCE_MISC_FLAG;

// DX10 miscFlags2 flags
typedef enum DX10_MISC_FLAG {
	DDS_ALPHA_MODE_UNKNOWN = 0x0,
	DDS_ALPHA_MODE_STRAIGHT = 0x1,
	DDS_ALPHA_MODE_PREMULTIPLIED = 0x2,
	DDS_ALPHA_MODE_OPAQUE = 0x3,
	DDS_ALPHA_MODE_CUSTOM = 0x4
} DX10_MISC_FLAG;



typedef struct DDS_PIXELFORMAT {
	uint32_t dwSize;
	uint32_t dwFlags;
	uint32_t dwFourCC;
	uint32_t dwRGBBitCount;
	uint32_t dwRBitMask;
	uint32_t dwGBitMask;
	uint32_t dwBBitMask;
	uint32_t dwABitMask;
} DDS_PIXELFORMAT;

typedef struct DDS_HEADER_DXT10 {
	DXGI_FORMAT					dxgiFormat;
	D3D10_RESOURCE_DIMENSION	resourceDimension;
	uint32_t					miscFlag;
	uint32_t					arraySize;
	uint32_t					miscFlags2;
} DDS_HEADER_DXT10;

typedef struct dds_file_data {
	uint32_t			dwSize;
	uint32_t			dwFlags;
	uint32_t			dwHeight;
	uint32_t			dwWidth;
	uint32_t			dwPitchOrLinearSize;
	uint32_t			dwDepth;
	uint32_t			dwMipMapCount;
	uint32_t			dwReserved1[11];
	DDS_PIXELFORMAT		ddspf;
	uint32_t			dwCaps;
	uint32_t			dwCaps2;
	uint32_t			dwCaps3;
	uint32_t			dwCaps4;
	uint32_t			dwReserved2;
	DDS_HEADER_DXT10*   ddsHeaderDx10;
	uint32_t			dwFileSize;
	uint32_t			dwBufferSize;
	unsigned char* blBuffer;
} DDSFile;


VkFormat get_vulkan_format(DXGI_FORMAT format, const bool alphaFlag) {
    switch (format) {

    case DXGI_FORMAT::DXGI_FORMAT_BC1_UNORM:
    {
        if (alphaFlag)
            return VK_FORMAT_BC1_RGBA_UNORM_BLOCK;
        else
            return VK_FORMAT_BC1_RGB_UNORM_BLOCK;
    }
    case DXGI_FORMAT::DXGI_FORMAT_BC1_UNORM_SRGB:
    {
        if (alphaFlag)
            return VK_FORMAT_BC1_RGBA_SRGB_BLOCK;
        else
            return VK_FORMAT_BC1_RGB_SRGB_BLOCK;
    }

    case DXGI_FORMAT::DXGI_FORMAT_BC2_UNORM:
        return VK_FORMAT_BC2_UNORM_BLOCK;
    case DXGI_FORMAT::DXGI_FORMAT_BC2_UNORM_SRGB:
        return VK_FORMAT_BC2_SRGB_BLOCK;

    case DXGI_FORMAT::DXGI_FORMAT_BC3_UNORM:
        return VK_FORMAT_BC3_UNORM_BLOCK;
    case DXGI_FORMAT::DXGI_FORMAT_BC3_UNORM_SRGB:
        return VK_FORMAT_BC3_SRGB_BLOCK;

    case DXGI_FORMAT::DXGI_FORMAT_BC4_UNORM:
        return VK_FORMAT_BC4_UNORM_BLOCK;
    case DXGI_FORMAT::DXGI_FORMAT_BC4_SNORM:
        return VK_FORMAT_BC4_SNORM_BLOCK;

    case DXGI_FORMAT::DXGI_FORMAT_BC5_UNORM:
        return VK_FORMAT_BC5_UNORM_BLOCK;
    case DXGI_FORMAT::DXGI_FORMAT_BC5_SNORM:
        return VK_FORMAT_BC5_SNORM_BLOCK;

    case DXGI_FORMAT::DXGI_FORMAT_BC7_UNORM:
        return VK_FORMAT_BC7_UNORM_BLOCK;
    case DXGI_FORMAT::DXGI_FORMAT_BC7_UNORM_SRGB:
        return VK_FORMAT_BC7_SRGB_BLOCK;

    case DXGI_FORMAT::DXGI_FORMAT_R8G8B8A8_UNORM:
        return VK_FORMAT_R8G8B8A8_UNORM;
    case DXGI_FORMAT::DXGI_FORMAT_R8G8B8A8_UNORM_SRGB:
        return VK_FORMAT_R8G8B8A8_SRGB;
    case DXGI_FORMAT::DXGI_FORMAT_R8G8B8A8_UINT:
        return VK_FORMAT_R8G8B8A8_UINT;
    case DXGI_FORMAT::DXGI_FORMAT_R8G8B8A8_SNORM:
        return VK_FORMAT_R8G8B8A8_SNORM;
    case DXGI_FORMAT::DXGI_FORMAT_R8G8B8A8_SINT:
        return VK_FORMAT_R8G8B8A8_SINT;
    case DXGI_FORMAT::DXGI_FORMAT_B8G8R8A8_UNORM:
        return VK_FORMAT_B8G8R8A8_UNORM;
    case DXGI_FORMAT::DXGI_FORMAT_B8G8R8A8_UNORM_SRGB:
        return VK_FORMAT_B8G8R8A8_SRGB;

    case DXGI_FORMAT::DXGI_FORMAT_R16G16B16A16_FLOAT:
        return VK_FORMAT_R16G16B16A16_SFLOAT;
    case DXGI_FORMAT::DXGI_FORMAT_R16G16B16A16_SINT:
        return VK_FORMAT_R16G16B16A16_SINT;
    case DXGI_FORMAT::DXGI_FORMAT_R16G16B16A16_UINT:
        return VK_FORMAT_R16G16B16A16_UINT;
    case DXGI_FORMAT::DXGI_FORMAT_R16G16B16A16_UNORM:
        return VK_FORMAT_R16G16B16A16_UNORM;
    case DXGI_FORMAT::DXGI_FORMAT_R16G16B16A16_SNORM:
        return VK_FORMAT_R16G16B16A16_SNORM;

    case DXGI_FORMAT::DXGI_FORMAT_R8G8_B8G8_UNORM:
    case DXGI_FORMAT::DXGI_FORMAT_G8R8_G8B8_UNORM:
    case DXGI_FORMAT::DXGI_FORMAT_YUY2:
    default:
        return VK_FORMAT_UNDEFINED;
    }
}

#define ISBITMASK(r, g, b, a) (ddpf.dwRBitMask == r && ddpf.dwGBitMask == g && ddpf.dwBBitMask == b && ddpf.dwABitMask == a)

#ifndef MAKEFOURCC
#define MAKEFOURCC(ch0, ch1, ch2, ch3)                              \
                ((uint32_t)(uint8_t)(ch0) | ((uint32_t)(uint8_t)(ch1) << 8) |       \
                ((uint32_t)(uint8_t)(ch2) << 16) | ((uint32_t)(uint8_t)(ch3) << 24))
#endif /* defined(MAKEFOURCC) */


static size_t bits_per_pixel(DXGI_FORMAT fmt)
{
    switch (fmt)
    {
    case DXGI_FORMAT::DXGI_FORMAT_R32G32B32A32_TYPELESS:
    case DXGI_FORMAT::DXGI_FORMAT_R32G32B32A32_FLOAT:
    case DXGI_FORMAT::DXGI_FORMAT_R32G32B32A32_UINT:
    case DXGI_FORMAT::DXGI_FORMAT_R32G32B32A32_SINT:
        return 128;

    case DXGI_FORMAT::DXGI_FORMAT_R32G32B32_TYPELESS:
    case DXGI_FORMAT::DXGI_FORMAT_R32G32B32_FLOAT:
    case DXGI_FORMAT::DXGI_FORMAT_R32G32B32_UINT:
    case DXGI_FORMAT::DXGI_FORMAT_R32G32B32_SINT:
        return 96;

    case DXGI_FORMAT::DXGI_FORMAT_R16G16B16A16_TYPELESS:
    case DXGI_FORMAT::DXGI_FORMAT_R16G16B16A16_FLOAT:
    case DXGI_FORMAT::DXGI_FORMAT_R16G16B16A16_UNORM:
    case DXGI_FORMAT::DXGI_FORMAT_R16G16B16A16_UINT:
    case DXGI_FORMAT::DXGI_FORMAT_R16G16B16A16_SNORM:
    case DXGI_FORMAT::DXGI_FORMAT_R16G16B16A16_SINT:
    case DXGI_FORMAT::DXGI_FORMAT_R32G32_TYPELESS:
    case DXGI_FORMAT::DXGI_FORMAT_R32G32_FLOAT:
    case DXGI_FORMAT::DXGI_FORMAT_R32G32_UINT:
    case DXGI_FORMAT::DXGI_FORMAT_R32G32_SINT:
    case DXGI_FORMAT::DXGI_FORMAT_R32G8X24_TYPELESS:
    case DXGI_FORMAT::DXGI_FORMAT_D32_FLOAT_S8X24_UINT:
    case DXGI_FORMAT::DXGI_FORMAT_R32_FLOAT_X8X24_TYPELESS:
    case DXGI_FORMAT::DXGI_FORMAT_X32_TYPELESS_G8X24_UINT:
        return 64;

    case DXGI_FORMAT::DXGI_FORMAT_R10G10B10A2_TYPELESS:
    case DXGI_FORMAT::DXGI_FORMAT_R10G10B10A2_UNORM:
    case DXGI_FORMAT::DXGI_FORMAT_R10G10B10A2_UINT:
    case DXGI_FORMAT::DXGI_FORMAT_R11G11B10_FLOAT:
    case DXGI_FORMAT::DXGI_FORMAT_R8G8B8A8_TYPELESS:
    case DXGI_FORMAT::DXGI_FORMAT_R8G8B8A8_UNORM:
    case DXGI_FORMAT::DXGI_FORMAT_R8G8B8A8_UNORM_SRGB:
    case DXGI_FORMAT::DXGI_FORMAT_R8G8B8A8_UINT:
    case DXGI_FORMAT::DXGI_FORMAT_R8G8B8A8_SNORM:
    case DXGI_FORMAT::DXGI_FORMAT_R8G8B8A8_SINT:
    case DXGI_FORMAT::DXGI_FORMAT_R16G16_TYPELESS:
    case DXGI_FORMAT::DXGI_FORMAT_R16G16_FLOAT:
    case DXGI_FORMAT::DXGI_FORMAT_R16G16_UNORM:
    case DXGI_FORMAT::DXGI_FORMAT_R16G16_UINT:
    case DXGI_FORMAT::DXGI_FORMAT_R16G16_SNORM:
    case DXGI_FORMAT::DXGI_FORMAT_R16G16_SINT:
    case DXGI_FORMAT::DXGI_FORMAT_R32_TYPELESS:
    case DXGI_FORMAT::DXGI_FORMAT_D32_FLOAT:
    case DXGI_FORMAT::DXGI_FORMAT_R32_FLOAT:
    case DXGI_FORMAT::DXGI_FORMAT_R32_UINT:
    case DXGI_FORMAT::DXGI_FORMAT_R32_SINT:
    case DXGI_FORMAT::DXGI_FORMAT_R24G8_TYPELESS:
    case DXGI_FORMAT::DXGI_FORMAT_D24_UNORM_S8_UINT:
    case DXGI_FORMAT::DXGI_FORMAT_R24_UNORM_X8_TYPELESS:
    case DXGI_FORMAT::DXGI_FORMAT_X24_TYPELESS_G8_UINT:
    case DXGI_FORMAT::DXGI_FORMAT_R9G9B9E5_SHAREDEXP:
    case DXGI_FORMAT::DXGI_FORMAT_R8G8_B8G8_UNORM:
    case DXGI_FORMAT::DXGI_FORMAT_G8R8_G8B8_UNORM:
    case DXGI_FORMAT::DXGI_FORMAT_B8G8R8A8_UNORM:
    case DXGI_FORMAT::DXGI_FORMAT_B8G8R8X8_UNORM:
    case DXGI_FORMAT::DXGI_FORMAT_R10G10B10_XR_BIAS_A2_UNORM:
    case DXGI_FORMAT::DXGI_FORMAT_B8G8R8A8_TYPELESS:
    case DXGI_FORMAT::DXGI_FORMAT_B8G8R8A8_UNORM_SRGB:
    case DXGI_FORMAT::DXGI_FORMAT_B8G8R8X8_TYPELESS:
    case DXGI_FORMAT::DXGI_FORMAT_B8G8R8X8_UNORM_SRGB:
        return 32;

    case DXGI_FORMAT::DXGI_FORMAT_R8G8_TYPELESS:
    case DXGI_FORMAT::DXGI_FORMAT_R8G8_UNORM:
    case DXGI_FORMAT::DXGI_FORMAT_R8G8_UINT:
    case DXGI_FORMAT::DXGI_FORMAT_R8G8_SNORM:
    case DXGI_FORMAT::DXGI_FORMAT_R8G8_SINT:
    case DXGI_FORMAT::DXGI_FORMAT_R16_TYPELESS:
    case DXGI_FORMAT::DXGI_FORMAT_R16_FLOAT:
    case DXGI_FORMAT::DXGI_FORMAT_D16_UNORM:
    case DXGI_FORMAT::DXGI_FORMAT_R16_UNORM:
    case DXGI_FORMAT::DXGI_FORMAT_R16_UINT:
    case DXGI_FORMAT::DXGI_FORMAT_R16_SNORM:
    case DXGI_FORMAT::DXGI_FORMAT_R16_SINT:
    case DXGI_FORMAT::DXGI_FORMAT_B5G6R5_UNORM:
    case DXGI_FORMAT::DXGI_FORMAT_B5G5R5A1_UNORM:
    case DXGI_FORMAT::DXGI_FORMAT_B4G4R4A4_UNORM:
        return 16;

    case DXGI_FORMAT::DXGI_FORMAT_R8_TYPELESS:
    case DXGI_FORMAT::DXGI_FORMAT_R8_UNORM:
    case DXGI_FORMAT::DXGI_FORMAT_R8_UINT:
    case DXGI_FORMAT::DXGI_FORMAT_R8_SNORM:
    case DXGI_FORMAT::DXGI_FORMAT_R8_SINT:
    case DXGI_FORMAT::DXGI_FORMAT_A8_UNORM:
        return 8;

    case DXGI_FORMAT::DXGI_FORMAT_R1_UNORM:
        return 1;

    case DXGI_FORMAT::DXGI_FORMAT_BC1_TYPELESS:
    case DXGI_FORMAT::DXGI_FORMAT_BC1_UNORM:
    case DXGI_FORMAT::DXGI_FORMAT_BC1_UNORM_SRGB:
    case DXGI_FORMAT::DXGI_FORMAT_BC4_TYPELESS:
    case DXGI_FORMAT::DXGI_FORMAT_BC4_UNORM:
    case DXGI_FORMAT::DXGI_FORMAT_BC4_SNORM:
        return 4;

    case DXGI_FORMAT::DXGI_FORMAT_BC2_TYPELESS:
    case DXGI_FORMAT::DXGI_FORMAT_BC2_UNORM:
    case DXGI_FORMAT::DXGI_FORMAT_BC2_UNORM_SRGB:
    case DXGI_FORMAT::DXGI_FORMAT_BC3_TYPELESS:
    case DXGI_FORMAT::DXGI_FORMAT_BC3_UNORM:
    case DXGI_FORMAT::DXGI_FORMAT_BC3_UNORM_SRGB:
    case DXGI_FORMAT::DXGI_FORMAT_BC5_TYPELESS:
    case DXGI_FORMAT::DXGI_FORMAT_BC5_UNORM:
    case DXGI_FORMAT::DXGI_FORMAT_BC5_SNORM:
    case DXGI_FORMAT::DXGI_FORMAT_BC6H_TYPELESS:
    case DXGI_FORMAT::DXGI_FORMAT_BC6H_UF16:
    case DXGI_FORMAT::DXGI_FORMAT_BC6H_SF16:
    case DXGI_FORMAT::DXGI_FORMAT_BC7_TYPELESS:
    case DXGI_FORMAT::DXGI_FORMAT_BC7_UNORM:
    case DXGI_FORMAT::DXGI_FORMAT_BC7_UNORM_SRGB:
        return 8;

    default:
        return 0;
    }
}

static DXGI_FORMAT get_DXGI_format_internal(const DDS_PIXELFORMAT& ddpf)
{
    if (ddpf.dwFlags & DDPF_RGB)
    {
        // Note that sRGB formats are written using the "DX10" extended header.

        switch (ddpf.dwRGBBitCount)
        {
        case 32:
            if (ISBITMASK(0x000000ff, 0x0000ff00, 0x00ff0000, 0xff000000))
            {
                return DXGI_FORMAT::DXGI_FORMAT_R8G8B8A8_UNORM;
            }

            if (ISBITMASK(0x00ff0000, 0x0000ff00, 0x000000ff, 0xff000000))
            {
                return DXGI_FORMAT::DXGI_FORMAT_B8G8R8A8_UNORM;
            }

            if (ISBITMASK(0x00ff0000, 0x0000ff00, 0x000000ff, 0x00000000))
            {
                return DXGI_FORMAT::DXGI_FORMAT_B8G8R8X8_UNORM;
            }

            if (ISBITMASK(0x3ff00000, 0x000ffc00, 0x000003ff, 0xc0000000))
            {
                return DXGI_FORMAT::DXGI_FORMAT_R10G10B10A2_UNORM;
            }

            if (ISBITMASK(0x0000ffff, 0xffff0000, 0x00000000, 0x00000000))
            {
                return DXGI_FORMAT::DXGI_FORMAT_R16G16_UNORM;
            }

            if (ISBITMASK(0xffffffff, 0x00000000, 0x00000000, 0x00000000))
            {
                return DXGI_FORMAT::DXGI_FORMAT_R32_FLOAT;
            }
            break;

        case 24:
            // alex(todo): implement D3DFMT_R8G8B8
            break;

        case 16:
            if (ISBITMASK(0x7c00, 0x03e0, 0x001f, 0x8000))
            {
                return DXGI_FORMAT::DXGI_FORMAT_B5G5R5A1_UNORM;
            }
            if (ISBITMASK(0xf800, 0x07e0, 0x001f, 0x0000))
            {
                return DXGI_FORMAT::DXGI_FORMAT_B5G6R5_UNORM;
            }

            // alex(todo): implement  D3DFMT_X1R5G5B5.
            if (ISBITMASK(0x0f00, 0x00f0, 0x000f, 0xf000))
            {
                return DXGI_FORMAT::DXGI_FORMAT_B4G4R4A4_UNORM;
            }

            // alex(todo): implement D3DFMT_X4R4G4B4.

            // alex(todo): implement D3DFMT_A8R3G3B2, D3DFMT_R3G3B2, D3DFMT_P8, D3DFMT_A8P8.
            break;
        }
    }
    else if (ddpf.dwFlags & DDPF_LUMINANCE)
    {
        if (8 == ddpf.dwRGBBitCount)
        {
            if (ISBITMASK(0x000000ff, 0x00000000, 0x00000000, 0x00000000))
            {
                return DXGI_FORMAT::DXGI_FORMAT_R8_UNORM; // D3DX10/11 writes this out as DX10 extension
            }

            // alex(todo): implement D3DFMT_A4L4.
        }

        if (16 == ddpf.dwRGBBitCount)
        {
            if (ISBITMASK(0x0000ffff, 0x00000000, 0x00000000, 0x00000000))
            {
                return DXGI_FORMAT::DXGI_FORMAT_R16_UNORM; // D3DX10/11 writes this out as DX10 extension.
            }
            if (ISBITMASK(0x000000ff, 0x00000000, 0x00000000, 0x0000ff00))
            {
                return DXGI_FORMAT::DXGI_FORMAT_R8G8_UNORM; // D3DX10/11 writes this out as DX10 extension.
            }
        }
    }
    else if (ddpf.dwFlags & DDPF_ALPHA)
    {
        if (8 == ddpf.dwRGBBitCount)
        {
            return DXGI_FORMAT::DXGI_FORMAT_A8_UNORM;
        }
    }
    else if (ddpf.dwFlags & DDPF_FOURCC)
    {
        if (MAKEFOURCC('D', 'X', 'T', '1') == ddpf.dwFourCC)
        {
            return DXGI_FORMAT::DXGI_FORMAT_BC1_UNORM;
        }
        if (MAKEFOURCC('D', 'X', 'T', '3') == ddpf.dwFourCC)
        {
            return DXGI_FORMAT::DXGI_FORMAT_BC2_UNORM;
        }
        if (MAKEFOURCC('D', 'X', 'T', '5') == ddpf.dwFourCC)
        {
            return DXGI_FORMAT::DXGI_FORMAT_BC3_UNORM;
        }

        if (MAKEFOURCC('D', 'X', 'T', '2') == ddpf.dwFourCC)
        {
            return DXGI_FORMAT::DXGI_FORMAT_BC2_UNORM;
        }
        if (MAKEFOURCC('D', 'X', 'T', '4') == ddpf.dwFourCC)
        {
            return DXGI_FORMAT::DXGI_FORMAT_BC3_UNORM;
        }

        if (MAKEFOURCC('A', 'T', 'I', '1') == ddpf.dwFourCC)
        {
            return DXGI_FORMAT::DXGI_FORMAT_BC4_UNORM;
        }
        if (MAKEFOURCC('B', 'C', '4', 'U') == ddpf.dwFourCC)
        {
            return DXGI_FORMAT::DXGI_FORMAT_BC4_UNORM;
        }
        if (MAKEFOURCC('B', 'C', '4', 'S') == ddpf.dwFourCC)
        {
            return DXGI_FORMAT::DXGI_FORMAT_BC4_SNORM;
        }

        if (MAKEFOURCC('A', 'T', 'I', '2') == ddpf.dwFourCC)
        {
            return DXGI_FORMAT::DXGI_FORMAT_BC5_UNORM;
        }
        if (MAKEFOURCC('B', 'C', '5', 'U') == ddpf.dwFourCC)
        {
            return DXGI_FORMAT::DXGI_FORMAT_BC5_UNORM;
        }
        if (MAKEFOURCC('B', 'C', '5', 'S') == ddpf.dwFourCC)
        {
            return DXGI_FORMAT::DXGI_FORMAT_BC5_SNORM;
        }

        // BC6H and BC7 are written using the "DX10" extended header
        if (MAKEFOURCC('R', 'G', 'B', 'G') == ddpf.dwFourCC)
        {
            return DXGI_FORMAT::DXGI_FORMAT_R8G8_B8G8_UNORM;
        }
        if (MAKEFOURCC('G', 'R', 'G', 'B') == ddpf.dwFourCC)
        {
            return DXGI_FORMAT::DXGI_FORMAT_G8R8_G8B8_UNORM;
        }

        switch (ddpf.dwFourCC)
        {
        case 36: // D3DFMT_A16B16G16R16
            return DXGI_FORMAT::DXGI_FORMAT_R16G16B16A16_UNORM;

        case 110: // D3DFMT_Q16W16V16U16
            return DXGI_FORMAT::DXGI_FORMAT_R16G16B16A16_SNORM;

        case 111: // D3DFMT_R16F
            return DXGI_FORMAT::DXGI_FORMAT_R16_FLOAT;

        case 112: // D3DFMT_G16R16F
            return DXGI_FORMAT::DXGI_FORMAT_R16G16_FLOAT;

        case 113: // D3DFMT_A16B16G16R16F
            return DXGI_FORMAT::DXGI_FORMAT_R16G16B16A16_FLOAT;

        case 114: // D3DFMT_R32F
            return DXGI_FORMAT::DXGI_FORMAT_R32_FLOAT;

        case 115: // D3DFMT_G32R32F
            return DXGI_FORMAT::DXGI_FORMAT_R32G32_FLOAT;

        case 116: // D3DFMT_A32B32G32R32F
            return DXGI_FORMAT::DXGI_FORMAT_R32G32B32A32_FLOAT;
        }
    }

    return DXGI_FORMAT::DXGI_FORMAT_UNKNOWN;
}

struct format_info
{
    DXGI_FORMAT m_format{ DXGI_FORMAT::DXGI_FORMAT_UNKNOWN };
    size_t m_array_size{ 1 };
    size_t m_height{ 0 };
    size_t m_width{ 0 };
    size_t m_depth{ 0 };
    bool m_is_cubemap{ false };
    D3D10_RESOURCE_DIMENSION m_resource_dimmension{ D3D10_RESOURCE_DIMENSION_UNKNOWN };
};

format_info get_DXGI_format(const DDSFile& dds)
{
    format_info out{};
    out.m_height = dds.dwHeight;
    out.m_width = dds.dwWidth;
    out.m_depth = dds.dwDepth;

    if (dds.ddsHeaderDx10)
    {
        out.m_array_size = dds.ddsHeaderDx10->arraySize;
        if (out.m_array_size == 0)
            return out;

        if (bits_per_pixel(dds.ddsHeaderDx10->dxgiFormat) == 0)
            return out;

        out.m_format = dds.ddsHeaderDx10->dxgiFormat;

        switch (dds.ddsHeaderDx10->resourceDimension)
        {
        case D3D10_RESOURCE_DIMENSION_TEXTURE1D:
            if ((dds.dwFlags & DDSD_HEIGHT) && out.m_height != 1)
                return out;
            out.m_height = out.m_depth = 1;
            break;

        case D3D10_RESOURCE_DIMENSION_TEXTURE2D:
            if (dds.ddsHeaderDx10->miscFlag & D3D11_RESOURCE_MISC_TEXTURECUBE)
            {
                out.m_array_size *= 6;
                out.m_is_cubemap = true;
            }
            out.m_depth = 1;
            break;

        case D3D10_RESOURCE_DIMENSION_TEXTURE3D:
            if (!(dds.dwFlags & DDSD_DEPTH))
                return out;

            if (out.m_array_size > 1)
                return out;
            break;

        default:
            return out;
        }

        out.m_resource_dimmension = dds.ddsHeaderDx10->resourceDimension;
        return out;
    }
    out.m_format = get_DXGI_format_internal(dds.ddspf);

    if (out.m_format == DXGI_FORMAT::DXGI_FORMAT_UNKNOWN)
        return out;

    if (dds.dwFlags & DDSD_DEPTH)
    {
        out.m_resource_dimmension = D3D10_RESOURCE_DIMENSION_TEXTURE3D;
    }
    else
    {
        if (dds.dwCaps2 & DDSCAPS2_CUBEMAP)
        {
            if ((dds.dwCaps2 & DDSCAPS2_CUBEMAP_ALLFACES) != DDSCAPS2_CUBEMAP_ALLFACES)
            {
                out.m_format = DXGI_FORMAT::DXGI_FORMAT_UNKNOWN;
                return out;
            }

            out.m_array_size = 6;
            out.m_is_cubemap = true;
        }

        out.m_depth = 1;
        out.m_resource_dimmension = D3D10_RESOURCE_DIMENSION_TEXTURE2D;
    }

    return out;
}

VkImageCreateInfo get_vulkan_image_create_info(DDSFile* dds)
{
    format_info format_info = get_DXGI_format(*dds);
    bool has_alpha = ((dds->dwFlags & DDPF_ALPHAPIXELS) == DDPF_ALPHAPIXELS);
    VkFormat format = get_vulkan_format(format_info.m_format, has_alpha);
    VkImageCreateInfo image_info = {};
    image_info.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;

    switch (format_info.m_resource_dimmension)
    {
    case D3D10_RESOURCE_DIMENSION_TEXTURE1D:
        image_info.imageType = VK_IMAGE_TYPE_3D;
        break;
    case D3D10_RESOURCE_DIMENSION_TEXTURE2D:
        image_info.imageType = VK_IMAGE_TYPE_2D;
        break;
    case D3D10_RESOURCE_DIMENSION_TEXTURE3D:
        image_info.imageType = VK_IMAGE_TYPE_3D;
        break;
    default:
        break;
    }

    image_info.format = format;
    image_info.extent.width = static_cast<uint32_t>(format_info.m_width);
    image_info.extent.height = static_cast<uint32_t>(format_info.m_height);
    image_info.extent.depth = static_cast<uint32_t>(format_info.m_depth);
    image_info.mipLevels = dds->dwMipMapCount == 0 ? 1 : dds->dwMipMapCount;
    image_info.arrayLayers = static_cast<uint32_t>(format_info.m_array_size);
    return image_info;
}

struct dds_file
{
    dds_file_data data{};
    bool is_valid = false;
};


void dds_free(dds_file_data* file, VkAllocationCallbacks* host_allocator) {
    if (host_allocator)
        host_allocator->pfnFree(host_allocator->pUserData, file->blBuffer);
    else
        delete[] file->blBuffer;
}

dds_file dds_load(unsigned char* data, size_t data_size, bool will_own_data, VkAllocationCallbacks* host_allocator)
{
    size_t used_data_size = 0;
    unsigned char filesig[4]{};

    unsigned int isDx10;

    if (data_size < 4)
        return {};
    memcpy(filesig, data, 4);
    data += 4;
    used_data_size += 4;

    if (memcmp(filesig, "DDS ", 4) != 0)
        return {};

    dds_file_data file{};

    if (data_size - used_data_size < 124)
        return {};

    memset(&file, 0, sizeof(DDSFile));
    memcpy(&file, data, 124);
    data += 124;
    used_data_size += 124;

    DDS_HEADER_DXT10 dds10_header = {};
    isDx10 = memcmp(&file.ddspf.dwFourCC, "DX10", 4) == 0 ? 1 : 0;
    if (isDx10) {

        if (data_size - used_data_size < sizeof(DDS_HEADER_DXT10))
            return {};

        memcpy(file.ddsHeaderDx10, data, sizeof(DDS_HEADER_DXT10));
        data += sizeof(DDS_HEADER_DXT10);
        used_data_size += sizeof(DDS_HEADER_DXT10);
    }

    file.dwFileSize = data_size;
    file.dwBufferSize = ((data_size - 124) - 4) - (isDx10 ? sizeof(DDS_HEADER_DXT10) : 0);

    if (will_own_data)
    {
        if (host_allocator)
            file.blBuffer = reinterpret_cast<unsigned char*>(host_allocator->pfnAllocation(host_allocator->pUserData, file.dwBufferSize, 1, VK_SYSTEM_ALLOCATION_SCOPE_OBJECT));
        else
            file.blBuffer = new unsigned char[file.dwBufferSize]();
    }
    else
        file.blBuffer = data;

    if ((file.blBuffer == 0) && will_own_data) {
        dds_free(&file, host_allocator);
        return {};
    }

    if ((data_size - used_data_size < file.dwBufferSize) && will_own_data)
    {
        dds_free(&file, host_allocator);
        return {};
    }
    if (will_own_data)
        memcpy(file.blBuffer, data, file.dwBufferSize);
    data += file.dwBufferSize;
    used_data_size += file.dwBufferSize;

    return { file, true };
}

void get_surface_info
(
    size_t width,
    size_t height,
    DDSFile& dds,
    size_t* out_num_bytes,
    size_t* out_row_bytes,
    size_t* out_num_rows
)
{
    format_info format_info = get_DXGI_format(dds);

    size_t num_bytes = 0;
    size_t row_bytes = 0;
    size_t num_rows = 0;

    bool bc = false;
    bool packed = false;
    size_t bcnum_bytes_per_block = 0;
    switch (format_info.m_format)
    {
    case DXGI_FORMAT_BC1_TYPELESS:
    case DXGI_FORMAT_BC1_UNORM:
    case DXGI_FORMAT_BC1_UNORM_SRGB:
    case DXGI_FORMAT_BC4_TYPELESS:
    case DXGI_FORMAT_BC4_UNORM:
    case DXGI_FORMAT_BC4_SNORM:
        bc = true;
        bcnum_bytes_per_block = 8;
        break;

    case DXGI_FORMAT_BC2_TYPELESS:
    case DXGI_FORMAT_BC2_UNORM:
    case DXGI_FORMAT_BC2_UNORM_SRGB:
    case DXGI_FORMAT_BC3_TYPELESS:
    case DXGI_FORMAT_BC3_UNORM:
    case DXGI_FORMAT_BC3_UNORM_SRGB:
    case DXGI_FORMAT_BC5_TYPELESS:
    case DXGI_FORMAT_BC5_UNORM:
    case DXGI_FORMAT_BC5_SNORM:
    case DXGI_FORMAT_BC6H_TYPELESS:
    case DXGI_FORMAT_BC6H_UF16:
    case DXGI_FORMAT_BC6H_SF16:
    case DXGI_FORMAT_BC7_TYPELESS:
    case DXGI_FORMAT_BC7_UNORM:
    case DXGI_FORMAT_BC7_UNORM_SRGB:
        bc = true;
        bcnum_bytes_per_block = 16;
        break;

    case DXGI_FORMAT_R8G8_B8G8_UNORM:
    case DXGI_FORMAT_G8R8_G8B8_UNORM:
        packed = true;
        break;
    }

    if (bc)
    {
        size_t numBlocksWide = 0;
        if (width > 0)
        {
            numBlocksWide = std::max<size_t>(1, (width + 3) / 4);
        }
        size_t numBlocksHigh = 0;
        if (height > 0)
        {
            numBlocksHigh = std::max<size_t>(1, (height + 3) / 4);
        }
        row_bytes = numBlocksWide * bcnum_bytes_per_block;
        num_rows = numBlocksHigh;
    }
    else if (packed)
    {
        row_bytes = ((width + 1) >> 1) * 4;
        num_rows = height;
    }
    else
    {
        size_t bpp = bits_per_pixel(format_info.m_format);
        row_bytes = (width * bpp + 7) / 8; // Round up to the nearest byte.
        num_rows = height;
    }

    num_bytes = row_bytes * num_rows;
    if (out_num_bytes)
    {
        *out_num_bytes = num_bytes;
    }
    if (out_row_bytes)
    {
        *out_row_bytes = row_bytes;
    }
    if (out_num_rows)
    {
        *out_num_rows = num_rows;
    }
}


acp_vulkan::dds_data acp_vulkan::dds_data_from_memory(void* data, size_t data_size, bool will_own_data, VkAllocationCallbacks* host_allocator)
{
    dds_file dds_file = dds_load(reinterpret_cast<unsigned char*>(data), data_size, will_own_data, host_allocator);
    if (!dds_file.is_valid)
    {
        if (will_own_data)
            dds_free(&dds_file.data, host_allocator);
        return {};
    }

    VkImageCreateInfo image_info = get_vulkan_image_create_info(&dds_file.data);
    image_info.usage = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
    image_info.samples = VK_SAMPLE_COUNT_1_BIT;

    if (image_info.format == VK_FORMAT_UNDEFINED)
    {
        if (will_own_data)
            dds_free(&dds_file.data, host_allocator);
        return {};
    }

    if (image_info.mipLevels > MAX_NUMBER_OF_MIPS)
    {
        if (will_own_data)
            dds_free(&dds_file.data, host_allocator);
        return {};
    }

    if ((image_info.imageType & VK_IMAGE_TYPE_1D) && (image_info.extent.width > 16384 || image_info.arrayLayers > 2048))
    {
        if (will_own_data)
            dds_free(&dds_file.data, host_allocator);
        return {};
    }

    if ((image_info.imageType & VK_IMAGE_TYPE_2D) && (image_info.extent.width > 16384 || image_info.extent.height > 16384 || image_info.arrayLayers > 2048))
    {
        if (will_own_data)
            dds_free(&dds_file.data, host_allocator);
        return {};
    }

    if ((image_info.imageType & VK_IMAGE_TYPE_3D) && (image_info.extent.width > 16384 || image_info.extent.height > 16384 || image_info.extent.depth > 16384 || image_info.arrayLayers > 1))
    {
        if (will_own_data)
            dds_free(&dds_file.data, host_allocator);
        return {};
    }

    size_t width = dds_file.data.dwWidth;
    size_t height = dds_file.data.dwHeight;

    size_t num_bytes = 0;
    size_t row_bytes = 0;
    size_t num_rows = 0;
    uint8_t* src_bits = dds_file.data.blBuffer;
    uint8_t* end_bits = dds_file.data.blBuffer + dds_file.data.dwBufferSize;

    acp_vulkan::dds_data out{};
    out.image_create_info = image_info;
    out.width = width;
    out.height = height;
    out.num_mips = 0;
    out.dss_buffer_data = dds_file.data.blBuffer;
    out.full_data = will_own_data ? dds_file.data.blBuffer : nullptr;

    for (size_t j = 0; j < image_info.arrayLayers; j++)
    {
        size_t w = image_info.extent.width;
        size_t h = image_info.extent.height;
        size_t d = image_info.extent.depth;
        for (size_t ii = 0; ii < image_info.mipLevels; ii++)
        {
            get_surface_info(w, h, dds_file.data, &num_bytes, &row_bytes, &num_rows);

            out.image_mip_data[ii].extents = { static_cast<uint32_t>(w), static_cast<uint32_t>(h), static_cast<uint32_t>(d) };
            out.image_mip_data[ii].data = src_bits;
            out.image_mip_data[ii].data_size = row_bytes * num_rows;
            out.num_mips++;

            if (src_bits + (num_bytes * d) > end_bits)
            {
                if (will_own_data)
                    dds_free(&dds_file.data, host_allocator);
                return {};
            }

            src_bits += num_bytes * d;

            w = w >> 1;
            h = h >> 1;
            d = d >> 1;
            if (w == 0)
            {
                w = 1;
            }
            if (h == 0)
            {
                h = 1;
            }
            if (d == 0)
            {
                d = 1;
            }
        }
    }
    
    return out;
}

acp_vulkan::dds_data acp_vulkan::dds_data_from_file(const char* path, VkAllocationCallbacks* host_allocator)
{
    FILE* dds_bytes = fopen(path, "rb");
    if (!dds_bytes)
        return {};

    fseek(dds_bytes, 0, SEEK_END);
    long dds_size = ftell(dds_bytes);
    fseek(dds_bytes, 0, SEEK_SET);

    unsigned char* dds_data = host_allocator ? 
        reinterpret_cast<unsigned char*>(host_allocator->pfnAllocation(host_allocator->pUserData, dds_size, 1, VK_SYSTEM_ALLOCATION_SCOPE_OBJECT)) 
        : new unsigned char[dds_size];

    if (!dds_data)
    {
        fclose(dds_bytes);
        return {};
    }
    
    size_t bytes_to_read = dds_size;
    size_t offset = 0;
    while (size_t bytes_read = fread(dds_data + offset, 1, bytes_to_read, dds_bytes))
    {
        offset += bytes_read;
        bytes_to_read -= bytes_read;
    }

    fclose(dds_bytes);

    if (dds_size < offset)
    {
        if (host_allocator)
            host_allocator->pfnFree(host_allocator->pUserData, dds_data);
        else
            delete[] dds_data;

        return {};
    }

    acp_vulkan::dds_data out = dds_data_from_memory(dds_data, dds_size, false, nullptr);
    out.full_data = dds_data; // The dds_data will own the memory and it will be removed on dds_data_free.
    return out;
}

void acp_vulkan::dds_data_free(dds_data* dds_data, VkAllocationCallbacks* host_allocator)
{
    if (host_allocator)
    {
        host_allocator->pfnFree(host_allocator->pUserData, dds_data);
        return;
    }
    if (dds_data->full_data)
        delete[] dds_data->full_data;
}

VkImageViewCreateInfo acp_vulkan::dds_data_create_view_info(const dds_data* dds_data, VkImage image)
{
    VkImageViewCreateInfo image_view_info = {};
    image_view_info.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    image_view_info.viewType = VK_IMAGE_VIEW_TYPE_2D;
    image_view_info.format = dds_data->image_create_info.format;
    image_view_info.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    image_view_info.subresourceRange.baseMipLevel = 0;
    image_view_info.subresourceRange.levelCount = dds_data->image_create_info.mipLevels;
    image_view_info.subresourceRange.baseArrayLayer = 0;
    image_view_info.subresourceRange.layerCount = dds_data->image_create_info.arrayLayers;
    image_view_info.image = image;
    return image_view_info;
}