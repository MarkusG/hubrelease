#ifndef __RELEASER_GIT_H__
#define __RELEASER_GIT_H__

#include <git2.h>

git_repository *repo;
git_remote *preferred_remote;

int h_git_error(void);
int h_git_warn(void);

const git_tag *h_git_tag_at(const char *commit);
const char *h_git_editor();

#endif
