# Python Requests Library - Complete API Reference

This document provides an exhaustive inventory of the Python Requests library API surface (v2.32.x) for the purpose of implementing equivalent functionality in C++.

---

## Table of Contents

1. [Top-Level API Functions](#top-level-api-functions)
2. [Session Class](#session-class)
3. [Request Class](#request-class)
4. [PreparedRequest Class](#preparedrequest-class)
5. [Response Class](#response-class)
6. [Exceptions](#exceptions)
7. [Authentication Classes](#authentication-classes)
8. [Adapters](#adapters)
9. [Cookies](#cookies)
10. [Structures](#structures)
11. [Status Codes](#status-codes)
12. [Hooks](#hooks)
13. [Utility Functions](#utility-functions)

---

## Top-Level API Functions

Module: `requests.api`

### `request(method, url, **kwargs)`

Base function for all HTTP requests.

**Required Parameters:**
| Parameter | Type | Description |
|-----------|------|-------------|
| `method` | `str` | HTTP method: "GET", "POST", "PUT", "PATCH", "DELETE", "HEAD", "OPTIONS" |
| `url` | `str` | Target URL |

**Optional Keyword Arguments:**
| Parameter | Type | Description |
|-----------|------|-------------|
| `params` | `dict`, `list[tuple]`, `bytes` | URL query string parameters |
| `data` | `dict`, `list[tuple]`, `bytes`, file-like | Request body (form-encoded, binary) |
| `json` | JSON-serializable | Auto-encoded JSON body, sets Content-Type |
| `headers` | `dict` | HTTP headers |
| `cookies` | `dict`, `CookieJar` | Cookies to send |
| `files` | `dict` | Multipart file upload. Values: file-like, `(filename, fileobj)`, or 3/4-tuple with content-type/headers |
| `auth` | `tuple`, callable | Authentication handler |
| `timeout` | `float`, `(float, float)` | Timeout in seconds. Single value or (connect, read) tuple |
| `allow_redirects` | `bool` | Follow redirects (default: `True`) |
| `proxies` | `dict` | Protocol-to-proxy URL mapping |
| `hooks` | `dict` | Event callbacks |
| `stream` | `bool` | Stream response body lazily |
| `verify` | `bool`, `str` | SSL verification. `True`, `False`, or CA bundle path |
| `cert` | `str`, `(str, str)` | Client SSL cert path or (cert, key) tuple |

**Returns:** `Response`

### Convenience Functions

All accept the same kwargs as `request()`:

| Function | Signature | Notes |
|----------|-----------|-------|
| `get(url, params=None, **kwargs)` | GET request | `params` explicitly listed |
| `post(url, data=None, json=None, **kwargs)` | POST request | `data` and `json` explicitly listed |
| `put(url, data=None, **kwargs)` | PUT request | |
| `patch(url, data=None, **kwargs)` | PATCH request | |
| `delete(url, **kwargs)` | DELETE request | |
| `head(url, **kwargs)` | HEAD request | `allow_redirects=False` by default |
| `options(url, **kwargs)` | OPTIONS request | |

---

## Session Class

Module: `requests.sessions`

Provides persistent settings, connection pooling, and cookie persistence across requests.

### Constructor

```python
Session()
```

### Instance Attributes

| Attribute | Type | Default | Description |
|-----------|------|---------|-------------|
| `headers` | `CaseInsensitiveDict` | Default UA, Accept, etc. | Default headers for all requests |
| `cookies` | `RequestsCookieJar` | Empty | Cookie jar, auto-updated from responses |
| `auth` | `tuple`, callable, `None` | `None` | Default authentication |
| `proxies` | `dict` | `{}` | Default proxy mappings |
| `hooks` | `dict` | `{'response': []}` | Default hooks |
| `params` | `dict` | `{}` | Default query parameters |
| `stream` | `bool` | `False` | Default streaming behavior |
| `verify` | `bool`, `str` | `True` | Default SSL verification |
| `cert` | `None`, `str`, `tuple` | `None` | Default client certificate |
| `max_redirects` | `int` | 30 | Maximum redirects before `TooManyRedirects` |
| `trust_env` | `bool` | `True` | Use environment for proxies, .netrc, CA paths |
| `adapters` | `dict` | HTTP/HTTPS adapters | Transport adapters by URL prefix |

### Methods

| Method | Signature | Returns | Description |
|--------|-----------|---------|-------------|
| `request` | `(method, url, params=None, data=None, headers=None, cookies=None, files=None, auth=None, timeout=None, allow_redirects=True, proxies=None, hooks=None, stream=None, verify=None, cert=None, json=None)` | `Response` | Core request method |
| `get` | `(url, **kwargs)` | `Response` | GET request |
| `post` | `(url, data=None, json=None, **kwargs)` | `Response` | POST request |
| `put` | `(url, data=None, **kwargs)` | `Response` | PUT request |
| `patch` | `(url, data=None, **kwargs)` | `Response` | PATCH request |
| `delete` | `(url, **kwargs)` | `Response` | DELETE request |
| `head` | `(url, **kwargs)` | `Response` | HEAD request |
| `options` | `(url, **kwargs)` | `Response` | OPTIONS request |
| `prepare_request` | `(request)` | `PreparedRequest` | Merge session defaults into Request |
| `send` | `(prepared_request, **kwargs)` | `Response` | Send PreparedRequest via adapter |
| `get_adapter` | `(url)` | `HTTPAdapter` | Get adapter for URL prefix |
| `mount` | `(prefix, adapter)` | `None` | Register adapter for URL prefix |
| `close` | `()` | `None` | Close all adapters and connections |

### Internal/Advanced Methods

| Method | Description |
|--------|-------------|
| `merge_environment_settings(url, proxies, stream, verify, cert)` | Merge per-request, session, and env settings |
| `rebuild_auth(prepared_request, response)` | Strip auth on cross-host redirect |
| `rebuild_proxies(prepared_request, proxies)` | Re-evaluate proxy for redirect |
| `rebuild_method(prepared_request, response)` | Change method per redirect status (e.g., POST→GET) |
| `resolve_redirects(response, request, ...)` | Generator yielding redirect responses |

### Context Manager

```python
with Session() as s:
    s.get(url)
# s.close() called automatically
```

---

## Request Class

Module: `requests.models`

User-facing request object before preparation.

### Constructor

```python
Request(method=None, url=None, headers=None, files=None, data=None,
        params=None, auth=None, cookies=None, hooks=None, json=None)
```

### Attributes

| Attribute | Type | Description |
|-----------|------|-------------|
| `method` | `str` | HTTP method |
| `url` | `str` | Target URL |
| `headers` | `dict` | Headers |
| `files` | `dict` | Files for multipart upload |
| `data` | various | Request body data |
| `params` | `dict` | Query parameters |
| `auth` | `tuple`, callable | Authentication |
| `cookies` | `dict`, `CookieJar` | Cookies |
| `hooks` | `dict` | Event hooks |
| `json` | JSON-serializable | JSON body |

### Methods

| Method | Signature | Returns | Description |
|--------|-----------|---------|-------------|
| `prepare` | `()` | `PreparedRequest` | Convert to PreparedRequest |
| `register_hook` | `(event, hook)` | `None` | Add event hook |
| `deregister_hook` | `(event, hook)` | `bool` | Remove event hook |

---

## PreparedRequest Class

Module: `requests.models`

Fully configured request ready for transmission.

### Attributes

| Attribute | Type | Description |
|-----------|------|-------------|
| `method` | `str` | HTTP method (uppercase) |
| `url` | `str` | Final URL with query params |
| `headers` | `dict` | Final headers including cookies |
| `body` | `bytes`, `str`, `None` | Encoded request body |
| `hooks` | `dict` | Event hooks |
| `_cookies` | `CookieJar` | Internal cookie store |
| `_body_position` | `int` | Stream position for retries |
| `path_url` | `str` | Path + query string portion |

### Methods

| Method | Signature | Description |
|--------|-----------|-------------|
| `prepare` | `(method=None, url=None, headers=None, files=None, data=None, params=None, auth=None, cookies=None, hooks=None, json=None)` | Full preparation |
| `prepare_method` | `(method)` | Normalize and set method |
| `prepare_url` | `(url, params)` | Combine URL and params |
| `prepare_headers` | `(headers)` | Merge headers |
| `prepare_body` | `(data, files, json=None)` | Encode body |
| `prepare_cookies` | `(cookies)` | Set Cookie header |
| `prepare_auth` | `(auth, url='')` | Apply authentication |
| `prepare_content_length` | `(body)` | Set Content-Length |
| `prepare_hooks` | `(hooks)` | Register hooks |
| `register_hook` | `(event, hook)` | Add event hook |
| `deregister_hook` | `(event, hook)` | Remove event hook |
| `copy` | `()` | Shallow copy |

**Preparation Order:** URL → headers → cookies → body → auth (auth needs final URL/body)

---

## Response Class

Module: `requests.models`

### Attributes

| Attribute | Type | Description |
|-----------|------|-------------|
| `status_code` | `int` | HTTP status code |
| `headers` | `CaseInsensitiveDict` | Response headers |
| `url` | `str` | Final URL (after redirects) |
| `text` | `str` | Decoded body using `encoding` |
| `content` | `bytes` | Raw body bytes |
| `encoding` | `str` | Character encoding (from headers or guessed) |
| `apparent_encoding` | `str` | Encoding detected from content (chardet/charset_normalizer) |
| `ok` | `bool` | `True` if `status_code < 400` |
| `reason` | `str` | HTTP reason phrase ("OK", "Not Found", etc.) |
| `cookies` | `RequestsCookieJar` | Response cookies |
| `elapsed` | `timedelta` | Time from send to response headers |
| `history` | `list[Response]` | Redirect history (oldest first) |
| `request` | `PreparedRequest` | The request that generated this response |
| `raw` | file-like | Raw socket stream (requires `stream=True`) |
| `is_redirect` | `bool` | Valid redirect with Location header |
| `is_permanent_redirect` | `bool` | 301 or 308 with Location |
| `links` | `dict` | Parsed Link header |
| `next` | `PreparedRequest`, `None` | Next request for redirects |

### Methods

| Method | Signature | Returns | Description |
|--------|-----------|---------|-------------|
| `json` | `(**kwargs)` | `dict`, `list`, etc. | Parse JSON body |
| `raise_for_status` | `()` | `None` | Raise `HTTPError` if status >= 400 |
| `iter_content` | `(chunk_size=1, decode_unicode=False)` | generator | Stream body in chunks |
| `iter_lines` | `(chunk_size=512, decode_unicode=False, delimiter=None)` | generator | Stream body line by line |
| `close` | `()` | `None` | Close connection |

---

## Exceptions

Module: `requests.exceptions`

### Exception Hierarchy

```
RequestException (base)
├── HTTPError
├── ConnectionError
│   ├── ProxyError
│   └── SSLError
├── Timeout
│   ├── ConnectTimeout (also inherits ConnectionError)
│   └── ReadTimeout
├── URLRequired
├── TooManyRedirects
├── MissingSchema (also inherits ValueError)
├── InvalidSchema (also inherits ValueError)
├── InvalidURL (also inherits ValueError)
│   └── InvalidProxyURL
├── InvalidHeader (also inherits ValueError)
├── InvalidJSONError
├── ChunkedEncodingError
├── ContentDecodingError (also inherits BaseHTTPError)
├── StreamConsumedError (also inherits TypeError)
├── RetryError
└── UnrewindableBodyError
```

### Exception Details

| Exception | When Raised |
|-----------|-------------|
| `RequestException` | Base for all requests exceptions |
| `HTTPError` | `raise_for_status()` with status >= 400 |
| `ConnectionError` | Network problems (DNS, refused, etc.) |
| `ProxyError` | Proxy server issues |
| `SSLError` | SSL/TLS certificate or handshake problems |
| `Timeout` | Request timeout (base for Connect/Read) |
| `ConnectTimeout` | Timeout during connection establishment |
| `ReadTimeout` | Timeout waiting for data |
| `URLRequired` | No URL provided |
| `TooManyRedirects` | Exceeded `max_redirects` |
| `MissingSchema` | URL missing scheme (http://, etc.) |
| `InvalidSchema` | Unsupported URL scheme |
| `InvalidURL` | Malformed URL |
| `InvalidProxyURL` | Malformed proxy URL |
| `InvalidHeader` | Invalid header value |
| `InvalidJSONError` | JSON decoding failed |
| `ChunkedEncodingError` | Invalid chunked transfer encoding |
| `ContentDecodingError` | Content decompression failed |
| `StreamConsumedError` | Content already consumed |
| `RetryError` | Retry logic exhausted |
| `UnrewindableBodyError` | Cannot rewind body for retry |

### Warnings

| Warning | Description |
|---------|-------------|
| `RequestsWarning` | Base warning class |
| `FileModeWarning` | File opened in text mode for upload |
| `RequestsDependencyWarning` | Dependency version mismatch |

---

## Authentication Classes

Module: `requests.auth`

### AuthBase

Abstract base class for authentication handlers.

```python
class AuthBase:
    def __call__(self, r):
        # Modify request r, return r
        raise NotImplementedError
```

### HTTPBasicAuth

```python
HTTPBasicAuth(username, password)
```

Adds `Authorization: Basic <base64(user:pass)>` header.

**Shorthand:** Passing `auth=(username, password)` tuple is equivalent.

### HTTPDigestAuth

```python
HTTPDigestAuth(username, password)
```

Implements HTTP Digest authentication with challenge-response.

### HTTPProxyAuth

```python
HTTPProxyAuth(username, password)
```

Like HTTPBasicAuth but sets `Proxy-Authorization` header for proxy authentication.

---

## Adapters

Module: `requests.adapters`

### BaseAdapter

Abstract base class for transport adapters.

| Method | Signature | Description |
|--------|-----------|-------------|
| `send` | `(request, stream=False, timeout=None, verify=True, cert=None, proxies=None)` | Send PreparedRequest, return Response |
| `close` | `()` | Clean up resources |

### HTTPAdapter

Connection-pooling adapter using urllib3.

#### Constructor

```python
HTTPAdapter(pool_connections=10, pool_maxsize=10, max_retries=0, pool_block=False)
```

| Parameter | Type | Default | Description |
|-----------|------|---------|-------------|
| `pool_connections` | `int` | 10 | Number of connection pools to cache (distinct hosts) |
| `pool_maxsize` | `int` | 10 | Max connections per pool |
| `max_retries` | `int`, `Retry` | 0 | Retry configuration |
| `pool_block` | `bool` | `False` | Block when pool exhausted |

#### Attributes

| Attribute | Type | Description |
|-----------|------|-------------|
| `poolmanager` | `PoolManager` | urllib3 pool manager |
| `config` | `dict` | Configuration |
| `_pool_connections` | `int` | Stored pool_connections |
| `_pool_maxsize` | `int` | Stored pool_maxsize |
| `_pool_block` | `bool` | Stored pool_block |

#### Methods

| Method | Signature | Description |
|--------|-----------|-------------|
| `init_poolmanager` | `(connections, maxsize, block=False, **pool_kwargs)` | Initialize PoolManager |
| `proxy_manager_for` | `(proxy, **proxy_kwargs)` | Get/create ProxyManager |
| `get_connection` | `(url, proxies=None)` | Get connection for URL |
| `send` | `(request, stream=False, timeout=None, verify=True, cert=None, proxies=None)` | Send request |
| `cert_verify` | `(conn, url, verify, cert)` | Configure SSL verification |
| `build_connection_pool_key_attributes` | `(request, verify, proxies=None, cert=None)` | Pool key attributes |
| `close` | `()` | Close poolmanager |

---

## Cookies

Module: `requests.cookies`

### RequestsCookieJar

Extends `http.cookiejar.CookieJar` with dict-like interface.

#### Methods

| Method | Signature | Returns | Description |
|--------|-----------|---------|-------------|
| `get` | `(name, default=None, domain=None, path=None)` | `str` | Get cookie value |
| `set` | `(name, value, **kwargs)` | `None` | Set cookie (None value removes) |
| `__getitem__` | `(name)` | `str` | Dict-style get (raises on ambiguity) |
| `__setitem__` | `(name, value)` | `None` | Dict-style set |
| `__delitem__` | `(name)` | `None` | Delete by name |
| `keys` | `()` | iterator | Cookie names |
| `values` | `()` | iterator | Cookie values |
| `items` | `()` | iterator | (name, value) pairs |
| `get_dict` | `(domain=None, path=None)` | `dict` | Filter to plain dict |
| `update` | `(other)` | `None` | Merge from dict/CookieJar |
| `copy` | `()` | `RequestsCookieJar` | Shallow copy |
| `set_cookie` | `(cookie)` | `None` | Add Cookie object |

### Cookie Object Attributes

From `http.cookiejar.Cookie`:

| Attribute | Type | Description |
|-----------|------|-------------|
| `name` | `str` | Cookie name |
| `value` | `str` | Cookie value |
| `domain` | `str` | Domain |
| `domain_specified` | `bool` | Domain was explicitly set |
| `domain_initial_dot` | `bool` | Domain starts with dot |
| `path` | `str` | Path |
| `path_specified` | `bool` | Path was explicitly set |
| `secure` | `bool` | HTTPS only |
| `expires` | `int`, `None` | Unix timestamp or None for session |
| `discard` | `bool` | Discard at session end |
| `comment` | `str`, `None` | Optional comment |
| `comment_url` | `str`, `None` | Optional comment URL |
| `rest` | `dict` | Non-standard attrs (e.g., HttpOnly) |
| `version` | `int` | Cookie version (0 or 1) |
| `rfc2109` | `bool` | Use RFC2109 |

### Utility Functions

| Function | Signature | Returns | Description |
|----------|-----------|---------|-------------|
| `cookiejar_from_dict` | `(cookie_dict, cookiejar=None, overwrite=True)` | `CookieJar` | Create jar from dict |
| `create_cookie` | `(name, value, **kwargs)` | `Cookie` | Create Cookie object |

#### create_cookie defaults

```python
version = 0
port = None
domain = ""
path = "/"
secure = False
expires = None
discard = True
comment = None
comment_url = None
rest = {'HttpOnly': None}
rfc2109 = False
```

---

## Structures

Module: `requests.structures`

### CaseInsensitiveDict

Dict with case-insensitive string keys. Used for HTTP headers.

| Method | Description |
|--------|-------------|
| `__init__(data=None, **kwargs)` | Initialize from mapping |
| `__setitem__(key, value)` | Store by lowercase key, preserve original |
| `__getitem__(key)` | Lookup by lowercase |
| `__delitem__(key)` | Delete by lowercase |
| `__contains__(key)` | Membership by lowercase |
| `__iter__()` | Iterate original-case keys |
| `__len__()` | Count entries |
| `__eq__(other)` | Case-insensitive comparison |
| `keys()` | Original-case keys |
| `values()` | Values |
| `items()` | (original_key, value) pairs |
| `lower_items()` | (lowercase_key, value) pairs |
| `copy()` | Shallow copy |
| `get(key, default=None)` | Get with default |

**Implementation:** Backed by OrderedDict mapping `lowercase_key -> (original_key, value)`

### LookupDict

Dict-like that returns `None` for missing keys instead of raising `KeyError`.

| Method | Description |
|--------|-------------|
| `__init__(name=None)` | Initialize with optional name |
| `__getitem__(key)` | Returns `None` if missing |
| `get(key, default=None)` | Get with default |
| `__repr__()` | `<lookup 'name'>` |

**Note:** Uses `__dict__` for storage (attributes, not dict entries).

---

## Status Codes

Module: `requests.status_codes`

Access via `requests.codes`:

```python
requests.codes.ok  # 200
requests.codes['not_found']  # 404
```

### 1xx Informational

| Code | Aliases |
|------|---------|
| 100 | `continue` |
| 101 | `switching_protocols` |
| 102 | `processing`, `early_hints` |
| 103 | `checkpoint` |

### 2xx Success

| Code | Aliases |
|------|---------|
| 200 | `ok`, `okay`, `all_ok`, `all_okay`, `all_good` |
| 201 | `created` |
| 202 | `accepted` |
| 203 | `non_authoritative_info`, `non_authoritative_information` |
| 204 | `no_content` |
| 205 | `reset_content`, `reset` |
| 206 | `partial_content`, `partial` |
| 207 | `multi_status`, `multiple_status`, `multi_stati`, `multiple_stati` |
| 208 | `already_reported` |
| 226 | `im_used` |

### 3xx Redirection

| Code | Aliases |
|------|---------|
| 300 | `multiple_choices` |
| 301 | `moved_permanently`, `moved` |
| 302 | `found` |
| 303 | `see_other`, `other` |
| 304 | `not_modified` |
| 305 | `use_proxy` |
| 306 | `switch_proxy` |
| 307 | `temporary_redirect`, `temporary_moved`, `temporary` |
| 308 | `permanent_redirect`, `resume_incomplete`, `resume` |

### 4xx Client Error

| Code | Aliases |
|------|---------|
| 400 | `bad_request`, `bad` |
| 401 | `unauthorized` |
| 402 | `payment_required`, `payment` |
| 403 | `forbidden` |
| 404 | `not_found` |
| 405 | `method_not_allowed`, `not_allowed` |
| 406 | `not_acceptable` |
| 407 | `proxy_authentication_required`, `proxy_auth`, `proxy_authentication` |
| 408 | `request_timeout`, `timeout` |
| 409 | `conflict` |
| 410 | `gone` |
| 411 | `length_required` |
| 412 | `precondition_failed`, `precondition` |
| 413 | `request_entity_too_large`, `payload_too_large` |
| 414 | `request_uri_too_large`, `uri_too_long` |
| 415 | `unsupported_media_type`, `unsupported_media`, `media_type` |
| 416 | `requested_range_not_satisfiable`, `range_not_satisfiable` |
| 417 | `expectation_failed` |
| 418 | `im_a_teapot`, `teapot`, `i_am_a_teapot` |
| 421 | `misdirected_request` |
| 422 | `unprocessable_entity`, `unprocessable` |
| 423 | `locked` |
| 424 | `failed_dependency`, `dependency` |
| 425 | `too_early` |
| 426 | `upgrade_required`, `upgrade` |
| 428 | `precondition_required`, `precondition` |
| 429 | `too_many_requests`, `too_many` |
| 431 | `header_fields_too_large`, `fields_too_large` |
| 451 | `unavailable_for_legal_reasons`, `legal_reasons` |

### 5xx Server Error

| Code | Aliases |
|------|---------|
| 500 | `internal_server_error`, `server_error` |
| 501 | `not_implemented` |
| 502 | `bad_gateway` |
| 503 | `service_unavailable`, `unavailable` |
| 504 | `gateway_timeout` |
| 505 | `http_version_not_supported`, `http_version` |
| 506 | `variant_also_negotiates` |
| 507 | `insufficient_storage` |
| 508 | `bandwidth_limit_exceeded`, `loop_detected` |
| 509 | `not_extended` |
| 510 | `not_extended` |
| 511 | `network_authentication_required`, `network_auth`, `network_authentication` |

---

## Hooks

Module: `requests.hooks`

### Supported Events

Only one event is currently supported: `"response"`

### Functions

| Function | Signature | Description |
|----------|-----------|-------------|
| `default_hooks` | `()` | Returns `{'response': []}` |
| `dispatch_hook` | `(key, hooks, hook_data, **kwargs)` | Execute hooks for event |

### dispatch_hook Behavior

1. If `hooks` is None/empty, return `hook_data`
2. Get hooks for `key`, wrap single callable in list
3. Call each hook with `hook(hook_data, **kwargs)`
4. If hook returns non-None, use as new `hook_data`
5. Return final `hook_data`

### Hook Registration (on Request/PreparedRequest)

| Method | Signature | Description |
|--------|-----------|-------------|
| `register_hook` | `(event, hook)` | Add hook for event |
| `deregister_hook` | `(event, hook)` | Remove hook, returns `True` if found |

### Session Hook Merging

```python
merge_hooks(request_hooks, session_hooks, dict_class=OrderedDict)
```

Combines per-request and session-level hooks.

---

## Utility Functions

Module: `requests.utils`

### Encoding

| Function | Signature | Returns | Description |
|----------|-----------|---------|-------------|
| `get_encoding_from_headers` | `(headers)` | `str`, `None` | Extract charset from Content-Type |
| `get_encodings_from_content` | `(content)` | `list[str]` | Extract encodings from HTML meta tags |

### URL Processing

| Function | Signature | Returns | Description |
|----------|-----------|---------|-------------|
| `requote_uri` | `(uri)` | `str` | Re-quote URI per RFC 3986 |
| `unquote_unreserved` | `(uri)` | `str` | Unquote unreserved characters |
| `address_in_network` | `(ip, net)` | `bool` | Check if IP in network |
| `dotted_netmask` | `(mask)` | `str` | Convert prefix to dotted netmask |
| `is_ipv4_address` | `(string_ip)` | `bool` | Validate IPv4 address |
| `is_valid_cidr` | `(string_network)` | `bool` | Validate CIDR notation |
| `urldefragauth` | `(url)` | `str` | Strip fragment and auth from URL |
| `prepend_scheme_if_needed` | `(url, scheme)` | `str` | Add scheme if missing |
| `get_auth_from_url` | `(url)` | `(user, pass)` | Extract auth from URL |

### Headers

| Function | Signature | Returns | Description |
|----------|-----------|---------|-------------|
| `parse_header_links` | `(value)` | `list[dict]` | Parse Link header |
| `default_headers` | `()` | `CaseInsensitiveDict` | Default request headers |
| `default_user_agent` | `(name='python-requests')` | `str` | Default User-Agent string |

### Cookies

| Function | Signature | Returns | Description |
|----------|-----------|---------|-------------|
| `dict_from_cookiejar` | `(cj)` | `dict` | Convert CookieJar to dict |
| `add_dict_to_cookiejar` | `(cj, cookie_dict)` | `CookieJar` | Add dict entries to jar |

### Streams/Files

| Function | Signature | Returns | Description |
|----------|-----------|---------|-------------|
| `super_len` | `(o)` | `int`, `None` | Get length of various objects |
| `get_netrc_auth` | `(url, raise_errors=False)` | `tuple`, `None` | Get auth from .netrc |
| `guess_filename` | `(obj)` | `str`, `None` | Guess filename from file object |
| `extract_zipped_paths` | `(path)` | `str` | Handle zipped file paths |
| `stream_decode_response_unicode` | `(iterator, r)` | generator | Decode streaming response |
| `iter_slices` | `(string, slice_length)` | generator | Slice string into chunks |

### Proxies

| Function | Signature | Returns | Description |
|----------|-----------|---------|-------------|
| `get_environ_proxies` | `(url, no_proxy=None)` | `dict` | Get proxies from environment |
| `select_proxy` | `(url, proxies)` | `str`, `None` | Select proxy for URL |
| `should_bypass_proxies` | `(url, no_proxy=None)` | `bool` | Check NO_PROXY rules |

### Environment

| Function | Signature | Returns | Description |
|----------|-----------|---------|-------------|
| `getproxies` | `()` | `dict` | System proxy settings |
| `proxy_bypass` | `(host)` | `bool` | Check if host bypasses proxy |

---

## Dependencies

### Required

| Package | Purpose |
|---------|---------|
| `urllib3` (≥1.21.1, <3.0.0) | HTTP connections, pooling |
| `idna` | Internationalized domain names |
| `certifi` | SSL certificate bundle |

### Encoding Detection (one required)

| Package | Version | Notes |
|---------|---------|-------|
| `charset_normalizer` | ≥2.0.0, <4.0.0 | Preferred for Python 3 |
| `chardet` | ≥3.0.2, <6.0.0 | Legacy, opt-in via `[use_chardet_on_py3]` |

---

## Constants

| Constant | Value | Description |
|----------|-------|-------------|
| `DEFAULT_REDIRECT_LIMIT` | 30 | Max redirects |
| `DEFAULT_POOLSIZE` | 10 | Default pool_connections/pool_maxsize |
| `DEFAULT_RETRIES` | 0 | Default retry count |
| `DEFAULT_POOLBLOCK` | False | Default pool_block |
| `REDIRECT_STATI` | `(301, 302, 303, 307, 308)` | Redirect status codes |

---

## File Upload Formats

The `files` parameter accepts:

```python
# Simple: field_name -> file_object
files = {'file': open('report.csv', 'rb')}

# With filename: field_name -> (filename, file_object)
files = {'file': ('report.csv', open('report.csv', 'rb'))}

# With content-type: field_name -> (filename, file_object, content_type)
files = {'file': ('report.csv', open('report.csv', 'rb'), 'text/csv')}

# With headers: field_name -> (filename, file_object, content_type, headers)
files = {'file': ('report.csv', open('report.csv', 'rb'), 'text/csv', {'X-Custom': 'value'})}

# Multiple files same field
files = [('images', ('foo.png', open('foo.png', 'rb'), 'image/png')),
         ('images', ('bar.png', open('bar.png', 'rb'), 'image/png'))]
```

---

## Timeout Formats

```python
# Single value: applies to both connect and read
timeout = 5.0

# Tuple: (connect_timeout, read_timeout)
timeout = (3.05, 27)

# None: wait forever (not recommended)
timeout = None
```

---

## Proxy Formats

```python
proxies = {
    'http': 'http://10.10.1.10:3128',
    'https': 'http://10.10.1.10:1080',
}

# With authentication
proxies = {
    'http': 'http://user:pass@10.10.1.10:3128',
}

# SOCKS proxies (requires requests[socks])
proxies = {
    'http': 'socks5://user:pass@host:port',
    'https': 'socks5://user:pass@host:port',
}
```
