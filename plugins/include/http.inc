/**
 * Asynchronous Linux HTTP(S) requests using libcurl.
 *
 * Requests are limited to HTTP/HTTPS, use verified TLS certificates, and
 * return callbacks on the game thread. Response bodies are limited to 32 KiB.
 */

#if defined _http_included
	#endinput
#endif
#define _http_included

#define HTTP_ERROR_RESPONSE_TOO_LARGE (-1)

/**
 * Queues an HTTP GET request.
 *
 * @param url       HTTP or HTTPS URL (maximum 2048 bytes).
 * @param callback  Callback name: callback(request_id, status, error, body[], data).
 * @param data      User cell passed to the callback.
 * @param timeout   Timeout in seconds (default 10, maximum 30).
 *
 * @return Request ID, or 0 if rejected.
 */
native http_get(const url[], const callback[], data = 0, timeout = 10);

/**
 * Queues an application/x-www-form-urlencoded HTTP POST request.
 *
 * @param url       HTTP or HTTPS URL (maximum 2048 bytes).
 * @param body      POST body (maximum 16384 bytes).
 * @param callback  Callback name: callback(request_id, status, error, body[], data).
 * @param data      User cell passed to the callback.
 * @param timeout   Timeout in seconds (default 10, maximum 30).
 *
 * @return Request ID, or 0 if rejected.
 */
native http_post(const url[], const body[], const callback[], data = 0, timeout = 10);

/**
 * @param request_id Request ID returned by http_get/http_post.
 * @param status     HTTP response status code, or 0 if unavailable.
 * @param error      libcurl error code; 0 means the transfer succeeded.
 *                   HTTP_ERROR_RESPONSE_TOO_LARGE is returned if the response
 *                   exceeded the module's 32 KiB limit.
 * @param body       Response body, truncated only when the size limit is hit.
 * @param data       User cell supplied to the request.
 */
forward Http_OnComplete(request_id, status, error, const body[], data);

/**
 * Percent-encodes a value for a URL query parameter.
 * Letters, digits, and -_.~ are left unchanged.
 *
 * @param input       Value to encode.
 * @param output      Destination buffer.
 * @param output_max  Maximum writable index, as returned by charsmax().
 *
 * @return True on success, false if the output buffer is too small.
 */
stock bool:http_urlencode(const input[], output[], output_max)
{
	static const hex[] = "0123456789ABCDEF";
	new out_pos;

	for (new i = 0; input[i]; i++)
	{
		new ch = input[i];
		if ((ch >= 'A' && ch <= 'Z')
			|| (ch >= 'a' && ch <= 'z')
			|| (ch >= '0' && ch <= '9')
			|| ch == '-' || ch == '_' || ch == '.' || ch == '~')
		{
			if (out_pos >= output_max)
			{
				output[0] = 0;
				return false;
			}
			output[out_pos++] = ch;
		}
		else
		{
			if (out_pos + 3 > output_max)
			{
				output[0] = 0;
				return false;
			}
			output[out_pos++] = '%';
			output[out_pos++] = hex[(ch >> 4) & 0x0F];
			output[out_pos++] = hex[ch & 0x0F];
		}
	}

	output[out_pos] = 0;
	return true;
}
