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
			printf("%0d. %s\n", j, stripped_remote);
			github_remotes[j] = malloc(MAX_URL * sizeof(char));
			strcpy(github_remotes[j++], stripped_remote);
		}
		printf("Select remote: ");
		int remote_index = 0;
		scanf("%d", &remote_index);
		strcpy(preferred_remote, github_remotes[remote_index]);
		remote_file = fopen(".releaser_remote", "w");
		fputs(github_remotes[remote_index], remote_file);
		fclose(remote_file);
	}
	char *api_url = malloc(MAX_URL * sizeof(char));
	sprintf(api_url, "%s/repos/%s/releases", base_url, preferred_remote);
	printf("%s\n", api_url);
	return 0;
}
