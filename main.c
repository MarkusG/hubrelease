#include <stdio.h>

#include "git.h"

int main(int argc, char *argv[])
{
	r_git_initialize();

	const char *commit = "133d5383640b677905f7de1905983b5d777efd09";
	const char *tag = r_git_tag_at(commit);
	if (!tag)
	{
		printf("No tag at %s\n", commit);
		return 1;
	}
	printf("Tag at %s is %s\n", commit, tag);
}
