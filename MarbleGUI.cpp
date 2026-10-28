/*
 * File: MarbleGUI.cpp
 * -------------------
 * CS106B Marble Solitaire
 * Graphics and user interaction for Marble Solitaire. You should not need to
 * read or modify this file, though you are welcome to look.
 *
 * How it fits together:
 *  - marbleSolitaire() runs on the same thread as main(), which the CS106
 *    library calls the "student thread". The window's buttons and mouse
 *    clicks are delivered on a different thread (the Qt GUI thread), so the
 *    listeners below just post a Command to a queue, and the event loop in
 *    marbleSolitaire() takes commands off the queue and carries them out.
 *    That keeps the window responsive while the student thread is busy
 *    animating or running the solver.
 *  - While solvePuzzle runs, MarbleBoard::encode calls compressBoard, which
 *    calls back into this file through MarbleInternal::compressHook. The hook counts boards for the progress
 *    display, draws boards for "Animate search", and stops the search when
 *    the Stop button is pressed (by throwing an exception that unwinds
 *    solvePuzzle).
 *  - All changes to the objects on the canvas are made on the Qt GUI thread
 *    (via GThread::runOnQtGuiThread) so they never race with painting.
 *
 * @author Cynthia Lee and Marty Stepp (original version, 2017)
 * @version 2026/09 rewritten for the Qt-based CS106 library
 */
#include "MarbleGUI.h"
#include <atomic>
#include <chrono>
#include <cmath>
#include <condition_variable>
#include <deque>
#include <mutex>
#include "console.h"
#include "error.h"
#include "filelib.h"
#include "gbutton.h"
#include "gcheckbox.h"
#include "gcontainer.h"
#include "gfilechooser.h"
#include "glabel.h"
#include "gobjects.h"
#include "goptionpane.h"
#include "gslider.h"
#include "gthread.h"
#include "gwindow.h"
#include "random.h"
#include "strlib.h"
#include "backtracking.h"
#include "marbletypes.h"
using namespace std;

/* Board support code, defined in marbletypes.cpp. Moves are checked using hole
 * numbers and one bit per hole, with a table of every possible jump. */
namespace MarbleInternal {
    extern void (*compressHook)(const Grid<Marble>& board);
    struct Jump {
        int start, over, end;   // hole numbers
    };
    extern const Jump kJumps[];
    extern const int kNumJumps;
    GridLocation holeLocation(int hole);
    int holeNumber(const GridLocation& loc);
    const Jump* findJump(int start, int end);
    uint64_t boardBits(const Grid<Marble>& board);
}
using MarbleInternal::Jump;
using MarbleInternal::kJumps;
using MarbleInternal::kNumJumps;
using MarbleInternal::holeLocation;
using MarbleInternal::holeNumber;
using MarbleInternal::findJump;
using MarbleInternal::boardBits;

namespace {

/* * * * * * Constants * * * * * */

const string kBoardDir = "marbles/boards/";
const string kImageDir = "marbles/images/";
const string kBackgroundImage = kImageDir + "wood-background.jpg";
const int kNumMarbleImages = 17;                 // marbles/images/m1.png ... m17.png
const int kNumHoles = 33;
const int kFrameMs = 16;                         // about 60 frames per second
const double kPi = 3.14159265358979323846;

const string kStatusColor = "#222222", kWarnColor = "#9a5b00", kErrorColor = "#b00020";
const string kSelectColor = "#ffd84d", kTargetColor = "#7cfc00";
const string kChangedColor = "#ff8c1a", kGoodColor = "#1f8f3a", kBadColor = "#b00020";

/* * * * * * Commands posted by listeners (Qt GUI thread) for the event loop (student thread) * * * * * */

enum CommandType {
    CMD_CLICK, CMD_LOAD, CMD_RESTART, CMD_UNDO, CMD_PLAY, CMD_SOLVE
};

struct Command {
    CommandType type;
    double x, y;   // for clicks
};

mutex gCommandLock;
condition_variable gCommandReady;
deque<Command> gCommands;

void postCommand(CommandType type, double x = 0, double y = 0) {
    {
        lock_guard<mutex> guard(gCommandLock);
        gCommands.push_back({ type, x, y });
    }
    gCommandReady.notify_one();
}

/* Waits up to timeoutMs for a command. Returns false if none arrived. */
bool waitForCommand(Command& cmd, int timeoutMs) {
    unique_lock<mutex> guard(gCommandLock);
    if (!gCommandReady.wait_for(guard, chrono::milliseconds(timeoutMs),
                                [] { return !gCommands.empty(); })) {
        return false;
    }
    cmd = gCommands.front();
    gCommands.pop_front();
    return true;
}

void discardCommands() {
    lock_guard<mutex> guard(gCommandLock);
    gCommands.clear();
}

/* settings changed by listeners and read by the student thread */
atomic<bool> gStopRequested(false);
atomic<bool> gAnimateSearch(false);
atomic<int> gSpeed(50);   // 0 (slow) to 100 (fast)

/* * * * * * Game state (student thread only) * * * * * */

Grid<Marble> gStartBoard;      // the board as loaded, for Restart
Grid<Marble> gBoard;           // the board as it is now
string gBoardName;
Vector<Move> gHistory;         // moves made so far, oldest first
Vector<Move> gRedo;            // moves available to Step/Play; next move is at the back
bool gPlaying = false;         // is Play stepping through gRedo?
bool gHasSelection = false;
GridLocation gSelected;
string gSolutionStatus;        // summary of the last Solve, shown while its moves play

/* * * * * * Graphics objects * * * * * */

GWindow* gWindow = nullptr;
GCanvas* gCanvas = nullptr;
GButton *gLoadButton, *gRestartButton, *gUndoButton, *gPlayButton, *gSolveButton,
        *gStopButton;
GCheckBox* gAnimateBox;
GSlider* gSpeedSlider;
GLabel* gStatusLabel;
GText *gCountText, *gNameText;

double gCell, gMargin, gMarbleSize;   // layout, scaled to fit the screen
Grid<GImage*> gMarbleAt;              // marble image in each hole, or nullptr
Vector<GImage*> gSpareMarbles;        // marble images not on the board
Grid<GOval*> gChangedMark;            // rings for "Animate search"
GOval* gSelectRing;
Vector<GOval*> gTargetDots;
GOval* gShadow;

/* * * * * * Small helpers * * * * * */

typedef chrono::steady_clock Clock;

double msSince(Clock::time_point start) {
    return chrono::duration<double, milli>(Clock::now() - start).count();
}

double clamp01(double x) {
    return x < 0 ? 0 : (x > 1 ? 1 : x);
}

double smoothstep(double edge0, double edge1, double x) {
    double t = clamp01((x - edge0) / (edge1 - edge0));
    return t * t * (3 - 2 * t);
}

void onGuiThread(GThunk func) {
    GThread::runOnQtGuiThread(func);
}

/* Puts the elements of v in random order. */
template <typename T>
void shuffleVector(Vector<T>& v) {
    for (int i = v.size() - 1; i > 0; i--) {
        int j = randomInteger(0, i);
        T temp = v[i];
        v[i] = v[j];
        v[j] = temp;
    }
}

string plural(int n, const string& word) {
    return integerToString(n) + " " + word + (n == 1 ? "" : "s");
}

string formatMs(double ms) {
    if (ms < 1000) {
        return integerToString(int(ms + 0.5)) + " ms";
    }
    return realToString(round(ms / 100) / 10) + " s";
}

uint64_t bit(int hole) {
    return uint64_t(1) << hole;
}

bool isHole(const Grid<Marble>& board, const GridLocation& loc) {
    return board.inBounds(loc) && board[loc] != MARBLE_INVALID;
}

bool isWon(const Grid<Marble>& board) {
    return countMarbles(board) == 1;
}

/* The jump for a move, or nullptr if the move is not a jump between two holes. */
const Jump* jumpFor(const Move& move) {
    int start = holeNumber(move.start), end = holeNumber(move.end);
    return (start < 0 || end < 0) ? nullptr : findJump(start, end);
}

/* The location of the marble a move jumps over (the move must be a jump). */
GridLocation jumpedLocation(const Move& move) {
    const Jump* jump = jumpFor(move);
    return jump ? holeLocation(jump->over) : move.start;
}

bool isLegal(uint64_t bits, const Jump& jump) {
    return (bits & bit(jump.start)) && (bits & bit(jump.over)) && !(bits & bit(jump.end));
}

/* Returns "" if the move is legal on the board, else a friendly explanation of why not. */
string whyIllegal(const Grid<Marble>& board, const Move& move) {
    int start = holeNumber(move.start), end = holeNumber(move.end);
    if (start < 0 || end < 0) {
        return "That move goes off the board.";
    }
    uint64_t bits = boardBits(board);
    if (!(bits & bit(start))) {
        return "There is no marble there to move.";
    }
    if (bits & bit(end)) {
        return "A marble can only land in an empty hole.";
    }
    const Jump* jump = findJump(start, end);
    if (!jump) {
        return "A marble must jump exactly two holes up, down, left, or right.";
    }
    if (!(bits & bit(jump->over))) {
        return "A marble can only jump over another marble.";
    }
    return "";
}

/* The GUI's own move finder, so the game is playable before the student code is written. */
Vector<Move> legalMovesFrom(const Grid<Marble>& board, const GridLocation& start) {
    Vector<Move> moves;
    int hole = holeNumber(start);
    uint64_t bits = boardBits(board);
    for (int i = 0; i < kNumJumps; i++) {
        if (kJumps[i].start == hole && isLegal(bits, kJumps[i])) {
            moves.add({ start, holeLocation(kJumps[i].end) });
        }
    }
    return moves;
}

bool hasLegalMove(const Grid<Marble>& board) {
    uint64_t bits = boardBits(board);
    for (int i = 0; i < kNumJumps; i++) {
        if (isLegal(bits, kJumps[i])) {
            return true;
        }
    }
    return false;
}

/* Makes a move, or undoes it: either way, the move's three holes swap between
 * marble and empty. The move must be legal (or, to undo it, just made). */
void flipMove(Grid<Marble>& board, const Move& move) {
    const Jump* jump = jumpFor(move);
    uint64_t bits = boardBits(board) ^ bit(jump->start) ^ bit(jump->over) ^ bit(jump->end);
    for (int hole : { jump->start, jump->over, jump->end }) {
        board[holeLocation(hole)] = (bits & bit(hole)) ? MARBLE_OCCUPIED : MARBLE_EMPTY;
    }
}

/* * * * * * Drawing * * * * * */

GPoint centerOf(const GridLocation& loc) {
    return GPoint(gMargin + (loc.col + 0.5) * gCell, gMargin + (loc.row + 0.5) * gCell);
}

bool locationAt(double x, double y, GridLocation& loc) {
    loc = GridLocation(int(floor((y - gMargin) / gCell)), int(floor((x - gMargin) / gCell)));
    return isHole(gBoard, loc);
}

void placeMarble(GImage* img, const GPoint& center, double size) {
    img->setBounds(center.x - size / 2, center.y - size / 2, size, size);
}

void setStatus(const string& message, const string& color = kStatusColor) {
    gStatusLabel->setForeground(color);
    gStatusLabel->setText(message);
}

void updateCountText() {
    string text = plural(countMarbles(gBoard), "marble");
    onGuiThread([text] {
        gCountText->setText(text);
        gCanvas->repaint();
    });
}

/* Puts the marble images in the holes to match the given board. */
void showBoard(const Grid<Marble>& board, bool shuffleImages) {
    onGuiThread([&board, shuffleImages] {
        for (const GridLocation& loc : gMarbleAt.locations()) {
            GImage* img = gMarbleAt[loc];
            if (img && (shuffleImages || board[loc] != MARBLE_OCCUPIED)) {
                img->setVisible(false);
                gSpareMarbles.add(img);
                gMarbleAt[loc] = nullptr;
            }
        }
        if (shuffleImages) {
            shuffleVector(gSpareMarbles);
        }
        for (const GridLocation& loc : gMarbleAt.locations()) {
            if (board[loc] == MARBLE_OCCUPIED && !gMarbleAt[loc]) {
                GImage* img = gSpareMarbles.remove(gSpareMarbles.size() - 1);
                img->setOpacity(1);
                placeMarble(img, centerOf(loc), gMarbleSize);
                img->setVisible(true);
                gMarbleAt[loc] = img;
            }
        }
        gCanvas->repaint();
    });
}

void clearSelection() {
    gHasSelection = false;
    onGuiThread([] {
        gSelectRing->setVisible(false);
        for (GOval* dot : gTargetDots) {
            dot->setVisible(false);
        }
        gCanvas->repaint();
    });
}

/* Rings the marble at 'loc' and puts a dot in each hole it could jump to. */
void showSelection(const GridLocation& loc, const Vector<Move>& moves, const string& color) {
    gHasSelection = true;
    gSelected = loc;
    onGuiThread([loc, moves, color] {
        GPoint c = centerOf(loc);
        double ringSize = gMarbleSize + 0.16 * gCell;
        gSelectRing->setColor(color);
        gSelectRing->setBounds(c.x - ringSize / 2, c.y - ringSize / 2, ringSize, ringSize);
        gSelectRing->setVisible(true);
        for (int i = 0; i < gTargetDots.size(); i++) {
            GOval* dot = gTargetDots[i];
            if (i < moves.size()) {
                GPoint t = centerOf(moves[i].end);
                double size = 0.34 * gCell;
                dot->setFillColor(color == kSelectColor ? kTargetColor : color);
                dot->setBounds(t.x - size / 2, t.y - size / 2, size, size);
                dot->setVisible(true);
            } else {
                dot->setVisible(false);
            }
        }
        gCanvas->repaint();
    });
}

void hideChangedMarks() {
    onGuiThread([] {
        for (GOval* mark : gChangedMark) {
            if (mark) {
                mark->setVisible(false);
            }
        }
    });
}

/*
 * Animates a jump: the marble hops in an arc from one hole to the other while
 * the jumped-over marble fades out (or, when undoing, fades back in).
 */
void animateJump(const Move& move, bool undo) {
    GridLocation from = undo ? move.end : move.start;
    GridLocation to = undo ? move.start : move.end;
    GridLocation over = jumpedLocation(move);
    GPoint p0 = centerOf(from), p1 = centerOf(to);
    double duration = 650 - 560 * gSpeed / 100.0;

    GImage* mover = gMarbleAt[from];
    GImage* jumped = nullptr;
    onGuiThread([&] {
        if (undo) {   // bring a marble back for the one that was jumped
            jumped = gSpareMarbles.remove(gSpareMarbles.size() - 1);
            jumped->setOpacity(0);
            placeMarble(jumped, centerOf(over), gMarbleSize);
            jumped->setVisible(true);
        } else {
            jumped = gMarbleAt[over];
        }
        gShadow->setVisible(true);
        gShadow->sendToFront();
        mover->sendToFront();
    });

    Clock::time_point start = Clock::now();
    while (true) {
        double t = clamp01(msSince(start) / duration);
        double ease = smoothstep(0, 1, t);
        double hop = sin(kPi * t);
        GPoint ground(p0.x + (p1.x - p0.x) * ease, p0.y + (p1.y - p0.y) * ease);
        double size = gMarbleSize * (1 + 0.18 * hop);
        double fade = undo ? smoothstep(0.2, 0.6, t) : 1 - smoothstep(0.4, 0.8, t);
        onGuiThread([&] {
            double shadowW = gMarbleSize * (1 - 0.25 * hop), shadowH = shadowW * 0.55;
            gShadow->setOpacity(0.35 * (1 - 0.5 * hop));
            gShadow->setBounds(ground.x - shadowW / 2, ground.y - shadowH / 2 + gMarbleSize * 0.3,
                               shadowW, shadowH);
            placeMarble(mover, GPoint(ground.x, ground.y - hop * 0.45 * gCell), size);
            jumped->setOpacity(clamp01(fade));
            gCanvas->repaint();
        });
        if (t >= 1) {
            break;
        }
        GThread::msleep(kFrameMs);
    }

    onGuiThread([&] {
        gShadow->setVisible(false);
        placeMarble(mover, p1, gMarbleSize);
        gMarbleAt[from] = nullptr;
        gMarbleAt[to] = mover;
        if (undo) {
            jumped->setOpacity(1);
            gMarbleAt[over] = jumped;
        } else {
            jumped->setVisible(false);
            jumped->setOpacity(1);
            gMarbleAt[over] = nullptr;
            gSpareMarbles.add(jumped);
        }
        gCanvas->repaint();
    });
}

/* * * * * * Running the student's solver * * * * * */

struct SearchStopped {};   // thrown by the hook when Stop is pressed

/* state shared between runStudentSolver and the compressBoard hook */
long gBoardsSeen;
Clock::time_point gSolveStart, gLastProgress, gLastSearchFrame;
Grid<Marble> gLastSearchBoard;

void showSearchFrame(const Grid<Marble>& board) {
    double delay = 500 * pow(1 - gSpeed / 100.0, 3);
    if (delay < kFrameMs && msSince(gLastSearchFrame) < kFrameMs) {
        return;   // too fast to see every board; skip frames
    }
    showBoard(board, false);
    onGuiThread([&board] {
        for (const GridLocation& loc : board.locations()) {
            GOval* mark = gChangedMark[loc];
            if (mark) {
                mark->setVisible(gLastSearchBoard.inBounds(loc) && board[loc] != gLastSearchBoard[loc]);
            }
        }
        gCountText->setText(plural(countMarbles(board), "marble"));
        gCanvas->repaint();
    });
    gLastSearchBoard = board;
    gLastSearchFrame = Clock::now();
    if (delay >= 1) {
        GThread::msleep(delay);
    }
}

/* Called from compressBoard while the student's solver is running. */
void searchHook(const Grid<Marble>& board) {
    gBoardsSeen++;
    if (gStopRequested) {
        throw SearchStopped();
    }
    if (msSince(gLastProgress) > 150) {
        setStatus("Searching... " + plural(gBoardsSeen, "board") + " explored so far ("
                  + formatMs(msSince(gSolveStart)) + ")");
        gLastProgress = Clock::now();
    }
    if (gAnimateSearch) {
        showSearchFrame(board);
    }
}

struct SolveResult {
    bool finished;       // false if stopped
    bool found;
    Vector<Move> moves;
    string problem;      // non-empty if found but the moves are not a valid solution
    bool boardChanged;   // returned false but did not restore the board
    long boards;
    double ms;
};

/* Runs solvePuzzle on a copy of the current board. */
SolveResult runStudentSolver() {
    gBoardsSeen = 0;
    gStopRequested = false;
    gSolveStart = gLastProgress = gLastSearchFrame = Clock::now();
    gLastSearchBoard = gBoard;

    MarbleBoard board(gBoard);
    Vector<Move> moves;
    SolveResult result = { false, false, {}, "", false, 0, 0 };

    struct HookInstaller {   // removes the hook however solvePuzzle exits
        HookInstaller()  { MarbleInternal::compressHook = searchHook; }
        ~HookInstaller() { MarbleInternal::compressHook = nullptr; }
    } installer;
    try {
        result.found = solvePuzzle(board, moves);
        result.finished = true;
    } catch (const SearchStopped&) {
        // Stop was pressed; result.finished stays false
    }
    result.boards = gBoardsSeen;
    result.ms = msSince(gSolveStart);
    if (result.finished && !result.found) {
        result.boardChanged = board != MarbleBoard(gBoard);
    }
    if (result.found) {
        result.moves = moves;
        result.problem = checkSolution(gBoard, result.moves);
    }
    return result;
}

/* * * * * * Game actions * * * * * */

void updateButtons(bool busy = false) {
    bool canUndo = !busy && !gHistory.isEmpty();
    bool canStep = !busy && !gRedo.isEmpty();
    string playText = gPlaying ? "Pause" : "Play";
    onGuiThread([=] {
        gLoadButton->setEnabled(!busy);
        gRestartButton->setEnabled(!busy);
        gUndoButton->setEnabled(canUndo);
        gPlayButton->setEnabled(canStep);
        gPlayButton->setText(playText);
        gSolveButton->setEnabled(!busy);
        gStopButton->setEnabled(busy);
    });
}

void setPlaying(bool playing) {
    gPlaying = playing && !gRedo.isEmpty();
    updateButtons();
}

/* Updates everything that depends on the board after it changes. */
void boardChanged(bool reportGameOver) {
    updateCountText();
    updateButtons();
    if (reportGameOver) {
        int count = countMarbles(gBoard);
        if (isWon(gBoard)) {
            setStatus("Solved! Only one marble left.", kGoodColor);
        } else if (!hasLegalMove(gBoard)) {
            setStatus("No more moves, with " + plural(count, "marble")
                      + " left. Undo, or Restart.", kWarnColor);
        }
    }
}

void startGame(const Grid<Marble>& board, const string& name, const string& message) {
    gStartBoard = board;
    gBoard = board;
    gBoardName = name;
    gHistory.clear();
    gRedo.clear();
    gPlaying = false;
    gSolutionStatus = "";
    clearSelection();
    hideChangedMarks();
    showBoard(gBoard, true);
    onGuiThread([name] {
        gNameText->setText(name);
        gNameText->setLocation(gCanvas->getWidth() - gMargin - gNameText->getWidth(),
                               gMargin + gNameText->getFontAscent());
    });
    setStatus(message);
    boardChanged(false);
}

/* Makes a move (which must be legal) with animation, and records it. */
void doMove(const Move& move) {
    animateJump(move, false);
    flipMove(gBoard, move);
    gHistory.add(move);
}

void undoLastMove() {
    if (gHistory.isEmpty()) {
        return;
    }
    clearSelection();
    Move move = gHistory.remove(gHistory.size() - 1);
    animateJump(move, true);
    flipMove(gBoard, move);
    gRedo.add(move);
    setStatus("Undid " + move.start.toString() + " -> " + move.end.toString() + ".");
    boardChanged(false);
}

void stepForward() {
    if (gRedo.isEmpty()) {
        setPlaying(false);
        return;
    }
    clearSelection();
    Move move = gRedo.remove(gRedo.size() - 1);
    if (!whyIllegal(gBoard, move).empty()) {   // shouldn't happen, but just in case
        gRedo.clear();
        setPlaying(false);
        setStatus("The next move is no longer legal here, so it was discarded.", kWarnColor);
        boardChanged(false);
        return;
    }
    doMove(move);
    if (gRedo.isEmpty()) {
        gPlaying = false;
    }
    string message = "Jumped " + move.start.toString() + " -> " + move.end.toString() + ".";
    if (!gRedo.isEmpty()) {
        message += " " + plural(gRedo.size(), "move") + " to go.";
    }
    setStatus(gSolutionStatus.empty() ? message : gSolutionStatus + " " + message);
    boardChanged(gRedo.isEmpty());
}

void handleClick(double x, double y) {
    setPlaying(false);
    GridLocation loc;
    if (!locationAt(x, y, loc)) {
        clearSelection();
        return;
    }
    if (gBoard[loc] == MARBLE_OCCUPIED) {
        if (gHasSelection && gSelected == loc) {
            clearSelection();
            setStatus("");
            return;
        }
        Vector<Move> moves = legalMovesFrom(gBoard, loc);
        showSelection(loc, moves, kSelectColor);
        if (moves.isEmpty()) {
            setStatus("That marble has no jumps right now.", kWarnColor);
        } else {
            setStatus("Now click the hole to jump it to.");
        }
        return;
    }
    if (!gHasSelection) {
        setStatus("Click a marble first, then the hole to jump it to.", kWarnColor);
        return;
    }
    Move move = { gSelected, loc };
    string problem = whyIllegal(gBoard, move);
    if (!problem.empty()) {
        setStatus(problem, kErrorColor);
        return;
    }
    clearSelection();
    gSolutionStatus = "";
    // following the solution keeps the rest of it; any other move discards it
    if (!gRedo.isEmpty() && gRedo[gRedo.size() - 1] == move) {
        gRedo.remove(gRedo.size() - 1);
    } else {
        gRedo.clear();
    }
    doMove(move);
    setStatus("Jumped " + move.start.toString() + " -> " + move.end.toString() + ".");
    boardChanged(true);
}

void loadBoardFile() {
    setPlaying(false);
    string filename = GFileChooser::showOpenDialog(gWindow, "Choose a board", kBoardDir, "*.txt");
    if (filename.empty()) {
        return;   // canceled
    }
    try {
        Grid<Marble> board = loadBoard(filename);
        startGame(board, getRoot(getTail(filename)), "Loaded " + getTail(filename) + ".");
    } catch (const ErrorException& e) {
        GOptionPane::showMessageDialog(gWindow, e.getMessage(), "Can't load board",
                                       GOptionPane::MessageType::MESSAGE_ERROR);
    }
}

void solveFromHere() {
    setPlaying(false);
    clearSelection();
    if (isWon(gBoard)) {
        setStatus("This board is already solved!", kGoodColor);
        return;
    }
    setStatus("Running your solvePuzzle...");
    updateButtons(true);
    Grid<GImage*> savedMarbleAt = gMarbleAt;      // "Animate search" moves the images around
    Vector<GImage*> savedSpares = gSpareMarbles;
    SolveResult r = runStudentSolver();
    hideChangedMarks();
    onGuiThread([&] {
        for (GImage* img : gSpareMarbles) {
            img->setVisible(false);
        }
        for (const GridLocation& loc : gMarbleAt.locations()) {
            if (gMarbleAt[loc]) {
                gMarbleAt[loc]->setVisible(false);
            }
        }
        gMarbleAt = savedMarbleAt;
        gSpareMarbles = savedSpares;
        for (const GridLocation& loc : gMarbleAt.locations()) {
            if (gMarbleAt[loc]) {
                gMarbleAt[loc]->setOpacity(1);
                placeMarble(gMarbleAt[loc], centerOf(loc), gMarbleSize);
                gMarbleAt[loc]->setVisible(true);
            }
        }
    });
    showBoard(gBoard, false);
    updateCountText();
    discardCommands();   // drop clicks made while the solver was busy

    string stats = plural(r.boards, "board") + " explored in " + formatMs(r.ms);
    if (r.boards == 0) {
        stats = formatMs(r.ms) + "; your solver never called encode";
    }
    cout << "solvePuzzle on " << gBoardName << ": ";
    if (!r.finished) {
        setStatus("Stopped the search after " + stats + ".", kWarnColor);
        cout << "stopped after " << stats << endl;
    } else if (!r.found) {
        string message = "Your solver says this board can't be won (" + stats + ").";
        if (r.boardChanged) {
            message += " Warning: it returned false but did not put the board back the way it was.";
        }
        setStatus(message, r.boardChanged ? kWarnColor : kStatusColor);
        cout << "returned false (" << stats << ")" << endl;
    } else if (!r.problem.empty()) {
        setStatus("Your solver returned true, but its moves don't solve the board. " + r.problem,
                  kErrorColor);
        cout << "returned true, but the moves are not a solution: " << r.problem << endl
             << "  moves returned: " << r.moves << endl;
    } else {
        gRedo.clear();
        for (int i = r.moves.size() - 1; i >= 0; i--) {
            gRedo.add(r.moves[i]);
        }
        string length = integerToString(r.moves.size()) + "-move";
        setStatus("Found a " + length + " solution (" + stats + "). Playing it now.", kGoodColor);
        cout << "found a " << length << " solution (" << stats << ")" << endl;
        gSolutionStatus = "Found a " + length + " solution (" + stats + ").";
        gPlaying = true;
    }
    updateButtons();
}

/* * * * * * Building the window * * * * * */

GButton* makeButton(const string& text, CommandType cmd) {
    GButton* button = new GButton(text);
    button->setActionListener([cmd] { postCommand(cmd); });
    return button;
}

void drawBackground() {
    double size = BOARD_SIZE * gCell + 2 * gMargin;
    if (fileExists(kBackgroundImage)) {
        GImage wood(kBackgroundImage);
        wood.setBounds(0, 0, size, size);
        gCanvas->draw(&wood);
    } else {
        gCanvas->setBackground("#c8a165");
    }
    double hole = 0.5 * gCell;
    for (const GridLocation& loc : gStartBoard.locations()) {
        if (!isHole(gStartBoard, loc)) {
            continue;
        }
        GPoint c = centerOf(loc);
        GOval rim(c.x - hole / 2 - 2, c.y - hole / 2 - 1, hole + 4, hole + 4);
        rim.setFilled(true);
        rim.setColor("#f3dcb0");
        rim.setFillColor("#f3dcb0");
        rim.setOpacity(0.6);
        gCanvas->draw(&rim);
        GOval pit(c.x - hole / 2, c.y - hole / 2, hole, hole);
        pit.setFilled(true);
        pit.setColor("#4a3015");
        pit.setFillColor("#5e3d1c");
        gCanvas->draw(&pit);
    }
}

void buildWindow() {
    // size the board to fit comfortably on the screen
    gCell = max(56.0, min(84.0, (GWindow::getScreenHeight() - 300) / 7.6));
    gMargin = 0.3 * gCell;
    gMarbleSize = 0.86 * gCell;
    double canvasSize = BOARD_SIZE * gCell + 2 * gMargin;

    gWindow = new GWindow(false);
    gWindow->setTitle("Marble Solitaire");
    gWindow->setExitOnClose(true);
    gWindow->setResizable(false);
    gCanvas = gWindow->getCanvas();
    gCanvas->setAutoRepaint(false);
    gCanvas->setSize(canvasSize, canvasSize);

    gLoadButton = makeButton("Load board...", CMD_LOAD);
    gRestartButton = makeButton("Restart", CMD_RESTART);
    gUndoButton = makeButton("Undo", CMD_UNDO);
    gPlayButton = makeButton("Play", CMD_PLAY);
    gSolveButton = makeButton("Solve", CMD_SOLVE);
    gStopButton = new GButton("Stop");
    gStopButton->setActionListener([] { gStopRequested = true; });   // the event loop is busy solving
    for (GButton* b : { gLoadButton, gRestartButton, gUndoButton, gPlayButton,
                        gSolveButton, gStopButton }) {
        gWindow->addToRegion(b, GWindow::REGION_NORTH);
    }
    gUndoButton->setTooltip("Take back the last move");
    gPlayButton->setTooltip("Play the rest of the solution");
    gSolveButton->setTooltip("Run your solvePuzzle from the current board");
    gStopButton->setTooltip("Stop your solver (works once it calls encode)");

    gStatusLabel = new GLabel(" ");
    gStatusLabel->setFont("SansSerif-Bold-13");
    gStatusLabel->setWordWrap(true);
    gStatusLabel->setPreferredSize(canvasSize - 20, 44);   // room for two lines
    gSpeedSlider = new GSlider(0, 100, gSpeed);
    gSpeedSlider->setActionListener([] { gSpeed = gSpeedSlider->getValue(); });
    gAnimateBox = new GCheckBox("Animate search", false);
    gAnimateBox->setActionListener([] { gAnimateSearch = gAnimateBox->isChecked(); });
    gAnimateBox->setTooltip("While your solver runs, show every board it explores");

    GContainer* options = new GContainer(GContainer::LAYOUT_FLOW_HORIZONTAL);
    options->add(new GLabel("Speed"));
    options->add(gSpeedSlider);
    options->add(gAnimateBox);
    GContainer* south = new GContainer(GContainer::LAYOUT_FLOW_VERTICAL);
    south->add(gStatusLabel);
    south->add(options);
    gWindow->addToRegion(south, GWindow::REGION_SOUTH);

    // objects on the canvas, back to front (the background is drawn once the window is showing)
    gStartBoard = fullBoard();
    gChangedMark.resize(BOARD_SIZE, BOARD_SIZE);
    gMarbleAt.resize(BOARD_SIZE, BOARD_SIZE);
    for (const GridLocation& loc : gStartBoard.locations()) {
        gMarbleAt[loc] = nullptr;
        gChangedMark[loc] = nullptr;
        if (isHole(gStartBoard, loc)) {
            GPoint c = centerOf(loc);
            double size = gMarbleSize + 0.1 * gCell;
            GOval* mark = new GOval(c.x - size / 2, c.y - size / 2, size, size);
            mark->setColor(kChangedColor);
            mark->setLineWidth(3);
            mark->setVisible(false);
            gCanvas->add(mark);
            gChangedMark[loc] = mark;
        }
    }
    gSelectRing = new GOval();
    gSelectRing->setLineWidth(4);
    gSelectRing->setVisible(false);
    gCanvas->add(gSelectRing);
    for (int i = 0; i < 4; i++) {
        GOval* dot = new GOval();
        dot->setFilled(true);
        dot->setColor("#00000000");
        dot->setOpacity(0.7);
        dot->setVisible(false);
        gCanvas->add(dot);
        gTargetDots.add(dot);
    }
    gShadow = new GOval();
    gShadow->setFilled(true);
    gShadow->setColor("#00000000");
    gShadow->setFillColor("black");
    gShadow->setVisible(false);
    gCanvas->add(gShadow);
    for (int i = 0; i < kNumHoles; i++) {
        string file = kImageDir + "m" + integerToString(randomInteger(1, kNumMarbleImages)) + ".png";
        if (!fileExists(file)) {
            error("Marble Solitaire can't find the image file " + file);
        }
        GImage* img = new GImage(file);
        img->setVisible(false);
        gCanvas->add(img);
        gSpareMarbles.add(img);
    }
    gCountText = new GText("");
    gCountText->setFont("SansSerif-Bold-" + integerToString(int(gCell * 0.2)));
    gCountText->setColor("#3b2412");
    gCanvas->add(gCountText, gMargin, gMargin + gCell * 0.35);
    gNameText = new GText("");
    gNameText->setFont("SansSerif-Bold-" + integerToString(int(gCell * 0.17)));
    gNameText->setColor("#3b2412");
    gCanvas->add(gNameText);

    gCanvas->setMouseListener([](GEvent e) {
        if (e.getEventType() == MOUSE_PRESSED) {
            postCommand(CMD_CLICK, e.getX(), e.getY());
        }
    });

    gWindow->setLocation(0, 0);
    gWindow->setVisible(true);
    gWindow->setResizable(true);   // pack only sizes the window correctly while it is resizable
    gWindow->pack();
    gWindow->setResizable(false);
    drawBackground();

    // put the console next to the window if there is room, otherwise below it
    double screenW = GWindow::getScreenWidth(), screenH = GWindow::getScreenHeight();
    if (screenW - gWindow->getWidth() >= 480) {
        setConsoleLocation(gWindow->getWidth() + 10, 0);
        setConsoleSize(min(700.0, screenW - gWindow->getWidth() - 20), screenH * 0.6);
    } else {
        setConsoleLocation(0, gWindow->getHeight() + 40);
        setConsoleSize(gWindow->getWidth(), 200);
    }
}

void eventLoop() {
    while (true) {
        Command cmd;
        int gap = int(650 - 600 * gSpeed / 100.0);
        if (!waitForCommand(cmd, gPlaying ? gap : 250)) {
            if (gPlaying) {
                stepForward();
            }
            continue;
        }
        switch (cmd.type) {
            case CMD_CLICK:   handleClick(cmd.x, cmd.y); break;
            case CMD_LOAD:    loadBoardFile(); break;
            case CMD_RESTART: startGame(gStartBoard, gBoardName, "Restarted " + gBoardName + "."); break;
            case CMD_UNDO:    setPlaying(false); undoLastMove(); break;
            case CMD_PLAY:    setPlaying(!gPlaying); break;
            case CMD_SOLVE:   solveFromHere(); break;
        }
    }
}

} // end of private namespace

void marbleSolitaire() {
    buildWindow();
    startGame(fullBoard(), "full-board",
              "Click a marble, then click the hole to jump it to. "
              "Goal: leave only one marble on the board.");
    eventLoop();   // runs until the window is closed, which quits the program
}
