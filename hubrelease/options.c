#include "options.h"

const char *argp_program_version =
	VERSION;
const char *argp_program_bug_address =
	"https://github.com/MarkusG/hubrelease/issues";
static char doc[] =
	"hubrelease -- automate the creation of GitHub releases";
static char args_doc[] = "[ASSET...]";

static struct argp_option options[] = {
	{"draft",		'd', 0,		0,	"Mark release as draft"},
	{"prerelease",		'p', 0,		0,	"Mark release as prerelease"},
	{"generate-token",	'g', 0,		0,	"Generate GitHub authentication token and exit"},
	{"token",		't', "TOKEN",	0,	"GitHub authentication token"},
	{"remote",		'r', "REMOTE",	0,	"Specify remote to parse to GitHub URL"},
	{ 0 }
};

static error_t parse_opt(int key, char *arg, struct argp_state *state)
{
	struct arguments *arguments = state->input;

	switch (key)
	{
		case 'd':
			arguments->draft = 1;
			break;
		case 'p':
			arguments->prerelease = 1;
			break;
		case 'g':
			arguments->generate_token = 1;
			break;
		case 't':
			arguments->token = arg;
			break;
		case 'r':
			arguments->remote = arg;
			break;

		case ARGP_KEY_ARG:
			// finished parsing options
			// set the assets pointer to the address of the first asset argument
			arguments->assets = &state->argv[state->next - 1];
			state->next = state->argc;
			break;

		default:
			return ARGP_ERR_UNKNOWN;
	}
	return 0;
}

static struct argp argp = { options, parse_opt, args_doc, doc };

struct arguments parse_options(int argc, char *argv[])
{
	struct arguments arguments;
	arguments.draft = 0;
	arguments.prerelease = 0;
	arguments.generate_token = 0;
	arguments.token = NULL;
	arguments.remote = NULL;
	arguments.assets = NULL;
	argp_parse(&argp, argc, argv, 0, 0, &arguments);
	return arguments;
}
