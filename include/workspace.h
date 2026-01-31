#ifndef WLRCTL_WORKSPACE_H
#define WLRCTL_WORKSPACE_H

#include "common.h"

void prepare_workspace(struct wlrctl *state, int argc, char *argv[]);
void run_workspace(struct wlrctl *state);

#endif
