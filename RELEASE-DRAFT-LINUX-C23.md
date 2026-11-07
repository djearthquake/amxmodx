# AMX Mod X 1.10.0-ss - Linux C23 release draft

Draft notes for a future GitHub release. Do not publish until the release is
reviewed and approved.

## Summary

Linux 32-bit AMX Mod X `1.10.0-ss` build from the modernized source tree,
with optimized compiler settings and a fix for a JIT crash that occurred when
plugins were loaded during map changes.

## Highlights

- Fixes the Linux x86 JIT plugin-loading crash caused by treating the AMX data
  segment offset as an absolute address.
- Verified on a LAN Counter-Strike server: the `starwars` map loaded with all
  19 stock plugins running.
- Builds Linux C and C++ sources with the newest standards supported by the
  selected GCC or Clang toolchain.
- Includes the PostgreSQL SQLX module, built when 32-bit `libpq` development
  files are available. The module is disabled by default.
- Includes `admin_sql.amxx` with PostgreSQL SQLX dialect support; live database
  integration remains to be tested.
- Includes an optional Linux-only libcurl HTTP(S) module for asynchronous GET
  and POST requests, with verified TLS, bounded responses, timeouts, and
  callbacks on the game thread. The module compiles, but runtime loading and
  live HTTPS tests remain to be validated.
- Includes the existing sockets nonblocking-connect fix.
- Packages the `base`, `cstrike`, `dod`, `esf`, `ns`, `tfc`, and `ts` trees.

## PostgreSQL admin setup

The package includes the SQL-backed admin plugin as `admin_sql.amxx`; the
default `admin.amxx` continues to use `users.ini`. To use PostgreSQL, install
the 32-bit `libpq.so.5` runtime and dependencies, enable `pgsql` in
`configs/modules.ini`, set `amx_sql_type "pgsql"` and the connection settings
in `configs/sql.cfg`, then enable `admin_sql.amxx` and disable `admin.amxx` in
`configs/plugins.ini`. Do not load both admin plugins together. The SQL plugin
creates its admin table if needed. Validate connectivity and admin loading
against a live PostgreSQL server before production use.

## Validation and limitations

- The AMXX runtime and package were rebuilt after the JIT fix.
- Packaged shared objects were checked for `DT_TEXTREL`.
- PostgreSQL module compilation and the module availability plugin were
  checked, but a live PostgreSQL server/database integration test has not been
  performed.
- The PostgreSQL module requires a 32-bit `libpq.so.5` runtime and its
  dependencies on the game server. Keep it disabled unless those dependencies
  are installed.
- This archive contains Linux 32-bit binaries; it is not a Windows or macOS
  release.

## Assets

- `amxx-linux-c23-packages.tar.gz` - all seven Linux package trees.
- `BUILDING-LINUX-C23.md` - build instructions, dependencies, and installation
  examples.

## Installation

See `BUILDING-LINUX-C23.md`. Back up the existing AMXX installation before
replacing files.
