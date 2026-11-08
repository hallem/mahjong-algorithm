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

#ifndef __MAHJONG_ALGORITHM__SHANTEN_H__
#define __MAHJONG_ALGORITHM__SHANTEN_H__

#include "tile.h"

#define MAHJONG_ALGORITHM_ENABLE_SHANTEN

namespace mahjong {

/**
 * @brief Convert packs to tiles
 *
 * @param [in] packs Packs
 * @param [in] pack_cnt Pack count
 * @param [out] tiles Tiles array
 * @param [in] tile_cnt Maximum tile count
 * @return intptr_t Actual tile count
 */
intptr_t packs_to_tiles(const pack_t *packs, intptr_t pack_cnt, tile_t *tiles, intptr_t tile_cnt);

/**
 * @brief Map tiles to a tile table (histogram)
 *
 * @param [in] tiles Tiles
 * @param [in] cnt Tile count
 * @param [out] tile_table Tile count table
 */
void map_tiles(const tile_t *tiles, intptr_t cnt, tile_table_t *tile_table);

/**
 * @brief Map hand tiles to a tile table
 *
 * @param [in] hand_tiles Hand tiles
 * @param [out] tile_table Tile count table
 * @return bool Whether hand tiles structure is valid (i.e. satisfies: meld_count * 3 + standing_tile_count = 13)
 */
bool map_hand_tiles(const hand_tiles_t *hand_tiles, tile_table_t *tile_table);

/**
 * @brief Convert tile table to tiles array
 *
 * @param [in] tile_table Tile count table
 * @param [out] tiles Tiles array
 * @param [in] max_cnt Maximum tile count
 * @return intptr_t Actual tile count
 */
intptr_t table_to_tiles(const tile_table_t &tile_table, tile_t *tiles, intptr_t max_cnt);

/**
 * @brief Useful tiles flag table type
 */
typedef bool useful_table_t[TILE_TABLE_SIZE];

/**
 * @addtogroup shanten
 * @{
 */

/**
 * @addtogroup regular
 * @{
 */

#ifdef MAHJONG_ALGORITHM_ENABLE_SHANTEN

/**
 * @brief Regular winning form shanten
 *
 * @param [in] standing_tiles Standing tiles
 * @param [in] standing_cnt Standing tile count
 * @param [out] useful_table Useful tiles flag table (can be null)
 * @return int Shanten number
 */
int regular_shanten(const tile_t *standing_tiles, intptr_t standing_cnt, useful_table_t *useful_table);

#endif

/**
 * @brief Whether regular winning form is waiting (tenpai)
 *
 * @param [in] standing_tiles Standing tiles
 * @param [in] standing_cnt Standing tile count
 * @param [out] waiting_table Waiting tiles flag table (can be null)
 * @return bool Whether waiting
 */
bool is_regular_wait(const tile_t *standing_tiles, intptr_t standing_cnt, useful_table_t *waiting_table);

/**
 * @brief Whether regular winning form is a winning hand
 *
 * @param [in] standing_tiles Standing tiles
 * @param [in] standing_cnt Standing tile count
 * @param [in] test_tile Tile to test
 * @return bool Whether winning
 */
bool is_regular_win(const tile_t *standing_tiles, intptr_t standing_cnt, tile_t test_tile);

/**
 * end group
 * @}
 */

/**
 * @addtogroup seven_pairs
 * @{
 */

#ifdef MAHJONG_ALGORITHM_ENABLE_SHANTEN

/**
 * @brief Seven Pairs shanten
 *
 * @param [in] standing_tiles Standing tiles
 * @param [in] standing_cnt Standing tile count
 * @param [out] useful_table Useful tiles flag table (can be null)
 * @return int Shanten number
 */
int seven_pairs_shanten(const tile_t *standing_tiles, intptr_t standing_cnt, useful_table_t *useful_table);

#endif

/**
 * @brief Whether Seven Pairs is waiting (tenpai)
 *
 * @param [in] standing_tiles Standing tiles
 * @param [in] standing_cnt Standing tile count
 * @param [out] waiting_table Waiting tiles flag table (can be null)
 * @return bool Whether waiting
 */
bool is_seven_pairs_wait(const tile_t *standing_tiles, intptr_t standing_cnt, useful_table_t *waiting_table);

/**
 * @brief Whether Seven Pairs is a winning hand
 *
 * @param [in] standing_tiles Standing tiles
 * @param [in] standing_cnt Standing tile count
 * @param [in] test_tile Tile to test
 * @return bool Whether winning
 */
bool is_seven_pairs_win(const tile_t *standing_tiles, intptr_t standing_cnt, tile_t test_tile);

/**
 * end group
 * @}
 */

/**
 * @addtogroup thirteen_orphans
 * @{
 */

#ifdef MAHJONG_ALGORITHM_ENABLE_SHANTEN

/**
 * @brief Thirteen Orphans shanten
 *
 * @param [in] standing_tiles Standing tiles
 * @param [in] standing_cnt Standing tile count
 * @param [out] useful_table Useful tiles flag table (can be null)
 * @return int Shanten number
 */
int thirteen_orphans_shanten(const tile_t *standing_tiles, intptr_t standing_cnt, useful_table_t *useful_table);

#endif

/**
 * @brief Whether Thirteen Orphans is waiting (tenpai)
 *
 * @param [in] standing_tiles Standing tiles
 * @param [in] standing_cnt Standing tile count
 * @param [out] waiting_table Waiting tiles flag table (can be null)
 * @return bool Whether waiting
 */
bool is_thirteen_orphans_wait(const tile_t *standing_tiles, intptr_t standing_cnt, useful_table_t *waiting_table);

/**
 * @brief Whether Thirteen Orphans is a winning hand
 *
 * @param [in] standing_tiles Standing tiles
 * @param [in] standing_cnt Standing tile count
 * @param [in] test_tile Tile to test
 * @return bool Whether winning
 */
bool is_thirteen_orphans_win(const tile_t *standing_tiles, intptr_t standing_cnt, tile_t test_tile);

/**
 * end group
 * @}
 */

/**
 * @addtogroup knitted_straight
 * @{
 */

#ifdef MAHJONG_ALGORITHM_ENABLE_SHANTEN

/**
 * @brief Knitted Straight shanten
 *
 * @param [in] standing_tiles Standing tiles
 * @param [in] standing_cnt Standing tile count
 * @param [out] useful_table Useful tiles flag table (can be null)
 * @return int Shanten number
 */
int knitted_straight_shanten(const tile_t *standing_tiles, intptr_t standing_cnt, useful_table_t *useful_table);

#endif

/**
 * @brief Whether Knitted Straight is waiting (tenpai)
 *
 * @param [in] standing_tiles Standing tiles
 * @param [in] standing_cnt Standing tile count
 * @param [out] waiting_table Waiting tiles flag table (can be null)
 * @return bool Whether waiting
 */
bool is_knitted_straight_wait(const tile_t *standing_tiles, intptr_t standing_cnt, useful_table_t *waiting_table);

/**
 * @brief Whether Knitted Straight is a winning hand
 *
 * @param [in] standing_tiles Standing tiles
 * @param [in] standing_cnt Standing tile count
 * @param [in] test_tile Tile to test
 * @return bool Whether winning
 */
bool is_knitted_straight_win(const tile_t *standing_tiles, intptr_t standing_cnt, tile_t test_tile);

/**
 * end group
 * @}
 */

/**
 * @addtogroup honors_and_knitted_tiles
 * @{
 */

#ifdef MAHJONG_ALGORITHM_ENABLE_SHANTEN

/**
 * @brief Lesser Honors and Knitted Tiles shanten
 *
 * @param [in] standing_tiles Standing tiles
 * @param [in] standing_cnt Standing tile count
 * @param [out] useful_table Useful tiles flag table (can be null)
 * @return int Shanten number
 */
int honors_and_knitted_tiles_shanten(const tile_t *standing_tiles, intptr_t standing_cnt, useful_table_t *useful_table);

#endif

/**
 * @brief Whether Lesser Honors and Knitted Tiles is waiting (tenpai)
 *
 * @param [in] standing_tiles Standing tiles
 * @param [in] standing_cnt Standing tile count
 * @param [out] waiting_table Waiting tiles flag table (can be null)
 * @return bool Whether waiting
 */
bool is_honors_and_knitted_tiles_wait(const tile_t *standing_tiles, intptr_t standing_cnt, useful_table_t *waiting_table);

/**
 * @brief Whether Lesser Honors and Knitted Tiles is a winning hand
 *
 * @param [in] standing_tiles Standing tiles
 * @param [in] standing_cnt Standing tile count
 * @param [in] test_tile Tile to test
 * @return bool Whether winning
 */
bool is_honors_and_knitted_tiles_win(const tile_t *standing_tiles, intptr_t standing_cnt, tile_t test_tile);

/**
 * end group
 * @}
 */

/**
 * @brief Whether hand is waiting (tenpai)
 *
 * @param [in] hand_tiles Hand tiles structure
 * @param [out] useful_table Useful tiles flag table (can be null)
 * @return bool Whether waiting
 */
bool is_waiting(const hand_tiles_t &hand_tiles, useful_table_t *useful_table);

/**
 * end group
 * @}
 */

#ifdef MAHJONG_ALGORITHM_ENABLE_SHANTEN

/**
 * @name form flags
 * @{
 *  Winning forms
 */
#define FORM_FLAG_REGULAR                   0x01  ///< Regular winning form
#define FORM_FLAG_SEVEN_PAIRS               0x02  ///< Seven Pairs
#define FORM_FLAG_THIRTEEN_ORPHANS          0x04  ///< Thirteen Orphans
#define FORM_FLAG_HONORS_AND_KNITTED_TILES  0x08  ///< Lesser Honors and Knitted Tiles
#define FORM_FLAG_KNITTED_STRAIGHT          0x10  ///< Knitted Straight
#define FORM_FLAG_ALL                       0xFF  ///< All winning forms
/**
 * @}
 */

/**
 * @brief Calculation result for enumerating discard tiles
 */
struct enum_result_t {
    tile_t discard_tile;                    ///< Discard this tile
    uint8_t form_flag;                      ///< Winning form
    int shanten;                            ///< Shanten number
    useful_table_t useful_table;            ///< Useful tiles flag table
};

/**
 * @brief Callback function for enumerating discard tiles
 *
 * @param [in] context User context passed through from enum_discard_tile unmodified
 * @param [in] result Calculation result
 * @retval true Continue enumeration
 * @retval false Terminate enumeration
 */
typedef bool (*enum_callback_t)(void *context, const enum_result_t *result);

/**
 * @brief Enumerate discard tiles
 *
 * @param [in] hand_tiles Hand tiles structure
 * @param [in] serving_tile Serving/drawn tile (can be 0, which only calculates hand tiles information)
 * @param [in] form_flag Which winning forms to calculate
 * @param [in] context User-defined parameter passed back to callback unmodified
 * @param [in] enum_callback Callback function
 */
void enum_discard_tile(const hand_tiles_t *hand_tiles, tile_t serving_tile, uint8_t form_flag,
    void *context, enum_callback_t enum_callback);

#endif

}

/**
 * end group
 * @}
 */

#endif
