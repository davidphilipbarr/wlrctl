#ifndef WLRCTL_UTIL_H
#define WLRCTL_UTIL_H

struct token {
	const char *name;
	int value;
};


int matchtok(const struct token tokens[], const char *name);

int timestamp();

void die(const char *fmt, ...);

#endif
