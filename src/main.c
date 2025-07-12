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
#include <wayland-client-core.h>
#include <wayland-client.h>
#include <wp-viewporter.h>
#include <xdg-shell.h>

static struct wl_compositor *wayland_compositor;
static struct xdg_wm_base *wayland_xdg_wm_base;
static struct wl_shm *wayland_shm;
static struct wp_viewporter *wayland_wp_viewporter;

static void wayland_registry_global_listener(
    __attribute__((unused)) void *data, struct wl_registry *wayland_registry,
    uint32_t name, const char *interface, uint32_t version) {
  if (strcmp(interface, "wl_compositor") == 0) {
    wayland_compositor = wl_registry_bind(wayland_registry, name,
                                          &wl_compositor_interface, version);
  } else if (strcmp(interface, "xdg_wm_base") == 0) {
    wayland_xdg_wm_base = wl_registry_bind(wayland_registry, name,
                                           &xdg_wm_base_interface, version);
  } else if (strcmp(interface, "wl_shm") == 0) {
    wayland_shm =
        wl_registry_bind(wayland_registry, name, &wl_shm_interface, version);
  } else if (strcmp(interface, "wp_viewporter") == 0) {
    wayland_wp_viewporter = wl_registry_bind(wayland_registry, name,
                                             &wp_viewporter_interface, version);
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
static uint32_t window_width;
static uint32_t window_height;

static void
wayland_xdg_surface_configure_listener(__attribute__((unused)) void *data,
                                       struct xdg_surface *xdg_surface,
                                       uint32_t serial) {
  xdg_surface_ack_configure(xdg_surface, serial);
  should_recommit = true;
}

static void wayland_xdg_toplevel_configure_listener(
    __attribute__((unused)) void *data,
    __attribute__((unused)) struct xdg_toplevel *xdg_toplevel, int32_t width,
    int32_t height, __attribute__((unused)) struct wl_array *states) {
  if (width != 0 && height != 0) {
    window_width = width;
    window_height = height;
    should_resize = true;
  }
}

static void wayland_xdg_toplevel_close_listener(
    __attribute__((unused)) void *data,
    __attribute__((unused)) struct xdg_toplevel *xdg_toplevel) {
  exit(EXIT_SUCCESS);
}

static void wayland_xdg_toplevel_configure_bounds_handler(
    __attribute__((unused)) void *data,
    __attribute__((unused)) struct xdg_toplevel *xdg_toplevel,
    __attribute__((unused)) int32_t boundsWidth,
    __attribute__((unused)) int32_t boundsHeight) {}

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

  png_bytepp png_rows = png_get_rows(png, png_info);
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
  assert(wayland_wp_viewporter != NULL);

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

  size_t size = 4 * png_width * png_height;
  int fd = syscall(SYS_memfd_create, "wayland shm", 0);
  assert(fd != -1);

  ftruncate(fd, size);
  uint8_t *pixel_data = mmap(0, size, PROT_WRITE, MAP_SHARED, fd, 0);
  assert(pixel_data != NULL);
  for (uint32_t y = 0; y < png_height; y++) {
    memcpy(pixel_data + y * png_width * 4, png_rows[y], png_width * 4);
  }
  msync(pixel_data, size, MS_SYNC);
  munmap(pixel_data, size);

  struct wl_shm_pool *wayland_shm_pool =
      wl_shm_create_pool(wayland_shm, fd, size);
  assert(wayland_shm_pool != NULL);

  struct wl_buffer *wayland_buffer =
      wl_shm_pool_create_buffer(wayland_shm_pool, 0, png_width, png_height,
                                png_width * 4, WL_SHM_FORMAT_ARGB8888);
  assert(wayland_buffer != NULL);

  wl_surface_commit(wayland_surface);

  window_width = 16 * png_width;
  window_height = 16 * png_height;

  struct wp_viewport *wayland_wp_viewport =
      wp_viewporter_get_viewport(wayland_wp_viewporter, wayland_surface);
  assert(wayland_wp_viewport != NULL);
  wp_viewport_set_source(wayland_wp_viewport, 0, 0, png_width, png_height);
  wp_viewport_set_destination(wayland_wp_viewport, window_width, window_height);

  bool is_attached = false;
  for (;;) {
    if (should_resize) {
      wp_viewport_set_destination(wayland_wp_viewport, window_width,
                                  window_height);
      should_resize = false;
    }
    if (should_recommit) {
      if (!is_attached) {
        wl_surface_attach(wayland_surface, wayland_buffer, 0, 0);
        is_attached = true;
      }
      wl_surface_commit(wayland_surface);
      should_recommit = false;
    }
    wl_display_dispatch(wayland_display);
  }
}
