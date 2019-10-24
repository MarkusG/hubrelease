#ifndef __RELEASER_GITHUB_H__
#define __RELEASER_GITHUB_H__

char *github_strip_remote(const char *remote);
int github_post_release(const char *payload);
int github_upload_asset(const char *path);

#endif
