/* Copyright (c) 2018-2022 Marcelo Zimbres Silva (mzimbres@gmail.com)
 *
 * Distributed under the Boost Software License, Version 1.0. (See
 * accompanying file LICENSE.txt)
 */

#include <boost/redis/connection.hpp>

#include <boost/asio/ssl/host_name_verification.hpp>
#include <boost/system/error_code.hpp>

#include <cstddef>
#include <string_view>
#define BOOST_TEST_MODULE conn_tls
#include <boost/test/included/unit_test.hpp>

#include "common.hpp"

namespace net = boost::asio;

using boost::redis::connection;
using boost::redis::request;
using boost::redis::response;
using boost::redis::config;
using boost::system::error_code;

namespace {

// CA certificate that signed the test server's certificate.
// This is a self-signed CA created for testing purposes.
// This must match tools/tls/ca.crt contents
static constexpr const char* ca_certificate = R"%(-----BEGIN CERTIFICATE-----
MIIDhzCCAm+gAwIBAgIUZGttu4o/Exs08EHCneeD3gHw7KkwDQYJKoZIhvcNAQEL
BQAwUjELMAkGA1UEBhMCRVMxGjAYBgNVBAoMEUJvb3N0LlJlZGlzIENJIENBMQsw
CQYDVQQLDAJJVDEaMBgGA1UEAwwRYm9vc3QtcmVkaXMtY2ktY2EwIBcNMjUwNjA3
MTI0NzUwWhgPMjA4MDAzMTAxMjQ3NTBaMFIxCzAJBgNVBAYTAkVTMRowGAYDVQQK
DBFCb29zdC5SZWRpcyBDSSBDQTELMAkGA1UECwwCSVQxGjAYBgNVBAMMEWJvb3N0
LXJlZGlzLWNpLWNhMIIBIjANBgkqhkiG9w0BAQEFAAOCAQ8AMIIBCgKCAQEAu7XV
sOoHB2J/5VtyJmMOzxhBbHKyQgW1YnMvYIb1JqIm7VuICA831SUw76n3j8mIK3zz
FfK2eYyUWf4Uo2j3uxmXDyjujqzIaUJNLcB53CQXkmIbqDigNhzUTPZ5A2MQ7xT+
t1eDbjsZ7XIM+aTShgtrpyxiccsgPJ3/XXme2RrqKeNvYsTYY6pquWZdyLOg/LOH
IeSJyL1/eQDRu/GsZjnR8UOE6uHfbjrLWls7Tifj/1IueVYCEhQZpJSWS8aUMLBZ
fi+t9YMCCK4DGy+6QlznGgVqdFFbTUt2C7tzqz+iF5dxJ8ogKMUPEeFrWiZpozoS
t60jV8fKwdXz854jLQIDAQABo1MwUTAdBgNVHQ4EFgQU2SoWvvZUW8JiDXtyuXZK
deaYYBswHwYDVR0jBBgwFoAU2SoWvvZUW8JiDXtyuXZKdeaYYBswDwYDVR0TAQH/
BAUwAwEB/zANBgkqhkiG9w0BAQsFAAOCAQEAqY4hGcdCFFPL4zveSDhR9H/akjae
uXbpo/9sHZd8e3Y4BtD8K05xa3417H9u5+S2XtyLQg5MON6J2LZueQEtE3wiR3ja
QIWbizqp8W54O5hTLQs6U/mWggfuL2R/HUw7ab4M8JobwHNEMK/WKZW71z0So/kk
W3wC0+1RH2PjMOZrCIflsD7EXYKIIr9afypAbhCQmCfu/GELuNx+LmaPi5JP4TTE
tDdhzWL04JLcZnA0uXb2Mren1AR9yKYH2I5tg5kQ3Bn/6v9+JiUhiejP3Vcbw84D
yFwRzN54bLanrJNILJhHPwnNIABXOtGUV05SZbYazJpiMst1a6eqDZhv/Q==
-----END CERTIFICATE-----)%";

static config make_tls_config()
{
   config cfg;
   cfg.use_ssl = true;
   cfg.addr.host = get_server_hostname();
   cfg.addr.port = "6380";
   return cfg;
}

// Using the default TLS context allows establishing TLS connections and execute requests
BOOST_AUTO_TEST_CASE(exec_default_ssl_context)
{
   auto const cfg = make_tls_config();
   constexpr std::string_view ping_value = "Kabuf";

   request req;
   req.push("PING", ping_value);

   response<std::string> resp;

   net::io_context ioc;
   connection conn{ioc};

   // The custom server uses a certificate signed by a CA
   // that is not trusted by default - skip verification.
   conn.next_layer().set_verify_mode(net::ssl::verify_none);

   bool exec_finished = false, run_finished = false;

   conn.async_exec(req, resp, [&](error_code ec, std::size_t) {
      exec_finished = true;
      BOOST_TEST(ec == error_code());
      conn.cancel();
   });

   conn.async_run(cfg, {}, [&](error_code ec) {
      run_finished = true;
      BOOST_TEST(ec == net::error::operation_aborted);
   });

   ioc.run_for(test_timeout);

   BOOST_TEST(exec_finished);
   BOOST_TEST(run_finished);
   BOOST_TEST(std::get<0>(resp).value() == ping_value);
}

// Users can pass a custom context with TLS config
BOOST_AUTO_TEST_CASE(exec_custom_ssl_context)
{
   auto const cfg = make_tls_config();
   constexpr std::string_view ping_value = "Kabuf";

   request req;
   req.push("PING", ping_value);

   response<std::string> resp;

   net::io_context ioc;
   net::ssl::context ctx{net::ssl::context::tls_client};

   // Configure the SSL context to trust the CA that signed the server's certificate.
   // The test certificate uses "redis" as its common name, regardless of the actual server's hostname
   ctx.add_certificate_authority(net::const_buffer(ca_certificate, std::strlen(ca_certificate)));
   ctx.set_verify_mode(net::ssl::verify_peer);
   ctx.set_verify_callback(net::ssl::host_name_verification("redis"));

   connection conn{ioc, std::move(ctx)};

   bool exec_finished = false, run_finished = false;

   conn.async_exec(req, resp, [&](error_code ec, std::size_t) {
      exec_finished = true;
      BOOST_TEST(ec == error_code());
      conn.cancel();
   });

   conn.async_run(cfg, {}, [&](error_code ec) {
      run_finished = true;
      BOOST_TEST(ec == net::error::operation_aborted);
   });

   ioc.run_for(test_timeout);

   BOOST_TEST(exec_finished);
   BOOST_TEST(run_finished);
   BOOST_TEST(std::get<0>(resp).value() == ping_value);
}

}  // namespace