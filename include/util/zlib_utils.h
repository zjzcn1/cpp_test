#pragma once

#include <boost/iostreams/filter/zlib.hpp>
#include <boost/iostreams/filtering_streambuf.hpp>
#include <boost/iostreams/device/back_inserter.hpp>
#include <boost/iostreams/copy.hpp>

namespace util {

    class ZlibUtils {
    public:

        static void compress(const char *in, size_t size, std::vector<char> &out) {
            boost::iostreams::filtering_streambuf<boost::iostreams::output> ios;
            ios.push(boost::iostreams::zlib_compressor(boost::iostreams::zlib::best_compression));
            ios.push(boost::iostreams::back_inserter(out));
            boost::iostreams::copy(boost::iostreams::basic_array_source<char>(in, size), ios);
        }

        static void compress(const std::string &in, std::vector<char> &out) {
            compress(&in[0], out);
        }

        static void compress(const std::vector<char> &in, std::vector<char> &out) {
            compress(&in[0], out);
        }

        static void decompress(const char *in, size_t size, std::vector<char> &out) {
            boost::iostreams::filtering_streambuf<boost::iostreams::output> ios;
            ios.push(boost::iostreams::zlib_decompressor());
            ios.push(boost::iostreams::back_inserter(out));
            boost::iostreams::copy(boost::iostreams::basic_array_source<char>(in, size), ios);
        }

        static void decompress(const std::string &in, std::vector<char> &out) {
            decompress(&in[0], out);
        }

        static void decompress(const std::vector<char> &in, std::vector<char> &out) {
            decompress(&in[0], out);
        }
    };
}
