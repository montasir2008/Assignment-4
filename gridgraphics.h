#pragma once
#include "grid.h"

/**
 * The initForPopulationGrid function takes a Grid parameter representing
 * a population grid.  Call initForPopulationGrid once before starting
 * redistrict to configure the starting state of the animation.
 * The initial drawing is a grid of gray numbers on a white background.
 *
 * The optional pauseForClick parameter allows you to configure the
 * animation to pause after each redraw and require a click from the user
 * to continue. If not specified, pauseForClick defaults to true.
 */
void initForPopulationGrid(Grid<int>& grid, bool pauseForClick = true);


/**
 * The drawHorizSplit function takes a population grid (same grid as
 * earlier configured in call to initForPopulationGrid), a GridLocationRange,
 * and the number of columns to apportion to the top. Any previous division(s)
 * of the range is erased, the full range is highighted in yellow, and a thick
 * horizontal line drawn to show the split into top and right sections according
 * to the requested apportionment.
 *
 * If the animation has been configured to pauseForClick, animation will
 * pause after drawing.
 *
 * If the split is invalid for the grid/range, an error is raised.
 */
void drawHorizSplit(Grid<int>& grid, GridLocationRange fullRange, int nTopRows);

/**
 * The drawVertSplit function takes a population grid (same grid as
 * earlier configured in call to initForPopulationGrid), a GridLocationRange,
 * and the number of rows to apportion to the left. Any previous division(s)
 * of the range is erased, the full range is highighted in yellow, and a thick
 * vertical line drawn to show the split into left and right sides according
 * to the requested apportionment.
 *
 * If the animation has been configured to pauseForClick, animation will
 * pause after drawing.
 *
 * If the split is invalid for the grid/range, an error is raised.
 */
void drawVertSplit(Grid<int>& grid, GridLocationRange fullRange, int nLeftCols);


/**
 * It can be helpful to pause the display after completing a redistrict operation
 * and give the user a chance to review the final state.  Call this function to pause
 * and wait for user to close the window when they are ready to move on.
 */
void waitForCloseWindow();