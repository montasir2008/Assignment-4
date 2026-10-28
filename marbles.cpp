/*
 * File: marbles.cpp
 * -----------------
 * TODO: Replace this comment with a description of your program, your name,
 * and your partner's name if you worked as a pair. Cite any sources of help.
 */
#include <iostream>
#include "backtracking.h"
#include "hashset.h"
#include "set.h"
#include "SimpleTest.h"
using namespace std;

/*
 * TODO: Replace this comment with a description of this function
 * and how you implemented it.
 */
bool solvePuzzle(MarbleBoard& board, Vector<Move>& moveHistory) {
    /* TODO: Fill in this function. */
    return false;
}


/* * * * * * Test Cases * * * * * */

PROVIDED_TEST("solvePuzzle: one marble in the center is already solved") {
    MarbleBoard board(loadBoard("marbles/boards/solved.txt"));
    Vector<Move> moves;
    EXPECT(solvePuzzle(board, moves));
    EXPECT_EQUAL(moves.size(), 0);
}

PROVIDED_TEST("solvePuzzle: two marbles are solved with one jump") {
    Grid<Marble> original = loadBoard("marbles/boards/one-jump.txt");
    MarbleBoard board(original);
    Vector<Move> moves;
    EXPECT(solvePuzzle(board, moves));
    EXPECT_EQUAL(moves.size(), 1);
    EXPECT_EQUAL(checkSolution(original, moves), "");
}


PROVIDED_TEST("solvePuzzle: an unsolvable board returns false and is left unchanged") {
    MarbleBoard board(loadBoard("marbles/boards/five-plus.txt"));
    MarbleBoard original = board;
    Vector<Move> moves;
    EXPECT(!solvePuzzle(board, moves));
    EXPECT_EQUAL(board, original);
    EXPECT_EQUAL(board.numMarbles(), original.numMarbles());
}

PROVIDED_TEST("solvePuzzle: solvable mid-size boards") {
    for (string name : { "cross", "seven-step", "plus", "double-cross"}) {
        Grid<Marble> original = loadBoard("marbles/boards/" + name + ".txt");
        MarbleBoard board(original);
        Vector<Move> moves;
        EXPECT(solvePuzzle(board, moves));
        EXPECT_EQUAL(checkSolution(original, moves), "");
    }
}

PROVIDED_TEST("solvePuzzle: mid-size unsolvable board") {
    MarbleBoard board(loadBoard("marbles/boards/tangle.txt"));
    Vector<Move> moves;
    EXPECT(!solvePuzzle(board, moves));
}

PROVIDED_TEST("solvePuzzle: the full 32-marble board") {
    MarbleBoard board;
    Vector<Move> moves;
    bool solved = false;
    TIME_OPERATION(32, solved = solvePuzzle(board, moves));
    EXPECT(solved);
    EXPECT_EQUAL(checkSolution(fullBoard(), moves), "");
}

PROVIDED_TEST("solvePuzzle: unsolvable 15-marble board (needs the explored-boards optimization)") {
    // A solver that does not skip boards it has already explored will take
    // a very, very long time on this test.
    MarbleBoard board(loadBoard("marbles/boards/letter-l.txt"));
    Vector<Move> moves;
    bool solved = true;
    TIME_OPERATION(15, solved = solvePuzzle(board, moves));
    EXPECT(!solved);
}
