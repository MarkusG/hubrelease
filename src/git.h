#ifndef __RELEASER_GIT_H__
#define __RELEASER_GIT_H__

#define REMOTE_URL_BUFSIZE 8
#include <git2.h>

int r_git_initialize();
const git_tag *r_git_tag_at(const char *commit);
const char *r_git_commit_at_head();
int r_git_push_tags();
const char **r_git_list_remote_urls();
const char *r_git_editor();
int r_git_set_preferred_remote(int index);

#endif
