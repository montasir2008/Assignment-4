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

bool solvePuzzleHelper(MarbleBoard& board, Vector<Move>& moveHistory, Set<BoardEncoding>& visited) {
    if (board.isSolved()) return true;
    if (visited.contains(board.encode())) return false;

    Vector<Move> possibleMoves = board.findPossibleMoves();

    for (Move move : possibleMoves) {
        board.makeMove(move);
        if (solvePuzzleHelper(board, moveHistory += move, visited)) return true;
        visited.add(board.encode());
        moveHistory.remove(moveHistory.size()-1);
        board.undoMove(move);
    }

    return false;
}

bool solvePuzzle(MarbleBoard& board, Vector<Move>& moveHistory) {
    Set<BoardEncoding> visited;
    return solvePuzzleHelper(board, moveHistory, visited);
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

STUDENT_TEST("Solver saves a complete three-move solution") {
    Grid<Marble> original = loadBoard("marbles/boards/three-step.txt");
    MarbleBoard board(original);
    Vector<Move> moves;

    EXPECT(solvePuzzle(board, moves));
    EXPECT_EQUAL(moves.size(), 3);
    EXPECT_EQUAL(checkSolution(original, moves), "");
}

STUDENT_TEST("Solver saves a complete solution for plus board") {
    Grid<Marble> original = loadBoard("marbles/boards/plus.txt");
    MarbleBoard board(original);
    Vector<Move> moves;

    EXPECT(solvePuzzle(board, moves));
    EXPECT_EQUAL(moves.size(), 8);
    EXPECT_EQUAL(checkSolution(original, moves), "");
}

STUDENT_TEST("Solver handles an unsolved board with no legal moves") {
    MarbleBoard board(loadBoard("marbles/boards/off-the-edge.txt"));
    MarbleBoard original = board;
    Vector<Move> moves;

    EXPECT(!solvePuzzle(board, moves));
    EXPECT(moves.isEmpty());
    EXPECT_EQUAL(board, original);
    EXPECT_EQUAL(board.numMarbles(), original.numMarbles());
}

STUDENT_TEST("Solver removes failed moves from history") {
    MarbleBoard board(loadBoard("marbles/boards/five-plus.txt"));
    MarbleBoard original = board;
    Vector<Move> moves;

    EXPECT(!solvePuzzle(board, moves));
    EXPECT(moves.isEmpty());
    EXPECT_EQUAL(board, original);
    EXPECT_EQUAL(board.numMarbles(), original.numMarbles());
}