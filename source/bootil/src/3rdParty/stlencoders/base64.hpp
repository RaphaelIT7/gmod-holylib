/*
 * Copyright (c) 2011, 2012 Thomas Kemmer <tkemmer@computer.org>
 *
 * Permission is hereby granted, free of charge, to any person
 * obtaining a copy of this software and associated documentation
 * files (the "Software"), to deal in the Software without
 * restriction, including without limitation the rights to use, copy,
 * modify, merge, publish, distribute, sublicense, and/or sell copies
 * of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS
 * BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN
 * ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#ifndef STLENCODERS_BASE64_HPP
#define STLENCODERS_BASE64_HPP

#include "error.hpp"
#include "lookup.hpp"
#include "traits.hpp"

/**
 * @file
 *
 * Implementation of the Base64 encoding scheme.
 */
namespace stlencoders {
    namespace detail {
        template<char C> struct base64_table_base { enum { value = 0x40 }; };

        template<> struct base64_table_base<'A'> { enum { value = 0x00 }; };
        template<> struct base64_table_base<'B'> { enum { value = 0x01 }; };
        template<> struct base64_table_base<'C'> { enum { value = 0x02 }; };
        template<> struct base64_table_base<'D'> { enum { value = 0x03 }; };
        template<> struct base64_table_base<'E'> { enum { value = 0x04 }; };
        template<> struct base64_table_base<'F'> { enum { value = 0x05 }; };
        template<> struct base64_table_base<'G'> { enum { value = 0x06 }; };
        template<> struct base64_table_base<'H'> { enum { value = 0x07 }; };
        template<> struct base64_table_base<'I'> { enum { value = 0x08 }; };
        template<> struct base64_table_base<'J'> { enum { value = 0x09 }; };
        template<> struct base64_table_base<'K'> { enum { value = 0x0a }; };
        template<> struct base64_table_base<'L'> { enum { value = 0x0b }; };
        template<> struct base64_table_base<'M'> { enum { value = 0x0c }; };
        template<> struct base64_table_base<'N'> { enum { value = 0x0d }; };
        template<> struct base64_table_base<'O'> { enum { value = 0x0e }; };
        template<> struct base64_table_base<'P'> { enum { value = 0x0f }; };
        template<> struct base64_table_base<'Q'> { enum { value = 0x10 }; };
        template<> struct base64_table_base<'R'> { enum { value = 0x11 }; };
        template<> struct base64_table_base<'S'> { enum { value = 0x12 }; };
        template<> struct base64_table_base<'T'> { enum { value = 0x13 }; };
        template<> struct base64_table_base<'U'> { enum { value = 0x14 }; };
        template<> struct base64_table_base<'V'> { enum { value = 0x15 }; };
        template<> struct base64_table_base<'W'> { enum { value = 0x16 }; };
        template<> struct base64_table_base<'X'> { enum { value = 0x17 }; };
        template<> struct base64_table_base<'Y'> { enum { value = 0x18 }; };
        template<> struct base64_table_base<'Z'> { enum { value = 0x19 }; };
        template<> struct base64_table_base<'a'> { enum { value = 0x1a }; };
        template<> struct base64_table_base<'b'> { enum { value = 0x1b }; };
        template<> struct base64_table_base<'c'> { enum { value = 0x1c }; };
        template<> struct base64_table_base<'d'> { enum { value = 0x1d }; };
        template<> struct base64_table_base<'e'> { enum { value = 0x1e }; };
        template<> struct base64_table_base<'f'> { enum { value = 0x1f }; };
        template<> struct base64_table_base<'g'> { enum { value = 0x20 }; };
        template<> struct base64_table_base<'h'> { enum { value = 0x21 }; };
        template<> struct base64_table_base<'i'> { enum { value = 0x22 }; };
        template<> struct base64_table_base<'j'> { enum { value = 0x23 }; };
        template<> struct base64_table_base<'k'> { enum { value = 0x24 }; };
        template<> struct base64_table_base<'l'> { enum { value = 0x25 }; };
        template<> struct base64_table_base<'m'> { enum { value = 0x26 }; };
        template<> struct base64_table_base<'n'> { enum { value = 0x27 }; };
        template<> struct base64_table_base<'o'> { enum { value = 0x28 }; };
        template<> struct base64_table_base<'p'> { enum { value = 0x29 }; };
        template<> struct base64_table_base<'q'> { enum { value = 0x2a }; };
        template<> struct base64_table_base<'r'> { enum { value = 0x2b }; };
        template<> struct base64_table_base<'s'> { enum { value = 0x2c }; };
        template<> struct base64_table_base<'t'> { enum { value = 0x2d }; };
        template<> struct base64_table_base<'u'> { enum { value = 0x2e }; };
        template<> struct base64_table_base<'v'> { enum { value = 0x2f }; };
        template<> struct base64_table_base<'w'> { enum { value = 0x30 }; };
        template<> struct base64_table_base<'x'> { enum { value = 0x31 }; };
        template<> struct base64_table_base<'y'> { enum { value = 0x32 }; };
        template<> struct base64_table_base<'z'> { enum { value = 0x33 }; };
        template<> struct base64_table_base<'0'> { enum { value = 0x34 }; };
        template<> struct base64_table_base<'1'> { enum { value = 0x35 }; };
        template<> struct base64_table_base<'2'> { enum { value = 0x36 }; };
        template<> struct base64_table_base<'3'> { enum { value = 0x37 }; };
        template<> struct base64_table_base<'4'> { enum { value = 0x38 }; };
        template<> struct base64_table_base<'5'> { enum { value = 0x39 }; };
        template<> struct base64_table_base<'6'> { enum { value = 0x3a }; };
        template<> struct base64_table_base<'7'> { enum { value = 0x3b }; };
        template<> struct base64_table_base<'8'> { enum { value = 0x3c }; };
        template<> struct base64_table_base<'9'> { enum { value = 0x3d }; };

        template<char C> struct base64_table : public base64_table_base<C> { };
        template<> struct base64_table<'+'> { enum { value = 0x3e }; };
        template<> struct base64_table<'/'> { enum { value = 0x3f }; };

        template<char C> struct base64url_table : public base64_table_base<C> { };
        template<> struct base64url_table<'-'> { enum { value = 0x3e }; };
        template<> struct base64url_table<'_'> { enum { value = 0x3f }; };
    }

    /**
     * @em %base64 character encoding traits class template.
     *
     * @tparam charT the encoding character type
     */
    template<class charT> struct base64_traits;


    /**
     * Character encoding traits specialization for @c char.
     *
     * This character encoding traits class defines the encoding
     * alphabet for the @em %base64 encoding scheme as defined in RFC
     * 4648 for the encoding character type @c char.
     */
    template<>
    struct base64_traits<char> {
        /**
         * The encoding character type.
         */
        typedef char char_type;

        /**
         * An integral type representing an octet.
         */
    	typedef unsigned char int_type;

        /**
         * Returns whether the character @a lhs is to be treated equal
         * to the character @a rhs.
         */
        static bool eq(const char_type& lhs, const char_type& rhs) {
            return lhs == rhs;
        }

        /**
         * Returns whether the integral value @a lhs is to be treated
         * equal to @a rhs.
         */
        static bool eq_int_type(const int_type& lhs, const int_type& rhs) {
            return lhs == rhs;
        }

        /**
         * Returns the character representation of a 6-bit value.
         */
    	static char_type to_char_type(const int_type& c) {
            return "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/"[c];
        }

        /**
         * Returns the 6-bit value represented by a character, or
         * inv() for characters not in the encoding alphabet.
         */
    	static int_type to_int_type(const char_type& c) {
            return lookup<detail::base64_table, int_type>(c);
    	}

        /**
         * Returns an integral value represented by no character in
         * the encoding alphabet.
         */
        static int_type inv() {
            return detail::base64url_table<'\0'>::value;
        }

        /**
         * Returns the character used to perform padding at the end of
         * a character range.
         */
    	static char_type pad() {
            return '=';
    	}
    };

    /**
     * Character encoding traits specialization for @c wchar_t.
     *
     * This character encoding traits class defines the encoding
     * alphabet for the @em %base64 encoding scheme as defined in RFC
     * 4648 for the encoding character type @c wchar_t.
     */
    template<>
    struct base64_traits<wchar_t>
    : public portable_wchar_encoding_traits<base64_traits<char> > {
    };

    /**
     * @em %base64hex character encoding traits class template.
     *
     * @tparam charT the encoding character type
     */
    template<class charT> struct base64url_traits;

    /**
     * Character encoding traits specialization for @c char.
     *
     * This character encoding traits class defines the encoding
     * alphabet for the @em %base64url encoding scheme as defined in
     * RFC 4648 for the encoding character type @c char.
     */
    template<>
    struct base64url_traits<char> {
        /**
         * The encoding character type.
         */
        typedef char char_type;

        /**
         * An integral type representing an octet.
         */
    	typedef unsigned char int_type;

        /**
         * Returns whether the character @a lhs is to be treated equal
         * to the character @a rhs.
         */
        static bool eq(const char_type& lhs, const char_type& rhs) {
            return lhs == rhs;
        }

        /**
         * Returns whether the integral value @a lhs is to be treated
         * equal to @a rhs.
         */
        static bool eq_int_type(const int_type& lhs, const int_type& rhs) {
            return lhs == rhs;
        }

        /**
         * Returns the character representation of a 6-bit value.
         */
    	static char_type to_char_type(const int_type& c) {
            return "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-_"[c];
        }

        /**
         * Returns the 6-bit value represented by a character, or
         * inv() for characters not in the encoding alphabet.
         */
    	static int_type to_int_type(const char_type& c) {
            return lookup<detail::base64url_table, int_type>(c);
    	}

        /**
         * Returns an integral value represented by no character in
         * the encoding alphabet.
         */
        static int_type inv() {
            return detail::base64url_table<'\0'>::value;
        }

        /**
         * Returns the character used to perform padding at the end of
         * a character range.
         */
    	static char_type pad() {
            return '=';
    	}
    };

    /**
     * Character encoding traits specialization for @c wchar_t.
     *
     * This character encoding traits class defines the encoding
     * alphabet for the @em %base64url encoding scheme as defined in
     * RFC 4648 for the encoding character type @c wchar_t.
     */

    template<>
    struct base64url_traits<wchar_t>
    : public portable_wchar_encoding_traits<base64url_traits<char> > {
    };

    /**
     * This class template implements the Base64 encoding as defined
     * in RFC 4648 for a given character type and encoding alphabet.
     *
     * The Base64 encoding is designed to represent arbitrary
     * sequences of octets in a form that allows the use of both
     * upper- and lowercase letters but that need not be human
     * readable.
     *
     * The encoding process represents 24-bit groups of input data as
     * output strings of 4 encoded characters.  Proceeding from left
     * to right, a 24-bit input group is formed by concatenating 3
     * 8-bit input groups.  These 24 bits are then treated as 4
     * concatenated 6-bit groups, each of which is translated into a
     * single character in the Base64 alphabet.
     *
     * @tparam charT the encoding character type
     *
     * @tparam traits the character encoding traits type
     */
    template<class charT, class traits = base64_traits<charT> >
    class base64 {
    private:
        struct noskip { };

    public:
        /**
         * The encoding character type.
         */
        typedef charT char_type;

        /**
         * The character encoding traits type.
         */
    	typedef traits traits_type;

        /**
         * An integral type representing an octet.
         */
        typedef typename traits::int_type int_type;

        /**
         * Encodes a range of octets.
         *
         * @tparam InputIterator an iterator type satisfying input
         * iterator requirements and referring to elements implicitly
         * convertible to int_type
         *
         * @tparam OutputIterator an iterator type satisfying output
         * iterator requirements
         *
         * @param first an input iterator to the first position in the
         * octet range to be encoded
         *
         * @param last an input iterator to the final position in the
         * octet range to be encoded
         *
         * @param result an output iterator to the encoded character
         * range
         *
         * @param pad if @c true, performs padding at the end of the
         * encoded character range
         *
         * @return an output iterator referring to one past the last
         * value assigned to the output range
         */
    	template<class InputIterator, class OutputIterator>
        static OutputIterator encode(
            InputIterator first, InputIterator last, OutputIterator result,
            bool pad = true
            )
        {
            for (; first != last; ++first) {
                int_type c0 = *first;
                *result = traits::to_char_type((c0 & 0xff) >> 2);
                ++result;

                if (++first == last) {
                    *result++ = traits::to_char_type((c0 & 0x03) << 4);
                    return pad ? pad_n(result, 2) : result;
                }

                int_type c1 = *first;
                *result = traits::to_char_type((c0 & 0x03) << 4 | (c1 & 0xff) >> 4);
                ++result;

                if (++first == last) {
                    *result++ = traits::to_char_type((c1 & 0x0f) << 2);
                    return pad ? pad_n(result, 1) : result;
                }

                int_type c2 = *first;
                *result = traits::to_char_type((c1 & 0x0f) << 2 | (c2 & 0xff) >> 6);
                ++result;
                *result = traits::to_char_type((c2 & 0x3f));
                ++result;
            }

            return result;
        }

        /**
         * Decodes a range of characters.
         *
         * @tparam InputIterator an iterator type satisfying input
         * iterator requirements and referring to elements implicitly
         * convertible to char_type
         *
         * @tparam OutputIterator an iterator type satisfying output
         * iterator requirements
         *
         * @param first an input iterator to the first position in the
         * character range to be decoded
         *
         * @param last an input iterator to the final position in the
         * character range to be decoded
         *
         * @param result an output iterator to the decoded octet range
         *
         * @return an output iterator referring to one past the last
         * value assigned to the output range
         *
         * @throw invalid_character if a character not in the encoding
         * alphabet is encountered
         *
         * @throw invalid_length if the input range contains an
         * invalid number of encoding characters
         */
    	template<class InputIterator, class OutputIterator>
        static OutputIterator decode(
            InputIterator first, InputIterator last, OutputIterator result
            )
    	{
            return decode(first, last, result, noskip());
        }

        /**
         * Decodes a range of characters.
         *
         * For every character @c c not in the encoding alphabet,
         * - if @a skip(c) evaluates to @c true, the character is
         *   ignored
         * - if @c c equals @c traits::pad(), returns
         * - otherwise, throws invalid_character
         *
         * @tparam InputIterator an iterator type satisfying input
         * iterator requirements and referring to elements implicitly
         * convertible to char_type
         *
         * @tparam OutputIterator an iterator type satisfying output
         * iterator requirements
         *
         * @tparam Predicate a predicate type
         *
         * @param first an input iterator to the first position in the
         * character range to be decoded
         *
         * @param last an input iterator to the final position in the
         * character range to be decoded
         *
         * @param result an output iterator to the decoded octet range
         *
         * @param skip a function object that, when applied to a value
         * of type char_type, returns a value testable as @c true
         *
         * @return an output iterator referring to one past the last
         * value assigned to the output range
         *
         * @throw invalid_character if a character not in the encoding
         * alphabet is encountered
         *
         * @throw invalid_length if the input range contains an
         * invalid number of encoding characters
         */
    	template<class InputIterator, class OutputIterator, class Predicate>
        static OutputIterator decode(
            InputIterator first, InputIterator last, OutputIterator result,
            Predicate skip
            )
        {
            for (;;) {
                int_type c0 = seek(first, last, skip);
                if (traits::eq_int_type(c0, traits::inv())) {
                    return result;
                }

                int_type c1 = seek(first, last, skip);
                if (traits::eq_int_type(c1, traits::inv())) {
                    throw invalid_length("base64 decode error");
                }

                *result = c0 << 2 | c1 >> 4;
                ++result;

                int_type c2 = seek(first, last, skip);
                if (traits::eq_int_type(c2, traits::inv())) {
                    return result;
                }

                *result = (c1 & 0x0f) << 4 | c2 >> 2;
                ++result;

                int_type c3 = seek(first, last, skip);
                if (traits::eq_int_type(c3, traits::inv())) {
                    return result;
                }

                *result = (c2 & 0x03) << 6 | c3;
                ++result;
            }
        }

        /**
         * Computes the maximum length of an encoded character
         * sequence.
         *
         * @tparam sizeT an integral type
         *
         * @param n the length of the input octet sequence
         *
         * @return the maximum length of the encoded character
         * sequence
         */
        template<class sizeT>
        static sizeT max_encode_size(sizeT n) {
            return (n + 2) / 3 * 4;
        }

        /**
         * Computes the maximum length of a decoded octet sequence.
         *
         * @tparam sizeT an integral type
         *
         * @param n the length of the input character sequence
         *
         * @return the maximum length of the decoded octet sequence
         */
        template<class sizeT>
        static sizeT max_decode_size(sizeT n) {
            return (n + 3) / 4 * 3;
        }

    private:
        template<class OutputIterator, class sizeT>
        static OutputIterator pad_n(OutputIterator result, sizeT n) {
            for (; n > 0; --n) {
                *result++ = traits::pad();
            }
            return result;
        }

        template<class InputIterator, class Predicate>
        static int_type seek(
            InputIterator& first, const InputIterator& last, Predicate& skip
            )
        {
            while (first != last) {
                char_type c = *first;
                ++first;

                int_type v = traits::to_int_type(c);
                if (!traits::eq_int_type(v, traits::inv())) {
                    return v;
                } else if (skip(c)) {
                    continue;
                } else if (traits::eq(c, traits::pad())) {
                    break;
                } else {
                    throw invalid_character("base64 decode error");
                }
            }

            return traits::inv();
        }

        template<class InputIterator>
        static int_type seek(
            InputIterator& first, const InputIterator& last, noskip&
            )
        {
            if (first != last) {
                char_type c = *first;
                ++first;

                int_type v = traits::to_int_type(c);
                if (!traits::eq_int_type(v, traits::inv())) {
                    return v;
                } else if (!traits::eq(c, traits::pad())) {
                    throw invalid_character("base64 decode error");
                }
            }

            return traits::inv();
        }
    };
}

#endif
