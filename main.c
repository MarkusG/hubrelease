#include <stdio.h>
#include <git2.h>

struct points_at_data {
	git_repository *repo;
	const git_oid *object;

	git_tag *tag;
} points_at_data;

int points_at_callback(const char *name, git_oid *oid, void *userdata)
{
	struct points_at_data *data;
	data = (struct points_at_data*)userdata;
		
	git_tag *this_tag;
	git_tag_lookup(&this_tag, data->repo, oid);
	if (!this_tag)
	{
		// not an annotated tag; tag id is same as commit id
		// TODO what if it's not a commit
		for (int i = 0; i < 20; i++)
		{
			if (data->object->id[i] != oid->id[i])
				return 0;
		}
		data->tag = this_tag;
		return 1;
	}

	const git_oid *this_target_oid = git_tag_target_id(this_tag);
	for (int i = 0; i < 20; i++)
	{
		if (data->object->id[i] != this_target_oid->id[i])
			return 0;
	}
	
	data->tag = this_tag;
	return 1;
}

int main(int argc, char *argv[])
{
	git_libgit2_init();

	git_repository *repo;
	int error = git_repository_open(&repo, ".git");
	if (error)
	{
		const git_error *error = git_error_last();
		printf("%s\n", error->message);
	}

	git_reference *head;
	git_repository_head(&head, repo);
	git_reference *resolved;
	git_reference_resolve(&resolved, head);
	const git_oid *head_oid = git_reference_target(resolved);

	struct points_at_data target;
	target.repo = repo;
	target.object = head_oid;
	target.tag = NULL;

	git_tag_foreach(repo, points_at_callback, &target);
	if (target.tag == NULL)
	{
		printf("No tag at HEAD\n");
		return 1;
	}
	const char* tag_name = git_tag_name(target.tag);
	printf("Tag at HEAD is %s\n", git_tag_name(target.tag));
	return 0;
}
