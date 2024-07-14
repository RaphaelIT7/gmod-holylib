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

#ifndef STLENCODERS_ITERATOR_HPP
#define STLENCODERS_ITERATOR_HPP

#include <algorithm>
#include <iterator>
#include <string>

/**
 * @file
 *
 * Common iterator adaptors.
 */
namespace stlencoders {
    /**
     * An output iterator adaptor used to wrap lines after a set
     * number of characters.
     *
     * @tparam Iterator the underlying iterator class
     *
     * @tparam charT the output character type
     *
     * @tparam traits the output character traits type
    */
    template<class Iterator, class charT, class traits = std::char_traits<charT> >
    class line_wrap_iterator
    : public std::iterator<std::output_iterator_tag, void, void, void, void>
    {
    public:
        /**
         * The underlying iterator type.
         */
        typedef Iterator iterator_type;

        /**
         * The character type.
         */
        typedef charT char_type;

        /**
         * The character traits type.
         */
        typedef traits traits_type;

        /**
         * An integral type holding the number of characters per line.
         */
        typedef typename traits::off_type off_type;

        /**
         * Constructs a line_wrap_iterator adaptor which will copy a
         * delimiter string to the underlying iterator after every @a
         * n characters.
         *
         * @param i the underlying iterator
         *
         * @param n the number of characters per line
         *
         * @param s the line delimiter string
         */
        line_wrap_iterator(Iterator i, off_type n, const charT* s) :
            current(i), pos(0), len(n), endl(s)
        {
        }

        /**
         * Returns @c current.
         */
        Iterator base() const {
            return current;
        }

        /**
         * Copies a character to the underlying output iterator.
         *
         * If the maximum number of characters per line has been
         * reached, copies the delimiter string to the underlying
         * output iterator.  Then outputs the given character and
         * increments the underlying iterator.
         *
         * @param c the character to be written
         * @return @c *this
         */
        line_wrap_iterator& operator=(char_type c) {
            if (len > 0) {
                if (pos == len) {
                    current = std::copy(endl, endl + traits::length(endl), current);
                    pos = 0;
                }
                ++pos;
            }

            *current = c;
            ++current;
            return *this;
        }

        /**
         * Returns @c *this.
         */
        line_wrap_iterator& operator*() {
            return *this;
        }

        /**
         * Returns @c *this.
         */
        line_wrap_iterator& operator++() {
            return *this;
        }

        /**
         * Returns @c *this.
         */
        line_wrap_iterator& operator++(int) {
            return *this;
        }

    protected:
        /**
         * The underlying iterator.
         */
        Iterator current;

    private:
        off_type pos;
        const off_type len;
        const charT* endl;
    };

    /**
     * Creates a line_wrap_iterator adaptor which will copy a
     * delimiter string to a given iterator after every @a n
     * characters.
     *
     * @tparam Iterator the underlying iterator class
     *
     * @tparam charT the output character type
     *
     * @tparam sizeT an integral type
     *
     * @param i the underlying iterator
     *
     * @param n the number of characters per line
     *
     * @param s the line delimiter string
     *
     * @return a @c line_wrap_iterator
     */
    template<class Iterator, class charT, class sizeT>
    inline line_wrap_iterator<Iterator, charT>
    line_wrapper(Iterator i, sizeT n, const charT* s)
    {
        return line_wrap_iterator<Iterator, charT>(i, n, s);
    }
}

#endif
