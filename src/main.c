#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "git.h"
#include "github.h"

#define MAX_URL 128
#define GITHUB_REMOTE_BUFSIZE 8

int main(int argc, char *argv[])
{
	r_git_initialize();

	const char **remote_urls = r_git_list_remote_urls();
	if (!remote_urls)
	{
		fprintf(stderr, "No remotes found\n");
		return 1;
	}

	char *base_url = "https://api.github.com";
	// go through the repository's remotes, present GitHub remotes to the user,
	// and let them choose the desired GitHub repository
	char *preferred_remote = malloc(MAX_URL * sizeof(char));
	FILE *remote_file = fopen(".releaser_remote", "r");
	if (remote_file)
	{
		fscanf(remote_file, "%s", preferred_remote);
		fclose(remote_file);
	}
	else
	{
		char **github_remotes = malloc(GITHUB_REMOTE_BUFSIZE * sizeof(char*));
		int i = 0;
		int j = 0;
		// go through all the remotes and select unique GitHub remotes
		while (remote_urls[i] != NULL)
		{
			char *stripped_remote = github_strip_remote(remote_urls[i++]);
			if (stripped_remote == NULL)
				continue;
			int k = 0;
			int dup = 0;
			while (k < j)
			{
				if (strcmp(github_remotes[k++], stripped_remote) == 0)
					dup = 1;
			}
			if (dup)
				continue;
			github_remotes[j] = malloc(MAX_URL * sizeof(char));
			strcpy(github_remotes[j++], stripped_remote);
		}
		// if there's more than one unique GitHub remote, prompt the user to choose
		// which one they want to release to
		int remote_index = 0;
		if (j != 1)
		{
			int k = 0;
			while (github_remotes[k] != NULL)
				printf("%0d. %s\n", k++, github_remotes[k]);
			printf("Select remote: ");
			scanf("%d", &remote_index);
			remote_file = fopen(".releaser_remote", "w");
			fputs(github_remotes[remote_index], remote_file);
			fclose(remote_file);
		}
		strcpy(preferred_remote, github_remotes[remote_index]);
	}
	char *api_url = malloc(MAX_URL * sizeof(char));
	sprintf(api_url, "%s/repos/%s/releases", base_url, preferred_remote);
	printf("%s\n", api_url);
	return 0;
}
