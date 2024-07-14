/*
 * Copyright (c) 2012 Thomas Kemmer <tkemmer@computer.org>
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

#ifndef STLENCODERS_BASE2_HPP
#define STLENCODERS_BASE2_HPP

#include "error.hpp"
#include "lookup.hpp"
#include "traits.hpp"

/**
 * @file
 *
 * Implementation of the Base2 encoding scheme.
 */
namespace stlencoders {
    namespace detail {
        template<char C> struct base2_table { enum { value = 2 }; };

        template<> struct base2_table<'0'> { enum { value = 0 }; };
        template<> struct base2_table<'1'> { enum { value = 1 }; };
    }

    /**
     * @em %base2 character encoding traits class template.
     *
     * @tparam charT the encoding character type
     */
    template<class charT> struct base2_traits;

    /**
     * Character encoding traits specialization for @c char.
     *
     * This character encoding traits class defines the encoding
     * alphabet for the @em %base2 encoding scheme for the encoding
     * character type @c char.
     */
    template<>
    struct base2_traits<char> {
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
         * Returns the lowercase character representation of a single
         * bit value.
         */
    	static char_type to_char_type(const int_type& c) {
            return "01"[c];
        }

        /**
         * Returns the single bit value represented by a character, or
         * inv() for characters not in the encoding alphabet.
         */
    	static int_type to_int_type(const char_type& c) {
            return lookup<detail::base2_table, int_type>(c);
    	}

        /**
         * Returns an integral value represented by no character in
         * the encoding alphabet.
         */
        static int_type inv() {
            return detail::base2_table<'\0'>::value;
        }
    };

    /**
     * Character encoding traits specialization for @c wchar_t.
     *
     * This character encoding traits class defines the encoding
     * alphabet for the @em %base2 encoding scheme for the encoding
     * character type @c wchar_t.
     */
    template<>
    struct base2_traits<wchar_t>
    : public portable_wchar_encoding_traits<base2_traits<char> > {
    };

    /**
     * This class template implements the standard Base2, or binary,
     * encoding.
     *
     * The encoding process represents 8-bit groups (octets) of input
     * data as output strings of 8 encoded characters.  Proceeding
     * from left to right, an 8-bit input is taken from the input
     * data.  These 8 bits are then translated individually into a
     * single character in the Base2 alphabet.
     *
     * @tparam charT the encoding character type
     *
     * @tparam traits the character encoding traits type
     */
    template<class charT, class traits = base2_traits<charT> >
    class base2 {
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
                *result = traits::to_char_type(c >> 7 & 1);
                ++result;
                *result = traits::to_char_type(c >> 6 & 1);
                ++result;
                *result = traits::to_char_type(c >> 5 & 1);
                ++result;
                *result = traits::to_char_type(c >> 4 & 1);
                ++result;
                *result = traits::to_char_type(c >> 3 & 1);
                ++result;
                *result = traits::to_char_type(c >> 2 & 1);
                ++result;
                *result = traits::to_char_type(c >> 1 & 1);
                ++result;
                *result = traits::to_char_type(c & 1);
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
                    throw invalid_length("base2 decode error");
                }

                int_type c2 = seek(first, last, skip);
                if (traits::eq_int_type(c2, traits::inv())) {
                    throw invalid_length("base2 decode error");
                }

                int_type c3 = seek(first, last, skip);
                if (traits::eq_int_type(c3, traits::inv())) {
                    throw invalid_length("base2 decode error");
                }

                int_type c4 = seek(first, last, skip);
                if (traits::eq_int_type(c4, traits::inv())) {
                    throw invalid_length("base2 decode error");
                }

                int_type c5 = seek(first, last, skip);
                if (traits::eq_int_type(c5, traits::inv())) {
                    throw invalid_length("base2 decode error");
                }

                int_type c6 = seek(first, last, skip);
                if (traits::eq_int_type(c6, traits::inv())) {
                    throw invalid_length("base2 decode error");
                }

                int_type c7 = seek(first, last, skip);
                if (traits::eq_int_type(c7, traits::inv())) {
                    throw invalid_length("base2 decode error");
                }

                *result = (c0 << 7 | c1 << 6 | c2 << 5 | c3 << 4 |
                           c4 << 3 | c5 << 2 | c6 << 1 | c7);
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
            return n * 8;
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
            return n / 8;
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
                    throw invalid_character("base2 decode error");
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
                    throw invalid_character("base2 decode error");
                }
            }

            return traits::inv();
        }
    };
}

#endif
