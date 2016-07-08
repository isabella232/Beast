//
// Copyright (c) 2013-2016 Vinnie Falco (vinnie dot falco at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

// Test that header file is self-contained.
#include <beast/core/detail/zlib/zlib.h>

#include <beast/unit_test/suite.hpp>
#include <cassert>
#include <memory>
#include <random>

namespace beast {
namespace detail {

class zlib_test : public beast::unit_test::suite
{
public:
    class buffer
    {
        std::size_t size_ = 0;
        std::size_t capacity_ = 0;
        std::unique_ptr<std::uint8_t[]> p_;

    public:
        buffer() = default;
        buffer(buffer&&) = default;
        buffer& operator=(buffer&&) = default;


        explicit
        buffer(std::size_t capacity)
        {
            reserve(capacity);
        }

        bool
        empty() const
        {
            return size_ == 0;
        }

        std::size_t
        size() const
        {
            return size_;
        }

        std::size_t
        capacity() const
        {
            return capacity_;
        }

        std::uint8_t const*
        data() const
        {
            return p_.get();
        }

        std::uint8_t*
        data()
        {
            return p_.get();
        }

        void
        reserve(std::size_t capacity)
        {
            if(capacity != capacity_)
            {
                p_.reset(new std::uint8_t[capacity]);
                capacity_ = capacity;
            }
        }

        void
        resize(std::size_t size)
        {
            assert(size <= capacity_);
            size_ = size;
        }
    };

    buffer
    make_source1(std::size_t size)
    {
        std::mt19937 rng;
        buffer b(size);
        auto p = b.data();
        std::size_t n = 0;
        static std::string const chars(
            "01234567890{}\"ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz"
            "{{{{{{{{{{}}}}}}}}}}  ");
        while(n < size)
        {
            *p++ = chars[rng()%chars.size()];
            ++n;
        }
        b.resize(n);
        return b;
    }

    buffer
    make_source2(std::size_t size)
    {
        std::mt19937 rng;
        buffer b(size);
        auto p = b.data();
        std::size_t n = 0;
        while(n < size)
        {
            *p++ = rng()%256;
            ++n;
        }
        b.resize(n);
        return b;
    }

    void
    checkInflate(buffer const& input,
        buffer const& dict, buffer const& original)
    {
        for(std::size_t i = 0; i < input.size(); ++i)
        {
            buffer output(original.size());
            z_stream zs;
            zs.zalloc = Z_NULL;
            zs.zfree = Z_NULL;
            zs.opaque = Z_NULL;
            zs.avail_in = 0;
            zs.next_in = Z_NULL;
            expect(inflateInit2(&zs, 15) == Z_OK);
            if(! dict.empty())
                expect(inflateSetDictionary(&zs,
                    dict.data(), dict.size()) == Z_OK);
            zs.next_out = output.data();
            zs.avail_out = output.capacity();
            if(i > 0)
            {
                zs.next_in = (Bytef*)input.data();
                zs.avail_in = i;
                auto result = inflate(&zs, Z_FULL_FLUSH);
                expect(result == Z_OK);
            }
            zs.next_in = (Bytef*)input.data() + i;
            zs.avail_in = input.size() - i;
            auto result = inflate(&zs, Z_FULL_FLUSH);
            output.resize(output.capacity() - zs.avail_out);
            expect(result == Z_OK);
            expect(output.size() == original.size());
            expect(std::memcmp(
                output.data(), original.data(), original.size()) == 0);
            inflateEnd(&zs);
        }
    }

    void testCompress()
    {
        static std::size_t constexpr N = 2048;
        for(int source = 0; source <= 1; ++source)
        {
            buffer original;
            switch(source)
            {
            case 0:
                original = make_source1(N);
                break;
            case 1:
                original = make_source2(N);
                break;
            }
            for(int dictno = 0; dictno <= 1; ++dictno)
            {
                std::string const s{
                    "01234567890{}\"ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz{} "};
                buffer dict;
                switch(dictno)
                {
                case 0:
                    break;
                case 1:
                {
                    dict.reserve(s.size());
                    std::memcpy(dict.data(), s.data(), s.size());
                    dict.resize(s.size());
                    break;
                }
                }
                for(int level = 0; level <= 9; ++level)
                {
                    for(int strategy = 0; strategy <= 4; ++strategy)
                    {
                        z_stream zs;
                        zs.zalloc = Z_NULL;
                        zs.zfree = Z_NULL;
                        zs.opaque = Z_NULL;
                        zs.avail_in = 0;
                        zs.next_in = Z_NULL;
                        expect(deflateInit2(&zs, 
                            level,
                            Z_DEFLATED,
                            -15,
                            4,
                            strategy) == Z_OK);
                        if(! dict.empty())
                            expect(deflateSetDictionary(&zs,
                                dict.data(), dict.size()) == Z_OK);
                        buffer output(deflateBound(&zs, original.size()));
                        zs.next_in = (Bytef*)original.data();
                        zs.avail_in = original.size();
                        zs.next_out = output.data();
                        zs.avail_out = output.capacity();
                        auto result = deflate(&zs, Z_FULL_FLUSH);
                        deflateEnd(&zs);
                        expect(result == Z_OK);
                        output.resize(output.capacity() - zs.avail_out);
                        checkInflate(output, dict, original);
                    }
                }
            }
        }
    }

    void run() override
    {
        testCompress();
    }
};

BEAST_DEFINE_TESTSUITE(zlib,core,beast);

} // detail
} // beast

