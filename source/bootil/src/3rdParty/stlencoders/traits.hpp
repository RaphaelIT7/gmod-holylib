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

#ifndef STLENCODERS_TRAITS_HPP
#define STLENCODERS_TRAITS_HPP

/**
 * @file
 *
 * Components for implementing, or working with, character encoding
 * traits.
 */
namespace stlencoders {
    /**
     * A character encoding traits adaptor that converts an underlying
     * encoding traits class for type @c char to type @c wchar_t.
     *
     * A portable encoding traits class' encoding alphabet must only
     * use characters from the basic execution character set:
     *
     * @code
     * a b c d e f g h i j k l m n o p q r s t u v w x y z
     * A B C D E F G H I J K L M N O P Q R S T U V W X Y Z
     * 0 1 2 3 4 5 6 7 8 9
     * _ { } [ ] # ( ) < > % : ; . ? * + - / ^ & | ∼ ! = , \ " '
     * @endcode
     *
     * @tparam traits the underlying character encoding traits class
     * with character type @c char
     */
    template<class traits>
    struct portable_wchar_encoding_traits {
        /**
         * The encoding character type.
         */
        typedef wchar_t char_type;

        /**
         * An integral type representing an octet.
         */
        typedef typename traits::int_type int_type;

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
            return traits::eq_int_type(lhs, rhs);
        }

        /**
         * Converts an integral value to a character in the encoding
         * alphabet.
         *
         * @param c an integral value
         *
         * @return the character representing @a c
         */
        static char_type to_char_type(const int_type& c) {
            return static_cast<wchar_t>(traits::to_char_type(c));
        }

        /**
         * Converts an integral value to a lowercase character in the
         * encoding alphabet.
         *
         * @param c an integral value
         *
         * @return the lowercase character representing @a c
         */
        static char_type to_char_type_lower(const int_type& c) {
            return static_cast<wchar_t>(traits::to_char_type_lower(c));
        }

        /**
         * Converts an integral value to an uppercase character in the
         * encoding alphabet.
         *
         * @param c an integral value
         *
         * @return the uppercase character representing @a c
         */
        static char_type to_char_type_upper(const int_type& c) {
            return static_cast<wchar_t>(traits::to_char_type_upper(c));
        }

        /**
         * Converts a character to an integral value.
         *
         * @param c a character
         *
         * @return the integral value represented by @a c, or inv()
         * for characters not in the encoding alphabet
         */
        static int_type to_int_type(const char_type& c) {
            if (c == static_cast<wchar_t>(static_cast<char>(c))) {
                return traits::to_int_type(static_cast<char>(c));
            } else {
                return traits::inv();
            }
        }

        /**
         * Returns the character used to perform padding at the end of
         * a character sequence.
         */
        static char_type pad() {
            return static_cast<wchar_t>(traits::pad());
        }

        /**
         * Returns an integral value represented by no character in
         * the encoding alphabet.
         */
        static int_type inv() {
            return traits::inv();
        }
    };

    /**
     * A character encoding traits adaptor that uses the lowercase
     * alphabet of an underlying encoding traits class.
     *
     * @tparam traits the character encoding traits class from which
     * this class is derived
     */
    template<class traits>
    struct lower_char_encoding_traits : public traits {
        /**
         * The encoding character type.
         */
        typedef typename traits::char_type char_type;

        /**
         * An integral type representing an octet.
         */
        typedef typename traits::int_type int_type;

        /**
         * Converts an integral value to a lowercase character in the
         * encoding alphabet.
         *
         * @param c an integral value
         *
         * @return the lowercase character representing @a c
         */
    	static char_type to_char_type(const int_type& c) {
            return traits::to_char_type_lower(c);
        }
    };

    /**
     * A character encoding traits adaptor that uses the uppercase
     * alphabet of an underlying encoding traits class.
     *
     * @tparam traits the character encoding traits class from which
     * this class is derived
     */
    template<class traits>
    struct upper_char_encoding_traits : public traits {
        /**
         * The encoding character type.
         */
        typedef typename traits::char_type char_type;

        /**
         * An integral type representing an octet.
         */
        typedef typename traits::int_type int_type;

        /**
         * Converts an integral value to an uppercase character in the
         * encoding alphabet.
         *
         * @param c an integral value
         *
         * @return the uppercase character representing @a c
         */
    	static char_type to_char_type(const int_type& c) {
            return traits::to_char_type_upper(c);
        }
    };
}

#endif
