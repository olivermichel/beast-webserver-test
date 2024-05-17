# Simple Webapp using Boost.Beast

* Generate certificate:
```
openssl req -x509 -newkey rsa:4096 -keyout key.pem -out cert.pem -days 365 -nodes \
    -subj "/C=US/ST=NJ/O=Princeton University/OU=COS/CN=localhost" \
    -addext "subjectAltName=IP:127.0.0.1"
```