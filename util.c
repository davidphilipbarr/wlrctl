#define _POSIX_C_SOURCE 200112L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <time.h>
#include "util.h"

int
matchtok(const struct token tokens[], const char *name)
{
	const struct token *tok;
	for (tok = tokens; tok->name != NULL; tok++) {
		if (strcmp(tok->name, name) == 0) {
			break;
		}
	}
	return tok->value;
}

int
timestamp()
{
	struct timespec tp;
	clock_gettime(CLOCK_MONOTONIC, &tp);
	int ms = 1000 * tp.tv_sec + tp.tv_nsec / 1000000;
	return ms;
}

void
die(const char *fmt, ...)
{
	va_list args;
	va_start(args, fmt);
	vfprintf(stderr, fmt, args);
	va_end(args);
	exit(EXIT_FAILURE);
}
