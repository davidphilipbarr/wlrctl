#define _DEFAULT_SOURCE
#include <assert.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <wayland-client.h>
#include "common.h"
#include "toplevel.h"
#include "util.h"

#include "wlr-foreign-toplevel-management-unstable-v1-client-protocol.h"
#include "ext-foreign-toplevel-list-v1-client-protocol.h"

static void noop() {}

static enum toplevel_action
parse_action(const char *action)
{
	static const struct token actions[] = {
		{"activate",   TOPLEVEL_ACTION_ACTIVATE  },
		{"close",      TOPLEVEL_ACTION_CLOSE     },
		{"find",       TOPLEVEL_ACTION_FIND      },
		{"focus",      TOPLEVEL_ACTION_ACTIVATE  },
		{"fullscreen", TOPLEVEL_ACTION_FULLSCREEN},
		{"list",       TOPLEVEL_ACTION_LIST      },
		{"maximize",   TOPLEVEL_ACTION_MAXIMIZE  },
		{"minimize",   TOPLEVEL_ACTION_MINIMIZE  },
		{"restore",    TOPLEVEL_ACTION_UNMINIMIZE},
		{"unminimize", TOPLEVEL_ACTION_UNMINIMIZE},
		{"wait",       TOPLEVEL_ACTION_WAIT      },
		{"waitfor",    TOPLEVEL_ACTION_WAITFOR   },
		{NULL, TOPLEVEL_ACTION_UNSPEC}
	};

	return matchtok(actions, action);
}

static enum toplevel_attr
parse_state(const char *state, bool *enabled){
	static const struct token states[] = {
		{"maximized",  TOPLEVEL_ATTR_MAXIMIZED },
		{"minimized",  TOPLEVEL_ATTR_MINIMIZED },
		{"activated",  TOPLEVEL_ATTR_ACTIVATED },
		{"active",     TOPLEVEL_ATTR_ACTIVATED },
		{"focused",    TOPLEVEL_ATTR_ACTIVATED },
		{"fullscreen", TOPLEVEL_ATTR_FULLSCREEN},
		{NULL, TOPLEVEL_ATTR_UNSPEC}
	};
	if (strncmp(state, "-", 1) == 0) {
		*enabled = false;
		state += 1;
	} else if (
		strncmp(state, "in", 2) == 0 ||
		strncmp(state, "un", 2) == 0 ) {
		*enabled = false;
		state += 2;
	} else {
		*enabled = true;
	}
	return matchtok(states, state);
}

static void
append(struct wl_array *arr, char *str)
{
	char **p = wl_array_add(arr, sizeof (char *));
	if (!p) {
		die("Could not allocate pointer for matchspec\n");
	} else {
		*p = str;
	}
}

static bool
contains_value(struct wl_array *arr, int value) {
	if (!value) {
		return false;
	}

	int *cursor;
	wl_array_for_each(cursor, arr) {
		if (*cursor == value) {
			return true;
		}
	}
	return false;
}

static bool
contains_str(struct wl_array *arr, char *str)
{
	if (!str) {
		return false;
	}

	char **cursor;
	wl_array_for_each(cursor, arr) {
		if (strcmp(str, *cursor) == 0) {
			return true;
		}
	}
	return false;
}

static void
matchspec_init(struct toplevel_matchspec *matchspec)
{
	wl_array_init(&matchspec->app_ids);
	wl_array_init(&matchspec->titles);
	wl_array_init(&matchspec->ids);
}

static void
matchspec_release(struct toplevel_matchspec *matchspec)
{
	wl_array_release(&matchspec->app_ids);
	wl_array_release(&matchspec->titles);
	wl_array_release(&matchspec->ids);
}

static void
matchspec_add_match(struct toplevel_matchspec *matchspec, char *match)
{
	char *value = match;
	char *attr = strsep(&value, ":");
	if (!value) {
		matchspec->attrs |= TOPLEVEL_ATTR_APPID;
		append(&matchspec->app_ids, attr);
		return;
	}

	bool enabled = true;
	struct token attrs[] = {
		{"app-id", TOPLEVEL_ATTR_APPID         },
		{"app_id", TOPLEVEL_ATTR_APPID         },
		{"title",  TOPLEVEL_ATTR_TITLE         },
		{"id",     TOPLEVEL_ATTR_ID            },
		{"state",  parse_state(value, &enabled)},
		{NULL, TOPLEVEL_ATTR_UNSPEC}
	};
	enum toplevel_attr pattr = matchtok(attrs, attr);

	switch (pattr) {
	case TOPLEVEL_ATTR_APPID:
		matchspec->attrs |= pattr;
		append(&matchspec->app_ids, value);
		return;
	case TOPLEVEL_ATTR_TITLE:
		matchspec->attrs |= pattr;
		append(&matchspec->titles, value);
		return;
	case TOPLEVEL_ATTR_ID:
		matchspec->attrs |= pattr;
		append(&matchspec->ids, value);
		return;
	case TOPLEVEL_ATTR_MAXIMIZED:
		matchspec->attrs |= pattr;
		matchspec->maximized = enabled;
		break;
	case TOPLEVEL_ATTR_MINIMIZED:
		matchspec->attrs |= pattr;
		matchspec->minimized = enabled;
		break;
	case TOPLEVEL_ATTR_ACTIVATED:
		matchspec->attrs |= pattr;
		matchspec->activated = enabled;
		break;
	case TOPLEVEL_ATTR_FULLSCREEN:
		matchspec->attrs |= pattr;
		matchspec->fullscreen = enabled;
		break;
	case TOPLEVEL_ATTR_UNSPEC:
	default:
		die("Unknown attribute: '%s:%s'\n", attr, value);
	}

	return;
}

static bool
is_matched(struct toplevel_data *data)
{
	struct toplevel_matchspec *matchspec = &data->cmd->matchspec;
	if (matchspec->attrs & TOPLEVEL_ATTR_APPID) {
		if (!contains_str(&matchspec->app_ids, data->app_id)) {
			return false;
		}
	}
	if (matchspec->attrs & TOPLEVEL_ATTR_TITLE) {
		if (!contains_str(&matchspec->titles, data->title)) {
			return false;
		}
	}
	if (matchspec->attrs & TOPLEVEL_ATTR_ID) {
		if (!contains_str(&matchspec->ids, data->identifier)) {
			return false;
		}
	}
	if (matchspec->attrs & TOPLEVEL_ATTR_MAXIMIZED) {
		if (contains_value(&data->state,
			ZWLR_FOREIGN_TOPLEVEL_HANDLE_V1_STATE_MAXIMIZED) ^
			matchspec->maximized) {
			return false;
		}
	}
	if (matchspec->attrs & TOPLEVEL_ATTR_MINIMIZED) {
		if (contains_value(&data->state,
			ZWLR_FOREIGN_TOPLEVEL_HANDLE_V1_STATE_MINIMIZED) ^
			matchspec->minimized) {
			return false;
		}
	}
	if (matchspec->attrs & TOPLEVEL_ATTR_ACTIVATED) {
		if (contains_value(&data->state,
			ZWLR_FOREIGN_TOPLEVEL_HANDLE_V1_STATE_ACTIVATED) ^
			matchspec->activated) {
			return false;
		}
	}
	if (matchspec->attrs & TOPLEVEL_ATTR_FULLSCREEN) {
		if (contains_value(&data->state,
			ZWLR_FOREIGN_TOPLEVEL_HANDLE_V1_STATE_FULLSCREEN) ^
			matchspec->fullscreen) {
			return false;
		}
	}
	return true;
}

static void
merge_toplevel_data(struct toplevel_data *data)
{
	struct toplevel_data *other;
	wl_list_for_each(other, &data->cmd->toplevels, link) {
		if (other == data) continue;

		bool id_match = (data->identifier && other->identifier && strcmp(data->identifier, other->identifier) == 0);
		bool app_match = (data->app_id && other->app_id && strcmp(data->app_id, other->app_id) == 0);
		bool title_match = (!data->title && !other->title) || (data->title && other->title && strcmp(data->title, other->title) == 0);

		if (id_match || (app_match && title_match)) {
			// Don't merge if both have the same handle type but DIFFERENT handles (implies different windows)
			if (data->handle && other->handle && data->handle != other->handle) continue;
			if (data->ftl_list_handle && other->ftl_list_handle && data->ftl_list_handle != other->ftl_list_handle) continue;

			// Merge handles and core data
			if (!data->handle && other->handle) data->handle = other->handle;
			if (data->handle && !other->handle) other->handle = data->handle;

			if (!data->ftl_list_handle && other->ftl_list_handle) data->ftl_list_handle = other->ftl_list_handle;
			if (data->ftl_list_handle && !other->ftl_list_handle) other->ftl_list_handle = data->ftl_list_handle;

			if (!data->identifier && other->identifier) data->identifier = strdup(other->identifier);
			if (data->identifier && !other->identifier) other->identifier = strdup(data->identifier);

			if (data->state.size == 0 && other->state.size > 0) wl_array_copy(&data->state, &other->state);
			if (other->state.size == 0 && data->state.size > 0) wl_array_copy(&other->state, &data->state);
			
			// sync printed status to avoid double-listing in complete_toplevel
			if (data->printed) other->printed = true;
			if (other->printed) data->printed = true;
			
			// DO NOT sync done/matched. Each protocol listener must handle its own activation logic.
		}
	}
}

struct toplevel_data *
toplevel_data_create(struct wlrctl_toplevel_command *cmd)
{
	struct toplevel_data *data = calloc(1, sizeof (struct toplevel_data));
	if (!data) {
		die("Failed to allocate toplevel data\n");
	}

	wl_array_init(&data->state);
	data->cmd = cmd;
	wl_list_insert(&cmd->toplevels, &data->link);

	return data;
}

void
toplevel_data_destroy(struct toplevel_data *data)
{
	free(data->app_id);
	free(data->title);
	free(data->identifier);
	wl_array_release(&data->state);
	free(data);
}

static void
zwlr_foreign_toplevel_handle_v1_handle_title(void *user_data,
	struct zwlr_foreign_toplevel_handle_v1 *toplevel,
	const char *title
	)
{
	struct toplevel_data *data = user_data;
	if (data->title) free(data->title);
	data->title = strdup(title);
}

static void
zwlr_foreign_toplevel_handle_v1_handle_app_id(void *user_data,
	struct zwlr_foreign_toplevel_handle_v1 *toplevel,
	const char *app_id
	)
{
	struct toplevel_data *data = user_data;
	if (data->app_id) free(data->app_id);
	data->app_id = strdup(app_id);
}

static void
zwlr_foreign_toplevel_handle_v1_handle_state(void *user_data,
	struct zwlr_foreign_toplevel_handle_v1 *toplevel,
	struct wl_array *state
	)
{
	struct toplevel_data *data = user_data;
	wl_array_copy(&data->state, state);
}

void stop_toplevel(struct wlrctl *state);

static void
zwlr_foreign_toplevel_handle_v1_handle_done(void *user_data,
	struct zwlr_foreign_toplevel_handle_v1 *toplevel
	)
{
	struct toplevel_data *data = user_data;
	merge_toplevel_data(data);

	if (data->cmd->action != TOPLEVEL_ACTION_WAITFOR && data->done) {
		return;
	} else {
		data->done = true;
	}

	if (data->cmd->complete || !is_matched(data)) {
		return;
	}
	data->cmd->any = true;
	data->matched = true;

	switch (data->cmd->action) {
	case TOPLEVEL_ACTION_MINIMIZE:
		if (data->handle) zwlr_foreign_toplevel_handle_v1_set_minimized(data->handle);
		break;
	case TOPLEVEL_ACTION_UNMINIMIZE:
		if (data->handle) zwlr_foreign_toplevel_handle_v1_unset_minimized(data->handle);
		break;
	case TOPLEVEL_ACTION_MAXIMIZE:
		if (data->handle) zwlr_foreign_toplevel_handle_v1_set_maximized(data->handle);
		break;
	case TOPLEVEL_ACTION_ACTIVATE:
		if (data->handle) {
			zwlr_foreign_toplevel_handle_v1_activate(data->handle, data->cmd->state->seat);
			data->cmd->complete = true;
			stop_toplevel(data->cmd->state);
			wl_display_flush(data->cmd->state->display);
			data->cmd->state->running = false;
		}
		break;
	case TOPLEVEL_ACTION_FULLSCREEN:
		if (data->handle) zwlr_foreign_toplevel_handle_v1_set_fullscreen(data->handle, NULL);
		break;
	case TOPLEVEL_ACTION_CLOSE:
		if (data->handle) {
            zwlr_foreign_toplevel_handle_v1_close(data->handle);
			data->cmd->complete = true;
			stop_toplevel(data->cmd->state);
			wl_display_flush(data->cmd->state->display);
			data->cmd->state->running = false;
        }
		break;
	case TOPLEVEL_ACTION_LIST:
		// Handled in complete_toplevel
		break;
	case TOPLEVEL_ACTION_FIND:
	case TOPLEVEL_ACTION_WAITFOR:
		data->cmd->complete = true;
		stop_toplevel(data->cmd->state);
		break;
	case TOPLEVEL_ACTION_WAIT:
		data->cmd->waiting++;
		break;
	case TOPLEVEL_ACTION_UNSPEC:
		// unreachable
		assert(false);
	}
}

void destroy_toplevel(struct wlrctl *state);

static void
zwlr_foreign_toplevel_handle_v1_handle_closed(
	void *user_data, struct zwlr_foreign_toplevel_handle_v1 *toplevel)
{
	struct toplevel_data *data = user_data;
	zwlr_foreign_toplevel_handle_v1_destroy(toplevel);
	if (data->cmd->complete || !data->matched) {
		return;
	}
	if (data->cmd->action == TOPLEVEL_ACTION_WAIT) {
		data->cmd->waiting--;
		if (data->cmd->waiting <= 0) {
			data->cmd->complete = true;
			stop_toplevel(data->cmd->state);
		}
	}
}

static void
zwlr_foreign_toplevel_handle_v1_handle_parent(
	void *user_data, struct zwlr_foreign_toplevel_handle_v1 *toplevel,
	struct zwlr_foreign_toplevel_handle_v1 *parent)
{
	struct toplevel_data *data = user_data;
	data->parent = parent;
}

static struct zwlr_foreign_toplevel_handle_v1_listener
zwlr_foreign_toplevel_handle_v1_listener = {
	.title = zwlr_foreign_toplevel_handle_v1_handle_title,
	.app_id = zwlr_foreign_toplevel_handle_v1_handle_app_id,
	.output_enter = noop,
	.output_leave = noop,
	.state = zwlr_foreign_toplevel_handle_v1_handle_state,
	.done = zwlr_foreign_toplevel_handle_v1_handle_done,
	.closed = zwlr_foreign_toplevel_handle_v1_handle_closed,
	.parent = zwlr_foreign_toplevel_handle_v1_handle_parent,
};

static void
zwlr_foreign_toplevel_manager_v1_handle_toplevel(
	void *data,
	struct zwlr_foreign_toplevel_manager_v1 *manager,
	struct zwlr_foreign_toplevel_handle_v1 *toplevel
	)
{
	struct wlrctl *state = data;
	struct wlrctl_toplevel_command *cmd = state->cmd;
	struct toplevel_data *toplevel_data = toplevel_data_create(cmd);
	toplevel_data->handle = toplevel;
	zwlr_foreign_toplevel_handle_v1_add_listener(
		toplevel,
		&zwlr_foreign_toplevel_handle_v1_listener,
		toplevel_data
	);
}

static void
zwlr_foreign_toplevel_manager_v1_handle_finished(
	void *data,
	struct zwlr_foreign_toplevel_manager_v1 *manager
	)
{
	struct wlrctl *state = data;
	state->running = false;
	destroy_toplevel(state);
}

struct zwlr_foreign_toplevel_manager_v1_listener
zwlr_foreign_toplevel_manager_v1_listener = {
	.toplevel = zwlr_foreign_toplevel_manager_v1_handle_toplevel,
	.finished = zwlr_foreign_toplevel_manager_v1_handle_finished,
};

static void
ext_foreign_toplevel_handle_v1_handle_closed(
	void *user_data, struct ext_foreign_toplevel_handle_v1 *toplevel)
{
	struct toplevel_data *data = user_data;
	ext_foreign_toplevel_handle_v1_destroy(toplevel);
	if (data->cmd->complete || !data->matched) {
		return;
	}
	if (data->cmd->action == TOPLEVEL_ACTION_WAIT) {
		data->cmd->waiting--;
		if (data->cmd->waiting <= 0) {
			data->cmd->complete = true;
			stop_toplevel(data->cmd->state);
		}
	}
}

static void
ext_foreign_toplevel_handle_v1_handle_done(void *user_data,
	struct ext_foreign_toplevel_handle_v1 *toplevel
	)
{
	struct toplevel_data *data = user_data;
	merge_toplevel_data(data);

	if (data->cmd->action != TOPLEVEL_ACTION_WAITFOR && data->done) {
		return;
	} else {
		data->done = true;
	}

	if (data->cmd->complete || !is_matched(data)) {
		return;
	}
	data->cmd->any = true;
	data->matched = true;

	switch (data->cmd->action) {
	case TOPLEVEL_ACTION_LIST:
		// Handled by complete_toplevel
		break;
	case TOPLEVEL_ACTION_ACTIVATE:
		if (data->handle) {
			zwlr_foreign_toplevel_handle_v1_activate(data->handle, data->cmd->state->seat);
			data->cmd->complete = true;
			stop_toplevel(data->cmd->state);
			wl_display_flush(data->cmd->state->display);
			data->cmd->state->running = false;
		}
		break;
	case TOPLEVEL_ACTION_MAXIMIZE:
		if (data->handle) zwlr_foreign_toplevel_handle_v1_set_maximized(data->handle);
		break;
	case TOPLEVEL_ACTION_MINIMIZE:
		if (data->handle) zwlr_foreign_toplevel_handle_v1_set_minimized(data->handle);
		break;
	case TOPLEVEL_ACTION_UNMINIMIZE:
		if (data->handle) zwlr_foreign_toplevel_handle_v1_unset_minimized(data->handle);
		break;
	case TOPLEVEL_ACTION_CLOSE:
		if (data->handle) {
            zwlr_foreign_toplevel_handle_v1_close(data->handle);
			data->cmd->complete = true;
			stop_toplevel(data->cmd->state);
			wl_display_flush(data->cmd->state->display);
			data->cmd->state->running = false;
        }
		break;
	case TOPLEVEL_ACTION_FULLSCREEN:
		if (data->handle) zwlr_foreign_toplevel_handle_v1_set_fullscreen(data->handle, NULL);
		break;
	case TOPLEVEL_ACTION_FIND:
	case TOPLEVEL_ACTION_WAITFOR:
		data->cmd->complete = true;
		stop_toplevel(data->cmd->state);
		break;
	case TOPLEVEL_ACTION_WAIT:
		data->cmd->waiting++;
		break;
	default:
		break;
	}
}

static void
ext_foreign_toplevel_handle_v1_handle_title(void *user_data,
	struct ext_foreign_toplevel_handle_v1 *toplevel,
	const char *title
	)
{
	struct toplevel_data *data = user_data;
	if (data->title) free(data->title);
	data->title = strdup(title);
}

static void
ext_foreign_toplevel_handle_v1_handle_app_id(void *user_data,
	struct ext_foreign_toplevel_handle_v1 *toplevel,
	const char *app_id
	)
{
	struct toplevel_data *data = user_data;
	if (data->app_id) free(data->app_id);
	data->app_id = strdup(app_id);
}

static void
ext_foreign_toplevel_handle_v1_handle_identifier(void *user_data,
	struct ext_foreign_toplevel_handle_v1 *toplevel,
	const char *identifier
	)
{
	struct toplevel_data *data = user_data;
	if (data->identifier) free(data->identifier);
	data->identifier = strdup(identifier);
}

static struct ext_foreign_toplevel_handle_v1_listener
ext_foreign_toplevel_handle_v1_listener = {
	.closed = ext_foreign_toplevel_handle_v1_handle_closed,
	.done = ext_foreign_toplevel_handle_v1_handle_done,
	.title = ext_foreign_toplevel_handle_v1_handle_title,
	.app_id = ext_foreign_toplevel_handle_v1_handle_app_id,
	.identifier = ext_foreign_toplevel_handle_v1_handle_identifier,
};

static void
ext_foreign_toplevel_list_v1_handle_toplevel(
	void *data,
	struct ext_foreign_toplevel_list_v1 *manager,
	struct ext_foreign_toplevel_handle_v1 *toplevel
	)
{
	struct wlrctl *state = data;
	struct wlrctl_toplevel_command *cmd = state->cmd;
	struct toplevel_data *toplevel_data = toplevel_data_create(cmd);
	toplevel_data->ftl_list_handle = toplevel;
	ext_foreign_toplevel_handle_v1_add_listener(
		toplevel,
		&ext_foreign_toplevel_handle_v1_listener,
		toplevel_data
	);
}

static void
ext_foreign_toplevel_list_v1_handle_finished(
	void *data,
	struct ext_foreign_toplevel_list_v1 *manager
	)
{
	struct wlrctl *state = data;
	state->running = false;
	destroy_toplevel(state);
}

struct ext_foreign_toplevel_list_v1_listener
ext_foreign_toplevel_list_v1_listener = {
	.toplevel = ext_foreign_toplevel_list_v1_handle_toplevel,
	.finished = ext_foreign_toplevel_list_v1_handle_finished,
};

void
prepare_toplevel(struct wlrctl *state, int argc, char *argv[])
{
	struct wlrctl_toplevel_command *cmd = calloc(1, sizeof (struct wlrctl_toplevel_command));
	assert(cmd);

	wl_list_init(&cmd->toplevels);
	matchspec_init(&cmd->matchspec);

	if (argc == 0) {
		die("Missing toplevel action\n");
	}

	char *action = argv[0];
	cmd->action = parse_action(action);
	if (!cmd->action) {
		die("Unknown toplevel action: '%s'\n", action);
	}

	for (int i = 1; i < argc; i++) {
		matchspec_add_match(&cmd->matchspec, argv[i]);
	}

	state->cmd = cmd;
	cmd->state = state;
}

void
stop_toplevel(struct wlrctl *state)
{
	if (state->ftl_mgr) {
		zwlr_foreign_toplevel_manager_v1_stop(state->ftl_mgr);
	}
	if (state->ftl_list_mgr) {
		ext_foreign_toplevel_list_v1_stop(state->ftl_list_mgr);
	}
}

void
complete_toplevel(void *data, struct wl_callback *callback, uint32_t serial)
{
	struct wlrctl *state = data;
	struct wlrctl_toplevel_command *cmd = state->cmd;
	if (callback) {
		wl_callback_destroy(callback);
	}
	if (cmd->action == TOPLEVEL_ACTION_WAITFOR ||
		(cmd->action == TOPLEVEL_ACTION_WAIT && (cmd->waiting > 0))) {
		return;
	}

	if (cmd->action == TOPLEVEL_ACTION_LIST) {
		struct toplevel_data *toplevel, *tmp;
		wl_list_for_each_safe(toplevel, tmp, &cmd->toplevels, link) {
			merge_toplevel_data(toplevel);
			if (is_matched(toplevel) && !toplevel->printed) {
				toplevel->printed = true;
				
				if (toplevel->app_id) {
					cmd->any = true;
					char state_str[256];
					state_str[0] = '\0';
					if (toplevel->state.size > 0) {
						int *cursor;
						wl_array_for_each(cursor, &toplevel->state) {
							switch (*cursor) {
							case ZWLR_FOREIGN_TOPLEVEL_HANDLE_V1_STATE_MAXIMIZED:
								strcat(state_str, " maximized");
								break;
							case ZWLR_FOREIGN_TOPLEVEL_HANDLE_V1_STATE_MINIMIZED:
								strcat(state_str, " minimized");
								break;
							case ZWLR_FOREIGN_TOPLEVEL_HANDLE_V1_STATE_ACTIVATED:
								strcat(state_str, " activated");
								break;
							case ZWLR_FOREIGN_TOPLEVEL_HANDLE_V1_STATE_FULLSCREEN:
								strcat(state_str, " fullscreen");
								break;
							}
						}
					}

					printf("%s%s%s%s: %s%s\n", 
						toplevel->app_id, 
						toplevel->identifier ? " [" : "",
						toplevel->identifier ? toplevel->identifier : "",
						toplevel->identifier ? "]" : "",
						toplevel->title ? toplevel->title : "",
						state_str);
				}
			}
		}
	}

	if (!cmd->complete) {
		cmd->complete = true;
		cmd->state->failed = !cmd->any;
		stop_toplevel(state);
		cmd->state->running = false;
	}
}

static struct wl_callback_listener complete_listener = {
	.done = complete_toplevel
};

void
run_toplevel(struct wlrctl *state)
{
	struct wlrctl_toplevel_command *cmd = state->cmd;
	if (cmd->action == TOPLEVEL_ACTION_LIST) {
		wl_display_roundtrip(state->display);
		complete_toplevel(state, NULL, 0);
	} else {
		struct wl_callback *complete = wl_display_sync(state->display);
		wl_callback_add_listener(complete, &complete_listener, state);
	}
}

void
destroy_toplevel(struct wlrctl *state)
{
	struct wlrctl_toplevel_command *cmd = state->cmd;
	if (!cmd) {
		return;
	}

	matchspec_release(&cmd->matchspec);

	// Release toplevels
	struct toplevel_data *data, *tmp;
	wl_list_for_each_safe(data, tmp, &cmd->toplevels, link) {
		wl_list_remove(&data->link);
		toplevel_data_destroy(data);
	}
	free(cmd);
	state->cmd = NULL;
}
