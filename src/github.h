#ifndef __RELEASER_GITHUB_H__
#define __RELEASER_GITHUB_H__

#define MAX_USERNAME 64
#define MAX_PASSWORD 64
#define MAX_OTP 8

char *github_strip_remote(const char *remote);
const char *github_generate_token();

#endif
