#define _XOPEN_SOURCE 500
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <wayland-client.h>
#include "common.h"
#include "output.h"
#include "util.h"
#include "wlr-output-management-unstable-v1-client-protocol.h"

static enum output_action
parse_action(const char *action)
{
	static const struct token actions[] = {
		{"list", OUTPUT_ACTION_LIST},
		{NULL, OUTPUT_ACTION_CONFIGURE}
	};
	return matchtok(actions, action);
}

static enum output_cfg_action
parse_cfg_action(const char *action)
{
	static const struct token actions[] = {
		{"show",            OUTPUT_CFG_ACTION_SHOW},
		{"set-mode",        OUTPUT_CFG_ACTION_SET_MODE},
		{"set-custom-mode", OUTPUT_CFG_ACTION_SET_CUSTOM_MODE},
		{"set-position",    OUTPUT_CFG_ACTION_SET_POSITION},
		{"set-transform",   OUTPUT_CFG_ACTION_SET_TRANSFORM},
		{"set-scale",       OUTPUT_CFG_ACTION_SET_SCALE},
		{NULL, OUTPUT_CFG_ACTION_UNSPEC}
	};

	return matchtok(actions, action);
}

static struct mode_data *
mode_data_create(struct head_data *head_data) {
	struct mode_data *mode_data = calloc(1, sizeof (struct mode_data));
	assert(mode_data);
	mode_data->head = head_data;
	wl_list_insert(&head_data->modes, &mode_data->link);
	return mode_data;
}

static void
mode_data_destroy(struct mode_data *mode_data) {
	free(mode_data);
}

static void
zwlr_output_mode_v1_handle_size(
	void *data,
	struct zwlr_output_mode_v1 *mode,
	int32_t width, int32_t height
	)
{
	struct mode_data *mode_data = data;
	mode_data->width = width;
	mode_data->height = height;
}

static void
zwlr_output_mode_v1_handle_refresh(
	void *data,
	struct zwlr_output_mode_v1 *mode,
	int32_t refresh
	)
{
	struct mode_data *mode_data = data;
	mode_data->refresh = refresh;
}

static void
zwlr_output_mode_v1_handle_preferred(
	void *data,
	struct zwlr_output_mode_v1 *mode
	)
{
	struct mode_data *mode_data = data;
	mode_data->preferred = true;
}

static void
zwlr_output_mode_v1_handle_finished(
	void *data,
	struct zwlr_output_mode_v1 *mode
	)
{
	struct mode_data *mode_data = data;
	struct mode_data *other, *tmp;
	wl_list_for_each_safe(other, tmp, &mode_data->head->modes, link) {
		if (other == mode_data) {
			wl_list_remove(&other->link);
			mode_data_destroy(mode_data);
		}
	}
}

static struct zwlr_output_mode_v1_listener
zwlr_output_mode_v1_listener = {
	.size = zwlr_output_mode_v1_handle_size,
	.refresh = zwlr_output_mode_v1_handle_refresh,
	.preferred = zwlr_output_mode_v1_handle_preferred,
	.finished = zwlr_output_mode_v1_handle_finished,
};

static struct head_data *
head_data_create(struct wlrctl_output_command *cmd) {
	struct head_data *head_data = calloc(1, sizeof (struct head_data));
	assert(head_data);
	wl_list_init(&head_data->modes);
	wl_list_insert(&cmd->heads, &head_data->link);
	head_data->cmd = cmd;
	return head_data;
}

static void
head_data_destroy(struct head_data *head_data) {
	free(head_data->description);
	free(head_data);
}

static void
zwlr_output_head_v1_handle_name(
	void *data,
	struct zwlr_output_head_v1 *head,
	const char *name
	)
{
	struct head_data *head_data = data;
	strncpy(head_data->name, name, 24);
	head_data->name[24 - 1] = '\0';
}

static void
zwlr_output_head_v1_handle_make(
	void *data,
	struct zwlr_output_head_v1 *head,
	const char *make
	)
{
	struct head_data *head_data = data;
	strncpy(head_data->make, make, 56);
	head_data->make[56 - 1] = '\0';
}

static void
zwlr_output_head_v1_handle_model(
	void *data,
	struct zwlr_output_head_v1 *head,
	const char *model
	)
{
	struct head_data *head_data = data;
	strncpy(head_data->model, model, 16);
	head_data->model[16 - 1] = '\0';
}

static void
zwlr_output_head_v1_handle_serial_number(
	void *data,
	struct zwlr_output_head_v1 *head,
	const char *serial
	)
{
	struct head_data *head_data = data;
	strncpy(head_data->serial, serial, 16);
	head_data->serial[16 - 1] = '\0';
}

static void
wlrctl_output_head_v1_handle_physical_size(
	void *data,
	struct zwlr_output_head_v1 *head,
	int32_t width, int32_t height
	)
{
	struct head_data *head_data = data;
	head_data->width = width;
	head_data->height = height;
}

static void
wlrctl_output_head_v1_handle_enabled(
	void *data,
	struct zwlr_output_head_v1 *head,
	int32_t enabled
	)
{
	struct head_data *head_data = data;
	head_data->enabled = enabled;
}

static void
wlrctl_output_head_v1_handle_position(
	void *data,
	struct zwlr_output_head_v1 *head,
	int32_t x, int32_t y
	)
{
	struct head_data *head_data = data;
	head_data->x = x;
	head_data->y = y;
}

static void
wlrctl_output_head_v1_handle_transform(
	void *data,
	struct zwlr_output_head_v1 *head,
	int32_t transform
	)
{
	struct head_data *head_data = data;
	head_data->transform = transform;
}

static void
zwlr_output_head_v1_handle_description(
	void *data,
	struct zwlr_output_head_v1 *head,
	const char *description
	)
{
	struct head_data *head_data = data;
	head_data->description = strdup(description);
}

static void
zwlr_output_head_v1_handle_scale(
	void *data,
	struct zwlr_output_head_v1 *head,
	wl_fixed_t scale
	)
{
	struct head_data *head_data = data;
	head_data->scale = wl_fixed_to_double(scale);
}

static void
zwlr_output_head_v1_handle_finished(
	void *data,
	struct zwlr_output_head_v1 *head
	)
{
	struct head_data *head_data = data;
	struct head_data *other, *tmp;
	wl_list_for_each_safe(other, tmp, &head_data->cmd->heads, link) {
		if (other == head_data) {
			wl_list_remove(&other->link);
			head_data_destroy(other);
		}
	}
}

static void
zwlr_output_head_v1_handle_mode(
	void *data,
	struct zwlr_output_head_v1 *head,
	struct zwlr_output_mode_v1 *mode
	)
{
	struct head_data *head_data = data;
	struct mode_data *mode_data = mode_data_create(head_data);
	mode_data->mode = mode;
	zwlr_output_mode_v1_add_listener(
		mode,
		&zwlr_output_mode_v1_listener,
		mode_data
	);
}

static void
zwlr_output_head_v1_handle_current_mode(
	void *data,
	struct zwlr_output_head_v1 *head,
	struct zwlr_output_mode_v1 *mode
	)
{
	struct head_data *head_data = data;
	struct mode_data *mode_data;
	wl_list_for_each(mode_data, &head_data->modes, link) {
		if (mode_data->mode == mode) {
			head_data->current_mode = mode_data;
			return;
		}
	}
}

static struct zwlr_output_head_v1_listener
zwlr_output_head_v1_listener = {
	.name = zwlr_output_head_v1_handle_name,
	.description = zwlr_output_head_v1_handle_description,
	.physical_size = wlrctl_output_head_v1_handle_physical_size,
	.mode = zwlr_output_head_v1_handle_mode,
	.enabled = wlrctl_output_head_v1_handle_enabled,
	.current_mode = zwlr_output_head_v1_handle_current_mode,
	.position = wlrctl_output_head_v1_handle_position,
	.transform = wlrctl_output_head_v1_handle_transform,
	.scale = zwlr_output_head_v1_handle_scale,
	.finished = zwlr_output_head_v1_handle_finished,
	.make = zwlr_output_head_v1_handle_make,
	.model = zwlr_output_head_v1_handle_model,
	.serial_number = zwlr_output_head_v1_handle_serial_number,
};

static void zwlr_output_manager_v1_handle_head(
	void *data,
	struct zwlr_output_manager_v1 *manager,
	struct zwlr_output_head_v1 *head
	)
{
	struct wlrctl *state = data;
	struct wlrctl_output_command *cmd = state->cmd;
	struct head_data *head_data = head_data_create(cmd);
	zwlr_output_head_v1_add_listener(
		head,
		&zwlr_output_head_v1_listener,
		head_data
	);
}

static void
zwlr_output_manager_v1_handle_finished(
	void *data,
	struct zwlr_output_manager_v1 *manager
	)
{
	struct wlrctl *state = data;
	state->running = false;
	destroy_output(state);
}

static void
do_output_cfg_action(struct wlrctl_output_command *cmd,
	struct head_data *head_data
	)
{
	switch (cmd->cfg_action) {
	default:
		puts("Not implemented");
		break;
	case OUTPUT_CFG_ACTION_UNSPEC:
		// unreachable
		assert(false);
	}
}



static void
zwlr_output_manager_v1_handle_done(void *user_data,
	struct zwlr_output_manager_v1 *manager,
	uint32_t serial
	)
{
	struct wlrctl *state = user_data;
	struct wlrctl_output_command *cmd = state->cmd;

	struct head_data *data;
	switch (cmd->action) {
	case OUTPUT_ACTION_LIST:
		wl_list_for_each(data, &cmd->heads, link) {
			printf("%s \"%s %s\"", data->name, data->make, data->model);
			if (data->current_mode) {
				struct mode_data *mode = data->current_mode;
				printf(" (%dx%d %.3fHz)", mode->width, mode->height, mode->refresh / 1000.0);
			} else if (!data->enabled) {
				printf(" (disabled)");
			}
			printf("\n");
		}
		break;
	case OUTPUT_ACTION_CONFIGURE:;
		struct head_data *head_data = NULL;
		wl_list_for_each(data, &cmd->heads, link) {
			if (strcmp(data->name, cmd->ident) == 0) {
				head_data = data;
			}
		}
		if (head_data) {
			do_output_cfg_action(cmd, head_data);
		} else {
			die("No matching outputs\n");
		}
		break;
	case OUTPUT_ACTION_UNSPEC:
		// unreachable
		assert(false);
	}
}

static struct zwlr_output_manager_v1_listener
zwlr_output_manager_v1_listener = {
	.head = zwlr_output_manager_v1_handle_head,
	.done = zwlr_output_manager_v1_handle_done,
	.finished = zwlr_output_manager_v1_handle_finished,
};

void
prepare_output(struct wlrctl *state, int argc, char *argv[])
{
	struct wlrctl_output_command *cmd =
		calloc(1, sizeof (struct wlrctl_output_command));
	assert(cmd);

	wl_list_init(&cmd->heads);
	if (argc == 0) {
		die("Missing output action or identifier\n");
	}

	char *action = argv[0];
	cmd->action = parse_action(action);
	if (cmd->action == OUTPUT_ACTION_CONFIGURE) {
		if (argc < 2 ) {
			cmd->cfg_action = OUTPUT_CFG_ACTION_SHOW;
		} else {
			cmd->cfg_action = parse_cfg_action(argv[1]);
			if (cmd->cfg_action == OUTPUT_CFG_ACTION_UNSPEC) {
				die("Unknown output configurataion '%s'\n", argv[1]);
			}
		}
		cmd->ident = strdup(argv[0]);
	}

	state->cmd = cmd;
	cmd->state = state;
}

void
stop_output(struct wlrctl *state)
{
	zwlr_output_manager_v1_stop(state->output_mgr);
}

void
complete_output(void *data, struct wl_callback *callback, uint32_t serial)
{
	struct wlrctl *state = data;
	// struct wlrctl_output_command *cmd = state->cmd;
	wl_callback_destroy(callback);
	stop_output(state);
}

void
run_output(struct wlrctl *state)
{
	// struct wlrctl_output_command *cmd = state->cmd;
	zwlr_output_manager_v1_add_listener(
		state->output_mgr,
		&zwlr_output_manager_v1_listener,
		state
	);
	stop_output(state);
}

void
destroy_output(struct wlrctl *state)
{
	struct wlrctl_output_command *cmd = state->cmd;
	struct head_data *data, *tmp;
	wl_list_for_each_safe(data, tmp, &cmd->heads, link) {
		wl_list_remove(&data->link);
		head_data_destroy(data);
	}
	free(cmd->ident);
	free(cmd);
}
