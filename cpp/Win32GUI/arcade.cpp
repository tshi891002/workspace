#include "arcade.h"

// RECT uses right and bottom edges rather than width and height. Keeping this
// conversion at the GDI boundary lets gameplay code work in friendlier units.
RECT Rect::ToWin() const {
    return RECT{ x, y, x + w, y + h };
}

bool Rect::Contains(int px, int py) const {
    return px >= x && px < x + w && py >= y && py < y + h;
}

void Fill(HDC dc, const Rect& r, COLORREF color) {
    // CreateSolidBrush allocates a GDI resource owned by this process. Every
    // created brush must be released with DeleteObject after FillRect uses it.
    HBRUSH brush = CreateSolidBrush(color);
    RECT wr = r.ToWin();
    FillRect(dc, &wr, brush);
    DeleteObject(brush);
}

void Frame(HDC dc, const Rect& r, COLORREF color, int width) {
    // GDI draws with objects selected into an HDC. Rectangle would normally use
    // both the current pen and brush, so select HOLLOW_BRUSH for an outline.
    // Restore the previous objects before deleting the temporary pen.
    HPEN pen = CreatePen(PS_SOLID, width, color);
    HGDIOBJ oldPen = SelectObject(dc, pen);
    HGDIOBJ oldBrush = SelectObject(dc, GetStockObject(HOLLOW_BRUSH));
    Rectangle(dc, r.x, r.y, r.x + r.w, r.y + r.h);
    SelectObject(dc, oldBrush);
    SelectObject(dc, oldPen);
    DeleteObject(pen);
}

void EllipseFill(HDC dc, const Rect& r, COLORREF fill, COLORREF outline) {
    // Ellipse uses the selected brush for its interior and selected pen for its
    // outline. As with Frame, selection is temporary and must be undone.
    HBRUSH brush = CreateSolidBrush(fill);
    HPEN pen = CreatePen(PS_SOLID, 1, outline);
    HGDIOBJ oldBrush = SelectObject(dc, brush);
    HGDIOBJ oldPen = SelectObject(dc, pen);
    Ellipse(dc, r.x, r.y, r.x + r.w, r.y + r.h);
    SelectObject(dc, oldPen);
    SelectObject(dc, oldBrush);
    DeleteObject(pen);
    DeleteObject(brush);
}

void Text(HDC dc, int x, int y, const std::wstring& text, int size, COLORREF color, int weight) {
    // The W suffix selects the Unicode Win32 API. Transparent background mode
    // allows text to sit on top of the already-painted game scene.
    HFONT font = CreateFontW(size, 0, 0, 0, weight, FALSE, FALSE, FALSE, DEFAULT_CHARSET,
                             OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
                             DEFAULT_PITCH | FF_SWISS, L"Segoe UI");
    HGDIOBJ oldFont = SelectObject(dc, font);
    SetBkMode(dc, TRANSPARENT);
    SetTextColor(dc, color);
    TextOutW(dc, x, y, text.c_str(), static_cast<int>(text.size()));
    SelectObject(dc, oldFont);
    DeleteObject(font);
}

void CenterText(HDC dc, const Rect& r, const std::wstring& text, int size, COLORREF color, int weight) {
    // DrawTextW provides alignment flags that TextOutW does not. END_ELLIPSIS
    // protects compact controls if a future label becomes unexpectedly long.
    HFONT font = CreateFontW(size, 0, 0, 0, weight, FALSE, FALSE, FALSE, DEFAULT_CHARSET,
                             OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
                             DEFAULT_PITCH | FF_SWISS, L"Segoe UI");
    HGDIOBJ oldFont = SelectObject(dc, font);
    SetBkMode(dc, TRANSPARENT);
    SetTextColor(dc, color);
    RECT wr = r.ToWin();
    DrawTextW(dc, text.c_str(), -1, &wr, DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
    SelectObject(dc, oldFont);
    DeleteObject(font);
}

void Header(HDC dc, const std::wstring& title, const std::wstring& help) {
    Text(dc, 26, 18, title, 30, RGB(250, 252, 255), FW_BOLD);
    Text(dc, 28, 54, help, 16, RGB(166, 176, 188));
    Frame(dc, Rect{ 18, 86, ClientW - 36, 1 }, RGB(54, 63, 75));
}

std::mt19937& Rng() {
    // GetTickCount is adequate as a casual-game seed. It is not suitable for
    // cryptography because it is only the elapsed milliseconds since startup.
    static std::mt19937 rng{ static_cast<unsigned int>(GetTickCount()) };
    return rng;
}
