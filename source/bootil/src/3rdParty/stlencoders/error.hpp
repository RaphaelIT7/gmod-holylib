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

#ifndef STLENCODERS_ERROR_HPP
#define STLENCODERS_ERROR_HPP

#include <stdexcept>

/**
 * @file
 *
 * Exception classes used to report decoding errors.
 */
namespace stlencoders {
    /**
     * Exception class thrown to report an unspecified error in a
     * decode operation.
     */
    class decode_error : public std::runtime_error {
    public:
        /**
         * Constructs an object of class @c decode_error.
         */
        explicit decode_error(const std::string& s)
        : runtime_error(s) { }
    };

    /**
     * Exception class thrown to report an invalid character.
     */
    class invalid_character : public decode_error {
    public:
        /**
         * Constructs an object of class @c invalid_character.
         */
        explicit invalid_character(const std::string& s)
        : decode_error(s) { }
    };

    /**
     * Exception class thrown to report an invalid length of a
     * character sequence.
     */
    class invalid_length : public decode_error {
    public:
        /**
         * Constructs an object of class @c invalid_length.
         */
        explicit invalid_length(const std::string& s)
        : decode_error(s) { }
    };
}

#endif
