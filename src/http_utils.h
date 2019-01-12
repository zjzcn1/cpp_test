#pragma once

#include <memory>
#include <string>
#include <unordered_map>

#include <boost/beast/core.hpp>

namespace http_server {
    namespace beast = boost::beast;

    class HttpUtils {
    public:
        static beast::string_view mime_type(beast::string_view path) {
            using beast::iequals;
            auto const ext = [&path] {
                auto const pos = path.rfind(".");
                if (pos == beast::string_view::npos)
                    return beast::string_view{};
                return path.substr(pos);
            }();
            if (iequals(ext, ".htm")) return "text/html";
            if (iequals(ext, ".html")) return "text/html";
            if (iequals(ext, ".php")) return "text/html";
            if (iequals(ext, ".css")) return "text/css";
            if (iequals(ext, ".txt")) return "text/plain";
            if (iequals(ext, ".js")) return "application/javascript";
            if (iequals(ext, ".json")) return "application/json";
            if (iequals(ext, ".xml")) return "application/xml";
            if (iequals(ext, ".swf")) return "application/x-shockwave-flash";
            if (iequals(ext, ".flv")) return "video/x-flv";
            if (iequals(ext, ".png")) return "image/png";
            if (iequals(ext, ".jpe")) return "image/jpeg";
            if (iequals(ext, ".jpeg")) return "image/jpeg";
            if (iequals(ext, ".jpg")) return "image/jpeg";
            if (iequals(ext, ".gif")) return "image/gif";
            if (iequals(ext, ".bmp")) return "image/bmp";
            if (iequals(ext, ".ico")) return "image/vnd.microsoft.icon";
            if (iequals(ext, ".tiff")) return "image/tiff";
            if (iequals(ext, ".tif")) return "image/tiff";
            if (iequals(ext, ".svg")) return "image/svg+xml";
            if (iequals(ext, ".svgz")) return "image/svg+xml";
            return "application/text";
        }

        // serialize path and query parameters to the target
        static std::string params_serialize(std::unordered_multimap<std::string, std::string> &param) {
            std::string path;
            if (!param.empty()) {
                path += "?";
                auto it = param.begin();
                for (; it != param.end(); ++it) {
                    path += url_encode(it->first) + "=" + url_encode(it->second) + "&";
                }
                path.pop_back();
            }
            return path;
        }

        static std::unordered_multimap<std::string, std::string> params_parse(std::string &path) {
            // separate the query params
            auto param_split = split(path, "?", 1);

            std::unordered_multimap<std::string, std::string> params;
            // set params
            if (param_split.size() == 2) {
                auto kv = split(param_split.at(1), "&");

                for (auto const &e : kv) {
                    if (e.empty()) {
                        continue;
                    }

                    auto k_v = split(e, "=", 1);
                    if (k_v.size() == 1) {
                        params.emplace(url_decode(e), "");
                    } else if (k_v.size() == 2) {
                        params.emplace(url_decode(k_v.at(0)), url_decode(k_v.at(1)));
                    }
                }
            }
            return params;
        }

        static std::string path_cat(std::string &base, std::basic_string<char, std::char_traits<char>, std::allocator<char>> path) {
            if (base.empty()) {
                return path;
            }

            std::string result = base;
#if BOOST_MSVC
            char constexpr path_separator = '\\';
        if(result.back() == path_separator)
            result.resize(result.size() - 1);
        result.append(path.data(), path.size());
        for(auto& c : result)
            if(c == '/')
                c = path_separator;
#else
            char constexpr path_separator = '/';
            if (result.back() == path_separator)
                result.resize(result.size() - 1);
            result.append(path.data(), path.size());
#endif
            return result;
        }

        static std::string hex_encode(char const c) {
            char s[3];

            if (c & 0x80) {
                std::snprintf(&s[0], 3, "%02X",
                              static_cast<unsigned int>(c & 0xff)
                );
            } else {
                std::snprintf(&s[0], 3, "%02X",
                              static_cast<unsigned int>(c)
                );
            }

            return std::string(s);
        }


        static char hex_decode(std::string const &s) {
            unsigned int n;

            std::sscanf(s.data(), "%x", &n);

            return static_cast<char>(n);
        }

        static std::string url_encode(std::string const &str) {
            std::string res;
            res.reserve(str.size());

            for (auto const &e : str) {
                if (e == ' ') {
                    res += "+";
                } else if (std::isalnum(static_cast<unsigned char>(e)) ||
                           e == '-' || e == '_' || e == '.' || e == '~') {
                    res += e;
                } else {
                    res += "%" + hex_encode(e);
                }
            }

            return res;
        }

        static std::string url_decode(std::string const &str) {
            std::string res;
            res.reserve(str.size());

            for (std::size_t i = 0; i < str.size(); ++i) {
                if (str[i] == '+') {
                    res += " ";
                } else if (str[i] == '%' && i + 2 < str.size() &&
                           std::isxdigit(static_cast<unsigned char>(str[i + 1])) &&
                           std::isxdigit(static_cast<unsigned char>(str[i + 2]))) {
                    res += hex_decode(str.substr(i + 1, 2));
                    i += 2;
                } else {
                    res += str[i];
                }
            }

            return res;
        }

        static std::vector<std::string> split(std::string const &str,
                                              std::string const &delim,
                                              std::size_t times = std::numeric_limits<std::size_t>::max()) {
            std::vector<std::string> vtok;
            std::size_t start{0};
            auto end = str.find(delim);

            while ((times-- > 0) && (end != std::string::npos)) {
                vtok.emplace_back(str.substr(start, end - start));
                start = end + delim.length();
                end = str.find(delim, start);
            }
            vtok.emplace_back(str.substr(start, end));

            return vtok;
        }
    };
}