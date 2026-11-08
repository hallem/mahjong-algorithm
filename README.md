mahjong-algorithm - Various Algorithms for Mahjong
=========

## Overview
- Although these algorithms are written in C++, they do not use C++ container classes and can be easily ported to other programming languages.
- `fan_calculator`: Fan (points/scoring) calculation.
- `stringify`: String conversion routines.
- See `unit_test.cpp` for details.

## Common Terminology
- Chow: A sequence of 3 consecutive numbered tiles of the same suit.
- Pung: Three identical tiles. Claimed from a discard is a melded/open pung; kept in hand without claiming is a concealed/closed pung. A kong is also treated as a pung (melded kong as melded pung, concealed kong as concealed pung).
- Set / Meld: General term for chows and pungs.
- Eye / Head / Pair: The isolated pair in a standard winning hand.
- Regular Winning Form: A winning hand form consisting of 4 sets and 1 pair.
- Special Winning Forms: Winning hand forms not consisting of 4 sets and 1 pair. Under Chinese Official / Mahjong Competition Rules (MCR), special winning forms include Seven Pairs, Thirteen Orphans, Lesser Honors and Knitted Tiles, etc.
- Concealed Hand: Also called men-zen-qing; the state of having made no chows, pungs, or melded kongs. Special winning forms are necessarily in a concealed state. Although concealed kongs do not break the concealed hand condition, they reveal information that the hand is not a special winning form.
- Melds: General term for chows, pungs, and kongs claiming tiles discarded by other players to complete sets. Usually excludes concealed kongs. When concealed kongs are included, they are called concealed melds, while chows, pungs, and melded kongs are called open/melded melds.
- Standing Tiles: Tiles in hand excluding melded chows, pungs, and kongs.
- Hand Tiles: The entire hand including standing tiles and melded chows, pungs, and kongs; sometimes refers only to standing tiles.
- Ready Hand / Waiting: The state of needing only one tile to complete a winning hand.
- 1-Shanten: The state of being one tile away from ready/waiting (also called 1-xiangting). Similarly, 2-shanten, 3-shanten, N-shanten.
- Shanten Number: The minimum number of tiles needed to reach the ready/waiting state.
- Useful Tiles: Tiles that reduce the shanten number (advancing tiles).
- Improvement Tiles: Tiles that increase the number of useful tiles (broadening the waiting/advancing tile range).
- Pair: Two identical tiles. An eye/head must be a pair, but a pair is not necessarily the eye/head.
- Two-sided Wait: (1) Two consecutive numbered tiles of the same suit (e.g., 45m), which can form a chow with adjacent tiles on either side. (2) Abbreviation for two-sided proto-run.
- Closed Wait / Inside Wait: (1) Two numbered tiles of the same suit separated by 1 rank (e.g., 57s), which can only form a chow with the middle tile; this middle tile is called the closed wait tile. (2) Abbreviation for closed wait proto-run. (3) The "Closed Wait" fan in MCR.
- Edge Wait: (1) Two consecutive numbered tiles at boundary positions (e.g., 12m or 89s), which can only form a chow with a tile on one side (3m or 7s); this 3m or 7s is called the edge wait tile. (2) Abbreviation for edge wait proto-run. (3) The "Edge Wait" fan in MCR.
- Proto-run / Incomplete Set: Two tiles that need one more tile to complete a set. Forms include two-sided proto-runs, closed-wait proto-runs, edge-wait proto-runs, and sometimes a pair can be viewed as an incomplete pung.
- Complex Incomplete Set: An incomplete set formed by 3 or more tiles, such as consecutive closed waits, two-sided wait with a pair, closed wait with a pair, edge wait with a pair, etc.
- Dual Wait / Double-Pair Wait: When waiting for a win, all other tiles have formed sets and two pairs remain; drawing a third tile to match either pair completes a pung and wins, with the remaining pair serving as the eye/head.

## String Format
- Numbered tiles: Characters (Wan) = `m`, Bamboos (Tiao/Sou) = `s`, Dots (Bing/Pin) = `p`. Suffixes use lowercase letters; consecutive tiles of the same suit can share a single suffix, e.g., `123m`, `678s`, etc.
- Honor tiles: East, South, West, North = `ESWN`; Red, Green, White Dragons = `CFP` (Chun, Fa, Po). Uses uppercase letters. Also compatible with Tenhou-style suffix `z`, where `567z` corresponds to Red, Green, White Dragons by Chinese convention.
- Chows, pungs, and kongs are enclosed in square brackets `[]`, optionally using comma + digit to indicate the tile source:
  - Chow: Since a player can only chow from the player to the left (upper seat), the digit after comma indicates which tile in the sequence was discarded by the upper seat. For example: `[567m,2]` means 57m chowed the 6m (the 2nd tile). If omitted, defaults to the 1st tile.
  - Pung: The digit after comma indicates who discarded the tile, based on intuitive left-to-right order: 1 for upper seat (left), 2 for opposite seat (across), 3 for lower seat (right). For example: `[999s,3]` means pung 9s from the lower seat. If omitted, defaults to upper seat.
  - Kong: Similar to pung, but if no digit is specified, it is treated as a concealed kong. For example: `[SSSS]` means concealed kong of South; `[8888p,1]` means melded kong of 8p from upper seat. When the digit is 5, 6, or 7, it represents an added kong (promoted kong). For example: `[1111s,6]` means after punging 1s from the opposite seat, drew another 1s to promote to kong.
- Examples:
  - `[EEEE][CCCC][FFFF][PPPP]NN`
  - `1112345678999s9s`
  - `[WWWW,1][444s]45m678pFF6m`
  - `[EEEE]288s349pSCFF2p`
  - `[123p,1][345s,2][999s,3]6m6pEW1m`
  - `356m18s1579pWNFF9p`
