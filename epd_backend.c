#include <epd_backend.h>
#include <epd.h>
#include <stdlib.h>
#include <wlr/interfaces/wlr_input_device.h>
#include <wlr/interfaces/wlr_output.h>
#include <wlr/render/egl.h>
#include <wlr/render/gles2.h>
#include <wlr/util/log.h>

static const struct wlr_backend_impl epd_backend_impl = {
  .start = epd_backend_start,
  .destroy = epd_backend_destroy,
  .get_renderer = epd_backend_get_renderer,
};

static bool
epd_backend_start(
  struct wlr_backend *wlr_backend
)
{
  struct epd_backend *backend = (struct epd_backend *) wlr_backend;
  wlr_log(WLR_INFO, "Starting epd backend");

  struct wlr_headless_output *output;
  wl_list_for_each(output, &backend->outputs, link) {
    wl_event_source_timer_update(output->frame_timer, output->frame_delay);
    wlr_output_update_enabled(&output->wlr_output, true);
    wlr_signal_emit_safe(&backend->backend.events.new_output,
                         &output->wlr_output);
  }

  backend->started = true;
  return true;
}
