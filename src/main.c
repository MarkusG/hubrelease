#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <sys/stat.h>

#include <jansson.h>

#include "common.h"
#include "options.h"
#include "curl.h"
#include "git.h"
#include "github.h"

#define MAX_REMOTE 64
#define MAX_ERROR 128
#define MAX_TITLE 64

#define PAYLOAD_EXTRA 256
#define REMOTE_BUFSIZE 8
#define ASSET_BUFSIZE 8

char opt_draft = 0;
char opt_prerelease = 0;
char *opt_remote;
const char *opt_github_token = NULL;
const char **opt_assets;

const char *argv0;
const char *base_url = "https://api.github.com";

int main(int argc, char *argv[])
{
	struct arguments args = parse_options(argc, argv);
	printf("draft: %s\npre: %s\ntok: %s\ngen: %s\nrem: %s\n",
		   args.draft ? "yes" : "no",
		   args.prerelease ? "yes" : "no",
		   args.token,
		   args.generate_token ? "yes" : "no",
		   args.remote);
	printf("assets:\n");
	for (int i = 0; args.assets[i]; i++)
		printf("%s\n", args.assets[i]);
	return 0;

	argv0 = argv[0];
	int n_assets;
	/* int result = parse_options(argc, argv, &n_assets); */
	/* if (result) */
	/* 	return result; */

	// initialize git
	git_libgit2_init();
	if (git_repository_open(&repo, ".git"))
	{
		h_git_error();
		return 1;
	}

	// initialize curl
	if (curl_global_init(CURL_GLOBAL_ALL))
	{
		fprintf(stderr, ERR "Could not initialize curl\n");
		return 1;
	}

	// go through the repository's remotes, present GitHub remotes to the user,
	// and let them choose the desired GitHub repository
	git_strarray array;
	if (git_remote_list(&array, repo))
	{
		h_git_error();
		return 1;
	}
	if (array.count == 0)
	{
		fprintf(stderr, ERR "No remotes found\n");
		return 1;
	}

	// set the preferred remote
	FILE *remote_file;
	if (opt_remote)
	{
		// get from command-line option
		if (git_remote_lookup(&preferred_remote, repo, opt_remote))
		{
			h_git_error();
			return 1;
		}
	}
	else if (remote_file = fopen(".hubrelease_remote", "r"))
	{
		// get from cache file
		const char *cached_remote_name = malloc(MAX_REMOTE * sizeof(char));
		fscanf(remote_file, "%s", cached_remote_name);
		fclose(remote_file);
		git_remote_lookup(&preferred_remote, repo, cached_remote_name);
	}
	else
	{
		// get from repository remotes
		// go through all the remotes and select unique GitHub remotes
		int bufsize = REMOTE_BUFSIZE;
		char **github_remotes = malloc(bufsize * sizeof(char*));
		char **github_remote_names = malloc(bufsize * sizeof(char*));
		int i = 0;
		int j = 0;
		for (i = 0; i < array.count; i++)
		{
			if (j > bufsize - 1)
			{
				bufsize += REMOTE_BUFSIZE;
				github_remotes = realloc(github_remotes, bufsize);
				github_remote_names = realloc(github_remotes, bufsize);
			}
				
			git_remote *remote;
			git_remote_lookup(&remote, repo, array.strings[i]);

			const char *name = git_remote_name(remote);
			const char *url = git_remote_url(remote);
			char *stripped_url = github_strip_remote(url);
			if (!stripped_url)
				continue;
			int k = 0;
			int dup = 0;
			while (k < j)
			{
				git_remote *check_remote;
				git_remote_lookup(&check_remote, repo, github_remotes[k++]);
				const char *check_url = git_remote_url(check_remote);
				const char *check_stripped = github_strip_remote(check_url);
				git_remote *current_remote;
				git_remote_lookup(&current_remote, repo, name);
				const char *current_url = git_remote_url(current_remote);
				const char *current_stripped = github_strip_remote(current_url);
				if (strcmp(check_stripped, current_stripped) == 0)
					dup = 1;
			}
			if (dup)
				continue;
			github_remotes[j] = malloc(MAX_REMOTE * sizeof(char));
			strcpy(github_remotes[j], name);
			github_remote_names[j] = malloc(MAX_URL * sizeof(char));
			strcpy(github_remote_names[j], stripped_url);
			j++;
		}
		github_remotes[j] = NULL;
		github_remote_names[j] = NULL;

		// if there's more than one unique GitHub remote, prompt the user to choose
		// which one they want to release to
		int remote_index = 0;
		if (j != 1)
		{
			int k = 0;
			while (github_remotes[k] != NULL)
				printf("%0d. %s\n", k++, github_remote_names[k]);
			printf("Select remote: ");
			scanf("%d", &remote_index);
		}
		// cache the preferred remote
		remote_file = fopen(".hubrelease_remote", "w");
		fputs(github_remotes[remote_index], remote_file);
		fclose(remote_file);
		git_remote_lookup(&preferred_remote, repo, github_remotes[remote_index]);
	}

	// get the repository's HEAD
	git_reference *head_ref;
	if (git_repository_head(&head_ref, repo))
	{
		h_git_error();
		return 1;
	}

	if (git_reference_resolve(&head_ref, head_ref))
	{
		h_git_error();
		return 1;
	}

	const git_oid *oid = git_reference_target(head_ref);

	char *head = malloc(42 * sizeof(char));
	head = git_oid_tostr(head, 42, oid);

	const git_tag *head_tag = h_git_tag_at(head);
	if (!head_tag)
	{
		fprintf(stderr, ERR "No annotated tag at HEAD\n");
		return 1;
	}


	// write release message
	if (access(".hubrelease_message", F_OK) != -1)
	{
		// user already wrote the message
		if (opt_draft)
		{
			fprintf(stderr, ERR "--draft set with pre-written release message\n");
			return 1;
		}
	}
	else if (opt_draft)
	{
		// write tag message to release message file
		FILE *release_message = fopen(".hubrelease_message", "w");
		if (!release_message)
		{
			fprintf(stderr, ERR "Could not open .hubrelease_message for writing\n");
			perror(argv0);
			return 1;
		}

		fputs(git_tag_message(head_tag), release_message);
		fclose(release_message);
	}
	else
	{
		if (!isatty(1))
		{
			fprintf(stderr, WARN "Opening editor while stdout is not to a tty\n");
			fprintf(stderr, WARN "If this causes a problem, consider editing .hubrelease_message before invoking hubrelease\n");
		}
		// open in editor
		// get git editor
		const char *editor = h_git_editor();
		if (!editor)
			editor = getenv("EDITOR");
		if (!editor)
		{
			fprintf(stderr, WARN "No editor found. Defaulting to nano");
			editor = "nano";
		}

		// have user edit message
		int len = strlen(editor) + strlen(".hubrelease_message");
		char *command = malloc(len * sizeof(char));
		sprintf(command, "%s .hubrelease_message", editor);
		system(command);

		// check if the user cleared the release message to abort the release
		struct stat release_message_stat;
		if (stat(".hubrelease_message", &release_message_stat) == -1 ||
			release_message_stat.st_size == 0)
		{
			fprintf(stderr, ERR "No release message\n");
			return 1;
		}
	}
	
	// ensure token exists
	if (!opt_github_token)
		// not passed on command line
		opt_github_token = getenv("HUBRELEASE_GITHUB_TOKEN");
	if (!opt_github_token)
	{
		// not passed through env var
		fprintf(stderr, ERR "No GitHub token supplied\n");
		return 1;
	}

	// form the release URL
	char *release_endpoint = malloc(MAX_URL * sizeof(char));
	char *stripped_remote = github_strip_remote(git_remote_url(preferred_remote));
	sprintf(release_endpoint, "%s/repos/%s/releases", base_url, stripped_remote);

	CURL *curl;
	CURLcode res;

	curl = curl_easy_init();
	if (!curl)
	{
		fprintf(stderr, ERR "Unknown error with libcurl");
		return 1;
	}

	curl_easy_setopt(curl, CURLOPT_URL, release_endpoint);
		
	// set useragent
	curl_easy_setopt(curl, CURLOPT_USERAGENT, "libcurl-agent/1.0");

	// set headers
	struct curl_slist *chunk = NULL;
	char *auth_header = malloc(MAX_HEADER * sizeof(char));
	sprintf(auth_header, "Authorization: token %s", opt_github_token);
	chunk = curl_slist_append(chunk, auth_header);
	curl_easy_setopt(curl, CURLOPT_HTTPHEADER, chunk);

	// set curl callback to save the response in memory
	curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_memory_callback);
	h_curl_response response;
	response.memory = malloc(1);
	response.size = 0;
	curl_easy_setopt(curl, CURLOPT_WRITEDATA, (void*)&response);

	// set POST data
	// get title and body from .hubrelease_message
	FILE *release_message = fopen(".hubrelease_message", "r");
	struct stat release_message_stat;
	if (stat(".hubrelease_message", &release_message_stat) == -1)
		perror(argv0);
	off_t message_size = release_message_stat.st_size;
	char *name = malloc(MAX_TITLE * sizeof(char));
	fscanf(release_message, "%[^\n]", name);
	char *body = malloc(release_message_stat.st_size * sizeof(char));
	fseek(release_message, 2, SEEK_CUR);
	char c;
	int i = 0;
	while ((c = fgetc(release_message)) != EOF)
		body[i++] = c;
	body[i] = '\0';
	fclose(release_message);
	if (remove(".hubrelease_message"))
		perror(argv0);

	// form data string and pass to curl
	json_t *data_json = json_pack("{sssssssbsb}", 
			"tag_name", git_tag_name(head_tag),
			"name", name,
			"body", body,
			"draft", opt_draft,
			"prerelease", opt_prerelease);
	char *data = json_dumps(data_json, JSON_INDENT(4));
	curl_easy_setopt(curl, CURLOPT_POSTFIELDS, data);

	// perform the request
	res = curl_easy_perform(curl);
	if (res != CURLE_OK)
	{
		fprintf(stderr, ERR "curl: %s\n", curl_easy_strerror(res));
		return 1;
	}

	json_error_t json_error;
	json_t *root = json_loads(response.memory, 0, &json_error);

	// check for errors
	long http_code = 0;
	curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &http_code);
	if (http_code >= 400)
	{
		json_t *error_message = json_object_get(root, "message");
		if (error_message)
		{
			fprintf(stderr, ERR "GitHub: %s", json_string_value(error_message));
			json_t *errors_array = json_object_get(root, "errors");
			if (errors_array)
			{
				fputs(" (", stderr);
				for (int i = 0; i < json_array_size(errors_array); i++)
				{
					json_t *error = json_array_get(errors_array, i);
					json_t *error_code = json_object_get(error, "code");
					fputs(json_string_value(error_code), stderr);
					if (i < json_array_size(errors_array) - 1)
						fputc(' ', stderr);
				}
				fputc(')', stderr);
			}
			fputc('\n', stderr);
			return 1;
		}
	}

	curl_easy_cleanup(curl);

	// get the release url and print to stdout
	json_t *html_url_field = json_object_get(root, "html_url");
	const char *html_url = json_string_value(html_url_field);

	fprintf(stderr, INFO "Release created at %s\n", html_url);
	printf("%s\n", html_url);

	// upload assets
	if (n_assets > 0)
	{
		// get upload url
		json_t *upload_field = json_object_get(root, "upload_url");
		const char *raw_upload_url = json_string_value(upload_field);
		char *base_upload_url = strdup(raw_upload_url);
		int len = strlen(raw_upload_url);
		int n = strlen("{?name,label}");
		base_upload_url[len - n] = '\0';

		// get asset files
		for (int i = 0; i < n_assets; i++)
		{
			fprintf(stderr, INFO "Uploading asset %s\n", opt_assets[i]);
			struct stat asset_stat;
			if (stat(opt_assets[i], &asset_stat) == -1)
			{
				fprintf(stderr, ERR "Error with asset %s:\n", opt_assets[i]);
				perror(argv0);
				continue;
			}
			if (asset_stat.st_size == 0)
			{
				fprintf(stderr, ERR "Empty asset file: %s\n", opt_assets[i]);
				return 1;
			}

			char *upload_url = malloc((strlen(base_upload_url) + strlen(opt_assets[i]) + strlen("?name=")) * sizeof(char));
			sprintf(upload_url, "%s?name=%s", base_upload_url, opt_assets[i]);

			// sanitize url
			// TODO do this properly
			int j = 0;
			char c;
			while ((c = upload_url[j]) != '\0')
			{
				if (c == ' ')
					upload_url[j] = '_';
				j++;
			}

			CURL *curl;
			CURLcode res;

			curl = curl_easy_init();
			if (!curl)
			{
				fprintf(stderr, ERR "Unknown error with libcurl");
				return 1;
			}

			// set url
			curl_easy_setopt(curl, CURLOPT_URL, upload_url);

			// set headers
			struct curl_slist *chunk = NULL;
			// TODO detect other file types
			chunk = curl_slist_append(chunk, "Content-Type: application/gzip");

			char *auth_header = malloc(MAX_HEADER * sizeof(char));
			sprintf(auth_header, "Authorization: token %s", opt_github_token);
			chunk = curl_slist_append(chunk, auth_header);
			curl_easy_setopt(curl, CURLOPT_HTTPHEADER, chunk);

			// write response to memory
			curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_memory_callback);
			h_curl_response response;
			response.memory = malloc(1);
			response.size = 0;
			curl_easy_setopt(curl, CURLOPT_WRITEDATA, (void*)&response);

			// set useragent
			curl_easy_setopt(curl, CURLOPT_USERAGENT, "libcurl-agent/1.0");

			// tell curl to upload the file
			FILE *asset_file = fopen(opt_assets[i], "r");

			curl_easy_setopt(curl, CURLOPT_UPLOAD, 1);
			curl_easy_setopt(curl, CURLOPT_READDATA, asset_file);
			curl_easy_setopt(curl, CURLOPT_INFILESIZE_LARGE, (curl_off_t)asset_stat.st_size);

			res = curl_easy_perform(curl);
			if (res != CURLE_OK)
			{
				fprintf(stderr, ERR "curl: %s\n", curl_easy_strerror(res));
				return 1;
			}
			fclose(asset_file);

			json_error_t json_error;
			json_t *root = json_loads(response.memory, 0, &json_error);

			// check for errors
			long http_code = 0;
			curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &http_code);
			if (http_code >= 400)
			{
				json_t *error_message = json_object_get(root, "message");
				if (error_message)
				{
					fprintf(stderr, ERR "GitHub: %s", json_string_value(error_message));
					json_t *errors_array = json_object_get(root, "errors");
					if (errors_array)
					{
						fputs(" (", stderr);
						for (int i = 0; i < json_array_size(errors_array); i++)
						{
							json_t *error = json_array_get(errors_array, i);
							json_t *error_code = json_object_get(error, "code");
							fputs(json_string_value(error_code), stderr);
							if (i < json_array_size(errors_array) - 1)
								fputc(' ', stderr);
						}
						fputc(')', stderr);
					}
					fputc('\n', stderr);
					return 1;
				}
			}
			curl_easy_cleanup(curl);
		}
	}
}

/* int parse_options(int argc, char *argv[], int *n_assets) */
/* { */
/* 	*n_assets = 0; */
/* 	int asset_bufsize = 0; */
/* 	opt_assets = malloc(asset_bufsize * sizeof(char*)); */
/* 	for (int i = 1; i < argc; i++) */
/* 	{ */
/* 		if (strcmp(argv[i], "--draft") == 0) */
/* 			opt_draft = 1; */
/* 		if (strcmp(argv[i], "--prerelease") == 0) */
/* 			opt_prerelease = 1; */
/* 		if (strcmp(argv[i], "--token") == 0) */
/* 		{ */
/* 			if (argv[i + 1][0] == '-' || !argv[i + 1]) */
/* 			{ */
/* 				fprintf(stderr, ERR "Option %s requires argument\n", argv[i]); */
/* 				return 1; */
/* 			} */
/* 			char *arg_github_token = malloc(strlen(argv[i + 1]) * sizeof(char)); */
/* 			strcpy(arg_github_token, argv[i + 1]); */
/* 			opt_github_token = arg_github_token; */
/* 		} */
/* 		if (strcmp(argv[i], "--generate-token") == 0) */
/* 		{ */
/* 			puts(github_generate_token()); */
/* 			return 0; */
/* 		} */
/* 		if (strcmp(argv[i], "--remote") == 0) */
/* 		{ */
/* 			if (argv[i + 1][0] == '-' || !argv[i + 1]) */
/* 			{ */
/* 				fprintf(stderr, ERR "Option %s requires argument\n", argv[i]); */
/* 				return 1; */
/* 			} */
/* 			opt_remote = malloc(strlen(argv[i + 1]) * sizeof(char)); */
/* 			strcpy(opt_remote, argv[i + 1]); */
/* 		} */
/* 		if (strcmp(argv[i], "--assets") == 0) */
/* 		{ */
/* 			if (argv[i + 1][0] == '-' || !argv[i + 1]) */
/* 			{ */
/* 				fprintf(stderr, ERR "Option %s requires argument\n", argv[i]); */
/* 				return 1; */
/* 			} */

/* 			i++; */
/* 			for (i; i < argc && argv[i][0] != '-'; i++) */
/* 			{ */
/* 				char dup = 0; */
/* 				for (int j = 0; j < *n_assets; j++) */
/* 				{ */
/* 					if (strcmp(opt_assets[j], argv[i]) == 0) */
/* 						dup = 1; */
/* 				} */
/* 				if (dup) */
/* 					continue; */

/* 				if (*n_assets + 1 > asset_bufsize) */
/* 				{ */
/* 					asset_bufsize += ASSET_BUFSIZE; */
/* 					opt_assets = realloc(opt_assets, asset_bufsize * sizeof(char*)); */
/* 				} */
/* 				opt_assets[(*n_assets)++] = argv[i]; */
/* 			} */
/* 		} */
/* 	} */
/* 	return 0; */
/* } */
