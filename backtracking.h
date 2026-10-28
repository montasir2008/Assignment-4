#pragma once


/* Needed for marbles.cpp */
#include "vector.h"
#include "MarbleBoard.h"
#include "marbletypes.h"
bool solvePuzzle(MarbleBoard& board, Vector<Move>& moveHistory);


/* Needed for redistrict.cpp */
#include "grid.h"
bool canRedistrict(Grid<int>& populationGrid, int nDistricts);
