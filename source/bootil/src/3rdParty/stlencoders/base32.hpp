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

#ifndef STLENCODERS_BASE32_HPP
#define STLENCODERS_BASE32_HPP

#include "error.hpp"
#include "lookup.hpp"
#include "traits.hpp"

/**
 * @file
 *
 * Implementation of the Base32 encoding scheme.
 */
namespace stlencoders {
    namespace detail {
        template<char C> struct base32_table { enum { value = 0x20 }; };

        template<> struct base32_table<'A'> { enum { value = 0x00 }; };
        template<> struct base32_table<'B'> { enum { value = 0x01 }; };
        template<> struct base32_table<'C'> { enum { value = 0x02 }; };
        template<> struct base32_table<'D'> { enum { value = 0x03 }; };
        template<> struct base32_table<'E'> { enum { value = 0x04 }; };
        template<> struct base32_table<'F'> { enum { value = 0x05 }; };
        template<> struct base32_table<'G'> { enum { value = 0x06 }; };
        template<> struct base32_table<'H'> { enum { value = 0x07 }; };
        template<> struct base32_table<'I'> { enum { value = 0x08 }; };
        template<> struct base32_table<'J'> { enum { value = 0x09 }; };
        template<> struct base32_table<'K'> { enum { value = 0x0a }; };
        template<> struct base32_table<'L'> { enum { value = 0x0b }; };
        template<> struct base32_table<'M'> { enum { value = 0x0c }; };
        template<> struct base32_table<'N'> { enum { value = 0x0d }; };
        template<> struct base32_table<'O'> { enum { value = 0x0e }; };
        template<> struct base32_table<'P'> { enum { value = 0x0f }; };
        template<> struct base32_table<'Q'> { enum { value = 0x10 }; };
        template<> struct base32_table<'R'> { enum { value = 0x11 }; };
        template<> struct base32_table<'S'> { enum { value = 0x12 }; };
        template<> struct base32_table<'T'> { enum { value = 0x13 }; };
        template<> struct base32_table<'U'> { enum { value = 0x14 }; };
        template<> struct base32_table<'V'> { enum { value = 0x15 }; };
        template<> struct base32_table<'W'> { enum { value = 0x16 }; };
        template<> struct base32_table<'X'> { enum { value = 0x17 }; };
        template<> struct base32_table<'Y'> { enum { value = 0x18 }; };
        template<> struct base32_table<'Z'> { enum { value = 0x19 }; };
        template<> struct base32_table<'2'> { enum { value = 0x1a }; };
        template<> struct base32_table<'3'> { enum { value = 0x1b }; };
        template<> struct base32_table<'4'> { enum { value = 0x1c }; };
        template<> struct base32_table<'5'> { enum { value = 0x1d }; };
        template<> struct base32_table<'6'> { enum { value = 0x1e }; };
        template<> struct base32_table<'7'> { enum { value = 0x1f }; };

        template<> struct base32_table<'a'> { enum { value = base32_table<'A'>::value }; };
        template<> struct base32_table<'b'> { enum { value = base32_table<'B'>::value }; };
        template<> struct base32_table<'c'> { enum { value = base32_table<'C'>::value }; };
        template<> struct base32_table<'d'> { enum { value = base32_table<'D'>::value }; };
        template<> struct base32_table<'e'> { enum { value = base32_table<'E'>::value }; };
        template<> struct base32_table<'f'> { enum { value = base32_table<'F'>::value }; };
        template<> struct base32_table<'g'> { enum { value = base32_table<'G'>::value }; };
        template<> struct base32_table<'h'> { enum { value = base32_table<'H'>::value }; };
        template<> struct base32_table<'i'> { enum { value = base32_table<'I'>::value }; };
        template<> struct base32_table<'j'> { enum { value = base32_table<'J'>::value }; };
        template<> struct base32_table<'k'> { enum { value = base32_table<'K'>::value }; };
        template<> struct base32_table<'l'> { enum { value = base32_table<'L'>::value }; };
        template<> struct base32_table<'m'> { enum { value = base32_table<'M'>::value }; };
        template<> struct base32_table<'n'> { enum { value = base32_table<'N'>::value }; };
        template<> struct base32_table<'o'> { enum { value = base32_table<'O'>::value }; };
        template<> struct base32_table<'p'> { enum { value = base32_table<'P'>::value }; };
        template<> struct base32_table<'q'> { enum { value = base32_table<'Q'>::value }; };
        template<> struct base32_table<'r'> { enum { value = base32_table<'R'>::value }; };
        template<> struct base32_table<'s'> { enum { value = base32_table<'S'>::value }; };
        template<> struct base32_table<'t'> { enum { value = base32_table<'T'>::value }; };
        template<> struct base32_table<'u'> { enum { value = base32_table<'U'>::value }; };
        template<> struct base32_table<'v'> { enum { value = base32_table<'V'>::value }; };
        template<> struct base32_table<'w'> { enum { value = base32_table<'W'>::value }; };
        template<> struct base32_table<'x'> { enum { value = base32_table<'X'>::value }; };
        template<> struct base32_table<'y'> { enum { value = base32_table<'Y'>::value }; };
        template<> struct base32_table<'z'> { enum { value = base32_table<'Z'>::value }; };

        template<char C> struct base32hex_table { enum { value = 0x20 }; };

        template<> struct base32hex_table<'0'> { enum { value = 0x00 }; };
        template<> struct base32hex_table<'1'> { enum { value = 0x01 }; };
        template<> struct base32hex_table<'2'> { enum { value = 0x02 }; };
        template<> struct base32hex_table<'3'> { enum { value = 0x03 }; };
        template<> struct base32hex_table<'4'> { enum { value = 0x04 }; };
        template<> struct base32hex_table<'5'> { enum { value = 0x05 }; };
        template<> struct base32hex_table<'6'> { enum { value = 0x06 }; };
        template<> struct base32hex_table<'7'> { enum { value = 0x07 }; };
        template<> struct base32hex_table<'8'> { enum { value = 0x08 }; };
        template<> struct base32hex_table<'9'> { enum { value = 0x09 }; };
        template<> struct base32hex_table<'A'> { enum { value = 0x0a }; };
        template<> struct base32hex_table<'B'> { enum { value = 0x0b }; };
        template<> struct base32hex_table<'C'> { enum { value = 0x0c }; };
        template<> struct base32hex_table<'D'> { enum { value = 0x0d }; };
        template<> struct base32hex_table<'E'> { enum { value = 0x0e }; };
        template<> struct base32hex_table<'F'> { enum { value = 0x0f }; };
        template<> struct base32hex_table<'G'> { enum { value = 0x10 }; };
        template<> struct base32hex_table<'H'> { enum { value = 0x11 }; };
        template<> struct base32hex_table<'I'> { enum { value = 0x12 }; };
        template<> struct base32hex_table<'J'> { enum { value = 0x13 }; };
        template<> struct base32hex_table<'K'> { enum { value = 0x14 }; };
        template<> struct base32hex_table<'L'> { enum { value = 0x15 }; };
        template<> struct base32hex_table<'M'> { enum { value = 0x16 }; };
        template<> struct base32hex_table<'N'> { enum { value = 0x17 }; };
        template<> struct base32hex_table<'O'> { enum { value = 0x18 }; };
        template<> struct base32hex_table<'P'> { enum { value = 0x19 }; };
        template<> struct base32hex_table<'Q'> { enum { value = 0x1a }; };
        template<> struct base32hex_table<'R'> { enum { value = 0x1b }; };
        template<> struct base32hex_table<'S'> { enum { value = 0x1c }; };
        template<> struct base32hex_table<'T'> { enum { value = 0x1d }; };
        template<> struct base32hex_table<'U'> { enum { value = 0x1e }; };
        template<> struct base32hex_table<'V'> { enum { value = 0x1f }; };

        template<> struct base32hex_table<'a'> { enum { value = base32hex_table<'A'>::value }; };
        template<> struct base32hex_table<'b'> { enum { value = base32hex_table<'B'>::value }; };
        template<> struct base32hex_table<'c'> { enum { value = base32hex_table<'C'>::value }; };
        template<> struct base32hex_table<'d'> { enum { value = base32hex_table<'D'>::value }; };
        template<> struct base32hex_table<'e'> { enum { value = base32hex_table<'E'>::value }; };
        template<> struct base32hex_table<'f'> { enum { value = base32hex_table<'F'>::value }; };
        template<> struct base32hex_table<'g'> { enum { value = base32hex_table<'G'>::value }; };
        template<> struct base32hex_table<'h'> { enum { value = base32hex_table<'H'>::value }; };
        template<> struct base32hex_table<'i'> { enum { value = base32hex_table<'I'>::value }; };
        template<> struct base32hex_table<'j'> { enum { value = base32hex_table<'J'>::value }; };
        template<> struct base32hex_table<'k'> { enum { value = base32hex_table<'K'>::value }; };
        template<> struct base32hex_table<'l'> { enum { value = base32hex_table<'L'>::value }; };
        template<> struct base32hex_table<'m'> { enum { value = base32hex_table<'M'>::value }; };
        template<> struct base32hex_table<'n'> { enum { value = base32hex_table<'N'>::value }; };
        template<> struct base32hex_table<'o'> { enum { value = base32hex_table<'O'>::value }; };
        template<> struct base32hex_table<'p'> { enum { value = base32hex_table<'P'>::value }; };
        template<> struct base32hex_table<'q'> { enum { value = base32hex_table<'Q'>::value }; };
        template<> struct base32hex_table<'r'> { enum { value = base32hex_table<'R'>::value }; };
        template<> struct base32hex_table<'s'> { enum { value = base32hex_table<'S'>::value }; };
        template<> struct base32hex_table<'t'> { enum { value = base32hex_table<'T'>::value }; };
        template<> struct base32hex_table<'u'> { enum { value = base32hex_table<'U'>::value }; };
        template<> struct base32hex_table<'v'> { enum { value = base32hex_table<'V'>::value }; };
    }

    /**
     * @em %base32 character encoding traits class template.
     *
     * @tparam charT the encoding character type
     */
    template<class charT> struct base32_traits;

    /**
     * Character encoding traits specialization for @c char.
     *
     * This character encoding traits class defines the encoding
     * alphabet for the @em %base32 encoding scheme as defined in RFC
     * 4648 for the encoding character type @c char.
     */
    template<>
    struct base32_traits<char> {
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
         * Returns the character representation of a 5-bit value.
         */
    	static char_type to_char_type(const int_type& c) {
            return to_char_type_upper(c);
        }

        /**
         * Returns the uppercase character representation of a 5-bit
         * value.
         */
    	static char_type to_char_type_upper(const int_type& c) {
            return "ABCDEFGHIJKLMNOPQRSTUVWXYZ234567"[c];
    	}

        /**
         * Returns the lowercase character representation of a 5-bit
         * value.
         */
    	static char_type to_char_type_lower(const int_type& c) {
            return "abcdefghijklmnopqrstuvwxyz234567"[c];
        }

        /**
         * Returns the 5-bit value represented by a character, or
         * inv() for characters not in the encoding alphabet.
         */
    	static int_type to_int_type(const char_type& c) {
            return lookup<detail::base32_table, int_type>(c);
    	}

        /**
         * Returns an integral value represented by no character in
         * the encoding alphabet.
         */
        static int_type inv() {
            return detail::base32_table<'\0'>::value;
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
     * alphabet for the @em %base32 encoding scheme as defined in RFC
     * 4648 for the encoding character type @c wchar_t.
     */
    template<>
    struct base32_traits<wchar_t>
    : public portable_wchar_encoding_traits<base32_traits<char> > {
    };

    /**
     * @em %base32hex character encoding traits class template.
     *
     * @tparam charT the encoding character type
     */
    template<class charT> struct base32hex_traits;

    /**
     * Character encoding traits specialization for @c char.
     *
     * This character encoding traits class defines the encoding
     * alphabet for the @em %base32hex encoding scheme as defined in
     * RFC 4648 for the encoding character type @c char.
     */
    template<>
    struct base32hex_traits<char> {
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
         * Returns the character representation of a 5-bit value.
         */
    	static char_type to_char_type(const int_type& c) {
            return to_char_type_upper(c);
        }

        /**
         * Returns the uppercase character representation of a 5-bit
         * value.
         */
    	static char_type to_char_type_upper(const int_type& c) {
            return "0123456789ABCDEFGHIJKLMNOPQRSTUV"[c];
    	}

        /**
         * Returns the lowercase character representation of a 5-bit
         * value.
         */
    	static char_type to_char_type_lower(const int_type& c) {
            return "0123456789abcdefghijklmnopqrstuv"[c];
        }

        /**
         * Returns the 5-bit value represented by a character, or
         * inv() for characters not in the encoding alphabet.
         */
    	static int_type to_int_type(const char_type& c) {
            return lookup<detail::base32hex_table, int_type>(c);
    	}

        /**
         * Returns an integral value represented by no character in
         * the encoding alphabet.
         */
        static int_type inv() {
            return detail::base32hex_table<'\0'>::value;
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
     * alphabet for the @em %base32hex encoding scheme as defined in
     * RFC 4648 for the encoding character type @c wchar_t.
     */
    template<>
    struct base32hex_traits<wchar_t>
    : portable_wchar_encoding_traits<base32hex_traits<char> > {
    };

    /**
     * This class template implements the Base32 encoding as defined
     * in RFC 4648 for a given character type and encoding alphabet.
     *
     * The Base32 encoding is designed to represent arbitrary
     * sequences of octets in a form that needs to be case insensitive
     * but that need not be human readable.
     *
     * The encoding process represents 40-bit groups of input data as
     * output strings of 8 encoded characters.  Proceeding from left
     * to right, a 40-bit input group is formed by concatenating 5
     * 8-bit input groups.  These 40 bits are then treated as 8
     * concatenated 5-bit groups, each of which is translated into a
     * single character in the Base32 alphabet.
     *
     * @tparam charT the encoding character type
     *
     * @tparam traits the character encoding traits type
     */
    template<class charT, class traits = base32_traits<charT> >
    class base32 {
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
                *result = traits::to_char_type((c0 & 0xff) >> 3);
                ++result;

                if (++first == last) {
                    *result++ = traits::to_char_type((c0 & 0x07) << 2);
                    return pad ? pad_n(result, 6) : result;
                }

                int_type c1 = *first;
                *result = traits::to_char_type((c0 & 0x07) << 2 | (c1 & 0xff) >> 6);
                ++result;
                *result = traits::to_char_type((c1 & 0x3f) >> 1);
                ++result;

                if (++first == last) {
                    *result++ = traits::to_char_type((c1 & 0x01) << 4);
                    return pad ? pad_n(result, 4) : result;
                }

                int_type c2 = *first;
                *result = traits::to_char_type((c1 & 0x01) << 4 | (c2 & 0xff) >> 4);
                ++result;

                if (++first == last) {
                    *result++ = traits::to_char_type((c2 & 0x0f) << 1);
                    return pad ? pad_n(result, 3) : result;
                }

                int_type c3 = *first;
                *result = traits::to_char_type((c2 & 0x0f) << 1 | (c3 & 0xff) >> 7);
                ++result;
                *result = traits::to_char_type((c3 & 0x7f) >> 2);
                ++result;

                if (++first == last) {
                    *result++ = traits::to_char_type((c3 & 0x03) << 3);
                    return pad ? pad_n(result, 1) : result;
                }

                int_type c4 = *first;
                *result = traits::to_char_type((c3 & 0x03) << 3 | (c4 & 0xff) >> 5);
                ++result;
                *result = traits::to_char_type((c4 & 0x1f));
                ++result;
            }

            return result;
        }

        /**
         * Encodes a range of octets using the lowercase encoding
         * alphabet.
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
        static OutputIterator encode_lower(
            InputIterator first, InputIterator last, OutputIterator result,
            bool pad = true
            )
        {
            typedef lower_char_encoding_traits<traits> lower_traits;
            return base32<charT, lower_traits>::encode(first, last, result, pad);
        }

        /**
         * Encodes a range of octets using the uppercase encoding
         * alphabet.
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
        static OutputIterator encode_upper(
            InputIterator first, InputIterator last, OutputIterator result,
            bool pad = true
            )
        {
            typedef upper_char_encoding_traits<traits> upper_traits;
            return base32<charT, upper_traits>::encode(first, last, result, pad);
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
            Predicate skip)
        {
            for (;;) {
                int_type c0 = seek(first, last, skip);
                if (traits::eq_int_type(c0, traits::inv())) {
                    return result;
                }

                int_type c1 = seek(first, last, skip);
                if (traits::eq_int_type(c1, traits::inv())) {
                    throw invalid_length("base32 decode error");
                }

                *result = c0 << 3 | c1 >> 2;
                ++result;

                int_type c2 = seek(first, last, skip);
                if (traits::eq_int_type(c2, traits::inv())) {
                    return result;
                }

                int_type c3 = seek(first, last, skip);
                if (traits::eq_int_type(c3, traits::inv())) {
                    throw invalid_length("base32 decode error");
                }

                *result = (c1 & 0x03) << 6 | c2 << 1 | c3 >> 4;
                ++result;

                int_type c4 = seek(first, last, skip);
                if (traits::eq_int_type(c4, traits::inv())) {
                    return result;
                }

                *result = (c3 & 0x0f) << 4 | c4 >> 1;
                ++result;

                int_type c5 = seek(first, last, skip);
                if (traits::eq_int_type(c5, traits::inv())) {
                    return result;
                }

                int_type c6 = seek(first, last, skip);
                if (traits::eq_int_type(c6, traits::inv())) {
                    throw invalid_length("base32 decode error");
                }

                *result = (c4 & 0x01) << 7 | c5 << 2 | c6 >> 3;
                ++result;

                int_type c7 = seek(first, last, skip);
                if (traits::eq_int_type(c7, traits::inv())) {
                    return result;
                }

                *result = (c6 & 0x07) << 5 | c7;
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
            return (n + 4) / 5 * 8;
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
            return (n + 7) / 8 * 5;
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
                    throw invalid_character("base32 decode error");
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
                    throw invalid_character("base32 decode error");
                }
            }

            return traits::inv();
        }
    };
}

#endif
