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

#include "stringify.h"
#include <cstring>

namespace mahjong {

int parse_hand_tiles(const char *str, size_t len, hand_tiles_t *hand_tiles, tile_t *serving_tile) {
    bool bracket = false;
    pack_t fixed_packs[4];  // Melded packs
    uint8_t pack_cnt = 0;
    tile_t standing_tiles[14];  // Standing tiles, including newly served/drawn tile
    uint8_t tile_cnt = 0, tile_max = 14;  // Standing tile length, maximum length
    uint8_t digit_rank[14];  // Digit sequence
    uint8_t digit_cnt = 0, digit_max = 14;  // Digit sequence length, maximum length
    tile_t temp_tile[14];  // Temporary tiles
    uint8_t temp_cnt = 0, temp_max = 14;  // Temporary tile length, maximum length
    tile_table_t tile_table = { 0 };  // Tile table

    for (size_t k = 0; k < len; ++k) {
        suit_t submit_suit = TILE_SUIT_NONE;  // Suit to submit
        tile_t submit_honor = 0;  // Honor tile to submit

        char ch = str[k];
        switch (ch) {
        case '1':case '2':case '3':
        case '4':case '5':case '6':
        case '7':case '8':case '9':
            // Add to digit sequence
            // If inside brackets, no need to consider temporary tile length
            // If outside brackets, when temporary tiles are already full, adding more is meaningless since they won't be used anyway
            if ((bracket || temp_cnt < temp_max) && digit_cnt < digit_max) {
                digit_rank[digit_cnt++] = ch - '0';
            }
            break;
        case 'm': submit_suit = TILE_SUIT_CHARACTERS; break;
        case 's': submit_suit = TILE_SUIT_BAMBOO; break;
        case 'p': submit_suit = TILE_SUIT_DOTS; break;
        case 'E': submit_honor = TILE_E; break;
        case 'S': submit_honor = TILE_S; break;
        case 'W': submit_honor = TILE_W; break;
        case 'N': submit_honor = TILE_N; break;
        case 'C': submit_honor = TILE_C; break;
        case 'F': submit_honor = TILE_F; break;
        case 'P': submit_honor = TILE_P; break;
        case '[':
            // Start meld
            if (bracket) return PARSE_ERROR_ILLEGAL_CHARACTER;
            if (pack_cnt == 4) return PARSE_ERROR_TOO_MANY_FIXED_PACKS;
            if (digit_cnt != 0) return PARSE_ERROR_SUFFIX;

            // Commit temporary tiles to hand tiles and update tile table
            for (uint8_t i = 0; i < temp_cnt && tile_cnt < tile_max; ++i) {
                tile_t t = temp_tile[i];
                if (++tile_table[t] > 4) return PARSE_ERROR_TILE_COUNT_GREATER_THAN_4;
                standing_tiles[tile_cnt++] = t;
            }

            // If hand has less than 3 tile spaces remaining, this meld is redundant
            if (tile_cnt + 3 > tile_max) return PARSE_ERROR_TOO_MANY_TILES;

            temp_cnt = 0;
            bracket = true;
            temp_max = 5;  // A meld has at most 4 tiles; reserve 1 extra to detect overflow error
            digit_max = 5;
            break;
        case ']': {
            // End meld
            if (!bracket) return PARSE_ERROR_ILLEGAL_CHARACTER;
            if (temp_cnt == 0) return PARSE_ERROR_WRONG_TILES_COUNT_FOR_FIXED_PACK;

            // Parse meld offer/source
            uint8_t o = 0;
            if (digit_cnt == 1) {
                o = digit_rank[0];
                digit_cnt = 0;
            }
            // Redundant digits exist
            if (digit_cnt != 0) return PARSE_ERROR_ILLEGAL_CHARACTER;

            switch (temp_cnt) {
            case 3: {
                // 3 tiles case
                tile_t t0 = temp_tile[0], t1 = temp_tile[1], t2 = temp_tile[2];
                if (o == 0) o = 1;

                // Meld source for 3 tiles can only be 1, 2, or 3
                if (o > 3) return PARSE_ERROR_ILLEGAL_CHARACTER;

                // Identical tiles form a pung
                if (t0 == t1 && t0 == t2) {
                    fixed_packs[pack_cnt++] = make_pack(o, PACK_TYPE_PUNG, t0);
                }
                else {
                    // Non-identical, try to form a chow
                    if (is_numbered_suit_quick(t0)) {
                        if ((t0 + 1 == t1 && t1 + 1 == t2) || (t2 + 1 == t1 && t1 + 1 == t0)) {
                            fixed_packs[pack_cnt++] = make_pack(o, PACK_TYPE_CHOW, t1);
                        }
                        else if ((t0 + 1 == t2 && t2 + 1 == t1) || (t1 + 1 == t2 && t2 + 1 == t0)) {
                            fixed_packs[pack_cnt++] = make_pack(o, PACK_TYPE_CHOW, t2);
                        }
                        else if ((t1 + 1 == t0 && t0 + 1 == t2) || (t2 + 1 == t0 && t0 + 1 == t1)) {
                            fixed_packs[pack_cnt++] = make_pack(o, PACK_TYPE_CHOW, t0);
                        }
                        else {
                            return PARSE_ERROR_CANNOT_MAKE_FIXED_PACK;
                        }
                    }
                    else {
                        return PARSE_ERROR_CANNOT_MAKE_FIXED_PACK;
                    }
                }
                break;
            }
            case 4: {
                // 4 tiles can only be a kong
                tile_t t = temp_tile[0];
                if (t == temp_tile[1] && t == temp_tile[2] && t == temp_tile[3]) {
                    // Meld source for 4 tiles can be 1, 2, 3 or 5, 6, 7
                    if (o > 7 || o == 4) return PARSE_ERROR_ILLEGAL_CHARACTER;

                    fixed_packs[pack_cnt++] = make_pack(o, PACK_TYPE_KONG, t);
                }
                else {
                    return PARSE_ERROR_CANNOT_MAKE_FIXED_PACK;
                }
                break;
            }
            default:
                // Any other tile count is an error
                return PARSE_ERROR_WRONG_TILES_COUNT_FOR_FIXED_PACK;
            }

            // Update tile table
            for (uint8_t i = 0; i < temp_cnt; ++i) {
                if (++tile_table[temp_tile[i]] > 4) return PARSE_ERROR_TILE_COUNT_GREATER_THAN_4;
            }
            temp_cnt = 0;
            bracket = false;
            tile_max -= 3;  // NOTE: Can directly subtract 3 here because the left bracket case already verified sufficient space
            temp_max = tile_max;
            digit_max = tile_max;
            break;
        }
        default: return PARSE_ERROR_ILLEGAL_CHARACTER;
        }

        // Convert digit sequence to numbered tiles and submit to temporary tiles
        if (submit_suit != TILE_SUIT_NONE) {
            if (digit_cnt == 0) return PARSE_ERROR_SUFFIX;
            for (uint8_t i = 0; i < digit_cnt && temp_cnt < temp_max; ++i) {
                temp_tile[temp_cnt++] = make_tile(submit_suit, digit_rank[i]);
            }
            digit_cnt = 0;
        }

        // Submit honor tile
        if (submit_honor != 0) {
            if (digit_cnt != 0) return PARSE_ERROR_SUFFIX;
            if (temp_cnt < temp_max) {
                temp_tile[temp_cnt++] = submit_honor;
            }
        }

        // Full, exit early
        if (!bracket && temp_cnt == temp_max) {
            break;
        }
    }

    if (digit_cnt != 0) return PARSE_ERROR_SUFFIX;

    // Process remaining temporary tiles
    // Commit temporary tiles to hand tiles and update tile table
    for (uint8_t i = 0; i < temp_cnt && tile_cnt < tile_max; ++i) {
        tile_t t = temp_tile[i];
        if (++tile_table[t] > 4) return PARSE_ERROR_TILE_COUNT_GREATER_THAN_4;
        standing_tiles[tile_cnt++] = t;
    }

    // Write back data only when no error
    if (tile_cnt == tile_max) {
        std::memcpy(hand_tiles->standing_tiles, standing_tiles, (tile_max - 1) * sizeof(tile_t));
        hand_tiles->tile_count = tile_max - 1;
        *serving_tile = standing_tiles[tile_max - 1];
    }
    else {
        std::memcpy(hand_tiles->standing_tiles, standing_tiles, tile_cnt * sizeof(tile_t));
        hand_tiles->tile_count = tile_cnt;
        *serving_tile = 0;
    }

    hand_tiles->pack_count = pack_cnt;
    if (pack_cnt != 0) {
        std::memcpy(hand_tiles->fixed_packs, fixed_packs, pack_cnt * sizeof(pack_t));
    }

    return PARSE_NO_ERROR;
}

// Convert hand tiles structure to string
size_t hand_tiles_to_string(const hand_tiles_t *hand_tiles, char *str, size_t max_size) {
    const auto &suffix_chars = tile_name<>::suffix;
    const auto &honor_chars = tile_name<>::honor;

    // Max length per melded pack is 8 (i.e.: [1111m1]), max length per standing tile is 2
    // 0 melds max length = 2 * 13 = 26
    // 1 meld max length = 8 + 2 * 10 = 28
    // 2 melds max length = 8 * 2 + 2 * 7 = 30
    // 3 melds max length = 8 * 3 + 2 * 4 = 32
    // 4 melds max length = 8 * 4 + 2 * 1 = 34
    char buf[35];

    char *p = buf;
    intptr_t pack_count = hand_tiles->pack_count;
    for (intptr_t i = 0; i < pack_count; ++i) {
        pack_t pack = hand_tiles->fixed_packs[i];

        tile_t t = pack_get_tile(pack);
        if (standard_tiles<>::check(t)) {
            uint8_t o = pack_get_offer(pack);
            uint8_t pt = pack_get_type(pack);
            suit_t s = tile_get_suit(t);
            rank_t r = tile_get_rank(t);
            switch (pt) {
            case PACK_TYPE_CHOW:
                *p++ = '[';
                if (s != TILE_SUIT_HONORS) {
                    *p++ = '0' + r - 1;
                    *p++ = '0' + r;
                    *p++ = '0' + r + 1;
                    *p++ = suffix_chars[s - 1];
                    if (o != 1) {
                        *p++ = '0' + o;
                    }
                }
                *p++ = ']';
                break;
            case PACK_TYPE_PUNG:
                *p++ = '[';
                if (s != TILE_SUIT_HONORS) {
                    *p++ = '0' + r;
                    *p++ = '0' + r;
                    *p++ = '0' + r;
                    *p++ = suffix_chars[s - 1];
                }
                else {
                    char ch = honor_chars[r - 1];
                    *p++ = ch;
                    *p++ = ch;
                    *p++ = ch;
                }
                if (o != 1) {
                    *p++ = '0' + o;
                }
                *p++ = ']';
                break;
            case PACK_TYPE_KONG:
                *p++ = '[';
                if (s != TILE_SUIT_HONORS) {
                    *p++ = '0' + r;
                    *p++ = '0' + r;
                    *p++ = '0' + r;
                    *p++ = '0' + r;
                    *p++ = suffix_chars[s - 1];
                }
                else {
                    char ch = honor_chars[r - 1];
                    *p++ = ch;
                    *p++ = ch;
                    *p++ = ch;
                    *p++ = ch;
                }
                if (o != 0) {
                    *p++ = '0' + (is_promoted_kong(pack) ? o | 0x4 : o);
                }
                *p++ = ']';
                break;
            }
        }
    }

    intptr_t max_count = 13 - pack_count * 3;
    intptr_t tile_count = hand_tiles->tile_count;
    if (tile_count > max_count) {
        tile_count = max_count;
    }

    suit_t suit = TILE_SUIT_NONE;
    for (intptr_t i = 0; i < tile_count; ++i) {
        tile_t t = hand_tiles->standing_tiles[i];
        if (standard_tiles<>::check(t)) {
            suit_t s = tile_get_suit(t);
            rank_t r = tile_get_rank(t);

            // Suit changed; write a suffix to end the previous suit
            if (suit != s) {
                if (suit != TILE_SUIT_NONE && suit != TILE_SUIT_HONORS) {
                    *p++ = suffix_chars[suit - 1];
                }
                suit = s;
            }

            if (s != TILE_SUIT_HONORS) {
                // Numbered tile: write digit first
                *p++ = '0' + r;
            }
            else {
                // Honor tile: write corresponding character directly
                *p++ = honor_chars[r - 1];
            }
        }
    }

    // Append unwritten suit suffix
    if (suit != TILE_SUIT_NONE && suit != TILE_SUIT_HONORS) {
        *p++ = suffix_chars[suit - 1];
    }

    size_t res = p - buf;
    if (res > max_size) {
        res = max_size;
    }
    std::memcpy(str, buf, res);

    // Append null terminator
    if (res < max_size) {
        str[res] = '\0';
    }

    return res;
}

}
