#include <stdio.h>
#include <stdlib.h>

#include "common.h"
#include "git.h"

git_repository *repo;
git_remote *preferred_remote;

int h_git_error(void)
{
	const git_error *error = git_error_last();
	return fprintf(stderr, ERR "git: %s\n", error->message);
}

int h_git_warn(void)
{
	const git_error *error = git_error_last();
	return fprintf(stderr, WARN "git: %s\n", error->message);
}

typedef struct tag_at_predicate {
	const git_oid *commit;
	git_tag *tag;
} tag_at_predicate;

int tag_at_callback(const char *name, git_oid *oid, void *userdata)
{
	tag_at_predicate *data;
	data = (tag_at_predicate*)userdata;
		
	git_tag *cur_tag;
	if (git_tag_lookup(&cur_tag, repo, oid))
	{
		fprintf(stderr, WARN "%s is lightweight and cannot be used for a release\n", name);
		h_git_warn();
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

const git_tag *h_git_tag_at(const char *commit)
{
	git_oid commit_oid;
	if (git_oid_fromstr(&commit_oid, commit))
	{
		h_git_warn();
		return NULL;
	}
	
	tag_at_predicate predicate;
	predicate.commit = &commit_oid;
	predicate.tag = NULL;

	git_tag_foreach(repo, tag_at_callback, &predicate);
	return predicate.tag;
}

const char *h_git_editor()
{
	git_config *config;
	if (git_repository_config(&config, repo))
	{
		h_git_warn();
		return NULL;
	}
	git_config *snapshot;
	if (git_config_snapshot(&snapshot, config))
	{
		h_git_warn();
		return NULL;
	}
	const char *editor;
	if (git_config_get_string(&editor, snapshot, "core.editor"))
	{
		h_git_warn();
		return NULL;
	}
	return editor;
}
