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

#include "fan_calculator.h"
#include <cassert>
#include <cstddef>
#include <cstring>
#include <algorithm>
#include <iterator>

/**
 * Overview of fan calculation process:
 * 1. Check special winning forms
 *   (1) Concealed hand state: Seven Pairs, Thirteen Orphans, Knitted Straight, Lesser Honors and Knitted Tiles, Nine Gates
 *   (2) 1 meld state: Knitted Straight
 * 2. Decompose by regular winning form (note Seven Pairs)
 * 3. Calculate fan points for decompositions from step 2, take the maximum

 * Scoring process for decomposed results:
 * 1. Count chows and pungs, score pungs themselves (involves: Pung of Terminals or Honors, Dragon Pung)
 * 2. Score sets by category:
 *   (1) 4 chows: first check Three-Suited / Pure Terminal Chows; if not, calculate fan for 4 chows
 *   (2) 3 chows + 1 pung: calculate fan for 3 chows
 *   (3) 2 chows + 2 pungs: calculate fan for 2 chows, calculate fan for 2 pungs
 *   (4) 1 chow + 3 pungs: calculate fan for 3 pungs
 * 3. Adjust by win method: involves Fully Concealed Hand, Melded Hand
 * 4. Adjust by eye/head: involves All Chows, Little Three Dragons, Little Four Winds
 * 5. Adjust by pack characteristics: involves Outside Hand, All With Five, All Even Pungs
 * 6. Adjust by suits: involves No Honors, One Voided Suit, Half Flush, Full Flush, All Types
 * 7. Adjust by tile characteristics: involves All Simples, Reversible Tiles, All Green, All Honors, All Terminals, All Terminals and Honors
 * 8. Adjust by numbered tile rank ranges: involves Upper Four, Lower Four, All Upper, All Five, All Lower
 * 9. Calculate Tile Hog
 * 10. Adjust by waiting type: involves Edge Wait, Closed Wait, Single Wait
 * 11. Adjust excluded fan combinations defined by rules
 * 12. Finally adjust Prevalent Wind and Seat Wind
 * 13. After all steps above, obtain fan result; if 0 fan, adjust to Chicken Hand
 */

#define MAX_DIVISION_CNT 20  // A hand will not have more than 20 divisions, enough

#if 0
#define LOG(fmt_, ...) std::printf(fmt_, ##__VA_ARGS__)
#else
#define LOG(...) ((void)0)
#endif

//#define STRICT_98_RULE

namespace mahjong {

// Map tiles to table
static void map_tiles(const tile_t *tiles, intptr_t cnt, tile_table_t &tile_table) {
    for (intptr_t i = 0; i < cnt; ++i) {
        ++tile_table[tiles[i]];
    }
}

// Map packs to table
static void map_packs(const pack_t *packs, intptr_t cnt, tile_table_t &tile_table) {
    for (intptr_t i = 0; i < cnt; ++i) {
        tile_t tile = pack_get_tile(packs[i]);
        switch (pack_get_type(packs[i])) {
        case PACK_TYPE_CHOW:
            ++tile_table[tile - 1];
            ++tile_table[tile];
            ++tile_table[tile + 1];
            break;
        case PACK_TYPE_PUNG:
            tile_table[tile] += 3;
            break;
        case PACK_TYPE_KONG:
            tile_table[tile] += 4;
            break;
        case PACK_TYPE_PAIR:
            tile_table[tile] += 2;
            break;
        default:
            UNREACHABLE();
            break;
        }
    }
}

// Get unique tiles in this hand (used to quickly check suits, ranges, and related fan)
static intptr_t table_unique(const tile_table_t &fixed_table, const tile_table_t &standing_table, tile_t *tile) {
    tile_t *p = tile;
    const auto &all_tiles = standard_tiles<>::all;
    for (int i = 0; i < 34; ++i) {
        tile_t t = all_tiles[i];
        if (fixed_table[t] || standing_table[t]) {
            *p++ = t;
        }
    }
    return p - tile;
}

//-------------------------------- Decomposition --------------------------------

namespace {

    // Division
    struct division_t {
        pack_t packs[5];  // Packs: 4 sets and 1 pair, 5 in total
    };

    // Division results
    struct division_result_t {
        division_t divisions[MAX_DIVISION_CNT];  // Each division
        intptr_t count;  // Total number of divisions
    };
}

// Final step of recursive division algorithm: add division
static void divide_tail_add_division(intptr_t fixed_cnt, const division_t *work_division, division_result_t *result) {
#ifdef __GNUC__
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Warray-bounds"
#endif

    // Make a copy of current divided sets, and sort concealed sets
    // Cannot sort directly on work_division->packs, otherwise outer recursive data would be modified
    division_t temp;
    std::memcpy(&temp, work_division, sizeof(temp));
    if (fixed_cnt < 4) {
        std::sort(temp.packs + fixed_cnt, temp.packs + 4);
    }

    // Check whether this division already exists
    if (std::none_of(&result->divisions[0], &result->divisions[result->count],
        [&temp, fixed_cnt](const division_t &od) {
        return std::equal(&od.packs[fixed_cnt], &od.packs[4], &temp.packs[fixed_cnt]);
    })) {
        // Write into division results
        std::memcpy(&result->divisions[result->count], &temp, sizeof(temp));
        ++result->count;
    }
    else {
        LOG("same case");
    }

#ifdef __GNUC__
#pragma GCC diagnostic pop
#endif
}

// Final step of recursive division
static bool divide_tail(tile_table_t &tile_table, intptr_t fixed_cnt, division_t *work_division, division_result_t *result) {
    const auto &all_tiles = standard_tiles<>::all;
    for (int i = 0; i < 34; ++i) {
        tile_t t = all_tiles[i];
        if (tile_table[t] < 2) {
            continue;
        }

        tile_table[t] -= 2;  // Deduct
        // All tiles are completely used
        if (std::all_of(std::begin(tile_table), std::end(tile_table), [](int n) { return n == 0; })) {
            tile_table[t] += 2;  // Restore

            // These 2 tiles serve as the pair (eye)
            work_division->packs[4] = make_pack(0, PACK_TYPE_PAIR, t);
            divide_tail_add_division(fixed_cnt, work_division, result);  // Record
            return true;
        }
        tile_table[t] += 2;  // Restore
    }

    return false;
}

// Recursive division
static bool divide_recursively(tile_table_t &tile_table, intptr_t fixed_cnt, intptr_t step, eigen_t prev_eigen, division_t *work_division, division_result_t *result) {
    const intptr_t idx = step + fixed_cnt;
    if (idx == 4) {  // Already have 4 sets
        return divide_tail(tile_table, fixed_cnt, work_division, result);
    }

    bool ret = false;

    // Iterate over tiles according to tile table
    const auto &all_tiles = standard_tiles<>::all;
    for (int i = 0; i < 34; ++i) {
        tile_t t = all_tiles[i];
        if (tile_table[t] < 1) {
            continue;
        }

        // Pung
        if (tile_table[t] > 2) {
            // If current pung eigenvalue is less than previous, this path has already been visited
            eigen_t eigen = make_eigen(t, t, t);
            if (eigen > prev_eigen) {
                work_division->packs[idx] = make_pack(0, PACK_TYPE_PUNG, t);  // Record pung
                // Deduct this pung, recurse
                tile_table[t] -= 3;
                if (divide_recursively(tile_table, fixed_cnt, step + 1, eigen, work_division, result)) {
                    ret = true;
                }
                // Restore
                tile_table[t] += 3;
            }
        }

        // Chow (numbered suits only)
        if (is_numbered_suit(t)) {
            if (tile_get_rank(t) < 8 && tile_table[t + 1] && tile_table[t + 2]) {
                // If current chow eigenvalue is less than previous, this path has already been visited
                eigen_t eigen = make_eigen(t, t + 1, t + 2);
                if (eigen >= prev_eigen) {
                    work_division->packs[idx] = make_pack(0, PACK_TYPE_CHOW, static_cast<tile_t>(t + 1));  // Record chow
                    // Deduct this chow, recurse
                    --tile_table[t];
                    --tile_table[t + 1];
                    --tile_table[t + 2];
                    if (divide_recursively(tile_table, fixed_cnt, step + 1, eigen, work_division, result)) {
                        ret = true;
                    }
                    // Restore
                    ++tile_table[t];
                    ++tile_table[t + 1];
                    ++tile_table[t + 2];
                }
            }
        }
    }

    return ret;
}

static bool has_pair(const tile_table_t &tile_table) {
    const auto &all_tiles = standard_tiles<>::all;
    for (int i = 0; i < 34; ++i) {
        if (tile_table[all_tiles[i]] > 1) {
            return true;
        }
    }
    return false;
}

// Decompose a hand
static bool divide_win_hand(tile_table_t &standing_table, const pack_t *fixed_packs, intptr_t fixed_cnt, division_result_t *result) {
    result->count = 0;

    if (!has_pair(standing_table)) {
        return false;
    }

    // Copy melded sets
    division_t work_division;
    std::memcpy(work_division.packs, fixed_packs, fixed_cnt * sizeof(pack_t));
    return divide_recursively(standing_table, fixed_cnt, 0, 0, &work_division, result);
}

//-------------------------------- Fan Calculation --------------------------------

// 4 sets incrementing by 1
static FORCE_INLINE bool is_four_shifted_1(rank_t r0, rank_t r1, rank_t r2, rank_t r3) {
    return (r0 + 1 == r1 && r1 + 1 == r2 && r2 + 1 == r3);
}

// 4 sets incrementing by 2
static FORCE_INLINE bool is_four_shifted_2(rank_t r0, rank_t r1, rank_t r2, rank_t r3) {
    return (r0 + 2 == r1 && r1 + 2 == r2 && r2 + 2 == r3);
}

// 3 sets incrementing by 1
static FORCE_INLINE bool is_shifted_1(rank_t r0, rank_t r1, rank_t r2) {
    return (r0 + 1 == r1 && r1 + 1 == r2);
}

// 3 sets incrementing by 2
static FORCE_INLINE bool is_shifted_2(rank_t r0, rank_t r1, rank_t r2) {
    return (r0 + 2 == r1 && r1 + 2 == r2);
}

// Three suits
static FORCE_INLINE bool is_mixed(suit_t s0, suit_t s1, suit_t s2) {
    return (s0 != s1 && s0 != s2 && s1 != s2);
}

// 3 sets incrementing by 1 unordered
static FORCE_INLINE bool is_shifted_1_unordered(rank_t r0, rank_t r1, rank_t r2) {
    return is_shifted_1(r1, r0, r2) || is_shifted_1(r2, r0, r1) || is_shifted_1(r0, r1, r2)
        || is_shifted_1(r2, r1, r0) || is_shifted_1(r0, r2, r1) || is_shifted_1(r1, r2, r0);
}

// Fan for 4 chows
static fan_t get_4_chows_fan(tile_t t0, tile_t t1, tile_t t2, tile_t t3) {
    // In order of frequency of occurrence

    // Four Pure Shifted Chows
    if (is_four_shifted_2(t0, t1, t2, t3)) {
#if DISTINGUISH_PURE_SHIFTED_CHOWS
        return FOUR_PURE_SHIFTED_CHOWS_2;
#else
        return FOUR_PURE_SHIFTED_CHOWS;
#endif
    }
    if (is_four_shifted_1(t0, t1, t2, t3)) {
#if DISTINGUISH_PURE_SHIFTED_CHOWS
        return FOUR_PURE_SHIFTED_CHOWS_1;
#else
        return FOUR_PURE_SHIFTED_CHOWS;
#endif
    }
    // Quadruple Chow
    if (t0 == t1 && t0 == t2 && t0 == t3) {
        return QUADRUPLE_CHOW;
    }
    // None of the above
    return FAN_NONE;
}

// Fan for 3 chows
static fan_t get_3_chows_fan(tile_t t0, tile_t t1, tile_t t2) {
    suit_t s0 = tile_get_suit(t0);
    suit_t s1 = tile_get_suit(t1);
    suit_t s2 = tile_get_suit(t2);

    rank_t r0 = tile_get_rank(t0);
    rank_t r1 = tile_get_rank(t1);
    rank_t r2 = tile_get_rank(t2);

    // In order of frequency of occurrence

    if (is_mixed(s0, s1, s2)) {  // Three suits
        // Mixed Shifted Chows
        if (is_shifted_1_unordered(r1, r0, r2)) {
            return MIXED_SHIFTED_CHOWS;
        }
        // Mixed Triple Chow
        if (r0 == r1 && r1 == r2) {
            return MIXED_TRIPLE_CHOW;
        }
        // Mixed Straight
        if ((r0 == 2 && r1 == 5 && r2 == 8) || (r0 == 2 && r1 == 8 && r2 == 5)
            || (r0 == 5 && r1 == 2 && r2 == 8) || (r0 == 5 && r1 == 8 && r2 == 2)
            || (r0 == 8 && r1 == 2 && r2 == 5) || (r0 == 8 && r1 == 5 && r2 == 2)) {
            return MIXED_STRAIGHT;
        }
    }
    else {  // One suit
        // Pure Straight
        if (t0 + 3 == t1 && t1 + 3 == t2) {
            return PURE_STRAIGHT;
        }
        // Pure Shifted Chows
        if (is_shifted_2(t0, t1, t2)) {
#if DISTINGUISH_PURE_SHIFTED_CHOWS
            return PURE_SHIFTED_CHOWS_2;
#else
            return PURE_SHIFTED_CHOWS;
#endif
        }
        if (is_shifted_1(t0, t1, t2)) {
#if DISTINGUISH_PURE_SHIFTED_CHOWS
            return PURE_SHIFTED_CHOWS_1;
#else
            return PURE_SHIFTED_CHOWS;
#endif
        }
        // Pure Triple Chow
        if (t0 == t1 && t0 == t2) {
            return PURE_TRIPLE_CHOW;
        }
    }
    // None of the above
    return FAN_NONE;
}

// Fan for 2 chows
static fan_t get_2_chows_fan_unordered(tile_t t0, tile_t t1) {
    // In order of frequency of occurrence

    if (!is_suit_equal_quick(t0, t1)) {  // Two suits
        // Mixed Double Chow
        if (is_rank_equal_quick(t0, t1)) {
            return MIXED_DOUBLE_CHOW;
        }
    }
    else {  // One suit
        // Short Straight
        if (t0 + 3 == t1 || t1 + 3 == t0) {
            return SHORT_STRAIGHT;
        }
        // Two Terminal Chows
        rank_t r0 = tile_get_rank(t0);
        rank_t r1 = tile_get_rank(t1);
        if ((r0 == 2 && r1 == 8) || (r0 == 8 && r1 == 2)) {
            return TWO_TERMINAL_CHOWS;
        }
        // Pure Double Chow
        if (t0 == t1) {
            return PURE_DOUBLE_CHOW;
        }
    }
    // None of the above
    return FAN_NONE;
}

// Fan for 4 pungs
static fan_t get_4_pungs_fan(tile_t t0, tile_t t1, tile_t t2, tile_t t3) {
    // Four Pure Shifted Pungs
    if (is_numbered_suit_quick(t0) && t0 + 1 == t1 && t1 + 1 == t2 && t2 + 1 == t3) {
        return FOUR_PURE_SHIFTED_PUNGS;
    }
    // Big Four Winds
    if (t0 == TILE_E && t1 == TILE_S && t2 == TILE_W && t3 == TILE_N) {
        return BIG_FOUR_WINDS;
    }
    // None of the above
    return FAN_NONE;
}

// Fan for 3 pungs
static fan_t get_3_pungs_fan(tile_t t0, tile_t t1, tile_t t2) {
    // In order of frequency of occurrence

    if (is_numbered_suit_quick(t0) && is_numbered_suit_quick(t1) && is_numbered_suit_quick(t2)) {  // Numbered suits
        suit_t s0 = tile_get_suit(t0);
        suit_t s1 = tile_get_suit(t1);
        suit_t s2 = tile_get_suit(t2);

        rank_t r0 = tile_get_rank(t0);
        rank_t r1 = tile_get_rank(t1);
        rank_t r2 = tile_get_rank(t2);

        if (is_mixed(s0, s1, s2)) {  // Three suits
            // Mixed Shifted Pungs
            if (is_shifted_1_unordered(r1, r0, r2)) {
                return MIXED_SHIFTED_PUNGS;
            }
            // Triple Pung
            if (r0 == r1 && r1 == r2) {
                return TRIPLE_PUNG;
            }
        }
        else {
            // Pure Shifted Pungs
            if (t0 + 1 == t1 && t1 + 1 == t2) {
                return PURE_SHIFTED_PUNGS;
            }
        }
    }
    else {
        // Big Three Winds
        if ((t0 == TILE_E && t1 == TILE_S && t2 == TILE_W)
            || (t0 == TILE_E && t1 == TILE_S && t2 == TILE_N)
            || (t0 == TILE_E && t1 == TILE_W && t2 == TILE_N)
            || (t0 == TILE_S && t1 == TILE_W && t2 == TILE_N)) {
            return BIG_THREE_WINDS;
        }
        // Big Three Dragons
        if (t0 == TILE_C && t1 == TILE_F && t2 == TILE_P) {
            return BIG_THREE_DRAGONS;
        }
    }

    // None of the above
    return FAN_NONE;
}

// Fan for 2 pungs
static fan_t get_2_pungs_fan_unordered(tile_t t0, tile_t t1) {
    // In order of frequency of occurrence
    if (is_numbered_suit_quick(t0) && is_numbered_suit_quick(t1)) {  // Numbered suits
        // Double Pung
        if (is_rank_equal_quick(t0, t1)) {
            return DOUBLE_PUNG;
        }
    }
    else {
        // Two Dragons Pungs
        if (is_dragons(t0) && is_dragons(t1)) {
            return TWO_DRAGONS_PUNGS;
        }
    }
    // None of the above
    return FAN_NONE;
}

// Fan for 1 pung
static fan_t get_1_pung_fan(tile_t mid_tile) {
    // Dragon Pung
    if (is_dragons(mid_tile)) {
        return DRAGON_PUNG;
    }
    // Pung of Terminals or Honors
    if (is_terminal(mid_tile) || is_winds(mid_tile)) {
        return PUNG_OF_TERMINALS_OR_HONORS;
    }
    // None of the above
    return FAN_NONE;
}

// When a 3-chow fan exists, remaining 4th chow scores at most 1 fan
static fan_t get_1_chow_extra_fan(tile_t tile0, tile_t tile1, tile_t tile2, tile_t tile_extra) {
    fan_t fan0 = get_2_chows_fan_unordered(tile0, tile_extra);
    fan_t fan1 = get_2_chows_fan_unordered(tile1, tile_extra);
    fan_t fan2 = get_2_chows_fan_unordered(tile2, tile_extra);

    // Return in the following order
    // Pure Double Chow
    if (fan0 == PURE_DOUBLE_CHOW || fan1 == PURE_DOUBLE_CHOW || fan2 == PURE_DOUBLE_CHOW) {
        return PURE_DOUBLE_CHOW;
    }
    // Mixed Double Chow
    if (fan0 == MIXED_DOUBLE_CHOW || fan1 == MIXED_DOUBLE_CHOW || fan2 == MIXED_DOUBLE_CHOW) {
        return MIXED_DOUBLE_CHOW;
    }
    // Short Straight
    if (fan0 == SHORT_STRAIGHT || fan1 == SHORT_STRAIGHT || fan2 == SHORT_STRAIGHT) {
        return SHORT_STRAIGHT;
    }
    // Two Terminal Chows
    if (fan0 == TWO_TERMINAL_CHOWS || fan1 == TWO_TERMINAL_CHOWS || fan2 == TWO_TERMINAL_CHOWS) {
        return TWO_TERMINAL_CHOWS;
    }

    return FAN_NONE;
}

// Combine-Once Principle:
// If an uncombined set of tiles can combine with an already combined set, it may only combine once.
//
// Principle of Non-Repetition:
// Tiles that have already formed a specific fan pattern cannot be combined with other sets to form the same fan pattern again for scoring.
//
// According to Combine-Once Principle, 234567s 234567p can only be scored as "Mixed Double Chow * 2, Short Straight * 1" or "Mixed Double Chow * 1, Short Straight * 2", not "Mixed Double Chow * 2, Short Straight * 2".
// According to the above two principles, 234s 223344567p can only be scored as: "Pure Double Chow, Mixed Double Chow, Short Straight", not "Mixed Double Chow * 2, Short Straight".
//
// Directly implementing the rules as a general graph algorithm is overly complex.
// Simplified approach: first count how many fan instances there are; when exceeding the allowed limit, deduct starting from duplicates.
static void exclusionary_rule(const fan_t *all_fans, long fan_cnt, long max_cnt, fan_table_t &fan_table) {
    // Count how many fan instances
    uint16_t table[4] = { 0 };
    long cnt = 0;
    for (long i = 0; i < fan_cnt; ++i) {
        if (all_fans[i] != FAN_NONE) {
            ++cnt;
            ++table[all_fans[i] - PURE_DOUBLE_CHOW];
        }
    }

    // When exceeding limit, deduct starting from duplicates
    int limit_cnt = 1;
    // First round reduces to 1, second round reduces to 0
    while (cnt > max_cnt && limit_cnt >= 0) {
        int idx = 4;  // Start deducting from Two Terminal Chows
        while (cnt > max_cnt && idx-- > 0) {
            while (static_cast<int>(table[idx]) > limit_cnt && cnt > max_cnt) {
                --table[idx];
                --cnt;
            }
        }
        --limit_cnt;
    }

    fan_table[PURE_DOUBLE_CHOW] = table[0];
    fan_table[MIXED_DOUBLE_CHOW] = table[1];
    fan_table[SHORT_STRAIGHT] = table[2];
    fan_table[TWO_TERMINAL_CHOWS] = table[3];
}

// Fan for 4 chows (3+1) structure
static bool calculate_3_of_4_chows(tile_t tile0, tile_t tile1, tile_t tile2, tile_t tile_extra, fan_table_t &fan_table) {
    fan_t fan;
    if ((fan = get_3_chows_fan(tile0, tile1, tile2)) != FAN_NONE) {
        fan_table[fan] = 1;
        if ((fan = get_1_chow_extra_fan(tile0, tile1, tile2, tile_extra)) != FAN_NONE) {
            fan_table[fan] = 1;
        }
        return true;
    }
    return false;
}

// Fan for 4 chows
static void calculate_4_chows(const tile_t (&mid_tiles)[4], fan_table_t &fan_table) {
    fan_t fan;
    // When a 4-chow fan exists, no need to check others
    if ((fan = get_4_chows_fan(mid_tiles[0], mid_tiles[1], mid_tiles[2], mid_tiles[3])) != FAN_NONE) {
        fan_table[fan] = 1;
        return;
    }

    // Check 3 chows
    if (calculate_3_of_4_chows(mid_tiles[0], mid_tiles[1], mid_tiles[2], mid_tiles[3], fan_table)  // 012 form a 3-chow fan
        || calculate_3_of_4_chows(mid_tiles[0], mid_tiles[1], mid_tiles[3], mid_tiles[2], fan_table)  // 013 form a 3-chow fan
        || calculate_3_of_4_chows(mid_tiles[0], mid_tiles[2], mid_tiles[3], mid_tiles[1], fan_table)  // 023 form a 3-chow fan
        || calculate_3_of_4_chows(mid_tiles[1], mid_tiles[2], mid_tiles[3], mid_tiles[0], fan_table)  // 123 form a 3-chow fan
        ) {
#if DISTINGUISH_PURE_SHIFTED_CHOWS
        // Pure Shifted Chows (wide) + Short Straight may be narrow Pure Shifted Chows
        if (fan_table[PURE_SHIFTED_CHOWS_2] && fan_table[SHORT_STRAIGHT]) {
            if (is_shifted_1(mid_tiles[0], mid_tiles[1], mid_tiles[2])
                || is_shifted_1(mid_tiles[0], mid_tiles[1], mid_tiles[3])
                || is_shifted_1(mid_tiles[0], mid_tiles[2], mid_tiles[3])
                || is_shifted_1(mid_tiles[1], mid_tiles[2], mid_tiles[3])) {
                fan_table[PURE_SHIFTED_CHOWS_2] = 0;
                fan_table[PURE_SHIFTED_CHOWS_1] = 1;
            }
        }
#endif
        return;
    }

    // When no 3-chow fan exists, 4 chows score at most 3 fan
    fan_t all_fans[6] = {
        get_2_chows_fan_unordered(mid_tiles[0], mid_tiles[1]),
        get_2_chows_fan_unordered(mid_tiles[0], mid_tiles[2]),
        get_2_chows_fan_unordered(mid_tiles[0], mid_tiles[3]),
        get_2_chows_fan_unordered(mid_tiles[1], mid_tiles[2]),
        get_2_chows_fan_unordered(mid_tiles[1], mid_tiles[3]),
        get_2_chows_fan_unordered(mid_tiles[2], mid_tiles[3])
    };

    int max_cnt = 3;

    // 0 has no relation with other 3 chows
    if (all_fans[0] == FAN_NONE && all_fans[1] == FAN_NONE && all_fans[2] == FAN_NONE) {
        --max_cnt;
    }

    // 1 has no relation with other 3 chows
    if (all_fans[0] == FAN_NONE && all_fans[3] == FAN_NONE && all_fans[4] == FAN_NONE) {
        --max_cnt;
    }

    // 2 has no relation with other 3 chows
    if (all_fans[1] == FAN_NONE && all_fans[3] == FAN_NONE && all_fans[5] == FAN_NONE) {
        --max_cnt;
    }

    // 3 has no relation with other 3 chows
    if (all_fans[2] == FAN_NONE && all_fans[4] == FAN_NONE && all_fans[5] == FAN_NONE) {
        --max_cnt;
    }

    if (max_cnt > 0) {
        exclusionary_rule(all_fans, 6, max_cnt, fan_table);
    }
}

// Fan for 3 chows
static void calculate_3_chows(const tile_t (&mid_tiles)[3], fan_table_t &fan_table) {
    fan_t fan;

    // When a 3-chow fan exists, no need to check others
    if ((fan = get_3_chows_fan(mid_tiles[0], mid_tiles[1], mid_tiles[2])) != FAN_NONE) {
        fan_table[fan] = 1;
        return;
    }

    // When none of above fan exists, 3 chows score at most 2 fan
    fan_t all_fans[3] = {
        get_2_chows_fan_unordered(mid_tiles[0], mid_tiles[1]),
        get_2_chows_fan_unordered(mid_tiles[0], mid_tiles[2]),
        get_2_chows_fan_unordered(mid_tiles[1], mid_tiles[2])
    };
    exclusionary_rule(all_fans, 3, 2, fan_table);
}

// Fan for 2 chows
static void calculate_2_chows_unordered(const tile_t (&mid_tiles)[2], fan_table_t &fan_table) {
    fan_t fan;
    if ((fan = get_2_chows_fan_unordered(mid_tiles[0], mid_tiles[1])) != FAN_NONE) {
        ++fan_table[fan];
    }
}

// Kongs and concealed pungs scoring
static void calculate_kongs(int concealed_pung_cnt, int melded_kong_cnt, int concealed_kong_cnt, fan_table_t &fan_table) {
    // Rules:
    // Three Kongs
    // Melded Melded Concealed Concealed_pung -> Three Kongs + Two Concealed Pungs + All Pungs
    // Melded Concealed Concealed Melded_pung -> Three Kongs + Two Concealed Pungs + All Pungs
    // Melded Concealed Concealed Concealed_pung -> Three Kongs + Three Concealed Pungs + All Pungs
    // Concealed Concealed Concealed Melded_pung -> Three Kongs + Three Concealed Pungs + All Pungs
    // Concealed Concealed Concealed Concealed_pung -> Three Kongs + Four Concealed Pungs
    //
    // Four Kongs
    // Concealed Melded Melded Melded -> Four Kongs
    // Concealed Concealed Melded Melded -> Four Kongs + Two Concealed Pungs
    // Concealed Concealed Concealed Melded -> Four Kongs + Three Concealed Pungs
    // Concealed Concealed Concealed Concealed -> Four Kongs + Four Concealed Pungs
    //

    int kong_cnt = melded_kong_cnt + concealed_kong_cnt;
    switch (kong_cnt) {
    case 0:  // 0 kongs
        switch (concealed_pung_cnt) {  // Concealed pung count
        case 2: fan_table[TWO_CONCEALED_PUNGS] = 1; break;
        case 3: fan_table[THREE_CONCEALED_PUNGS] = 1; break;
        case 4: fan_table[FOUR_CONCEALED_PUNGS] = 1; break;
        default: break;
        }
        break;
    case 1:  // 1 kong
        if (melded_kong_cnt == 1) {  // Melded kong
            fan_table[MELDED_KONG] = 1;
            switch (concealed_pung_cnt) {  // Concealed pung count
            case 2: fan_table[TWO_CONCEALED_PUNGS] = 1; break;
            case 3: fan_table[THREE_CONCEALED_PUNGS] = 1; break;
            default: break;
            }
        }
        else {  // Concealed kong
            fan_table[CONCEALED_KONG] = 1;
            switch (concealed_pung_cnt) {  // Concealed pung count
            case 1: fan_table[TWO_CONCEALED_PUNGS] = 1; break;
            case 2: fan_table[THREE_CONCEALED_PUNGS] = 1; break;
            case 3: fan_table[FOUR_CONCEALED_PUNGS] = 1; break;
            default: break;
            }
        }
        break;
    case 2:  // 2 kongs
        switch (concealed_kong_cnt) {
        case 0:  // Two Melded Kongs
            fan_table[TWO_MELDED_KONGS] = 1;
            if (concealed_pung_cnt == 2) {  // Concealed pung count
                fan_table[TWO_CONCEALED_PUNGS] = 1;
            }
            break;
        case 1:  // Concealed Kong and Melded Kong
#if SUPPORT_CONCEALED_KONG_AND_MELDED_KONG
            fan_table[CONCEALED_KONG_AND_MELDED_KONG] = 1;
#else
            fan_table[MELDED_KONG] = 1;
            fan_table[CONCEALED_KONG] = 1;
#endif
            switch (concealed_pung_cnt) {  // Concealed pung count
            case 1: fan_table[TWO_CONCEALED_PUNGS] = 1; break;
            case 2: fan_table[THREE_CONCEALED_PUNGS] = 1; break;
            default: break;
            }
            break;
        case 2:  // Two Concealed Kongs
            fan_table[TWO_CONCEALED_KONGS] = 1;
            switch (concealed_pung_cnt) {  // Concealed pung count
            case 1: fan_table[THREE_CONCEALED_PUNGS] = 1; break;
            case 2: fan_table[FOUR_CONCEALED_PUNGS] = 1; break;
            default: break;
            }
            break;
        default:
            break;
        }
        break;
    case 3:  // 3 kongs
        fan_table[THREE_KONGS] = 1;
        switch (concealed_kong_cnt) {  // Concealed kong count
        case 0:  // 3 melded kongs
            break;
        case 1:  // 1 concealed kong, 2 melded kongs
            if (concealed_pung_cnt > 0) {
                fan_table[TWO_CONCEALED_PUNGS] = 1;
            }
            break;
        case 2:  // 2 concealed kongs, 1 melded kong
            if (concealed_pung_cnt == 0) {
                fan_table[TWO_CONCEALED_PUNGS] = 1;
            }
            else {
                fan_table[THREE_CONCEALED_PUNGS] = 1;
            }
            break;
        case 3:  // 3 concealed kongs
            if (concealed_pung_cnt == 0) {
                fan_table[THREE_CONCEALED_PUNGS] = 1;
            }
            else {
                fan_table[FOUR_CONCEALED_PUNGS] = 1;
            }
            break;
        default:
            break;
        }
        break;
    case 4:  // 4 kongs
        fan_table[FOUR_KONGS] = 1;
        switch (concealed_kong_cnt) {
        case 2: fan_table[TWO_CONCEALED_PUNGS] = 1; break;
        case 3: fan_table[THREE_CONCEALED_PUNGS] = 1; break;
        case 4: fan_table[FOUR_CONCEALED_PUNGS] = 1; break;
        default: break;
        }
        break;
    default:
        UNREACHABLE();
        break;
    }
}

// Fan for 4 pungs
static void calculate_4_pungs(const tile_t (&mid_tiles)[4], fan_table_t &fan_table) {
    fan_t fan;
    // When a 4-pung fan exists, no need to check others
    if ((fan = get_4_pungs_fan(mid_tiles[0], mid_tiles[1], mid_tiles[2], mid_tiles[3])) != FAN_NONE) {
        fan_table[fan] = 1;
        return;
    }

    // Check 3 pungs
    bool has_3_pungs_fan = false;
    int free_pack_idx = -1;  // Unused 1 pung
    // 012 form a 3-pung fan
    if ((fan = get_3_pungs_fan(mid_tiles[0], mid_tiles[1], mid_tiles[2])) != FAN_NONE) {
        fan_table[fan] = 1;
        free_pack_idx = 3;
        has_3_pungs_fan = true;
    }
    // 013 form a 3-pung fan
    else if ((fan = get_3_pungs_fan(mid_tiles[0], mid_tiles[1], mid_tiles[3])) != FAN_NONE) {
        fan_table[fan] = 1;
        free_pack_idx = 2;
        has_3_pungs_fan = true;
    }
    // 023 form a 3-pung fan
    else if ((fan = get_3_pungs_fan(mid_tiles[0], mid_tiles[2], mid_tiles[3])) != FAN_NONE) {
        fan_table[fan] = 1;
        free_pack_idx = 1;
        has_3_pungs_fan = true;
    }
    // 123 form a 3-pung fan
    else if ((fan = get_3_pungs_fan(mid_tiles[1], mid_tiles[2], mid_tiles[3])) != FAN_NONE) {
        fan_table[fan] = 1;
        free_pack_idx = 0;
        has_3_pungs_fan = true;
    }

    // When a 3-pung fan exists, the remaining 4th pung can only combine once
    if (has_3_pungs_fan) {
        for (int i = 0; i < 4; ++i) {
            if (i == free_pack_idx) {
                continue;
            }
            // Test fan types against unused pung in turn
            if ((fan = get_2_pungs_fan_unordered(mid_tiles[i], mid_tiles[free_pack_idx])) != FAN_NONE) {
                ++fan_table[fan];
                break;
            }
        }
        return;
    }

    // When no 3-pung fan exists, calculate pairwise
    if ((fan = get_2_pungs_fan_unordered(mid_tiles[0], mid_tiles[1])) != FAN_NONE) {
        ++fan_table[fan];
    }
    if ((fan = get_2_pungs_fan_unordered(mid_tiles[0], mid_tiles[2])) != FAN_NONE) {
        ++fan_table[fan];
    }
    if ((fan = get_2_pungs_fan_unordered(mid_tiles[0], mid_tiles[3])) != FAN_NONE) {
        ++fan_table[fan];
    }
    if ((fan = get_2_pungs_fan_unordered(mid_tiles[1], mid_tiles[2])) != FAN_NONE) {
        ++fan_table[fan];
    }
    if ((fan = get_2_pungs_fan_unordered(mid_tiles[1], mid_tiles[3])) != FAN_NONE) {
        ++fan_table[fan];
    }
    if ((fan = get_2_pungs_fan_unordered(mid_tiles[2], mid_tiles[3])) != FAN_NONE) {
        ++fan_table[fan];
    }
}

// Fan for 3 pungs
static void calculate_3_pungs(const tile_t (&mid_tiles)[3], fan_table_t &fan_table) {
    fan_t fan;

    // When a 3-pung fan exists (Shifted Pungs, Triple Pung, Big Three Winds, Big Three Dragons), no need to check others
    if ((fan = get_3_pungs_fan(mid_tiles[0], mid_tiles[1], mid_tiles[2])) != FAN_NONE) {
        fan_table[fan] = 1;
        return;
    }

    // When no 3-pung fan exists, calculate pairwise
    if ((fan = get_2_pungs_fan_unordered(mid_tiles[0], mid_tiles[1])) != FAN_NONE) {
        ++fan_table[fan];
    }
    if ((fan = get_2_pungs_fan_unordered(mid_tiles[0], mid_tiles[2])) != FAN_NONE) {
        ++fan_table[fan];
    }
    if ((fan = get_2_pungs_fan_unordered(mid_tiles[1], mid_tiles[2])) != FAN_NONE) {
        ++fan_table[fan];
    }
}

// Fan for 2 pungs
static void calculate_2_pungs_unordered(const tile_t (&mid_tiles)[2], fan_table_t &fan_table) {
    fan_t fan = get_2_pungs_fan_unordered(mid_tiles[0], mid_tiles[1]);
    if (fan != FAN_NONE) {
        ++fan_table[fan];
    }
}

// Pure Terminal Chows
static bool is_pure_terminal_chows(const pack_t (&chow_packs)[4], pack_t pair_pack) {
    if (tile_get_rank(pack_get_tile(pair_pack)) != 5) {  // 5 as pair (eye)
        return false;
    }

    int cnt_123 = 0, cnt_789 = 0;
    suit_t pair_suit = tile_get_suit(pack_get_tile(pair_pack));
    for (int i = 0; i < 4; ++i) {
        suit_t suit = tile_get_suit(pack_get_tile(chow_packs[i]));
        if (suit != pair_suit) {  // Same suit as pair
            return false;
        }
        rank_t rank = tile_get_rank(pack_get_tile(chow_packs[i]));
        switch (rank) {
        case 2: ++cnt_123; break;
        case 8: ++cnt_789; break;
        default: return false;
        }
    }
    return (cnt_123 == 2 && cnt_789 == 2);  // Two sets each of 123 and 789
}

// Three-Suited Terminal Chows
static bool is_three_suited_terminal_chows(const pack_t (&chow_packs)[4], pack_t pair_pack) {
    if (tile_get_rank(pack_get_tile(pair_pack)) != 5) {  // 5 as pair (eye)
        return false;
    }

    int suit_table_123[4] = { 0 };
    int suit_table_789[4] = { 0 };
    suit_t pair_suit = tile_get_suit(pack_get_tile(pair_pack));
    for (int i = 0; i < 4; ++i) {
        suit_t suit = tile_get_suit(pack_get_tile(chow_packs[i]));
        if (suit == pair_suit) {  // Different suit from pair
            return false;
        }
        rank_t rank = tile_get_rank(pack_get_tile(chow_packs[i]));
        switch (rank) {
        case 2: ++suit_table_123[suit]; break;
        case 8: ++suit_table_789[suit]; break;
        default: return false;
        }
    }

    // In the two suits different from pair suit, one 123 and one 789 each
    switch (pair_suit) {
    case 1: return (suit_table_123[2] && suit_table_123[3] && suit_table_789[2] && suit_table_789[3]);
    case 2: return (suit_table_123[1] && suit_table_123[3] && suit_table_789[1] && suit_table_789[3]);
    case 3: return (suit_table_123[1] && suit_table_123[2] && suit_table_789[1] && suit_table_789[2]);
    default: return false;
    }
}

// Check Seven Pairs waiting
static void check_seven_pairs_waiting(const tile_table_t &standing_table, tile_table_t &waiting_table) {
    int pairs = 0;
    const auto &all_tiles = standard_tiles<>::all;
    for (int i = 0; i < 34; ++i) {
        uint16_t cnt = standing_table[all_tiles[i]];
        // Paired tiles
        if (cnt == 2 || cnt == 3) ++pairs;
        else if (cnt == 4) pairs += 2;
    }

    if (pairs == 6) {
        for (int i = 0; i < 34; ++i) {
            tile_t t = all_tiles[i];
            uint16_t cnt = standing_table[t];
            if (cnt == 1 || cnt == 3) {
                waiting_table[t] = 1;  // Wait on this tile
                break;
            }
        }
    }
}

// Get proto-run waiting
static void get_regular_pack_waiting(const tile_table_t &tile_table, tile_table_t &waiting_table) {
    const auto &all_tiles = standard_tiles<>::all;
    for (int i = 0; i < 34; ++i) {
        tile_t t = all_tiles[i];
        if (tile_table[t] < 1) {
            continue;
        }
        if (tile_table[t] > 1) {
            waiting_table[t] = 1;  // Dual wait
            return;
        }
        if (is_numbered_suit_quick(t)) {  // Numbered suit proto-run
            rank_t r = tile_get_rank(t);
            if (r > 1 && tile_table[t - 1]) {  // Two-sided or edge wait
                if (r < 9) waiting_table[t + 1] = 1;
                if (r > 2) waiting_table[t - 2] = 1;
                return;
            }
            if (r > 2 && tile_table[t - 2]) {  // Closed wait
                waiting_table[t - 1] = 1;
                return;
            }
        }
    }
}

// Check regular winning form waiting
static void check_regular_waiting(tile_table_t &tile_table, intptr_t tile_cnt, eigen_t prev_eigen, tile_table_t &waiting_table) {
    const auto &all_tiles = standard_tiles<>::all;
    // 1 tile must wait on pair (eye)
    if (tile_cnt == 1) {
        for (int i = 0; i < 34; ++i) {
            tile_t t = all_tiles[i];
            uint16_t cnt = tile_table[t];
            if (cnt == 1) {
                waiting_table[t] = 1;
                break;
            }
        }
        return;
    }

    // 4 tiles could have a pair waiting on proto-run
    if (tile_cnt == 4) {
        // Deduct pair
        for (int i = 0; i < 34; ++i) {
            tile_t t = all_tiles[i];
            if (tile_table[t] < 2) {
                continue;
            }

            // Deduct pair, get proto-run waiting tiles
            tile_table[t] -= 2;
            get_regular_pack_waiting(tile_table, waiting_table);
            // Restore
            tile_table[t] += 2;
        }
    }

    // Recursively reduce sets
    for (int i = 0; i < 34; ++i) {
        tile_t t = all_tiles[i];
        if (tile_table[t] < 1) {
            continue;
        }

        // Pung
        if (tile_table[t] > 2) {
            // If current pung eigenvalue is less than previous, this path has already been visited
            eigen_t eigen = make_eigen(t, t, t);
            if (eigen > prev_eigen) {
                // Deduct this pung, recurse
                tile_table[t] -= 3;
                check_regular_waiting(tile_table, tile_cnt - 3, eigen, waiting_table);
                // Restore
                tile_table[t] += 3;
            }
        }

        // Chow (numbered suits only)
        if (is_numbered_suit(t)) {
            // Chow t, t+1, t+2; clearly t cannot be a numbered tile with rank 8 or higher
            if (tile_get_rank(t) < 8 && tile_table[t + 1] && tile_table[t + 2]) {
                // If current chow eigenvalue is less than previous, this path has already been visited
                eigen_t eigen = make_eigen(t, t + 1, t + 2);
                if (eigen >= prev_eigen) {
                    // Deduct this chow, recurse
                    --tile_table[t];
                    --tile_table[t + 1];
                    --tile_table[t + 2];
                    check_regular_waiting(tile_table, tile_cnt - 3, eigen, waiting_table);
                    // Restore
                    ++tile_table[t];
                    ++tile_table[t + 1];
                    ++tile_table[t + 2];
                }
            }
        }
    }
}

// Whether unique wait (single/closed/edge wait)
static bool is_unique_waiting(tile_table_t &standing_table, intptr_t tile_cnt, tile_t win_tile) {
    tile_table_t waiting_table{};

    // Deduct winning tile
    --standing_table[win_tile];

    // In case of 13 tiles, check Seven Pairs
    if (tile_cnt == 13) {
        check_seven_pairs_waiting(standing_table, waiting_table);
    }

    // Regular winning form
    check_regular_waiting(standing_table, tile_cnt, 0, waiting_table);

    // Restore winning tile
    ++standing_table[win_tile];

    bool waiting = false;
    const auto &all_tiles = standard_tiles<>::all;
    for (int i = 0; i < 34; ++i) {
        if (waiting_table[all_tiles[i]]) {
            if (waiting) {
                // If another wait already exists, it is not a unique wait
                return false;
            }
            waiting = true;
        }
    }
    return waiting;
}

// Adjust by win method: involves Fully Concealed Hand, Melded Hand
static void adjust_by_self_drawn(const pack_t (&packs)[5], intptr_t fixed_cnt, bool self_drawn, fan_table_t &fan_table) {
    std::ptrdiff_t melded_cnt = std::count_if(&packs[0], &packs[fixed_cnt], &is_pack_melded);  // Count of melded packs

    switch (melded_cnt) {
    case 0:  // 0 melded sets: self-drawn is Fully Concealed Hand, discard is Concealed Hand
        fan_table[self_drawn ? FULLY_CONCEALED_HAND : CONCEALED_HAND] = 1;
        break;
    case 4:
        // 4 melded sets: self-drawn is still self-drawn, discard is Melded Hand
        fan_table[self_drawn ? SELF_DRAWN : MELDED_HAND] = 1;
        break;
    default:
        if (self_drawn) {
            fan_table[SELF_DRAWN] = 1;
        }
        break;
    }
}

// Adjust by eye/head: involves All Chows, Little Three Dragons, Little Four Winds
static void adjust_by_pair_tile(tile_t pair_tile, intptr_t chow_cnt, fan_table_t &fan_table) {
    if (chow_cnt == 4) {  // All 4 sets are chows
        if (is_numbered_suit_quick(pair_tile)) {  // Numbered suit pair (eye)
            fan_table[ALL_CHOWS] = 1;  // All Chows
        }
        return;
    }

    // On basis of Two Dragons Pungs, if pair is dragons, adjust to Little Three Dragons
    if (fan_table[TWO_DRAGONS_PUNGS]) {
        if (is_dragons(pair_tile)) {
            fan_table[LITTLE_THREE_DRAGONS] = 1;
            fan_table[TWO_DRAGONS_PUNGS] = 0;
        }
        return;
    }
    // On basis of Big Three Winds, if pair is winds, adjust to Little Four Winds
    if (fan_table[BIG_THREE_WINDS]) {
        if (is_winds(pair_tile)) {
            fan_table[LITTLE_FOUR_WINDS] = 1;
            fan_table[BIG_THREE_WINDS] = 0;
        }
        return;
    }
}

// Adjust by suits: involves No Honors, One Voided Suit, Half Flush, Full Flush, All Types
static void adjust_by_suits(const tile_t *tiles, intptr_t tile_cnt, fan_table_t &fan_table) {
    // Map suits using bit operations
    uint8_t suit_flag = 0;
    for (intptr_t i = 0; i < tile_cnt; ++i) {
        suit_flag |= (1 << tile_get_suit(tiles[i]));
    }

    // 1111 0001
    if (!(suit_flag & 0xF1U)) {
        fan_table[NO_HONORS] = 1;  // No Honors
    }

    // 1110 0011
    if (!(suit_flag & 0xE3U)) {
        ++fan_table[ONE_VOIDED_SUIT];  // One Voided Suit (Characters)
    }
    // 1110 0101
    if (!(suit_flag & 0xE5U)) {
        ++fan_table[ONE_VOIDED_SUIT];  // One Voided Suit (Bamboo)
    }
    // 1110 1001
    if (!(suit_flag & 0xE9U)) {
        ++fan_table[ONE_VOIDED_SUIT];  // One Voided Suit (Dots)
    }

    // When voiding 2 suits, adjust to Half Flush or Full Flush depending on honors
    if (fan_table[ONE_VOIDED_SUIT] == 2) {
        fan_table[ONE_VOIDED_SUIT] = 0;
        if (fan_table[NO_HONORS] == 0) {
            fan_table[HALF_FLUSH] = 1;
        }
        else {
            fan_table[FULL_FLUSH] = 1;
            fan_table[NO_HONORS] = 0;
        }
    }

    // 0001 1110
    if (suit_flag == 0x1EU) {  // All three numbered suits and honors present
        if (std::any_of(tiles, tiles + tile_cnt, &is_winds)
            && std::any_of(tiles, tiles + tile_cnt, &is_dragons)) {
            fan_table[ALL_TYPES] = 1;  // All Types
        }
    }
}

// Adjust by numbered tile rank ranges: involves Upper Four, Lower Four, All Upper, All Five, All Lower
static void adjust_by_rank_range(const tile_t *tiles, intptr_t tile_cnt, fan_table_t &fan_table) {
#ifdef STRICT_98_RULE
    if (fan_table[SEVEN_PAIRS]) {
        return;  // Strict 98 rules: Seven Pairs does not stack with these
    }
#endif

    // Map which ranks are present
    uint16_t rank_flag = 0;
    for (intptr_t i = 0; i < tile_cnt; ++i) {
        if (!is_numbered_suit_quick(tiles[i])) {
            return;
        }
        rank_flag |= (1 << tile_get_rank(tiles[i]));
    }

    // 1111 1111 1110 0001
    // Check if only 1, 2, 3, 4 are present
    if (!(rank_flag & 0xFFE1)) {
        // Containing 4 is Lower Four, otherwise All Lower
        fan_table[rank_flag & 0x0010 ? LOWER_FOUR : LOWER_TILES] = 1;
        return;
    }
    // 1111 1100 0011 1111
    // Check if only 6, 7, 8, 9 are present
    if (!(rank_flag & 0xFC3F)) {
        // Containing 6 is Upper Four, otherwise All Upper
        fan_table[rank_flag & 0x0040 ? UPPER_FOUR : UPPER_TILES] = 1;
        return;
    }
    // 1111 1111 1000 1111
    // Check if only 4, 5, 6 are present
    if (!(rank_flag & 0xFF8F)) {
        // All Five
        fan_table[MIDDLE_TILES] = 1;
    }
}

// Adjust by pack characteristics: involves Outside Hand, All With Five, All Even Pungs
static void adjust_by_packs_traits(const pack_t (&packs)[5], fan_table_t &fan_table) {
    // Count packs containing terminal numbered tiles 1/9, honors, 5s, even-numbered tiles
    int terminal_pack = 0;
    int honor_pack = 0;
    int five_pack = 0;
    int even_pack = 0;
    for (int i = 0; i < 5; ++i) {
        tile_t tile = pack_get_tile(packs[i]);
        if (is_numbered_suit_quick(tile)) {  // Numbered tile
            rank_t rank = tile_get_rank(tile);
            if (pack_get_type(packs[i]) == PACK_TYPE_CHOW) {  // Chow
                switch (rank) {
                case 2: case 8: ++terminal_pack; break;  // Numbered terminal 1/9
                case 4: case 5: case 6: ++five_pack; break;  // Contains 5
                default: break;
                }
            }
            else {  // Pung or pair
                switch (rank) {
                case 1: case 9: ++terminal_pack; break;  // Numbered terminal 1/9
                case 5: ++five_pack; break;  // Contains 5
                case 2: case 4: case 6: case 8: ++even_pack; break;  // Even pung
                default: break;
                }
            }
        }
        else {
            ++honor_pack;  // Honor tile
        }
    }

    // All 5 packs contain terminal 1/9 and honor tiles; tentatively score Outside Hand
    if (terminal_pack + honor_pack == 5) {
        fan_table[OUTSIDE_HAND] = 1;
        return;
    }
    // All With Five
    if (five_pack == 5) {
        fan_table[ALL_FIVE] = 1;
        return;
    }
    // All Even Pungs
    if (even_pack == 5) {
        fan_table[ALL_EVEN_PUNGS] = 1;
    }
}

// Adjust by tile characteristics: involves All Simples, Reversible Tiles, All Green, All Honors, All Terminals, All Terminals and Honors
static void adjust_by_tiles_traits(const tile_t *tiles, intptr_t tile_cnt, fan_table_t &fan_table) {
    // All Simples
    if (std::none_of(tiles, tiles + tile_cnt, &is_terminal_or_honor)) {
        fan_table[ALL_SIMPLES] = 1;
    }

    // Reversible Tiles
    if (std::all_of(tiles, tiles + tile_cnt, &is_reversible)) {
        fan_table[REVERSIBLE_TILES] = 1;
    }

#ifdef STRICT_98_RULE
    if (fan_table[SEVEN_PAIRS]) {
        return;  // Strict 98 rules: Seven Pairs does not support All Green, All Honors, All Terminals, All Terminals and Honors
    }
#endif

    // All Green
    if (std::all_of(tiles, tiles + tile_cnt, &is_green)) {
        fan_table[ALL_GREEN] = 1;
    }

    // If All Simples, no need to check All Honors, All Terminals, All Terminals and Honors
    if (fan_table[ALL_SIMPLES] != 0) {
        return;
    }

    // All Honors
    if (std::all_of(tiles, tiles + tile_cnt, &is_honor)) {
        fan_table[ALL_HONORS] = 1;
        return;
    }
    // All Terminals
    if (std::all_of(tiles, tiles + tile_cnt, &is_terminal)) {
        fan_table[ALL_TERMINALS] = 1;
        return;
    }
    // All Terminals and Honors
    if (std::all_of(tiles, tiles + tile_cnt, &is_terminal_or_honor)) {
        fan_table[ALL_TERMINALS_AND_HONORS] = 1;
    }
}

// Tile Hog adjustment
static void adjust_by_tiles_hog(const tile_table_t &tile_table, intptr_t kong_cnt, fan_table_t &fan_table) {
    // Count how many tile types have used 4 tiles minus kongs count = Tile Hog count
    intptr_t cnt = std::count(std::begin(tile_table), std::end(tile_table), 4);
    fan_table[TILE_HOG] = static_cast<uint8_t>(cnt - kong_cnt);
}

// Adjust by wait type: involves Edge Wait, Closed Wait, Single Wait
static void adjust_by_waiting_form(const pack_t *concealed_packs, intptr_t pack_cnt, tile_t win_tile, fan_table_t &fan_table) {
    // Melded Hand and Four Kongs do not score Single Wait, and cannot have Edge Wait or Closed Wait
    if (fan_table[MELDED_HAND] || fan_table[FOUR_KONGS]) {
        return;
    }

    // In case of waiting on 1 tile, check the position of the winning tile
    // Edge Wait: 0x01, Closed Wait: 0x02, Single Wait: 0x04
    uint8_t pos_flag = 0;

    for (intptr_t i = 0; i < pack_cnt; ++i) {
        switch (pack_get_type(concealed_packs[i])) {
        case PACK_TYPE_CHOW: {
            tile_t mid_tile = pack_get_tile(concealed_packs[i]);
            if (mid_tile == win_tile) {
                pos_flag |= 0x02U;  // Closed Wait
            }
            else if (mid_tile + 1 == win_tile || mid_tile - 1 == win_tile) {
                pos_flag |= 0x01U;  // Edge Wait
            }
            break;
        }
        case PACK_TYPE_PAIR: {
            tile_t mid_tile = pack_get_tile(concealed_packs[i]);
            if (mid_tile == win_tile) {
                pos_flag |= 0x04U;  // Single Wait
            }
            break;
        }
        default:
            break;
        }
    }

    // When multiple possibilities exist, only one may be scored
    if (pos_flag & 0x01U) {
        fan_table[EDGE_WAIT] = 1;
    }
    else if (pos_flag & 0x02U) {
        fan_table[CLOSED_WAIT] = 1;
    }
    else if (pos_flag & 0x04U) {
        fan_table[SINGLE_WAIT] = 1;
    }
}

// Adjust rule exclusions
static void final_adjust(fan_table_t &fan_table) {
    // Big Four Winds does not score Big Three Winds, All Pungs, Prevalent Wind, Seat Wind, Pung of Terminals or Honors
    if (fan_table[BIG_FOUR_WINDS]) {
        fan_table[ALL_PUNGS] = 0;
        fan_table[PUNG_OF_TERMINALS_OR_HONORS] = 0;
    }
    // Big Three Dragons does not score Two Dragons Pungs, Dragon Pung (strict 98 rules: does not score One Voided Suit)
    if (fan_table[BIG_THREE_DRAGONS]) {
        fan_table[DRAGON_PUNG] = 0;
#ifdef STRICT_98_RULE
        fan_table[ONE_VOIDED_SUIT] = 0;
#endif
    }
    // All Green does not score Half Flush, One Voided Suit
    if (fan_table[ALL_GREEN]) {
        fan_table[HALF_FLUSH] = 0;
        fan_table[ONE_VOIDED_SUIT] = 0;
    }
    // Four Kongs does not score Single Wait
    if (fan_table[FOUR_KONGS]) {
        fan_table[SINGLE_WAIT] = 0;
    }

    // All Terminals does not score All Terminals and Honors, All Pungs, Outside Hand, Pung of Terminals or Honors, No Honors (strict 98 rules: does not score Double Pung, Triple Pung)
    if (fan_table[ALL_TERMINALS]) {
        fan_table[ALL_PUNGS] = 0;
        fan_table[OUTSIDE_HAND] = 0;
        fan_table[PUNG_OF_TERMINALS_OR_HONORS] = 0;
        fan_table[NO_HONORS] = 0;
        fan_table[DOUBLE_PUNG] = 0;  // Common practice does not score Double Pung
#ifdef STRICT_98_RULE
        fan_table[TRIPLE_PUNG] = 0;
#endif
    }

    // Little Four Winds does not score Big Three Winds
    if (fan_table[LITTLE_FOUR_WINDS]) {
        // In Little Four Winds, if the 4th set is a 1/9 pung, it is All Terminals and Honors; if dragon pung, it is All Honors; neither scores Pung of Terminals or Honors
        // If chow or 2-8 pung, no extra Pung of Terminals or Honors exists
        // Therefore set Pung of Terminals or Honors to 0 here
        fan_table[PUNG_OF_TERMINALS_OR_HONORS] = 0;
    }

    // Little Three Dragons does not score Two Dragons Pungs, Dragon Pung (strict 98 rules: does not score One Voided Suit)
    if (fan_table[LITTLE_THREE_DRAGONS]) {
        fan_table[DRAGON_PUNG] = 0;
#ifdef STRICT_98_RULE
        fan_table[ONE_VOIDED_SUIT] = 0;
#endif
    }

    // All Honors does not score All Terminals and Honors, All Pungs, Outside Hand, Pung of Terminals or Honors, One Voided Suit
    if (fan_table[ALL_HONORS]) {
        fan_table[ALL_PUNGS] = 0;
        fan_table[OUTSIDE_HAND] = 0;
        fan_table[PUNG_OF_TERMINALS_OR_HONORS] = 0;
        fan_table[ONE_VOIDED_SUIT] = 0;
    }
    // Four Concealed Pungs does not score All Pungs, Concealed Hand; convert Fully Concealed Hand to Self-Drawn
    if (fan_table[FOUR_CONCEALED_PUNGS]) {
        fan_table[ALL_PUNGS] = 0;
        fan_table[CONCEALED_HAND] = 0;
        if (fan_table[FULLY_CONCEALED_HAND]) {
            fan_table[FULLY_CONCEALED_HAND] = 0;
            fan_table[SELF_DRAWN] = 1;
        }
    }
    // Pure Terminal Chows does not score Seven Pairs, Full Flush, All Chows, Pure Double Chow, Two Terminal Chows, One Voided Suit, No Honors
    if (fan_table[PURE_TERMINAL_CHOWS]) {
        fan_table[FULL_FLUSH] = 0;
        fan_table[ALL_CHOWS] = 0;
        fan_table[NO_HONORS] = 0;
    }
#ifdef STRICT_98_RULE
    // Quadruple Chow does not score Pure Triple Chow, Pure Double Chow, Tile Hog (strict 98 rules: does not score One Voided Suit)
    if (fan_table[QUADRUPLE_CHOW]) {
        fan_table[ONE_VOIDED_SUIT] = 0;
    }
#endif
    // Four Pure Shifted Pungs does not score Pure Shifted Pungs, All Pungs (strict 98 rules: does not score One Voided Suit)
    if (fan_table[FOUR_PURE_SHIFTED_PUNGS]) {
        fan_table[ALL_PUNGS] = 0;
#ifdef STRICT_98_RULE
        fan_table[ONE_VOIDED_SUIT] = 0;
#endif
    }

#ifdef STRICT_98_RULE
    // Four Pure Shifted Chows does not score Pure Shifted Chows, Two Terminal Chows, Short Straight (strict 98 rules: does not score One Voided Suit)
    if (fan_table[FOUR_PURE_SHIFTED_CHOWS]) {
        fan_table[ONE_VOIDED_SUIT] = 0;
    }
#endif

    // All Terminals and Honors does not score All Pungs, Outside Hand, Pung of Terminals or Honors
    if (fan_table[ALL_TERMINALS_AND_HONORS]) {
        fan_table[ALL_PUNGS] = 0;
        fan_table[OUTSIDE_HAND] = 0;
        fan_table[PUNG_OF_TERMINALS_OR_HONORS] = 0;
    }

    // All Even Pungs does not score All Pungs, All Simples, No Honors
    if (fan_table[ALL_EVEN_PUNGS]) {
        fan_table[ALL_PUNGS] = 0;
        fan_table[ALL_SIMPLES] = 0;
        fan_table[NO_HONORS] = 0;
    }
    // All Upper does not score Upper Four, No Honors
    if (fan_table[UPPER_TILES]) {
        fan_table[NO_HONORS] = 0;
    }
    // All Five does not score All Simples
    if (fan_table[MIDDLE_TILES]) {
        fan_table[ALL_SIMPLES] = 0;
        fan_table[NO_HONORS] = 0;
    }
    // All Lower does not score Lower Four, No Honors
    if (fan_table[LOWER_TILES]) {
        fan_table[NO_HONORS] = 0;
    }

    // Three-Suited Terminal Chows does not score All Chows, No Honors, Mixed Double Chow, Two Terminal Chows
    if (fan_table[THREE_SUITED_TERMINAL_CHOWS]) {
        fan_table[ALL_CHOWS] = 0;
        fan_table[NO_HONORS] = 0;
    }
    // All With Five does not score All Simples, No Honors
    if (fan_table[ALL_FIVE]) {
        fan_table[ALL_SIMPLES] = 0;
        fan_table[NO_HONORS] = 0;
    }

    // Upper Four does not score No Honors
    if (fan_table[UPPER_FOUR]) {
        fan_table[NO_HONORS] = 0;
    }
    // Lower Four does not score No Honors
    if (fan_table[LOWER_FOUR]) {
        fan_table[NO_HONORS] = 0;
    }
    // Big Three Winds does not score internal Pung of Terminals or Honors (strict 98 rules: does not score One Voided Suit)
    if (fan_table[BIG_THREE_WINDS]) {
        // If not All Honors or All Terminals and Honors, deduct 3 Pungs of Terminals or Honors
        if (!fan_table[ALL_HONORS] && !fan_table[ALL_TERMINALS_AND_HONORS]) {
            assert(fan_table[PUNG_OF_TERMINALS_OR_HONORS] >= 3);
            fan_table[PUNG_OF_TERMINALS_OR_HONORS] -= 3;
        }
#ifdef STRICT_98_RULE
        fan_table[ONE_VOIDED_SUIT] = 0;
#endif
    }

    // Reversible Tiles does not score One Voided Suit
    if (fan_table[REVERSIBLE_TILES]) {
        fan_table[ONE_VOIDED_SUIT] = 0;
    }
    // Last Tile Draw does not score Self-Drawn
    if (fan_table[LAST_TILE_DRAW]) {
        fan_table[SELF_DRAWN] = 0;
    }
    // Out with Replacement Tile does not score Self-Drawn
    if (fan_table[OUT_WITH_REPLACEMENT_TILE]) {
        fan_table[SELF_DRAWN] = 0;
    }

    // Melded Hand does not score Single Wait
    if (fan_table[MELDED_HAND]) {
        fan_table[SINGLE_WAIT] = 0;
    }
    // Two Dragons Pungs does not score Dragon Pung
    if (fan_table[TWO_DRAGONS_PUNGS]) {
        fan_table[DRAGON_PUNG] = 0;
    }

    // Fully Concealed Hand does not score Self-Drawn
    if (fan_table[FULLY_CONCEALED_HAND]) {
        fan_table[SELF_DRAWN] = 0;
    }

    // All Chows does not score No Honors
    if (fan_table[ALL_CHOWS]) {
        fan_table[NO_HONORS] = 0;
    }
    // All Simples does not score No Honors
    if (fan_table[ALL_SIMPLES]) {
        fan_table[NO_HONORS] = 0;
    }
}


// Adjust Prevalent Wind and Seat Wind
static void adjust_by_winds(tile_t tile, wind_t prevalent_wind, wind_t seat_wind, fan_table_t &fan_table) {
    // Big Three Winds, All Terminals and Honors, All Honors, Little Four Winds: these have already deducted Pung of Terminals or Honors
    bool is_deducted = (fan_table[BIG_THREE_WINDS] || fan_table[ALL_TERMINALS_AND_HONORS] || fan_table[ALL_HONORS] || fan_table[LITTLE_FOUR_WINDS]);

    rank_t delta = tile - TILE_E;
    if (delta == static_cast<int>(prevalent_wind) - static_cast<int>(wind_t::EAST)) {
        fan_table[PREVALENT_WIND] = 1;
        if (!is_deducted) {
            --fan_table[PUNG_OF_TERMINALS_OR_HONORS];
        }
    }
    if (delta == static_cast<int>(seat_wind) - static_cast<int>(wind_t::EAST)) {
        fan_table[SEAT_WIND] = 1;
        // When Prevalent Wind and Seat Wind are the same, Pung of Terminals or Honors has already been deducted
        if (seat_wind != prevalent_wind && !is_deducted) {
            --fan_table[PUNG_OF_TERMINALS_OR_HONORS];
        }
    }
}

// Adjust by win flag: involves Last Tile, Last Tile Draw, Last Tile Claim, Self-Drawn
static void adjust_by_win_flag(win_flag_t win_flag, fan_table_t &fan_table) {
    if (win_flag & WIN_FLAG_LAST_TILE) {
        fan_table[LAST_TILE] = 1;
    }
    if (win_flag & WIN_FLAG_SELF_DRAWN) {
        fan_table[SELF_DRAWN] = 1;
        if (win_flag & WIN_FLAG_WALL_LAST) {
            fan_table[LAST_TILE_DRAW] = 1;
            fan_table[SELF_DRAWN] = 0;
        }
        if (win_flag & WIN_FLAG_KONG_INVOLVED) {
            fan_table[OUT_WITH_REPLACEMENT_TILE] = 1;
            fan_table[SELF_DRAWN] = 0;
        }
    }
    else {
        if (win_flag & WIN_FLAG_WALL_LAST) {
            fan_table[LAST_TILE_CLAIM] = 1;
        }
        if (win_flag & WIN_FLAG_KONG_INVOLVED) {
            fan_table[ROBBING_THE_KONG] = 1;
            fan_table[LAST_TILE] = 0;
        }
    }
}

#if SUPPORT_BLESSINGS
static void adjust_by_initial_hands(bool east, win_flag_t win_flag, fan_table_t &fan_table) {
    // When self-drawn: dealer is Blessing of Heaven, non-dealer is Blessing of Human II
    if (win_flag & WIN_FLAG_SELF_DRAWN) {
        fan_table[east ? BLESSING_OF_HEAVEN : BLESSING_OF_HUMAN_2] = 1;
        fan_table[FULLY_CONCEALED_HAND] = 0;
        fan_table[SELF_DRAWN] = 0;
    }
    // When discard: dealer discard is Blessing of Earth, non-dealer discard is Blessing of Human I
    // NOTE: Treated uniformly as Blessing of Earth here; external code differentiates
    else {
        // Dealer does not have Blessing of Earth or Blessing of Human
        if (!east) {
            fan_table[BLESSING_OF_EARTH] = 1;
            fan_table[CONCEALED_HAND] = 0;
        }
    }
}
#endif

// Regular winning form fan calculation
static void calculate_regular_fan(const pack_t (&packs)[5], const tile_table_t &fixed_table, const tile_table_t &standing_table,
    const tile_t *unique_tiles, intptr_t unique_cnt,
    const calculate_param_t *calculate_param, bool unique_waiting, win_flag_t win_flag, fan_table_t &fan_table) {

    pack_t pair_pack = 0, chow_packs[4], pung_packs[4];
    int chow_cnt = 0, pung_cnt = 0, concealed_pung_cnt = 0, melded_kong_cnt = 0, concealed_kong_cnt = 0;
    for (int i = 0; i < 5; ++i) {
        pack_t pack = packs[i];
        switch (pack_get_type(pack)) {
        case PACK_TYPE_CHOW:
            chow_packs[chow_cnt++] = pack;
            break;
        case PACK_TYPE_PUNG:
            pung_packs[pung_cnt++] = pack;
            if (!is_pack_melded(pack)) {
                ++concealed_pung_cnt;
            }
            break;
        case PACK_TYPE_KONG:
            pung_packs[pung_cnt++] = pack;
            ++(is_pack_melded(pack) ? melded_kong_cnt : concealed_kong_cnt);
            break;
        case PACK_TYPE_PAIR:
            pair_pack = packs[i];
            break;
        default:
            UNREACHABLE();
            return;
        }
    }

    if (pair_pack == 0 || chow_cnt + pung_cnt != 4) {
        return;
    }

    const tile_t win_tile = calculate_param->win_tile;

    // Adjust by win flag: involves Last Tile, Last Tile Draw, Last Tile Claim, Self-Drawn
    adjust_by_win_flag(win_flag, fan_table);

    // Discard winning tile: if it cannot be interpreted as part of a chow, interpret as pung and mark as melded pung
    if ((win_flag & WIN_FLAG_SELF_DRAWN) == 0) {
        // Winning tile cannot be interpreted as part of a chow
        if (std::none_of(chow_packs, chow_packs + chow_cnt, [win_tile](pack_t chow_pack) {
            tile_t tile = pack_get_tile(chow_pack);
            return !is_pack_melded(chow_pack)
                && (tile - 1 == win_tile || tile == win_tile || tile + 1 == win_tile);
        })) {
            for (intptr_t i = 0; i < pung_cnt; ++i) {
                if (pack_get_tile(pung_packs[i]) == win_tile && !is_pack_melded(pung_packs[i])) {
                    --concealed_pung_cnt;
                }
            }
        }
    }

    if (pung_cnt != 0) { // Pungs present
        calculate_kongs(concealed_pung_cnt, melded_kong_cnt, concealed_kong_cnt, fan_table);

        // Four Kongs and Four Concealed Pungs do not score All Pungs; others add All Pungs fan first
        if (pung_cnt == 4) {
            if (fan_table[FOUR_KONGS] == 0 && fan_table[FOUR_CONCEALED_PUNGS] == 0) {
                fan_table[ALL_PUNGS] = 1;
            }
        }

        // Score each pung individually (Dragon Pung, Pung of Terminals or Honors)
        for (intptr_t i = 0; i < pung_cnt; ++i) {
            fan_t fan = get_1_pung_fan(pack_get_tile(pung_packs[i]));
            if (fan != FAN_NONE) {
                ++fan_table[fan];
            }
        }
    }

    switch (chow_cnt) {
    case 4: {  // 4 chows
        // Check Three-Suited / Pure Terminal Chows
        if (is_three_suited_terminal_chows(chow_packs, pair_pack)) {
            fan_table[THREE_SUITED_TERMINAL_CHOWS] = 1;
            break;
        }
        if (is_pure_terminal_chows(chow_packs, pair_pack)) {
            fan_table[PURE_TERMINAL_CHOWS] = 1;
            break;
        }

        tile_t mid_tiles[4];
        mid_tiles[0] = pack_get_tile(chow_packs[0]);
        mid_tiles[1] = pack_get_tile(chow_packs[1]);
        mid_tiles[2] = pack_get_tile(chow_packs[2]);
        mid_tiles[3] = pack_get_tile(chow_packs[3]);
        std::sort(std::begin(mid_tiles), std::end(mid_tiles));

        calculate_4_chows(mid_tiles, fan_table);
        break;
    }
    case 3: {  // 3 chows + 1 pung
        tile_t mid_tiles[3];
        mid_tiles[0] = pack_get_tile(chow_packs[0]);
        mid_tiles[1] = pack_get_tile(chow_packs[1]);
        mid_tiles[2] = pack_get_tile(chow_packs[2]);
        std::sort(std::begin(mid_tiles), std::end(mid_tiles));

        calculate_3_chows(mid_tiles, fan_table);
        break;
    }
    case 2: {  // 2 chows + 2 pungs
        tile_t mid_tiles_chow[2];
        mid_tiles_chow[0] = pack_get_tile(chow_packs[0]);
        mid_tiles_chow[1] = pack_get_tile(chow_packs[1]);

        tile_t mid_tiles_pung[2];
        mid_tiles_pung[0] = pack_get_tile(pung_packs[0]);
        mid_tiles_pung[1] = pack_get_tile(pung_packs[1]);

        calculate_2_chows_unordered(mid_tiles_chow, fan_table);
        calculate_2_pungs_unordered(mid_tiles_pung, fan_table);
        break;
    }
    case 1: {  // 1 chow + 3 pungs
        tile_t mid_tiles[3];
        mid_tiles[0] = pack_get_tile(pung_packs[0]);
        mid_tiles[1] = pack_get_tile(pung_packs[1]);
        mid_tiles[2] = pack_get_tile(pung_packs[2]);
        std::sort(std::begin(mid_tiles), std::end(mid_tiles));

        calculate_3_pungs(mid_tiles, fan_table);
        break;
    }
    case 0: {  // 4 pungs
        tile_t mid_tiles[4];
        mid_tiles[0] = pack_get_tile(pung_packs[0]);
        mid_tiles[1] = pack_get_tile(pung_packs[1]);
        mid_tiles[2] = pack_get_tile(pung_packs[2]);
        mid_tiles[3] = pack_get_tile(pung_packs[3]);
        std::sort(std::begin(mid_tiles), std::end(mid_tiles));

        calculate_4_pungs(mid_tiles, fan_table);
        break;
    }
    default:
        UNREACHABLE();
        break;
    }

    intptr_t fixed_cnt = calculate_param->hand_tiles.pack_count;

    // Adjust by win method: involves Fully Concealed Hand, Melded Hand
    adjust_by_self_drawn(packs, fixed_cnt, (win_flag & WIN_FLAG_SELF_DRAWN) != 0, fan_table);

#if SUPPORT_BLESSINGS
    // Blessing of Heaven, Blessing of Earth, Blessing of Human
    if (fixed_cnt == 0 && (win_flag & WIN_FLAG_INITIAL)) {
        adjust_by_initial_hands(calculate_param->seat_wind == wind_t::EAST, win_flag, fan_table);
    }
#endif

    // Adjust by eye/head: involves All Chows, Little Three Dragons, Little Four Winds
    adjust_by_pair_tile(pack_get_tile(pair_pack), chow_cnt, fan_table);
    // Adjust by pack characteristics: involves Outside Hand, All With Five, All Even Pungs
    adjust_by_packs_traits(packs, fan_table);

    tile_table_t merged_table{};
    std::transform(std::begin(standing_table), std::end(standing_table), std::begin(fixed_table), std::begin(merged_table),
        [](uint16_t a, uint16_t b)->uint16_t { return a + b; });

    // Adjust by suits: involves No Honors, One Voided Suit, Half Flush, Full Flush, All Types
    adjust_by_suits(unique_tiles, unique_cnt, fan_table);
    // Adjust by tile characteristics: involves All Simples, Reversible Tiles, All Green, All Honors, All Terminals, All Terminals and Honors
    adjust_by_tiles_traits(unique_tiles, unique_cnt, fan_table);
    // Adjust by numbered tile rank ranges: involves Upper Four, Lower Four, All Upper, All Five, All Lower
    adjust_by_rank_range(unique_tiles, unique_cnt, fan_table);
    // Tile Hog adjustment (Quadruple Chow does not score Tile Hog)
    if (fan_table[QUADRUPLE_CHOW] == 0) {
        adjust_by_tiles_hog(merged_table, melded_kong_cnt + concealed_kong_cnt, fan_table);
    }

    if (unique_waiting) {
        // Adjust by wait type: involves Edge Wait, Closed Wait, Single Wait
        adjust_by_waiting_form(packs + fixed_cnt, 5 - fixed_cnt, win_tile, fan_table);
    }

    // Adjust rule exclusions
    final_adjust(fan_table);

    // Adjust Prevalent Wind and Seat Wind (Big Four Winds does not score Prevalent Wind, Seat Wind)
    if (fan_table[BIG_FOUR_WINDS] == 0) {
        wind_t prevalent_wind = calculate_param->prevalent_wind;
        wind_t seat_wind = calculate_param->seat_wind;

        for (intptr_t i = 0; i < pung_cnt; ++i) {
            tile_t tile = pack_get_tile(pung_packs[i]);
            if (is_winds(tile)) {
                adjust_by_winds(tile, prevalent_wind, seat_wind, fan_table);
            }
        }
    }

    // If no fan points at all, score Chicken Hand
    if (std::all_of(std::begin(fan_table), std::end(fan_table), [](uint16_t p) { return p == 0; })) {
        fan_table[CHICKEN_HAND] = 1;
    }
}

// "Knitted Straight + Sets + Pair" winning form fan calculation
static bool calculate_knitted_straight_fan(const tile_table_t &fixed_table, const tile_table_t &standing_table,
    const calculate_param_t *calculate_param, win_flag_t win_flag, fan_table_t &fan_table) {

    const hand_tiles_t *hand_tiles = &calculate_param->hand_tiles;
    const pack_t *fixed_packs = hand_tiles->fixed_packs;
    const tile_t win_tile = calculate_param->win_tile;

    if (!has_pair(standing_table)) {
        return false;
    }

    // Match Knitted Straight
    const auto &standard_knitted_straight = standard_tiles<>::knitted_straight;
    const tile_t (*matched_seq)[9] = std::find_if(&standard_knitted_straight[0], &standard_knitted_straight[6],
        [&standing_table](const tile_t (&seq)[9]) {
        return std::all_of(std::begin(seq), std::end(seq), [&standing_table](tile_t t) { return standing_table[t] > 0; });
    });

    if (matched_seq == &standard_knitted_straight[6]) {
        return false;
    }

    // Deduct Knitted Straight
    tile_table_t tile_table;
    std::memcpy(tile_table, standing_table, sizeof(tile_table));
    std::for_each(std::begin(*matched_seq), std::end(*matched_seq), [&tile_table](tile_t t) { --tile_table[t]; });

    // Decompose by regular winning form
    division_result_t result;
    result.count = 0;
    division_t work_division;
    std::memset(&work_division, 0, sizeof(work_division));

    // Logic here: treat 9 tiles of Knitted Straight as 3 completed sets, leaving indices 0, 1, 2 empty
    // If the 4th set is melded, place it at index 3
    // Then recurse regular winning form starting from fixed_cnt + 3
    // In decomposition result, index 3 is 4th set, index 4 is pair (eye)
    const intptr_t fixed_cnt = hand_tiles->pack_count;
    if (fixed_cnt == 1) {
        work_division.packs[3] = fixed_packs[0];
    }
    divide_recursively(tile_table, fixed_cnt + 3, 0, 0, &work_division, &result);
    if (result.count != 1) {
        return false;
    }

    const pack_t *packs = result.divisions[0].packs;  // packs at indices 0, 1, 2 are unused
    const pack_t &involved_pack = packs[3];  // Independent 4th set
    const tile_t pair_tile = pack_get_tile(packs[4]);  // Pair tile

    // Mark fan
    fan_table[KNITTED_STRAIGHT] = 1;  // Knitted Straight

    // Adjust according to 4th set
    const uint8_t involved_type = pack_get_type(involved_pack);
    if (involved_type == PACK_TYPE_CHOW) {  // 4th set is chow
        // Numbered tile pair gives All Chows
        if (is_numbered_suit_quick(pair_tile)) {
            fan_table[ALL_CHOWS] = 1;
        }

        // Only pair can form Tile Hog
        if (fixed_table[pair_tile] + standing_table[pair_tile] == 4) {
            fan_table[TILE_HOG] = 1;
        }
    }
    else {  // 4th set is pung
        const tile_t involved_tile = pack_get_tile(involved_pack);
        if (is_honor(involved_tile)) {  // Honor tile pung
            if (is_winds(involved_tile)) {
                // Tentatively mark as Pung of Terminals or Honors, adjust later by Prevalent/Seat Wind
                fan_table[PUNG_OF_TERMINALS_OR_HONORS] = 1;
                adjust_by_winds(involved_tile, calculate_param->prevalent_wind, calculate_param->seat_wind, fan_table);

                // Wind pung + Dragon pair gives All Types
                if (is_dragons(pair_tile)) {
                    fan_table[ALL_TYPES] = 1;
                }
            }
            else {
                fan_table[DRAGON_PUNG] = 1;

                // Dragon pung + Wind pair gives All Types
                if (is_winds(pair_tile)) {
                    fan_table[ALL_TYPES] = 1;
                }
            }
        }
        else {  // Numbered tile pung
            if (is_terminal(involved_tile)) {  // Numbered terminal 1/9 pung
                fan_table[PUNG_OF_TERMINALS_OR_HONORS] = 1;
            }

            // Numbered tile pair gives No Honors
            if (!is_honor(pair_tile)) {
                fan_table[NO_HONORS] = 1;
            }

            // Only numbered pung can form Tile Hog
            if (involved_type != PACK_TYPE_KONG && fixed_table[involved_tile] + standing_table[involved_tile] == 4) {
                fan_table[TILE_HOG] = 1;
            }
        }
    }

    adjust_by_win_flag(win_flag, fan_table);

    // Adjust according to melded/concealed state of 4th set
    if (is_pack_melded(involved_pack)) {
        if (involved_type == PACK_TYPE_KONG) {
            fan_table[MELDED_KONG] = 1;
        }
    }
    else {
        if (involved_type == PACK_TYPE_KONG) {
            fan_table[CONCEALED_KONG] = 1;
        }

        // Add Concealed Hand or Fully Concealed Hand
        if (win_flag & WIN_FLAG_SELF_DRAWN) {
            fan_table[FULLY_CONCEALED_HAND] = 1;
            fan_table[SELF_DRAWN] = 0;
        }
        else {
            fan_table[CONCEALED_HAND] = 1;
        }
    }

#if SUPPORT_BLESSINGS
    // Blessing of Heaven, Blessing of Earth, Blessing of Human
    if (fixed_cnt == 0 && (win_flag & WIN_FLAG_INITIAL)) {
        adjust_by_initial_hands(calculate_param->seat_wind == wind_t::EAST, win_flag, fan_table);
    }
#endif

    const bool heavenly = calculate_param->seat_wind == wind_t::EAST
        && fixed_cnt == 0
        && (win_flag & (WIN_FLAG_INITIAL | WIN_FLAG_SELF_DRAWN)) == (WIN_FLAG_INITIAL | WIN_FLAG_SELF_DRAWN);

#if KNITTED_STRAIGHT_BODY_WITH_ECS == 0
    // Winning tile is within Knitted Straight range: do not score Edge, Closed, or Single Wait
    if (std::none_of(std::begin(*matched_seq), std::end(*matched_seq), [win_tile](tile_t t) { return t == win_tile; })) {
        if (fixed_cnt == 0) {  // Concealed hand may have Edge, Closed, or Single Wait
            // Blessing of Heaven does not score Edge, Closed, or Single Wait
            if (!heavenly) {
                if (is_unique_waiting(tile_table, 4, win_tile)) {
                    // Adjust by wait type: involves Edge Wait, Closed Wait, Single Wait
                    adjust_by_waiting_form(packs + 3, 2, win_tile, fan_table);
                }
            }
        }
        else {
            // In non-concealed state, if waiting tile is outside Knitted Straight, must be Single Wait
            fan_table[SINGLE_WAIT] = 1;
        }
    }
#else
    // If waiting tile can be interpreted as part of Knitted Straight and also as Edge, Closed, or Single Wait, score it
    if (fixed_cnt == 0) {  // Concealed hand may have Edge, Closed, or Single Wait
        // Blessing of Heaven does not score Edge, Closed, or Single Wait
        if (!heavenly) {
            if (is_unique_waiting(tile_table, 4, win_tile)) {
                // Adjust by wait type: involves Edge Wait, Closed Wait, Single Wait
                adjust_by_waiting_form(packs + 3, 2, win_tile, fan_table);
            }
        }
    }
    else {
        // In non-concealed state, if waiting tile is outside Knitted Straight, must be Single Wait
        if (std::none_of(std::begin(*matched_seq), std::end(*matched_seq), [win_tile](tile_t t) { return t == win_tile; })) {
            fan_table[SINGLE_WAIT] = 1;
        }
        else {
            // In non-concealed state, if waiting for pair on Knitted Straight body, tile will appear 3 times
            if (standing_table[win_tile] == 3) {
                fan_table[SINGLE_WAIT] = 1;
            }
        }
    }
#endif

    // Adjust rule exclusions
    final_adjust(fan_table);

    return true;
}

// Seven Pairs
static bool is_seven_pairs(const tile_table_t &tile_table) {
    const auto &all_tiles = standard_tiles<>::all;
    for (int i = 0; i < 34; ++i) {
        tile_t t = all_tiles[i];
        if (tile_table[t] & 1) {
            return false;
        }
    }
    return true;
}

// Seven Shifted Pairs
static bool is_seven_shifted_pairs(const tile_table_t &tile_table, suit_t suit) {
    if (suit == TILE_SUIT_HONORS) {
        return false;
    }

    // Seven Shifted Pairs has only three patterns: 1~7, 2~8, 3~9; in all cases, 3~7 have pairs
    tile_t t3 = make_tile(suit, 3);
    if (tile_table[t3] == 2 && tile_table[t3 + 1] == 2 && tile_table[t3 + 2] == 2 && tile_table[t3 + 3] == 2 && tile_table[t3 + 4] == 2) {
        if (tile_table[t3 - 1] == 2) {
            // 2 is present: could be 1~7 or 2~8
            return (tile_table[t3 - 2] == 2 || tile_table[t3 + 5] == 2);
        }

        // 2 is absent: can only be 3~9
        return (tile_table[t3 + 5] == 2 && tile_table[t3 + 6] == 2);
    }
    return false;
}

// Thirteen Orphans
static FORCE_INLINE bool is_thirteen_orphans(const tile_t *unique_tiles, intptr_t unique_cnt) {
    const auto &standard_thirteen_orphans = standard_tiles<>::thirteen_orphans;
    return unique_cnt == 13 && std::equal(standard_thirteen_orphans, standard_thirteen_orphans + 13, unique_tiles);
}

// Lesser / Greater Honors and Knitted Tiles fan calculation
static bool calculate_honors_and_knitted_tiles(const tile_t *unique_tiles, intptr_t unique_cnt, fan_table_t &fan_table) {
    if (unique_cnt != 14) {
        return false;
    }

    const tile_t *honor_begin = std::find_if(unique_tiles, unique_tiles + 14, &is_honor);
    int numbered_cnt = static_cast<int>(honor_begin - unique_tiles);
    // Numbered tiles count > 9 or < 7 cannot be Lesser Honors and Knitted Tiles
    if (numbered_cnt > 9 || numbered_cnt < 7) {
        return false;
    }

    // Match Knitted Straight
    const auto &standard_knitted_straight = standard_tiles<>::knitted_straight;
    if (std::none_of(&standard_knitted_straight[0], &standard_knitted_straight[6],
        [&unique_tiles, honor_begin](const tile_t (&seq)[9]) {
        return std::includes(std::begin(seq), std::end(seq), unique_tiles, honor_begin);
    })) {
        return false;
    }

    const auto &standard_thirteen_orphans = standard_tiles<>::thirteen_orphans;
    if (numbered_cnt == 7 && std::equal(std::begin(standard_thirteen_orphans) + 6, std::end(standard_thirteen_orphans), unique_tiles + 7)) {
        // All seven honors present gives Greater Honors and Knitted Tiles
        fan_table[GREATER_HONORS_AND_KNITTED_TILES] = 1;
        return true;
    }
    else if (std::includes(std::begin(standard_thirteen_orphans) + 6, std::end(standard_thirteen_orphans), honor_begin, unique_tiles + 14)) {
        // Lesser Honors and Knitted Tiles
        fan_table[LESSER_HONORS_AND_KNITTED_TILES] = 1;
        if (numbered_cnt == 9) {  // 9 numbered tiles: Lesser Honors and Knitted Tiles with Knitted Straight
            fan_table[KNITTED_STRAIGHT] = 1;
        }
        return true;
    }

    return false;
}

static void adjust_by_win_flag_4_special_form(wind_t seat_wind, win_flag_t win_flag, fan_table_t &fan_table) {
    adjust_by_win_flag(win_flag, fan_table);
#if SUPPORT_BLESSINGS
    // Blessing of Heaven, Blessing of Earth, Blessing of Human
    if (win_flag & WIN_FLAG_INITIAL) {
        adjust_by_initial_hands(seat_wind == wind_t::EAST, win_flag, fan_table);
    }
#else
    (void)seat_wind;
#endif
}

// Special winning forms fan calculation
static bool calculate_special_form_fan(const tile_table_t &standing_table, tile_t win_tile,
    const tile_t *unique_tiles, intptr_t unique_cnt, wind_t seat_wind, win_flag_t win_flag, fan_table_t &fan_table) {

    // In order of frequency of occurrence

    // Seven Pairs
    if (is_seven_pairs(standing_table)) {
        suit_t s = tile_get_suit(win_tile);
        if (is_seven_shifted_pairs(standing_table, s)) {
            // Seven Shifted Pairs
            fan_table[SEVEN_SHIFTED_PAIRS] = 1;

            // 2~8 Seven Shifted Pairs adds All Simples
            if (standing_table[make_tile(s, 1)] == 0 && standing_table[make_tile(s, 9)] == 0) {
                fan_table[ALL_SIMPLES] = 1;
            }

            adjust_by_win_flag_4_special_form(seat_wind, win_flag, fan_table);
        }
        else {
            // Standard Seven Pairs
            fan_table[SEVEN_PAIRS] = 1;

            // Adjust by suits: involves No Honors, One Voided Suit, Half Flush, Full Flush, All Types
            adjust_by_suits(unique_tiles, unique_cnt, fan_table);
            // Adjust by tile characteristics: involves All Simples, Reversible Tiles, All Green, All Honors, All Terminals, All Terminals and Honors
            adjust_by_tiles_traits(unique_tiles, unique_cnt, fan_table);
            // Adjust by numbered tile rank ranges: involves Upper Four, Lower Four, All Upper, All Five, All Lower
            adjust_by_rank_range(unique_tiles, unique_cnt, fan_table);
            // Tile Hog adjustment
            adjust_by_tiles_hog(standing_table, 0, fan_table);

            adjust_by_win_flag_4_special_form(seat_wind, win_flag, fan_table);

            // Adjust rule exclusions
            final_adjust(fan_table);
        }

        return true;
    }

    // Lesser / Greater Honors and Knitted Tiles
    if (calculate_honors_and_knitted_tiles(unique_tiles, unique_cnt, fan_table)) {
        adjust_by_win_flag_4_special_form(seat_wind, win_flag, fan_table);
        return true;
    }

    // Thirteen Orphans
    if (is_thirteen_orphans(unique_tiles, unique_cnt)) {
        fan_table[THIRTEEN_ORPHANS] = 1;
        adjust_by_win_flag_4_special_form(seat_wind, win_flag, fan_table);
        return true;
    }

    return false;
}

// Nine Gates fan calculation
static bool calculate_nine_gates_fan(const tile_table_t &standing_table, tile_t win_tile, wind_t seat_wind, win_flag_t win_flag, fan_table_t &fan_table) {
    suit_t s;
    rank_t r;
    const bool heavenly = seat_wind == wind_t::EAST
        && (win_flag & (WIN_FLAG_INITIAL | WIN_FLAG_SELF_DRAWN)) == (WIN_FLAG_INITIAL | WIN_FLAG_SELF_DRAWN);
    if (heavenly) {
#if NINE_GATES_WHEN_BLESSING_OF_HEAVEN
        // NOTE: Nine Gates on Blessing of Heaven check is different from normal

        s = tile_get_suit(win_tile);

        // Find 2~8 tiles with count 2; if missing any, definitely not Nine Gates
        tile_t win_tile2 = 0;
        for (rank_t i = 2; i < 9; ++i) {
            tile_t tmp = make_tile(s, i);
            const uint16_t cnt = standing_table[tmp];
            if (cnt == 0) {
                return false;
            }
            else if (cnt == 2) {
                win_tile2 = tmp;
            }
        }

        if (win_tile2 != 0) {
            // If one rank has 2 tiles and both 1 and 9 have 3 tiles, it is Nine Gates
            if (standing_table[make_tile(s, 1)] == 3 && standing_table[make_tile(s, 9)] == 3) {
                r = tile_get_rank(win_tile2);
            }
            else {
                return false;
            }
        }
        else {
            // If all 2~8 have 1 tile, one of 1 and 9 must have 4 tiles and the other 3 tiles
            if (standing_table[make_tile(s, 1)] == 4 && standing_table[make_tile(s, 9)] == 3) {
                r = 1;
            }
            else if (standing_table[make_tile(s, 1)] == 3 && standing_table[make_tile(s, 9)] == 4) {
                r = 2;
            }
            else {
                return false;
            }
        }
#else
        return false;
#endif
    }
    else {
        s = tile_get_suit(win_tile);
        r = tile_get_rank(win_tile);
        if (r == 1) {
            if (standing_table[win_tile] != 4
                || standing_table[make_tile(s, 9)] != 3
                || std::any_of(standing_table + make_tile(s, 2), standing_table + make_tile(s, 9), [](int n) { return n != 1; })) {
                return false;
            }
        }
        else if (r == 9) {
            if (standing_table[win_tile] != 4
                || standing_table[make_tile(s, 1)] != 3
                || std::any_of(standing_table + make_tile(s, 2), standing_table + make_tile(s, 9), [](int n) { return n != 1; })) {
                return false;
            }
        }
        else {
            if (standing_table[win_tile] != 2
                || standing_table[make_tile(s, 1)] != 3
                || standing_table[make_tile(s, 9)] != 3
                || std::any_of(standing_table + make_tile(s, 2), standing_table + make_tile(s, r), [](int n) { return n != 1; })
                || std::any_of(standing_table + make_tile(s, r + 1), standing_table + make_tile(s, 9), [](int n) { return n != 1; })) {
                return false;
            }
        }
    }

    fan_table[NINE_GATES] = 1;

    // Win on 1 or 9 adds Pure Straight, Tile Hog
    // Win on 2, 5, or 8 adds Two Concealed Pungs, Pung of Terminals or Honors
    // Win on 2, 3, 4, 6, 7, or 8 adds Short Straight
    switch (r) {
    case 1: case 9:
        fan_table[PURE_STRAIGHT] = 1;
        fan_table[TILE_HOG] = 1;
        break;
    case 2: case 8:
        fan_table[TWO_CONCEALED_PUNGS] = 1;
        fan_table[SHORT_STRAIGHT] = 1;
        fan_table[PUNG_OF_TERMINALS_OR_HONORS] = 1;
        break;
    case 5:
        fan_table[TWO_CONCEALED_PUNGS] = 1;
        fan_table[PUNG_OF_TERMINALS_OR_HONORS] = 1;
        break;
    case 4: case 7: case 3: case 6:
        fan_table[SHORT_STRAIGHT] = 1;
        break;
    default:
        break;
    }

    adjust_by_win_flag_4_special_form(seat_wind, win_flag, fan_table);

    return true;
}

// Calculate fan points from fan table
static int get_fan_by_table(const fan_table_t &fan_table) {
    const auto &fan_value_table = fan_value<>::table;
    int fan = 0;
    for (int i = 1; i < FAN_TABLE_SIZE; ++i) {
        if (fan_table[i] == 0) {
            continue;
        }
        fan += fan_value_table[i] * fan_table[i];
#if 0  // Debug
        if (fan_table[i] == 1) {
            LOG("%s %hu\n", fan_name<>::text[i], fan_value_table[i]);
        }
        else {
            LOG("%s %hu*%hu\n", fan_name<>::text[i], fan_value_table[i], fan_table[i]);
        }
#endif
    }
    return fan;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////
// Calculate fan
//
int calculate_fan(const calculate_param_t *calculate_param, fan_table_t *fan_table) {
    const hand_tiles_t *hand_tiles = &calculate_param->hand_tiles;
    const intptr_t fixed_cnt = hand_tiles->pack_count;
    const intptr_t standing_cnt = hand_tiles->tile_count;

    // Treat each melded pack as 3 tiles, then total tiles = 13
    if (standing_cnt <= 0 || fixed_cnt < 0 || fixed_cnt > 4 || fixed_cnt * 3 + standing_cnt != 13) {
        return ERROR_WRONG_TILES_COUNT;
    }

    const tile_t win_tile = calculate_param->win_tile;

    tile_table_t fixed_table{}, standing_table{};
    map_packs(hand_tiles->fixed_packs, fixed_cnt, fixed_table);
    map_tiles(hand_tiles->standing_tiles, standing_cnt, standing_table);
    ++standing_table[win_tile];  // Add winning tile to hand

    tile_t unique_tiles[18];
    const intptr_t unique_cnt = table_unique(fixed_table, standing_table, unique_tiles);

    win_flag_t win_flag = calculate_param->win_flag;

    // Correct win flags
    // If standing tiles include winning tile, cannot be Last Tile
    if (standing_table[win_tile] != 1) {
        win_flag &= ~WIN_FLAG_LAST_TILE;
    }

    // If winning tile appears 3 times in melds, must be Last Tile
    if (3 == fixed_table[win_tile]) {
        win_flag |= WIN_FLAG_LAST_TILE;
    }

    // Append kong flag
    if (win_flag & WIN_FLAG_KONG_INVOLVED) {
        if (win_flag & WIN_FLAG_SELF_DRAWN) {  // Self-drawn
            // If hand has no kong, cannot be Out with Replacement Tile
            if (std::none_of(hand_tiles->fixed_packs, hand_tiles->fixed_packs + fixed_cnt,
                [](pack_t pack) { return pack_get_type(pack) == PACK_TYPE_KONG; })) {
                win_flag &= ~WIN_FLAG_KONG_INVOLVED;
            }
        }
        else {  // Discard
            // If winning tile appeared in hand, cannot be Robbing the Kong
            if (fixed_table[win_tile] != 0 || standing_table[win_tile] != 1) {
                win_flag &= ~WIN_FLAG_KONG_INVOLVED;
            }
        }
    }

    // Maximum fan flag
    int max_fan = 0;
    fan_table_t tmp_table = { 0 };

    // First check special winning forms
    if (fixed_cnt == 0) {  // Concealed hand state: could be regular form Knitted Straight
        // Frequency: Seven Pairs > Knitted Straight > Nine Gates
        if (calculate_special_form_fan(standing_table, win_tile, unique_tiles, unique_cnt, calculate_param->seat_wind, win_flag, tmp_table)
            || calculate_knitted_straight_fan(fixed_table, standing_table, calculate_param, win_flag, tmp_table)
            || calculate_nine_gates_fan(standing_table, win_tile, calculate_param->seat_wind, win_flag, tmp_table)) {
            max_fan = get_fan_by_table(tmp_table);
            LOG("fan = %d\n\n", max_fan);
        }
    }
    else if (fixed_cnt == 1) {  // 1 meld state: could be regular form Knitted Straight
        if (calculate_knitted_straight_fan(fixed_table, standing_table, calculate_param, win_flag, tmp_table)) {
            max_fan = get_fan_by_table(tmp_table);
            LOG("fan = %d\n\n", max_fan);
        }
    }

    // Cannot form special winning form or is Seven Pairs
    // Seven Pairs must also be decomposed as regular form because in extreme cases regular form fan can exceed Seven Pairs
    if (max_fan == 0 || tmp_table[SEVEN_PAIRS] == 1) {
        const bool heavenly = calculate_param->seat_wind == wind_t::EAST
            && fixed_cnt == 0
            && (win_flag & (WIN_FLAG_INITIAL | WIN_FLAG_SELF_DRAWN)) == (WIN_FLAG_INITIAL | WIN_FLAG_SELF_DRAWN);
        const bool unique_waiting = !heavenly && is_unique_waiting(standing_table, standing_cnt, win_tile);

        // Division
        division_result_t result;
        if (divide_win_hand(standing_table, hand_tiles->fixed_packs, fixed_cnt, &result)) {
            fan_table_t fan_tables[MAX_DIVISION_CNT] = { { 0 } };
            const fan_table_t *selected_fan_table = nullptr;

            // Iterate over all decompositions, calculate fan, and find the decomposition with maximum fan points
            for (intptr_t i = 0; i < result.count; ++i) {
#if 0  // Debug
                // print division
#endif
                fan_table_t &current_table = fan_tables[i];
                calculate_regular_fan(result.divisions[i].packs, fixed_table, standing_table, unique_tiles, unique_cnt,
                    calculate_param, unique_waiting, win_flag, current_table);
                int current_fan = get_fan_by_table(current_table);
                if (current_fan > max_fan) {
                    max_fan = current_fan;
                    selected_fan_table = &current_table;
                }
                else if (current_fan == max_fan) {
                    // 2022.3.7 Prefer Pure Triple Chow and All Five
                    // 2023.9.2 Prefer Triple Pung
                    if (current_table[PURE_TRIPLE_CHOW] == 1 || tmp_table[SEVEN_PAIRS] == 1 || current_table[TRIPLE_PUNG]) {
                        selected_fan_table = &current_table;
                    }
                }
                LOG("fan = %d\n\n", current_fan);
            }

            if (fan_table != nullptr && selected_fan_table != nullptr) {
                std::memcpy(tmp_table, *selected_fan_table, sizeof(tmp_table));
            }
        }
    }

    if (max_fan == 0) {
        return ERROR_NOT_WIN;
    }

    // Add Flower Tiles
    max_fan += calculate_param->flower_count;

    if (fan_table != nullptr) {
        std::memcpy(*fan_table, tmp_table, sizeof(*fan_table));
        (*fan_table)[FLOWER_TILES] = calculate_param->flower_count;
    }

    return max_fan;
}

#if SUPPORT_BLESSINGS
// Check whether hand is necessarily concealed
static bool inevitably_concealed(const fan_table_t &fan_table) {
    return fan_table[SEVEN_PAIRS]
        || fan_table[LESSER_HONORS_AND_KNITTED_TILES]
        || fan_table[GREATER_HONORS_AND_KNITTED_TILES]
        || fan_table[FOUR_CONCEALED_PUNGS]
        || fan_table[THIRTEEN_ORPHANS]
        || fan_table[SEVEN_SHIFTED_PAIRS]
        || fan_table[NINE_GATES];
}

// Revoke Blessing of Heaven, Earth, and Human
int revoke_blessings(fan_table_t &fan_table) {
    const auto &fan_value_table = fan_value<>::table;
    if (fan_table[BLESSING_OF_HEAVEN]) {
        int res = fan_value_table[BLESSING_OF_HEAVEN];
        fan_table[BLESSING_OF_HEAVEN] = 0;
        if (!inevitably_concealed(fan_table)) {
            fan_table[FULLY_CONCEALED_HAND] = 1;
            return res - fan_value_table[FULLY_CONCEALED_HAND];
        }
        else {
            fan_table[SELF_DRAWN] = 1;
            return res - fan_value_table[SELF_DRAWN];
        }
    }
    else if (fan_table[BLESSING_OF_HUMAN_2]) {
        int res = fan_value_table[BLESSING_OF_HUMAN_2];
        fan_table[BLESSING_OF_HUMAN_2] = 0;
        if (!inevitably_concealed(fan_table)) {
            fan_table[FULLY_CONCEALED_HAND] = 1;
            return res - fan_value_table[FULLY_CONCEALED_HAND];
        }
        else {
            fan_table[SELF_DRAWN] = 1;
            return res - fan_value_table[SELF_DRAWN];
        }
    }
    else if (fan_table[BLESSING_OF_EARTH]) {
        int res = fan_value_table[BLESSING_OF_EARTH];
        fan_table[BLESSING_OF_EARTH] = 0;
        if (!inevitably_concealed(fan_table)) {
            fan_table[CONCEALED_HAND] = 1;
            return res - fan_value_table[CONCEALED_HAND];
        }
        else {
            return res;
        }
    }
    else if (fan_table[BLESSING_OF_HUMAN_1]) {
        int res = fan_value_table[BLESSING_OF_HUMAN_1];
        fan_table[BLESSING_OF_HUMAN_1] = 0;
        if (!inevitably_concealed(fan_table)) {
            fan_table[CONCEALED_HAND] = 1;
            return res - fan_value_table[CONCEALED_HAND];
        }
        else {
            return res;
        }
    }
    return 0;
}
#endif

}
