#ifndef UNICODE
#define UNICODE
#endif
#ifndef _UNICODE
#define _UNICODE
#endif

#include <windows.h>
#include <mmsystem.h>
#include <cmath>
#include <cstdlib>
#include <ctime>
#include <thread>
#include <atomic>
#include <vector>
#include <algorithm>
#include <random>

#pragma comment(lib, "winmm.lib")

constexpr int TIMER_ID = 1;
constexpr int TIMER_MS = 40;
constexpr int SAMPLE_RATE = 8000;
constexpr int CHANNELS = 1;
constexpr int BITS = 8;
constexpr int BLOCK_SAMPLES = 2048;
constexpr int WAVE_BUFFERS = 10;
constexpr DWORD EXIT_HOTKEY_MOD = MOD_CONTROL | MOD_SHIFT;
constexpr UINT  EXIT_HOTKEY_KEY = 'X';

int g_virtX = 0, g_virtY = 0;
int g_virtW = 0, g_virtH = 0;

HWND g_hWnd = nullptr;
HDC g_hdcOverlay = nullptr;
HBITMAP g_hbmOverlay = nullptr;
void* g_pvBits = nullptr;

HWAVEOUT g_hWave = nullptr;
WAVEHDR g_waveHeaders[WAVE_BUFFERS];
std::atomic<bool> g_audioRunning = true;
std::thread g_audioThread;
std::atomic<DWORD> g_t = 0;

using BytebeatFunc = BYTE(*)(DWORD);
BytebeatFunc g_currentFormula = nullptr;
std::atomic<int> g_formulaIndex = 0;
DWORD g_formulaSwitchTime = 0;

BytebeatFunc g_formulas[] = {
    [](DWORD t) -> BYTE { return static_cast<BYTE>((t * ((t >> 9 | t >> 13) & 25 & t >> 6)) & 0xFF); },
    [](DWORD t) -> BYTE { return static_cast<BYTE>((t * (t >> 8 | t >> 11 | t >> 13)) & 0xFF); },
    [](DWORD t) -> BYTE { return static_cast<BYTE>(((t >> 7 | t >> 11) * (t & 0x34)) & 0xFF); },
    [](DWORD t) -> BYTE { return static_cast<BYTE>(((t * (t >> 7)) ^ (t >> 11)) & 0xFF); },
    [](DWORD t) -> BYTE { return static_cast<BYTE>((static_cast<int>(sin(static_cast<double>(t) * 0.005) * 127 + 128) ^ (t >> 6)) & 0xFF); },
    [](DWORD t) -> BYTE { return static_cast<BYTE>(((t * (t >> 5)) | (t >> 9) | (t >> 12)) & 0xFF); }
};
constexpr int FORMULA_COUNT = sizeof(g_formulas) / sizeof(g_formulas[0]);

BYTE currentBytebeat(DWORD t) {
    BytebeatFunc f = g_currentFormula;
    if (!f) f = g_formulas[0];
    return f(t);
}

void fillBuffer(WAVEHDR& hdr) {
    BYTE* buf = reinterpret_cast<BYTE*>(hdr.lpData);
    for (int i = 0; i < BLOCK_SAMPLES; ++i) {
        DWORD t = g_t.fetch_add(1);
        buf[i] = currentBytebeat(t);
    }
}

void audioThreadFunc() {
    g_currentFormula = g_formulas[0];
    g_formulaSwitchTime = GetTickCount() + 3000;
    while (g_audioRunning) {
        DWORD now = GetTickCount();
        if (now >= g_formulaSwitchTime) {
            g_formulaSwitchTime = now + 2500 + (rand() % 2000);
            int next = (g_formulaIndex.load() + 1) % FORMULA_COUNT;
            g_formulaIndex.store(next);
            g_currentFormula = g_formulas[next];
        }
        for (int i = 0; i < WAVE_BUFFERS; ++i) {
            if (g_waveHeaders[i].dwFlags & WHDR_DONE) {
                waveOutUnprepareHeader(g_hWave, &g_waveHeaders[i], sizeof(WAVEHDR));
                fillBuffer(g_waveHeaders[i]);
                waveOutPrepareHeader(g_hWave, &g_waveHeaders[i], sizeof(WAVEHDR));
                waveOutWrite(g_hWave, &g_waveHeaders[i], sizeof(WAVEHDR));
            }
        }
        Sleep(1);
    }
}

bool initAudio(HWND hwnd) {
    WAVEFORMATEX wfx{};
    wfx.wFormatTag = WAVE_FORMAT_PCM;
    wfx.nChannels = CHANNELS;
    wfx.nSamplesPerSec = SAMPLE_RATE;
    wfx.wBitsPerSample = BITS;
    wfx.nBlockAlign = (wfx.nChannels * wfx.wBitsPerSample) / 8;
    wfx.nAvgBytesPerSec = wfx.nSamplesPerSec * wfx.nBlockAlign;
    wfx.cbSize = 0;
    MMRESULT res = waveOutOpen(&g_hWave, WAVE_MAPPER, &wfx,
                               reinterpret_cast<DWORD_PTR>(hwnd), 0, CALLBACK_WINDOW);
    if (res != MMSYSERR_NOERROR) return false;
    for (int i = 0; i < WAVE_BUFFERS; ++i) {
        memset(&g_waveHeaders[i], 0, sizeof(WAVEHDR));
        g_waveHeaders[i].lpData = new CHAR[BLOCK_SAMPLES];
        g_waveHeaders[i].dwBufferLength = BLOCK_SAMPLES;
        fillBuffer(g_waveHeaders[i]);
        waveOutPrepareHeader(g_hWave, &g_waveHeaders[i], sizeof(WAVEHDR));
        waveOutWrite(g_hWave, &g_waveHeaders[i], sizeof(WAVEHDR));
    }
    g_audioThread = std::thread(audioThreadFunc);
    return true;
}

void stopAudio() {
    g_audioRunning = false;
    if (g_audioThread.joinable()) g_audioThread.join();
    if (g_hWave) {
        waveOutReset(g_hWave);
        for (int i = 0; i < WAVE_BUFFERS; ++i) {
            if (g_waveHeaders[i].lpData) {
                waveOutUnprepareHeader(g_hWave, &g_waveHeaders[i], sizeof(WAVEHDR));
                delete[] g_waveHeaders[i].lpData;
            }
        }
        waveOutClose(g_hWave);
        g_hWave = nullptr;
    }
}

bool InitOverlay() {
    g_virtX = GetSystemMetrics(SM_XVIRTUALSCREEN);
    g_virtY = GetSystemMetrics(SM_YVIRTUALSCREEN);
    g_virtW = GetSystemMetrics(SM_CXVIRTUALSCREEN);
    g_virtH = GetSystemMetrics(SM_CYVIRTUALSCREEN);

    g_hWnd = CreateWindowEx(
        WS_EX_TOPMOST | WS_EX_TRANSPARENT | WS_EX_LAYERED | WS_EX_NOACTIVATE,
        L"STATIC", nullptr,
        WS_POPUP,
        g_virtX, g_virtY, g_virtW, g_virtH,
        nullptr, nullptr, GetModuleHandle(nullptr), nullptr);
    if (!g_hWnd) return false;

    BITMAPINFO bmi{};
    bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bmi.bmiHeader.biWidth = g_virtW;
    bmi.bmiHeader.biHeight = -g_virtH;
    bmi.bmiHeader.biPlanes = 1;
    bmi.bmiHeader.biBitCount = 32;
    bmi.bmiHeader.biCompression = BI_RGB;

    HDC hdcScreen = GetDC(nullptr);
    g_hdcOverlay = CreateCompatibleDC(hdcScreen);
    g_hbmOverlay = CreateDIBSection(hdcScreen, &bmi, DIB_RGB_COLORS, &g_pvBits, nullptr, 0);
    SelectObject(g_hdcOverlay, g_hbmOverlay);
    ReleaseDC(nullptr, hdcScreen);

    ShowWindow(g_hWnd, SW_SHOW);
    return true;
}

void UpdateOverlay() {
    HDC hdcScreen = GetDC(nullptr);
    BLENDFUNCTION blend{};
    blend.BlendOp = AC_SRC_OVER;
    blend.SourceConstantAlpha = 255;
    blend.AlphaFormat = 0;

    POINT ptSrc = { 0, 0 };
    POINT ptDst = { g_virtX, g_virtY };
    SIZE sizeWnd = { g_virtW, g_virtH };

    UpdateLayeredWindow(g_hWnd, hdcScreen, &ptDst, &sizeWnd,
                        g_hdcOverlay, &ptSrc, 0, &blend, ULW_ALPHA);
    ReleaseDC(nullptr, hdcScreen);
}

void captureScreenToOverlay() {
    HDC hdcScreen = GetDC(nullptr);
    BitBlt(g_hdcOverlay, 0, 0, g_virtW, g_virtH, hdcScreen, g_virtX, g_virtY, SRCCOPY);
    ReleaseDC(nullptr, hdcScreen);

    BYTE* p = static_cast<BYTE*>(g_pvBits);
    for (int i = 0; i < g_virtW * g_virtH * 4; i += 4)
        p[i + 3] = 255;
}

using PixelModifier = void(*)(BYTE* pixels, int w, int h);
using OverlayDrawFunc = void(*)(HDC hdc, int w, int h);

enum EffectType { PIXEL, OVERLAY_DRAW };

struct Effect {
    EffectType type;
    union {
        PixelModifier pixelFunc;
        OverlayDrawFunc overlayFunc;
    };

    Effect(PixelModifier f) : type(PIXEL) { pixelFunc = f; }
    Effect(OverlayDrawFunc f) : type(OVERLAY_DRAW) { overlayFunc = f; }
};

void effectInvertPixels(BYTE* pixels, int w, int h) {
    for (int i = 0; i < w * h * 4; i += 4) {
        pixels[i + 0] ^= 0xFF;
        pixels[i + 1] ^= 0xFF;
        pixels[i + 2] ^= 0xFF;
        pixels[i + 3] = 255;
    }
}

void effectMeltingPixels(BYTE* pixels, int w, int h) {
    static float t = 0.0f;
    t += 0.15f;
    std::vector<BYTE> src(pixels, pixels + w * h * 4);
    for (int y = 0; y < h; ++y) {
        for (int x = 0; x < w; ++x) {
            float shift = sinf(x * 0.02f + t) * 15.0f + cosf(y * 0.015f + t * 0.7f) * 10.0f;
            int newY = std::clamp(static_cast<int>(y + shift), 0, h - 1);
            int dstIdx = (y * w + x) * 4;
            int srcIdx = (newY * w + x) * 4;
            memcpy(&pixels[dstIdx], &src[srcIdx], 4);
        }
    }
}

void effectWavePixels(BYTE* pixels, int w, int h) {
    static float phase = 0.0f;
    phase += 0.2f;
    std::vector<BYTE> src(pixels, pixels + w * h * 4);
    for (int y = 0; y < h; ++y) {
        float ampX = 15.0f + sinf(y * 0.1f + phase) * 10.0f;
        float ampY = 10.0f + cosf(y * 0.05f + phase * 0.8f) * 8.0f;
        for (int x = 0; x < w; ++x) {
            int srcX = std::clamp(static_cast<int>(x + sinf(y * 0.02f + phase) * ampX), 0, w - 1);
            int srcY = std::clamp(static_cast<int>(y + cosf(x * 0.03f + phase) * ampY), 0, h - 1);
            memcpy(&pixels[(y * w + x) * 4], &src[(srcY * w + srcX) * 4], 4);
        }
    }
}

void effectPixelSortPixels(BYTE* pixels, int w, int h) {
    for (int x = 0; x < w; x += (rand() % 8 + 5)) {
        int startY = rand() % (h / 4);
        int endY = h - rand() % (h / 4);
        if (startY >= endY) continue;
        std::vector<DWORD> column(endY - startY);
        for (int y = startY; y < endY; ++y)
            column[y - startY] = *reinterpret_cast<DWORD*>(&pixels[(y * w + x) * 4]);
        std::sort(column.begin(), column.end(), [](DWORD a, DWORD b) {
            BYTE* pa = reinterpret_cast<BYTE*>(&a);
            BYTE* pb = reinterpret_cast<BYTE*>(&b);
            return (pa[2] * 0.299f + pa[1] * 0.587f + pa[0] * 0.114f) <
                   (pb[2] * 0.299f + pb[1] * 0.587f + pb[0] * 0.114f);
        });
        for (int y = startY; y < endY; ++y)
            *reinterpret_cast<DWORD*>(&pixels[(y * w + x) * 4]) = column[y - startY];
    }
}

void effectSwirlPixels(BYTE* pixels, int w, int h) {
    static float angle = 0.0f;
    angle += 0.01f;
    float cx = w / 2.0f, cy = h / 2.0f;
    float maxR = sqrtf(cx*cx + cy*cy);
    std::vector<BYTE> src(pixels, pixels + w * h * 4);
    for (int y = 0; y < h; ++y) {
        for (int x = 0; x < w; ++x) {
            float dx = x - cx, dy = y - cy;
            float r = sqrtf(dx*dx + dy*dy);
            float a = atan2f(dy, dx) + angle * (r / maxR) * 5.0f;
            int srcX = std::clamp(static_cast<int>(cx + r * cosf(a)), 0, w - 1);
            int srcY = std::clamp(static_cast<int>(cy + r * sinf(a)), 0, h - 1);
            memcpy(&pixels[(y * w + x) * 4], &src[(srcY * w + srcX) * 4], 4);
        }
    }
}

void effectPixelatePixels(BYTE* pixels, int w, int h) {
    int block = 8 + rand() % 16;
    for (int y = 0; y < h; y += block) {
        for (int x = 0; x < w; x += block) {
            int avgR = 0, avgG = 0, avgB = 0, count = 0;
            for (int dy = 0; dy < block && y + dy < h; ++dy) {
                for (int dx = 0; dx < block && x + dx < w; ++dx) {
                    int idx = ((y + dy) * w + (x + dx)) * 4;
                    avgB += pixels[idx + 0];
                    avgG += pixels[idx + 1];
                    avgR += pixels[idx + 2];
                    count++;
                }
            }
            avgR /= count; avgG /= count; avgB /= count;
            for (int dy = 0; dy < block && y + dy < h; ++dy) {
                for (int dx = 0; dx < block && x + dx < w; ++dx) {
                    int idx = ((y + dy) * w + (x + dx)) * 4;
                    pixels[idx + 0] = avgB;
                    pixels[idx + 1] = avgG;
                    pixels[idx + 2] = avgR;
                    pixels[idx + 3] = 255;
                }
            }
        }
    }
}

void effectMirrorPixels(BYTE* pixels, int w, int h) {
    for (int y = 0; y < h; ++y) {
        for (int x = 0; x < w / 2; ++x) {
            int left = (y * w + x) * 4;
            int right = (y * w + (w - 1 - x)) * 4;
            std::swap(pixels[left + 0], pixels[right + 0]);
            std::swap(pixels[left + 1], pixels[right + 1]);
            std::swap(pixels[left + 2], pixels[right + 2]);
        }
    }
}

void effectGlitchShiftPixels(BYTE* pixels, int w, int h) {
    for (int y = 0; y < h; y += rand() % 32 + 16) {
        int shift = (rand() % 80 - 40);
        int lineH = rand() % 32 + 8;
        for (int dy = 0; dy < lineH && y + dy < h; ++dy) {
            if (shift > 0)
                memmove(&pixels[((y + dy) * w + shift) * 4],
                        &pixels[((y + dy) * w) * 4], (w - shift) * 4);
            else if (shift < 0)
                memmove(&pixels[((y + dy) * w) * 4],
                        &pixels[((y + dy) * w - shift) * 4], (w + shift) * 4);
        }
    }
}

void effectTunnelPixels(BYTE* pixels, int w, int h) {
    static float t = 0.0f;
    t += 0.02f;
    int cx = w / 2, cy = h / 2;
    std::vector<BYTE> src(pixels, pixels + w * h * 4);
    for (int y = 0; y < h; y += 4) {
        for (int x = 0; x < w; x += 4) {
            float dx = x - cx, dy = y - cy;
            float dist = sqrtf(dx*dx + dy*dy);
            float angle = atan2f(dy, dx) + t * (1.0f + dist * 0.01f);
            int srcX = std::clamp(static_cast<int>(cx + dist * cosf(angle)), 0, w - 1);
            int srcY = std::clamp(static_cast<int>(cy + dist * sinf(angle)), 0, h - 1);
            for (int dy2 = 0; dy2 < 4 && y + dy2 < h; ++dy2) {
                for (int dx2 = 0; dx2 < 4 && x + dx2 < w; ++dx2) {
                    memcpy(&pixels[((y+dy2)*w + (x+dx2))*4],
                           &src[(srcY+dy2)*w + (srcX+dx2)*4], 4);
                }
            }
        }
    }
}

void effectFlashPixels(BYTE* pixels, int w, int h) {
    memset(pixels, 255, w * h * 4);
}

void effectColorCyclePixels(BYTE* pixels, int w, int h) {
    static int shift = 0;
    shift = (shift + 1) % 3;
    for (int i = 0; i < w * h * 4; i += 4) {
        BYTE r = pixels[i+2], g = pixels[i+1], b = pixels[i+0];
        if (shift == 0) { pixels[i+2]=g; pixels[i+1]=b; pixels[i+0]=r; }
        else if (shift == 1) { pixels[i+2]=b; pixels[i+1]=r; pixels[i+0]=g; }
        else { pixels[i+2]=~pixels[i+2]; pixels[i+1]=~pixels[i+1]; pixels[i+0]=~pixels[i+0]; }
        pixels[i+3] = 255;
    }
}

void effectFractalLinesOverlay(HDC hdc, int w, int h) {
    static DWORD seed = 0;
    seed += 0x1337;
    srand(seed);
    for (int i = 0; i < 800; ++i) {
        HPEN hPen = CreatePen(PS_SOLID, 1, RGB(rand() % 256, rand() % 256, rand() % 256));
        SelectObject(hdc, hPen);
        MoveToEx(hdc, rand() % w, rand() % h, nullptr);
        LineTo(hdc, rand() % w, rand() % h);
        DeleteObject(hPen);
    }
}

void effectWindowDuplicationOverlay(HDC hdc, int w, int h) {
    HDC hdcMem = CreateCompatibleDC(hdc);
    HBITMAP hbmCopy = CreateCompatibleBitmap(hdc, w, h);
    SelectObject(hdcMem, hbmCopy);
    BitBlt(hdcMem, 0, 0, w, h, g_hdcOverlay, 0, 0, SRCCOPY);

    srand(GetTickCount());
    for (int i = 0; i < 60; ++i) {
        int rw = w / 5 + rand() % (w / 3);
        int rh = h / 5 + rand() % (h / 3);
        int rx = rand() % (w - rw);
        int ry = rand() % (h - rh);
        StretchBlt(hdc, rx, ry, rw, rh, hdcMem, 0, 0, w, h, SRCCOPY);
    }
    HCURSOR hCur = GetCursor();
    ICONINFO ii;
    if (GetIconInfo(hCur, &ii)) {
        for (int i = 0; i < 100; ++i)
            DrawIcon(hdc, rand() % w, rand() % h, hCur);
        DeleteObject(ii.hbmColor);
        DeleteObject(ii.hbmMask);
    }
    DeleteObject(hbmCopy);
    DeleteDC(hdcMem);
}

void effectBlockShuffleOverlay(HDC hdc, int w, int h) {
    HDC hdcMem = CreateCompatibleDC(hdc);
    HBITMAP hbmCopy = CreateCompatibleBitmap(hdc, w, h);
    SelectObject(hdcMem, hbmCopy);
    BitBlt(hdcMem, 0, 0, w, h, g_hdcOverlay, 0, 0, SRCCOPY);

    constexpr int BLOCK_W = 80, BLOCK_H = 60;
    int cols = (w + BLOCK_W - 1) / BLOCK_W;
    int rows = (h + BLOCK_H - 1) / BLOCK_H;
    std::vector<std::pair<int,int>> blocks;
    for (int r = 0; r < rows; ++r)
        for (int c = 0; c < cols; ++c)
            blocks.emplace_back(c, r);
    std::random_device rd;
    std::mt19937 gen(rd());
    std::shuffle(blocks.begin(), blocks.end(), gen);
    for (size_t i = 0; i < blocks.size(); ++i) {
        int srcCol = i % cols, srcRow = i / cols;
        int dstCol = blocks[i].first, dstRow = blocks[i].second;
        BitBlt(hdc, dstCol * BLOCK_W, dstRow * BLOCK_H, BLOCK_W, BLOCK_H,
               hdcMem, srcCol * BLOCK_W, srcRow * BLOCK_H, SRCCOPY);
    }
    DeleteObject(hbmCopy);
    DeleteDC(hdcMem);
}

void effectLensOverlay(HDC hdc, int w, int h) {
    captureScreenToOverlay();
    POINT pt;
    GetCursorPos(&pt);
    int cx = pt.x - g_virtX, cy = pt.y - g_virtY;
    int radius = 120;
    for (int y = std::max(0, cy - radius); y < std::min(h, cy + radius); ++y) {
        for (int x = std::max(0, cx - radius); x < std::min(w, cx + radius); ++x) {
            float dx = x - cx, dy = y - cy;
            if (dx*dx + dy*dy < radius*radius) {
                float factor = 1.5f;
                int srcX = std::clamp(static_cast<int>(cx + dx / factor), 0, w - 1);
                int srcY = std::clamp(static_cast<int>(cy + dy / factor), 0, h - 1);
                COLORREF c = GetPixel(g_hdcOverlay, srcX, srcY);
                SetPixel(hdc, x, y, c);
            }
        }
    }
}

Effect g_effects[] = {
    Effect(effectInvertPixels),
    Effect(effectMeltingPixels),
    Effect(effectWavePixels),
    Effect(effectPixelSortPixels),
    Effect(effectFractalLinesOverlay),
    Effect(effectWindowDuplicationOverlay),
    Effect(effectFlashPixels),
    Effect(effectSwirlPixels),
    Effect(effectBlockShuffleOverlay),
    Effect(effectColorCyclePixels),
    Effect(effectPixelatePixels),
    Effect(effectMirrorPixels),
    Effect(effectGlitchShiftPixels),
    Effect(effectLensOverlay),
    Effect(effectTunnelPixels)
};
constexpr int EFFECT_COUNT = sizeof(g_effects) / sizeof(g_effects[0]);

std::atomic<bool> g_cursorChaos = true;
std::thread g_cursorThread;

void cursorMadnessThread() {
    while (g_cursorChaos) {
        int x = g_virtX + (rand() % g_virtW);
        int y = g_virtY + (rand() % g_virtH);
        SetCursorPos(x, y);
        Sleep(rand() % 20 + 5);
    }
}

LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    static int effectIdx = 0;
    static int frameCounter = 0;
    switch (msg) {
    case WM_CREATE: {
        srand(static_cast<unsigned>(time(nullptr)));
        if (!InitOverlay()) {
            MessageBox(nullptr, L"Overlay creation failed", L"Error", MB_ICONERROR);
            return -1;
        }
        RegisterHotKey(hwnd, 100, EXIT_HOTKEY_MOD, EXIT_HOTKEY_KEY);
        SetWindowsHookEx(WH_KEYBOARD_LL,
            [](int nCode, WPARAM wParam, LPARAM lParam) -> LRESULT {
                if (nCode == HC_ACTION && wParam == WM_KEYDOWN) {
                    KBDLLHOOKSTRUCT* p = (KBDLLHOOKSTRUCT*)lParam;
                    if (p->vkCode == VK_ESCAPE) {
                        PostMessage(g_hWnd, WM_CLOSE, 0, 0);
                        return 1;
                    }
                }
                return CallNextHookEx(nullptr, nCode, wParam, lParam);
            }, GetModuleHandle(nullptr), 0);

        if (!initAudio(hwnd)) {
            MessageBox(nullptr, L"Audio init failed", L"Error", MB_ICONERROR);
            return -1;
        }
        g_cursorThread = std::thread(cursorMadnessThread);
        SetTimer(hwnd, TIMER_ID, TIMER_MS, nullptr);
        return 0;
    }
    case WM_TIMER: {
        Effect& eff = g_effects[effectIdx];
        if (eff.type == PIXEL) {
            captureScreenToOverlay();
            eff.pixelFunc(static_cast<BYTE*>(g_pvBits), g_virtW, g_virtH);
        } else {
            eff.overlayFunc(g_hdcOverlay, g_virtW, g_virtH);
        }
        UpdateOverlay();

        frameCounter++;
        if (frameCounter > 25) {
            frameCounter = 0;
            effectIdx = (effectIdx + 1) % EFFECT_COUNT;
        }
        return 0;
    }
    case WM_HOTKEY:
        if (wParam == 100) PostMessage(hwnd, WM_CLOSE, 0, 0);
        return 0;
    case WM_DESTROY:
        KillTimer(hwnd, TIMER_ID);
        UnregisterHotKey(hwnd, 100);
        g_cursorChaos = false;
        if (g_cursorThread.joinable()) g_cursorThread.join();
        stopAudio();
        if (g_hdcOverlay) DeleteDC(g_hdcOverlay);
        if (g_hbmOverlay) DeleteObject(g_hbmOverlay);
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProc(hwnd, msg, wParam, lParam);
}

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE /*hPrevInstance*/, LPSTR /*lpCmdLine*/, int nCmdShow)
{
    const wchar_t CLASS_NAME[] = L"OverlayChaosClass";
    WNDCLASS wc{};
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hInstance;
    wc.lpszClassName = CLASS_NAME;
    RegisterClass(&wc);

    HWND hwnd = CreateWindowEx(0, CLASS_NAME, L"", WS_POPUP,
                               0, 0, 0, 0, nullptr, nullptr, hInstance, nullptr);
    if (!hwnd) return 1;

    MSG msg{};
    while (GetMessage(&msg, nullptr, 0, 0))
    {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }
    return static_cast<int>(msg.wParam);
}