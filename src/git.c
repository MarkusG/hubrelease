#include <stdio.h>
#include <stdlib.h>

#include "common.h"
#include "git.h"

int r_git_error(void)
{
	const git_error *error = git_error_last();
	return fprintf(stderr, ERR "git: %s\n", error->message);
}

int r_git_warn(void)
{
	const git_error *error = git_error_last();
	return fprintf(stderr, WARN "git: %s\n", error->message);
}

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
		r_git_error();
		return 1;
	}
}

int tag_at_callback(const char *name, git_oid *oid, void *userdata)
{
	tag_at_predicate *data;
	data = (tag_at_predicate*)userdata;
		
	git_tag *cur_tag;
	if (git_tag_lookup(&cur_tag, repo, oid))
	{
		fprintf(stderr, WARN "%s is lightweight and cannot be used for a release\n", name);
		r_git_warn();
		return 0;
	}

	const git_oid *this_target_oid = git_tag_target_id(cur_tag);
	for (int i = 0; i < 20; i++)
	{
		if (data->commit->id[i] != this_target_oid->id[i])
			return 0;
	}
	
	data->tag = cur_tag;
}

const git_tag *r_git_tag_at(const char *commit)
{
	git_oid commit_oid;
	if (git_oid_fromstr(&commit_oid, commit))
	{
		r_git_warn();
		return NULL;
	}
	
	tag_at_predicate predicate;
	predicate.commit = &commit_oid;
	predicate.tag = NULL;

	git_tag_foreach(repo, tag_at_callback, &predicate);
	return predicate.tag;
}

const char *r_git_commit_at_head()
{
	git_reference *head;
	if (git_repository_head(&head, repo))
	{
		r_git_error();
		return NULL;
	}

	if (git_reference_resolve(&head, head))
	{
		r_git_error();
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
		r_git_error();
		return NULL;
	}
	if (array.count == 0)
		return NULL;

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

const char *r_git_editor()
{
	git_config *config;
	if (git_repository_config(&config, repo))
	{
		r_git_warn();
		return NULL;
	}
	git_config *snapshot;
	if (git_config_snapshot(&snapshot, config))
	{
		r_git_warn();
		return NULL;
	}
	const char *editor;
	if (git_config_get_string(&editor, snapshot, "core.editor"))
	{
		r_git_warn();
		return NULL;
	}
	return editor;
}
