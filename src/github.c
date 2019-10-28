#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// strip a git remote URL to only user/repo
char *github_strip_remote(const char *remote)
{
	char *result = malloc(strlen(remote) * sizeof(char));
	if (strncmp(remote, "git@github.com:", 15) == 0)
	{
		int start = 15;
		if (remote[start] == '/')
			// git@github.com:/user/repo.git instead of
			// git@github.com:user/repo.git
			start++;
		int len = strlen(remote) - start - 4;
		strncpy(result, &remote[start], len);
		result[len] = '\0';
		return result;
	}
	else if (strncmp(remote, "https://github.com/", 19) == 0)
	{
		// https://github.com/user/repo.git
		int n = strlen(remote) - 20 - 3;
		strncpy(result, &remote[19], n);
		result[n] = '\0';
		return result;
	}

	return NULL;
}
