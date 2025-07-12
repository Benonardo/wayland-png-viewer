#include <assert.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <syscall.h>
#include <unistd.h>

#include <png.h>
#include <wayland-client.h>
#include <xdg-shell.h>
#include <zxdg-decoration.h>

static struct wl_compositor *wayland_compositor;
static struct xdg_wm_base *wayland_xdg_wm_base;
static struct zxdg_decoration_manager_v1 *wayland_zxdg_decoration_manager_v1;
static struct wl_shm *wayland_shm;

static void wayland_registry_global_listener(
    __attribute__((unused)) void *data, struct wl_registry *wayland_registry,
    uint32_t name, const char *interface, uint32_t version) {
  if (strcmp(interface, "wl_compositor") == 0) {
    wayland_compositor = wl_registry_bind(wayland_registry, name,
                                          &wl_compositor_interface, version);
  } else if (strcmp(interface, "xdg_wm_base") == 0) {
    wayland_xdg_wm_base = wl_registry_bind(wayland_registry, name,
                                           &xdg_wm_base_interface, version);
  } else if (strcmp(interface, "zxdg_decoration_manager_v1") == 0) {
    wayland_zxdg_decoration_manager_v1 = wl_registry_bind(
        wayland_registry, name, &zxdg_decoration_manager_v1_interface, version);
  } else if (strcmp(interface, "wl_shm") == 0) {
    wayland_shm =
        wl_registry_bind(wayland_registry, name, &wl_shm_interface, version);
  }
}

static void
wayland_xdg_wm_base_ping_listener(__attribute__((unused)) void *data,
                                  struct xdg_wm_base *xdg_wm_base,
                                  uint32_t serial) {
  xdg_wm_base_pong(xdg_wm_base, serial);
}

static bool should_resize = true;
static bool should_recommit = false;
static bool size_changed = false;

static int32_t window_width;
static int32_t window_height;
static int32_t bounds_width = INT32_MAX;
static int32_t bounds_height = INT32_MAX;

static void
wayland_xdg_surface_configure_listener(__attribute__((unused)) void *data,
                                       struct xdg_surface *xdg_surface,
                                       uint32_t serial) {
  xdg_surface_ack_configure(xdg_surface, serial);
  if (size_changed) {
    should_resize = true;
    size_changed = false;
  }
  should_recommit = true;
}

static void wayland_xdg_toplevel_configure_listener(
    __attribute__((unused)) void *data,
    __attribute__((unused)) struct xdg_toplevel *xdg_toplevel, int32_t width,
    int32_t height, __attribute__((unused)) struct wl_array *states) {
  if (width != 0) {
    window_width = width;
    if (window_width > bounds_width) {
      window_width = bounds_width;
    }
    size_changed = true;
  }
  if (height != 0) {
    window_height = height;
    if (window_height > bounds_height) {
      window_height = bounds_height;
    }
    size_changed = true;
  }
}

static void wayland_xdg_toplevel_close_listener(
    __attribute__((unused)) void *data,
    __attribute__((unused)) struct xdg_toplevel *xdg_toplevel) {
  exit(0);
}

static void wayland_xdg_toplevel_configure_bounds_handler(
    __attribute__((unused)) void *data,
    __attribute__((unused)) struct xdg_toplevel *xdg_toplevel, int32_t width,
    int32_t height) {
  puts("bounds");
  if (width != 0) {
    bounds_width = width;
    if (window_width > bounds_width) {
      window_width = bounds_width;
      size_changed = true;
    }
  }
  if (height != 0) {
    bounds_height = height;
    if (window_height > bounds_height) {
      window_height = bounds_height;
      size_changed = true;
    }
  }
}

static void wayland_xdg_toplevel_wm_capabilities_handler(
    __attribute__((unused)) void *data,
    __attribute__((unused)) struct xdg_toplevel *xdg_toplevel,
    __attribute__((unused)) struct wl_array *capabilities) {}

int main(int argc, char **argv) {
  assert(argc == 2);
  FILE *file = fopen(argv[1], "r");
  assert(file != NULL);

  png_structp png =
      png_create_read_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
  assert(png != NULL);
  png_infop png_info = png_create_info_struct(png);
  assert(png_info != NULL);
  png_init_io(png, file);
  png_set_filler(png, 0xFF, PNG_FILLER_AFTER);
  png_read_png(png, png_info,
               PNG_TRANSFORM_SCALE_16 | PNG_TRANSFORM_GRAY_TO_RGB |
                   PNG_TRANSFORM_EXPAND | PNG_TRANSFORM_BGR,
               NULL);

  uint32_t **png_rows = (uint32_t **)png_get_rows(png, png_info);
  assert(png_rows != NULL);
  png_uint_32 png_height = png_get_image_height(png, png_info);
  png_uint_32 png_width = png_get_image_width(png, png_info);

  struct wl_display *wayland_display = wl_display_connect(NULL);
  assert(wayland_display != NULL);

  struct wl_registry *wayland_registry =
      wl_display_get_registry(wayland_display);
  assert(wayland_registry != NULL);
  struct wl_registry_listener wayland_registry_listener = {
      wayland_registry_global_listener, NULL};
  wl_registry_add_listener(wayland_registry, &wayland_registry_listener, NULL);

  wl_display_dispatch(wayland_display);
  wl_display_roundtrip(wayland_display);
  assert(wayland_compositor != NULL);
  assert(wayland_xdg_wm_base != NULL);
  assert(wayland_shm != NULL);

  struct xdg_wm_base_listener wayland_xdg_wm_base_listener = {
      wayland_xdg_wm_base_ping_listener};
  xdg_wm_base_add_listener(wayland_xdg_wm_base, &wayland_xdg_wm_base_listener,
                           NULL);

  struct wl_surface *wayland_surface =
      wl_compositor_create_surface(wayland_compositor);
  assert(wayland_surface != NULL);

  struct xdg_surface *wayland_xdg_surface =
      xdg_wm_base_get_xdg_surface(wayland_xdg_wm_base, wayland_surface);
  assert(wayland_xdg_surface != NULL);
  struct xdg_surface_listener wayland_xdg_surface_listener = {
      wayland_xdg_surface_configure_listener};
  xdg_surface_add_listener(wayland_xdg_surface, &wayland_xdg_surface_listener,
                           NULL);

  struct xdg_toplevel *wayland_xdg_toplevel =
      xdg_surface_get_toplevel(wayland_xdg_surface);
  assert(wayland_xdg_toplevel != NULL);
  xdg_toplevel_set_title(wayland_xdg_toplevel, "PNG Viewer");
  struct xdg_toplevel_listener wayland_xdg_toplevel_listener = {
      wayland_xdg_toplevel_configure_listener,
      wayland_xdg_toplevel_close_listener,
      wayland_xdg_toplevel_configure_bounds_handler,
      wayland_xdg_toplevel_wm_capabilities_handler};
  xdg_toplevel_add_listener(wayland_xdg_toplevel,
                            &wayland_xdg_toplevel_listener, NULL);

  if (wayland_zxdg_decoration_manager_v1 != NULL) {
    struct zxdg_toplevel_decoration_v1 *wayland_zxdg_toplevel_decoration_v1 =
        zxdg_decoration_manager_v1_get_toplevel_decoration(
            wayland_zxdg_decoration_manager_v1, wayland_xdg_toplevel);
    assert(wayland_zxdg_toplevel_decoration_v1 != NULL);
    zxdg_toplevel_decoration_v1_set_mode(
        wayland_zxdg_toplevel_decoration_v1,
        ZXDG_TOPLEVEL_DECORATION_V1_MODE_SERVER_SIDE);
  }
#ifdef DEBUG
  else {
    fwrite("Could not enable server-side decorations\n", 41, 1, stderr);
  }
#endif

  int fd = syscall(SYS_memfd_create, "pixel_data", 0);
  assert(fd != -1);

  struct wl_shm_pool *wayland_shm_pool = wl_shm_create_pool(wayland_shm, fd, 1);
  assert(wayland_shm_pool != NULL);

  wl_surface_commit(wayland_surface);

  window_width = png_width * 16;
  if (window_width > bounds_width) {
    window_width = bounds_width;
  }
  window_height = png_height * 16;
  if (window_height > bounds_height) {
    window_height = bounds_height;
  }
  size_t max_pool_size = 1;
  for (;;) {
    if (should_resize) {
      if ((uint32_t)window_width < png_width) {
        window_width = png_width;
      }
      if ((uint32_t)window_height < png_height) {
        window_height = png_height;
      }
      size_t size = 4 * window_width * window_height;
      if (size > max_pool_size) {
        wl_shm_pool_resize(wayland_shm_pool, size);
        ftruncate(fd, size);
        max_pool_size = size;
      }

      int32_t x_padding = 0;
      int32_t y_padding = 0;
      int32_t scale = 1;
      if ((float)window_width / window_height > (float)png_width / png_height) {
        puts("first");
        scale = window_height / png_height;
        x_padding = (window_width - png_width * scale) / 2;
      } else {
        puts("second");
        scale = window_width / png_width;
        y_padding = (window_height - png_height * scale) / 2;
      }
      printf("\t%f %f\n", (float)window_width / window_height,
             (float)png_width / png_height);
      printf("\tscale: %d\n", scale);
      printf("\tx %d y %d\n", x_padding, y_padding);

      uint32_t *pixel_data = mmap(0, size, PROT_WRITE, MAP_SHARED, fd, 0);
      assert(pixel_data != NULL);
      memset(pixel_data, 0, y_padding * window_width * 4);
      for (int32_t window_y = 0; window_y < window_height - y_padding * 2;
           window_y++) {
        memset(pixel_data + (y_padding + window_y) * window_width, 0,
               x_padding * 4);
        for (int32_t window_x = 0; window_x < window_width - x_padding * 2;
             window_x++) {
          uint32_t png_y = window_y / scale;
          uint32_t png_x = window_x / scale;

          uint32_t pixel_value;
          if (png_x >= png_width || png_y >= png_height) {
            pixel_value = 0;
          } else {
            pixel_value = png_rows[png_y][png_x];

            uint8_t alpha = pixel_value >> 24;
            if (alpha != 0xFF) {
              uint8_t red = ((pixel_value >> 16) & 0xFF) * alpha / 0xFF;
              uint8_t green = ((pixel_value >> 8) & 0xFF) * alpha / 0xFF;
              uint8_t blue = (pixel_value & 0xFF) * alpha / 0xFF;
              pixel_value = alpha << 24 | red << 16 | green << 8 | blue;
            }
          }
          pixel_data[(y_padding + window_y) * window_width + x_padding +
                     window_x] = pixel_value;
        }
        memset(pixel_data + (y_padding + window_y + 1) * window_width -
                   x_padding,
               0, x_padding * 4);
      }
      memset(pixel_data + (window_height - y_padding) * window_width, 0,
             y_padding * window_width * 4);
      msync(pixel_data, size, MS_SYNC);
      munmap(pixel_data, size);

      should_resize = false;
    }
    if (should_recommit) {
      struct wl_buffer *wayland_buffer = wl_shm_pool_create_buffer(
          wayland_shm_pool, 0, window_width, window_height, 4 * window_width,
          WL_SHM_FORMAT_XRGB8888);
      assert(wayland_buffer != NULL);

      wl_surface_attach(wayland_surface, wayland_buffer, 0, 0);
      wl_surface_commit(wayland_surface);

      should_recommit = false;
    }
    wl_display_dispatch(wayland_display);
  }
}
