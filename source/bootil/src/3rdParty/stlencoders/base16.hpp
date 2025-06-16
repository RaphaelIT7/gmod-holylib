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

#ifndef STLENCODERS_BASE16_HPP
#define STLENCODERS_BASE16_HPP

#include "error.hpp"
#include "lookup.hpp"
#include "traits.hpp"

/**
 * @file
 *
 * Implementation of the Base16 encoding scheme.
 */
namespace stlencoders {
    namespace detail {
        template<char C> struct base16_table { enum { value = 0x10 }; };

        template<> struct base16_table<'0'> { enum { value = 0x00 }; };
        template<> struct base16_table<'1'> { enum { value = 0x01 }; };
        template<> struct base16_table<'2'> { enum { value = 0x02 }; };
        template<> struct base16_table<'3'> { enum { value = 0x03 }; };
        template<> struct base16_table<'4'> { enum { value = 0x04 }; };
        template<> struct base16_table<'5'> { enum { value = 0x05 }; };
        template<> struct base16_table<'6'> { enum { value = 0x06 }; };
        template<> struct base16_table<'7'> { enum { value = 0x07 }; };
        template<> struct base16_table<'8'> { enum { value = 0x08 }; };
        template<> struct base16_table<'9'> { enum { value = 0x09 }; };
        template<> struct base16_table<'A'> { enum { value = 0x0a }; };
        template<> struct base16_table<'B'> { enum { value = 0x0b }; };
        template<> struct base16_table<'C'> { enum { value = 0x0c }; };
        template<> struct base16_table<'D'> { enum { value = 0x0d }; };
        template<> struct base16_table<'E'> { enum { value = 0x0e }; };
        template<> struct base16_table<'F'> { enum { value = 0x0f }; };

        template<> struct base16_table<'a'> { enum { value = base16_table<'A'>::value }; };
        template<> struct base16_table<'b'> { enum { value = base16_table<'B'>::value }; };
        template<> struct base16_table<'c'> { enum { value = base16_table<'C'>::value }; };
        template<> struct base16_table<'d'> { enum { value = base16_table<'D'>::value }; };
        template<> struct base16_table<'e'> { enum { value = base16_table<'E'>::value }; };
        template<> struct base16_table<'f'> { enum { value = base16_table<'F'>::value }; };
    }

    /**
     * @em %base16 character encoding traits class template.
     *
     * @tparam charT the encoding character type
     */
    template<class charT> struct base16_traits;

    /**
     * Character encoding traits specialization for @c char.
     *
     * This character encoding traits class defines the encoding
     * alphabet for the @em %base16 encoding scheme as defined in RFC
     * 4648 for the encoding character type @c char.
     */
    template<>
    struct base16_traits<char> {
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
         * Returns the character representation of a 4-bit value.
         */
    	static char_type to_char_type(const int_type& c) {
            return to_char_type_upper(c);
        }

        /**
         * Returns the uppercase character representation of a 4-bit
         * value.
         */
    	static char_type to_char_type_upper(const int_type& c) {
            return "0123456789ABCDEF"[c];
    	}

        /**
         * Returns the lowercase character representation of a 4-bit
         * value.
         */
    	static char_type to_char_type_lower(const int_type& c) {
            return "0123456789abcdef"[c];
        }

        /**
         * Returns the 4-bit value represented by a character, or
         * inv() for characters not in the encoding alphabet.
         */
    	static int_type to_int_type(const char_type& c) {
            return lookup<detail::base16_table, int_type>(c);
    	}

        /**
         * Returns an integral value represented by no character in
         * the encoding alphabet.
         */
        static int_type inv() {
            return detail::base16_table<'\0'>::value;
        }
    };

    /**
     * Character encoding traits specialization for @c wchar_t.
     *
     * This character encoding traits class defines the encoding
     * alphabet for the @em %base16 encoding scheme as defined in RFC
     * 4648 for the encoding character type @c wchar_t.
     */
    template<>
    struct base16_traits<wchar_t>
    : public portable_wchar_encoding_traits<base16_traits<char> > {
    };

    /**
     * This class template implements the Base16 encoding as defined
     * in RFC 4648 for a given character type and encoding alphabet.
     *
     * Base16 encoding is the standard case insensitive hexadecimal
     * encoding.
     *
     * The encoding process represents 8-bit groups (octets) of input
     * data as output strings of 2 encoded characters.  Proceeding
     * from left to right, an 8-bit input is taken from the input
     * data.  These 8 bits are then treated as 2 concatenated 4-bit
     * groups, each of which is translated into a single character in
     * the Base16 alphabet.
     *
     * @tparam charT the encoding character type
     *
     * @tparam traits the character encoding traits type
     */
    template<class charT, class traits = base16_traits<charT> >
    class base16 {
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
         * @return an output iterator referring to one past the last
         * value assigned to the output range
         */
        template<class InputIterator, class OutputIterator>
        static OutputIterator encode(
            InputIterator first, InputIterator last, OutputIterator result
            )
        {
            for (; first != last; ++first) {
                int_type c = *first;
            	*result = traits::to_char_type((c & 0xff) >> 4);
                ++result;
            	*result = traits::to_char_type((c & 0x0f));
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
         * @return an output iterator referring to one past the last
         * value assigned to the output range
         */
        template<class InputIterator, class OutputIterator>
        static OutputIterator encode_lower(
            InputIterator first, InputIterator last, OutputIterator result
            )
        {
            typedef lower_char_encoding_traits<traits> lower_traits;
            return base16<charT, lower_traits>::encode(first, last, result);
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
         * @return an output iterator referring to one past the last
         * value assigned to the output range
         */
        template<class InputIterator, class OutputIterator>
        static OutputIterator encode_upper(
            InputIterator first, InputIterator last, OutputIterator result
            )
        {
            typedef upper_char_encoding_traits<traits> upper_traits;
            return base16<charT, upper_traits>::encode(first, last, result);
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
         * @throw invalid_length if the input range contains an odd
         * number of encoding characters
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
         *
         * - if @a skip(c) evaluates to @c true, the character is
         *   ignored
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
         * @throw invalid_length if the input range contains an odd
         * number of encoding characters
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
                    throw invalid_length("base16 decode error");
                }

                *result = c0 << 4 | c1;
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
            return n * 2;
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
            return n / 2;
        }

    private:
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
                } else if (!skip(c)) {
                    throw invalid_character("base16 decode error");
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
                } else {
                    throw invalid_character("base16 decode error");
                }
            }

            return traits::inv();
        }
    };
}

#endif
