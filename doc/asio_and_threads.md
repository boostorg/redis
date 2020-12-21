A question the arises frequently is how to do multi-threading properly
with ASIO. They can all be classified according to whether
io_context::run() is called from multiple or a single thread.

## Calling io_context::run() from one thread.

This case has a special case a single threaded app.

## Calling io_context::run() from multiple threads.


