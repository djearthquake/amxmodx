# Building the Linux C23 AMX Mod X package

This checkout's optimized Linux build is configured in `build-linux-c23-opt`.
The build produces 32-bit AMX Mod X binaries and modules, including the
PostgreSQL SQLX module when 32-bit `libpq` development files are available,
and the asynchronous HTTP(S) module when 32-bit libcurl development files are
available.
The sockets module is built from `modules/sockets/sockets.cpp` in this checkout,
including the current nonblocking-connect fix.

## PostgreSQL build dependency

Install the i386 PostgreSQL development package and `pkg-config` on a Debian
system with i386 architecture enabled:

```bash
sudo dpkg --add-architecture i386
sudo apt-get update
sudo apt-get install pkg-config libpq-dev:i386
```

AMBuild detects `libpq` through `pkg-config`. If the development files are
installed somewhere nonstandard, set `PKG_CONFIG_PATH` to the directory
containing `libpq.pc` before building.

## HTTP(S) module dependency

The HTTP module is Linux-only and uses libcurl. On Debian, install the 32-bit
development package and `pkg-config`:

```bash
sudo dpkg --add-architecture i386
sudo apt-get update
sudo apt-get install pkg-config libcurl4-openssl-dev:i386
```

AMBuild skips the HTTP module if it cannot find libcurl through `pkg-config`.
The server needs the matching 32-bit libcurl runtime and its shared-library
dependencies to load `http_amxx_i386.so`.

If using an extracted SDK instead of system-installed development files, set
`SDK` to its location (for example, under your home directory):

```bash
SDK="$HOME/.cache/amxx-libpq-i386"
export PKG_CONFIG_PATH="$SDK/usr/lib/i386-linux-gnu/pkgconfig"
export PKG_CONFIG_SYSROOT_DIR="$SDK"
```

## Build this configured tree

Run from any directory; the paths below target the existing Downloads checkout:

```bash
SRC="$HOME/Downloads/amxx_source/amxmodx"
if [ -n "${SDK:-}" ]; then
  export PKG_CONFIG_PATH="$SDK/usr/lib/i386-linux-gnu/pkgconfig"
  export PKG_CONFIG_SYSROOT_DIR="$SDK"
fi
pkg-config --exists libpq || {
  echo "libpq development files are required; see PostgreSQL build dependency above."
  exit 1
}
cd "$SRC" || exit 1
PYENV_VERSION=3.6.3 PYTHONPATH="$SRC/ambuild" \
  python3.6 -c 'from ambuild2.run import cli_run; cli_run()' \
  -j4 build-linux-c23-opt
```

AMBuild reuses current outputs and rebuilds changed targets. This source tree's
build script selects C23 for supported modern GCC and Clang versions.

## JIT fix and server update

This build includes a fix for a Linux x86 JIT crash when loading plugins. The
JIT was using the AMX data-segment offset as an absolute source address while
copying plugin data. The fixed build adds the AMX header base to that offset.
It was verified on a LAN Counter-Strike server: the `starwars` map loaded with
all 19 stock plugins running.

To update an existing Counter-Strike server, stop it first, copy the archive to
the server, then run these commands from its `cstrike` directory:

```bash
ARCHIVE="$HOME/amxx-linux-c23-packages.tar.gz"
AMXX="addons/amxmodx/dlls/amxmodx_mm_i386.so"
cp -a "$AMXX" "$AMXX.bak.$(date +%Y%m%d-%H%M%S)"
tar -xzf "$ARCHIVE" -C "$PWD" --strip-components=1 \
  base/addons/amxmodx/dlls/amxmodx_mm_i386.so
```

Restart the server after replacing the binary. This updates only the AMXX core;
back up the complete `addons/amxmodx` directory before installing the full
package.

## Outputs

The base Linux package is assembled under:

```text
build-linux-c23-opt/packages/base/addons/amxmodx/
```

The PostgreSQL module is `modules/pgsql_amxx_i386.so`. The PostgreSQL
availability test plugin is `plugins/pgsql_module_test.amxx`, with its source
under `scripting/testsuite/pgsql_module_test.sma`. The plugin is packaged for
testing but is not enabled by default. To run it on a server, install the
32-bit `libpq.so.5` runtime and its dependencies, enable `pgsql` in
`configs/modules.ini`, and add `pgsql_module_test.amxx` to `configs/plugins.ini`.

The SQL-backed admin plugin is also built as `plugins/admin_sql.amxx`. To use
PostgreSQL for admin records, enable `pgsql` in `configs/modules.ini`, set
`amx_sql_type "pgsql"` and the connection settings in `configs/sql.cfg`, and
replace `admin.amxx` with `admin_sql.amxx` in `configs/plugins.ini`. Do not
enable both admin plugins at once. The admin SQL plugin creates its table if
needed. PostgreSQL runtime behavior still needs a live database test.

The sockets module is `modules/sockets_amxx_i386.so`. Other game-specific
package trees (such as `cstrike` and `dod`) are generated alongside `base`.
The base package includes the compiler's scripting include files.
The optional `http_amxx_i386.so` module provides async HTTP GET/POST over
HTTP/HTTPS with certificate verification, a bounded response body, and
callbacks on the game thread. See `scripting/include/http.inc` and
`scripting/testsuite/http_test.sma`. Enable `http` in `configs/modules.ini`
to load it. The test plugin provides an RCON-only `amxx_http_test "<URL>"`
command; quote the URL so the game console passes it intact. It is packaged
for manual testing and is not enabled by default.

## Install on a Condition Zero server

Back up the existing `czero/addons/amxmodx` directory first. From the server
root, extract the generic base files and Counter-Strike files into `czero`:

```bash
ARCHIVE=/path/to/amxx-linux-c23-packages.tar.gz
tar -xzf "$ARCHIVE" -C czero --strip-components=1 base
tar -xzf "$ARCHIVE" -C czero --strip-components=1 cstrike
```

The archive paths then land under `czero/addons/amxmodx/`. PostgreSQL remains
disabled by default; enable it in `czero/addons/amxmodx/configs/modules.ini`
only after installing the 32-bit `libpq.so.5` runtime and its dependencies.

To bundle all generated package trees into one archive after a successful build:

```bash
cd "$SRC/build-linux-c23-opt" || exit 1
tar -czf amxx-linux-c23-packages.tar.gz -C packages \
  base cstrike dod esf ns tfc ts
```
