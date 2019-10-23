#include <git2.h>

typedef struct tag_at_predicate {
	const git_oid *commit;
	git_tag *tag;
} tag_at_predicate;
