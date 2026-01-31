#ifndef WLRCTL_OUTPUT_H
#define WLRCTL_OUTPUT_H

enum output_action {
	OUTPUT_ACTION_LIST = 1,

	OUTPUT_ACTION_CONFIGURE,
	OUTPUT_ACTION_UNSPEC
};

enum output_cfg_action {
	OUTPUT_CFG_ACTION_SHOW = 1,
	OUTPUT_CFG_ACTION_SET_MODE,
	OUTPUT_CFG_ACTION_SET_CUSTOM_MODE,
	OUTPUT_CFG_ACTION_SET_POSITION,
	OUTPUT_CFG_ACTION_SET_TRANSFORM,
	OUTPUT_CFG_ACTION_SET_SCALE,

	OUTPUT_CFG_ACTION_UNSPEC
};

struct wlrctl_output_command {
	enum output_action action;
	char *ident;
	enum output_cfg_action cfg_action;
	struct wl_list heads;
	struct wlrctl *state;
};

struct head_data {
	char name[24];
	char model[16];
	char make[56];
	char serial[16];
	char *description;
	int32_t x, y, width, height;
	struct wl_list modes;
	bool enabled;
	struct mode_data *current_mode;
	enum wl_output_transform transform;
	double scale;
	struct wl_list link;
	struct wlrctl_output_command *cmd;
};

struct mode_data {
	int32_t width, height;
	int32_t refresh;
	bool preferred;
	struct head_data *head;
	struct wl_list link; //head_data::modes
	struct zwlr_output_mode_v1 *mode;
};

void prepare_output(struct wlrctl *state, int argc, char **argv);
void run_output(struct wlrctl *state);
void stop_output(struct wlrctl *state);
void destroy_output(struct wlrctl *state);

#endif
