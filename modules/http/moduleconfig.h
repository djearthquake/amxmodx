// vim: set sts=2 ts=8 sw=2 tw=99 et ft=c:

#ifndef __MODULECONFIG_H__
#define __MODULECONFIG_H__

#include <amxmodx_version.h>

#define MODULE_NAME "HTTP"
#define MODULE_VERSION AMXX_VERSION
#define MODULE_AUTHOR "SPiNX"
#define MODULE_URL "https://github.com/djearthquake/proxysnort"
#define MODULE_LOGTAG "HTTP"
#define MODULE_LIBRARY "http"
#define MODULE_LIBCLASS ""

#ifdef __DATE__
#define MODULE_DATE __DATE__
#else
#define MODULE_DATE "Unknown"
#endif

#define USE_METAMOD
#define FN_AMXX_ATTACH OnAmxxAttach
#define FN_AMXX_DETACH OnAmxxDetach
#define FN_AMXX_PLUGINSLOADED OnPluginsLoaded
#define FN_AMXX_PLUGINSUNLOADING OnPluginsUnloading
#define FN_StartFrame StartFrame

#endif
