# Changelog

## master

* Moves from `boost::optional` to `std::optional`. This is part of
  moving to C++17.

* Changes the behaviour of the second `connection::async_run` overload
  so that it also always return an error when the connection is lost.

* Adds TLS support, see intro_tls.cpp for an example on how to use it.

* Adds example on how to resolve addresses over sentinels, see
  subscriber_sentinel.cpp.

* Adds `endpoint` where in addition to host and port, users can also
  optionally provide username and password that are needed to connect
  to the Redis server and the expected server role (see
  `error::unexpected_server_role`).

* `connection::async_run` checks whether the server role received in
  the hello command is equal to the expected server role specified in
  the the `aedis::endpoint`. To skip this check let the role variable
  empty.

* Removes reconnect functionanlity from the `connection` class. It is
  possible in simple reconnection strategies but bloats the
  `connection` class in more complex scenarios, for example, with
  sentinel, authentication and TLS. This is trivial to implement
  separated from the class. As a result the enum `event` and
  `async_receive_event` have been removed from the class too.

* Fixes a bug in `connection::async_receive_push` that prevented
  passing any response adapter other that `adapt(std::vector<node>)`.

* Changes the behaviour of `aedis::adapt()` that caused RESP3 errors
  to be ignored. One consequence of it is that `connection::async_run`
  would not exit with failure in servers that required authentication.

* Changes the behaviour of `connection::async_run` that would cause it
  to complete with success when an error in the
  `connection::async_exec` occurred.

* Moves the buildsystem from autotools to CMake.

## v1.0.0

* Adds experimental cmake support for windows users.

* Adds new class `aedis::sync` that wraps an `aedis::connection` in
  a thread-safe and synchronous API.  All free functions from the
  `sync.hpp` are now member functions of `aedis::sync`.

* Split `aedis::connection::async_receive_event` in two functions, one
  to receive events and another for server side pushes, see
  `aedis::connection::async_receive_push`.

* Removes collision between `aedis::adapter::adapt` and
  `aedis::adapt`.

* Adds `connection::operation` enum to replace `cancel_*` member
  functions with a single cancel function that gets the operations
  that should be cancelled as argument.

* Bugfix: a bug on reconnect from a state where the `connection` object
  had unsent commands. It could cause `async_exec` to never
  complete under certain conditions.

* Bugfix: Documentation of `adapt()` functions were missing from
  Doxygen.

## v0.3.0

* Adds `experimental::exec` and `receive_event` functions to offer a
  thread safe and synchronous way of executing requests across
  threads. See `intro_sync.cpp` and `subscriber_sync.cpp` for
  examples.

* `connection::async_read_push` was renamed to `async_receive_event`.

* `connection::async_receive_event` is now being used to communicate
  internal events to the user, such as resolve, connect, push etc. For
  examples see subscriber.cpp and `connection::event`.

* The `aedis` directory has been moved to `include` to look more
  similar to Boost libraries. Users should now replace `-I/aedis-path`
  with `-I/aedis-path/include` in the compiler flags.

* The `AUTH` and `HELLO` commands are now sent automatically. This change was
  necessary to implement reconnection. The username and password
  used in `AUTH` should be provided by the user on
  `connection::config`.

* Adds support for reconnection. See `connection::enable_reconnect`.

* Fixes a bug in the `connection::async_run(host, port)` overload
  that was causing crashes on reconnection.

* Fixes the executor usage in the connection class. Before theses
  changes it was imposing `any_io_executor` on users.

* `connection::async_receiver_event` is not cancelled anymore when
  `connection::async_run` exits. This change makes user code simpler.

* `connection::async_exec` with host and port overload has been
  removed. Use the other `connection::async_run` overload.

* The host and port parameters from `connection::async_run` have been
  move to `connection::config` to better support authentication and
  failover.

* Many simplifications in the `chat_room` example.

* Fixes build in clang the compilers and makes some improvements in
  the documentation.

## v0.2.1

* Fixes a bug that happens on very high load.

## v0.2.0

* Major rewrite of the high-level API. There is no more need to use the low-level API anymore.
* No more callbacks: Sending requests follows the ASIO asynchronous model.
* Support for reconnection: Pending requests are not canceled when a connection is lost and are re-sent when a new one is established.
* The library is not sending HELLO-3 on user behalf anymore. This is important to support AUTH properly.

## v0.1.2

* Adds reconnect coroutine in the `echo_server` example.
* Corrects `client::async_wait_for_data` with `make_parallel_group` to launch operation.
* Improvements in the documentation.
* Avoids dynamic memory allocation in the client class after reconnection.

## v0.1.1

* Improves the documentation and adds some features to the high-level client.

## v0.1.0

* Improvements in the design and documentation.

## v0.0.1

* First release to collect design feedback.
