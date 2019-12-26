#ifndef __RELEASER_OPTIONS_H__
#define __RELEASER_OPTIONS_H__

#include <argp.h>

struct arguments {
	int draft, prerelease;
	char *token, *remote, **assets;
	int generate_token;
};

struct arguments parse_options(int argc, char *argv[]);

#endif
