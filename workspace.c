#define _POSIX_C_SOURCE 200809L
#include "workspace.h"
#include "common.h"
#include "ext-workspace-v1-client-protocol.h"



#include "util.h"
#include <assert.h>
#include <getopt.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <wayland-client.h>

struct workspace_info {
  char *name;
  struct ext_workspace_handle_v1 *handle;
  bool active;
  struct wl_list link;
};

static struct wl_list workspaces;

static void workspace_handle_id(void *data,
                                struct ext_workspace_handle_v1 *handle,
                                const char *id) {
  // ID isn't currently used for listing/activation by name, but could be
  // useful.
}

static void workspace_handle_name(void *data,
                                  struct ext_workspace_handle_v1 *handle,
                                  const char *name) {
  struct workspace_info *info = data;
  if (info->name) {
    free(info->name);
  }
  info->name = strdup(name);
}

static void workspace_handle_coordinates(void *data,
                                         struct ext_workspace_handle_v1 *handle,
                                         struct wl_array *coordinates) {}

static void workspace_handle_state(void *data,
                                   struct ext_workspace_handle_v1 *handle,
                                   uint32_t state) {
  struct workspace_info *info = data;
  info->active = state & EXT_WORKSPACE_HANDLE_V1_STATE_ACTIVE;
}

static void workspace_handle_capabilities(
    void *data, struct ext_workspace_handle_v1 *handle, uint32_t capabilities) {
}

static void workspace_handle_removed(void *data,
                                     struct ext_workspace_handle_v1 *handle) {
  struct workspace_info *info = data;
  ext_workspace_handle_v1_destroy(handle);
  wl_list_remove(&info->link);
  free(info->name);
  free(info);
}

static const struct ext_workspace_handle_v1_listener workspace_listener = {
    .id = workspace_handle_id,
    .name = workspace_handle_name,
    .coordinates = workspace_handle_coordinates,
    .state = workspace_handle_state,
    .capabilities = workspace_handle_capabilities,
    .removed = workspace_handle_removed,
};

static void
manager_handle_workspace_group(void *data, struct ext_workspace_manager_v1 *mgr,
                               struct ext_workspace_group_handle_v1 *group) {
  // We don't currently track groups explicitly for simple listing/activation,
  // but the protocol requires handling the binding.
  // For now we just let it exist.
}

static void
manager_handle_workspace(void *data, struct ext_workspace_manager_v1 *mgr,
                         struct ext_workspace_handle_v1 *workspace) {
  struct workspace_info *info = calloc(1, sizeof(struct workspace_info));
  info->handle = workspace;
  ext_workspace_handle_v1_add_listener(workspace, &workspace_listener, info);
  wl_list_insert(workspaces.prev, &info->link);
}

static void manager_handle_done(void *data,
                                struct ext_workspace_manager_v1 *mgr) {
  // Initial sync done or updates committed.
}

static void manager_handle_finished(void *data,
                                    struct ext_workspace_manager_v1 *mgr) {
  // Compositor is done with us.
}

static const struct ext_workspace_manager_v1_listener manager_listener = {
    .workspace_group = manager_handle_workspace_group,
    .workspace = manager_handle_workspace,
    .done = manager_handle_done,
    .finished = manager_handle_finished,
};

// ... implementation of prepare_workspace and run_workspace will go here ...
// Splitting this up to avoid huge file creation in one go if needed, but I'll
// try to put basic structure.



struct workspace_cmd {
  enum {
    ACTION_LIST,
    ACTION_ACTIVATE,
    ACTION_CURRENT,
    ACTION_NEXT,
    ACTION_PREV
  } action;
  char *target_name;
};

void prepare_workspace_impl(struct wlrctl *state, int argc, char *argv[]) {
  if (argc == 0) {
    die("Missing workspace action\n");
  }

  struct workspace_cmd *cmd = calloc(1, sizeof(struct workspace_cmd));
  if (strcmp(argv[0], "list") == 0) {
    cmd->action = ACTION_LIST;
  } else if (strcmp(argv[0], "current") == 0) {
    cmd->action = ACTION_CURRENT;
  } else if (strcmp(argv[0], "next") == 0) {
    cmd->action = ACTION_NEXT;
  } else if (strcmp(argv[0], "prev") == 0) {
    cmd->action = ACTION_PREV;
  } else if (strcmp(argv[0], "activate") == 0 || strcmp(argv[0], "goto") == 0) {
    if (argc < 2) {
      die("Missing workspace name for %s\n", argv[0]);
    }
    cmd->action = ACTION_ACTIVATE;
    cmd->target_name = strdup(argv[1]);
  } else {
    die("Unknown workspace action: %s\n", argv[0]);
  }
  state->cmd = cmd;
}

// Wrapper to match signature in header if I implement it directly there or
// here. I pasted the impl above.
void prepare_workspace(struct wlrctl *state, int argc, char *argv[]) {
  prepare_workspace_impl(state, argc, argv);
}

void run_workspace(struct wlrctl *state) {
  wl_list_init(&workspaces);
  ext_workspace_manager_v1_add_listener(state->workspace_mgr, &manager_listener,
                                        state);

  // Roundtrip to get initial events
  wl_display_roundtrip(state->display);

  // Wait for a bit more if needed? Usually roundtrip is enough for initial
  // enumeration if the compositor sends them immediately. The protocol says
  // "After a client binds ... each workspace will be sent".

  struct workspace_cmd *cmd = state->cmd;

  if (cmd->action == ACTION_LIST) {
    struct workspace_info *info;
    wl_list_for_each(info, &workspaces, link) {
      if (info->name) {
        printf("%s\n", info->name);
      }
    }
  } else if (cmd->action == ACTION_ACTIVATE) {
    struct workspace_info *info;
    bool found = false;
    wl_list_for_each(info, &workspaces, link) {
      if (info->name && strcmp(info->name, cmd->target_name) == 0) {
        ext_workspace_handle_v1_activate(info->handle);
        found = true;
        break;
      }
    }
    if (!found) {
      fprintf(stderr, "Workspace '%s' not found\n", cmd->target_name);
      exit(EXIT_FAILURE);
    }
    // Commit the request?
    // The protocol has a commit request on the manager for atomic updates.
    ext_workspace_manager_v1_commit(state->workspace_mgr);
    wl_display_roundtrip(state->display);
  } else if (cmd->action == ACTION_CURRENT) {
    struct workspace_info *info;
    wl_list_for_each(info, &workspaces, link) {
      if (info->active) {
        printf("%s\n", info->name);
        break;
      }
    }
  } else if (cmd->action == ACTION_NEXT || cmd->action == ACTION_PREV) {
    struct workspace_info *pos;
    struct workspace_info *target = NULL;
    struct workspace_info *active_ws = NULL;

    wl_list_for_each(pos, &workspaces, link) {
      if (pos->active) {
        active_ws = pos;
        break;
      }
    }

    if (!active_ws) {
       // Fallback: pick safe default or exit?
       // If no workspace is active, maybe just pick the first one?
       // For now, let's complain.
       fprintf(stderr, "No active workspace found\n");
       exit(EXIT_FAILURE);
    }

    struct wl_list *link = &active_ws->link;
    if (cmd->action == ACTION_NEXT) {
      if (link->next == &workspaces) {
        target = wl_container_of(workspaces.next, target, link);
      } else {
        target = wl_container_of(link->next, target, link);
      }
    } else {
      if (link->prev == &workspaces) {
        target = wl_container_of(workspaces.prev, target, link);
      } else {
        target = wl_container_of(link->prev, target, link);
      }
    }

    if (target) {
      ext_workspace_handle_v1_activate(target->handle);
      ext_workspace_manager_v1_commit(state->workspace_mgr);
      wl_display_roundtrip(state->display);
    }
  }

  state->running = false;
}
