/*
 * File: gridgraphics.cpp
 * ----------------------
 * @author Julie Zelenski, Fall 2023
 * Implementation of the graphics support routines. You are
 * welcome to read over this code if you curious, but you will
 * not need to edit it.
 */

#include "gridgraphics.h"
#include "gbutton.h"
#include "gcolor.h"
#include "gfont.h"
#include "glabel.h"
#include "gwindow.h"
#include "strlib.h"
#include "vector.h"
#include <iomanip>
using namespace std;

static string kCellBackground = "#FAFAFA";
static string kCellWallColor = "Gray";
static string kTextColor = "Dark Gray";
static string kLineColor = "Dark Gray";
static string kHighlightLineColor = "#941751";
static string kHighlightRangeColor = "#FFFC79";
static bool gInitialized;
static GWindow *gWindow;
static GButton *gButton[2];
static GLabel *gLabel;
static bool gPauseForClick;

static int gCellSize;
static string gFontDesc;
static Grid<int> gGrid;
struct lineT {
    GLine* gline;
    GridLocationRange range;
    friend bool operator< (const lineT& one, const lineT& two) { return one.gline < two.gline; }
    friend bool operator== (const lineT& one, const lineT& two) { return one.gline == two.gline; }
};
static Set<lineT> gSplitLines;

static void unpause(bool noFuturePause = false) {
    gLabel->setColor(gLabel->getBackground());
    gLabel->setEnabled(false);
    gButton[0]->setEnabled(false);
    gButton[1]->setEnabled(false);
    if (noFuturePause) gPauseForClick = false;
}

static void pauseForClick() {
    if (!gInitialized) return;
    gLabel->setText("Paused...");
    gLabel->setColor(kHighlightLineColor);
    gButton[0]->setEnabled(true);
    gButton[1]->setEnabled(true);
    while (gButton[0]->isEnabled()) pause(100);
}

void waitForCloseWindow() {
    if (!gInitialized || !gWindow->isVisible()) return;
    gLabel->setColor(kHighlightLineColor);
    gLabel->setText("Close window to finish");
    gWindow->setCloseOperation(GWindow::CLOSE_HIDE);
    while (gWindow->isVisible()) pause(100);
}

static void initialize() {
    gWindow = new GWindow(35, 35);
    gWindow->setVisible(false);
    gWindow->setWindowTitle("Redistrict");
    gWindow->setRepaintImmediately(false);
    gWindow->setAutoRepaint(false);
    gWindow->setExitOnClose(false);
    gWindow->setLocation(10, 10);
    gWindow->setResizable(false);
    gLabel = new GLabel("Paused...");
    gWindow->addToRegion(gLabel, GWindow::REGION_SOUTH);
    gButton[0] = new GButton("Step");
    gButton[0]->setActionListener([]() { unpause(); });
    gWindow->addToRegion(gButton[0], GWindow::REGION_SOUTH);
    gButton[1] = new GButton("Continue");
    gButton[1]->setActionListener([]() { unpause(true); });
    gWindow->addToRegion(gButton[1], GWindow::REGION_SOUTH);
    gInitialized = true;
}

static GDimension getTextBounds(string text, string font) {
    gWindow->setFont(font);
    QFontMetrics metrics(GFont::toQFont(font));
    return GDimension { double(metrics.horizontalAdvance(QString::fromStdString(text))), double(metrics.ascent()) };
}

// This goopy piece of code is needed to account for platform differences in
// how fonts are sized. Pay no attention.
static void configureFontSize(double width, double height, string text) {
    GDimension bounds;
    int fontSize = 1;
    do {
        gFontDesc = "Helvetica-" + integerToString(++fontSize);
        bounds = getTextBounds(text, gFontDesc);
    } while (bounds.height < height/2 && bounds.width < width/2);
}

static void changeDimensions(int numRows, int numCols) {
    static const int kMinWindowSize = 300, kMaxWindowSize = 720;
    static const int kDefaultCellSize = 70;

    if (!gInitialized) initialize();
    gWindow->setVisible(false);
    gCellSize = kDefaultCellSize;
    int minDimension = min(numRows, numCols);
    int maxDimension = max(numRows, numCols);
    gCellSize = min(max(kDefaultCellSize, kMinWindowSize/minDimension), kMaxWindowSize/maxDimension);
    gWindow->setCanvasSize(numCols*gCellSize, numRows*gCellSize + 1);
    configureFontSize(gCellSize, gCellSize, "##");
    gWindow->setVisible(true);
}

static GRectangle rectForRange(GridLocationRange range) {
    return GRectangle(range.startCol()*gCellSize, range.startRow()*gCellSize,
                      range.numCols()*gCellSize, range.numRows()*gCellSize);
}

static void horizBounds(GridLocationRange range, int k, GPoint& start, GPoint& end) {
    double y = (k + range.startRow())*gCellSize;
    start = GPoint(range.startCol()*gCellSize, y);
    end = GPoint((1 + range.endCol())*gCellSize, y);
}

static void vertBounds(GridLocationRange range, int k, GPoint& start, GPoint& end) {
    double x = (k + range.startCol())*gCellSize;
    start = GPoint(x, range.startRow()*gCellSize);
    end = GPoint(x, (1 + range.endRow())*gCellSize);
}

static void drawCenteredText(GPoint center, double width, double height, string text) {
    GDimension bounds = getTextBounds(text, gFontDesc);
    gWindow->setColor(kTextColor);
    gWindow->drawString(text, center.x + (width-bounds.width)/2, center.y + height - (height-bounds.height)/2);
}

static void drawPopulationLabel(GridLocation loc) {
    GPoint center(loc.col*gCellSize, loc.row*gCellSize);
    drawCenteredText(center, gCellSize, gCellSize, integerToString(gGrid[loc]));
}

static void drawBaseGrid(GridLocationRange *chosen = nullptr) {
    gWindow->clearCanvasPixels();
    gWindow->setColor(kCellBackground);
    gWindow->fillRect(rectForRange(gGrid.locations()));

    if (chosen) {
        gWindow->setColor(kHighlightRangeColor);
        gWindow->fillRect(rectForRange(*chosen));
    }
    gWindow->setLineWidth(1);
    gWindow->setLineStyle(GObject::LINE_DOT);
    for (const auto& cur: gGrid.locations()) {
        gWindow->setColor(kCellWallColor);
        gWindow->drawRect(rectForRange(GridLocationRange(cur, cur)));
        drawPopulationLabel(cur);
    }
    gWindow->setColor(kCellWallColor);
    gWindow->setLineWidth(1);
    gWindow->setLineStyle(GObject::LINE_SOLID);
    gWindow->drawRect(rectForRange(gGrid.locations()));
}

void initForPopulationGrid(Grid<int> &g, bool pauseOnRedraw) {
    if (!gInitialized) initialize();
    gWindow->setCloseOperation(GWindow::CLOSE_DO_NOTHING);
    gPauseForClick = pauseOnRedraw;
    if (g.numRows() != gGrid.numRows() || g.numCols() != gGrid.numCols()) {
        changeDimensions(g.numRows(), g.numCols());
    }
    gWindow->setVisible(true);
    gWindow->clear();
    gSplitLines.clear();
    gGrid = g;
    drawBaseGrid();
    gWindow->repaint();
    if (gPauseForClick) pauseForClick();
}

static GLine *createSplitLine(GridLocationRange range, int k, bool horiz) {
    GPoint start, end;
    horiz ?  horizBounds(range, k, start, end) : vertBounds(range, k, start, end);
    GLine *gl = new GLine(start, end);
    gl->setLineWidth(5);
    gl->setColor(kLineColor);
    gWindow->add(gl);
    gSplitLines.add({gl, range}); // store gline with range
    return gl;
}

static void addSplitLine(GridLocationRange range, int k, bool horiz) {
    if (!gGrid.locations().contains(range.startLocation()) || !gGrid.locations().contains(range.endLocation()))
        error("Invalid request to draw split, range [" + range.toString() + "] is outside grid!");
    if (horiz && (k == 0 || k >= range.numRows())) error("Invalid request to drawHorizSplit (top or bottom has zero rows) ");
    if (!horiz && (k == 0 || k >= range.numCols())) error("Invalid request to drawVertSplit (left or right has zero cols)");
    GLine *gl = createSplitLine(range, k, horiz);
    gl->setColor(kHighlightLineColor);
    drawBaseGrid(&range);
    gWindow->repaint();
    if (gPauseForClick) pauseForClick();
    gl->setColor(kLineColor);
    drawBaseGrid();
    gWindow->repaint();
}

static void removeSplitLinesInRange(GridLocationRange range) {
    Set<lineT> toRemove;
    // iterate over all split lines, find those within given range (subranges too)
    for (const auto& lt: gSplitLines) {
        GridLocationRange lineRange = lt.range;
        if (range.contains(lineRange.startLocation()) && range.contains(lineRange.endLocation())) {
            gWindow->remove(lt.gline); // remove from window canvas
            toRemove.add(lt);
        }
    }
    gSplitLines -= toRemove;
    gWindow->repaint();
}

void drawVertSplit(Grid<int>& grid, GridLocationRange range, int k) {
    if (!gInitialized || grid != gGrid) return;
    removeSplitLinesInRange(range);
    addSplitLine(range, k, false);
}

void drawHorizSplit(Grid<int>& grid, GridLocationRange range, int k) {
    if (!gInitialized || grid != gGrid) return;
    removeSplitLinesInRange(range);
    addSplitLine(range, k, true);
}

void printGrid() {
    if (gGrid.isEmpty()) error("printDistricts called without previous call to initForPopulationGrid");

    for (int row = 0; row < gGrid.numRows(); row++) {
        for (int col = 0; col < gGrid.numCols(); col++) {
            cout << "+----";
        }
        cout << "+" << endl;
        for (int col = 0; col < gGrid.numCols(); col++) {
            cout << "| " << right << setw(2) << setfill(' ') << gGrid[row][col] << " ";
        }
        cout << "|" << endl;
    }
    for (int col = 0; col < gGrid.numCols(); col++) {
        cout << "+----";
    }
    cout << "+" << endl;
}
