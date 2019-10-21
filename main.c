#include <stdio.h>
#include <git2.h>

// also clean up everything and make it more readable. variable names in callback are a mess

struct tag_target {
	git_repository *repo;
	const git_oid *object;

	git_tag *tag;
} tag_target;

int points_at_callback(const char *name, git_oid *oid, void *userdata)
{
	printf("%s\n", name);
	char *oid_str = malloc(64 * sizeof(char));
	git_oid_tostr(oid_str, 64, oid);
	printf("tag id:    %s\n", oid_str);
	struct tag_target *target;
	target = (struct tag_target*)userdata;
		
	git_tag *tag;
	git_tag_lookup(&tag, target->repo, oid);
	if (!tag)
	{
		// not an annotated tag; tag id is same as commit id
		// TODO what if it's not a commit
		for (int i = 0; i < 20; i++)
		{
			if (target->object->id[i] != oid->id[i])
				return 0;
		}
		printf("Match!\n");
		target->tag = tag;
		return 1;
	}

	const git_oid *target_oid = git_tag_target_id(tag);
	git_oid_tostr(oid_str, 64, target_oid);

	printf("target id: %s\n\n", oid_str);

	for (int i = 0; i < 20; i++)
	{
		if (target->object->id[i] != target_oid->id[i])
			return 0;
	}
	
	printf("Match!\n");
	target->tag = tag;
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
	const git_oid *target = git_reference_target(resolved);

	git_commit *head_commit;
	error = git_commit_lookup(&head_commit, repo, target);
	if (error)
	{
		const git_error *error = git_error_last();
		printf("%s\n", error->message);
	}

	struct tag_target tag_target;
	tag_target.repo = repo;
	tag_target.object = target;
	// figure out why not setting this to null makes the tag's name the name of a random shell variable
	tag_target.tag = NULL;

	git_tag_foreach(repo, points_at_callback, &tag_target);
	if (tag_target.tag == NULL)
	{
		printf("No tag at HEAD\n");
		return 1;
	}
	const char* tag_name = git_tag_name(tag_target.tag);
	printf("Tag at HEAD is %s\n", git_tag_name(tag_target.tag));
	return 0;
}
