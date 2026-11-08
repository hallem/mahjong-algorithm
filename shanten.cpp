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

#include "shanten.h"
#include <cassert>
#include <cstring>
#include <limits>
#include <algorithm>
#include <iterator>

#ifdef MAHJONG_ALGORITHM_ENABLE_SHANTEN
#define STATIC_IF_NECESSARY
#else
#define STATIC_IF_NECESSARY static
#endif

namespace mahjong {

// Convert packs to tiles
intptr_t packs_to_tiles(const pack_t *packs, intptr_t pack_cnt, tile_t *tiles, intptr_t tile_cnt) {
    if (packs == nullptr || tiles == nullptr) {
        return 0;
    }

    intptr_t cnt = 0;
    for (int i = 0; i < pack_cnt && cnt < tile_cnt; ++i) {
        tile_t tile = pack_get_tile(packs[i]);
        switch (pack_get_type(packs[i])) {
        case PACK_TYPE_CHOW:
            if (cnt < tile_cnt) tiles[cnt++] = static_cast<tile_t>(tile - 1);
            if (cnt < tile_cnt) tiles[cnt++] = tile;
            if (cnt < tile_cnt) tiles[cnt++] = static_cast<tile_t>(tile + 1);
            break;
        case PACK_TYPE_PUNG:
            if (cnt < tile_cnt) tiles[cnt++] = tile;
            if (cnt < tile_cnt) tiles[cnt++] = tile;
            if (cnt < tile_cnt) tiles[cnt++] = tile;
            break;
        case PACK_TYPE_KONG:
            if (cnt < tile_cnt) tiles[cnt++] = tile;
            if (cnt < tile_cnt) tiles[cnt++] = tile;
            if (cnt < tile_cnt) tiles[cnt++] = tile;
            if (cnt < tile_cnt) tiles[cnt++] = tile;
            break;
        case PACK_TYPE_PAIR:
            if (cnt < tile_cnt) tiles[cnt++] = tile;
            if (cnt < tile_cnt) tiles[cnt++] = tile;
            break;
        default:
            UNREACHABLE();
            break;
        }
    }
    return cnt;
}

// Map tiles to table
void map_tiles(const tile_t *tiles, intptr_t cnt, tile_table_t *tile_table) {
    std::memset(*tile_table, 0, sizeof(*tile_table));
    for (intptr_t i = 0; i < cnt; ++i) {
        ++(*tile_table)[tiles[i]];
    }
}

// Map hand tiles to table
bool map_hand_tiles(const hand_tiles_t *hand_tiles, tile_table_t *tile_table) {
    // Treat each melded pack as 3 tiles, then total tiles = 13
    if (hand_tiles->tile_count <= 0 || hand_tiles->pack_count < 0 || hand_tiles->pack_count > 4
        || hand_tiles->pack_count * 3 + hand_tiles->tile_count != 13) {
        return false;
    }

    // Restore melds to tiles
    tile_t tiles[18];
    intptr_t tile_cnt = 0;
    if (hand_tiles->pack_count == 0) {
        std::memcpy(tiles, hand_tiles->standing_tiles, 13 * sizeof(tile_t));
        tile_cnt = 13;
    }
    else {
        tile_cnt = packs_to_tiles(hand_tiles->fixed_packs, hand_tiles->pack_count, tiles, 18);
        std::memcpy(tiles + tile_cnt, hand_tiles->standing_tiles, hand_tiles->tile_count * sizeof(tile_t));
        tile_cnt += hand_tiles->tile_count;
    }

    // Map to table
    map_tiles(tiles, tile_cnt, tile_table);
    return true;
}

// Convert table to tiles
intptr_t table_to_tiles(const tile_table_t &tile_table, tile_t *tiles, intptr_t max_cnt) {
    intptr_t cnt = 0;
    const auto &all_tiles = standard_tiles<>::all;
    for (int i = 0; i < 34; ++i) {
        tile_t t = all_tiles[i];
        for (int n = 0; n < tile_table[t]; ++n) {
            *tiles++ = t;
            ++cnt;
            if (cnt == max_cnt) {
                return max_cnt;
            }
        }
    }
    return cnt;
}

#ifdef MAHJONG_ALGORITHM_ENABLE_SHANTEN

// Recursively calculate regular winning form shanten
// Parameters:
//   tile_table: Tile count table
//   has_pair: Whether a pair (eye) is present
//   pack_cnt: Number of completed sets
//   partner_cnt: Number of proto-runs (incomplete sets)
// Data from 0 to fixed_cnt is unused, reserved for melded sets
static int regular_shanten_recursively(tile_table_t &tile_table, const bool has_pair, const unsigned pack_cnt, const unsigned partner_cnt,
    const intptr_t fixed_cnt, eigen_t pack_eigen, eigen_t partner_eigen) {
    const auto &all_tiles = standard_tiles<>::all;
    if (fixed_cnt == 4) {  // 4 melds
        for (int i = 0; i < 34; ++i) {
            tile_t t = all_tiles[i];
            if (tile_table[t] > 1) {
                return -1;
            }
        }
        return 0;
    }

    if (pack_cnt == 4) {  // Already have 4 sets
        return has_pair ? -1 : 0;  // If pair exists, won (-1); if no pair, waiting (0)
    }

    int max_ret;  // Maximum shanten returnable from current state

    // Algorithm explanation:
    // Missing sets = 4 - completed sets
    // Missing proto-runs = missing sets - existing proto-runs
    // Combining both: missing proto-runs = 4 - completed sets - existing proto-runs
    int partner_need = 4 - pack_cnt - partner_cnt;
    if (partner_need > 0) {  // When more proto-runs are needed
        // With pair: shanten = existing proto-runs + missing proto-runs * 2 - 1
        // Without pair: shanten = existing proto-runs + missing proto-runs * 2
        max_ret = partner_cnt + partner_need * 2 - (has_pair ? 1 : 0);
    }
    else {  // When enough proto-runs exist
        // With pair: shanten = 3 - completed sets
        // Without pair: shanten = 4 - completed sets
        max_ret = (has_pair ? 3 : 4) - pack_cnt;
    }

    int result = max_ret;

    if (pack_cnt + partner_cnt > 4) {  // Proto-run overload
        return max_ret;
    }

    for (int i = 0; i < 34; ++i) {
        tile_t t = all_tiles[i];
        if (tile_table[t] < 1) {
            continue;
        }

        // Pair (eye)
        if (!has_pair && tile_table[t] > 1) {
            // Reduce pair, recurse
            tile_table[t] -= 2;
            int ret = regular_shanten_recursively(tile_table, true, pack_cnt, partner_cnt,
                fixed_cnt, pack_eigen, partner_eigen);
            result = std::min(ret, result);
            // Restore
            tile_table[t] += 2;
        }

        // Pung
        if (tile_table[t] > 2) {
            // If current pung eigenvalue is less than previous, this path has already been visited
            eigen_t eigen = make_eigen(t, t, t);
            if (eigen > pack_eigen) {
                // Reduce this pung, recurse
                tile_table[t] -= 3;
                int ret = regular_shanten_recursively(tile_table, has_pair, pack_cnt + 1, partner_cnt,
                    fixed_cnt, eigen, partner_eigen);
                result = std::min(ret, result);
                // Restore
                tile_table[t] += 3;
            }
        }

        // Chow (numbered suits only)
        bool is_numbered = is_numbered_suit(t);
        // Chow t, t+1, t+2; clearly t cannot be a numbered tile with rank 8 or higher
        if (is_numbered && tile_get_rank(t) < 8 && tile_table[t + 1] && tile_table[t + 2]) {
            // If current chow eigenvalue is less than previous, this path has already been visited
            eigen_t eigen = make_eigen(t, t + 1, t + 2);
            if (eigen >= pack_eigen) {
                // Reduce this chow, recurse
                --tile_table[t];
                --tile_table[t + 1];
                --tile_table[t + 2];
                int ret = regular_shanten_recursively(tile_table, has_pair, pack_cnt + 1, partner_cnt,
                    fixed_cnt, eigen, partner_eigen);
                result = std::min(ret, result);
                // Restore
                ++tile_table[t];
                ++tile_table[t + 1];
                ++tile_table[t + 2];
            }
        }

        // If shanten has already been reduced by removing pair/sets, calculating by proto-runs will not yield fewer
        if (result < max_ret) {
            continue;
        }

        // Pung proto-run
        if (tile_table[t] > 1) {
            // If current pung proto-run eigenvalue is less than previous, this path has already been visited
            eigen_t eigen = make_eigen(t, t, 0);
            if (eigen > partner_eigen) {
                // Reduce pung proto-run, recurse
                tile_table[t] -= 2;
                int ret = regular_shanten_recursively(tile_table, has_pair, pack_cnt, partner_cnt + 1,
                    fixed_cnt, pack_eigen, eigen);
                result = std::min(ret, result);
                // Restore
                tile_table[t] += 2;
            }
        }

        // Chow proto-run (numbered suits only)
        if (is_numbered) {
            // Two-sided or edge wait proto-run t, t+1; clearly t cannot be a numbered tile of rank 9
            if (tile_get_rank(t) < 9 && tile_table[t + 1]) {  // Two-sided or edge wait
                // If current chow proto-run eigenvalue is less than previous, this path has already been visited
                eigen_t eigen = make_eigen(t, t + 1, 0);
                if (eigen >= partner_eigen) {
                    // Reduce proto-run, recurse
                    --tile_table[t];
                    --tile_table[t + 1];
                    int ret = regular_shanten_recursively(tile_table, has_pair, pack_cnt, partner_cnt + 1,
                        fixed_cnt, pack_eigen, eigen);
                    result = std::min(ret, result);
                    // Restore
                    ++tile_table[t];
                    ++tile_table[t + 1];
                }
            }
            // Closed wait proto-run t, t+2; clearly t cannot be a numbered tile with rank 8 or higher
            if (tile_get_rank(t) < 8 && tile_table[t + 2]) {  // Closed wait
                // If current chow proto-run eigenvalue is less than previous, this path has already been visited
                eigen_t eigen = make_eigen(t, t + 2, 0);
                if (eigen >= partner_eigen) {
                    // Reduce proto-run, recurse
                    --tile_table[t];
                    --tile_table[t + 2];
                    int ret = regular_shanten_recursively(tile_table, has_pair, pack_cnt, partner_cnt + 1,
                        fixed_cnt, pack_eigen, eigen);
                    result = std::min(ret, result);
                    // Restore
                    ++tile_table[t];
                    ++tile_table[t + 2];
                }
            }
        }
    }

    return result;
}

// Whether numbered tile has proto-run
static bool numbered_tile_has_partner(const tile_table_t &tile_table, tile_t t) {
    rank_t r = tile_get_rank(t);
    if (r < 9) { if (tile_table[t + 1]) return true; }
    if (r < 8) { if (tile_table[t + 2]) return true; }
    if (r > 1) { if (tile_table[t - 1]) return true; }
    if (r > 2) { if (tile_table[t - 2]) return true; }
    return false;
}

// Calculate regular winning form shanten using table as parameter
static int regular_shanten_from_table(tile_table_t &tile_table, intptr_t fixed_cnt, useful_table_t *useful_table) {
    // Calculate shanten
    int result = regular_shanten_recursively(tile_table, false, static_cast<uint16_t>(fixed_cnt), 0,
        fixed_cnt, 0, 0);

    if (useful_table == nullptr) {
        return result;
    }

    // Enumerate all tiles to find those that can reduce shanten
    const auto &all_tiles = standard_tiles<>::all;
    for (int i = 0; i < 34; ++i) {
        tile_t t = all_tiles[i];
        if (tile_table[t] == 4 && result > 0) {
            continue;
        }

        if (tile_table[t] == 0) {
            // Skip isolated honor tiles and non-adjacent numbered tiles; these cannot reduce shanten
            if (is_honor(t) || !numbered_tile_has_partner(tile_table, t)) {
                continue;
            }
        }

        ++tile_table[t];
        int temp = regular_shanten_recursively(tile_table, false, static_cast<uint16_t>(fixed_cnt), 0,
            fixed_cnt, 0, 0);
        if (temp < result) {
            (*useful_table)[t] = true;  // Mark as useful tile
        }
        --tile_table[t];
    }

    return result;
}

// Regular winning form shanten
int regular_shanten(const tile_t *standing_tiles, intptr_t standing_cnt, useful_table_t *useful_table) {
    if (standing_tiles == nullptr || (standing_cnt != 13
        && standing_cnt != 10 && standing_cnt != 7 && standing_cnt != 4 && standing_cnt != 1)) {
        return std::numeric_limits<int>::max();
    }

    // Map standing tiles to table
    tile_table_t tile_table;
    map_tiles(standing_tiles, standing_cnt, &tile_table);

    if (useful_table != nullptr) {
        std::memset(*useful_table, 0, sizeof(*useful_table));
    }
    return regular_shanten_from_table(tile_table, (13 - standing_cnt) / 3, useful_table);
}

#endif

// Check whether 1 tile is waiting for regular winning form
static bool is_regular_wait_1(tile_table_t &tile_table, useful_table_t *waiting_table) {
    const auto &all_tiles = standard_tiles<>::all;
    for (int i = 0; i < 34; ++i) {
        tile_t t = all_tiles[i];
        if (tile_table[t] != 1) {
            continue;
        }

        // Single wait for pair (eye)
        tile_table[t] = 0;
        if (std::all_of(std::begin(tile_table), std::end(tile_table), [](int n) { return n == 0; })) {
            tile_table[t] = 1;
            if (waiting_table != nullptr) {  // If waiting tiles not needed, can return directly
                (*waiting_table)[t] = true;
            }
            return true;
        }
        tile_table[t] = 1;
    }

    return false;
}

// Check whether 2 tiles are waiting for regular winning form
static bool is_regular_wait_2(const tile_table_t &tile_table, useful_table_t *waiting_table) {
    bool ret = false;
    const auto &all_tiles = standard_tiles<>::all;
    for (int i = 0; i < 34; ++i) {
        tile_t t = all_tiles[i];
        if (tile_table[t] < 1) {
            continue;
        }
        if (tile_table[t] > 1) {
            if (waiting_table != nullptr) {  // Get waiting tiles
                (*waiting_table)[t] = true;  // Dual wait
                ret = true;
                continue;
            }
            else {  // If waiting tiles not needed, can return directly
                return true;
            }
        }
        if (is_numbered_suit_quick(t)) {  // Numbered suit proto-run
            rank_t r = tile_get_rank(t);
            if (r > 1 && tile_table[t - 1]) {  // Two-sided or edge wait
                if (waiting_table != nullptr) {  // Get waiting tiles
                    if (r < 9) (*waiting_table)[t + 1] = true;
                    if (r > 2) (*waiting_table)[t - 2] = true;
                    ret = true;
                    continue;
                }
                else {  // If waiting tiles not needed, can return directly
                    return true;
                }
            }
            if (r > 2 && tile_table[t - 2]) {  // Closed wait
                if (waiting_table != nullptr) {  // Get waiting tiles
                    (*waiting_table)[t - 1] = true;
                    ret = true;
                    continue;
                }
                else {  // If waiting tiles not needed, can return directly
                    return true;
                }
            }
        }
    }
    return ret;
}

// Check whether 4 tiles are waiting for regular winning form
static bool is_regular_wait_4(tile_table_t &tile_table, useful_table_t *waiting_table) {
    bool ret = false;
    // Reduce pair
    const auto &all_tiles = standard_tiles<>::all;
    for (int i = 0; i < 34; ++i) {
        tile_t t = all_tiles[i];
        if (tile_table[t] < 2) {
            continue;
        }
        // Reduce pair, recurse
        tile_table[t] -= 2;
        if (is_regular_wait_2(tile_table, waiting_table)) {
            ret = true;
        }
        // Restore
        tile_table[t] += 2;
        if (ret && waiting_table == nullptr) {  // If waiting tiles not needed, can end recursion directly
            return true;
        }
    }

    return ret;
}

// Recursively check whether regular winning form is waiting
static bool is_regular_wait_recursively(tile_table_t &tile_table, intptr_t left_cnt, eigen_t prev_eigen, useful_table_t *waiting_table) {
    if (left_cnt == 1) {
        return is_regular_wait_1(tile_table, waiting_table);
    }

    bool ret = false;
    if (left_cnt == 4) {
        ret = is_regular_wait_4(tile_table, waiting_table);
        if (ret && waiting_table == nullptr) {  // If waiting tiles not needed, can end recursion directly
            return true;
        }
    }

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
                // Reduce this pung, recurse
                tile_table[t] -= 3;
                if (is_regular_wait_recursively(tile_table, left_cnt - 3, eigen, waiting_table)) {
                    ret = true;
                }
                // Restore
                tile_table[t] += 3;
                if (ret && waiting_table == nullptr) {  // If waiting tiles not needed, can end recursion directly
                    return true;
                }
            }
        }

        // Chow (numbered suits only)
        if (is_numbered_suit(t)) {
            // Chow t, t+1, t+2; clearly t cannot be a numbered tile with rank 8 or higher
            if (tile_get_rank(t) < 8 && tile_table[t + 1] && tile_table[t + 2]) {
                // If current chow eigenvalue is less than previous, this path has already been visited
                eigen_t eigen = make_eigen(t, t + 1, t + 2);
                if (eigen >= prev_eigen) {
                    // Reduce this chow, recurse
                    --tile_table[t];
                    --tile_table[t + 1];
                    --tile_table[t + 2];
                    if (is_regular_wait_recursively(tile_table, left_cnt - 3, eigen, waiting_table)) {
                        ret = true;
                    }
                    // Restore
                    ++tile_table[t];
                    ++tile_table[t + 1];
                    ++tile_table[t + 2];
                    if (ret && waiting_table == nullptr) {  // If waiting tiles not needed, can end recursion directly
                        return true;
                    }
                }
            }
        }
    }

    return ret;
}

// Whether regular winning form is waiting
// The reason we do not simply call the shanten function and check if return value is 0
// is that the former reduces proto-runs, which is unnecessary for win checks; thus a separate faster logic is used
bool is_regular_wait(const tile_t *standing_tiles, intptr_t standing_cnt, useful_table_t *waiting_table) {
    // Map standing tiles to table
    tile_table_t tile_table;
    map_tiles(standing_tiles, standing_cnt, &tile_table);

    if (waiting_table != nullptr) {
        std::memset(*waiting_table, 0, sizeof(*waiting_table));
    }
    return is_regular_wait_recursively(tile_table, standing_cnt, 0, waiting_table);
}

// Whether 2 tiles can form a win in regular winning form
static bool is_regular_win_2(const tile_table_t &tile_table) {
    // Find unused tile
    typedef std::remove_all_extents<tile_table_t>::type table_elem_t;
    const table_elem_t *it = std::find_if(std::begin(tile_table), std::end(tile_table), [](table_elem_t n) { return n > 0; });
    // Exists and count equals 2
    if (it == std::end(tile_table) || *it != 2) {
        return false;
    }
    // Other unused tiles exist
    return std::none_of(it + 1, std::end(tile_table), [](int n) { return n > 0; });
}

// Recursively calculate whether regular winning form is a win
// The reason we do not simply call the shanten function and check if return value is -1
// is that the former reduces proto-runs, which is unnecessary for win checks; thus a separate faster logic is used
static bool is_regular_win_recursively(tile_table_t &tile_table, intptr_t left_cnt, eigen_t prev_eigen) {
    if (left_cnt == 2) {
        return is_regular_win_2(tile_table);
    }

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
                // Reduce this pung, recurse
                tile_table[t] -= 3;
                bool ret = is_regular_win_recursively(tile_table, left_cnt - 3, eigen);
                // Restore
                tile_table[t] += 3;
                if (ret) {
                    return true;
                }
            }
        }

        // Chow (numbered suits only)
        if (is_numbered_suit(t)) {
            // Chow t, t+1, t+2; clearly t cannot be a numbered tile with rank 8 or higher
            if (tile_get_rank(t) < 8 && tile_table[t + 1] && tile_table[t + 2]) {
                // If current chow eigenvalue is less than previous, this path has already been visited
                eigen_t eigen = make_eigen(t, t + 1, t + 2);
                if (eigen >= prev_eigen) {
                    // Reduce this chow, recurse
                    --tile_table[t];
                    --tile_table[t + 1];
                    --tile_table[t + 2];
                    bool ret = is_regular_win_recursively(tile_table, left_cnt - 3, eigen);
                    // Restore
                    ++tile_table[t];
                    ++tile_table[t + 1];
                    ++tile_table[t + 2];
                    if (ret) {
                        return true;
                    }
                }
            }
        }
    }

    return false;
}

// Whether regular winning form is a win
bool is_regular_win(const tile_t *standing_tiles, intptr_t standing_cnt, tile_t test_tile) {
    // Map standing tiles to table
    tile_table_t tile_table;
    map_tiles(standing_tiles, standing_cnt, &tile_table);
    ++tile_table[test_tile];  // Add test tile
    return is_regular_win_recursively(tile_table, standing_cnt + 1, 0);
}

//-------------------------------- Seven Pairs --------------------------------

// Seven Pairs shanten
STATIC_IF_NECESSARY int seven_pairs_shanten(const tile_t *standing_tiles, intptr_t standing_cnt, useful_table_t *useful_table) {
    if (standing_tiles == nullptr || standing_cnt != 13) {
        return std::numeric_limits<int>::max();
    }

    // Map tile types to table and count pairs
    int pair_cnt = 0;
    tile_table_t tile_table = { 0 };
    for (intptr_t i = 0; i < standing_cnt; ++i) {
        tile_t tile = standing_tiles[i];
        ++tile_table[tile];
        if (tile_table[tile] == 2) {
            ++pair_cnt;
            tile_table[tile] = 0;
        }
    }

    // Useful tiles
    if (useful_table != nullptr) {
        std::transform(std::begin(tile_table), std::end(tile_table), std::begin(*useful_table), [](int n) { return n != 0; });
    }
    return 6 - pair_cnt;
}

// Whether Seven Pairs is waiting
bool is_seven_pairs_wait(const tile_t *standing_tiles, intptr_t standing_cnt, useful_table_t *waiting_table) {
    // Directly calculate shanten; 0 shanten means waiting
    if (waiting_table == nullptr) {
        return (0 == seven_pairs_shanten(standing_tiles, standing_cnt, nullptr));
    }

    useful_table_t useful_table;
    if (0 == seven_pairs_shanten(standing_tiles, standing_cnt, &useful_table)) {
        std::memcpy(*waiting_table, useful_table, sizeof(*waiting_table));
        return true;
    }
    return false;
}

// Whether Seven Pairs is a win
bool is_seven_pairs_win(const tile_t *standing_tiles, intptr_t standing_cnt, tile_t test_tile) {
    useful_table_t useful_table;
    return (0 == seven_pairs_shanten(standing_tiles, standing_cnt, &useful_table)
        && useful_table[test_tile]);
}

//-------------------------------- Thirteen Orphans --------------------------------

// Thirteen Orphans shanten
STATIC_IF_NECESSARY int thirteen_orphans_shanten(const tile_t *standing_tiles, intptr_t standing_cnt, useful_table_t *useful_table) {
    if (standing_tiles == nullptr || standing_cnt != 13) {
        return std::numeric_limits<int>::max();
    }

    // Map tile types to table
    tile_table_t tile_table;
    map_tiles(standing_tiles, standing_cnt, &tile_table);

    const auto &standard_thirteen_orphans = standard_tiles<>::thirteen_orphans;
    bool has_pair = false;
    int cnt = 0;
    for (int i = 0; i < 13; ++i) {
        int n = tile_table[standard_thirteen_orphans[i]];
        if (n > 0) {
            ++cnt;  // Number of distinct terminal/honor tiles
            if (n > 1) {
                has_pair = true;  // Terminal/honor tile pair
            }
        }
    }

    // When a pair exists, shanten = 12 - count of distinct terminal/honor tiles
    // When no pair exists, shanten = 13 - count of distinct terminal/honor tiles
    int ret = has_pair ? 12 - cnt : 13 - cnt;

    if (useful_table != nullptr) {
        // First mark all terminal and honor tiles as useful tiles
        std::memset(*useful_table, 0, sizeof(*useful_table));
        std::for_each(std::begin(standard_thirteen_orphans), std::end(standard_thirteen_orphans),
            [useful_table](tile_t t) {
            (*useful_table)[t] = true;
        });

        // When a pair exists, existing terminal/honor tiles are not needed
        if (has_pair) {
            for (int i = 0; i < 13; ++i) {
                tile_t t = standard_thirteen_orphans[i];
                int n = tile_table[t];
                if (n > 0) {
                    (*useful_table)[t] = false;
                }
            }
        }
    }

    return ret;
}

// Whether Thirteen Orphans is waiting
bool is_thirteen_orphans_wait(const tile_t *standing_tiles, intptr_t standing_cnt, useful_table_t *waiting_table) {
    // Directly calculate shanten; 0 shanten means waiting
    if (waiting_table == nullptr) {
        return (0 == thirteen_orphans_shanten(standing_tiles, standing_cnt, nullptr));
    }

    useful_table_t useful_table;
    if (0 == thirteen_orphans_shanten(standing_tiles, standing_cnt, &useful_table)) {
        std::memcpy(*waiting_table, useful_table, sizeof(*waiting_table));
        return true;
    }
    return false;
}

// Whether Thirteen Orphans is a win
bool is_thirteen_orphans_win(const tile_t *standing_tiles, intptr_t standing_cnt, tile_t test_tile) {
    useful_table_t useful_table;
    return (0 == thirteen_orphans_shanten(standing_tiles, standing_cnt, &useful_table)
        && useful_table[test_tile]);
}

//-------------------------------- "Knitted Straight + Sets + Pair" Winning Form --------------------------------

// Check whether Knitted Straight is waiting using table as parameter
static bool is_knitted_straight_wait_from_table(const tile_table_t &tile_table, intptr_t left_cnt, useful_table_t *waiting_table) {
    // Match Knitted Straight
    const auto &standard_knitted_straight = standard_tiles<>::knitted_straight;
    const tile_t (*matched_seq)[9] = nullptr;
    tile_t missing_tiles[9];
    int missing_cnt = 0;
    for (int i = 0; i < 6; ++i) {  // Test each of the 6 Knitted Straights
        missing_cnt = 0;
        for (int k = 0; k < 9; ++k) {
            tile_t t = standard_knitted_straight[i][k];
            if (tile_table[t] == 0) {  // Missing
                missing_tiles[missing_cnt++] = t;
            }
        }
        if (missing_cnt < 2) {  // Missing 2 or more cannot be waiting
            matched_seq = &standard_knitted_straight[i];
            break;
        }
    }

    if (matched_seq == nullptr || missing_cnt > 2) {
        return false;
    }

    if (waiting_table != nullptr) {
        std::memset(*waiting_table, 0, sizeof(*waiting_table));
    }

    // Deduct Knitted Straight
    tile_table_t temp_table;
    std::memcpy(&temp_table, &tile_table, sizeof(temp_table));
    for (int i = 0; i < 9; ++i) {
        tile_t t = (*matched_seq)[i];
        if (temp_table[t]) {
            --temp_table[t];
        }
    }

    if (missing_cnt == 1) {  // If missing 1 tile, remaining tiles after removing Knitted Straight must be complete to be waiting
        if (left_cnt == 10) {
            if (is_regular_win_recursively(temp_table, 2, 0)) {
                if (waiting_table != nullptr) {  // Get waiting tile: wait for the missing tile of Knitted Straight
                    (*waiting_table)[missing_tiles[0]] = true;
                }
                return true;
            }
        }
        else {
            if (is_regular_win_recursively(temp_table, 5, 0)) {
                if (waiting_table != nullptr) {  // Get waiting tile: wait for the missing tile of Knitted Straight
                    (*waiting_table)[missing_tiles[0]] = true;
                }
                return true;
            }
        }
    }
    else if (missing_cnt == 0) {  // If Knitted Straight is complete, remaining tiles must be waiting for the entire hand to be waiting
        if (left_cnt == 10) {
            return is_regular_wait_1(temp_table, waiting_table);
        }
        else {
            return is_regular_wait_recursively(temp_table, 4, 0, waiting_table);
        }
    }

    return false;
}

#ifdef MAHJONG_ALGORITHM_ENABLE_SHANTEN

// Shanten for regular winning form containing main fan; can be used to calculate whole hand shanten for 3-meld fan like Pure Shifted Chows, Pure Triple Chow, Straight
static int regular_shanten_specified(const tile_table_t &tile_table, const tile_t *main_tiles, int main_cnt,
    intptr_t fixed_cnt, useful_table_t *useful_table) {

    tile_table_t temp_table;
    std::memcpy(&temp_table, &tile_table, sizeof(temp_table));
    int exist_cnt = 0;

    // Count tiles of main fan
    for (int i = 0; i < main_cnt; ++i) {
        tile_t t = main_tiles[i];
        int n = tile_table[t];
        if (n > 0) {  // Present, deduct it
            ++exist_cnt;
            --temp_table[t];
        }
    }

    // Record useful tiles
    if (useful_table != nullptr) {
        std::memset(*useful_table, 0, sizeof(*useful_table));

        // Count missing tiles of main fan
        for (int i = 0; i < main_cnt; ++i) {
            tile_t t = main_tiles[i];
            int n = tile_table[t];
            if (n <= 0) {
                (*useful_table)[t] = true;
            }
        }
    }

    // Shanten of remaining tiles
    int result = regular_shanten_from_table(temp_table, fixed_cnt + main_cnt / 3, useful_table);

    // Shanten = missing tiles of main fan + shanten of remaining tiles
    return (main_cnt - exist_cnt) + result;
}

// Knitted Straight shanten
int knitted_straight_shanten(const tile_t *standing_tiles, intptr_t standing_cnt, useful_table_t *useful_table) {
    if (standing_tiles == nullptr || (standing_cnt != 13 && standing_cnt != 10)) {
        return std::numeric_limits<int>::max();
    }

    // Map to table
    tile_table_t tile_table;
    map_tiles(standing_tiles, standing_cnt, &tile_table);

    int ret = std::numeric_limits<int>::max();

    // When useful tiles are needed, obtain them while calculating shanten
    const auto &standard_knitted_straight = standard_tiles<>::knitted_straight;
    if (useful_table != nullptr) {
        std::memset(*useful_table, 0, sizeof(*useful_table));

        useful_table_t temp_table;

        // Calculate for each of the 6 Knitted Straights
        for (int i = 0; i < 6; ++i) {
            int fixed_cnt = (13 - static_cast<int>(standing_cnt)) / 3;
            int st = regular_shanten_specified(tile_table, standard_knitted_straight[i], 9, fixed_cnt, &temp_table);
            if (st < ret) {  // If shanten is smaller, overwrite data directly
                ret = st;
                std::memcpy(*useful_table, temp_table, sizeof(*useful_table));  // Directly overwrite previous useful tiles data
            }
            else if (st == ret) {  // If shanten of two Knitted Straights is equal, merge useful tiles
                std::transform(std::begin(*useful_table), std::end(*useful_table), std::begin(temp_table),
                    std::begin(*useful_table), [](bool u, bool t) { return u || t; });
            }
        }
    }
    else {
        // Calculate for each of the 6 Knitted Straights
        for (int i = 0; i < 6; ++i) {
            int fixed_cnt = (13 - static_cast<int>(standing_cnt)) / 3;
            int st = regular_shanten_specified(tile_table, standard_knitted_straight[i], 9, fixed_cnt, nullptr);
            if (st < ret) {
                ret = st;
            }
        }
    }

    return ret;
}

#endif

// Whether Knitted Straight is waiting
bool is_knitted_straight_wait(const tile_t *standing_tiles, intptr_t standing_cnt, useful_table_t *waiting_table) {
    if (standing_tiles == nullptr || (standing_cnt != 13 && standing_cnt != 10)) {
        return false;
    }

    // Map standing tiles to table
    tile_table_t tile_table;
    map_tiles(standing_tiles, standing_cnt, &tile_table);

    return is_knitted_straight_wait_from_table(tile_table, standing_cnt, waiting_table);
}

// Whether Knitted Straight is a win
bool is_knitted_straight_win(const tile_t *standing_tiles, intptr_t standing_cnt, tile_t test_tile) {
    useful_table_t waiting_table;
    return (is_knitted_straight_wait(standing_tiles, standing_cnt, &waiting_table)
        && waiting_table[test_tile]);
}

//-------------------------------- Lesser Honors and Knitted Tiles / Greater Honors and Knitted Tiles --------------------------------

// Shanten for 1 Knitted Straight pattern of Lesser Honors and Knitted Tiles
static int honors_and_knitted_tiles_shanten_1(const tile_t *standing_tiles, intptr_t standing_cnt, int which_seq, useful_table_t *useful_table) {
    if (standing_tiles == nullptr || standing_cnt != 13) {
        return std::numeric_limits<int>::max();
    }

    // Map tile types to table
    tile_table_t tile_table;
    map_tiles(standing_tiles, standing_cnt, &tile_table);

    int cnt = 0;

    // Count numbered tiles in Knitted Straight part
    const auto &standard_knitted_straight = standard_tiles<>::knitted_straight;
    for (int i = 0; i < 9; ++i) {
        tile_t t = standard_knitted_straight[which_seq][i];
        int n = tile_table[t];
        if (n > 0) {  // Present, increment count
            ++cnt;
        }
    }

    // Count honor tiles
    const auto &standard_thirteen_orphans = standard_tiles<>::thirteen_orphans;
    for (int i = 6; i < 13; ++i) {
        tile_t t = standard_thirteen_orphans[i];
        int n = tile_table[t];
        if (n > 0) {  // Present, increment count
            ++cnt;
        }
    }

    // Record useful tiles
    if (useful_table != nullptr) {
        std::memset(*useful_table, 0, sizeof(*useful_table));

        // Count missing numbered tiles in Knitted Straight part
        for (int i = 0; i < 9; ++i) {
            tile_t t = standard_knitted_straight[which_seq][i];
            int n = tile_table[t];
            if (n <= 0) {
                (*useful_table)[t] = true;
            }
        }

        // Count missing honor tiles
        for (int i = 6; i < 13; ++i) {
            tile_t t = standard_thirteen_orphans[i];
            int n = tile_table[t];
            if (n <= 0) {
                (*useful_table)[t] = true;
            }
        }
    }

    // Shanten = 13 - matching tile count
    return 13 - cnt;
}

// Lesser Honors and Knitted Tiles shanten
STATIC_IF_NECESSARY int honors_and_knitted_tiles_shanten(const tile_t *standing_tiles, intptr_t standing_cnt, useful_table_t *useful_table) {
    int ret = std::numeric_limits<int>::max();

    // When useful tiles are needed, obtain them while calculating shanten
    if (useful_table != nullptr) {
        std::memset(*useful_table, 0, sizeof(*useful_table));

        useful_table_t temp_table;

        // Calculate for each of the 6 Knitted Straights
        for (int i = 0; i < 6; ++i) {
            int st = honors_and_knitted_tiles_shanten_1(standing_tiles, standing_cnt, i, &temp_table);
            if (st < ret) {  // If shanten is smaller, overwrite data directly
                ret = st;
                std::memcpy(*useful_table, temp_table, sizeof(*useful_table));  // Directly overwrite previous useful tiles data
            }
            else if (st == ret) {  // If shanten of two Knitted Straights is equal, merge useful tiles
                std::transform(std::begin(*useful_table), std::end(*useful_table), std::begin(temp_table),
                    std::begin(*useful_table), [](bool u, bool t) { return u || t; });
            }
        }
    }
    else {
        // Calculate for each of the 6 Knitted Straights
        for (int i = 0; i < 6; ++i) {
            int st = honors_and_knitted_tiles_shanten_1(standing_tiles, standing_cnt, i, nullptr);
            if (st < ret) {
                ret = st;
            }
        }
    }
    return ret;
}

// Whether Lesser Honors and Knitted Tiles is waiting
bool is_honors_and_knitted_tiles_wait(const tile_t *standing_tiles, intptr_t standing_cnt, useful_table_t *waiting_table) {
    // Directly calculate shanten; 0 shanten means waiting
    if (waiting_table == nullptr) {
        return (0 == honors_and_knitted_tiles_shanten(standing_tiles, standing_cnt, nullptr));
    }

    useful_table_t useful_table;
    if (0 == honors_and_knitted_tiles_shanten(standing_tiles, standing_cnt, &useful_table)) {
        std::memcpy(*waiting_table, useful_table, sizeof(*waiting_table));
        return true;
    }
    return false;
}

// Whether Lesser Honors and Knitted Tiles is a win
bool is_honors_and_knitted_tiles_win(const tile_t *standing_tiles, intptr_t standing_cnt, tile_t test_tile) {
    useful_table_t useful_table;
    if (0 == honors_and_knitted_tiles_shanten(standing_tiles, standing_cnt, &useful_table)) {
        return useful_table[test_tile];
    }
    return false;
}

//-------------------------------- Combined Analysis of All Hand Forms --------------------------------

bool is_waiting(const hand_tiles_t &hand_tiles, useful_table_t *useful_table) {
    bool special_waiting = false, regular_waiting = false;
    useful_table_t table_special, table_regular;

    if (hand_tiles.tile_count == 13) {
        if (is_thirteen_orphans_wait(hand_tiles.standing_tiles, 13, &table_special)) {
            special_waiting = true;
        }
        else if (is_honors_and_knitted_tiles_wait(hand_tiles.standing_tiles, 13, &table_special)) {
            special_waiting = true;
        }
        else if (is_seven_pairs_wait(hand_tiles.standing_tiles, 13, &table_special)) {
            special_waiting = true;
        }
        else if (is_knitted_straight_wait(hand_tiles.standing_tiles, 13, &table_special)) {
            special_waiting = true;
        }
    }
    else if (hand_tiles.tile_count == 10) {
        if (is_knitted_straight_wait(hand_tiles.standing_tiles, 10, &table_special)) {
            special_waiting = true;
        }
    }

    if (is_regular_wait(hand_tiles.standing_tiles, hand_tiles.tile_count, &table_regular)) {
        regular_waiting = true;
    }

    if (useful_table != nullptr) {
        if (special_waiting && regular_waiting) {
            std::transform(std::begin(table_special), std::end(table_special), std::begin(table_regular), std::begin(*useful_table),
                [](bool a, bool b) { return a || b; });
        }
        else if (regular_waiting) {
            std::memcpy(*useful_table, table_regular, sizeof(table_regular));
        }
        else if (special_waiting) {
            std::memcpy(*useful_table, table_special, sizeof(table_special));
        }
    }

    return (special_waiting || regular_waiting);
}

#ifdef MAHJONG_ALGORITHM_ENABLE_SHANTEN

//-------------------------------- Enumerate Discard Tiles --------------------------------

// Enumerate discarding a tile once
static bool enum_discard_tile_1(const hand_tiles_t *hand_tiles, tile_t discard_tile, uint8_t form_flag,
    void *context, enum_callback_t enum_callback) {
    enum_result_t result;
    result.discard_tile = discard_tile;
    result.form_flag = FORM_FLAG_REGULAR;
    result.shanten = regular_shanten(hand_tiles->standing_tiles, hand_tiles->tile_count, &result.useful_table);
    if (result.shanten == 0 && result.useful_table[discard_tile]) {  // 0-shanten and discarded tile is useful, adjust to win
        result.shanten = -1;
    }
    if (!enum_callback(context, &result)) {
        return false;
    }

    // Special winning forms only need calculation when standing tiles count is 13
    if (hand_tiles->tile_count == 13) {
        if (form_flag | FORM_FLAG_SEVEN_PAIRS) {
            result.form_flag = FORM_FLAG_SEVEN_PAIRS;
            result.shanten = seven_pairs_shanten(hand_tiles->standing_tiles, hand_tiles->tile_count, &result.useful_table);
            if (result.shanten == 0 && result.useful_table[discard_tile]) {  // 0-shanten and discarded tile is useful, adjust to win
                result.shanten = -1;
            }
            if (!enum_callback(context, &result)) {
                return false;
            }
        }

        if (form_flag | FORM_FLAG_THIRTEEN_ORPHANS) {
            result.form_flag = FORM_FLAG_THIRTEEN_ORPHANS;
            result.shanten = thirteen_orphans_shanten(hand_tiles->standing_tiles, hand_tiles->tile_count, &result.useful_table);
            if (result.shanten == 0 && result.useful_table[discard_tile]) {  // 0-shanten and discarded tile is useful, adjust to win
                result.shanten = -1;
            }
            if (!enum_callback(context, &result)) {
                return false;
            }
        }

        if (form_flag | FORM_FLAG_HONORS_AND_KNITTED_TILES) {
            result.form_flag = FORM_FLAG_HONORS_AND_KNITTED_TILES;
            result.shanten = honors_and_knitted_tiles_shanten(hand_tiles->standing_tiles, hand_tiles->tile_count, &result.useful_table);
            if (result.shanten == 0 && result.useful_table[discard_tile]) {  // 0-shanten and discarded tile is useful, adjust to win
                result.shanten = -1;
            }
            if (!enum_callback(context, &result)) {
                return false;
            }
        }
    }

    // Knitted Straight only needs calculation when standing tiles count is 13 or 10
    if (hand_tiles->tile_count == 13 || hand_tiles->tile_count == 10) {
        if (form_flag | FORM_FLAG_KNITTED_STRAIGHT) {
            result.form_flag = FORM_FLAG_KNITTED_STRAIGHT;
            result.shanten = knitted_straight_shanten(hand_tiles->standing_tiles, hand_tiles->tile_count, &result.useful_table);
            if (result.shanten == 0 && result.useful_table[discard_tile]) {  // 0-shanten and discarded tile is useful, adjust to win
                result.shanten = -1;
            }
            if (!enum_callback(context, &result)) {
                return false;
            }
        }
    }

    return true;
}

// Enumerate discard tiles
void enum_discard_tile(const hand_tiles_t *hand_tiles, tile_t serving_tile, uint8_t form_flag,
    void *context, enum_callback_t enum_callback) {
    // First calculate discarding the drawn/served tile
    if (!enum_discard_tile_1(hand_tiles, serving_tile, form_flag, context, enum_callback)) {
        return;
    }

    if (serving_tile == 0) {
        return;
    }

    // Map standing tiles to table
    tile_table_t tile_table;
    map_tiles(hand_tiles->standing_tiles, hand_tiles->tile_count, &tile_table);

    // Duplicate hand tiles
    hand_tiles_t temp;
    std::memcpy(&temp, hand_tiles, sizeof(temp));

    // Try discarding each standing tile in hand in turn
    const auto &all_tiles = standard_tiles<>::all;
    for (int i = 0; i < 34; ++i) {
        tile_t t = all_tiles[i];
        if (tile_table[t] && t != serving_tile && tile_table[serving_tile] < 4) {
            --tile_table[t];  // Discard this tile
            ++tile_table[serving_tile];  // Serve/draw this tile

            // Convert from table to standing tiles
            table_to_tiles(tile_table, temp.standing_tiles, temp.tile_count);

            // Calculate
            if (!enum_discard_tile_1(&temp, t, form_flag, context, enum_callback)) {
                return;
            }

            // Restore
            --tile_table[serving_tile];
            ++tile_table[t];
        }
    }
}

#endif

}
