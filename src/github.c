#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// strip a git remote URL to only user/repo
char *github_strip_remote(const char *remote)
{
	char *result = malloc(strlen(remote) * sizeof(char));
	// this is janky as all hell but I don't see a reason to not do it
	if (strncmp(remote, "git@github.com:", 15) == 0)
	{
		// git@github.com:user/repo.git
		int n = strlen(remote) - 13 - 6;
		strncpy(result, &remote[15], n);
		result[n + 1] = '\0';
		return result;
	}
	else if (strncmp(remote, "https://github.com/", 19) == 0)
	{
		// https://github.com/user/repo.git
		int n = strlen(remote) - 20 - 3;
		strncpy(result, &remote[19], n);
		result[n + 1] = '\0';
		return result;
	}
}
