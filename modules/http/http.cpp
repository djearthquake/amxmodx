#include <atomic>
#include <cstring>
#include <deque>
#include <string>

#include <curl/curl.h>
#include <pthread.h>

#include "amxxmodule.h"

namespace
{
	const size_t kMaxUrlLength = 2048;
	const size_t kMaxPostLength = 16384;
	const size_t kMaxResponseLength = 32768;
	const size_t kMaxQueuedRequests = 16;
	const int kWorkerCount = 2;
	const int kDefaultTimeout = 10;
	const int kMaxTimeout = 30;
	const int kResponseTooLarge = -1;

	struct HttpRequest
	{
		unsigned int id;
		int forward;
		cell data;
		std::string url;
		std::string post;
		long timeout;
		bool is_post;
	};

	struct HttpResult
	{
		HttpRequest *request;
		long status;
		int error;
		std::string body;
		bool response_too_large;
	};

	pthread_mutex_t g_mutex = PTHREAD_MUTEX_INITIALIZER;
	pthread_cond_t g_condition = PTHREAD_COND_INITIALIZER;
	std::deque<HttpRequest *> g_pending;
	std::deque<HttpResult *> g_completed;
	size_t g_outstanding = 0;
	pthread_t g_workers[kWorkerCount];
	int g_workers_started = 0;
	bool g_accepting = false;
	std::atomic<bool> g_stopping(false);
	std::atomic<unsigned int> g_next_id(1);
	bool g_curl_initialized = false;

	size_t WriteResponse(char *data, size_t size, size_t count, void *context)
	{
		HttpResult *result = static_cast<HttpResult *>(context);
		if (size != 0 && count > static_cast<size_t>(-1) / size)
			return 0;

		size_t length = size * count;
		if (length > kMaxResponseLength - result->body.size())
		{
			result->response_too_large = true;
			return 0;
		}

		result->body.append(data, length);
		return length;
	}

	int TransferProgress(void *, curl_off_t, curl_off_t, curl_off_t, curl_off_t)
	{
		return g_stopping.load() ? 1 : 0;
	}

	void *WorkerMain(void *)
	{
		for (;;)
		{
			pthread_mutex_lock(&g_mutex);
			while (g_pending.empty() && !g_stopping.load())
				pthread_cond_wait(&g_condition, &g_mutex);

			if (g_stopping.load())
			{
				pthread_mutex_unlock(&g_mutex);
				return nullptr;
			}

			HttpRequest *request = g_pending.front();
			g_pending.pop_front();
			pthread_mutex_unlock(&g_mutex);

			HttpResult *result = new HttpResult();
			result->request = request;
			result->status = 0;
			result->error = CURLE_FAILED_INIT;
			result->response_too_large = false;

			CURL *curl = curl_easy_init();
			if (curl)
			{
				char error_buffer[CURL_ERROR_SIZE] = {};
				curl_easy_setopt(curl, CURLOPT_URL, request->url.c_str());
				curl_easy_setopt(curl, CURLOPT_NOSIGNAL, 1L);
				curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, request->timeout);
				curl_easy_setopt(curl, CURLOPT_TIMEOUT, request->timeout);
				curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 0L);
#if LIBCURL_VERSION_NUM >= 0x075500
				curl_easy_setopt(curl, CURLOPT_PROTOCOLS_STR, "http,https");
				curl_easy_setopt(curl, CURLOPT_REDIR_PROTOCOLS_STR, "http,https");
#else
				curl_easy_setopt(curl, CURLOPT_PROTOCOLS, CURLPROTO_HTTP | CURLPROTO_HTTPS);
				curl_easy_setopt(curl, CURLOPT_REDIR_PROTOCOLS, CURLPROTO_HTTP | CURLPROTO_HTTPS);
#endif
				curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 1L);
				curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 2L);
				curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteResponse);
				curl_easy_setopt(curl, CURLOPT_WRITEDATA, result);
				curl_easy_setopt(curl, CURLOPT_ERRORBUFFER, error_buffer);
				curl_easy_setopt(curl, CURLOPT_NOPROGRESS, 0L);
				curl_easy_setopt(curl, CURLOPT_XFERINFOFUNCTION, TransferProgress);

				if (request->is_post)
				{
					curl_easy_setopt(curl, CURLOPT_POST, 1L);
					curl_easy_setopt(curl, CURLOPT_POSTFIELDS, request->post.c_str());
					curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE_LARGE,
						static_cast<curl_off_t>(request->post.size()));
				}

				CURLcode code = curl_easy_perform(curl);
				result->error = result->response_too_large ? kResponseTooLarge : code;
				curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &result->status);
				curl_easy_cleanup(curl);
			}

			pthread_mutex_lock(&g_mutex);
			g_completed.push_back(result);
			pthread_mutex_unlock(&g_mutex);
		}
	}

	void FreeRequest(HttpRequest *request)
	{
		if (request->forward > 0)
			MF_UnregisterSPForward(request->forward);
		delete request;
	}

	void FreeResult(HttpResult *result)
	{
		FreeRequest(result->request);
		delete result;
	}

	void ShutdownWorkers()
	{
		g_stopping.store(true);
		pthread_mutex_lock(&g_mutex);
		g_accepting = false;
		while (!g_pending.empty())
		{
			FreeRequest(g_pending.front());
			g_pending.pop_front();
			g_outstanding--;
		}
		pthread_cond_broadcast(&g_condition);
		pthread_mutex_unlock(&g_mutex);

		for (int i = 0; i < g_workers_started; i++)
			pthread_join(g_workers[i], nullptr);
		g_workers_started = 0;

		pthread_mutex_lock(&g_mutex);
		while (!g_completed.empty())
		{
			FreeResult(g_completed.front());
			g_completed.pop_front();
			g_outstanding--;
		}
		pthread_mutex_unlock(&g_mutex);
	}

	void StartWorkers()
	{
		if (!g_curl_initialized || g_workers_started != 0)
			return;

		g_stopping.store(false);
		for (int i = 0; i < kWorkerCount; i++)
		{
			if (pthread_create(&g_workers[i], nullptr, WorkerMain, nullptr) != 0)
			{
				ShutdownWorkers();
				MF_Log("Failed to start HTTP worker thread.");
				return;
			}
			g_workers_started++;
		}

		pthread_mutex_lock(&g_mutex);
		g_accepting = true;
		pthread_mutex_unlock(&g_mutex);
	}

	cell QueueRequest(AMX *amx, cell *params, bool is_post)
	{
		int url_length = 0;
		const char *url = MF_GetAmxString(amx, params[1], 0, &url_length);
		if (url_length <= 0 || static_cast<size_t>(url_length) > kMaxUrlLength
			|| (strncmp(url, "http://", 7) != 0 && strncmp(url, "https://", 8) != 0))
		{
			MF_LogError(amx, AMX_ERR_NATIVE, "HTTP URL must be an http:// or https:// URL no longer than %d bytes.", static_cast<int>(kMaxUrlLength));
			return 0;
		}

		int body_length = 0;
		const char *body = "";
		int callback_param = 2;
		if (is_post)
		{
			body = MF_GetAmxString(amx, params[2], 1, &body_length);
			if (static_cast<size_t>(body_length) > kMaxPostLength)
			{
				MF_LogError(amx, AMX_ERR_NATIVE, "HTTP POST body exceeds %d bytes.", static_cast<int>(kMaxPostLength));
				return 0;
			}
			callback_param = 3;
		}

		int ignored_length = 0;
		const char *callback = MF_GetAmxString(amx, params[callback_param], 2, &ignored_length);
		int forward = MF_RegisterSPForwardByName(amx, callback,
			FP_CELL, FP_CELL, FP_CELL, FP_STRING, FP_CELL, FP_DONE);
		if (forward < 1)
		{
			MF_LogError(amx, AMX_ERR_NATIVE, "HTTP callback function not found: %s", callback);
			return 0;
		}

		int data_param = callback_param + 1;
		int timeout_param = callback_param + 2;
		int timeout = params[timeout_param];
		if (timeout <= 0)
			timeout = kDefaultTimeout;
		if (timeout > kMaxTimeout)
			timeout = kMaxTimeout;

		HttpRequest *request = new HttpRequest();
		request->id = g_next_id.fetch_add(1);
		if (request->id == 0)
			request->id = g_next_id.fetch_add(1);
		request->forward = forward;
		request->data = params[data_param];
		request->url.assign(url, url_length);
		request->post.assign(body, body_length);
		request->timeout = timeout;
		request->is_post = is_post;

		pthread_mutex_lock(&g_mutex);
		if (!g_accepting || g_outstanding >= kMaxQueuedRequests)
		{
			pthread_mutex_unlock(&g_mutex);
			FreeRequest(request);
			MF_LogError(amx, AMX_ERR_NATIVE, "HTTP request queue is full or shutting down.");
			return 0;
		}
		g_pending.push_back(request);
		g_outstanding++;
		pthread_cond_signal(&g_condition);
		pthread_mutex_unlock(&g_mutex);

		return static_cast<cell>(request->id);
	}

	cell AMX_NATIVE_CALL http_get(AMX *amx, cell *params)
	{
		return QueueRequest(amx, params, false);
	}

	cell AMX_NATIVE_CALL http_post(AMX *amx, cell *params)
	{
		return QueueRequest(amx, params, true);
	}

	AMX_NATIVE_INFO g_natives[] =
	{
		{ "http_get", http_get },
		{ "http_post", http_post },
		{ nullptr, nullptr }
	};
}

void OnAmxxAttach()
{
	CURLcode result = curl_global_init(CURL_GLOBAL_DEFAULT);
	if (result != CURLE_OK)
	{
		MF_Log("libcurl initialization failed: %s", curl_easy_strerror(result));
	}
	else
	{
		g_curl_initialized = true;
	}
	MF_AddNatives(g_natives);
	StartWorkers();
}

void OnPluginsLoaded()
{
	StartWorkers();
}

void OnPluginsUnloading()
{
	ShutdownWorkers();
}

void StartFrame()
{
	for (;;)
	{
		pthread_mutex_lock(&g_mutex);
		if (g_completed.empty())
		{
			pthread_mutex_unlock(&g_mutex);
			break;
		}
		HttpResult *result = g_completed.front();
		g_completed.pop_front();
		g_outstanding--;
		pthread_mutex_unlock(&g_mutex);

		MF_ExecuteForward(result->request->forward,
			static_cast<cell>(result->request->id),
			static_cast<cell>(result->status),
			static_cast<cell>(result->error),
			result->body.c_str(),
			result->request->data);
		FreeResult(result);
	}

	RETURN_META(MRES_IGNORED);
}

void OnAmxxDetach()
{
	ShutdownWorkers();
	if (g_curl_initialized)
	{
		curl_global_cleanup();
		g_curl_initialized = false;
	}
}
