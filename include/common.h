#ifndef WLRCTL_COMMON_H
#define WLRCTL_COMMON_H

#include <stdbool.h>

enum wlrctl_command {
	WLRCTL_COMMAND_UNSPEC = 0,
	WLRCTL_COMMAND_KEYBOARD,
	WLRCTL_COMMAND_POINTER,
	WLRCTL_COMMAND_TOPLEVEL,
	WLRCTL_COMMAND_OUTPUT,
};

struct wlrctl {
	// Globals
	struct wl_display *display;
	struct wl_registry *registry;
	struct wl_seat *seat;
	struct zwp_virtual_keyboard_manager_v1 *vkbd_mgr;
	struct zwlr_foreign_toplevel_manager_v1 *ftl_mgr;
	struct zwlr_virtual_pointer_manager_v1 *vp_mgr;
	struct zwlr_output_manager_v1 *output_mgr;

	// State
	bool running, failed;
	enum wlrctl_command cmd_type;
	void *cmd;
};

#endif
