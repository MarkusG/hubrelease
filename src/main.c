#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <sys/stat.h>

#include <jansson.h>

#include "common.h"
#include "curl.h"
#include "git.h"
#include "github.h"

#define MAX_URL 128
#define MAX_USERNAME 64
#define MAX_PASSWORD 64
#define MAX_OTP 8
#define MAX_HEADER 64
#define MAX_REMOTE 64
#define MAX_ERROR 128
#define MAX_TITLE 64

#define PAYLOAD_EXTRA 256
#define REMOTE_BUFSIZE 8

char opt_draft = 0;
char opt_prerelease = 0;
char *opt_remote;
const char *opt_github_token;

int main(int argc, char *argv[])
{
	// parse command-line options
	for (int i = 1; i < argc; i++)
	{
		if (strcmp(argv[i], "--draft") == 0)
			opt_draft = 1;
		if (strcmp(argv[i], "--prerelease") == 0)
			opt_prerelease = 1;
		if (strcmp(argv[i], "--token") == 0)
		{
			if (!argv[i + 1])
			{
				fprintf(stderr, ERR "Option %s requires argument\n", argv[i]);
				return 1;
			}
			char *arg_github_token = malloc(strlen(argv[i + 1]) * sizeof(char));
			strcpy(arg_github_token, argv[i + 1]);
			opt_github_token = arg_github_token;
		}
		if (strcmp(argv[i], "--remote") == 0)
		{
			if (!argv[i + 1])
			{
				fprintf(stderr, ERR "Option %s requires argument\n", argv[i]);
				return 1;
			}
			opt_remote = malloc(strlen(argv[i + 1]) * sizeof(char));
			strcpy(opt_remote, argv[i + 1]);
		}
	}

	// initialize git
	git_libgit2_init();
	if (git_repository_open(&repo, ".git"))
	{
		r_git_error();
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
		r_git_error();
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
			r_git_error();
			return 1;
		}
	}
	else if (remote_file = fopen(".releaser_remote", "r"))
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
		remote_file = fopen(".releaser_remote", "w");
		fputs(github_remotes[remote_index], remote_file);
		fclose(remote_file);
		git_remote_lookup(&preferred_remote, repo, github_remotes[remote_index]);
	}

	// get the repository's HEAD
	git_reference *head_ref;
	if (git_repository_head(&head_ref, repo))
	{
		r_git_error();
		return 1;
	}

	if (git_reference_resolve(&head_ref, head_ref))
	{
		r_git_error();
		return 1;
	}

	const git_oid *oid = git_reference_target(head_ref);

	char *head = malloc(42 * sizeof(char));
	head = git_oid_tostr(head, 42, oid);

	const git_tag *head_tag = r_git_tag_at(head);
	if (!head_tag)
	{
		fprintf(stderr, ERR "No annotated tag at HEAD\n");
		return 1;
	}

	// write release message
	// write tag message to release message file
	FILE *release_message = fopen(".releaser_message", "w");
	if (!release_message)
	{
		fprintf(stderr, ERR "Could not open .releaser_message for writing\n");
		perror(argv[0]);
		return 1;
	}

	// write tag message to release message
	fprintf(release_message, "%s", git_tag_message(head_tag));
	fclose(release_message);

	// edit the release message if not a draft
	if (!opt_draft)
	{
		// get git editor
		const char *editor = r_git_editor();
		if (!editor)
			editor = getenv("EDITOR");
		if (!editor)
		{
			fprintf(stderr, WARN "No editor found. Defaulting to nano");
			editor = "nano";
		}

		// open vim and wait for user to edit message
		pid_t pid = fork();
		if (pid == -1)
		{
			fprintf(stderr, ERR "Couldn't fork\n");
			return 1;
		}
		if (pid == 0)
		{
			const char *args[] = { NULL, ".releaser_message", NULL };
			args[0] = editor;
			execvp(editor, (char**)args);
			exit(0);
		}
		else
			wait(NULL);

		// check if the user cleared the release message to abort the release
		struct stat release_message_stat;
		if (stat(".releaser_message", &release_message_stat) == -1)
			perror(argv[0]);
		if (release_message_stat.st_size == 0)
		{
			fprintf(stderr, ERR "Empty release message\n");
			return 1;
		}
	}
	
	char *base_url = "https://api.github.com";
	// authorize to GitHub
	// acquire access token
	FILE *token_file = fopen(".releaser_token", "r");
	if (token_file)
	{
		char *file_github_token = malloc(42);
		fgets(file_github_token, 41, token_file);
		opt_github_token = file_github_token;
		fclose(token_file);
	}
	else if (!opt_github_token)
	{
		// get the user's username
		char *github_username = malloc(MAX_USERNAME * sizeof(char));
		if (!github_username)
			perror(argv[0]);
		fprintf(stderr, "GitHub username: \n");
		scanf("%s", github_username);

		// get the user's password
		// should anything special be done here to protect the password?
		char *github_password = malloc(MAX_USERNAME * sizeof(char));
		if (!github_password)
			perror(argv[0]);
		fprintf(stderr, "GitHub password: \n");
		// TODO no echo
		scanf("%s", github_password);

		// acquire a token
		CURL *curl;
		CURLcode res;

		curl = curl_easy_init();
		if (!curl)
		{
			fprintf(stderr, ERR "Unknown error with libcurl");
			return 1;
		}
		
		// form auth URL
		char *auth_url = malloc((strlen(base_url) + strlen("/authorizations")) * sizeof(char));
		sprintf(auth_url, "%s/authorizations", base_url);
		curl_easy_setopt(curl, CURLOPT_URL, auth_url);

		// get OTP code if user is using 2FA
		char *github_otp = malloc(MAX_OTP * sizeof(char));
		if (!github_otp)
			perror(argv[0]);
		fprintf(stderr, "GitHub 2FA code (0 if not using 2FA): ");
		scanf("%s", github_otp);

		// set headers
		struct curl_slist *chunk = NULL;
		chunk = curl_slist_append(chunk, "Content-Type: application/json");

		if (strcmp(github_otp, "0") != 0)
		{
			char *github_otp_header = malloc(MAX_HEADER * sizeof(char));
			sprintf(github_otp_header, "X-GitHub-OTP: %s", github_otp);
			chunk = curl_slist_append(chunk, github_otp_header);
		}

		curl_easy_setopt(curl, CURLOPT_HTTPHEADER, chunk);

		// set username/password
		char *github_userpwd = malloc((MAX_USERNAME + MAX_PASSWORD) * sizeof(char));
		sprintf(github_userpwd, "%s:%s", github_username, github_password);
		curl_easy_setopt(curl, CURLOPT_USERPWD, github_userpwd);

		// set curl callback to save the response in memory
		curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_memory_callback);
		r_curl_response response;
		response.memory = malloc(1);
		response.size = 0;
		curl_easy_setopt(curl, CURLOPT_WRITEDATA, (void*)&response);

		// set useragent
		curl_easy_setopt(curl, CURLOPT_USERAGENT, "libcurl-agent/1.0");

		// set POST data
		char data[] = "{\"scopes\": [\"public_repo\"], \"note\": \"releaser\"}";
		curl_easy_setopt(curl, CURLOPT_POSTFIELDS, data);


		// perform the request
		res = curl_easy_perform(curl);
		if (res != CURLE_OK)
		{
			fprintf(stderr, ERR "curl: %s\n", curl_easy_strerror(res));
			return 1;
		}

		// read the response into a json object
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

		// strip the token from the response
		json_t *json_token = json_object_get(root, "token");
		opt_github_token = json_string_value(json_token);

		FILE *token_file = fopen(".releaser_token", "w");
		fputs(opt_github_token, token_file);
		fclose(token_file);
		fprintf(stderr, INFO "Wrote token to .releaser_token. DO NOT TRACK THIS FILE WITH GIT\n");
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
	r_curl_response response;
	response.memory = malloc(1);
	response.size = 0;
	curl_easy_setopt(curl, CURLOPT_WRITEDATA, (void*)&response);

	// set POST data
	// get title and body from .releaser_message
	release_message = fopen(".releaser_message", "r");
	struct stat release_message_stat;
	if (stat(".releaser_message", &release_message_stat) == -1)
		perror(argv[0]);
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

	// get the release url and print to stdout
	json_t *url_field = json_object_get(root, "url");
	const char *release_url = json_string_value(url_field);

	fprintf(stderr, INFO "Release created at:\n");
	printf("%s\n", release_url);
}
