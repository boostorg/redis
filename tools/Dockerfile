FROM redis:alpine

COPY tls/ /tls/

CMD [ "redis-server", \
      "--tls-port", "6380", \
      "--tls-cert-file", "/tls/server.crt", \
      "--tls-key-file", "/tls/server-key.key", \
      "--tls-ca-cert-file", "/tls/ca.crt", \
      "--tls-auth-clients", "no" \
    ]
