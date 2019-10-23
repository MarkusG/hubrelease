#ifndef __RELEASER_GIT_H__
#define __RELEASER_GIT_H__
#include <git2.h>

int r_git_initialize();
const char *r_git_tag_at(const char *commit);
int r_git_tag_create(char *commit);
int r_git_push_tags();
char **r_git_list_remotes();
#endif
