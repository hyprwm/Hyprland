#include <GLES2/gl2ext.h>

#ifndef DRM_WLR_FUNCS
#define DRM_WLR_FUNCS

struct wlr_pixel_format_info {
    uint32_t drm_format;

    /* Equivalent of the format if it has an alpha channel,
	 * DRM_FORMAT_INVALID (0) if NA
	 */
    uint32_t opaque_substitute;

    /* Bytes per block (including padding) */
    uint32_t bytes_per_block;
    /* Size of a block in pixels (zero for 1×1) */
    uint32_t block_width, block_height;

    /* True if the format has an alpha channel */
    bool has_alpha;
};

static const struct wlr_pixel_format_info pixel_format_info[] = {
    {
        .drm_format      = DRM_FORMAT_XRGB8888,
        .bytes_per_block = 4,
    },
    {
        .drm_format        = DRM_FORMAT_ARGB8888,
        .opaque_substitute = DRM_FORMAT_XRGB8888,
        .bytes_per_block   = 4,
        .has_alpha         = true,
    },
    {
        .drm_format      = DRM_FORMAT_XBGR8888,
        .bytes_per_block = 4,
    },
    {
        .drm_format        = DRM_FORMAT_ABGR8888,
        .opaque_substitute = DRM_FORMAT_XBGR8888,
        .bytes_per_block   = 4,
        .has_alpha         = true,
    },
    {
        .drm_format      = DRM_FORMAT_RGBX8888,
        .bytes_per_block = 4,
    },
    {
        .drm_format        = DRM_FORMAT_RGBA8888,
        .opaque_substitute = DRM_FORMAT_RGBX8888,
        .bytes_per_block   = 4,
        .has_alpha         = true,
    },
    {
        .drm_format      = DRM_FORMAT_BGRX8888,
        .bytes_per_block = 4,
    },
    {
        .drm_format        = DRM_FORMAT_BGRA8888,
        .opaque_substitute = DRM_FORMAT_BGRX8888,
        .bytes_per_block   = 4,
        .has_alpha         = true,
    },
    {
        .drm_format      = DRM_FORMAT_R8,
        .bytes_per_block = 1,
    },
    {
        .drm_format      = DRM_FORMAT_GR88,
        .bytes_per_block = 2,
    },
    {
        .drm_format      = DRM_FORMAT_RGB888,
        .bytes_per_block = 3,
    },
    {
        .drm_format      = DRM_FORMAT_BGR888,
        .bytes_per_block = 3,
    },
    {
        .drm_format      = DRM_FORMAT_RGBX4444,
        .bytes_per_block = 2,
    },
    {
        .drm_format        = DRM_FORMAT_RGBA4444,
        .opaque_substitute = DRM_FORMAT_RGBX4444,
        .bytes_per_block   = 2,
        .has_alpha         = true,
    },
    {
        .drm_format      = DRM_FORMAT_BGRX4444,
        .bytes_per_block = 2,
    },
    {
        .drm_format        = DRM_FORMAT_BGRA4444,
        .opaque_substitute = DRM_FORMAT_BGRX4444,
        .bytes_per_block   = 2,
        .has_alpha         = true,
    },
    {
        .drm_format      = DRM_FORMAT_RGBX5551,
        .bytes_per_block = 2,
    },
    {
        .drm_format        = DRM_FORMAT_RGBA5551,
        .opaque_substitute = DRM_FORMAT_RGBX5551,
        .bytes_per_block   = 2,
        .has_alpha         = true,
    },
    {
        .drm_format      = DRM_FORMAT_BGRX5551,
        .bytes_per_block = 2,
    },
    {
        .drm_format        = DRM_FORMAT_BGRA5551,
        .opaque_substitute = DRM_FORMAT_BGRX5551,
        .bytes_per_block   = 2,
        .has_alpha         = true,
    },
    {
        .drm_format      = DRM_FORMAT_XRGB1555,
        .bytes_per_block = 2,
    },
    {
        .drm_format        = DRM_FORMAT_ARGB1555,
        .opaque_substitute = DRM_FORMAT_XRGB1555,
        .bytes_per_block   = 2,
        .has_alpha         = true,
    },
    {
        .drm_format      = DRM_FORMAT_RGB565,
        .bytes_per_block = 2,
    },
    {
        .drm_format      = DRM_FORMAT_BGR565,
        .bytes_per_block = 2,
    },
    {
        .drm_format      = DRM_FORMAT_XRGB2101010,
        .bytes_per_block = 4,
    },
    {
        .drm_format        = DRM_FORMAT_ARGB2101010,
        .opaque_substitute = DRM_FORMAT_XRGB2101010,
        .bytes_per_block   = 4,
        .has_alpha         = true,
    },
    {
        .drm_format      = DRM_FORMAT_XBGR2101010,
        .bytes_per_block = 4,
    },
    {
        .drm_format        = DRM_FORMAT_ABGR2101010,
        .opaque_substitute = DRM_FORMAT_XBGR2101010,
        .bytes_per_block   = 4,
        .has_alpha         = true,
    },
    {
        .drm_format      = DRM_FORMAT_XBGR16161616F,
        .bytes_per_block = 8,
    },
    {
        .drm_format        = DRM_FORMAT_ABGR16161616F,
        .opaque_substitute = DRM_FORMAT_XBGR16161616F,
        .bytes_per_block   = 8,
        .has_alpha         = true,
    },
    {
        .drm_format      = DRM_FORMAT_XBGR16161616,
        .bytes_per_block = 8,
    },
    {
        .drm_format        = DRM_FORMAT_ABGR16161616,
        .opaque_substitute = DRM_FORMAT_XBGR16161616,
        .bytes_per_block   = 8,
        .has_alpha         = true,
    },
    {
        .drm_format      = DRM_FORMAT_YVYU,
        .bytes_per_block = 4,
        .block_width     = 2,
        .block_height    = 1,
    },
    {
        .drm_format      = DRM_FORMAT_VYUY,
        .bytes_per_block = 4,
        .block_width     = 2,
        .block_height    = 1,
    },
};

static const size_t                        pixel_format_info_size = sizeof(pixel_format_info) / sizeof(pixel_format_info[0]);

static const struct wlr_pixel_format_info* drm_get_pixel_format_info(uint32_t fmt) {
    for (size_t i = 0; i < pixel_format_info_size; ++i) {
        if (pixel_format_info[i].drm_format == fmt) {
            return &pixel_format_info[i];
        }
    }

    return NULL;
}

/*static uint32_t convert_wl_shm_format_to_drm(enum wl_shm_format fmt) {
    switch (fmt) {
        case WL_SHM_FORMAT_XRGB8888: return DRM_FORMAT_XRGB8888;
        case WL_SHM_FORMAT_ARGB8888: return DRM_FORMAT_ARGB8888;
        default: return (uint32_t)fmt;
    }
}*/

static enum wl_shm_format convert_drm_format_to_wl_shm(uint32_t fmt) {
    switch (fmt) {
        case DRM_FORMAT_XRGB8888: return WL_SHM_FORMAT_XRGB8888;
        case DRM_FORMAT_ARGB8888: return WL_SHM_FORMAT_ARGB8888;
        default: return (enum wl_shm_format)fmt;
    }
}

static uint32_t pixel_format_info_pixels_per_block(const struct wlr_pixel_format_info* info) {
    uint32_t pixels = info->block_width * info->block_height;
    return pixels > 0 ? pixels : 1;
}

static int32_t div_round_up(int32_t dividend, int32_t divisor) {
    int32_t quotient = dividend / divisor;
    if (dividend % divisor != 0) {
        quotient++;
    }
    return quotient;
}

static int32_t pixel_format_info_min_stride(const wlr_pixel_format_info* fmt, int32_t width) {
    int32_t pixels_per_block = (int32_t)pixel_format_info_pixels_per_block(fmt);
    int32_t bytes_per_block  = (int32_t)fmt->bytes_per_block;
    if (width > INT32_MAX / bytes_per_block) {
        wlr_log(WLR_DEBUG, "Invalid width %d (overflow)", width);
        return 0;
    }
    return div_round_up(width * bytes_per_block, pixels_per_block);
}

#endif