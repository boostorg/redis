# Changelog

## v0.2.2

* `connection::async_read_push` has been renamed to `async_receive`.

* The `aedis` directory has been moved to `include` to look more
  similar to Boost libraries. Users should now replace `-I/aedis-path`
  with `-I/aedis-path/include` in the compiler flags.

* Adds `experimental::exec` functions to offer a thread safe and
  synchronous way of executing requests. See `intro_sync.cpp` for and
  example.

* Fixes a bug in the `connection::async_exec(host, port)` overload
  that was causing crashes on reconnect.

* Fixes the executor usage in the connection class. Before theses
  changes it was only supporting `any_io_executor`.

* Fixes build in clang the compilers.

* Many simplifications in the `chat_room` example.

* Makes make improvements in the documentation.

##v0.2.1

* Bugfixes and improvements in the documentation.
