#include <GLES2/gl2ext.h>

#ifndef DRM_WLR_FUNCS
#define DRM_WLR_FUNCS

struct wlr_pixel_format_info {
    uint32_t drm_format;

    /* Equivalent of the format if it has an alpha channel,
	 * DRM_FORMAT_INVALID (0) if NA
	 */
    uint32_t opaque_substitute;

    /* Bits per pixels */
    uint32_t bpp;

    /* True if the format has an alpha channel */
    bool has_alpha;
};

static const struct wlr_pixel_format_info pixel_format_info[] = {
    {
        .drm_format        = DRM_FORMAT_XRGB8888,
        .opaque_substitute = DRM_FORMAT_INVALID,
        .bpp               = 32,
        .has_alpha         = false,
    },
    {
        .drm_format        = DRM_FORMAT_ARGB8888,
        .opaque_substitute = DRM_FORMAT_XRGB8888,
        .bpp               = 32,
        .has_alpha         = true,
    },
    {
        .drm_format        = DRM_FORMAT_XBGR8888,
        .opaque_substitute = DRM_FORMAT_INVALID,
        .bpp               = 32,
        .has_alpha         = false,
    },
    {
        .drm_format        = DRM_FORMAT_ABGR8888,
        .opaque_substitute = DRM_FORMAT_XBGR8888,
        .bpp               = 32,
        .has_alpha         = true,
    },
    {
        .drm_format        = DRM_FORMAT_RGBX8888,
        .opaque_substitute = DRM_FORMAT_INVALID,
        .bpp               = 32,
        .has_alpha         = false,
    },
    {
        .drm_format        = DRM_FORMAT_RGBA8888,
        .opaque_substitute = DRM_FORMAT_RGBX8888,
        .bpp               = 32,
        .has_alpha         = true,
    },
    {
        .drm_format        = DRM_FORMAT_BGRX8888,
        .opaque_substitute = DRM_FORMAT_INVALID,
        .bpp               = 32,
        .has_alpha         = false,
    },
    {
        .drm_format        = DRM_FORMAT_BGRA8888,
        .opaque_substitute = DRM_FORMAT_BGRX8888,
        .bpp               = 32,
        .has_alpha         = true,
    },
    {
        .drm_format        = DRM_FORMAT_BGR888,
        .opaque_substitute = DRM_FORMAT_INVALID,
        .bpp               = 24,
        .has_alpha         = false,
    },
    {
        .drm_format        = DRM_FORMAT_RGBX4444,
        .opaque_substitute = DRM_FORMAT_INVALID,
        .bpp               = 16,
        .has_alpha         = false,
    },
    {
        .drm_format        = DRM_FORMAT_RGBA4444,
        .opaque_substitute = DRM_FORMAT_RGBX4444,
        .bpp               = 16,
        .has_alpha         = true,
    },
    {
        .drm_format        = DRM_FORMAT_RGBX5551,
        .opaque_substitute = DRM_FORMAT_INVALID,
        .bpp               = 16,
        .has_alpha         = false,
    },
    {
        .drm_format        = DRM_FORMAT_RGBA5551,
        .opaque_substitute = DRM_FORMAT_RGBX5551,
        .bpp               = 16,
        .has_alpha         = true,
    },
    {
        .drm_format        = DRM_FORMAT_RGB565,
        .opaque_substitute = DRM_FORMAT_INVALID,
        .bpp               = 16,
        .has_alpha         = false,
    },
    {
        .drm_format        = DRM_FORMAT_BGR565,
        .opaque_substitute = DRM_FORMAT_INVALID,
        .bpp               = 16,
        .has_alpha         = false,
    },
    {
        .drm_format        = DRM_FORMAT_XRGB2101010,
        .opaque_substitute = DRM_FORMAT_INVALID,
        .bpp               = 32,
        .has_alpha         = false,
    },
    {
        .drm_format        = DRM_FORMAT_ARGB2101010,
        .opaque_substitute = DRM_FORMAT_XRGB2101010,
        .bpp               = 32,
        .has_alpha         = true,
    },
    {
        .drm_format        = DRM_FORMAT_XBGR2101010,
        .opaque_substitute = DRM_FORMAT_INVALID,
        .bpp               = 32,
        .has_alpha         = false,
    },
    {
        .drm_format        = DRM_FORMAT_ABGR2101010,
        .opaque_substitute = DRM_FORMAT_XBGR2101010,
        .bpp               = 32,
        .has_alpha         = true,
    },
    {
        .drm_format        = DRM_FORMAT_XBGR16161616F,
        .opaque_substitute = DRM_FORMAT_INVALID,
        .bpp               = 64,
        .has_alpha         = false,
    },
    {
        .drm_format        = DRM_FORMAT_ABGR16161616F,
        .opaque_substitute = DRM_FORMAT_XBGR16161616F,
        .bpp               = 64,
        .has_alpha         = true,
    },
    {
        .drm_format        = DRM_FORMAT_XBGR16161616,
        .opaque_substitute = DRM_FORMAT_INVALID,
        .bpp               = 64,
        .has_alpha         = false,
    },
    {
        .drm_format        = DRM_FORMAT_ABGR16161616,
        .opaque_substitute = DRM_FORMAT_XBGR16161616,
        .bpp               = 64,
        .has_alpha         = true,
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

struct wlr_gles2_pixel_format {
    uint32_t drm_format;
    // optional field, if empty then internalformat = format
    GLint gl_internalformat;
    GLint gl_format, gl_type;
    bool  has_alpha;
};

static const struct wlr_gles2_pixel_format formats[] = {
    {
        .drm_format = DRM_FORMAT_ARGB8888,
        .gl_format  = GL_BGRA_EXT,
        .gl_type    = GL_UNSIGNED_BYTE,
        .has_alpha  = true,
    },
    {
        .drm_format = DRM_FORMAT_XRGB8888,
        .gl_format  = GL_BGRA_EXT,
        .gl_type    = GL_UNSIGNED_BYTE,
        .has_alpha  = false,
    },
    {
        .drm_format = DRM_FORMAT_XBGR8888,
        .gl_format  = GL_RGBA,
        .gl_type    = GL_UNSIGNED_BYTE,
        .has_alpha  = false,
    },
    {
        .drm_format = DRM_FORMAT_ABGR8888,
        .gl_format  = GL_RGBA,
        .gl_type    = GL_UNSIGNED_BYTE,
        .has_alpha  = true,
    },
    {
        .drm_format = DRM_FORMAT_BGR888,
        .gl_format  = GL_RGB,
        .gl_type    = GL_UNSIGNED_BYTE,
        .has_alpha  = false,
    },
#if WLR_LITTLE_ENDIAN
    {
        .drm_format = DRM_FORMAT_RGBX4444,
        .gl_format  = GL_RGBA,
        .gl_type    = GL_UNSIGNED_SHORT_4_4_4_4,
        .has_alpha  = false,
    },
    {
        .drm_format = DRM_FORMAT_RGBA4444,
        .gl_format  = GL_RGBA,
        .gl_type    = GL_UNSIGNED_SHORT_4_4_4_4,
        .has_alpha  = true,
    },
    {
        .drm_format = DRM_FORMAT_RGBX5551,
        .gl_format  = GL_RGBA,
        .gl_type    = GL_UNSIGNED_SHORT_5_5_5_1,
        .has_alpha  = false,
    },
    {
        .drm_format = DRM_FORMAT_RGBA5551,
        .gl_format  = GL_RGBA,
        .gl_type    = GL_UNSIGNED_SHORT_5_5_5_1,
        .has_alpha  = true,
    },
    {
        .drm_format = DRM_FORMAT_RGB565,
        .gl_format  = GL_RGB,
        .gl_type    = GL_UNSIGNED_SHORT_5_6_5,
        .has_alpha  = false,
    },
    {
        .drm_format = DRM_FORMAT_XBGR2101010,
        .gl_format  = GL_RGBA,
        .gl_type    = GL_UNSIGNED_INT_2_10_10_10_REV_EXT,
        .has_alpha  = false,
    },
    {
        .drm_format = DRM_FORMAT_ABGR2101010,
        .gl_format  = GL_RGBA,
        .gl_type    = GL_UNSIGNED_INT_2_10_10_10_REV_EXT,
        .has_alpha  = true,
    },
    {
        .drm_format = DRM_FORMAT_XBGR16161616F,
        .gl_format  = GL_RGBA,
        .gl_type    = GL_HALF_FLOAT_OES,
        .has_alpha  = false,
    },
    {
        .drm_format = DRM_FORMAT_ABGR16161616F,
        .gl_format  = GL_RGBA,
        .gl_type    = GL_HALF_FLOAT_OES,
        .has_alpha  = true,
    },
    {
        .drm_format        = DRM_FORMAT_XBGR16161616,
        .gl_internalformat = GL_RGBA16_EXT,
        .gl_format         = GL_RGBA,
        .gl_type           = GL_UNSIGNED_SHORT,
        .has_alpha         = false,
    },
    {
        .drm_format        = DRM_FORMAT_ABGR16161616,
        .gl_internalformat = GL_RGBA16_EXT,
        .gl_format         = GL_RGBA,
        .gl_type           = GL_UNSIGNED_SHORT,
        .has_alpha         = true,
    },
#endif
};

static const struct wlr_gles2_pixel_format* get_gles2_format_from_drm(uint32_t fmt) {
    for (size_t i = 0; i < sizeof(formats) / sizeof(*formats); ++i) {
        if (formats[i].drm_format == fmt) {
            return &formats[i];
        }
    }
    return NULL;
}

#endif