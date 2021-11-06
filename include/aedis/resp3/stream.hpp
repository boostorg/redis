/* Copyright (c) 2019 - 2021 Marcelo Zimbres Silva (mzimbres at gmail dot com)
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#pragma once

#include <aedis/net.hpp>

#include <aedis/resp3/request.hpp>
#include <aedis/resp3/type.hpp>
#include <aedis/resp3/response.hpp>
#include <aedis/resp3/detail/read.hpp>

namespace aedis {
namespace resp3 {

/** Reads and writes redis commands.
 */
template <class NextLayer>
class stream {
public:
   /// The type of the next layer.
   using next_layer_type = typename std::remove_reference<NextLayer>::type;

   /// The type of the executor associated with the object.
   using executor_type = typename next_layer_type::executor_type;

private:
   using buffer_type = std::string;
   buffer_type buffer_;
   net::coroutine coro_ = net::coroutine();
   type type_ = type::invalid;
   next_layer_type next_layer_;

public:
   template <class Arg>
   explicit stream(Arg&& arg)
   : next_layer_(std::forward<Arg>(arg))
   { }

   stream(stream&& other) = default;
   stream& operator=(stream&& other) = delete;

   /// Get the executor associated with the object.
   /**
    * This function may be used to obtain the executor object that the stream
    * uses to dispatch handlers for asynchronous operations.
    *
    * @return A copy of the executor that stream will use to dispatch handlers.
    */
   executor_type get_executor() noexcept
      { return next_layer_.get_executor(); }

   /// Get a reference to the next layer.
   /**
    * This function returns a reference to the next layer in a stack of
    * stream layers.
    *
    * @return A reference to the next layer in the stack of stream
    * layers.  Ownership is not transferred to the caller.
    */
   next_layer_type const& next_layer() const
      { return next_layer_; }

   /// Get a reference to the next layer.
   /**
    * This function returns a reference to the next layer in a stack
    * of stream layers.
    *
    * @return A reference to the next layer in the stack of stream
    * layers.  Ownership is not transferred to the caller.
    */
   next_layer_type& next_layer()
      { return next_layer_; }

   /// Writes and reads requests.
   /** Performs the following operations
    *
    *  1. Write one or more requests in the queue (see async_write_some)
    *  2. Reads the responses for each command in the request
    *     individually, returning control to the users.
    *
    *  When there is no more requests to be written it will wait on a
    *  read.
    */
   template<class CompletionToken = net::default_completion_token_t<executor_type>>
   auto async_consume(
      std::queue<request>& requests,
      response& resp,
      CompletionToken&& token = net::default_completion_token_t<executor_type>{})
   {
     return net::async_compose<
	CompletionToken, void(boost::system::error_code, type)>(
	   detail::consumer_op {next_layer_, buffer_, requests, resp, type_, coro_},
	   token, next_layer_);
   }

   /** @brief Writes one or more requests to the stream.
    *
    * Sends the last request in the input queue to the server. If the
    * next request happens to contain commands the have a push type as
    * a response (see subscribe) they will also be sent.
    */
   template<class CompletionToken = net::default_completion_token_t<executor_type>>
   auto async_write_some(
      std::queue<request>& requests,
      CompletionToken&& token = net::default_completion_token_t<executor_type>{})
   {
     return net::async_compose<
	CompletionToken,
	void(boost::system::error_code)>(
	   detail::write_some_op{next_layer_, requests},
	   token, next_layer_);
   }

   /** @brief Reads one command from the redis response
    *
    *  Note: This function has to be called once for each command.
    */
   template <class CompletionToken = net::default_completion_token_t<executor_type>>
   auto async_read(
      response& resp,
      CompletionToken&& token = net::default_completion_token_t<executor_type>{})
   {
      return net::async_compose
         < CompletionToken
         , void(boost::system::error_code)
         >(detail::parse_op<NextLayer, buffer_type> {next_layer_, &buffer_, &resp},
           token, next_layer_);
   }

   template <class CompletionToken = net::default_completion_token_t<executor_type>>
   auto async_write(
      request& req,
      CompletionToken&& token = net::default_completion_token_t<executor_type>{})
   {
      return net::async_write(next_layer_, net::buffer(req.payload), token);
   }
};

} // resp3
} // aedis
