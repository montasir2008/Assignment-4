/*
 * TODO: remove and replace this file header comment
 * You will edit and turn in this file.
 * Remove starter comments and add your own
 * comments on each function and on complex code sections.
 */
#include <iostream>    // for cout, endl
#include <string>      // for string class
#include "backtracking.h"
#include "grid.h"
#include "gridgraphics.h"
#include "SimpleTest.h"
using namespace std;


/* TODO: Design and implement helpers
 * We offer some suggested helper prototypes. You are free to
 * to modify and extend on these suggestions as you see fit.
 *
 * NOTE: the prototype of required function canRedistrict must not be changed.
 */
int subgridSum(Grid<int>& g, GridLocationRange range) {
    int sum = 0;
    for (int i = range.startRow(); i <= range.endRow(); i++) {
        for (int j = range.startCol(); j <= range.endCol(); j++) {
            sum += g[i][j];
        }
    }
    return sum;
}

void divideHorizontal(GridLocationRange range, int indexOfBottomMostRowInTopPart, GridLocationRange& topPart, GridLocationRange& bottomPart) {
    topPart = GridLocationRange(range.startRow(), range.startCol(),indexOfBottomMostRowInTopPart, range.endCol());
    bottomPart = GridLocationRange(indexOfBottomMostRowInTopPart+1, range.startCol(), range.endRow(), range.endCol());
}

void divideVertical(GridLocationRange range, int indexOfRightMostColInLeftSide, GridLocationRange& leftSide, GridLocationRange& rightSide) {
    leftSide = GridLocationRange(range.startRow(), range.startCol(), range.endRow(), indexOfRightMostColInLeftSide);
    rightSide = GridLocationRange(range.startRow(), indexOfRightMostColInLeftSide + 1, range.endRow(),range.endCol());
}


bool canRedistrictHelper(Grid<int>& populationGrid, GridLocationRange current, int& target) {
    if (subgridSum(populationGrid, current) == target) return true;
    if (subgridSum(populationGrid, current) % target != 0) return false;

    for (int i = current.startRow(); i < current.endRow(); i++) {
        GridLocationRange topPart;
        GridLocationRange bottomPart;
        divideHorizontal(current, i, topPart, bottomPart);
        drawHorizSplit(populationGrid, current,i + 1 - current.startRow());

        if (canRedistrictHelper(populationGrid, topPart, target) && canRedistrictHelper(populationGrid, bottomPart, target)) return true;
    }

    for (int i = current.startCol(); i < current.endCol(); i++) {
        GridLocationRange leftSide;
        GridLocationRange rightSide;
        divideVertical(current, i, leftSide, rightSide);
        drawVertSplit(populationGrid, current, i+1 - current.startCol());

        if (canRedistrictHelper(populationGrid, rightSide, target) && canRedistrictHelper(populationGrid, leftSide, target)) return true;
    }

    return false;
}

/*
 * TODO: Replace this comment with a descriptive function
 * header comment.
 */
bool canRedistrict(Grid<int>& populationGrid, int nDistricts) {
    initForPopulationGrid(populationGrid, false);

    GridLocationRange starter(0, 0, populationGrid.numRows() - 1, populationGrid.numCols() - 1);
    int total = subgridSum(populationGrid, starter);
    int target = total/nDistricts;
    return canRedistrictHelper(populationGrid, starter, target);
}

/* * * * * * Test Cases * * * * * */

STUDENT_TEST("subgridSum handles a single cell, row, and column") {
    Grid<int> grid = {
        {1, 2, 3},
        {4, 5, 6},
        {7, 8, 9}
    };

    EXPECT_EQUAL(subgridSum(grid, GridLocationRange(1, 1, 1, 1)), 5);
    EXPECT_EQUAL(subgridSum(grid, GridLocationRange(1, 0, 1, 2)), 15);
    EXPECT_EQUAL(subgridSum(grid, GridLocationRange(0, 2, 2, 2)), 18);
}

STUDENT_TEST("subgridSum handles an offset rectangle and zeros") {
    Grid<int> grid = {
        {9, 9, 9, 9},
        {9, 0, 2, 3},
        {9, 4, 0, 6}
    };

    EXPECT_EQUAL(subgridSum(grid, GridLocationRange(1, 1, 2, 3)), 15);
    EXPECT_EQUAL(subgridSum(grid, GridLocationRange(1, 1, 1, 1)), 0);
    EXPECT_EQUAL(subgridSum(grid, grid.locations()), 69);
}

STUDENT_TEST("divideHorizontal preserves boundaries for every valid cut") {
    GridLocationRange range(2, 3, 6, 7);

    for (int cut = range.startRow(); cut < range.endRow(); cut++) {
        GridLocationRange top, bottom;
        divideHorizontal(range, cut, top, bottom);

        EXPECT_EQUAL(top.startRow(), 2);
        EXPECT_EQUAL(top.startCol(), 3);
        EXPECT_EQUAL(top.endRow(), cut);
        EXPECT_EQUAL(top.endCol(), 7);

        EXPECT_EQUAL(bottom.startRow(), cut + 1);
        EXPECT_EQUAL(bottom.startCol(), 3);
        EXPECT_EQUAL(bottom.endRow(), 6);
        EXPECT_EQUAL(bottom.endCol(), 7);
    }
}

STUDENT_TEST("divideHorizontal splits a two-cell column") {
    GridLocationRange range(0, 0, 1, 0);
    GridLocationRange top, bottom;
    divideHorizontal(range, 0, top, bottom);

    EXPECT_EQUAL(top.startRow(), 0);
    EXPECT_EQUAL(top.endRow(), 0);
    EXPECT_EQUAL(top.startCol(), 0);
    EXPECT_EQUAL(top.endCol(), 0);

    EXPECT_EQUAL(bottom.startRow(), 1);
    EXPECT_EQUAL(bottom.endRow(), 1);
    EXPECT_EQUAL(bottom.startCol(), 0);
    EXPECT_EQUAL(bottom.endCol(), 0);
}

STUDENT_TEST("divideVertical preserves boundaries for every valid cut") {
    GridLocationRange range(2, 3, 6, 7);

    for (int cut = range.startCol(); cut < range.endCol(); cut++) {
        GridLocationRange left, right;
        divideVertical(range, cut, left, right);

        EXPECT_EQUAL(left.startRow(), 2);
        EXPECT_EQUAL(left.startCol(), 3);
        EXPECT_EQUAL(left.endRow(), 6);
        EXPECT_EQUAL(left.endCol(), cut);

        EXPECT_EQUAL(right.startRow(), 2);
        EXPECT_EQUAL(right.startCol(), cut + 1);
        EXPECT_EQUAL(right.endRow(), 6);
        EXPECT_EQUAL(right.endCol(), 7);
    }
}

STUDENT_TEST("divideVertical splits a two-cell row") {
    GridLocationRange range(0, 0, 0, 1);
    GridLocationRange left, right;
    divideVertical(range, 0, left, right);

    EXPECT_EQUAL(left.startRow(), 0);
    EXPECT_EQUAL(left.endRow(), 0);
    EXPECT_EQUAL(left.startCol(), 0);
    EXPECT_EQUAL(left.endCol(), 0);

    EXPECT_EQUAL(right.startRow(), 0);
    EXPECT_EQUAL(right.endRow(), 0);
    EXPECT_EQUAL(right.startCol(), 1);
    EXPECT_EQUAL(right.endCol(), 1);
}

STUDENT_TEST("canRedistrictHelper handles completed and unsplittable cells") {
    Grid<int> grid = {{6}};
    int target = 6;

    EXPECT(canRedistrictHelper(grid, grid.locations(), target));

    target = 3;
    EXPECT(!canRedistrictHelper(grid, grid.locations(), target));
}

STUDENT_TEST("canRedistrictHelper rejects a nondivisible subrange") {
    Grid<int> grid = {{2, 3, 1, 4}};
    int target = 5;

    // The entire grid is divisible by 5, but this candidate piece is not.
    GridLocationRange current(0, 0, 0, 2);
    EXPECT(!canRedistrictHelper(grid, current, target));
}

STUDENT_TEST("canRedistrictHelper solves an offset subrange") {
    Grid<int> grid = {
        {10, 10, 10},
        {10,  2,  3},
        {10,  1,  4}
    };
    GridLocationRange current(1, 1, 2, 2);
    int target = 5;

    EXPECT(canRedistrictHelper(grid, current, target));
}

STUDENT_TEST("canRedistrict handles one district") {
    Grid<int> grid = {{2, 3}, {4, 5}};

    EXPECT(canRedistrict(grid, 1));
}

STUDENT_TEST("canRedistrict handles single rows and columns") {
    Grid<int> row = {{2, 3, 1, 4}};
    Grid<int> column = {{2}, {3}, {1}, {4}};

    EXPECT(canRedistrict(row, 2));
    EXPECT(canRedistrict(column, 2));
}

STUDENT_TEST("canRedistrict allows unequal populations across a cut") {
    Grid<int> grid = {{10, 10, 20, 20}};

    // Three districts of 20: the first cut separates 20 from 40.
    EXPECT(canRedistrict(grid, 3));
}

STUDENT_TEST("canRedistrict tries later cuts when earlier cuts fail") {
    Grid<int> grid = {{1, 1, 2}};

    // Cutting after the first cell fails; after the second succeeds.
    EXPECT(canRedistrict(grid, 2));
}

STUDENT_TEST("canRedistrict requires both sides of a cut to succeed") {
    Grid<int> grid = {{2, 1, 3}};

    // The first cell is a district, but the remaining piece cannot
    // form two districts of population 2.
    EXPECT(!canRedistrict(grid, 3));
}

STUDENT_TEST("canRedistrict combines horizontal and vertical cuts") {
    Grid<int> grid = {
        {1, 1},
        {2, 2}
    };
    Grid<int> original = grid;

    EXPECT(canRedistrict(grid, 3));
    EXPECT_EQUAL(grid, original);
}

STUDENT_TEST("canRedistrict rejects a divisible but unsplittable board") {
    Grid<int> grid = {
        {1, 3},
        {3, 1}
    };

    EXPECT(!canRedistrict(grid, 4));
}

STUDENT_TEST("canRedistrict handles zero-population cells") {
    Grid<int> grid = {{0, 2, 0, 2, 0}};

    EXPECT(canRedistrict(grid, 2));
}


/* Please not add/modify/remove the PROVIDED_TEST entries below.
 * Place your student tests cases above the provided tests.
 */


PROVIDED_TEST("subgridSum sums values in grid range") {
    // This is an example test of helper function subgridSum
    Grid<int> grid = { {1, 2, 3}, {4, 5, 6}, {7, 8, 9}, {10, 10, 10} };
    GridLocationRange all = grid.locations();
    GridLocationRange firstColumn = GridLocationRange(0, 0, 3, 0);
    GridLocationRange bottomCorner = GridLocationRange(3, 2, 3, 2);

    EXPECT_EQUAL(subgridSum(grid, firstColumn), 22);
    EXPECT_EQUAL(subgridSum(grid, bottomCorner), 10);
    EXPECT_EQUAL(subgridSum(grid, all), 75);
}

PROVIDED_TEST("nevada example from writeup can be divided into 4 districts") {
    Grid<int> nevada = { {20, 31, 20},
                         { 5, 19, 30},
                         {25, 25, 25} };

    initForPopulationGrid(nevada); // you can remove if you don't wish to animate this test case
    EXPECT(canRedistrict(nevada, 4));
    waitForCloseWindow();
}

PROVIDED_TEST("wyoming example from writeup cannot be divided into 2 districts") {
    Grid<int> wyoming = { {10, 15},
                          { 5, 10} };

    initForPopulationGrid(wyoming); // you can remove if you don't wish to animate this test case
    EXPECT(!canRedistrict(wyoming, 2));
    waitForCloseWindow();
}

PROVIDED_TEST("small example, 5 districts") {
    Grid<int> small = { {3, 7, 10},
                        {6, 3, 1},
                        {1, 10, 9} };
    initForPopulationGrid(small);  // you can remove if you don't wish to animate this test case
    EXPECT(canRedistrict(small, 5));
    waitForCloseWindow();
}

PROVIDED_TEST("medium example, 12 districts") {
    Grid<int> medium = { {10, 1,  2, 5, 1, 4},
                          {1, 1,  8, 2, 1, 7},
                          {9, 8, 10, 3, 3, 4},
                          {5, 2,  2, 8, 7, 3},
                          {2, 1,  1, 7, 1, 1} };
    initForPopulationGrid(medium);  // you can remove if you don't wish to animate this test case
    EXPECT(canRedistrict(medium, 12));
    waitForCloseWindow();
}

PROVIDED_TEST("large example can be divided into 25 districts") {
    Grid<int> large = { { 5,  2, 2,  1,  6, 4, 8, 2},
                        {10, 10, 2,  1,  3, 4, 2, 3},
                        { 1,  1, 4,  5,  1, 9, 1, 4},
                        { 9,  5, 5, 10, 10, 3, 3, 4},
                        { 3,  1, 1,  1,  2, 8, 6, 2},
                        { 2,  1, 1,  7,  3, 5, 1, 1},
                        { 1,  1, 2,  2,  7, 5, 1, 9},
                        { 4,  1, 2, 10,  1, 9, 5, 5} };
    initForPopulationGrid(large); // you can remove if you don't wish to animate this test case
    EXPECT(canRedistrict(large, 25));
    waitForCloseWindow();
}
