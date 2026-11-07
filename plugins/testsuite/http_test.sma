#include <amxmodx>
#include <amxmisc>
#include <http>

public plugin_init()
{
	register_plugin("HTTP Module Test", AMXX_VERSION_STR, "AMXX Dev Team")
	register_concmd("amxx_http_test", "CommandHttpTest", ADMIN_RCON, "<^"http(s) URL^">")
}

public CommandHttpTest(id, level, cid)
{
	if (!cmd_access(id, level, cid, 2))
		return PLUGIN_HANDLED

	new url[2049]
	read_argv(1, url, charsmax(url))

	new request_id = http_get(url, "HttpTestComplete", id)
	if (!request_id)
	{
		console_print(id, "[HTTP] Request was rejected.")
		return PLUGIN_HANDLED
	}

	console_print(id, "[HTTP] Queued request %d.", request_id)
	return PLUGIN_HANDLED
}

public HttpTestComplete(request_id, status, error, const body[], data)
{
	new preview[256]
	copy(preview, charsmax(preview), body)
	server_print("[HTTP] Request %d completed: HTTP %d, curl error %d, body: %s",
		request_id, status, error, preview)

	if (data > 0 && is_user_connected(data))
		client_print(data, print_console, "[HTTP] Request %d completed: HTTP %d, curl error %d",
			request_id, status, error)
}
