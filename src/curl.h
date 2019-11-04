#ifndef __RELEASER_CURL_H__
#define __RELEASER_CURL_H__

#include <curl/curl.h>

#define MAX_URL 128
#define MAX_HEADER 64

typedef struct h_curl_response {
	char *memory;
	size_t size;
} h_curl_response;

size_t write_memory_callback(void *contents, size_t size, size_t nmemb, void *userp);

#endif
