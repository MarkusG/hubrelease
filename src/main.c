#include <stdio.h>

#include "git.h"
#include "github.h"

int main(int argc, char *argv[])
{
	r_git_initialize();

	const char **remote_urls = r_git_list_remote_urls();

	const char *head = r_git_commit_at_head();
	printf("HEAD: %s\n", head);

	const char *tag = r_git_tag_at(head);
	if (!tag)
	{
		printf("No tag at %s\n", head);
		/* return 1; */
	}
	printf("Tag at %s is %s\n", head, tag);

	printf("\n");
	int i = 0;
	while (remote_urls[i] != NULL)
	{
		printf("remote: %s\n", remote_urls[i]);
		printf("stripped: %s\n", github_strip_remote(remote_urls[i++]));
	}
	return 0;
}
