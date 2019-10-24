#include <stdio.h>
#include <stdlib.h>

#include "git.h"

typedef struct tag_at_predicate {
	const git_oid *commit;
	git_tag *tag;
} tag_at_predicate;

static git_repository *repo;

int r_git_initialize()
{
	git_libgit2_init();
	if (git_repository_open(&repo, ".git"))
	{
		const git_error *error = git_error_last();
		fprintf(stderr, "git: %s\n", error->message);
		return 1;
	}
}

int tag_at_callback(const char *name, git_oid *oid, void *userdata)
{
	tag_at_predicate *data;
	data = (tag_at_predicate*)userdata;
		
	git_tag *cur_tag;
	git_tag_lookup(&cur_tag, repo, oid);
	if (!cur_tag)
	{
		// not an annotated tag; tag id is same as commit id
		for (int i = 0; i < 20; i++)
		{
			if (data->commit->id[i] != oid->id[i])
				return 0;
		}
		data->tag = cur_tag;
		return 1;
	}

	const git_oid *this_target_oid = git_tag_target_id(cur_tag);
	for (int i = 0; i < 20; i++)
	{
		if (data->commit->id[i] != this_target_oid->id[i])
			return 0;
	}
	
	data->tag = cur_tag;
}

const char *r_git_tag_at(const char *commit)
{
	git_oid *commit_oid;
	if (git_oid_fromstr(commit_oid, commit))
	{
		const git_error *error = git_error_last();
		fprintf(stderr, "git: %s\n", error->message);
		return NULL;
	}
	
	tag_at_predicate predicate;
	predicate.commit = commit_oid;
	predicate.tag = NULL;

	git_tag_foreach(repo, tag_at_callback, &predicate);
	if (predicate.tag == NULL)
		return NULL;
	return git_tag_name(predicate.tag);
}

const char *r_git_commit_at_head()
{
	git_reference *head;
	if (git_repository_head(&head, repo))
	{
		const git_error *error = git_error_last();
		fprintf(stderr, "git: %s\n", error->message);
		return NULL;
	}

	if (git_reference_resolve(&head, head))
	{
		const git_error *error = git_error_last();
		fprintf(stderr, "git: %s\n", error->message);
		return NULL;
	}

	const git_oid *oid = git_reference_target(head);

	char *out = malloc(42 * sizeof(char));
	return git_oid_tostr(out, 42, oid);
}

const char **r_git_list_remote_urls()
{
	git_strarray array;
	if (git_remote_list(&array, repo))
	{
		const git_error *error = git_error_last();
		fprintf(stderr, "git: %s\n", error->message);
		return NULL;
	}

	int bufsize = REMOTE_URL_BUFSIZE;
	const char **list = malloc(bufsize * sizeof(char*)) ;
	int i;
	for (i = 0; i < array.count; i++)
	{
		if (i > bufsize - 1)
		{
			bufsize += REMOTE_URL_BUFSIZE;
			list = realloc(list, bufsize);
		}
		git_remote *remote;
		git_remote_lookup(&remote, repo, array.strings[i]);

		list[i] = git_remote_url(remote);
	}
	list[i] = NULL;
	git_strarray_free(&array);
	return list;
}
