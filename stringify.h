/****************************************************************************
 Copyright (c) 2016-2027 Jeff Wang <summer_insects@163.com>

 Permission is hereby granted, free of charge, to any person obtaining a copy
 of this software and associated documentation files (the "Software"), to deal
 in the Software without restriction, including without limitation the rights
 to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 copies of the Software, and to permit persons to whom the Software is
 furnished to do so, subject to the following conditions:

 The above copyright notice and this permission notice shall be included in all
 copies or substantial portions of the Software.

 THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 SOFTWARE.
 ****************************************************************************/

#ifndef __MAHJONG_ALGORITHM__STRINGIFY_H__
#define __MAHJONG_ALGORITHM__STRINGIFY_H__

#include "tile.h"

namespace mahjong {

using std::size_t;

/**
 * @brief String format:
 * - Numbered tiles: Characters = m, Bamboo = s, Dots = p. Suffixes use lowercase letters; consecutive tiles of the same suit can share a single suffix, e.g., 123m, 678s, etc.
 * - Honor tiles: East, South, West, North = ESWN; Red, Green, White Dragons = CFP. Uses uppercase letters.
 * - Chows, pungs, and kongs are enclosed in square brackets [], optionally using a digit to indicate the tile source:
 *    - Chow: Indicates which tile in the sequence was discarded by the upper seat; e.g., [567m2] means 57m chowed the 6m (the 2nd tile). If omitted, defaults to the 1st tile.
 *    - Pung: Indicates who discarded the tile: 1 for upper seat, 2 for opposite seat, 3 for lower seat; e.g., [999s3] means pung 9s from lower seat. If omitted, defaults to upper seat.
 *    - Kong: Similar to pung, but if omitted, it is treated as a concealed kong. E.g.: [SSSS] means concealed kong of South; [8888p1] means melded kong of 8p from upper seat. When the digit is 5, 6, or 7, it represents an added kong (promoted kong). E.g.: [1111s6] means after punging 1s from opposite seat, drew another 1s to promote to kong.
 * - Examples
 *    - [EEEE][CCCC][FFFF][PPPP]NN
 *    - 1112345678999s9s
 *    - [WWWW1][444s]45m678pFF6m
 *    - [EEEE]288s349pSCFF2p
 *    - [123p1][345s2][999s3]6m6pEW1m
 *    - 356m18s1579pWNFF9p
 */

/**
 * @addtogroup stringify
 * @{
 */

template <class Dummy = int>
struct tile_name {
    static const char *text[34];
    static const char suffix[3];
    static const char honor[7];
};

template <class Dummy>
const char *tile_name<Dummy>::text[] = {
    "1m", "2m", "3m", "4m", "5m", "6m", "7m", "8m", "9m",
    "1s", "2s", "3s", "4s", "5s", "6s", "7s", "8s", "9s",
    "1p", "2p", "3p", "4p", "5p", "6p", "7p", "8p", "9p",
    "E" , "S" , "W" , "N" , "C" , "F" , "P"
};

template <class Dummy>
const char tile_name<Dummy>::suffix[] = { 'm', 's', 'p' };

template <class Dummy>
const char tile_name<Dummy>::honor[] = { 'E', 'S', 'W', 'N', 'C', 'F', 'P' };

/**
 * @name error codes
 * @{
 *  Error codes for parsing tiles
 */
#define PARSE_NO_ERROR 0                                ///< No error
#define PARSE_ERROR_ILLEGAL_CHARACTER -1                ///< Illegal character
#define PARSE_ERROR_SUFFIX -2                           ///< Suffix error
#define PARSE_ERROR_WRONG_TILES_COUNT_FOR_FIXED_PACK -3 ///< Melded pack contains wrong number of tiles
#define PARSE_ERROR_CANNOT_MAKE_FIXED_PACK -4           ///< Cannot correctly parse melded pack
#define PARSE_ERROR_TOO_MANY_FIXED_PACKS -5             ///< Too many melded packs (a legal hand has at most 4 melds)
#define PARSE_ERROR_TOO_MANY_TILES -6                   ///< Too many tiles
#define PARSE_ERROR_TILE_COUNT_GREATER_THAN_4 -7        ///< A tile appears more than 4 times

 /**
  * @}
  */

/**
 * @brief Parse string into hand tiles structure and serving tile
 * @param [in] str String
 * @param [in] len String length
 * @param [out] hand_tiles Hand tiles structure
 * @param [out] serving_tile Serving/drawn tile
 * @retval PARSE_NO_ERROR No error
 * @retval PARSE_ERROR_ILLEGAL_CHARACTER Illegal character
 * @retval PARSE_ERROR_NO_SUFFIX_AFTER_DIGIT Missing suffix after digits
 * @retval PARSE_ERROR_WRONG_TILES_COUNT_FOR_FIXED_PACK Melded pack contains wrong number of tiles
 * @retval PARSE_ERROR_CANNOT_MAKE_FIXED_PACK Cannot correctly parse melded pack
 * @retval PARSE_ERROR_TOO_MANY_FIXED_PACKS Too many melded packs (a legal hand has at most 4 melds)
 * @retval PARSE_ERROR_TOO_MANY_TILES Too many tiles
 * @retval PARSE_ERROR_TILE_COUNT_GREATER_THAN_4 A tile appears more than 4 times
 */
int parse_hand_tiles(const char *str, size_t len, hand_tiles_t *hand_tiles, tile_t *serving_tile);

/**
 * @brief Convert hand tiles structure to string
 * @param [in] hand_tiles Hand tiles structure
 * @param [out] str Output string
 * @param [in] max_size Maximum string buffer size (Note: result including \0 is at most 35 characters)
 * @return size_t String length
 */
size_t hand_tiles_to_string(const hand_tiles_t *hand_tiles, char *str, size_t max_size);

/**
 * end group
 * @}
 */

}

#endif
