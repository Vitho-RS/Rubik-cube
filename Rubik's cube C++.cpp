#pragma comment(lib, "opengl32.lib")
#pragma comment(lib, "glu32.lib")
#pragma comment(lib, "user32.lib")
#pragma comment(lib, "gdi32.lib")

#include <windows.h>
#include <gl/gl.h>
#include <gl/glu.h>
#include <cmath>
#include <vector>
#include <string>
#include <ctime>
#include <cstdio>
#include <cstdarg>
#include <cstring>
#include <cctype>
#include <array>

// --- Constants ---
#define PI 3.14159265358979323846
const float DEGTORAD = (float)(PI / 180.0);
const float RADTODEG = (float)(180.0 / PI);

// --- Globals ---
int windowWidth = 800;
int windowHeight = 600;
int numberOfSides = 3;
float cubeSpeed = (float)(PI / 15.0);
int blockWidth = 50;

// FPS Globals
int frameCount = 0;
int currentFPS = 0;
DWORD lastTime = 0;
GLuint fontBase = 0;

// --- Structs ---
struct Color {
    int r, g, b;
    bool operator==(const Color& other) const { return r == other.r && g == other.g && b == other.b; }
    bool operator!=(const Color& other) const { return !(*this == other); }
};

Color CreateColor(int r, int g, int b) { return { r, g, b }; }

// Standard Colors
Color COL_WHITE = CreateColor(255, 255, 255);
Color COL_YELLOW = CreateColor(255, 255, 0);
Color COL_GREEN = CreateColor(0, 200, 80);
Color COL_BLUE = CreateColor(0, 80, 220);
Color COL_RED = CreateColor(220, 0, 0);
Color COL_ORANGE = CreateColor(255, 100, 0);
Color COL_BLACK = CreateColor(20, 20, 20);

struct PVector {
    float x, y, z;
    PVector(float x = 0, float y = 0, float z = 0) : x(x), y(y), z(z) {}
};

// --- Cube State Globals ---
int n = numberOfSides;
bool turning = false;
std::string moveQueue = ""; // internal format: uppercase = CW, lowercase = CCW (also supports x/X internally)
float rotationAngle = 0;
int rotationAxis = -1; // 0=X, 1=Y, 2=Z
int rotatingIndex = -1; // Which slice
bool turningClockwise = true;
bool turningWholeCube = false;

// Camera
float xRotationKey = 25.0f * DEGTORAD;
float yRotationKey = -45.0f * DEGTORAD;
int lastMouseX = 0, lastMouseY = 0;
bool isDragging = false;

// Forward declaration for demo macro
class Cube;
Cube* cube = nullptr;

char colorToFaceLetter(const Color& c);

// --- Text/Font Helpers ---

void BuildFont(HDC hDC) {
    fontBase = glGenLists(96);
    HFONT font = CreateFont(-24, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE, ANSI_CHARSET,
        OUT_TT_PRECIS, CLIP_DEFAULT_PRECIS, ANTIALIASED_QUALITY,
        FF_DONTCARE | DEFAULT_PITCH, "Arial");
    HFONT oldfont = (HFONT)SelectObject(hDC, font);
    wglUseFontBitmaps(hDC, 32, 96, fontBase);
    SelectObject(hDC, oldfont);
    DeleteObject(font);
}

void KillFont() {
    glDeleteLists(fontBase, 96);
}

void glPrint(int x, int y, const char* fmt, ...) {
    char text[256];
    va_list ap;
    if (fmt == NULL) return;
    va_start(ap, fmt);
    vsprintf_s(text, sizeof(text), fmt, ap);
    va_end(ap);

    glDisable(GL_LIGHTING);
    glDisable(GL_DEPTH_TEST);
    glMatrixMode(GL_PROJECTION);
    glPushMatrix();
    glLoadIdentity();
    gluOrtho2D(0, windowWidth, windowHeight, 0);
    glMatrixMode(GL_MODELVIEW);
    glPushMatrix();
    glLoadIdentity();
    glColor3f(1.0f, 1.0f, 1.0f);
    glRasterPos2i(x, y);
    glPushAttrib(GL_LIST_BIT);
    glListBase(fontBase - 32);
    glCallLists((GLsizei)strlen(text), GL_UNSIGNED_BYTE, text);
    glPopAttrib();
    glPopMatrix();
    glMatrixMode(GL_PROJECTION);
    glPopMatrix();
    glMatrixMode(GL_MODELVIEW);
    glEnable(GL_DEPTH_TEST);
    glEnable(GL_LIGHTING);
}

// --- Block Class ---
class Block {
public:
    PVector pos;
    Color colors[6]; // 0:Left, 1:Right, 2:Up, 3:Down, 4:Back, 5:Front

    Block(PVector p) : pos(p) {
        resetColors();
    }
    Block() : pos(0, 0, 0) {}

    void resetColors() {
        for (int i = 0; i < 6; i++) colors[i] = COL_BLACK;

        // Map positions (Coordinate: X=Right, Y=Up, Z=Front)
        if (pos.x == 0) colors[0] = COL_ORANGE; // Left
        if (pos.x == n - 1) colors[1] = COL_RED; // Right
        if (pos.y == n - 1) colors[2] = COL_WHITE; // Up (Top)
        if (pos.y == 0) colors[3] = COL_YELLOW; // Down (Bottom)
        if (pos.z == 0) colors[4] = COL_BLUE; // Back
        if (pos.z == n - 1) colors[5] = COL_GREEN; // Front
    }

    void turn(int axis, bool clockwise) {
        Color temp[6];
        for (int i = 0; i < 6; i++) temp[i] = colors[i];

        if (axis == 0) { // X-Axis
            if (clockwise) {
                colors[5] = temp[2]; // Front = Up
                colors[3] = temp[5]; // Down = Front
                colors[4] = temp[3]; // Back = Down
                colors[2] = temp[4]; // Up = Back
            } else {
                colors[4] = temp[2]; // Back = Up
                colors[3] = temp[4]; // Down = Back
                colors[5] = temp[3]; // Front = Down
                colors[2] = temp[5]; // Up = Front
            }
        }
        else if (axis == 1) { // Y-Axis
            if (clockwise) {
                colors[1] = temp[5]; // Right = Front
                colors[4] = temp[1]; // Back = Right
                colors[0] = temp[4]; // Left = Back
                colors[5] = temp[0]; // Front = Left
            } else {
                colors[0] = temp[5]; // Left = Front
                colors[4] = temp[0]; // Back = Left
                colors[1] = temp[4]; // Right = Back
                colors[5] = temp[1]; // Front = Right
            }
        }
        else if (axis == 2) { // Z-Axis
            if (clockwise) {
                colors[0] = temp[2]; // Left = Up
                colors[3] = temp[0]; // Down = Left
                colors[1] = temp[3]; // Right = Down
                colors[2] = temp[1]; // Up = Right
            } else {
                colors[1] = temp[2]; // Right = Up
                colors[3] = temp[1]; // Down = Right
                colors[0] = temp[3]; // Left = Down
                colors[2] = temp[0]; // Up = Left
            }
        }
    }

    void drawFace(int faceNo) {
        Color c = colors[faceNo];
        if (c == COL_BLACK) return;

        glDisable(GL_LIGHTING);

        float s = blockWidth / 2.0f;
        float st = s - 4.0f;
        float d = s + 0.5f;

        glColor3ub(c.r, c.g, c.b);

        glBegin(GL_QUADS);
        switch (faceNo) {
        case 0: // Left (-x)
            glNormal3f(-1, 0, 0); glVertex3f(-d, -st, st); glVertex3f(-d, -st, -st); glVertex3f(-d, st, -st); glVertex3f(-d, st, st); break;
        case 1: // Right (+x)
            glNormal3f(1, 0, 0); glVertex3f(d, -st, st); glVertex3f(d, st, st); glVertex3f(d, st, -st); glVertex3f(d, -st, -st); break;
        case 2: // Up (+y)
            glNormal3f(0, 1, 0); glVertex3f(-st, d, st); glVertex3f(-st, d, -st); glVertex3f(st, d, -st); glVertex3f(st, d, st); break;
        case 3: // Down (-y)
            glNormal3f(0, -1, 0); glVertex3f(-st, -d, st); glVertex3f(st, -d, st); glVertex3f(st, -d, -st); glVertex3f(-st, -d, -st); break;
        case 4: // Back (-z)
            glNormal3f(0, 0, -1); glVertex3f(-st, -st, -d); glVertex3f(-st, st, -d); glVertex3f(st, st, -d); glVertex3f(st, -st, -d); break;
        case 5: // Front (+z)
            glNormal3f(0, 0, 1); glVertex3f(-st, -st, d); glVertex3f(st, -st, d); glVertex3f(st, st, d); glVertex3f(-st, st, d); break;
        }
        glEnd();
        glEnable(GL_LIGHTING);
    }

    void show() {
        glColor3ub(20, 20, 20);
        float s = blockWidth / 2.0f - 1.0f;

        glBegin(GL_QUADS);
        // Front (+Z)
        glNormal3f(0, 0, 1); glVertex3f(-s, -s, s); glVertex3f(s, -s, s); glVertex3f(s, s, s); glVertex3f(-s, s, s);
        // Back (-Z)
        glNormal3f(0, 0, -1); glVertex3f(-s, -s, -s); glVertex3f(-s, s, -s); glVertex3f(s, s, -s); glVertex3f(s, -s, -s);
        // Top (+Y)
        glNormal3f(0, 1, 0); glVertex3f(-s, s, -s); glVertex3f(-s, s, s); glVertex3f(s, s, s); glVertex3f(s, s, -s);
        // Bottom (-Y)
        glNormal3f(0, -1, 0); glVertex3f(-s, -s, -s); glVertex3f(s, -s, -s); glVertex3f(s, -s, s); glVertex3f(-s, -s, s);
        // Right (+X)
        glNormal3f(1, 0, 0); glVertex3f(s, -s, -s); glVertex3f(s, s, -s); glVertex3f(s, s, s); glVertex3f(s, -s, s);
        // Left (-X)
        glNormal3f(-1, 0, 0); glVertex3f(-s, -s, -s); glVertex3f(-s, -s, s); glVertex3f(-s, s, s); glVertex3f(-s, s, -s);
        glEnd();

        for (int i = 0; i < 6; i++) drawFace(i);
    }
};

char colorToFaceLetter(const Color& c) {
    if (c == COL_WHITE)  return 'U';
    if (c == COL_RED)    return 'R';
    if (c == COL_GREEN)  return 'F';
    if (c == COL_YELLOW) return 'D';
    if (c == COL_ORANGE) return 'L';
    if (c == COL_BLUE)   return 'B';
    return '?';
}

// --- Cube Class ---
class Cube {
public:
    std::vector<std::vector<std::vector<Block>>> blocks;

    Cube() { init(); }

    void init() {
        blocks.resize(n);
        for (int i = 0; i < n; i++) {
            blocks[i].resize(n);
            for (int j = 0; j < n; j++) {
                blocks[i][j].resize(n);
                for (int k = 0; k < n; k++) {
                    blocks[i][j][k] = Block(PVector((float)i, (float)j, (float)k));
                }
            }
        }
    }

    void reset() {
        moveQueue.clear();
        turning = false;
        rotationAngle = 0;
        turningWholeCube = false;

        for (int i = 0; i < n; i++)
            for (int j = 0; j < n; j++)
                for (int k = 0; k < n; k++) {
                    blocks[i][j][k].pos = PVector((float)i, (float)j, (float)k);
                    blocks[i][j][k].resetColors();
                }
    }

    bool isSolved() const {
        return toFaceletString() == "UUUUUUUUURRRRRRRRRFFFFFFFFFDDDDDDDDDLLLLLLLLLBBBBBBBBB";
    }

    std::string toFaceletString() const {
        std::string out; out.reserve(54);
        for (int z = 0; z < n; ++z) for (int x = 0; x < n; ++x) out.push_back(colorToFaceLetter(blocks[x][n - 1][z].colors[2]));
        for (int y = n - 1; y >= 0; --y) for (int z = n - 1; z >= 0; --z) out.push_back(colorToFaceLetter(blocks[n - 1][y][z].colors[1]));
        for (int y = n - 1; y >= 0; --y) for (int x = 0; x < n; ++x) out.push_back(colorToFaceLetter(blocks[x][y][n - 1].colors[5]));
        for (int z = n - 1; z >= 0; --z) for (int x = 0; x < n; ++x) out.push_back(colorToFaceLetter(blocks[x][0][z].colors[3]));
        for (int y = n - 1; y >= 0; --y) for (int z = 0; z < n; ++z) out.push_back(colorToFaceLetter(blocks[0][y][z].colors[0]));
        for (int y = n - 1; y >= 0; --y) for (int x = n - 1; x >= 0; --x) out.push_back(colorToFaceLetter(blocks[x][y][0].colors[4]));
        return out;
    }

    bool validateFacelets(std::string* err = nullptr) const {
        if (n != 3) { if (err) *err = "Solver only supports 3x3"; return false; }
        std::string f = toFaceletString();
        if (f.size() != 54) { if (err) *err = "Bad facelet length"; return false; }
        int cnt[256] = { 0 };
        for (char c : f) cnt[(unsigned char)c]++;
        const char* faces = "URFDLB";
        for (int i = 0; i < 6; ++i) if (cnt[(unsigned char)faces[i]] != 9) { if (err) *err = "Invalid color counts"; return false; }
        if (f.find('?') != std::string::npos) { if (err) *err = "Unknown sticker color"; return false; }
        return true;
    }

    bool runSelfTests(bool verbose = true) {
        bool ok = true;
        reset();
        std::string solved = toFaceletString();
        if (solved != "UUUUUUUUURRRRRRRRRFFFFFFFFFDDDDDDDDDLLLLLLLLLBBBBBBBBB") {
            if (verbose) printf("[SELFTEST] Solved export mismatch: %s\n", solved.c_str());
            ok = false;
        }
        auto runAlg = [&](const char* name, const char* alg) {
            reset(); queueAlgorithm(alg);
            int guard = 0;
            while ((turning || !moveQueue.empty()) && guard++ < 1000) update();
            bool one = isSolved();
            if (verbose) printf("[SELFTEST] %s: %s\n", name, one ? "OK" : "FAIL");
            ok = ok && one;
        };
        runAlg("R4", "R R R R");
        runAlg("U4", "U U U U");
        runAlg("F4", "F F F F");
        runAlg("X4", "X X X X");
        runAlg("Y4", "Y Y Y Y");
        runAlg("Z4", "Z Z Z Z");
        runAlg("Alg+Inv", "R U R' U' U R U' R'");
        runAlg("SexyMove x6", "R U R' U' R U R' U' R U R' U' R U R' U' R U R' U' R U R' U'");
        return ok;
    }

    bool solveCurrentState() {
        std::string err;
        if (!validateFacelets(&err)) { printf("Solver validation failed: %s\n", err.c_str()); return false; }
        std::string facelets = toFaceletString();
#ifdef USE_MIN2PHASE
        Search search;
        std::string solution = search.solution(facelets, 21, 5000, 0);
        if (solution.empty() || solution.find("Error") != std::string::npos) {
            printf("Solver failed: %s\n", solution.c_str());
            return false;
        }
        printf("Solver: %s\n", solution.c_str());
        queueAlgorithm(solution);
        return true;
#else
        printf("Solver not compiled in. Add Search.h/Search.cpp (min2phase/Kociemba) and compile with -DUSE_MIN2PHASE\n");
        printf("Facelets: %s\n", facelets.c_str());
        return false;
#endif
    }

    void show() {
        float offset = (n - 1) * blockWidth / 2.0f;

        for (int i = 0; i < n; i++) {
            for (int j = 0; j < n; j++) {
                for (int k = 0; k < n; k++) {
                    glPushMatrix();

                    bool isRotating = false;
                    if (turning) {
                        if (turningWholeCube) isRotating = true;
                        else if (rotationAxis == 0 && i == rotatingIndex) isRotating = true;
                        else if (rotationAxis == 1 && j == rotatingIndex) isRotating = true;
                        else if (rotationAxis == 2 && k == rotatingIndex) isRotating = true;
                    }

                    if (isRotating) {
                        // FIX: make animation rotation match finalizeTurn's direction (right-hand rule)
                        float angle = rotationAngle * RADTODEG * (turningClockwise ? 1.0f : -1.0f);

                        if (rotationAxis == 0) glRotatef(angle, 1, 0, 0);
                        if (rotationAxis == 1) glRotatef(angle, 0, 1, 0);
                        if (rotationAxis == 2) glRotatef(angle, 0, 0, 1);
                    }

                    glTranslatef(i * blockWidth - offset, j * blockWidth - offset, k * blockWidth - offset);
                    blocks[i][j][k].show();
                    glPopMatrix();
                }
            }
        }
    }

    void finalizeTurn(int axis, int index, bool clockwise) {
        std::vector<std::vector<Block>> slice;
        slice.resize(n, std::vector<Block>(n));

        for (int a = 0; a < n; a++) {
            for (int b = 0; b < n; b++) {
                if (axis == 0) slice[a][b] = blocks[index][a][b];
                if (axis == 1) slice[a][b] = blocks[a][index][b];
                if (axis == 2) slice[a][b] = blocks[a][b][index];
            }
        }

        std::vector<std::vector<Block>> newSlice = slice;

        for (int r = 0; r < n; r++) {
            for (int c = 0; c < n; c++) {
                if (clockwise) {
                    if (axis == 0) newSlice[n - 1 - c][r] = slice[r][c];
                    if (axis == 1) newSlice[c][n - 1 - r] = slice[r][c];
                    if (axis == 2) newSlice[n - 1 - c][r] = slice[r][c];
                }
                else {
                    if (axis == 0) newSlice[c][n - 1 - r] = slice[r][c];
                    if (axis == 1) newSlice[n - 1 - c][r] = slice[r][c];
                    if (axis == 2) newSlice[c][n - 1 - r] = slice[r][c];
                }
            }
        }

        for (int r = 0; r < n; r++) {
            for (int c = 0; c < n; c++) {
                newSlice[r][c].turn(axis, clockwise);
            }
        }

        for (int a = 0; a < n; a++) {
            for (int b = 0; b < n; b++) {
                if (axis == 0) blocks[index][a][b] = newSlice[a][b];
                if (axis == 1) blocks[a][index][b] = newSlice[a][b];
                if (axis == 2) blocks[a][b][index] = newSlice[a][b];
            }
        }

        for (int i = 0; i < n; i++)
            for (int j = 0; j < n; j++)
                for (int k = 0; k < n; k++)
                    blocks[i][j][k].pos = PVector((float)i, (float)j, (float)k);
    }

    void update() {
        if (turning) {
            float speed = cubeSpeed;
            if (moveQueue.length() > 5) speed *= 2;

            rotationAngle += speed;
            if (rotationAngle >= PI / 2.0f) {
                rotationAngle = 0;
                turning = false;

                if (turningWholeCube) {
                    for (int idx = 0; idx < n; ++idx) {
                        finalizeTurn(rotationAxis, idx, turningClockwise);
                    }
                }
                else {
                    finalizeTurn(rotationAxis, rotatingIndex, turningClockwise);
                }

                if (!moveQueue.empty()) {
                    processMoveChar(moveQueue[0]);
                    moveQueue.erase(0, 1);
                }
            }
        }
        else if (!moveQueue.empty()) {
            processMoveChar(moveQueue[0]);
            moveQueue.erase(0, 1);
        }
    }

    void processMoveChar(char m) {
        turning = true;
        rotationAngle = 0;
        turningWholeCube = false;

        bool cw = true;
        char upper = (char)toupper((unsigned char)m);
        cw = (m == upper);

        switch (upper) {
        case 'L': rotationAxis = 0; rotatingIndex = 0; turningClockwise = !cw; break;
        case 'R': rotationAxis = 0; rotatingIndex = n - 1; turningClockwise = cw; break;
        case 'U': rotationAxis = 1; rotatingIndex = n - 1; turningClockwise = cw; break;
        case 'D': rotationAxis = 1; rotatingIndex = 0; turningClockwise = !cw; break;
        case 'F': rotationAxis = 2; rotatingIndex = n - 1; turningClockwise = cw; break;
        case 'B': rotationAxis = 2; rotatingIndex = 0; turningClockwise = !cw; break;

        case 'X': rotationAxis = 0; rotatingIndex = 0; turningWholeCube = true; turningClockwise = cw; break;
        case 'Y': rotationAxis = 1; rotatingIndex = 0; turningWholeCube = true; turningClockwise = cw; break;
        case 'Z': rotationAxis = 2; rotatingIndex = 0; turningWholeCube = true; turningClockwise = cw; break;

        default:
            turning = false;
            return;
        }
    }

    void queueMove(std::string moves) {
        moveQueue += moves;
    }

    void queueAlgorithm(const std::string& alg) {
        size_t i = 0;
        while (i < alg.size()) {
            unsigned char ch = (unsigned char)alg[i];

            if (isspace(ch)) { i++; continue; }

            char face = alg[i];
            char upper = (char)toupper((unsigned char)face);

            if (islower((unsigned char)face) &&
                (upper == 'R' || upper == 'L' || upper == 'U' || upper == 'D' || upper == 'F' || upper == 'B')) {
                i++;
                continue;
            }

            bool isMove =
                (upper == 'R' || upper == 'L' || upper == 'U' ||
                    upper == 'D' || upper == 'F' || upper == 'B' ||
                    upper == 'X' || upper == 'Y' || upper == 'Z');

            if (!isMove) {
                i++;
                continue;
            }

            i++;

            bool prime = false;
            int turns = 1;

            if (i < alg.size() && alg[i] == '\'') {
                prime = true;
                i++;
            }

            if (i < alg.size() && alg[i] == '2') {
                turns = 2;
                i++;
            }

            char internalBase = upper;
            if (prime) internalBase = (char)tolower((unsigned char)internalBase);

            for (int t = 0; t < turns; ++t) {
                moveQueue.push_back(internalBase);
            }
        }
    }

    void scramble() {
        std::string ops = "LRUDFB";
        for (int i = 0; i < 20; i++) {
            char m = ops[rand() % 6];
            if (rand() % 2 == 0) m = (char)tolower((unsigned char)m);
            moveQueue += m;
        }
    }
};

// --- Window & Input ---

void Resize(int w, int h) {
    if (h == 0) h = 1;
    windowWidth = w;
    windowHeight = h;
    glViewport(0, 0, w, h);
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    gluPerspective(45.0f, (float)w / h, 1.0f, 1000.0f);
    glMatrixMode(GL_MODELVIEW);
}

LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    switch (uMsg) {
    case WM_CLOSE: PostQuitMessage(0); return 0;
    case WM_SIZE: Resize(LOWORD(lParam), HIWORD(lParam)); return 0;

    case WM_KEYDOWN:
        if (wParam == VK_ESCAPE) PostQuitMessage(0);
        if (wParam == VK_SPACE) cube->scramble();
        if (wParam == 'P' && (GetKeyState(VK_CONTROL) & 0x8000)) cube->reset();
        if (wParam == 'S') cube->solveCurrentState();
        if (wParam == 'T') cube->runSelfTests(true);

        {
            bool shift = (GetKeyState(VK_SHIFT) & 0x8000) != 0;
            char key = 0;
            if (wParam == 'U') key = shift ? 'u' : 'U';
            if (wParam == 'D') key = shift ? 'd' : 'D';
            if (wParam == 'L') key = shift ? 'l' : 'L';
            if (wParam == 'R') key = shift ? 'r' : 'R';
            if (wParam == 'F') key = shift ? 'f' : 'F';
            if (wParam == 'B') key = shift ? 'b' : 'B';
            if (wParam == 'X') key = shift ? 'x' : 'X';
            if (wParam == 'Y') key = shift ? 'y' : 'Y';
            if (wParam == 'Z') key = shift ? 'z' : 'Z';

            if (key != 0) cube->queueMove(std::string(1, key));
        }

        if (wParam == '1') cube->queueAlgorithm("R U R' U'");
        if (wParam == '2') cube->queueAlgorithm("F R U R' U' F'");
        if (wParam == '3') cube->queueAlgorithm("R2 U2 F2");

        if (wParam == VK_LEFT) yRotationKey -= 0.1f;
        if (wParam == VK_RIGHT) yRotationKey += 0.1f;
        if (wParam == VK_UP) xRotationKey -= 0.1f;
        if (wParam == VK_DOWN) xRotationKey += 0.1f;
        return 0;

    case WM_LBUTTONDOWN:
        lastMouseX = LOWORD(lParam);
        lastMouseY = HIWORD(lParam);
        isDragging = true;
        return 0;

    case WM_LBUTTONUP:
        isDragging = false;
        return 0;

    case WM_MOUSEMOVE:
        if (isDragging) {
            int x = LOWORD(lParam);
            int y = HIWORD(lParam);
            yRotationKey += (x - lastMouseX) * 0.005f;
            xRotationKey += (y - lastMouseY) * 0.005f;
            lastMouseX = x;
            lastMouseY = y;
        }
        return 0;
    }
    return DefWindowProc(hwnd, uMsg, wParam, lParam);
}

void DrawScene() {
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glLoadIdentity();

    gluLookAt(0, 0, 600, 0, 0, 0, 0, 1, 0);

    glRotatef(xRotationKey * RADTODEG, 1, 0, 0);
    glRotatef(yRotationKey * RADTODEG, 0, 1, 0);

    cube->show();
    cube->update();

    frameCount++;
    DWORD currentTime = GetTickCount();
    if (currentTime - lastTime >= 1000) {
        currentFPS = frameCount;
        frameCount = 0;
        lastTime = currentTime;
    }

    glPrint(10, 24, "FPS: %d", currentFPS);
    glPrint(10, 48, "Space: Scramble | Ctrl+P: Reset");
    glPrint(10, 72, "Keys: U D L R F B X Y Z (Shift = prime)");
    glPrint(10, 96, "T: Self-tests");
}

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow) {
    srand((unsigned int)time(0));

    WNDCLASSEX wc = { sizeof(WNDCLASSEX), CS_OWNDC, WindowProc, 0, 0, hInstance, NULL, LoadCursor(NULL, IDC_ARROW), NULL, NULL, "CubeApp", NULL };
    RegisterClassEx(&wc);
    HWND hwnd = CreateWindowEx(0, "CubeApp", "Rubik's Cube C++", WS_OVERLAPPEDWINDOW | WS_VISIBLE, 100, 100, 800, 600, NULL, NULL, hInstance, NULL);

    HDC hdc = GetDC(hwnd);
    PIXELFORMATDESCRIPTOR pfd = { sizeof(PIXELFORMATDESCRIPTOR), 1, PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL | PFD_DOUBLEBUFFER, PFD_TYPE_RGBA, 24, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 32, 0, 0, PFD_MAIN_PLANE, 0, 0, 0, 0 };
    int format = ChoosePixelFormat(hdc, &pfd);
    SetPixelFormat(hdc, format, &pfd);
    HGLRC hrc = wglCreateContext(hdc);
    wglMakeCurrent(hdc, hrc);

    BuildFont(hdc);

    glEnable(GL_DEPTH_TEST);
    glEnable(GL_NORMALIZE);
    glClearColor(0.2f, 0.2f, 0.2f, 1.0f);

    glEnable(GL_LIGHTING);
    glEnable(GL_LIGHT0);
    glEnable(GL_COLOR_MATERIAL);
    GLfloat light_pos[] = { 300.0f, 400.0f, 500.0f, 1.0f };
    glLightfv(GL_LIGHT0, GL_POSITION, light_pos);
    GLfloat ambient[] = { 0.2f, 0.2f, 0.2f, 1.0f };
    glLightfv(GL_LIGHT0, GL_AMBIENT, ambient);

    cube = new Cube();
    lastTime = GetTickCount();

    MSG msg;
    bool running = true;
    while (running) {
        if (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE)) {
            if (msg.message == WM_QUIT) running = false;
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
        else {
            DrawScene();
            SwapBuffers(hdc);
            Sleep(1);
        }
    }

    KillFont();
    delete cube;
    wglMakeCurrent(NULL, NULL);
    wglDeleteContext(hrc);
    ReleaseDC(hwnd, hdc);
    DestroyWindow(hwnd);
    return 0;
}