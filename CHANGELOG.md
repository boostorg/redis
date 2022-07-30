# Changelog

## v0.2.2

* `connection::async_read_push` has been renamed to `async_receive`.

* The `aedis` directory has been moved to `include` to look more
  similar to Boost libraries. Users should now replace `-I/aedis-path`
  with `-I/aedis-path/include` to the compiler flags.

* Adds experimental `sync_wrapper`. This class offers a thread-safe
  and blocking API on top of the connection `class`. It is meant to
  satisfy users that can't make their code asynchronous.

* Fixes a bug in the `connection::async_exec(host, port)` overload
  that was causing crashes on reconnect.

* Fixes the executor usage in the connection class. Before theses
  changes it was only supporting `any_io_executor`.

* Fixes build in clang the compilers.

* Many simplifications in the `chat_room` example.

* Makes make improvements in the documentation.

##v0.2.1

* Bugfixes and improvements in the documentation.
