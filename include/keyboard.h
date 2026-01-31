#ifndef WLRCTL_DEV_KEYBOARD_H
#define WLRCTL_DEV_KEYBOARD_H

enum keyboard_action {
	KEYBOARD_ACTION_UNSPEC = 0,
	KEYBOARD_ACTION_TYPE,
};

struct wlrctl_keyboard_command {
	enum keyboard_action action;
	char *text;
	int mods_depressed;

	struct zwp_virtual_keyboard_v1 *device;
	struct xkb_context *xkb_context;
	struct {
		uint32_t format;
		uint32_t size;
		int fd;
	} keymap;
	struct wlrctl *state;
};

void prepare_keyboard(struct wlrctl *state, int argc, char *argv[]);
void run_keyboard(struct wlrctl *state);
void destroy_keyboard(struct wlrctl *state);

#endif
