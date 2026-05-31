#include "games.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <deque>
#include <random>
#include <string>
#include <vector>

// Missionaries & Cannibals is a state puzzle rather than a real-time game. The
// model records people on the left bank, people currently in the boat, and
// which bank holds the boat. Right-bank counts are derived from those values.
class MissionariesGame final : public Game {
    int leftM = 3, leftC = 3, boatM = 0, boatC = 0;
    bool boatLeft = true;
    std::wstring message;

    bool ValidBank(int m, int c) const {
        // A bank is safe when it has no missionaries or when missionaries are
        // not outnumbered by cannibals.
        return m == 0 || m >= c;
    }

    bool Valid() const {
        int rightM = 3 - leftM - boatM;
        int rightC = 3 - leftC - boatC;
        return ValidBank(leftM, leftC) && ValidBank(rightM, rightC) && leftM >= 0 && leftC >= 0 && rightM >= 0 && rightC >= 0 && boatM + boatC <= 2;
    }

    void Load(bool missionary) {
        int& bankCount = missionary ? leftM : leftC;
        int& boatCount = missionary ? boatM : boatC;
        int rightBank = missionary ? 3 - leftM - boatM : 3 - leftC - boatC;
        if (boatM + boatC >= 2) { message = L"The boat carries at most two."; return; }
        if (boatLeft) {
            if (bankCount <= 0) return;
            --bankCount;
            ++boatCount;
        } else {
            if (rightBank <= 0) return;
            ++boatCount;
        }
        message.clear();
    }

    void Unload(bool missionary) {
        int& bankCount = missionary ? leftM : leftC;
        int& boatCount = missionary ? boatM : boatC;
        if (boatCount <= 0) return;
        --boatCount;
        if (boatLeft) ++bankCount;
        message.clear();
    }

    void Cross() {
        // Crossing updates derived ownership in two stages: return the boat's
        // occupants to the bank it left, flip sides, then remove them from the
        // new bank representation. The boat itself retains its passengers.
        if (boatM + boatC == 0) { message = L"Load someone before crossing."; return; }
        if (!boatLeft) {
            leftM += boatM;
            leftC += boatC;
        }
        boatLeft = !boatLeft;
        if (boatLeft) {
            leftM -= boatM;
            leftC -= boatC;
        }
        if (!Valid()) {
            message = L"Unsafe bank. Restart and try another crossing.";
        } else {
            message.clear();
        }
    }

public:
    void Reset() override {
        leftM = leftC = 3;
        boatM = boatC = 0;
        boatLeft = true;
        message = L"Click M/C to load, unload, then Cross.";
    }

    MissionariesGame() { Reset(); }

    void DrawPeople(HDC dc, int m, int c, int x, int y) {
        for (int i = 0; i < m; ++i) {
            EllipseFill(dc, Rect{ x + i * 28, y, 22, 22 }, RGB(91, 184, 232));
            CenterText(dc, Rect{ x + i * 28, y + 22, 22, 20 }, L"M", 14, RGB(220, 235, 245), FW_BOLD);
        }
        for (int i = 0; i < c; ++i) {
            EllipseFill(dc, Rect{ x + i * 28, y + 54, 22, 22 }, RGB(224, 103, 89));
            CenterText(dc, Rect{ x + i * 28, y + 76, 22, 20 }, L"C", 14, RGB(245, 224, 220), FW_BOLD);
        }
    }

    void Draw(HDC dc) override {
        Header(dc, L"Missionaries & Cannibals", L"Move everyone across. Missionaries can never be outnumbered on either bank. R restarts, Esc returns.");
        Fill(dc, Rect{ 60, 210, 780, 150 }, RGB(45, 98, 129));
        Fill(dc, Rect{ 40, 100, 190, 430 }, RGB(45, 82, 52));
        Fill(dc, Rect{ 670, 100, 190, 430 }, RGB(45, 82, 52));
        Rect boat{ boatLeft ? 270 : 520, 282, 110, 42 };
        Fill(dc, boat, RGB(131, 96, 58));
        CenterText(dc, boat, L"Boat", 18, RGB(255, 244, 221), FW_BOLD);
        DrawPeople(dc, leftM, leftC, 78, 148);
        DrawPeople(dc, 3 - leftM - boatM, 3 - leftC - boatC, 710, 148);
        DrawPeople(dc, boatM, boatC, boat.x + 20, boat.y - 88);
        CenterText(dc, Rect{ 310, 470, 90, 38 }, L"Load M", 18, RGB(235, 240, 245), FW_BOLD);
        CenterText(dc, Rect{ 410, 470, 90, 38 }, L"Load C", 18, RGB(235, 240, 245), FW_BOLD);
        CenterText(dc, Rect{ 510, 470, 120, 38 }, L"Unload", 18, RGB(235, 240, 245), FW_BOLD);
        CenterText(dc, Rect{ 640, 470, 90, 38 }, L"Cross", 18, RGB(235, 240, 245), FW_BOLD);
        Frame(dc, Rect{ 310, 470, 90, 38 }, RGB(120, 135, 151));
        Frame(dc, Rect{ 410, 470, 90, 38 }, RGB(120, 135, 151));
        Frame(dc, Rect{ 510, 470, 120, 38 }, RGB(120, 135, 151));
        Frame(dc, Rect{ 640, 470, 90, 38 }, RGB(120, 135, 151));
        if (leftM == 0 && leftC == 0 && boatM == 0 && boatC == 0 && !boatLeft) {
            Text(dc, 318, 548, L"Solved. Everyone crossed safely.", 24, RGB(111, 225, 158), FW_BOLD);
        } else if (!message.empty()) {
            Text(dc, 246, 548, message, 20, RGB(237, 216, 130), FW_BOLD);
        }
    }

    void MouseDown(int x, int y) override {
        if (Rect{ 310, 470, 90, 38 }.Contains(x, y)) Load(true);
        else if (Rect{ 410, 470, 90, 38 }.Contains(x, y)) Load(false);
        else if (Rect{ 510, 470, 120, 38 }.Contains(x, y)) { Unload(true); Unload(false); }
        else if (Rect{ 640, 470, 90, 38 }.Contains(x, y)) Cross();
    }

    void KeyDown(WPARAM key) override {
        Game::KeyDown(key);
        if (key == 'M') Load(true);
        if (key == 'C') Load(false);
        if (key == 'U') { Unload(true); Unload(false); }
        if (key == VK_SPACE || key == VK_RETURN) Cross();
    }
};

std::unique_ptr<Game> CreateMissionariesGame() {
    return std::make_unique<MissionariesGame>();
}

