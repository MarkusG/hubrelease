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
#define MAX_REMOTE 64
#define REMOTE_BUFSIZE 8

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

	// initialize git
	git_libgit2_init();
	if (git_repository_open(&repo, ".git"))
	{
		r_git_error();
		return 1;
	}

	// go through the repository's remotes, present GitHub remotes to the user,
	// and let them choose the desired GitHub repository
	git_strarray array;
	if (git_remote_list(&array, repo))
	{
		r_git_error();
		return 1;
	}
	if (array.count == 0)
	{
		fprintf(stderr, ERR "No remotes found\n");
		return 1;
	}

	FILE *remote_file = fopen(".releaser_remote", "r");
	if (remote_file)
	{
		const char *cached_remote_name = malloc(MAX_REMOTE * sizeof(char));
		fscanf(remote_file, "%s", cached_remote_name);
		fclose(remote_file);
		git_remote_lookup(&preferred_remote, repo, cached_remote_name);
	}
	else
	{
		// go through all the remotes and select unique GitHub remotes
		int bufsize = REMOTE_BUFSIZE;
		char **github_remotes = malloc(bufsize * sizeof(char*));
		char **github_remote_names = malloc(bufsize * sizeof(char*));
		int i = 0;
		int j = 0;
		for (i = 0; i < array.count; i++)
		{
			if (j > bufsize - 1)
			{
				bufsize += REMOTE_BUFSIZE;
				github_remotes = realloc(github_remotes, bufsize);
				github_remote_names = realloc(github_remotes, bufsize);
			}
				
			git_remote *remote;
			git_remote_lookup(&remote, repo, array.strings[i]);

			const char *name = git_remote_name(remote);
			const char *url = git_remote_url(remote);
			char *stripped_url = github_strip_remote(url);
			if (!stripped_url)
				continue;
			int k = 0;
			int dup = 0;
			while (k < j)
			{
				git_remote *check_remote;
				git_remote_lookup(&check_remote, repo, github_remotes[k++]);
				const char *check_url = git_remote_url(check_remote);
				const char *check_stripped = github_strip_remote(check_url);
				git_remote *current_remote;
				git_remote_lookup(&current_remote, repo, name);
				const char *current_url = git_remote_url(current_remote);
				const char *current_stripped = github_strip_remote(current_url);
				if (strcmp(check_stripped, current_stripped) == 0)
					dup = 1;
			}
			if (dup)
				continue;
			github_remotes[j] = malloc(MAX_REMOTE * sizeof(char));
			strcpy(github_remotes[j], name);
			github_remote_names[j] = malloc(MAX_URL * sizeof(char));
			strcpy(github_remote_names[j], stripped_url);
			j++;
		}
		github_remotes[j] = NULL;
		github_remote_names[j] = NULL;

		// if there's more than one unique GitHub remote, prompt the user to choose
		// which one they want to release to
		int remote_index = 0;
		if (j != 1)
		{
			int k = 0;
			while (github_remotes[k] != NULL)
				printf("%0d. %s\n", k++, github_remote_names[k]);
			printf("Select remote: ");
			scanf("%d", &remote_index);
		}
		remote_file = fopen(".releaser_remote", "w");
		fputs(github_remotes[remote_index], remote_file);
		fclose(remote_file);
		git_remote_lookup(&preferred_remote, repo, github_remotes[remote_index]);
	}

	char *api_url = malloc(MAX_URL * sizeof(char));
	char *stripped_remote = github_strip_remote(git_remote_url(preferred_remote));
	char *base_url = "https://api.github.com";
	sprintf(api_url, "%s/repos/%s/releases", base_url, stripped_remote);

	// get the repository's HEAD
	git_reference *head_ref;
	if (git_repository_head(&head_ref, repo))
	{
		r_git_error();
		return 1;
	}

	if (git_reference_resolve(&head_ref, head_ref))
	{
		r_git_error();
		return 1;
	}

	const git_oid *oid = git_reference_target(head_ref);

	char *head = malloc(42 * sizeof(char));
	head = git_oid_tostr(head, 42, oid);

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
