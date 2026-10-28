/*
 * File: marbletypes.cpp
 * ---------------------
 * CS106B Marble Solitaire
 * This provided file implements the types and helper functions declared in
 * marbletypes.h. You should not modify this file.
 *
 * @author Cynthia Lee and Marty Stepp (original version, 2017)
 * @version 2026/09 updated for the Qt-based CS106 library
 */
#include "marbletypes.h"
#include <fstream>
#include <sstream>
#include "error.h"
#include "filelib.h"
#include "strlib.h"
using namespace std;

/* Returns true if this square of a 7x7 grid is one of the corner squares that is not on the board. */
static bool isCornerSquare(int row, int col) {
    return (row < 2 || row > 4) && (col < 2 || col > 4);
}

/*
 * Support code shared by this file and the graphical interface. It works on the
 * board as a set of bits, one per hole, and is not meant to be used by student code.
 */
namespace MarbleInternal {

/* The GUI installs this hook while your solver is running so that it can show
 * progress, animate the search, and stop the search when asked. */
void (*compressHook)(const Grid<Marble>& board) = nullptr;

struct Jump {
    int start, over, end;   // hole numbers
};

/* Every possible jump, as hole numbers. Holes are numbered 0 to 32. */
extern const Jump kJumps[] = {   // extern: const would otherwise be private to this file
    { 0, 1, 2}, { 0, 3, 8}, { 1, 4, 9}, { 2, 5,10}, { 2, 1, 0}, { 3, 4, 5},
    { 3, 8,15}, { 4, 9,16}, { 5,10,17}, { 5, 4, 3}, { 6, 7, 8}, { 6,13,20},
    { 7, 8, 9}, { 7,14,21}, { 8, 3, 0}, { 8, 9,10}, { 8,15,22}, { 8, 7, 6},
    { 9, 4, 1}, { 9,10,11}, { 9,16,23}, { 9, 8, 7}, {10, 5, 2}, {10,11,12},
    {10,17,24}, {10, 9, 8}, {11,18,25}, {11,10, 9}, {12,19,26}, {12,11,10},
    {13,14,15}, {14,15,16}, {15, 8, 3}, {15,16,17}, {15,22,27}, {15,14,13},
    {16, 9, 4}, {16,17,18}, {16,23,28}, {16,15,14}, {17,10, 5}, {17,18,19},
    {17,24,29}, {17,16,15}, {18,17,16}, {19,18,17}, {20,13, 6}, {20,21,22},
    {21,14, 7}, {21,22,23}, {22,15, 8}, {22,23,24}, {22,27,30}, {22,21,20},
    {23,16, 9}, {23,24,25}, {23,28,31}, {23,22,21}, {24,17,10}, {24,25,26},
    {24,29,32}, {24,23,22}, {25,18,11}, {25,24,23}, {26,19,12}, {26,25,24},
    {27,22,15}, {27,28,29}, {28,23,16}, {29,24,17}, {29,28,27}, {30,27,22},
    {30,31,32}, {31,28,23}, {32,29,24}, {32,31,30}
};
extern const int kNumJumps = sizeof(kJumps) / sizeof(kJumps[0]);

static Vector<GridLocation> makeHoleLocations() {
    Vector<GridLocation> holes;
    for (int row = 0; row < BOARD_SIZE; row++) {
        for (int col = 0; col < BOARD_SIZE; col++) {
            if (!isCornerSquare(row, col)) {
                holes.add(GridLocation(row, col));
            }
        }
    }
    return holes;
}

/* Returns the locations of the holes, in hole-number order. */
static const Vector<GridLocation>& holeLocations() {
    static const Vector<GridLocation> holes = makeHoleLocations();   // built once, thread-safe
    return holes;
}

GridLocation holeLocation(int hole) {
    return holeLocations()[hole];
}

/* Returns the hole number at the given location, or -1 if it is not a hole. */
int holeNumber(const GridLocation& loc) {
    const Vector<GridLocation>& holes = holeLocations();
    for (int i = 0; i < holes.size(); i++) {
        if (holes[i] == loc) {
            return i;
        }
    }
    return -1;
}

/* Returns the jump between the given holes, or nullptr if there is none. */
const Jump* findJump(int start, int end) {
    for (int i = 0; i < kNumJumps; i++) {
        if (kJumps[i].start == start && kJumps[i].end == end) {
            return &kJumps[i];
        }
    }
    return nullptr;
}

/* One bit per hole: 1 for a marble, 0 for empty. */
uint64_t boardBits(const Grid<Marble>& board) {
    uint64_t bits = 0;
    const Vector<GridLocation>& holes = holeLocations();
    for (int i = 0; i < holes.size(); i++) {
        if (board[holes[i]] == MARBLE_OCCUPIED) {
            bits |= uint64_t(1) << i;
        }
    }
    return bits;
}

} // namespace MarbleInternal

using namespace MarbleInternal;

BoardEncoding compressBoard(const Grid<Marble>& board) {
    // 33 holes need more than 32 bits, so a 64-bit integer is used. (The 2017
    // version used 32 bits, which made two different holes share a bit.)
    BoardEncoding encoding = { boardBits(board) };
    if (compressHook) {
        compressHook(board);
    }
    return encoding;
}

Grid<Marble> fullBoard() {
    Grid<Marble> board(BOARD_SIZE, BOARD_SIZE);
    for (int row = 0; row < BOARD_SIZE; row++) {
        for (int col = 0; col < BOARD_SIZE; col++) {
            board[row][col] = isCornerSquare(row, col) ? MARBLE_INVALID : MARBLE_OCCUPIED;
        }
    }
    board[BOARD_CENTER] = MARBLE_EMPTY;
    return board;
}

Grid<Marble> boardFromStrings(const Vector<string>& rows) {
    if (rows.size() != BOARD_SIZE) {
        error("A board must have " + integerToString(BOARD_SIZE) + " rows, but found "
              + integerToString(rows.size()));
    }
    Grid<Marble> board(BOARD_SIZE, BOARD_SIZE);
    for (int row = 0; row < BOARD_SIZE; row++) {
        string line = trimEnd(rows[row]);
        if (line.length() != BOARD_SIZE) {
            error("Row " + integerToString(row) + " of the board must have "
                  + integerToString(BOARD_SIZE) + " characters: \"" + line + "\"");
        }
        for (int col = 0; col < BOARD_SIZE; col++) {
            char ch = line[col];
            if (isCornerSquare(row, col) != (ch == 'X')) {
                error("Square " + GridLocation(row, col).toString() + " of the board should be "
                      + (isCornerSquare(row, col) ? "'X'" : "'O' or '-'") + ", but is '" + ch + "'");
            }
            if (ch == 'O') {
                board[row][col] = MARBLE_OCCUPIED;
            } else if (ch == '-') {
                board[row][col] = MARBLE_EMPTY;
            } else if (ch == 'X') {
                board[row][col] = MARBLE_INVALID;
            } else {
                error(string("Unexpected character '") + ch + "' in row " + integerToString(row)
                      + " of the board");
            }
        }
    }
    return board;
}

Grid<Marble> loadBoard(const string& filename) {
    ifstream in;
    if (!openFile(in, filename)) {
        error("Cannot open board file \"" + filename + "\"");
    }
    Vector<string> rows;
    string line;
    while (getline(in, line)) {
        line = trim(line);
        // skip comments and blank lines; also skip the "7" row/column count
        // lines found in board files from the 2017 version of the assignment
        if (line.empty() || line[0] == '#' || stringIsInteger(line)) {
            continue;
        }
        rows.add(line);
    }
    try {
        return boardFromStrings(rows);
    } catch (const ErrorException& e) {
        error("Board file \"" + getTail(filename) + "\" is not valid: " + e.getMessage());
    }
    return Grid<Marble>();   // not reached
}

int countMarbles(const Grid<Marble>& board) {
    int count = 0;
    for (Marble m : board) {
        if (m == MARBLE_OCCUPIED) {
            count++;
        }
    }
    return count;
}

string boardToString(const Grid<Marble>& board) {
    ostringstream out;
    for (int row = 0; row < board.numRows(); row++) {
        for (int col = 0; col < board.numCols(); col++) {
            out << board[row][col];
        }
        out << endl;
    }
    return out.str();
}

string checkSolution(Grid<Marble> board, const Vector<Move>& moves) {
    uint64_t bits = boardBits(board);
    for (int i = 0; i < moves.size(); i++) {
        const Move& m = moves[i];
        string where = "Move #" + integerToString(i + 1) + " (" + m.start.toString() + " -> "
                + m.end.toString() + ") ";
        int start = holeNumber(m.start), end = holeNumber(m.end);
        if (start < 0 || end < 0) {
            return where + "starts or ends outside of the board.";
        }
        const Jump* jump = findJump(start, end);
        if (!jump) {
            return where + "is not a jump of exactly two squares up, down, left, or right.";
        }
        if (!(bits >> start & 1)) {
            return where + "starts from a hole that has no marble in it.";
        }
        if (!(bits >> jump->over & 1)) {
            return where + "jumps over a hole that has no marble in it.";
        }
        if (bits >> end & 1) {
            return where + "lands on a hole that already has a marble in it.";
        }
        bits ^= (uint64_t(1) << start) | (uint64_t(1) << jump->over) | (uint64_t(1) << end);
    }
    int count = 0;
    for (; bits != 0; bits &= bits - 1) {
        count++;
    }
    if (count != 1) {
        return "After all " + integerToString(moves.size()) + " moves, the board has "
                + integerToString(count) + " marbles left, so it is not solved.";
    }
    return "";
}

ostream& operator <<(ostream& out, Marble marble) {
    switch (marble) {
        case MARBLE_EMPTY:    return out << '-';
        case MARBLE_OCCUPIED: return out << 'O';
        case MARBLE_INVALID:  return out << 'X';
    }
    return out << '?';
}

bool operator ==(const Move& m1, const Move& m2) {
    return m1.start == m2.start && m1.end == m2.end;
}

bool operator !=(const Move& m1, const Move& m2) {
    return !(m1 == m2);
}

bool operator <(const Move& m1, const Move& m2) {
    if (m1.start != m2.start) {
        return m1.start < m2.start;
    }
    return m1.end < m2.end;
}

int hashCode(const Move& move) {
    return hashCode(move.start, move.end);
}

ostream& operator <<(ostream& out, const Move& move) {
    return out << move.start << " -> " << move.end;
}

bool operator ==(const BoardEncoding& e1, const BoardEncoding& e2) {
    return e1.bits == e2.bits;
}

bool operator !=(const BoardEncoding& e1, const BoardEncoding& e2) {
    return e1.bits != e2.bits;
}

bool operator <(const BoardEncoding& e1, const BoardEncoding& e2) {
    return e1.bits < e2.bits;
}

int hashCode(const BoardEncoding& encoding) {
    // mix both halves; the library's hashCode for integers keeps only the low 32 bits
    return hashCode(static_cast<int>(encoding.bits), static_cast<int>(encoding.bits >> 32));
}

ostream& operator <<(ostream& out, const BoardEncoding& encoding) {
    return out << "BoardEncoding(0x" << std::hex << encoding.bits << std::dec << ")";
}
