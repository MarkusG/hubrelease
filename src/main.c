#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <sys/stat.h>

#include "common.h"
#include "git.h"
#include "github.h"

#define MAX_URL 128
#define GITHUB_REMOTE_BUFSIZE 8

char opt_draft = 0;
char opt_prerelease = 0;

int main(int argc, char *argv[])
{
	for (int i = 1; i < argc; i++)
	{
		if (strcmp(argv[i], "--draft") == 0)
			opt_draft = 1;
		if (strcmp(argv[i], "--prerelease") == 0)
			opt_prerelease = 1;
	}

	r_git_initialize();

	const char **remote_urls = r_git_list_remote_urls();
	if (!remote_urls)
	{
		fprintf(stderr, ERR "No remotes found\n");
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

	const char *head = r_git_commit_at_head();
	const git_tag *head_tag = r_git_tag_at(head);
	if (!head_tag)
	{
		fprintf(stderr, ERR "No annotated tag at HEAD\n");
		return 1;
	}
	// write release message
	// write tag message to release message file
	FILE *release_message = fopen(".releaser_message", "w");
	if (!release_message)
	{
		fprintf(stderr, ERR "Could not open .releaser_message for writing\n");
		perror(argv[0]);
		return 1;
	}

	if (opt_draft)
		fprintf(release_message, "%s", git_tag_name(head_tag));
	else
		fprintf(release_message, "%s", git_tag_message(head_tag));
	fclose(release_message);

	if (!opt_draft)
	{
		// get git editor
		const char *editor = r_git_editor();
		if (!editor)
			editor = getenv("EDITOR");
		if (!editor)
		{
			fprintf(stderr, WARN "No editor found. Defaulting to nano");
			editor = "nano";
		}

		// open vim and wait for user to edit message
		pid_t pid = fork();
		if (pid == -1)
		{
			fprintf(stderr, ERR "Couldn't fork\n");
			return 1;
		}
		if (pid == 0)
		{
			const char *args[] = { NULL, ".releaser_message", NULL };
			args[0] = editor;
			execvp(editor, (char**)args);
			exit(0);
		}
		else
			wait(NULL);

		// check if the user cleared the release message to abort the release
		struct stat release_message_stat;
		if (stat(".releaser_message", &release_message_stat) == -1)
			perror(argv[0]);
		if (release_message_stat.st_size == 0)
		{
			fprintf(stderr, ERR "Empty release message\n");
			return 1;
		}
	}
}
