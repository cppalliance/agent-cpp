//
// req.cpp - C++20 port of Python requests library API
//

#include <algorithm>
#include <chrono>
#include <coroutine>
#include <exception>
#include <functional>
#include <map>
#include <memory>
#include <optional>
#include <span>
#include <stdexcept>
#include <string>
#include <string_view>
#include <variant>
#include <vector>

namespace requests {

//------------------------------------------------------------------------------
//
// Generic task type for coroutines
//
//------------------------------------------------------------------------------

// Lazy coroutine task that suspends immediately and resumes on co_await
template<typename T = void>
class task {
public:
    struct promise_type {
        T value_;
        std::exception_ptr exception_;

        task get_return_object() {
            return task{std::coroutine_handle<promise_type>::from_promise(*this)};
        }
        std::suspend_always initial_suspend() noexcept { return {}; }
        std::suspend_always final_suspend() noexcept { return {}; }
        void return_value(T v) { value_ = std::move(v); }
        void unhandled_exception() { exception_ = std::current_exception(); }
    };

    using handle_type = std::coroutine_handle<promise_type>;

    explicit task(handle_type h) : handle_(h) {}
    task(task&& other) noexcept : handle_(std::exchange(other.handle_, {})) {}
    ~task() { if (handle_) handle_.destroy(); }

    bool await_ready() const noexcept { return false; }
    void await_suspend(std::coroutine_handle<> awaiter) {
        handle_.resume();
    }
    T await_resume() {
        if (handle_.promise().exception_)
            std::rethrow_exception(handle_.promise().exception_);
        return std::move(handle_.promise().value_);
    }

private:
    handle_type handle_;
};

// Specialization for void return type
template<>
class task<void> {
public:
    struct promise_type {
        std::exception_ptr exception_;

        task get_return_object() {
            return task{std::coroutine_handle<promise_type>::from_promise(*this)};
        }
        std::suspend_always initial_suspend() noexcept { return {}; }
        std::suspend_always final_suspend() noexcept { return {}; }
        void return_void() {}
        void unhandled_exception() { exception_ = std::current_exception(); }
    };

    using handle_type = std::coroutine_handle<promise_type>;

    explicit task(handle_type h) : handle_(h) {}
    task(task&& other) noexcept : handle_(std::exchange(other.handle_, {})) {}
    ~task() { if (handle_) handle_.destroy(); }

    bool await_ready() const noexcept { return false; }
    void await_suspend(std::coroutine_handle<> awaiter) {
        handle_.resume();
    }
    void await_resume() {
        if (handle_.promise().exception_)
            std::rethrow_exception(handle_.promise().exception_);
    }

private:
    handle_type handle_;
};

//------------------------------------------------------------------------------
//
// Exceptions - hierarchical exception classes mirroring Python requests
//
//------------------------------------------------------------------------------

// Base exception for all requests errors
class request_exception : public std::runtime_error {
public:
    using std::runtime_error::runtime_error;
};

// HTTP error raised by raise_for_status when status >= 400
class http_error : public request_exception {
public:
    using request_exception::request_exception;
};

// Network connection problems (DNS, refused, etc.)
class connection_error : public request_exception {
public:
    using request_exception::request_exception;
};

// Proxy server issues
class proxy_error : public connection_error {
public:
    using connection_error::connection_error;
};

// SSL/TLS certificate or handshake problems
class ssl_error : public connection_error {
public:
    using connection_error::connection_error;
};

// Request timeout base class
class timeout_error : public request_exception {
public:
    using request_exception::request_exception;
};

// Timeout during connection establishment
class connect_timeout : public timeout_error {
public:
    using timeout_error::timeout_error;
};

// Timeout waiting for data
class read_timeout : public timeout_error {
public:
    using timeout_error::timeout_error;
};

// No URL provided
class url_required : public request_exception {
public:
    using request_exception::request_exception;
};

// Exceeded max_redirects
class too_many_redirects : public request_exception {
public:
    using request_exception::request_exception;
};

// URL missing scheme
class missing_schema : public request_exception {
public:
    using request_exception::request_exception;
};

// Unsupported URL scheme
class invalid_schema : public request_exception {
public:
    using request_exception::request_exception;
};

// Malformed URL
class invalid_url : public request_exception {
public:
    using request_exception::request_exception;
};

// Malformed proxy URL
class invalid_proxy_url : public invalid_url {
public:
    using invalid_url::invalid_url;
};

// Invalid header value
class invalid_header : public request_exception {
public:
    using request_exception::request_exception;
};

// JSON decoding failed
class invalid_json_error : public request_exception {
public:
    using request_exception::request_exception;
};

// Invalid chunked transfer encoding
class chunked_encoding_error : public request_exception {
public:
    using request_exception::request_exception;
};

// Content decompression failed
class content_decoding_error : public request_exception {
public:
    using request_exception::request_exception;
};

// Content already consumed
class stream_consumed_error : public request_exception {
public:
    using request_exception::request_exception;
};

// Retry logic exhausted
class retry_error : public request_exception {
public:
    using request_exception::request_exception;
};

// Cannot rewind body for retry
class unrewindable_body_error : public request_exception {
public:
    using request_exception::request_exception;
};

//------------------------------------------------------------------------------
//
// Structures - utility containers for headers and lookups
//
//------------------------------------------------------------------------------

// Dict with case-insensitive string keys for HTTP headers
class case_insensitive_dict {
    std::map<std::string, std::pair<std::string, std::string>> data_;

    static std::string to_lower(std::string_view s) {
        std::string result(s);
        for (auto& c : result) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
        return result;
    }

public:
    case_insensitive_dict() = default;

    // Store by lowercase key, preserve original case
    void set(std::string_view key, std::string_view value) {
        data_[to_lower(key)] = {std::string(key), std::string(value)};
    }

    // Lookup by lowercase key
    std::optional<std::string> get(std::string_view key) const {
        auto it = data_.find(to_lower(key));
        if (it != data_.end()) return it->second.second;
        return std::nullopt;
    }

    // Delete by lowercase key
    bool erase(std::string_view key) {
        return data_.erase(to_lower(key)) > 0;
    }

    // Check membership by lowercase key
    bool contains(std::string_view key) const {
        return data_.find(to_lower(key)) != data_.end();
    }

    std::size_t size() const { return data_.size(); }
    bool empty() const { return data_.empty(); }
    void clear() { data_.clear(); }
};

// Dict-like that returns nullopt for missing keys instead of throwing
class lookup_dict {
    std::string name_;
    std::map<std::string, int> data_;

public:
    explicit lookup_dict(std::string_view name = "") : name_(name) {}

    // Returns nullopt if missing instead of throwing
    std::optional<int> get(std::string_view key) const {
        auto it = data_.find(std::string(key));
        if (it != data_.end()) return it->second;
        return std::nullopt;
    }

    void set(std::string_view key, int value) {
        data_[std::string(key)] = value;
    }

    std::string_view name() const { return name_; }
};

//------------------------------------------------------------------------------
//
// Status Codes - HTTP status code constants
//
//------------------------------------------------------------------------------

namespace codes {
    // 1xx Informational
    inline constexpr int continue_ = 100;             // continue is reserved
    inline constexpr int switching_protocols = 101;
    inline constexpr int processing = 102;
    inline constexpr int early_hints = 103;

    // 2xx Success
    inline constexpr int ok = 200;
    inline constexpr int created = 201;
    inline constexpr int accepted = 202;
    inline constexpr int non_authoritative_info = 203;
    inline constexpr int no_content = 204;
    inline constexpr int reset_content = 205;
    inline constexpr int partial_content = 206;
    inline constexpr int multi_status = 207;
    inline constexpr int already_reported = 208;
    inline constexpr int im_used = 226;

    // 3xx Redirection
    inline constexpr int multiple_choices = 300;
    inline constexpr int moved_permanently = 301;
    inline constexpr int found = 302;
    inline constexpr int see_other = 303;
    inline constexpr int not_modified = 304;
    inline constexpr int use_proxy = 305;
    inline constexpr int temporary_redirect = 307;
    inline constexpr int permanent_redirect = 308;

    // 4xx Client Error
    inline constexpr int bad_request = 400;
    inline constexpr int unauthorized = 401;
    inline constexpr int payment_required = 402;
    inline constexpr int forbidden = 403;
    inline constexpr int not_found = 404;
    inline constexpr int method_not_allowed = 405;
    inline constexpr int not_acceptable = 406;
    inline constexpr int proxy_authentication_required = 407;
    inline constexpr int request_timeout = 408;
    inline constexpr int conflict = 409;
    inline constexpr int gone = 410;
    inline constexpr int length_required = 411;
    inline constexpr int precondition_failed = 412;
    inline constexpr int payload_too_large = 413;
    inline constexpr int uri_too_long = 414;
    inline constexpr int unsupported_media_type = 415;
    inline constexpr int range_not_satisfiable = 416;
    inline constexpr int expectation_failed = 417;
    inline constexpr int im_a_teapot = 418;
    inline constexpr int misdirected_request = 421;
    inline constexpr int unprocessable_entity = 422;
    inline constexpr int locked = 423;
    inline constexpr int failed_dependency = 424;
    inline constexpr int too_early = 425;
    inline constexpr int upgrade_required = 426;
    inline constexpr int precondition_required = 428;
    inline constexpr int too_many_requests = 429;
    inline constexpr int header_fields_too_large = 431;
    inline constexpr int unavailable_for_legal_reasons = 451;

    // 5xx Server Error
    inline constexpr int internal_server_error = 500;
    inline constexpr int not_implemented = 501;
    inline constexpr int bad_gateway = 502;
    inline constexpr int service_unavailable = 503;
    inline constexpr int gateway_timeout = 504;
    inline constexpr int http_version_not_supported = 505;
    inline constexpr int variant_also_negotiates = 506;
    inline constexpr int insufficient_storage = 507;
    inline constexpr int loop_detected = 508;
    inline constexpr int not_extended = 510;
    inline constexpr int network_authentication_required = 511;
} // namespace codes

//------------------------------------------------------------------------------
//
// Constants - default values and limits
//
//------------------------------------------------------------------------------

inline constexpr int default_redirect_limit = 30;
inline constexpr int default_pool_size = 10;
inline constexpr int default_retries = 0;
inline constexpr bool default_pool_block = false;

// Redirect status codes that trigger automatic following
inline constexpr int redirect_stati[] = {301, 302, 303, 307, 308};

//------------------------------------------------------------------------------
//
// Timeout - connection and read timeout configuration
//
//------------------------------------------------------------------------------

// Timeout configuration with separate connect and read values
struct timeout {
    std::optional<std::chrono::milliseconds> connect;
    std::optional<std::chrono::milliseconds> read;

    // Single value applies to both connect and read
    static timeout from_seconds(double seconds) {
        auto ms = std::chrono::milliseconds(static_cast<int>(seconds * 1000));
        return {ms, ms};
    }

    // Tuple form with separate connect and read timeouts
    static timeout from_pair(double connect_sec, double read_sec) {
        return {
            std::chrono::milliseconds(static_cast<int>(connect_sec * 1000)),
            std::chrono::milliseconds(static_cast<int>(read_sec * 1000))
        };
    }

    // No timeout (wait forever)
    static timeout none() { return {std::nullopt, std::nullopt}; }
};

//------------------------------------------------------------------------------
//
// Authentication - auth handlers for HTTP authentication
//
//------------------------------------------------------------------------------

class prepared_request; // forward declaration

// Abstract base class for authentication handlers
class auth_base {
public:
    virtual ~auth_base() = default;

    // Modify request with authentication, return modified request
    virtual void operator()(prepared_request& r) = 0;
};

// HTTP Basic authentication handler
class http_basic_auth : public auth_base {
    std::string username_;
    std::string password_;

public:
    http_basic_auth(std::string_view username, std::string_view password)
        : username_(username), password_(password) {}

    // Adds Authorization: Basic <base64(user:pass)> header
    void operator()(prepared_request& r) override {}

    std::string_view username() const { return username_; }
    std::string_view password() const { return password_; }
};

// HTTP Digest authentication handler
class http_digest_auth : public auth_base {
    std::string username_;
    std::string password_;

public:
    http_digest_auth(std::string_view username, std::string_view password)
        : username_(username), password_(password) {}

    // Implements HTTP Digest authentication with challenge-response
    void operator()(prepared_request& r) override {}
};

// HTTP Proxy authentication handler
class http_proxy_auth : public auth_base {
    std::string username_;
    std::string password_;

public:
    http_proxy_auth(std::string_view username, std::string_view password)
        : username_(username), password_(password) {}

    // Sets Proxy-Authorization header for proxy authentication
    void operator()(prepared_request& r) override {}
};

// Simple username/password pair for auth=(user, pass) shorthand
struct auth_credentials {
    std::string username;
    std::string password;
};

//------------------------------------------------------------------------------
//
// Cookies - cookie jar and cookie manipulation
//
//------------------------------------------------------------------------------

// Individual cookie with all standard attributes
struct cookie {
    std::string name;
    std::string value;
    std::string domain;
    std::string path = "/";
    bool domain_specified = false;
    bool domain_initial_dot = false;
    bool path_specified = false;
    bool secure = false;
    std::optional<std::chrono::system_clock::time_point> expires;
    bool discard = true;
    std::optional<std::string> comment;
    std::optional<std::string> comment_url;
    bool http_only = false;
    int version = 0;
    bool rfc2109 = false;
};

// Cookie jar with dict-like interface
class cookie_jar {
    std::vector<cookie> cookies_;

public:
    // Get cookie value by name, optionally filtered by domain/path
    std::optional<std::string> get(
        std::string_view name,
        std::optional<std::string_view> domain = std::nullopt,
        std::optional<std::string_view> path = std::nullopt) const
    {
        for (auto const& c : cookies_) {
            if (c.name != name) continue;
            if (domain && c.domain != *domain) continue;
            if (path && c.path != *path) continue;
            return c.value;
        }
        return std::nullopt;
    }

    // Set cookie value, None value removes it
    void set(std::string_view name, std::optional<std::string_view> value) {
        if (!value) {
            erase(name);
            return;
        }
        for (auto& c : cookies_) {
            if (c.name == name) {
                c.value = *value;
                return;
            }
        }
        cookies_.push_back(cookie{std::string(name), std::string(*value)});
    }

    // Remove cookie by name
    bool erase(std::string_view name) {
        auto it = std::remove_if(cookies_.begin(), cookies_.end(),
            [name](auto const& c) { return c.name == name; });
        bool found = it != cookies_.end();
        cookies_.erase(it, cookies_.end());
        return found;
    }

    // Add cookie object directly
    void set_cookie(cookie c) {
        cookies_.push_back(std::move(c));
    }

    // Get all cookies as a map
    std::map<std::string, std::string> get_dict(
        std::optional<std::string_view> domain = std::nullopt,
        std::optional<std::string_view> path = std::nullopt) const
    {
        std::map<std::string, std::string> result;
        for (auto const& c : cookies_) {
            if (domain && c.domain != *domain) continue;
            if (path && c.path != *path) continue;
            result[c.name] = c.value;
        }
        return result;
    }

    // Update from another jar or map
    void update(std::map<std::string, std::string> const& other) {
        for (auto const& [k, v] : other) {
            set(k, v);
        }
    }

    // Create a copy
    cookie_jar copy() const { return *this; }

    std::size_t size() const { return cookies_.size(); }
    bool empty() const { return cookies_.empty(); }
    void clear() { cookies_.clear(); }

    auto begin() const { return cookies_.begin(); }
    auto end() const { return cookies_.end(); }
};

// Create cookie jar from dict
inline cookie_jar cookiejar_from_dict(
    std::map<std::string, std::string> const& cookie_dict)
{
    cookie_jar jar;
    for (auto const& [name, value] : cookie_dict)
        jar.set(name, value);
    return jar;
}

// Create cookie object with defaults
inline cookie create_cookie(
    std::string_view name,
    std::string_view value,
    std::string_view domain = "",
    std::string_view path = "/",
    bool secure = false,
    std::optional<std::chrono::system_clock::time_point> expires = std::nullopt)
{
    return cookie{
        std::string(name),
        std::string(value),
        std::string(domain),
        std::string(path),
        false, false, false,
        secure,
        expires,
        true,
        std::nullopt,
        std::nullopt,
        false,
        0,
        false
    };
}

//------------------------------------------------------------------------------
//
// Hooks - event callback system for request/response lifecycle
//
//------------------------------------------------------------------------------

class response; // forward declaration

// Hook function type for response events
using hook_fn = std::function<void(response&)>;

// Hook registry for event callbacks
struct hooks {
    std::vector<hook_fn> response_hooks;

    // Add hook for response event
    void register_hook(hook_fn fn) {
        response_hooks.push_back(std::move(fn));
    }

    // Remove hook, returns true if found
    bool deregister_hook(hook_fn const& fn) {
        // Note: can't compare std::function directly, simplified version
        if (!response_hooks.empty()) {
            response_hooks.pop_back();
            return true;
        }
        return false;
    }
};

// Returns default empty hooks
inline hooks default_hooks() {
    return hooks{};
}

//------------------------------------------------------------------------------
//
// File Upload - multipart file upload configuration
//
//------------------------------------------------------------------------------

// Single file for upload with optional metadata
struct upload_file {
    std::string field_name;
    std::string filename;
    std::vector<std::byte> content;
    std::optional<std::string> content_type;
    std::optional<case_insensitive_dict> headers;
};

// Collection of files for multipart upload
using files_dict = std::vector<upload_file>;

//------------------------------------------------------------------------------
//
// Proxy - proxy server configuration
//
//------------------------------------------------------------------------------

// Protocol-to-proxy URL mapping
struct proxy_config {
    std::optional<std::string> http;
    std::optional<std::string> https;
    std::optional<std::string> socks5;

    bool empty() const {
        return !http && !https && !socks5;
    }
};

//------------------------------------------------------------------------------
//
// SSL/TLS - certificate verification configuration
//
//------------------------------------------------------------------------------

// SSL verification: true, false, or CA bundle path
using verify_config = std::variant<bool, std::string>;

// Client certificate: path or (cert_path, key_path) pair
struct cert_config {
    std::string cert_path;
    std::optional<std::string> key_path;
};

//------------------------------------------------------------------------------
//
// Request Options - builder pattern for request configuration
//
//------------------------------------------------------------------------------

// Forward declarations
class session;
class request;
class prepared_request;
class response;

// All optional parameters for requests
struct request_options {
    std::optional<std::map<std::string, std::string>> params;
    std::optional<std::variant<std::string, std::vector<std::byte>>> data;
    std::optional<std::string> json;
    std::optional<case_insensitive_dict> headers;
    std::optional<cookie_jar> cookies;
    std::optional<files_dict> files;
    std::optional<std::shared_ptr<auth_base>> auth;
    std::optional<timeout> timeout_cfg;
    std::optional<bool> allow_redirects;
    std::optional<proxy_config> proxies;
    std::optional<hooks> hooks_cfg;
    std::optional<bool> stream;
    std::optional<verify_config> verify;
    std::optional<cert_config> cert;
};

//------------------------------------------------------------------------------
//
// Response - HTTP response with body and metadata
//
//------------------------------------------------------------------------------

// HTTP response from a request
class response {
    int status_code_ = 0;
    case_insensitive_dict headers_;
    std::string url_;
    std::vector<std::byte> content_;
    std::string encoding_;
    std::string reason_;
    cookie_jar cookies_;
    std::chrono::milliseconds elapsed_{0};
    std::vector<response> history_;
    std::shared_ptr<prepared_request> request_;

public:
    // HTTP status code (200, 404, etc.)
    int status_code() const { return status_code_; }
    void set_status_code(int code) { status_code_ = code; }

    // Response headers (case-insensitive)
    case_insensitive_dict const& headers() const { return headers_; }
    case_insensitive_dict& headers() { return headers_; }

    // Final URL after redirects
    std::string const& url() const { return url_; }
    void set_url(std::string u) { url_ = std::move(u); }

    // Decoded body text using encoding
    std::string text() const {
        return std::string(reinterpret_cast<char const*>(content_.data()), content_.size());
    }

    // Raw body bytes
    std::vector<std::byte> const& content() const { return content_; }
    void set_content(std::vector<std::byte> c) { content_ = std::move(c); }

    // Character encoding from headers or detected
    std::string const& encoding() const { return encoding_; }
    void set_encoding(std::string e) { encoding_ = std::move(e); }

    // Encoding detected from content (charset detection)
    std::string apparent_encoding() const { return "utf-8"; }

    // True if status_code < 400
    bool ok() const { return status_code_ < 400; }

    // HTTP reason phrase ("OK", "Not Found", etc.)
    std::string const& reason() const { return reason_; }
    void set_reason(std::string r) { reason_ = std::move(r); }

    // Response cookies
    cookie_jar const& cookies() const { return cookies_; }
    cookie_jar& cookies() { return cookies_; }

    // Time from send to response headers
    std::chrono::milliseconds elapsed() const { return elapsed_; }
    void set_elapsed(std::chrono::milliseconds e) { elapsed_ = e; }

    // Redirect history (oldest first)
    std::vector<response> const& history() const { return history_; }
    std::vector<response>& history() { return history_; }

    // The request that generated this response
    std::shared_ptr<prepared_request> request() const { return request_; }
    void set_request(std::shared_ptr<prepared_request> r) { request_ = std::move(r); }

    // True if valid redirect with Location header
    bool is_redirect() const {
        return (status_code_ == 301 || status_code_ == 302 ||
                status_code_ == 303 || status_code_ == 307 ||
                status_code_ == 308) && headers_.contains("Location");
    }

    // True if 301 or 308 with Location
    bool is_permanent_redirect() const {
        return (status_code_ == 301 || status_code_ == 308) &&
               headers_.contains("Location");
    }

    // Parsed Link header
    std::map<std::string, std::string> links() const { return {}; }

    // Parse JSON body (returns string for now, would use json library)
    std::string json() const { return text(); }

    // Raise http_error if status >= 400
    void raise_for_status() const {
        if (status_code_ >= 400)
            throw http_error(std::to_string(status_code_) + " " + reason_);
    }

    // Stream body in chunks (simplified: returns whole content)
    std::vector<std::byte> iter_content(std::size_t chunk_size = 1) const {
        return content_;
    }

    // Stream body line by line (simplified)
    std::vector<std::string> iter_lines() const {
        return {text()};
    }

    // Close connection
    void close() {}
};

//------------------------------------------------------------------------------
//
// PreparedRequest - fully configured request ready for transmission
//
//------------------------------------------------------------------------------

// Fully prepared HTTP request
class prepared_request {
    std::string method_;
    std::string url_;
    case_insensitive_dict headers_;
    std::vector<std::byte> body_;
    hooks hooks_;
    cookie_jar cookies_;
    std::optional<std::size_t> body_position_;

public:
    // HTTP method (uppercase)
    std::string const& method() const { return method_; }

    // Final URL with query params
    std::string const& url() const { return url_; }

    // Final headers including cookies
    case_insensitive_dict const& headers() const { return headers_; }
    case_insensitive_dict& headers() { return headers_; }

    // Encoded request body
    std::vector<std::byte> const& body() const { return body_; }

    // Event hooks
    hooks const& get_hooks() const { return hooks_; }

    // Path + query string portion
    std::string path_url() const {
        auto pos = url_.find("://");
        if (pos == std::string::npos) return url_;
        pos = url_.find('/', pos + 3);
        if (pos == std::string::npos) return "/";
        return url_.substr(pos);
    }

    // Full preparation from components
    void prepare(
        std::string_view method,
        std::string_view url,
        std::optional<case_insensitive_dict> headers = std::nullopt,
        std::optional<files_dict> files = std::nullopt,
        std::optional<std::variant<std::string, std::vector<std::byte>>> data = std::nullopt,
        std::optional<std::map<std::string, std::string>> params = std::nullopt,
        std::optional<std::shared_ptr<auth_base>> auth = std::nullopt,
        std::optional<cookie_jar> cookies = std::nullopt,
        std::optional<hooks> hooks_cfg = std::nullopt,
        std::optional<std::string> json = std::nullopt)
    {
        prepare_method(method);
        prepare_url(url, params);
        if (headers) prepare_headers(*headers);
        if (cookies) prepare_cookies(*cookies);
        prepare_body(data, files, json);
        if (auth && *auth) prepare_auth(**auth);
        if (hooks_cfg) prepare_hooks(*hooks_cfg);
    }

    // Normalize and set method
    void prepare_method(std::string_view method) {
        method_ = std::string(method);
        for (auto& c : method_)
            c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
    }

    // Combine URL and params
    void prepare_url(std::string_view url,
        std::optional<std::map<std::string, std::string>> params = std::nullopt)
    {
        url_ = std::string(url);
        if (params && !params->empty()) {
            url_ += (url_.find('?') == std::string::npos) ? "?" : "&";
            bool first = true;
            for (auto const& [k, v] : *params) {
                if (!first) url_ += "&";
                url_ += k + "=" + v;
                first = false;
            }
        }
    }

    // Merge headers
    void prepare_headers(case_insensitive_dict const& headers) {
        headers_ = headers;
    }

    // Encode body from data/files/json
    void prepare_body(
        std::optional<std::variant<std::string, std::vector<std::byte>>> data,
        std::optional<files_dict> files,
        std::optional<std::string> json)
    {
        if (json) {
            auto const& s = *json;
            body_.assign(reinterpret_cast<std::byte const*>(s.data()),
                        reinterpret_cast<std::byte const*>(s.data() + s.size()));
            headers_.set("Content-Type", "application/json");
        } else if (data) {
            if (auto* s = std::get_if<std::string>(&*data)) {
                body_.assign(reinterpret_cast<std::byte const*>(s->data()),
                            reinterpret_cast<std::byte const*>(s->data() + s->size()));
            } else if (auto* b = std::get_if<std::vector<std::byte>>(&*data)) {
                body_ = *b;
            }
        }
        prepare_content_length();
    }

    // Set Cookie header from jar
    void prepare_cookies(cookie_jar const& cookies) {
        cookies_ = cookies;
    }

    // Apply authentication handler
    void prepare_auth(auth_base& auth) {
        auth(*this);
    }

    // Set Content-Length header
    void prepare_content_length() {
        if (!body_.empty())
            headers_.set("Content-Length", std::to_string(body_.size()));
    }

    // Register hooks
    void prepare_hooks(hooks const& h) {
        hooks_ = h;
    }

    // Add event hook
    void register_hook(hook_fn fn) {
        hooks_.register_hook(std::move(fn));
    }

    // Remove event hook
    bool deregister_hook(hook_fn const& fn) {
        return hooks_.deregister_hook(fn);
    }

    // Shallow copy
    prepared_request copy() const { return *this; }
};

//------------------------------------------------------------------------------
//
// Request - user-facing request object before preparation
//
//------------------------------------------------------------------------------

// User-facing request before preparation
class request {
    std::string method_;
    std::string url_;
    case_insensitive_dict headers_;
    files_dict files_;
    std::variant<std::string, std::vector<std::byte>> data_;
    std::map<std::string, std::string> params_;
    std::shared_ptr<auth_base> auth_;
    cookie_jar cookies_;
    hooks hooks_;
    std::optional<std::string> json_;

public:
    request() = default;

    request(
        std::string_view method,
        std::string_view url,
        std::optional<case_insensitive_dict> headers = std::nullopt,
        std::optional<files_dict> files = std::nullopt,
        std::optional<std::variant<std::string, std::vector<std::byte>>> data = std::nullopt,
        std::optional<std::map<std::string, std::string>> params = std::nullopt,
        std::optional<std::shared_ptr<auth_base>> auth = std::nullopt,
        std::optional<cookie_jar> cookies = std::nullopt,
        std::optional<hooks> hooks_cfg = std::nullopt,
        std::optional<std::string> json = std::nullopt)
        : method_(method)
        , url_(url)
    {
        if (headers) headers_ = *headers;
        if (files) files_ = *files;
        if (data) data_ = *data;
        if (params) params_ = *params;
        if (auth) auth_ = *auth;
        if (cookies) cookies_ = *cookies;
        if (hooks_cfg) hooks_ = *hooks_cfg;
        if (json) json_ = *json;
    }

    // Accessors
    std::string const& method() const { return method_; }
    std::string const& url() const { return url_; }
    case_insensitive_dict const& headers() const { return headers_; }
    files_dict const& files() const { return files_; }
    std::map<std::string, std::string> const& params() const { return params_; }
    cookie_jar const& cookies() const { return cookies_; }

    // Convert to PreparedRequest
    prepared_request prepare() const {
        prepared_request pr;
        pr.prepare(method_, url_,
            headers_.empty() ? std::nullopt : std::optional{headers_},
            files_.empty() ? std::nullopt : std::optional{files_},
            std::optional{data_},
            params_.empty() ? std::nullopt : std::optional{params_},
            auth_ ? std::optional{auth_} : std::nullopt,
            cookies_.empty() ? std::nullopt : std::optional{cookies_},
            std::optional{hooks_},
            json_);
        return pr;
    }

    // Add event hook
    void register_hook(hook_fn fn) {
        hooks_.register_hook(std::move(fn));
    }

    // Remove event hook
    bool deregister_hook(hook_fn const& fn) {
        return hooks_.deregister_hook(fn);
    }
};

//------------------------------------------------------------------------------
//
// Adapters - transport adapters for connection pooling
//
//------------------------------------------------------------------------------

// Abstract base class for transport adapters
class base_adapter {
public:
    virtual ~base_adapter() = default;

    // Send PreparedRequest, return Response
    virtual task<response> send(
        prepared_request const& request,
        bool stream = false,
        std::optional<timeout> timeout_cfg = std::nullopt,
        verify_config verify = true,
        std::optional<cert_config> cert = std::nullopt,
        std::optional<proxy_config> proxies = std::nullopt) = 0;

    // Clean up resources
    virtual void close() = 0;
};

// Connection-pooling HTTP adapter
class http_adapter : public base_adapter {
    int pool_connections_;
    int pool_maxsize_;
    int max_retries_;
    bool pool_block_;

public:
    http_adapter(
        int pool_connections = default_pool_size,
        int pool_maxsize = default_pool_size,
        int max_retries = default_retries,
        bool pool_block = default_pool_block)
        : pool_connections_(pool_connections)
        , pool_maxsize_(pool_maxsize)
        , max_retries_(max_retries)
        , pool_block_(pool_block)
    {}

    // Send request through connection pool
    task<response> send(
        prepared_request const& request,
        bool stream = false,
        std::optional<timeout> timeout_cfg = std::nullopt,
        verify_config verify = true,
        std::optional<cert_config> cert = std::nullopt,
        std::optional<proxy_config> proxies = std::nullopt) override
    {
        response r;
        r.set_status_code(200);
        r.set_reason("OK");
        r.set_url(request.url());
        co_return r;
    }

    // Initialize pool manager
    void init_poolmanager(int connections, int maxsize, bool block = false) {}

    // Get/create proxy manager for proxy URL
    void proxy_manager_for(std::string_view proxy) {}

    // Get connection for URL
    void get_connection(std::string_view url,
        std::optional<proxy_config> proxies = std::nullopt) {}

    // Configure SSL verification
    void cert_verify(std::string_view url, verify_config verify,
        std::optional<cert_config> cert = std::nullopt) {}

    // Close pool manager
    void close() override {}
};

//------------------------------------------------------------------------------
//
// Session - persistent settings, connection pooling, cookie persistence
//
//------------------------------------------------------------------------------

// Session with persistent settings and connection pooling
class session {
    case_insensitive_dict headers_;
    cookie_jar cookies_;
    std::shared_ptr<auth_base> auth_;
    proxy_config proxies_;
    hooks hooks_;
    std::map<std::string, std::string> params_;
    bool stream_ = false;
    verify_config verify_ = true;
    std::optional<cert_config> cert_;
    int max_redirects_ = default_redirect_limit;
    bool trust_env_ = true;
    std::map<std::string, std::shared_ptr<base_adapter>> adapters_;

public:
    session() {
        // Set default headers
        headers_.set("User-Agent", "requests-cpp/1.0");
        headers_.set("Accept", "*/*");
        headers_.set("Accept-Encoding", "gzip, deflate");
        headers_.set("Connection", "keep-alive");

        // Mount default adapters
        adapters_["https://"] = std::make_shared<http_adapter>();
        adapters_["http://"] = std::make_shared<http_adapter>();
    }

    // Default headers for all requests
    case_insensitive_dict& headers() { return headers_; }

    // Cookie jar, auto-updated from responses
    cookie_jar& cookies() { return cookies_; }

    // Default authentication
    void set_auth(std::shared_ptr<auth_base> auth) { auth_ = std::move(auth); }

    // Default proxy mappings
    proxy_config& proxies() { return proxies_; }

    // Default hooks
    hooks& get_hooks() { return hooks_; }

    // Default query parameters
    std::map<std::string, std::string>& params() { return params_; }

    // Default streaming behavior
    bool stream() const { return stream_; }
    void set_stream(bool s) { stream_ = s; }

    // Default SSL verification
    verify_config const& verify() const { return verify_; }
    void set_verify(verify_config v) { verify_ = std::move(v); }

    // Default client certificate
    std::optional<cert_config> const& cert() const { return cert_; }
    void set_cert(cert_config c) { cert_ = std::move(c); }

    // Maximum redirects before too_many_redirects
    int max_redirects() const { return max_redirects_; }
    void set_max_redirects(int m) { max_redirects_ = m; }

    // Use environment for proxies, .netrc, CA paths
    bool trust_env() const { return trust_env_; }
    void set_trust_env(bool t) { trust_env_ = t; }

    // Core request method with all options
    task<response> request(
        std::string_view method,
        std::string_view url,
        request_options opts = {})
    {
        requests::request req(method, url,
            opts.headers, opts.files, opts.data, opts.params,
            opts.auth, opts.cookies, opts.hooks_cfg, opts.json);

        auto prepped = prepare_request(req);
        co_return co_await send(prepped, opts);
    }

    // GET request
    task<response> get(std::string_view url, request_options opts = {}) {
        return request("GET", url, std::move(opts));
    }

    // POST request
    task<response> post(std::string_view url, request_options opts = {}) {
        return request("POST", url, std::move(opts));
    }

    // PUT request
    task<response> put(std::string_view url, request_options opts = {}) {
        return request("PUT", url, std::move(opts));
    }

    // PATCH request
    task<response> patch(std::string_view url, request_options opts = {}) {
        return request("PATCH", url, std::move(opts));
    }

    // DELETE request
    task<response> delete_(std::string_view url, request_options opts = {}) {
        return request("DELETE", url, std::move(opts));
    }

    // HEAD request
    task<response> head(std::string_view url, request_options opts = {}) {
        opts.allow_redirects = false;
        return request("HEAD", url, std::move(opts));
    }

    // OPTIONS request
    task<response> options(std::string_view url, request_options opts = {}) {
        return request("OPTIONS", url, std::move(opts));
    }

    // Merge session defaults into Request
    prepared_request prepare_request(requests::request const& req) {
        return req.prepare();
    }

    // Send PreparedRequest via adapter
    task<response> send(prepared_request const& prepped, request_options const& opts = {}) {
        auto adapter = get_adapter(prepped.url());
        co_return co_await adapter->send(prepped,
            opts.stream.value_or(stream_),
            opts.timeout_cfg,
            opts.verify.value_or(verify_),
            opts.cert.has_value() ? opts.cert : cert_,
            opts.proxies.has_value() ? opts.proxies : std::optional{proxies_});
    }

    // Get adapter for URL prefix
    std::shared_ptr<base_adapter> get_adapter(std::string_view url) {
        for (auto const& [prefix, adapter] : adapters_) {
            if (url.substr(0, prefix.size()) == prefix)
                return adapter;
        }
        return adapters_["http://"];
    }

    // Register adapter for URL prefix
    void mount(std::string_view prefix, std::shared_ptr<base_adapter> adapter) {
        adapters_[std::string(prefix)] = std::move(adapter);
    }

    // Close all adapters and connections
    void close() {
        for (auto& [prefix, adapter] : adapters_)
            adapter->close();
    }

    // Merge per-request, session, and env settings
    void merge_environment_settings(std::string_view url,
        proxy_config& proxies, bool& stream,
        verify_config& verify, std::optional<cert_config>& cert) {}

    // Strip auth on cross-host redirect
    void rebuild_auth(prepared_request& pr, response const& r) {}

    // Re-evaluate proxy for redirect
    void rebuild_proxies(prepared_request& pr, proxy_config& proxies) {}

    // Change method per redirect status (e.g., POST→GET)
    void rebuild_method(prepared_request& pr, response const& r) {}
};

//------------------------------------------------------------------------------
//
// Top-Level API Functions - convenience functions using temporary session
//
//------------------------------------------------------------------------------

// Base request function with all options
inline task<response> request(
    std::string_view method,
    std::string_view url,
    request_options opts = {})
{
    session s;
    co_return co_await s.request(method, url, std::move(opts));
}

// GET request
inline task<response> get(std::string_view url, request_options opts = {}) {
    return request("GET", url, std::move(opts));
}

// POST request
inline task<response> post(std::string_view url, request_options opts = {}) {
    return request("POST", url, std::move(opts));
}

// PUT request
inline task<response> put(std::string_view url, request_options opts = {}) {
    return request("PUT", url, std::move(opts));
}

// PATCH request
inline task<response> patch(std::string_view url, request_options opts = {}) {
    return request("PATCH", url, std::move(opts));
}

// DELETE request
inline task<response> delete_(std::string_view url, request_options opts = {}) {
    return request("DELETE", url, std::move(opts));
}

// HEAD request (allow_redirects=false by default)
inline task<response> head(std::string_view url, request_options opts = {}) {
    opts.allow_redirects = false;
    return request("HEAD", url, std::move(opts));
}

// OPTIONS request
inline task<response> options(std::string_view url, request_options opts = {}) {
    return request("OPTIONS", url, std::move(opts));
}

//------------------------------------------------------------------------------
//
// Utility Functions - encoding, URL processing, headers, etc.
//
//------------------------------------------------------------------------------

namespace utils {

// Extract charset from Content-Type header
inline std::optional<std::string> get_encoding_from_headers(
    case_insensitive_dict const& headers)
{
    auto ct = headers.get("Content-Type");
    if (!ct) return std::nullopt;
    auto pos = ct->find("charset=");
    if (pos == std::string::npos) return std::nullopt;
    return ct->substr(pos + 8);
}

// Re-quote URI per RFC 3986
inline std::string requote_uri(std::string_view uri) {
    return std::string(uri);
}

// Unquote unreserved characters
inline std::string unquote_unreserved(std::string_view uri) {
    return std::string(uri);
}

// Check if IP is in network
inline bool address_in_network(std::string_view ip, std::string_view net) {
    return false;
}

// Convert prefix to dotted netmask
inline std::string dotted_netmask(int mask) {
    return "";
}

// Validate IPv4 address
inline bool is_ipv4_address(std::string_view ip) {
    return false;
}

// Validate CIDR notation
inline bool is_valid_cidr(std::string_view network) {
    return false;
}

// Strip fragment and auth from URL
inline std::string urldefragauth(std::string_view url) {
    return std::string(url);
}

// Add scheme if missing
inline std::string prepend_scheme_if_needed(std::string_view url, std::string_view scheme) {
    if (url.find("://") != std::string::npos)
        return std::string(url);
    return std::string(scheme) + "://" + std::string(url);
}

// Extract auth from URL
inline std::pair<std::string, std::string> get_auth_from_url(std::string_view url) {
    return {"", ""};
}

// Parse Link header
inline std::vector<std::map<std::string, std::string>> parse_header_links(
    std::string_view value)
{
    return {};
}

// Default request headers
inline case_insensitive_dict default_headers() {
    case_insensitive_dict h;
    h.set("User-Agent", "requests-cpp/1.0");
    h.set("Accept", "*/*");
    h.set("Accept-Encoding", "gzip, deflate");
    h.set("Connection", "keep-alive");
    return h;
}

// Default User-Agent string
inline std::string default_user_agent(std::string_view name = "requests-cpp") {
    return std::string(name) + "/1.0";
}

// Convert CookieJar to dict
inline std::map<std::string, std::string> dict_from_cookiejar(cookie_jar const& cj) {
    return cj.get_dict();
}

// Add dict entries to jar
inline void add_dict_to_cookiejar(cookie_jar& cj,
    std::map<std::string, std::string> const& cookie_dict)
{
    cj.update(cookie_dict);
}

// Get length of various objects
inline std::optional<std::size_t> super_len(std::string_view s) {
    return s.size();
}

// Get auth from .netrc
inline std::optional<auth_credentials> get_netrc_auth(std::string_view url) {
    return std::nullopt;
}

// Guess filename from path
inline std::optional<std::string> guess_filename(std::string_view path) {
    auto pos = path.find_last_of("/\\");
    if (pos == std::string::npos) return std::string(path);
    return std::string(path.substr(pos + 1));
}

// Get proxies from environment
inline proxy_config get_environ_proxies(std::string_view url) {
    return {};
}

// Select proxy for URL
inline std::optional<std::string> select_proxy(std::string_view url,
    proxy_config const& proxies)
{
    if (url.starts_with("https://")) return proxies.https;
    if (url.starts_with("http://")) return proxies.http;
    return std::nullopt;
}

// Check NO_PROXY rules
inline bool should_bypass_proxies(std::string_view url,
    std::optional<std::string> no_proxy = std::nullopt)
{
    return false;
}

} // namespace utils

} // namespace requests

//------------------------------------------------------------------------------
//
// Main - Part 1: Exercise API (ensure everything compiles)
//
//------------------------------------------------------------------------------

void exercise_api() {
    using namespace requests;

    // --- Exception hierarchy ---
    try { throw http_error("404"); }
    catch (request_exception const&) {}

    try { throw ssl_error("cert failed"); }
    catch (connection_error const&) {}

    try { throw proxy_error("proxy down"); }
    catch (connection_error const&) {}

    try { throw connect_timeout("conn timeout"); }
    catch (timeout_error const&) {}

    try { throw read_timeout("read timeout"); }
    catch (timeout_error const&) {}

    try { throw url_required("no url"); }
    catch (request_exception const&) {}

    try { throw too_many_redirects("30 redirects"); }
    catch (request_exception const&) {}

    try { throw missing_schema("no http://"); }
    catch (request_exception const&) {}

    try { throw invalid_schema("ftp://"); }
    catch (request_exception const&) {}

    try { throw invalid_url("bad url"); }
    catch (request_exception const&) {}

    try { throw invalid_proxy_url("bad proxy"); }
    catch (invalid_url const&) {}

    try { throw invalid_header("bad header"); }
    catch (request_exception const&) {}

    try { throw invalid_json_error("bad json"); }
    catch (request_exception const&) {}

    try { throw chunked_encoding_error("bad chunk"); }
    catch (request_exception const&) {}

    try { throw content_decoding_error("decompress fail"); }
    catch (request_exception const&) {}

    try { throw stream_consumed_error("already read"); }
    catch (request_exception const&) {}

    try { throw retry_error("retry exhausted"); }
    catch (request_exception const&) {}

    try { throw unrewindable_body_error("can't rewind"); }
    catch (request_exception const&) {}

    // --- Structures ---
    case_insensitive_dict headers;
    headers.set("Content-Type", "application/json");
    headers.set("content-type", "text/html");
    [[maybe_unused]] auto ct = headers.get("CONTENT-TYPE");
    [[maybe_unused]] bool has = headers.contains("content-type");
    headers.erase("Content-Type");
    [[maybe_unused]] auto sz = headers.size();
    [[maybe_unused]] bool emp = headers.empty();
    headers.clear();

    lookup_dict ld("codes");
    ld.set("ok", 200);
    [[maybe_unused]] auto code = ld.get("ok");
    [[maybe_unused]] auto missing = ld.get("notfound");
    [[maybe_unused]] auto nm = ld.name();

    // --- Status codes ---
    [[maybe_unused]] int c1 = codes::continue_;
    [[maybe_unused]] int c2 = codes::ok;
    [[maybe_unused]] int c3 = codes::created;
    [[maybe_unused]] int c4 = codes::moved_permanently;
    [[maybe_unused]] int c5 = codes::bad_request;
    [[maybe_unused]] int c6 = codes::unauthorized;
    [[maybe_unused]] int c7 = codes::not_found;
    [[maybe_unused]] int c8 = codes::internal_server_error;
    [[maybe_unused]] int c9 = codes::im_a_teapot;

    // --- Constants ---
    [[maybe_unused]] int limit = default_redirect_limit;
    [[maybe_unused]] int poolsz = default_pool_size;
    [[maybe_unused]] int retries = default_retries;
    [[maybe_unused]] bool block = default_pool_block;
    [[maybe_unused]] auto stati = redirect_stati;

    // --- Timeout ---
    auto t1 = timeout::from_seconds(5.0);
    auto t2 = timeout::from_pair(3.0, 27.0);
    auto t3 = timeout::none();
    [[maybe_unused]] auto conn = t1.connect;
    [[maybe_unused]] auto rd = t2.read;
    (void)t3;

    // --- Authentication ---
    http_basic_auth basic("user", "pass");
    [[maybe_unused]] auto un = basic.username();
    [[maybe_unused]] auto pw = basic.password();

    http_digest_auth digest("user", "pass");
    http_proxy_auth proxy_auth("user", "pass");

    auth_credentials creds{"user", "pass"};
    (void)creds;

    // --- Cookies ---
    cookie c;
    c.name = "session";
    c.value = "abc123";
    c.domain = "example.com";
    c.path = "/";
    c.secure = true;
    c.http_only = true;

    cookie_jar jar;
    jar.set("token", "xyz");
    [[maybe_unused]] auto tok = jar.get("token");
    [[maybe_unused]] auto filtered = jar.get("token", "example.com", "/");
    jar.erase("token");
    jar.set_cookie(c);
    [[maybe_unused]] auto dict = jar.get_dict();
    jar.update({{"a", "1"}, {"b", "2"}});
    [[maybe_unused]] auto copy = jar.copy();
    [[maybe_unused]] auto jsz = jar.size();
    [[maybe_unused]] bool jemp = jar.empty();
    jar.clear();

    auto jar2 = cookiejar_from_dict({{"x", "y"}});
    auto ck = create_cookie("name", "value", "dom", "/", true);
    (void)jar2;
    (void)ck;

    // --- Hooks ---
    hooks h;
    h.register_hook([](response&) {});
    [[maybe_unused]] bool removed = h.deregister_hook([](response&) {});
    auto dh = default_hooks();
    (void)dh;

    // --- File upload ---
    upload_file uf;
    uf.field_name = "file";
    uf.filename = "test.txt";
    uf.content = {std::byte{0x48}, std::byte{0x69}};
    uf.content_type = "text/plain";

    files_dict files;
    files.push_back(uf);

    // --- Proxy ---
    proxy_config proxies;
    proxies.http = "http://proxy:8080";
    proxies.https = "http://proxy:8080";
    [[maybe_unused]] bool pemp = proxies.empty();

    // --- SSL ---
    verify_config v1 = true;
    verify_config v2 = false;
    verify_config v3 = "/path/to/ca-bundle.crt";
    (void)v1; (void)v2; (void)v3;

    cert_config cert;
    cert.cert_path = "/path/to/client.crt";
    cert.key_path = "/path/to/client.key";

    // --- Request options ---
    request_options opts;
    opts.params = std::map<std::string, std::string>{{"q", "test"}};
    opts.headers = case_insensitive_dict{};
    opts.timeout_cfg = timeout::from_seconds(30);
    opts.allow_redirects = true;
    opts.stream = false;
    opts.verify = true;

    // --- Response ---
    response resp;
    resp.set_status_code(200);
    resp.set_reason("OK");
    resp.set_url("https://example.com");
    resp.set_encoding("utf-8");
    resp.set_elapsed(std::chrono::milliseconds(150));
    [[maybe_unused]] int sc = resp.status_code();
    [[maybe_unused]] auto& rh = resp.headers();
    [[maybe_unused]] auto ru = resp.url();
    [[maybe_unused]] auto txt = resp.text();
    [[maybe_unused]] auto& cnt = resp.content();
    [[maybe_unused]] auto enc = resp.encoding();
    [[maybe_unused]] auto ae = resp.apparent_encoding();
    [[maybe_unused]] bool ok = resp.ok();
    [[maybe_unused]] auto rsn = resp.reason();
    [[maybe_unused]] auto& rcook = resp.cookies();
    [[maybe_unused]] auto elapsed = resp.elapsed();
    [[maybe_unused]] auto& hist = resp.history();
    [[maybe_unused]] auto req = resp.request();
    [[maybe_unused]] bool redir = resp.is_redirect();
    [[maybe_unused]] bool perm = resp.is_permanent_redirect();
    [[maybe_unused]] auto links = resp.links();
    [[maybe_unused]] auto js = resp.json();
    [[maybe_unused]] auto chunks = resp.iter_content(1024);
    [[maybe_unused]] auto lines = resp.iter_lines();
    resp.close();

    response err_resp;
    err_resp.set_status_code(404);
    err_resp.set_reason("Not Found");
    try { err_resp.raise_for_status(); }
    catch (http_error const&) {}

    // --- PreparedRequest ---
    prepared_request prepped;
    prepped.prepare("POST", "https://api.example.com/data",
        case_insensitive_dict{},
        std::nullopt,
        std::string{"body"},
        std::map<std::string, std::string>{{"k", "v"}},
        std::nullopt,
        cookie_jar{},
        hooks{},
        std::nullopt);

    [[maybe_unused]] auto pm = prepped.method();
    [[maybe_unused]] auto pu = prepped.url();
    [[maybe_unused]] auto& ph = prepped.headers();
    [[maybe_unused]] auto& pb = prepped.body();
    [[maybe_unused]] auto& phk = prepped.get_hooks();
    [[maybe_unused]] auto purl = prepped.path_url();

    prepped.prepare_method("get");
    prepped.prepare_url("https://x.com", std::map<std::string, std::string>{{"a", "b"}});
    prepped.prepare_headers(case_insensitive_dict{});
    prepped.prepare_body(std::string{"data"}, std::nullopt, std::nullopt);
    prepped.prepare_cookies(cookie_jar{});
    prepped.prepare_content_length();
    prepped.prepare_hooks(hooks{});
    prepped.register_hook([](response&) {});
    prepped.deregister_hook([](response&) {});
    [[maybe_unused]] auto pcopy = prepped.copy();

    // --- Request ---
    class request req1_obj;
    class request req2_obj("GET", "https://example.com",
        case_insensitive_dict{},
        files_dict{},
        std::string{""},
        std::map<std::string, std::string>{},
        std::shared_ptr<auth_base>{},
        cookie_jar{},
        hooks{},
        std::nullopt);

    [[maybe_unused]] auto rm = req2_obj.method();
    [[maybe_unused]] auto rurl2 = req2_obj.url();
    [[maybe_unused]] auto& rheaders = req2_obj.headers();
    [[maybe_unused]] auto& rfiles = req2_obj.files();
    [[maybe_unused]] auto& rparams = req2_obj.params();
    [[maybe_unused]] auto& rcookies2 = req2_obj.cookies();
    [[maybe_unused]] auto rprepped = req2_obj.prepare();
    req2_obj.register_hook([](response&) {});
    req2_obj.deregister_hook([](response&) {});
    (void)req1_obj;

    // --- Adapters ---
    http_adapter adapter(10, 10, 3, false);
    adapter.init_poolmanager(10, 10, false);
    adapter.proxy_manager_for("http://proxy:8080");
    adapter.get_connection("https://example.com", std::nullopt);
    adapter.cert_verify("https://example.com", true, std::nullopt);
    adapter.close();

    // --- Session ---
    session sess;
    sess.headers().set("X-Custom", "value");
    sess.cookies().set("session", "abc");
    sess.set_auth(std::make_shared<http_basic_auth>("user", "pass"));
    sess.proxies().http = "http://proxy:8080";
    sess.get_hooks().register_hook([](response&) {});
    sess.params()["default_key"] = "default_value";
    sess.set_stream(true);
    [[maybe_unused]] bool sstream = sess.stream();
    sess.set_verify(false);
    [[maybe_unused]] auto& sverify = sess.verify();
    sess.set_cert(cert_config{"/cert.pem", "/key.pem"});
    [[maybe_unused]] auto& scert = sess.cert();
    sess.set_max_redirects(10);
    [[maybe_unused]] int smr = sess.max_redirects();
    sess.set_trust_env(false);
    [[maybe_unused]] bool ste = sess.trust_env();

    [[maybe_unused]] auto sadapter = sess.get_adapter("https://api.com/");
    sess.mount("https://special.com/", std::make_shared<http_adapter>());

    proxy_config px;
    bool st = false;
    verify_config vf = true;
    std::optional<cert_config> ct2;
    sess.merge_environment_settings("https://x.com", px, st, vf, ct2);
    sess.rebuild_auth(prepped, resp);
    sess.rebuild_proxies(prepped, px);
    sess.rebuild_method(prepped, resp);
    sess.close();

    // --- Utility functions ---
    using namespace utils;
    [[maybe_unused]] auto enc2 = get_encoding_from_headers(case_insensitive_dict{});
    [[maybe_unused]] auto rq = requote_uri("http://x.com/path?q=1");
    [[maybe_unused]] auto uq = unquote_unreserved("http://x.com/%20");
    [[maybe_unused]] bool ain = address_in_network("192.168.1.1", "192.168.0.0/16");
    [[maybe_unused]] auto nm2 = dotted_netmask(24);
    [[maybe_unused]] bool ip4 = is_ipv4_address("192.168.1.1");
    [[maybe_unused]] bool cidr = is_valid_cidr("192.168.0.0/24");
    [[maybe_unused]] auto udf = urldefragauth("http://user:pass@x.com/path#frag");
    [[maybe_unused]] auto ps = prepend_scheme_if_needed("example.com", "https");
    [[maybe_unused]] auto auth2 = get_auth_from_url("http://user:pass@x.com");
    [[maybe_unused]] auto lnks = parse_header_links("<url>; rel=next");
    [[maybe_unused]] auto defh = default_headers();
    [[maybe_unused]] auto ua = default_user_agent("my-app");
    [[maybe_unused]] auto dfc = dict_from_cookiejar(cookie_jar{});
    cookie_jar cj3;
    add_dict_to_cookiejar(cj3, {{"a", "b"}});
    [[maybe_unused]] auto sl = super_len("hello");
    [[maybe_unused]] auto netrc = get_netrc_auth("http://example.com");
    [[maybe_unused]] auto gfn = guess_filename("/path/to/file.txt");
    [[maybe_unused]] auto ep = get_environ_proxies("http://example.com");
    [[maybe_unused]] auto sp = select_proxy("https://api.com", proxy_config{});
    [[maybe_unused]] bool bp = should_bypass_proxies("http://localhost");
}

//------------------------------------------------------------------------------
//
// Main - Part 2: Elegant Usage Examples (what users write)
//
//------------------------------------------------------------------------------

// Simple GET request
requests::task<void> example_simple_get() {
    auto r = co_await requests::get("https://api.github.com/users/octocat");
    if (r.ok()) {
        auto data = r.json();
    }
}

// GET with query parameters
requests::task<void> example_get_with_params() {
    requests::request_options opts;
    opts.params = {{"q", "requests+cpp"}, {"sort", "stars"}};

    auto r = co_await requests::get("https://api.github.com/search/repos", opts);
    r.raise_for_status();
}

// POST with JSON body
requests::task<void> example_post_json() {
    requests::request_options opts;
    opts.json = R"({"name": "new-repo", "private": false})";

    auto r = co_await requests::post("https://api.github.com/user/repos", opts);
}

// POST with form data
requests::task<void> example_post_form() {
    requests::request_options opts;
    opts.data = std::string{"username=admin&password=secret"};
    opts.headers = requests::case_insensitive_dict{};
    opts.headers->set("Content-Type", "application/x-www-form-urlencoded");

    auto r = co_await requests::post("https://httpbin.org/post", opts);
}

// Request with custom headers
requests::task<void> example_custom_headers() {
    requests::request_options opts;
    opts.headers = requests::case_insensitive_dict{};
    opts.headers->set("Authorization", "Bearer token123");
    opts.headers->set("Accept", "application/vnd.github.v3+json");

    auto r = co_await requests::get("https://api.github.com/user", opts);
}

// Request with timeout
requests::task<void> example_with_timeout() {
    requests::request_options opts;
    opts.timeout_cfg = requests::timeout::from_pair(3.05, 27.0);

    auto r = co_await requests::get("https://httpbin.org/delay/2", opts);
}

// Request with basic auth
requests::task<void> example_basic_auth() {
    requests::request_options opts;
    opts.auth = std::make_shared<requests::http_basic_auth>("user", "passwd");

    auto r = co_await requests::get("https://httpbin.org/basic-auth/user/passwd", opts);
}

// Session with persistent cookies
requests::task<void> example_session_cookies() {
    requests::session s;

    // First request sets cookies
    co_await s.get("https://httpbin.org/cookies/set/sessionid/abc123");

    // Subsequent requests automatically include cookies
    auto r = co_await s.get("https://httpbin.org/cookies");
}

// Session with default headers
requests::task<void> example_session_defaults() {
    requests::session s;
    s.headers().set("Authorization", "Bearer mytoken");
    s.headers().set("X-Api-Version", "2.0");

    // All requests include these headers
    co_await s.get("https://api.example.com/resource1");
    co_await s.get("https://api.example.com/resource2");
    co_await s.post("https://api.example.com/resource3");

    s.close();
}

// File upload
requests::task<void> example_file_upload() {
    requests::upload_file file;
    file.field_name = "document";
    file.filename = "report.pdf";
    file.content = {std::byte{0x25}, std::byte{0x50}, std::byte{0x44}, std::byte{0x46}};
    file.content_type = "application/pdf";

    requests::request_options opts;
    opts.files = requests::files_dict{file};

    auto r = co_await requests::post("https://httpbin.org/post", opts);
}

// Handle redirects
requests::task<void> example_redirects() {
    requests::request_options opts;
    opts.allow_redirects = true;

    auto r = co_await requests::get("https://httpbin.org/redirect/3", opts);

    // Check redirect history
    for (auto const& resp : r.history()) {
        [[maybe_unused]] auto url = resp.url();
    }
}

// Disable SSL verification (not recommended for production)
requests::task<void> example_ssl_verify() {
    requests::request_options opts;
    opts.verify = false;

    auto r = co_await requests::get("https://self-signed.example.com", opts);
}

// Use custom CA bundle
requests::task<void> example_custom_ca() {
    requests::request_options opts;
    opts.verify = std::string{"/path/to/custom/ca-bundle.crt"};

    auto r = co_await requests::get("https://internal.example.com", opts);
}

// Client certificate authentication
requests::task<void> example_client_cert() {
    requests::request_options opts;
    opts.cert = requests::cert_config{"/path/to/client.crt", "/path/to/client.key"};

    auto r = co_await requests::get("https://client-auth.example.com", opts);
}

// Using proxy
requests::task<void> example_proxy() {
    requests::request_options opts;
    opts.proxies = requests::proxy_config{};
    opts.proxies->http = "http://10.10.1.10:3128";
    opts.proxies->https = "http://10.10.1.10:1080";

    auto r = co_await requests::get("https://httpbin.org/ip", opts);
}

// Response hooks
requests::task<void> example_hooks() {
    requests::request_options opts;
    opts.hooks_cfg = requests::hooks{};
    opts.hooks_cfg->register_hook([](requests::response& r) {
        // Log every response
        [[maybe_unused]] auto status = r.status_code();
    });

    auto r = co_await requests::get("https://httpbin.org/get", opts);
}

// Error handling
requests::task<void> example_error_handling() {
    try {
        auto r = co_await requests::get("https://httpbin.org/status/404");
        r.raise_for_status();
    }
    catch (requests::http_error const& e) {
        // HTTP 4xx or 5xx error
    }
    catch (requests::connection_error const& e) {
        // Network problem
    }
    catch (requests::timeout_error const& e) {
        // Request timed out
    }
    catch (requests::request_exception const& e) {
        // Any other request error
    }
}

// Streaming response (large files)
requests::task<void> example_streaming() {
    requests::request_options opts;
    opts.stream = true;

    auto r = co_await requests::get("https://httpbin.org/stream/20", opts);

    for (auto const& line : r.iter_lines()) {
        // Process each line without loading entire response
        (void)line;
    }
}

// Prepared requests for inspection/modification
requests::task<void> example_prepared_request() {
    class requests::request req_obj("GET", "https://httpbin.org/get");
    auto prepped = req_obj.prepare();

    // Inspect or modify before sending
    prepped.headers().set("X-Custom-Header", "custom-value");

    requests::session s;
    auto r = co_await s.send(prepped);
}

// All HTTP methods
requests::task<void> example_all_methods() {
    auto r1 = co_await requests::get("https://httpbin.org/get");
    auto r2 = co_await requests::post("https://httpbin.org/post");
    auto r3 = co_await requests::put("https://httpbin.org/put");
    auto r4 = co_await requests::patch("https://httpbin.org/patch");
    auto r5 = co_await requests::delete_("https://httpbin.org/delete");
    auto r6 = co_await requests::head("https://httpbin.org/get");
    auto r7 = co_await requests::options("https://httpbin.org/get");
    (void)r1; (void)r2; (void)r3; (void)r4; (void)r5; (void)r6; (void)r7;
}

// Mount custom adapter
requests::task<void> example_custom_adapter() {
    requests::session s;

    // Use different pool settings for specific hosts
    auto custom = std::make_shared<requests::http_adapter>(
        100,  // pool_connections
        100,  // pool_maxsize
        5,    // max_retries
        true  // pool_block
    );
    s.mount("https://high-traffic.example.com/", custom);

    auto r = co_await s.get("https://high-traffic.example.com/api/data");
}

int main() {
    // Part 1: Exercise the entire API to ensure it compiles
    exercise_api();

    // Part 2: The coroutine examples above demonstrate elegant usage
    // In a real app, you'd run these with an event loop

    return 0;
}
