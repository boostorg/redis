# Changelog

## v0.2.2

* `connection::async_read_push` has been renamed to `async_receive`.

* The `aedis` directory has been moved to `include` to look more
  similar to Boost libraries. Users should now replace `-I/aedis-path`
  with `-I/aedis-path/include` in the compiler flags.

* Adds `experimental::exec` functions to offer a thread safe and
  synchronous way of executing requests. See `intro_sync.cpp` and
  `subscriber_sync.cpp` for an example.

* Fixes a bug in the `connection::async_exec(host, port)` overload
  that was causing crashes on reconnect.

* Fixes the executor usage in the connection class. Before theses
  changes it was only supporting `any_io_executor`.

* `connection::async_receiver` is not cancelled anymore when
  `connection::async_run` exits. This change simplifies the
  implementation failover operations.

* `connection::async_exec` with host and port overload has been
  removed. Use the net `connection::async_run` overload.

* The host and port parameters from `connection::async_run` have been
  move to `connection::config` to better support authentication and
  failover.

* Many simplifications in the `chat_room` example.

* Fixes build in clang the compilers and makes some improvements in the documentation.

##v0.2.1

* Bugfixes and improvements in the documentation.
