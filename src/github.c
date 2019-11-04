#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <jansson.h>

#include "common.h"
#include "github.h"
#include "curl.h"

// strip a git remote URL to only user/repo
char *github_strip_remote(const char *remote)
{
	char *result = malloc(strlen(remote) * sizeof(char));
	if (strncmp(remote, "git@github.com:", 15) == 0)
	{
		int start = 15;
		if (remote[start] == '/')
			// git@github.com:/user/repo.git instead of
			// git@github.com:user/repo.git
			start++;
		int len = strlen(remote) - start - 4;
		strncpy(result, &remote[start], len);
		result[len] = '\0';
		return result;
	}
	else if (strncmp(remote, "https://github.com/", 19) == 0)
	{
		// https://github.com/user/repo.git
		int n = strlen(remote) - 20 - 3;
		strncpy(result, &remote[19], n);
		result[n] = '\0';
		return result;
	}

	return NULL;
}

const char *github_generate_token()
{
	// get the user's username
	char *github_username = malloc(MAX_USERNAME * sizeof(char));
	if (!github_username)
		perror(argv0);
	fprintf(stderr, "GitHub username: ");
	scanf("%s", github_username);

	// get the user's password
	// should anything special be done here to protect the password?
	char *github_password = malloc(MAX_USERNAME * sizeof(char));
	if (!github_password)
		perror(argv0);
	fprintf(stderr, "GitHub password: ");
	// TODO no echo
	scanf("%s", github_password);

	// acquire a token
	CURL *curl;
	CURLcode res;

	curl = curl_easy_init();
	if (!curl)
	{
		fprintf(stderr, ERR "Unknown error with libcurl");
		return NULL;
	}

	// form auth URL
	char *auth_url = malloc((strlen(base_url) + strlen("/authorizations")) * sizeof(char));
	sprintf(auth_url, "%s/authorizations", base_url);
	curl_easy_setopt(curl, CURLOPT_URL, auth_url);

	// get OTP code if user is using 2FA
	char *github_otp = malloc(MAX_OTP * sizeof(char));
	if (!github_otp)
		perror(argv0);
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
	h_curl_response response;
	response.memory = malloc(1);
	response.size = 0;
	curl_easy_setopt(curl, CURLOPT_WRITEDATA, (void*)&response);

	// set useragent
	curl_easy_setopt(curl, CURLOPT_USERAGENT, "libcurl-agent/1.0");

	// set POST data
	char data[] = "{\"scopes\": [\"public_repo\"], \"note\": \"hubrelease\"}";
	curl_easy_setopt(curl, CURLOPT_POSTFIELDS, data);


	// perform the request
	res = curl_easy_perform(curl);
	if (res != CURLE_OK)
	{
		fprintf(stderr, ERR "curl: %s\n", curl_easy_strerror(res));
		return NULL;
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
			return NULL;
		}
	}

	// strip the token from the response
	json_t *json_token = json_object_get(root, "token");
	return json_string_value(json_token);

	curl_easy_cleanup(curl);
}
