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

#ifndef __MAHJONG_ALGORITHM__TILE_H__
#define __MAHJONG_ALGORITHM__TILE_H__

#include <cstddef>
#include <cstdint>

 // force inline
#ifndef FORCE_INLINE
#if defined(_MSC_VER) && (_MSC_VER >= 1200)
#define FORCE_INLINE __forceinline
#elif defined(__GNUC__) && ((__GNUC__ << 8 | __GNUC_MINOR__) >= 0x301)
#define FORCE_INLINE __inline__ __attribute__((__always_inline__))
#else
#define FORCE_INLINE inline
#endif
#endif

 // unreachable
#ifndef UNREACHABLE
#if defined(_MSC_VER) && (_MSC_VER >= 1300)
#define UNREACHABLE() __assume(0)
#elif defined(__clang__) || (defined(__GNUC__) && ((__GNUC__ << 8 | __GNUC_MINOR__) >= 0x405))
#define UNREACHABLE() __builtin_unreachable()
#else
#define UNREACHABLE() assert(0)
#endif
#endif

namespace mahjong {

using std::uint8_t;
using std::uint16_t;
using std::uint32_t;

using std::intptr_t;

/**
 * @brief Brief terminology used in code comments
 * - Chow: A sequence of 3 consecutive numbered tiles of the same suit.
 * - Pung: Three identical tiles. Claimed from a discard is a melded pung; kept in hand without claiming is a concealed pung. A kong is also treated as a pung (melded kong as melded pung, concealed kong as concealed pung).
 * - Set / Meld: General term for chows and pungs.
 * - Eye / Head / Pair: The isolated pair in a standard winning hand.
 * - Regular Winning Form: A winning hand form consisting of 4 sets and 1 pair.
 * - Special Winning Forms: Winning hand forms not consisting of 4 sets and 1 pair. In MCR, includes Seven Pairs, Thirteen Orphans, Lesser Honors and Knitted Tiles, etc.
 * - Concealed Hand: State of having made no chows, pungs, or melded kongs. Special winning forms are necessarily concealed. Concealed kongs do not break the concealed hand condition, but reveal information that the hand is not a special winning form.
 * - Melds: General term for chows, pungs, and kongs claiming tiles discarded by other players to complete sets; usually excludes concealed kongs. When including concealed kongs, they are called concealed melds, while chows, pungs, and melded kongs are called open/melded melds.
 * - Standing Tiles: Hand tiles excluding melded chows, pungs, and kongs.
 * - Hand Tiles: The entire hand including standing tiles and melded chows, pungs, and kongs; sometimes refers only to standing tiles.
 * - Ready Hand / Waiting: The state of needing only one tile to complete a winning hand.
 * - 1-Shanten: The state of being one tile away from ready/waiting. Similarly, 2-shanten, 3-shanten, N-shanten.
 * - Shanten Number: The minimum number of tiles needed to reach the ready/waiting state.
 * - Useful Tiles: Tiles that reduce the shanten number (advancing tiles).
 * - Improvement Tiles: Tiles that increase the number of useful tiles (broadening the waiting range).
 * - Pair: Two identical tiles. An eye/head must be a pair, but a pair is not necessarily the eye/head.
 * - Two-sided Wait: Two consecutive numbered tiles of the same suit (e.g., 45m), which can form a chow with adjacent tiles on either side.
 * - Closed Wait: Two numbered tiles of the same suit separated by 1 rank (e.g., 57s), which can only form a chow with the middle tile.
 * - Edge Wait: Two consecutive numbered tiles at boundary positions (e.g., 12m or 89s), which can only form a chow with a tile on one side (3m or 7s).
 * - Proto-run / Incomplete Set: Two tiles that need one more tile to complete a set (pairs/incomplete pungs, two-sided waits, closed waits, edge waits).
 * - Complex Incomplete Set: Incomplete set formed by 3 or more tiles, such as consecutive closed waits, two-sided wait with a pair, closed wait with a pair, edge wait with a pair, etc.
 * - Dual Wait / Double-Pair Wait: When waiting for a win, all other tiles have formed sets and two pairs remain; either pair matching completes a pung to win, with the other serving as the eye/head.
 */


/**
 * @addtogroup tile
 * @{
 */

/**
 * @brief Suit
 */
typedef uint8_t suit_t;

/**
 * @brief Rank
 */
typedef uint8_t rank_t;

#define TILE_SUIT_NONE          0  ///< Invalid
#define TILE_SUIT_CHARACTERS    1  ///< Characters (CHARACTERS)
#define TILE_SUIT_BAMBOO        2  ///< Bamboo (BAMBOO)
#define TILE_SUIT_DOTS          3  ///< Dots (DOTS)
#define TILE_SUIT_HONORS        4  ///< Honors (HONORS)

/**
 * @brief Tile\n
 * Memory layout:
 * - 0-3 4-bit tile rank
 * - 4-7 4-bit tile suit
 * Valid tiles are:
 * - 0x11 - 0x19 Characters (CHARACTERS)
 * - 0x21 - 0x29 Bamboo (BAMBOO)
 * - 0x31 - 0x39 Dots (DOTS)
 * - 0x41 - 0x47 Honors (HONORS)
 * - 0x51 - 0x58 Flowers (FLOWER)
 */
typedef uint8_t tile_t;

/**
 * @brief Make a tile
 *  The function does not validate input legality. If an invalid value is passed, the legality of the return value cannot be guaranteed.
 * @param [in] suit Suit
 * @param [in] rank Rank
 * @return tile_t Tile
 */
static FORCE_INLINE tile_t make_tile(suit_t suit, rank_t rank) {
    return (((suit & 0xF) << 4) | (rank & 0xF));
}

/**
 * @brief Get the suit of a tile
 *  The function does not validate input legality. If an invalid value is passed, the legality of the return value cannot be guaranteed.
 * @param [in] tile Tile
 * @return suit_t Suit
 */
static FORCE_INLINE suit_t tile_get_suit(tile_t tile) {
    return ((tile >> 4) & 0xF);
}

/**
 * @brief Check if a tile is a flower tile
 *  The function does not validate input legality. If an invalid value is passed, the legality of the return value cannot be guaranteed.
 * @param [in] tile Tile
 * @return bool
 */
static FORCE_INLINE bool is_flower(tile_t tile) {
    return ((tile >> 4) & 0xF) == 5;
}

/**
 * @brief Get the rank of a tile
 *  The function does not validate input legality. If an invalid value is passed, the legality of the return value cannot be guaranteed.
 * @param [in] tile Tile
 * @return rank_t Rank
 */
static FORCE_INLINE rank_t tile_get_rank(tile_t tile) {
    return (tile & 0xF);
}

/**
 * @brief Values of all tiles, excluding flower tiles
 */
enum tile_value_t {
    TILE_1m = 0x11, TILE_2m, TILE_3m, TILE_4m, TILE_5m, TILE_6m, TILE_7m, TILE_8m, TILE_9m,
    TILE_1s = 0x21, TILE_2s, TILE_3s, TILE_4s, TILE_5s, TILE_6s, TILE_7s, TILE_8s, TILE_9s,
    TILE_1p = 0x31, TILE_2p, TILE_3p, TILE_4p, TILE_5p, TILE_6p, TILE_7p, TILE_8p, TILE_9p,
    TILE_E  = 0x41, TILE_S , TILE_W , TILE_N , TILE_C , TILE_F , TILE_P ,
    TILE_TABLE_SIZE
};

template <typename Dummy = int>
struct standard_tiles {
    static const tile_t all[34];  // All valid tiles, excluding flower tiles

    static bool check(tile_t tile) {
        return valid_map[tile >> 5] & (1u << (tile & 0x1f));
    }

    static const tile_t thirteen_orphans[13];  // Thirteen Orphans 13-way wait
    static const tile_t knitted_straight[6][9];  // Exactly 6 patterns of Knitted Straight

private:
    static const uint32_t valid_map[8];
};

/**
 * @brief All valid tiles, excluding flower tiles
 */
template <typename Dummy>
const tile_t standard_tiles<Dummy>::all[] = {
    TILE_1m, TILE_2m, TILE_3m, TILE_4m, TILE_5m, TILE_6m, TILE_7m, TILE_8m, TILE_9m,
    TILE_1s, TILE_2s, TILE_3s, TILE_4s, TILE_5s, TILE_6s, TILE_7s, TILE_8s, TILE_9s,
    TILE_1p, TILE_2p, TILE_3p, TILE_4p, TILE_5p, TILE_6p, TILE_7p, TILE_8p, TILE_9p,
    TILE_E , TILE_S , TILE_W , TILE_N , TILE_C , TILE_F , TILE_P
};

template <typename Dummy>
const tile_t standard_tiles<Dummy>::thirteen_orphans[13] = {
    TILE_1m, TILE_9m, TILE_1s, TILE_9s, TILE_1p, TILE_9p, TILE_E, TILE_S, TILE_W, TILE_N, TILE_C, TILE_F, TILE_P
};

// Exactly 6 patterns of Knitted Straight:
// 147m 258s 369p
// 147m 369s 258p
// 258m 147s 369p
// 258m 369s 147p
// 369m 147s 258p
// 369m 258s 147p
template <typename Dummy>
const tile_t standard_tiles<Dummy>::knitted_straight[6][9] = {
    { TILE_1m, TILE_4m, TILE_7m, TILE_2s, TILE_5s, TILE_8s, TILE_3p, TILE_6p, TILE_9p },
    { TILE_1m, TILE_4m, TILE_7m, TILE_3s, TILE_6s, TILE_9s, TILE_2p, TILE_5p, TILE_8p },
    { TILE_2m, TILE_5m, TILE_8m, TILE_1s, TILE_4s, TILE_7s, TILE_3p, TILE_6p, TILE_9p },
    { TILE_2m, TILE_5m, TILE_8m, TILE_3s, TILE_6s, TILE_9s, TILE_1p, TILE_4p, TILE_7p },
    { TILE_3m, TILE_6m, TILE_9m, TILE_1s, TILE_4s, TILE_7s, TILE_2p, TILE_5p, TILE_8p },
    { TILE_3m, TILE_6m, TILE_9m, TILE_2s, TILE_5s, TILE_8s, TILE_1p, TILE_4p, TILE_7p },
};

// Table used to help validate whether a tile is valid
template <typename Dummy>
const uint32_t standard_tiles<Dummy>::valid_map[] = {
    0x3fe0000u, 0x3fe03feu, 0xfeu
};


/**
 * @brief Tile table type
 *
 * Description: In algorithms such as ready hand checks and shanten calculations, there are two mainstream ways to store tiles:
 * - One is using a tile table (histogram), where each index represents the count of that tile. The advantage is that when reducing sets during recursive calculation, one only needs to modify values at the corresponding index in the table; the disadvantage is that the total count of tiles in the hand is inconvenient to determine.
 * - The other is directly using an array of tiles. The advantage is that the total count of tiles in hand is easy to determine; the disadvantage is that reducing sets during recursive calculation is inconvenient, requiring element deletion from the array.
 */
typedef uint16_t tile_table_t[TILE_TABLE_SIZE];

#define PACK_TYPE_NONE 0  ///< Invalid
#define PACK_TYPE_CHOW 1  ///< Chow
#define PACK_TYPE_PUNG 2  ///< Pung
#define PACK_TYPE_KONG 3  ///< Kong
#define PACK_TYPE_PAIR 4  ///< Pair / Eye

/**
 * @brief Pack / Meld
 *  Used to represent a set (meld) or a pair (eye).
 *
 * Memory layout:
 * - 0-7 8-bit tile: Tile (for a chow, represents the middle tile; e.g., for 234p, the tile is 3p)
 * - 8-11 4-bit type: Pack type, using PACK_TYPE_xxx macros
 * - 12-13 2-bit offer: Offer/source information, range 0-3\n
 * - 14 1-bit promoted: Whether this is an added kong (promoted kong)
 *       0 indicates concealed (concealed chow, concealed pung, concealed kong), non-zero indicates melded (melded chow, melded pung, melded kong)\n
 *       When the pack is a pung or kong, 1, 2, 3 indicate offered by upper seat / opposite seat / lower seat respectively\n
 *       When the pack is a chow, since claiming a chow can only be from the upper seat, 1, 2, 3 indicate which tile (1st, 2nd, or 3rd) was offered by the upper seat
 */
typedef uint16_t pack_t;

/**
 * @brief Make a pack
 *  The function does not validate input legality. If an invalid value is passed, the legality of the return value cannot be guaranteed.
 * @param [in] offer Offer info
 * @param [in] type Pack type
 * @param [in] tile Tile (for a chow, the middle tile)
 */
static FORCE_INLINE pack_t make_pack(uint8_t offer, uint8_t type, tile_t tile) {
    return (offer << 12 | (type << 8) | tile);
}

/**
 * @brief Check if pack is melded
 *  The function does not validate input legality. If an invalid value is passed, the legality of the return value cannot be guaranteed.
 * @param [in] pack Pack
 * @return bool
 */
static FORCE_INLINE bool is_pack_melded(pack_t pack) {
    return !!(pack & 0x3000);
}

/**
 * @brief Check if pack is an added kong (promoted kong)
 *  Meaningless when pack is not PACK_TYPE_KONG.
 *  The function does not validate input legality. If an invalid value is passed, the legality of the return value cannot be guaranteed.
 * @param [in] pack Pack
 * @return bool
 */
static FORCE_INLINE bool is_promoted_kong(pack_t pack) {
    return !!(pack & 0x4000);
}

/**
 * @brief Convert a pung pack to an added kong
 *  Meaningless when pack is not PACK_TYPE_PUNG.
 *  The function does not validate input legality. If an invalid value is passed, the legality of the return value cannot be guaranteed.
 * @param [in] pack Pung pack
 * @return pack_t Added kong pack
 */
static FORCE_INLINE pack_t promote_pung_to_kong(pack_t pack) {
    return pack | 0x4300;
}

/**
 * @brief Get offer info of pack
 *  The function does not validate input legality. If an invalid value is passed, the legality of the return value cannot be guaranteed.
 * @param [in] pack Pack
 * @return uint8_t
 */
static FORCE_INLINE uint8_t pack_get_offer(pack_t pack) {
    return ((pack >> 12) & 0x3);
}

/**
 * @brief Get the type of pack
 *  The function does not validate input legality. If an invalid value is passed, the legality of the return value cannot be guaranteed.
 * @param [in] pack Pack
 * @return uint8_t Pack type
 */
static FORCE_INLINE uint8_t pack_get_type(pack_t pack) {
    return ((pack >> 8) & 0xF);
}

/**
 * @brief Get the tile of pack
 *  The function does not validate input legality. If an invalid value is passed, the legality of the return value cannot be guaranteed.
 * @param [in] pack Pack
 * @return tile_t Tile (for a chow, the middle tile)
 */
static FORCE_INLINE tile_t pack_get_tile(pack_t pack) {
    return (pack & 0xFF);
}

/**
 * @brief Hand tiles structure
 *  Hand tiles structure must satisfy the equation: 3 * melded pack count + standing tile count = 13
 */
struct hand_tiles_t {
    pack_t fixed_packs[5];      ///< Melded packs (sets), including concealed kongs
    intptr_t pack_count;        ///< Melded pack (set) count, including concealed kongs
    tile_t standing_tiles[13];  ///< Standing tiles
    intptr_t tile_count;        ///< Standing tile count
};

// Eigenvalue of a pack, used to determine whether a partition has already been visited
typedef uint32_t eigen_t;

static FORCE_INLINE eigen_t make_eigen(tile_t t1, tile_t t2, tile_t t3) {
    return (static_cast<eigen_t>(t1) << 16 | static_cast<eigen_t>(t2) << 8 | t3);
}

/**
 * @brief Check if a tile can be part of All Green
 *  The function does not validate input legality. If an invalid value is passed, the legality of the return value cannot be guaranteed.
 * @param [in] tile Tile
 * @return bool
 */
static FORCE_INLINE bool is_green(tile_t tile) {
    // Basic element check: 23468s and Green Dragon (Fa) are green tiles
    //return (tile == TILE_2s || tile == TILE_3s || tile == TILE_4s || tile == TILE_6s || tile == TILE_8s || tile == TILE_F);

    // Algorithm principle:
    // 0x48-0x11=0x37=55 fits within a 64-bit integer range.
    // Use each bit of uint64_t to represent a tile flag, precompute a magic number,
    // and then test the corresponding bit.
    return !!(0x0020000000AE0000ULL & (1ULL << (tile - TILE_1m)));
}

/**
 * @brief Check if a tile can be part of Reversible Tiles
 *  The function does not validate input legality. If an invalid value is passed, the legality of the return value cannot be guaranteed.
 * @param [in] tile Tile
 * @return bool
 */
static FORCE_INLINE bool is_reversible(tile_t tile) {
    // Basic element check: 245689s, 1234589p, and White Dragon (Bai) are reversible tiles
    //return (tile == TILE_2s || tile == TILE_4s || tile == TILE_5s || tile == TILE_6s || tile == TILE_8s || tile == TILE_9s ||
    //    tile == TILE_1p || tile == TILE_2p || tile == TILE_3p || tile == TILE_4p || tile == TILE_5p || tile == TILE_8p || tile == TILE_9p ||
    //    tile == TILE_P);

    // Algorithm principle same as is_green
    return !!(0x0040019F01BA0000ULL & (1ULL << (tile - TILE_1m)));
}

/**
 * @brief Check if a tile is a terminal (1 or 9 of numbered suit)
 *  The function does not validate input legality. If an invalid value is passed, the legality of the return value cannot be guaranteed.
 * @param [in] tile Tile
 * @return bool
 */
static FORCE_INLINE bool is_terminal(tile_t tile) {
    // Basic element check
    //return (tile == TILE_1m || tile == TILE_9m || tile == TILE_1s || tile == TILE_9s || tile == TILE_1p || tile == TILE_9p);

    // Algorithm principle: Observe binary bits of terminals:
    // 0x11: 0001 0001
    // 0x19: 0001 1001
    // 0x21: 0010 0001
    // 0x29: 0010 1001
    // 0x31: 0011 0001
    // 0x39: 0011 1001
    // The lower 4 bits of all tiles only appear between 0001 and 1001; bitwise AND with 0111 gives 1 only for 0001 and 1001.
    // The upper 4 bits of all numbered tiles only appear between 0001 and 0011; bitwise AND with 1100 must be 0.
    // Thus constructing magic number 0xC7 (1100 0111) and bitwise ANDing with tile, if result is 1, it is a terminal.
    // Flaw: The lower 4-bit operation misjudges 0xB, 0xD, 0xF; upper 4-bit operation misjudges 0x01 and 0x09.
    return ((tile & 0xC7) == 1);
}

/**
 * @brief Check if a tile is a wind tile
 * @param [in] tile Tile
 * @return bool
 */
static FORCE_INLINE bool is_winds(tile_t tile) {
    return (tile > 0x40 && tile < 0x45);
}

/**
 * @brief Check if a tile is a dragon tile
 * @param [in] tile Tile
 * @return bool
 */
static FORCE_INLINE bool is_dragons(tile_t tile) {
    return (tile > 0x44 && tile < 0x48);
}

/**
 * @brief Check if a tile is an honor tile
 * @param [in] tile Tile
 * @return bool
 */
static FORCE_INLINE bool is_honor(tile_t tile) {
    return (tile > 0x40 && tile < 0x48);
}

/**
 * @brief Check if a tile is a numbered tile
 * @param [in] tile Tile
 * @return bool
 */
static FORCE_INLINE bool is_numbered_suit(tile_t tile) {
    if (tile < 0x1A) return (tile > 0x10);
    if (tile < 0x2A) return (tile > 0x20);
    if (tile < 0x3A) return (tile > 0x30);
    return false;
}

/**
 * @brief Check if a tile is a numbered tile (faster)
 *  The function does not validate input legality. If an invalid value is passed, the legality of the return value cannot be guaranteed.
 * @see is_numbered_suit
 * @param [in] tile Tile
 * @return bool
 */
static FORCE_INLINE bool is_numbered_suit_quick(tile_t tile) {
    // Algorithm principle: Numbered tiles are 0x11-0x19, 0x21-0x29, 0x31-0x39; bitwise AND with 0xC0 gives 0.
    return !(tile & 0xC0);
}

/**
 * @brief Check if a tile is a terminal or honor tile
 * @param [in] tile Tile
 * @return bool
 */
static FORCE_INLINE bool is_terminal_or_honor(tile_t tile) {
    return is_terminal(tile) || is_honor(tile);
}

/**
 * @brief Check if two tiles have the same suit (faster)
 *  The function does not validate input legality. If an invalid value is passed, the legality of the return value cannot be guaranteed.
 * @param [in] tile0 Tile 0
 * @param [in] tile1 Tile 1
 * @return bool
 */
static FORCE_INLINE bool is_suit_equal_quick(tile_t tile0, tile_t tile1) {
    // Algorithm principle: Upper 4 bits represent suit
    return ((tile0 & 0xF0) == (tile1 & 0xF0));
}

/**
 * @brief Check if two tiles have the same rank (faster)
 *  The function does not validate input legality. If an invalid value is passed, the legality of the return value cannot be guaranteed.
 * @param [in] tile0 Tile 0
 * @param [in] tile1 Tile 1
 * @return bool
 */
static FORCE_INLINE bool is_rank_equal_quick(tile_t tile0, tile_t tile1) {
    // Algorithm principle: Lower 4 bits represent rank. Upper 4 bits set to C to filter out honor tiles
    return ((tile0 & 0xCF) == (tile1 & 0xCF));
}

/**
 * end group
 * @}
 */

}

#endif
