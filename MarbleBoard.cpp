/*
 * File: MarbleBoard.cpp
 * ---------------------
 * TODO: Replace this comment with a description of your program, your name,
 * and your partner's name if you worked as a pair. Cite any sources of help.
 */
#include "MarbleBoard.h"
#include "set.h"
#include "SimpleTest.h"
using namespace std;

/*
 * TODO: Replace this comment with a description of this function
 * and how you implemented it.
 */
Vector<Move> MarbleBoard::findPossibleMoves() const {
    Vector<Move> moves;
    for (int row = 0; row < _grid.numRows(); row++) {
        for (int col = 0; col < _grid.numCols(); col++) {
            if (_grid[row][col] == MARBLE_OCCUPIED) {
                GridLocation start(row, col);

                GridLocation possible[4] = {
                    {row - 2, col},
                    {row + 2, col},
                    {row, col - 2},
                    {row, col + 2}
                };

                GridLocation possibleMarbles[4] = {
                    {row - 1, col},
                    {row + 1, col},
                    {row, col - 1},
                    {row, col + 1}
                };

                for (int i = 0; i < 4; i++) {
                    if (_grid.inBounds(possible[i])) {
                        if (_grid[possible[i]] == MARBLE_EMPTY && _grid[possibleMarbles[i]] == MARBLE_OCCUPIED) {
                            Move valid = {start, possible[i]};
                            moves.add(valid);
                        }
                    }
                }
            }
        }
    }
    return moves;
}

/*
 * TODO: Replace this comment with a description of this function
 * and how you implemented it.
 */
void MarbleBoard::makeMove(Move move) {
    _grid[move.end] = MARBLE_OCCUPIED;
    _grid[move.start] = MARBLE_EMPTY;

    GridLocation changed = move.start;

    if (move.end.col - move.start.col == 2) {
        changed.col += 1;
    }
    else if (move.end.col - move.start.col == -2) {
        changed.col -= 1;
    }
    else if (move.end.row - move.start.row == 2) {
        changed.row += 1;
    }
    else if (move.end.row - move.start.row == -2) {
        changed.row -= 1;
    }
    _grid[changed] = MARBLE_EMPTY;
    _numMarbles--;
}

/*
 * TODO: Replace this comment with a description of this function
 * and how you implemented it.
 */
void MarbleBoard::undoMove(Move move) {
    _grid[move.end] = MARBLE_EMPTY;
    _grid[move.start] =  MARBLE_OCCUPIED;

    GridLocation changed = move.start;

    if (move.end.col - move.start.col == 2) {
        changed.col += 1;
    }
    else if (move.end.col - move.start.col == -2) {
        changed.col -= 1;
    }
    else if (move.end.row - move.start.row == 2) {
        changed.row += 1;
    }
    else if (move.end.row - move.start.row == -2) {
        changed.row -= 1;
    }
    _grid[changed] = MARBLE_OCCUPIED;
    _numMarbles++;
}



/* The functions below have already been written for you! */

MarbleBoard::MarbleBoard() : MarbleBoard(fullBoard()) {}   // default constructor creates the starting board

MarbleBoard::MarbleBoard(const Grid<Marble>& grid) {
    _grid = grid;
    _numMarbles = countMarbles(grid);
}

int MarbleBoard::numMarbles() const {
    return _numMarbles;
}

bool MarbleBoard::isSolved() const {
    return _numMarbles == 1;
}

Marble MarbleBoard::at(GridLocation loc) const {
    return _grid[loc];
}

BoardEncoding MarbleBoard::encode() const {
    return compressBoard(_grid);
}

Grid<Marble> MarbleBoard::toGrid() const {
    return _grid;
}

string MarbleBoard::toString() const {
    return boardToString(_grid);
}


/* * * * * * Test Cases * * * * * */

/*
 * Helper for the tests below: returns the moves as a Set, so that the tests
 * do not depend on the order in which findPossibleMoves returns its moves.
 */
static Set<Move> toSet(const Vector<Move>& moves) {
    Set<Move> result;
    for (const Move& move : moves) {
        result.add(move);
    }
    return result;
}

PROVIDED_TEST("MarbleBoard::findPossibleMoves: a lone marble has no moves") {
    MarbleBoard board(loadBoard("marbles/boards/solved.txt"));
    EXPECT_EQUAL(board.findPossibleMoves().size(), 0);
}

PROVIDED_TEST("MarbleBoard::findPossibleMoves: two marbles side by side can jump either way") {
    MarbleBoard board(boardFromStrings({
        "XX---XX",
        "XX---XX",
        "-------",
        "-OO----",
        "-------",
        "XX---XX",
        "XX---XX"
    }));
    Vector<Move> moves = board.findPossibleMoves();
    Set<Move> expected = { { {3, 1}, {3, 3} },     // left marble jumps right
                           { {3, 2}, {3, 0} } };   // right marble jumps left
    EXPECT_EQUAL(moves.size(), 2);
    EXPECT_EQUAL(toSet(moves), expected);
}

PROVIDED_TEST("MarbleBoard::findPossibleMoves: no jumping over empty holes, diagonally, off the board, or into corners") {
    // marbles with empty holes between them
    MarbleBoard spaced(boardFromStrings({
        "XXO-OXX",
        "XX---XX",
        "O-O-O-O",
        "-------",
        "O-O-O-O",
        "XX---XX",
        "XXO-OXX"
    }));
    EXPECT_EQUAL(spaced.findPossibleMoves().size(), 0);

    // marbles touching only diagonally
    MarbleBoard diagonals(loadBoard("marbles/boards/no-diagonals.txt"));
    EXPECT_EQUAL(diagonals.findPossibleMoves().size(), 0);

    // the only jumps in this line of marbles would land off the board or in a corner
    MarbleBoard edge(loadBoard("marbles/boards/off-the-edge.txt"));
    EXPECT_EQUAL(edge.findPossibleMoves().size(), 0);
}

PROVIDED_TEST("MarbleBoard::findPossibleMoves: the full board has exactly four moves, all into the center") {
    MarbleBoard board;
    Vector<Move> moves = board.findPossibleMoves();
    Set<Move> expected = { { {1, 3}, {3, 3} }, { {5, 3}, {3, 3} },
                           { {3, 1}, {3, 3} }, { {3, 5}, {3, 3} } };
    EXPECT_EQUAL(moves.size(), 4);
    EXPECT_EQUAL(toSet(moves), expected);
}

PROVIDED_TEST("MarbleBoard::makeMove and undoMove: jump down into the center, then undo") {
    MarbleBoard board;
    Move move = { {1, 3}, {3, 3} };
    board.makeMove(move);
    EXPECT_EQUAL(board.at({1, 3}), MARBLE_EMPTY);       // jumping marble left its start
    EXPECT_EQUAL(board.at({2, 3}), MARBLE_EMPTY);       // jumped-over marble is gone
    EXPECT_EQUAL(board.at({3, 3}), MARBLE_OCCUPIED);    // jumping marble landed
    EXPECT_EQUAL(board.numMarbles(), 31);

    board.undoMove(move);
    EXPECT_EQUAL(board, MarbleBoard());
    EXPECT_EQUAL(board.numMarbles(), 32);
}

PROVIDED_TEST("MarbleBoard::makeMove and undoMove: jump to the left, then undo") {
    MarbleBoard board(loadBoard("marbles/boards/one-jump.txt"));
    MarbleBoard original = board;
    Move move = { {3, 2}, {3, 0} };
    board.makeMove(move);
    EXPECT_EQUAL(board, MarbleBoard(boardFromStrings({
        "XX---XX",
        "XX---XX",
        "-------",
        "O------",
        "-------",
        "XX---XX",
        "XX---XX"
    })));
    EXPECT(board.isSolved());
    board.undoMove(move);
    EXPECT_EQUAL(board, original);
    EXPECT_EQUAL(board.numMarbles(), 2);
}

STUDENT_TEST("makeMove and undoMove work in all four directions") {
    Vector<Move> moves = {
        {{1, 3}, {3, 3}},  // down
        {{5, 3}, {3, 3}},  // up
        {{3, 1}, {3, 3}},  // right
        {{3, 5}, {3, 3}}   // left
    };

    for (Move move : moves) {
        MarbleBoard board;
        MarbleBoard original = board;

        GridLocation middle = {
            (move.start.row + move.end.row) / 2,
            (move.start.col + move.end.col) / 2
        };

        board.makeMove(move);

        EXPECT_EQUAL(board.at(move.start), MARBLE_EMPTY);
        EXPECT_EQUAL(board.at(middle), MARBLE_EMPTY);
        EXPECT_EQUAL(board.at(move.end), MARBLE_OCCUPIED);
        EXPECT_EQUAL(board.numMarbles(), 31);

        board.undoMove(move);

        EXPECT_EQUAL(board, original);
        EXPECT_EQUAL(board.numMarbles(), 32);
    }
}

STUDENT_TEST("Make and undo moves on two provided boards") {
    Vector<string> files = {"plus.txt", "three-step.txt"};
    Vector<Move> moves = {
        {{3, 2}, {3, 0}},  // plus: jump left
        {{4, 2}, {2, 2}}   // three-step: jump up
    };

    for (int i = 0; i < files.size(); i++) {
        MarbleBoard board(loadBoard("marbles/boards/" + files[i]));
        MarbleBoard original = board;
        int count = board.numMarbles();
        Move move = moves[i];
        GridLocation middle = {
            (move.start.row + move.end.row) / 2,
            (move.start.col + move.end.col) / 2
        };

        board.makeMove(move);

        EXPECT_EQUAL(board.at(move.start), MARBLE_EMPTY);
        EXPECT_EQUAL(board.at(middle), MARBLE_EMPTY);
        EXPECT_EQUAL(board.at(move.end), MARBLE_OCCUPIED);
        EXPECT_EQUAL(board.numMarbles(), count - 1);

        board.undoMove(move);

        EXPECT_EQUAL(board, original);
        EXPECT_EQUAL(board.numMarbles(), count);
    }
}

STUDENT_TEST("findPossibleMoves on three-step board") {
    MarbleBoard board(loadBoard("marbles/boards/three-step.txt"));
    MarbleBoard original = board;

    Set<Move> expected = {
        {{3, 2}, {3, 4}},
        {{3, 3}, {3, 1}},
        {{4, 2}, {2, 2}},
        {{4, 2}, {6, 2}}
    };

    Vector<Move> moves = board.findPossibleMoves();

    EXPECT_EQUAL(moves.size(), 4);
    EXPECT_EQUAL(toSet(moves), expected);
    EXPECT_EQUAL(board, original);
    EXPECT_EQUAL(board.numMarbles(), 4);
}

STUDENT_TEST("findPossibleMoves on plus board in all four directions") {
    MarbleBoard board(loadBoard("marbles/boards/plus.txt"));

    Set<Move> expected = {
        {{2, 3}, {0, 3}},  // up
        {{3, 2}, {3, 0}},  // left
        {{3, 4}, {3, 6}},  // right
        {{4, 3}, {6, 3}}   // down
    };

    Vector<Move> moves = board.findPossibleMoves();

    EXPECT_EQUAL(moves.size(), 4);
    EXPECT_EQUAL(toSet(moves), expected);
}