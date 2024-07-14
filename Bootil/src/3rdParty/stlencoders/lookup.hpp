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

#ifndef STLENCODERS_LOOKUP_HPP
#define STLENCODERS_LOOKUP_HPP

#include <climits>

/**
 * @file
 *
 * Character lookup table implementation.
 */
namespace stlencoders {
    /**
     * Maps a character to its corresponding value in a lookup table.
     * The lookup table is statically initialized from the class
     * template @a LUT, which is parameterized by a non-type argument
     * of type @c char, and shall provide a constant expression @c
     * LUT<c>::value implicitly convertible to type @a T for each
     * character @c c.
     *
     * To create a lookup table that maps the characters @c '0' and @c
     * '1' to their corresponding integral values, and any other
     * character to -1, a class template may be defined as:
     *
     * @code
     * template<char C> struct base2_table {
     *     enum { value = -1 };
     * };
     * template<> struct base2_table<'0'> {
     *     enum { value = 0 };
     * };
     * template<> struct base2_table<'1'> {
     *     enum { value = 1 };
     * };
     *
     * lookup<base2_table, int>('0'); // returns 0
     * lookup<base2_table, int>('1'); // returns 1
     * lookup<base2_table, int>('2'); // returns -1
     * @endcode
     *
     * @tparam LUT the class template defining the lookup table
     *
     * @tparam T the lookup table's value type
     *
     * @param c the character to map
     *
     * @return @c LUT<c>::value
     *
     * @note This implementation limits the size of the lookup table
     * to 256 entries, even on platforms where @c char is more than
     * eight bits wide.  For characters whose unsigned representation
     * is outside this range, @c LUT<'\\0'>\::value is returned.
     *
     */
    template<template<char> class LUT, class T>
    inline const T& lookup(char c)
    {
        static const T table[] = {
            LUT<'\000'>::value, LUT<'\001'>::value, LUT<'\002'>::value, LUT<'\003'>::value,
            LUT<'\004'>::value, LUT<'\005'>::value, LUT<'\006'>::value, LUT<'\007'>::value,
            LUT<'\010'>::value, LUT<'\011'>::value, LUT<'\012'>::value, LUT<'\013'>::value,
            LUT<'\014'>::value, LUT<'\015'>::value, LUT<'\016'>::value, LUT<'\017'>::value,
            LUT<'\020'>::value, LUT<'\021'>::value, LUT<'\022'>::value, LUT<'\023'>::value,
            LUT<'\024'>::value, LUT<'\025'>::value, LUT<'\026'>::value, LUT<'\027'>::value,
            LUT<'\030'>::value, LUT<'\031'>::value, LUT<'\032'>::value, LUT<'\033'>::value,
            LUT<'\034'>::value, LUT<'\035'>::value, LUT<'\036'>::value, LUT<'\037'>::value,
            LUT<'\040'>::value, LUT<'\041'>::value, LUT<'\042'>::value, LUT<'\043'>::value,
            LUT<'\044'>::value, LUT<'\045'>::value, LUT<'\046'>::value, LUT<'\047'>::value,
            LUT<'\050'>::value, LUT<'\051'>::value, LUT<'\052'>::value, LUT<'\053'>::value,
            LUT<'\054'>::value, LUT<'\055'>::value, LUT<'\056'>::value, LUT<'\057'>::value,
            LUT<'\060'>::value, LUT<'\061'>::value, LUT<'\062'>::value, LUT<'\063'>::value,
            LUT<'\064'>::value, LUT<'\065'>::value, LUT<'\066'>::value, LUT<'\067'>::value,
            LUT<'\070'>::value, LUT<'\071'>::value, LUT<'\072'>::value, LUT<'\073'>::value,
            LUT<'\074'>::value, LUT<'\075'>::value, LUT<'\076'>::value, LUT<'\077'>::value,
            LUT<'\100'>::value, LUT<'\101'>::value, LUT<'\102'>::value, LUT<'\103'>::value,
            LUT<'\104'>::value, LUT<'\105'>::value, LUT<'\106'>::value, LUT<'\107'>::value,
            LUT<'\110'>::value, LUT<'\111'>::value, LUT<'\112'>::value, LUT<'\113'>::value,
            LUT<'\114'>::value, LUT<'\115'>::value, LUT<'\116'>::value, LUT<'\117'>::value,
            LUT<'\120'>::value, LUT<'\121'>::value, LUT<'\122'>::value, LUT<'\123'>::value,
            LUT<'\124'>::value, LUT<'\125'>::value, LUT<'\126'>::value, LUT<'\127'>::value,
            LUT<'\130'>::value, LUT<'\131'>::value, LUT<'\132'>::value, LUT<'\133'>::value,
            LUT<'\134'>::value, LUT<'\135'>::value, LUT<'\136'>::value, LUT<'\137'>::value,
            LUT<'\140'>::value, LUT<'\141'>::value, LUT<'\142'>::value, LUT<'\143'>::value,
            LUT<'\144'>::value, LUT<'\145'>::value, LUT<'\146'>::value, LUT<'\147'>::value,
            LUT<'\150'>::value, LUT<'\151'>::value, LUT<'\152'>::value, LUT<'\153'>::value,
            LUT<'\154'>::value, LUT<'\155'>::value, LUT<'\156'>::value, LUT<'\157'>::value,
            LUT<'\160'>::value, LUT<'\161'>::value, LUT<'\162'>::value, LUT<'\163'>::value,
            LUT<'\164'>::value, LUT<'\165'>::value, LUT<'\166'>::value, LUT<'\167'>::value,
            LUT<'\170'>::value, LUT<'\171'>::value, LUT<'\172'>::value, LUT<'\173'>::value,
            LUT<'\174'>::value, LUT<'\175'>::value, LUT<'\176'>::value, LUT<'\177'>::value,
            LUT<'\200'>::value, LUT<'\201'>::value, LUT<'\202'>::value, LUT<'\203'>::value,
            LUT<'\204'>::value, LUT<'\205'>::value, LUT<'\206'>::value, LUT<'\207'>::value,
            LUT<'\210'>::value, LUT<'\211'>::value, LUT<'\212'>::value, LUT<'\213'>::value,
            LUT<'\214'>::value, LUT<'\215'>::value, LUT<'\216'>::value, LUT<'\217'>::value,
            LUT<'\220'>::value, LUT<'\221'>::value, LUT<'\222'>::value, LUT<'\223'>::value,
            LUT<'\224'>::value, LUT<'\225'>::value, LUT<'\226'>::value, LUT<'\227'>::value,
            LUT<'\230'>::value, LUT<'\231'>::value, LUT<'\232'>::value, LUT<'\233'>::value,
            LUT<'\234'>::value, LUT<'\235'>::value, LUT<'\236'>::value, LUT<'\237'>::value,
            LUT<'\240'>::value, LUT<'\241'>::value, LUT<'\242'>::value, LUT<'\243'>::value,
            LUT<'\244'>::value, LUT<'\245'>::value, LUT<'\246'>::value, LUT<'\247'>::value,
            LUT<'\250'>::value, LUT<'\251'>::value, LUT<'\252'>::value, LUT<'\253'>::value,
            LUT<'\254'>::value, LUT<'\255'>::value, LUT<'\256'>::value, LUT<'\257'>::value,
            LUT<'\260'>::value, LUT<'\261'>::value, LUT<'\262'>::value, LUT<'\263'>::value,
            LUT<'\264'>::value, LUT<'\265'>::value, LUT<'\266'>::value, LUT<'\267'>::value,
            LUT<'\270'>::value, LUT<'\271'>::value, LUT<'\272'>::value, LUT<'\273'>::value,
            LUT<'\274'>::value, LUT<'\275'>::value, LUT<'\276'>::value, LUT<'\277'>::value,
            LUT<'\300'>::value, LUT<'\301'>::value, LUT<'\302'>::value, LUT<'\303'>::value,
            LUT<'\304'>::value, LUT<'\305'>::value, LUT<'\306'>::value, LUT<'\307'>::value,
            LUT<'\310'>::value, LUT<'\311'>::value, LUT<'\312'>::value, LUT<'\313'>::value,
            LUT<'\314'>::value, LUT<'\315'>::value, LUT<'\316'>::value, LUT<'\317'>::value,
            LUT<'\320'>::value, LUT<'\321'>::value, LUT<'\322'>::value, LUT<'\323'>::value,
            LUT<'\324'>::value, LUT<'\325'>::value, LUT<'\326'>::value, LUT<'\327'>::value,
            LUT<'\330'>::value, LUT<'\331'>::value, LUT<'\332'>::value, LUT<'\333'>::value,
            LUT<'\334'>::value, LUT<'\335'>::value, LUT<'\336'>::value, LUT<'\337'>::value,
            LUT<'\340'>::value, LUT<'\341'>::value, LUT<'\342'>::value, LUT<'\343'>::value,
            LUT<'\344'>::value, LUT<'\345'>::value, LUT<'\346'>::value, LUT<'\347'>::value,
            LUT<'\350'>::value, LUT<'\351'>::value, LUT<'\352'>::value, LUT<'\353'>::value,
            LUT<'\354'>::value, LUT<'\355'>::value, LUT<'\356'>::value, LUT<'\357'>::value,
            LUT<'\360'>::value, LUT<'\361'>::value, LUT<'\362'>::value, LUT<'\363'>::value,
            LUT<'\364'>::value, LUT<'\365'>::value, LUT<'\366'>::value, LUT<'\367'>::value,
            LUT<'\370'>::value, LUT<'\371'>::value, LUT<'\372'>::value, LUT<'\373'>::value,
            LUT<'\374'>::value, LUT<'\375'>::value, LUT<'\376'>::value, LUT<'\377'>::value
        };

#if UCHAR_MAX > 255
        unsigned char n = static_cast<unsigned char>(c);
        return n < (sizeof table / sizeof *table) ? table[n] : *table;
#else
        return table[static_cast<unsigned char>(c)];
#endif
    }
}

#endif
