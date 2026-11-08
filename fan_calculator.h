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

#ifndef __MAHJONG_ALGORITHM__FAN_CALCULATOR_H__
#define __MAHJONG_ALGORITHM__FAN_CALCULATOR_H__

#include "tile.h"

#define SUPPORT_CONCEALED_KONG_AND_MELDED_KONG      1  // Support Concealed Kong and Melded Kong
#define KNITTED_STRAIGHT_BODY_WITH_ECS              1  // Edge/closed/single wait on body of Knitted Straight
#define DISTINGUISH_PURE_SHIFTED_CHOWS              0  // Distinguish step interval of Pure Shifted Chows
#define NINE_GATES_WHEN_BLESSING_OF_HEAVEN          1  // Support Nine Gates when Blessing of Heaven
#define SUPPORT_BLESSINGS                           0  // Support Blessing of Heaven, Earth, and Human

namespace mahjong {

/**
 * @addtogroup calculator
 * @{
 */

/**
 * @brief Fan types
 */
enum fan_t {
    FAN_NONE = 0,                       ///< Invalid
    BIG_FOUR_WINDS,                     ///< Big Four Winds
    BIG_THREE_DRAGONS,                  ///< Big Three Dragons
    ALL_GREEN,                          ///< All Green
    NINE_GATES,                         ///< Nine Gates
    FOUR_KONGS,                         ///< Four Kongs
    SEVEN_SHIFTED_PAIRS,                ///< Seven Shifted Pairs
    THIRTEEN_ORPHANS,                   ///< Thirteen Orphans

    ALL_TERMINALS,                      ///< All Terminals
    LITTLE_FOUR_WINDS,                  ///< Little Four Winds
    LITTLE_THREE_DRAGONS,               ///< Little Three Dragons
    ALL_HONORS,                         ///< All Honors
    FOUR_CONCEALED_PUNGS,               ///< Four Concealed Pungs
    PURE_TERMINAL_CHOWS,                ///< Pure Terminal Chows

    QUADRUPLE_CHOW,                     ///< Quadruple Chow
    FOUR_PURE_SHIFTED_PUNGS,            ///< Four Pure Shifted Pungs

#if DISTINGUISH_PURE_SHIFTED_CHOWS
    FOUR_PURE_SHIFTED_CHOWS_1,          ///< Four Pure Shifted Chows I
    FOUR_PURE_SHIFTED_CHOWS_2,          ///< Four Pure Shifted Chows II
#else
    FOUR_PURE_SHIFTED_CHOWS,            ///< Four Pure Shifted Chows
#endif
    THREE_KONGS,                        ///< Three Kongs
    ALL_TERMINALS_AND_HONORS,           ///< All Terminals and Honors

    SEVEN_PAIRS,                        ///< Seven Pairs
    GREATER_HONORS_AND_KNITTED_TILES,   ///< Greater Honors and Knitted Tiles
    ALL_EVEN_PUNGS,                     ///< All Even Pungs
    FULL_FLUSH,                         ///< Full Flush
    PURE_TRIPLE_CHOW,                   ///< Pure Triple Chow
    PURE_SHIFTED_PUNGS,                 ///< Pure Shifted Pungs
    UPPER_TILES,                        ///< Upper Tiles
    MIDDLE_TILES,                       ///< Middle Tiles
    LOWER_TILES,                        ///< Lower Tiles

    PURE_STRAIGHT,                      ///< Pure Straight
    THREE_SUITED_TERMINAL_CHOWS,        ///< Three-Suited Terminal Chows
#if DISTINGUISH_PURE_SHIFTED_CHOWS
    PURE_SHIFTED_CHOWS_1,               ///< Pure Shifted Chows I
    PURE_SHIFTED_CHOWS_2,               ///< Pure Shifted Chows II
#else
    PURE_SHIFTED_CHOWS,                 ///< Pure Shifted Chows
#endif
    ALL_FIVE,                           ///< All Five
    TRIPLE_PUNG,                        ///< Triple Pung
    THREE_CONCEALED_PUNGS,              ///< Three Concealed Pungs

    LESSER_HONORS_AND_KNITTED_TILES,    ///< Lesser Honors and Knitted Tiles
    KNITTED_STRAIGHT,                   ///< Knitted Straight
    UPPER_FOUR,                         ///< Upper Four
    LOWER_FOUR,                         ///< Lower Four
    BIG_THREE_WINDS,                    ///< Big Three Winds

    MIXED_STRAIGHT,                     ///< Mixed Straight
    REVERSIBLE_TILES,                   ///< Reversible Tiles
    MIXED_TRIPLE_CHOW,                  ///< Mixed Triple Chow
    MIXED_SHIFTED_PUNGS,                ///< Mixed Shifted Pungs
    CHICKEN_HAND,                       ///< Chicken Hand
    LAST_TILE_DRAW,                     ///< Last Tile Draw
    LAST_TILE_CLAIM,                    ///< Last Tile Claim
    OUT_WITH_REPLACEMENT_TILE,          ///< Out with Replacement Tile
    ROBBING_THE_KONG,                   ///< Robbing the Kong

    ALL_PUNGS,                          ///< All Pungs
    HALF_FLUSH,                         ///< Half Flush
    MIXED_SHIFTED_CHOWS,                ///< Mixed Shifted Chows
    ALL_TYPES,                          ///< All Types
    MELDED_HAND,                        ///< Melded Hand
    TWO_CONCEALED_KONGS,                ///< Two Concealed Kongs
    TWO_DRAGONS_PUNGS,                  ///< Two Dragons Pungs

    OUTSIDE_HAND,                       ///< Outside Hand
    FULLY_CONCEALED_HAND,               ///< Fully Concealed Hand
    TWO_MELDED_KONGS,                   ///< Two Melded Kongs
    LAST_TILE,                          ///< Last Tile

    DRAGON_PUNG,                        ///< Dragon Pung
    PREVALENT_WIND,                     ///< Prevalent Wind
    SEAT_WIND,                          ///< Seat Wind
    CONCEALED_HAND,                     ///< Concealed Hand
    ALL_CHOWS,                          ///< All Chows
    TILE_HOG,                           ///< Tile Hog
    DOUBLE_PUNG,                        ///< Double Pung
    TWO_CONCEALED_PUNGS,                ///< Two Concealed Pungs
    CONCEALED_KONG,                     ///< Concealed Kong
    ALL_SIMPLES,                        ///< All Simples

    PURE_DOUBLE_CHOW,                   ///< Pure Double Chow
    MIXED_DOUBLE_CHOW,                  ///< Mixed Double Chow
    SHORT_STRAIGHT,                     ///< Short Straight
    TWO_TERMINAL_CHOWS,                 ///< Two Terminal Chows
    PUNG_OF_TERMINALS_OR_HONORS,        ///< Pung of Terminals or Honors
    MELDED_KONG,                        ///< Melded Kong
    ONE_VOIDED_SUIT,                    ///< One Voided Suit
    NO_HONORS,                          ///< No Honors
    EDGE_WAIT,                          ///< Edge Wait
    CLOSED_WAIT,                        ///< Closed Wait
    SINGLE_WAIT,                        ///< Single Wait
    SELF_DRAWN,                         ///< Self-Drawn

    FLOWER_TILES,                       ///< Flower Tiles

#if SUPPORT_CONCEALED_KONG_AND_MELDED_KONG
    CONCEALED_KONG_AND_MELDED_KONG,     ///< Concealed Kong and Melded Kong
#endif

#if SUPPORT_BLESSINGS
    BLESSING_OF_HEAVEN,                 ///< Blessing of Heaven
    BLESSING_OF_EARTH,                  ///< Blessing of Earth
    BLESSING_OF_HUMAN_1,                ///< Blessing of Human I (discard)
    BLESSING_OF_HUMAN_2,                ///< Blessing of Human II (self-drawn)
#endif

    FAN_TABLE_SIZE
};

/**
 * @brief Wind (used to represent prevalent and seat winds)
 */
enum class wind_t {
    EAST, SOUTH, WEST, NORTH
};

/**
 * @brief Win flag
 */
typedef uint8_t win_flag_t;

/**
 * @name win flag
 * @{
 */
#define WIN_FLAG_DISCARD        0   ///< Win by discard
#define WIN_FLAG_SELF_DRAWN     1   ///< Self-drawn
#define WIN_FLAG_LAST_TILE      2   ///< Last tile
#define WIN_FLAG_KONG_INVOLVED  4   ///< Kong involved: combined with discard is Robbing the Kong, with self-drawn is Out with Replacement Tile
#define WIN_FLAG_WALL_LAST      8   ///< Last tile of wall: combined with discard is Last Tile Claim, with self-drawn is Last Tile Draw
#define WIN_FLAG_INITIAL        16  ///< Initial hand: combined with discard is Blessing of Earth (dealer discards) or Blessing of Human (non-dealer discards), with self-drawn is Blessing of Heaven (dealer) or Blessing of Human (non-dealer)

/**
 * @}
 */

/**
 * @name error codes
 * @{
 */
#define ERROR_WRONG_TILES_COUNT -1              ///< Wrong number of tiles
#define ERROR_TILE_MORE_THAN_4 -2               ///< A tile appears more than 4 times
#define ERROR_NOT_WIN -3                        ///< Hand is not a winning hand
/**
 * @}
 */

/**
 * @brief Fan calculation parameters
 */
struct calculate_param_t {
    hand_tiles_t hand_tiles;    ///< Hand tiles
    tile_t win_tile;            ///< Winning tile
    uint8_t flower_count;       ///< Number of flower tiles
    win_flag_t win_flag;        ///< Win flag
    wind_t prevalent_wind;      ///< Prevalent wind
    wind_t seat_wind;           ///< Seat wind
};

/**
 * @brief Fan table
 */
typedef uint16_t fan_table_t[FAN_TABLE_SIZE];

/**
 * @brief Calculate fan
 *
 * @param [in] calculate_param Fan calculation parameters
 * @param [out] fan_table Fan table; when a fan is present, it will be set to the count of occurrences
 * @retval >0 Fan points
 * @retval ERROR_WRONG_TILES_COUNT Wrong number of tiles
 * @retval ERROR_TILE_MORE_THAN_4 A tile appears more than 4 times
 * @retval ERROR_NOT_WIN Hand is not a winning hand
 */
int calculate_fan(const calculate_param_t *calculate_param, fan_table_t *fan_table);

#if SUPPORT_BLESSINGS
/**
 * @brief Revoke Blessing of Heaven, Earth, and Human
 *
 * @param [out] fan_table Fan table
 * @retval Corrected fan points
 */
int revoke_blessings(fan_table_t &fan_table);
#endif

/**
 * @brief Fan names (English)
 */
template <class Dummy = int>
struct fan_name {
    static const char *text[FAN_TABLE_SIZE];
};

template <class Dummy>
const char *fan_name<Dummy>::text[] = {
    "None",
    "Big Four Winds", "Big Three Dragons", "All Green", "Nine Gates", "Four Kongs", "Seven Shifted Pairs", "Thirteen Orphans",
    "All Terminals", "Little Four Winds", "Little Three Dragons", "All Honors", "Four Concealed Pungs", "Pure Terminal Chows",
    "Quadruple Chow", "Four Pure Shifted Pungs",
#if DISTINGUISH_PURE_SHIFTED_CHOWS
    "Four Pure Shifted Chows I", "Four Pure Shifted Chows II",
#else
    "Four Pure Shifted Chows",
#endif
    "Three Kongs", "All Terminals and Honors",
    "Seven Pairs", "Greater Honors and Knitted Tiles", "All Even Pungs", "Full Flush", "Pure Triple Chow", "Pure Shifted Pungs", "Upper Tiles", "Middle Tiles", "Lower Tiles",
    "Pure Straight", "Three-Suited Terminal Chows",
#if DISTINGUISH_PURE_SHIFTED_CHOWS
    "Pure Shifted Chows I", "Pure Shifted Chows II",
#else
    "Pure Shifted Chows",
#endif
    "All Five", "Triple Pung", "Three Concealed Pungs",
    "Lesser Honors and Knitted Tiles", "Knitted Straight", "Upper Four", "Lower Four", "Big Three Winds",
    "Mixed Straight", "Reversible Tiles", "Mixed Triple Chow", "Mixed Shifted Pungs", "Chicken Hand", "Last Tile Draw", "Last Tile Claim", "Out with Replacement Tile", "Robbing the Kong",
    "All Pungs", "Half Flush", "Mixed Shifted Chows", "All Types", "Melded Hand", "Two Concealed Kongs", "Two Dragons Pungs",
    "Outside Hand", "Fully Concealed Hand", "Two Melded Kongs", "Last Tile",
    "Dragon Pung", "Prevalent Wind", "Seat Wind", "Concealed Hand", "All Chows", "Tile Hog", "Double Pung",
    "Two Concealed Pungs", "Concealed Kong", "All Simples",
    "Pure Double Chow", "Mixed Double Chow", "Short Straight", "Two Terminal Chows", "Pung of Terminals or Honors", "Melded Kong", "One Voided Suit", "No Honors", "Edge Wait", "Closed Wait", "Single Wait", "Self-Drawn",
    "Flower Tiles"
#if SUPPORT_CONCEALED_KONG_AND_MELDED_KONG
    , "Concealed Kong and Melded Kong"
#endif
#if SUPPORT_BLESSINGS
    , "Blessing of Heaven", "Blessing of Earth", "Blessing of Human I", "Blessing of Human II"
#endif
};

/**
 * @brief Fan values
 */
template <class Dummy = int>
struct fan_value {
    static const uint16_t table[FAN_TABLE_SIZE];
};

template <class Dummy>
const uint16_t fan_value<Dummy>::table[] = {
    0,
    88, 88, 88, 88, 88, 88, 88,
    64, 64, 64, 64, 64, 64,
    48, 48,
#if DISTINGUISH_PURE_SHIFTED_CHOWS
    32,
#endif
    32, 32, 32,
    24, 24, 24, 24, 24, 24, 24, 24, 24,
#if DISTINGUISH_PURE_SHIFTED_CHOWS
    16,
#endif
    16, 16, 16, 16, 16, 16,
    12, 12, 12, 12, 12,
    8, 8, 8, 8, 8, 8, 8, 8, 8,
    6, 6, 6, 6, 6, 6, 6,
    4, 4, 4, 4,
    2, 2, 2, 2, 2, 2, 2, 2, 2, 2,
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
    1
#if SUPPORT_CONCEALED_KONG_AND_MELDED_KONG
    , 5
#endif
#if SUPPORT_BLESSINGS
    , 8, 8, 8, 8
#endif
};

/**
 * end group
 * @}
 */

}

#endif
