/*
 * File: marbletypes.h
 * -------------------
 * CS106B Marble Solitaire
 * This provided file declares the types and helper functions shared by the
 * MarbleBoard class, your solver (marbles.cpp), the test cases, and the
 * graphical interface:
 *
 *   Marble          the state of one square on the board
 *   Move            one jump, from a start location to an end location
 *   BoardEncoding   a compact stand-in for a whole board (see MarbleBoard::encode)
 *
 * plus some functions for creating, loading, and printing boards.
 *
 * You should not modify this file. Your code must work with it unchanged.
 *
 * @author Cynthia Lee and Marty Stepp (original version, 2017)
 * @version 2026/09 updated for the Qt-based CS106 library
 */
#pragma once
#include <cstdint>
#include <iostream>
#include <string>
#include "grid.h"
#include "gridlocation.h"
#include "vector.h"

/*
 * The board is a 7x7 grid. The four 2x2 corners of the grid are not part of
 * the board, which leaves 33 holes arranged in a plus shape:
 *
 *       X X O O O X X        O = marble
 *       X X O O O X X        - = empty hole
 *       O O O O O O O        X = not part of the board
 *       O O O - O O O
 *       O O O O O O O
 *       X X O O O X X
 *       X X O O O X X
 *
 * A game is won when exactly one marble is left, in any hole.
 */
const int BOARD_SIZE = 7;
const GridLocation BOARD_CENTER(3, 3);

/*
 * The state of one square of the board grid.
 */
enum Marble {
    MARBLE_EMPTY,       // a hole with no marble in it
    MARBLE_OCCUPIED,    // a hole with a marble in it
    MARBLE_INVALID      // not part of the board (the four corners of the grid)
};

/*
 * A Move jumps the marble at the start location over the marble next to it
 * and into the empty hole at the end location, which is two squares away in
 * the same row or column. The jumped-over marble is removed from the board.
 *
 * Moves can be created like this:
 *
 *     Move m = { {1, 3}, {3, 3} };     // jump from row 1, col 3 down to row 3, col 3
 *     Move m2;
 *     m2.start = {3, 5};
 *     m2.end = {3, 3};
 *
 * Moves can be compared with == and stored in a Set, HashSet, or Map.
 */
struct Move {
    GridLocation start;
    GridLocation end;
};

/*
 * A BoardEncoding is a small value that stands in for an entire board.
 * Call MarbleBoard::encode (in MarbleBoard.h) to get one. Two boards with marbles in the same
 * places always have equal encodings, and boards that differ always have
 * different encodings. A BoardEncoding uses much less memory than a
 * Grid<Marble>, so you can store hundreds of thousands of them in a Set or
 * HashSet.
 *
 * You don't need to know what is inside a BoardEncoding. (It is one bit per
 * hole, packed into a 64-bit integer.)
 */
struct BoardEncoding {
    uint64_t bits;
};

/*
 * Returns the BoardEncoding for the given board.
 */
BoardEncoding compressBoard(const Grid<Marble>& board);

/*
 * Returns the standard starting board: a marble in every hole except the center.
 */
Grid<Marble> fullBoard();

/*
 * Builds a board from 7 strings of 7 characters each, using the characters
 * 'O' for a marble, '-' for an empty hole, and 'X' for a corner square that is
 * not part of the board. This is handy for writing test cases, e.g.
 *
 *     Grid<Marble> board = boardFromStrings({
 *         "XX---XX",
 *         "XX---XX",
 *         "-------",
 *         "-OO----",
 *         "-------",
 *         "XX---XX",
 *         "XX---XX"
 *     });
 *
 * Raises an error if the strings do not describe a valid board.
 */
Grid<Marble> boardFromStrings(const Vector<std::string>& rows);

/*
 * Reads a board from a file such as "marbles/boards/cross.txt". Lines that
 * start with '#' are comments; the rest of the file is in the same format
 * as boardFromStrings. Raises an error if the file is missing or malformed.
 */
Grid<Marble> loadBoard(const std::string& filename);

/*
 * Returns the number of marbles on the board.
 */
int countMarbles(const Grid<Marble>& board);

/*
 * Returns a multi-line picture of the board, in the same format as
 * boardFromStrings, which can be useful when debugging, e.g.
 *     cout << boardToString(board) << endl;
 */
std::string boardToString(const Grid<Marble>& board);

/*
 * Checks a proposed solution. Starting from the given board, makes each move
 * in order. Returns the empty string if every move is legal and the last move
 * leaves exactly one marble on the board. Otherwise returns a message describing the
 * first problem found. The test cases and the GUI use this to check the moves
 * returned by solvePuzzle.
 */
std::string checkSolution(Grid<Marble> board, const Vector<Move>& moves);

/*
 * Operators that let Marble, Move, and BoardEncoding values be printed,
 * compared, and stored in collections. You don't need to call these directly.
 */
std::ostream& operator <<(std::ostream& out, Marble marble);
bool operator ==(const Move& m1, const Move& m2);
bool operator !=(const Move& m1, const Move& m2);
bool operator <(const Move& m1, const Move& m2);
int hashCode(const Move& move);
std::ostream& operator <<(std::ostream& out, const Move& move);
bool operator ==(const BoardEncoding& e1, const BoardEncoding& e2);
bool operator !=(const BoardEncoding& e1, const BoardEncoding& e2);
bool operator <(const BoardEncoding& e1, const BoardEncoding& e2);
int hashCode(const BoardEncoding& encoding);
std::ostream& operator <<(std::ostream& out, const BoardEncoding& encoding);
