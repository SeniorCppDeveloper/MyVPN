#define NOMINMAX
#define _WINSOCK_DEPRECATED_NO_WARNINGS
#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
#include <iphlpapi.h>
#include <icmpapi.h>
#include <netioapi.h>
#include <bcrypt.h>
#include <windowsx.h>
#include <objidl.h>
#include <algorithm>
using std::min;
using std::max;
#include <gdiplus.h>
#include <vector>
#include <cmath>
#include <cstdlib>
#include <ctime>
#include <cstdio>
#include <cctype>
#include <thread>
#include <atomic>
#include <string>
#include <shellapi.h>
#include <cwchar>
#include <cwctype>
#include <commdlg.h>
#include <winhttp.h>
#include <wincrypt.h>
#include <functional>

#ifdef _MSC_VER
#pragma comment(lib, "gdiplus.lib")
#pragma comment(lib, "user32.lib")
#pragma comment(lib, "gdi32.lib")
#pragma comment(lib, "iphlpapi.lib")
#pragma comment(lib, "ws2_32.lib")
#pragma comment(lib, "advapi32.lib")
#pragma comment(lib, "shell32.lib")
#pragma comment(lib, "comdlg32.lib")
#pragma comment(lib, "bcrypt.lib")
#pragma comment(lib, "winhttp.lib")
#pragma comment(lib, "crypt32.lib")
#pragma comment(linker, "/MANIFESTUAC:\"level='requireAdministrator' uiAccess='false'\"")
#endif

using namespace Gdiplus;

static const int W = 520;
static const int H = 620;
static const int TITLE_H = 44;

static const wchar_t* BUY_URL = L"https://funpay.com/users/17807093/";
static const long long PERIOD_SEC = 30LL * 86400LL;
static const ULONG64   GB = 1024ULL * 1024ULL * 1024ULL;

struct Plan { int months; int days; int price; const wchar_t* code; };
static const Plan PLANS[3] = { {1, 30, 199, L"1m"}, {3, 90, 499, L"3m"}, {12, 365, 1690, L"12m"} };
static const int BASE_PRICE = 199;

enum State { ST_OFF, ST_CONNECTING, ST_ON };
enum ScreenState { SCR_LOGIN, SCR_REGISTER, SCR_MAIN, SCR_SUB };

enum Hit {
    H_NONE = 0, H_CLOSE, H_MIN, H_CHIP, H_MENU_SUB, H_MENU_OUT, H_CONNECT, H_EDIT_AVATAR,
    H_LOGIN, H_SWITCH, H_FIELD_USER, H_FIELD_PASS, H_BUY, H_BACK, H_PLAN0, H_PLAN1, H_PLAN2, H_FIELD_KEY, H_ACTIVATE, H_COUNT
};

struct Particle { float x, y, r, speed, phase, alpha; };

struct Account {
    std::wstring user, salt, hash, avatar;
    long long    subExpire = 0;
    long long    periodStart = 0;
    ULONG64      traffic = 0;
};

static HWND  g_hWnd = nullptr;
static State g_state = ST_OFF;
static ScreenState g_screen = SCR_LOGIN;

static Account g_acc;
static Bitmap* g_avatarBitmap = nullptr;

static std::wstring g_inputUser, g_inputPass, g_loginError;
static bool  g_inputFocusUser = true;
static int   g_failCount = 0;
static ULONGLONG g_lockUntil = 0;
static std::wstring g_inputKey, g_keyMsg;
static bool g_keyFocus = true, g_keyMsgOk = false;
static float g_keyMsgT = 0.f;
static int g_keyFails = 0;
static ULONGLONG g_keyLock = 0;

static Hit   g_hit = H_NONE, g_down = H_NONE;
static float g_hv[H_COUNT] = {};
static bool  g_tracking = false, g_closing = false, g_menuOpen = false;
static int   g_selPlan = 1;
static float g_planSel[3] = { 0.f, 1.f, 0.f };
static float g_focus[3] = { 1.f, 0.f, 0.f };

static float g_time = 0.f, g_press = 0.f, g_conn = 0.f, g_pingFade = 1.f, g_alpha = 0.f;
static float g_menuAnim = 0.f, g_scrTime = 0.f, g_shake = 0.f, g_keyShake = 0.f, g_periodBar = 0.f;
static float g_priceShown = 0.f, g_trafficShown = 0.f, g_rip = 0.f;
static BYTE  g_lastA = 0;

static ULONGLONG g_connectedAt = 0, g_lastTick = 0, g_lastSample = 0, g_lastSave = 0;
static int  g_ping = -1, g_failStreak = 0;
static std::vector<int> g_pings;
static std::vector<Particle> g_particles;

static ULONG64 g_lastRx = 0, g_lastTx = 0;
static bool    g_haveBase = false;
static float   g_downSpeed = 0.f, g_upSpeed = 0.f;

static ULONG_PTR   g_gdipToken = 0;
static FontFamily* g_ff = nullptr;
static Font *g_fBrand, *g_fBtn, *g_fStatus, *g_fTimer, *g_fSmall, *g_fBar, *g_fPing, *g_fInput;

typedef BOOL(__cdecl* PFN_WireGuardTunnelService)(LPCWSTR confFile);
static HMODULE g_tunnelLib = nullptr;
static PFN_WireGuardTunnelService g_tunnelProc = nullptr;

static const wchar_t* TUNNEL_NAME = L"null_vpn";
static const wchar_t* TUNNEL_SERVICE = L"WireGuardTunnel$null_vpn";

static std::atomic<bool> g_busy(false);
static std::atomic<bool> g_pingRun(false);
static std::atomic<int>  g_workers(0);
static std::thread g_pingThread;
static std::wstring g_confPath;
static std::string  g_pingIp = "1.1.1.1";
static const wchar_t* g_errMsg = nullptr;
static ULONGLONG g_errUntil = 0;

static HICON g_icoBig = nullptr, g_icoSmall = nullptr;

#define WM_APP_CONNECTED (WM_APP + 1)
#define WM_APP_PING      (WM_APP + 2)
#define WM_APP_UI        (WM_APP + 3)

static std::wstring ExeDir() {
    wchar_t b[MAX_PATH];
    GetModuleFileNameW(nullptr, b, MAX_PATH);
    std::wstring s = b;
    return s.substr(0, s.find_last_of(L"\\/"));
}

static long long NowUnix() { return (long long)time(nullptr); }

static bool IsPremium() { return g_acc.subExpire > NowUnix(); }

static int DaysLeftSub() {
    long long d = g_acc.subExpire - NowUnix();
    return d <= 0 ? 0 : (int)((d + 86399) / 86400);
}

static std::wstring FmtDate(long long t) {
    __time64_t tt = t;
    tm lt = {};
    _localtime64_s(&lt, &tt);
    wchar_t b[32];
    swprintf_s(b, L"%02d.%02d.%04d", lt.tm_mday, lt.tm_mon + 1, lt.tm_year + 1900);
    return b;
}

static void RollPeriod(Account& a) {
    long long now = NowUnix();
    if (a.periodStart == 0) { a.periodStart = now; return; }
    if (now - a.periodStart >= PERIOD_SEC) {
        a.traffic = 0;
        a.periodStart += ((now - a.periodStart) / PERIOD_SEC) * PERIOD_SEC;
    }
}

static std::vector<std::wstring> Split(const std::wstring& s, wchar_t sep) {
    std::vector<std::wstring> out;
    size_t st = 0;
    for (;;) {
        size_t p = s.find(sep, st);
        if (p == std::wstring::npos) { out.push_back(s.substr(st)); break; }
        out.push_back(s.substr(st, p - st));
        st = p + 1;
    }
    return out;
}

static std::wstring ToHex(const BYTE* d, size_t n) {
    static const wchar_t* HX = L"0123456789abcdef";
    std::wstring s;
    for (size_t i = 0; i < n; i++) { s += HX[d[i] >> 4]; s += HX[d[i] & 15]; }
    return s;
}


static std::wstring DataPath(const wchar_t* name) { return ExeDir() + L"\\" + name; }

// ===================== сервер: API, токен, устройство =====================
static const wchar_t* API_HOST = L"82.148.250.26";
static const char* API_PIN = "81febbdfd68746198029a1633b5d6b45692f5658da439f5d88a4ded34e87db3f";

static std::string g_token;
static std::wstring g_errText;
static bool g_authBusy = false, g_keyBusy = false, g_refreshing = false;

static void ForceLogout(const std::wstring& msg);
static void KeyResult(bool ok, const std::wstring& msg);

static void SaveCurrent() {}   // данные аккаунта теперь на сервере

static std::string Utf8(const std::wstring& w) {
    if (w.empty()) return "";
    int n = WideCharToMultiByte(CP_UTF8, 0, w.c_str(), (int)w.size(), nullptr, 0, nullptr, nullptr);
    std::string s(n, 0);
    WideCharToMultiByte(CP_UTF8, 0, w.c_str(), (int)w.size(), &s[0], n, nullptr, nullptr);
    return s;
}
static std::wstring FromUtf8(const std::string& s) {
    if (s.empty()) return L"";
    int n = MultiByteToWideChar(CP_UTF8, 0, s.c_str(), (int)s.size(), nullptr, 0);
    std::wstring w(n, 0);
    MultiByteToWideChar(CP_UTF8, 0, s.c_str(), (int)s.size(), &w[0], n);
    return w;
}
static std::string JEsc(const std::string& s) {
    std::string o;
    for (unsigned char c : s) {
        if (c == '"' || c == '\\') { o += '\\'; o += (char)c; }
        else if (c < 32) { char b[8]; sprintf_s(b, "\\u%04x", c); o += b; }
        else o += (char)c;
    }
    return o;
}
static bool JStr(const std::string& j, const char* key, std::string& out) {
    std::string k = std::string("\"") + key + "\"";
    size_t p = j.find(k);
    if (p == std::string::npos) return false;
    p = j.find(':', p + k.size());
    if (p == std::string::npos) return false;
    p = j.find_first_not_of(" \t\r\n", p + 1);
    if (p == std::string::npos || j[p] != '"') return false;
    out.clear();
    for (++p; p < j.size() && j[p] != '"'; ++p) {
        if (j[p] == '\\' && p + 1 < j.size()) {
            ++p;
            out += (j[p] == 'n' ? '\n' : j[p] == 'r' ? '\r' : j[p] == 't' ? '\t' : j[p]);
        } else out += j[p];
    }
    return true;
}
static long long JNum(const std::string& j, const char* key, long long def = 0) {
    std::string k = std::string("\"") + key + "\"";
    size_t p = j.find(k);
    if (p == std::string::npos) return def;
    p = j.find(':', p + k.size());
    if (p == std::string::npos) return def;
    p = j.find_first_not_of(" \t\r\n", p + 1);
    if (p == std::string::npos || !(isdigit((unsigned char)j[p]) || j[p] == '-')) return def;
    return _strtoi64(j.c_str() + p, nullptr, 10);
}

static bool PinOk(HINTERNET req) {
    PCCERT_CONTEXT c = nullptr;
    DWORD sz = sizeof(c);
    if (!WinHttpQueryOption(req, WINHTTP_OPTION_SERVER_CERT_CONTEXT, &c, &sz) || !c) return false;
    BYTE h[32];
    bool ok = BCRYPT_SUCCESS(BCryptHash(BCRYPT_SHA256_ALG_HANDLE, nullptr, 0,
        c->pbCertEncoded, c->cbCertEncoded, h, 32));
    CertFreeCertificateContext(c);
    if (!ok) return false;
    char b[65];
    for (int i = 0; i < 32; i++) sprintf_s(b + i * 2, 3, "%02x", h[i]);
    return strcmp(b, API_PIN) == 0;
}

struct ApiRes { bool ok = false; int code = 0; std::string body; std::wstring err = L"Нет связи с сервером"; };

static ApiRes ApiCall(const char* method, const wchar_t* path, const std::string& token, const std::string& body) {
    ApiRes r;
    HINTERNET s = WinHttpOpen(L"NullVPN/1.0", WINHTTP_ACCESS_TYPE_DEFAULT_PROXY, nullptr, nullptr, 0);
    if (!s) return r;
    WinHttpSetTimeouts(s, 8000, 8000, 15000, 15000);
    HINTERNET c = WinHttpConnect(s, API_HOST, INTERNET_DEFAULT_HTTPS_PORT, 0);
    DWORD fl = SECURITY_FLAG_IGNORE_UNKNOWN_CA | SECURITY_FLAG_IGNORE_CERT_CN_INVALID |
               SECURITY_FLAG_IGNORE_CERT_DATE_INVALID | SECURITY_FLAG_IGNORE_CERT_WRONG_USAGE;
    auto open = [&](const wchar_t* m, const wchar_t* p) -> HINTERNET {
        HINTERNET q = WinHttpOpenRequest(c, m, p, nullptr, WINHTTP_NO_REFERER,
            WINHTTP_DEFAULT_ACCEPT_TYPES, WINHTTP_FLAG_SECURE);
        if (q) WinHttpSetOption(q, WINHTTP_OPTION_SECURITY_FLAGS, &fl, sizeof(fl));
        return q;
    };
    if (c) {
        // пробный запрос без секретов: проверяем пин сертификата
        HINTERNET pr = open(L"GET", L"/");
        bool pinned = pr && WinHttpSendRequest(pr, WINHTTP_NO_ADDITIONAL_HEADERS, 0, WINHTTP_NO_REQUEST_DATA, 0, 0, 0) && PinOk(pr);
        if (pr) WinHttpCloseHandle(pr);
        if (pinned) {
            HINTERNET q = open(std::wstring(method, method + strlen(method)).c_str(), path);
            if (q) {
                std::wstring hd = L"Content-Type: application/json\r\n";
                if (!token.empty()) hd += L"Authorization: Bearer " + FromUtf8(token) + L"\r\n";
                DWORD len = (DWORD)body.size();
                if (WinHttpSendRequest(q, hd.c_str(), (DWORD)-1, len ? (LPVOID)body.data() : WINHTTP_NO_REQUEST_DATA, len, len, 0)
                    && PinOk(q) && WinHttpReceiveResponse(q, nullptr)) {
                    DWORD code = 0, cs = sizeof(code);
                    WinHttpQueryHeaders(q, WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
                        WINHTTP_HEADER_NAME_BY_INDEX, &code, &cs, WINHTTP_NO_HEADER_INDEX);
                    DWORD n = 0;
                    char buf[4096];
                    while (WinHttpReadData(q, buf, sizeof(buf), &n) && n) r.body.append(buf, n);
                    r.code = (int)code;
                    r.ok = code >= 200 && code < 300;
                    if (r.ok) r.err.clear();
                    else {
                        std::string d;
                        if (JStr(r.body, "detail", d) && !d.empty()) r.err = FromUtf8(d);
                        else r.err = L"Ошибка сервера (" + std::to_wstring(code) + L")";
                    }
                }
                WinHttpCloseHandle(q);
            }
        }
        WinHttpCloseHandle(c);
    }
    WinHttpCloseHandle(s);
    return r;
}

// выполнить функцию в UI-потоке
static void UiPost(std::function<void()> f) {
    PostMessageW(g_hWnd, WM_APP_UI, 0, (LPARAM)new std::function<void()>(std::move(f)));
}

static std::wstring ReadLineFile(const std::wstring& path) {
    FILE* f = nullptr;
    _wfopen_s(&f, path.c_str(), L"rt, ccs=UTF-8");
    if (!f) return L"";
    wchar_t line[1024] = {};
    fgetws(line, 1024, f);
    fclose(f);
    std::wstring s = line;
    while (!s.empty() && (s.back() == L'\n' || s.back() == L'\r')) s.pop_back();
    return s;
}

// аватар хранится локально, путь к картинке привязан к логину
static std::wstring AvatarFile(const std::wstring& u) {
    std::string b = Utf8(u);
    for (auto& ch : b) ch = (char)tolower((unsigned char)ch);
    return DataPath((L"avatar_" + ToHex((const BYTE*)b.data(), b.size()) + L".txt").c_str());
}
static void SaveAvatarPath() {
    FILE* f = nullptr;
    _wfopen_s(&f, AvatarFile(g_acc.user).c_str(), L"wt, ccs=UTF-8");
    if (f) { fputws(g_acc.avatar.c_str(), f); fclose(f); }
}

// постоянный id устройства: сервер создаёт под него отдельный WireGuard-пир
static std::string DeviceId() {
    std::wstring p = DataPath(L"device.dat");
    std::wstring s = ReadLineFile(p);
    if (!s.empty()) return Utf8(s);
    BYTE r[6] = {};
    BCryptGenRandom(nullptr, r, 6, BCRYPT_USE_SYSTEM_PREFERRED_RNG);
    std::wstring id = L"win-" + ToHex(r, 6);
    FILE* f = nullptr;
    _wfopen_s(&f, p.c_str(), L"wt, ccs=UTF-8");
    if (f) { fputws(id.c_str(), f); fclose(f); }
    return Utf8(id);
}

static std::wstring ReadSession() {
    FILE* f = nullptr;
    _wfopen_s(&f, DataPath(L"session.dat").c_str(), L"rt, ccs=UTF-8");
    if (!f) return L"";
    wchar_t line[256] = {};
    fgetws(line, 256, f);
    fclose(f);
    std::wstring s = line;
    while (!s.empty() && (s.back() == L'\n' || s.back() == L'\r')) s.pop_back();
    return s;
}

static void WriteSession(const std::wstring& user) {
    std::wstring path = DataPath(L"session.dat");
    if (user.empty()) { DeleteFileW(path.c_str()); return; }
    FILE* f = nullptr;
    _wfopen_s(&f, path.c_str(), L"wt, ccs=UTF-8");
    if (f) { fputws(user.c_str(), f); fclose(f); }
}

static void LoadAvatar(const std::wstring& path) {
    delete g_avatarBitmap;
    g_avatarBitmap = nullptr;
    if (path.empty()) return;

    Bitmap src(path.c_str());
    if (src.GetLastStatus() != Ok || src.GetWidth() == 0 || src.GetHeight() == 0) return;

    const int S = 160;
    Bitmap* out = new Bitmap(S, S, PixelFormat32bppARGB);
    {
        Graphics gg(out);
        gg.SetInterpolationMode(InterpolationModeHighQualityBicubic);
        gg.SetSmoothingMode(SmoothingModeHighQuality);
        gg.SetPixelOffsetMode(PixelOffsetModeHalf);
        float w = (float)src.GetWidth(), h = (float)src.GetHeight();
        float s = std::min(w, h);
        gg.DrawImage(&src, RectF(0.f, 0.f, (REAL)S, (REAL)S), (w - s) / 2.f, (h - s) / 2.f, s, s, UnitPixel);
    }
    if (out->GetLastStatus() != Ok) { delete out; return; }
    g_avatarBitmap = out;
}

static void SelectAvatar() {
    OPENFILENAMEW ofn;
    ZeroMemory(&ofn, sizeof(ofn));
    wchar_t szFile[MAX_PATH] = { 0 };
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = g_hWnd;
    ofn.lpstrFile = szFile;
    ofn.nMaxFile = MAX_PATH;
    ofn.lpstrFilter = L"Изображения\0*.png;*.jpg;*.jpeg;*.bmp\0Все файлы\0*.*\0";
    ofn.nFilterIndex = 1;
    ofn.Flags = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST | OFN_NOCHANGEDIR;
    if (GetOpenFileNameW(&ofn) == TRUE) {
        g_acc.avatar = szFile;
        LoadAvatar(g_acc.avatar);
        SaveAvatarPath();
    }
}

static float Clamp01(float t) { return t < 0.f ? 0.f : (t > 1.f ? 1.f : t); }
static float Ease(float t) { t = Clamp01(t); float u = 1.f - t; return 1.f - u * u * u; }

static float PeriodFrac() {
    if (g_acc.periodStart == 0) return 0.f;
    return Clamp01((float)((double)(NowUnix() - g_acc.periodStart) / (double)PERIOD_SEC));
}

static Color LerpC(Color a, Color b, float t) {
    t = Clamp01(t);
    return Color((BYTE)(a.GetA() + (b.GetA() - a.GetA()) * t),
        (BYTE)(a.GetR() + (b.GetR() - a.GetR()) * t),
        (BYTE)(a.GetG() + (b.GetG() - a.GetG()) * t),
        (BYTE)(a.GetB() + (b.GetB() - a.GetB()) * t));
}

static Color WithA(Color c, float a) {
    return Color((BYTE)(Clamp01(a) * 255.f), c.GetR(), c.GetG(), c.GetB());
}

static void Approach(float& v, float target, float rate, float dt) {
    v += (target - v) * std::min(1.f, dt * rate);
}

static void RoundRectPath(GraphicsPath& p, float x, float y, float w, float h, float r) {
    float d = r * 2.f;
    if (d > h) d = h;
    if (d > w) d = w;
    p.AddArc(x, y, d, d, 180, 90);
    p.AddArc(x + w - d, y, d, d, 270, 90);
    p.AddArc(x + w - d, y + h - d, d, d, 0, 90);
    p.AddArc(x, y + h - d, d, d, 90, 90);
    p.CloseFigure();
}

static void Txt(Graphics& g, const WCHAR* s, Font* f, RectF r, Color c,
    StringAlignment h = StringAlignmentCenter,
    StringAlignment v = StringAlignmentCenter) {
    StringFormat sf;
    sf.SetAlignment(h);
    sf.SetLineAlignment(v);
    sf.SetFormatFlags(StringFormatFlagsNoWrap);
    SolidBrush b(c);
    g.DrawString(s, -1, f, r, &sf, &b);
}

static void TxtEll(Graphics& g, const WCHAR* s, Font* f, RectF r, Color c,
    StringAlignment h = StringAlignmentNear) {
    StringFormat sf;
    sf.SetAlignment(h);
    sf.SetLineAlignment(StringAlignmentCenter);
    sf.SetTrimming(StringTrimmingEllipsisCharacter);
    sf.SetFormatFlags(StringFormatFlagsNoWrap);
    SolidBrush b(c);
    g.DrawString(s, -1, f, r, &sf, &b);
}

static float TextW(Graphics& g, const std::wstring& s, Font* f) {
    if (s.empty()) return 0.f;
    RectF b;
    g.MeasureString(s.c_str(), -1, f, PointF(0.f, 0.f), StringFormat::GenericTypographic(), &b);
    return b.Width;
}

static void Card(Graphics& g, float x, float y, float w, float h, float r,
    Color c1, Color c2, Color border, float bw = 1.f) {
    GraphicsPath p;
    RoundRectPath(p, x, y, w, h, r);
    LinearGradientBrush b(RectF(x, y, w, h), c1, c2, LinearGradientModeVertical);
    g.FillPath(&b, &p);
    Pen pen(border, bw);
    g.DrawPath(&pen, &p);
}

static void Glow(Graphics& g, float x, float y, float w, float h, float r, Color c, float k, int layers = 3) {
    if (k <= 0.01f) return;
    for (int i = layers; i >= 1; i--) {
        GraphicsPath gp;
        RoundRectPath(gp, x, y, w, h, r);
        Pen pn(WithA(c, k * 0.12f / (float)i * 2.f), 2.f + i * 3.f);
        g.DrawPath(&pn, &gp);
    }
}

struct Reveal {
    Graphics& g;
    GraphicsState st;
    float p;
    RectF r;
    Reveal(Graphics& gg, int idx, RectF rr) : g(gg), r(rr) {
        p = Ease((g_scrTime - idx * 0.07f) * 3.2f);
        st = g.Save();
        g.TranslateTransform(0.f, (1.f - p) * 10.f);
    }
    ~Reveal() {
        g.Restore(st);
        if (p < 0.999f) {
            SolidBrush v(Color((BYTE)((1.f - p) * 255.f), 9, 6, 20));
            g.FillRectangle(&v, r.X - 4.f, r.Y, r.Width + 8.f, r.Height + 10.f);
        }
    }
};

static void IconCrown(Graphics& g, float cx, float cy, Color c) {
    Pen p(c, 1.7f);
    p.SetLineJoin(LineJoinRound);
    PointF pts[7] = { PointF(cx - 8.f, cy + 5.f), PointF(cx - 8.f, cy - 4.f), PointF(cx - 3.f, cy + 1.f),
                      PointF(cx, cy - 6.f), PointF(cx + 3.f, cy + 1.f), PointF(cx + 8.f, cy - 4.f), PointF(cx + 8.f, cy + 5.f) };
    g.DrawPolygon(&p, pts, 7);
}

static void IconLogout(Graphics& g, float cx, float cy, Color c) {
    Pen p(c, 1.8f);
    p.SetStartCap(LineCapRound);
    p.SetEndCap(LineCapRound);
    p.SetLineJoin(LineJoinRound);
    PointF door[4] = { PointF(cx - 1.f, cy - 7.f), PointF(cx - 7.f, cy - 7.f), PointF(cx - 7.f, cy + 7.f), PointF(cx - 1.f, cy + 7.f) };
    g.DrawLines(&p, door, 4);
    g.DrawLine(&p, cx - 3.f, cy, cx + 8.f, cy);
    PointF head[3] = { PointF(cx + 4.f, cy - 4.f), PointF(cx + 8.f, cy), PointF(cx + 4.f, cy + 4.f) };
    g.DrawLines(&p, head, 3);
}

static void IconUser(Graphics& g, float cx, float cy, Color c) {
    Pen p(c, 1.7f);
    g.DrawEllipse(&p, cx - 3.5f, cy - 8.f, 7.f, 7.f);
    g.DrawArc(&p, cx - 7.f, cy + 1.f, 14.f, 12.f, 180.f, 180.f);
}

static void IconLock(Graphics& g, float cx, float cy, Color c) {
    Pen p(c, 1.7f);
    g.DrawArc(&p, cx - 4.f, cy - 8.f, 8.f, 12.f, 180.f, 180.f);
    GraphicsPath lp;
    RoundRectPath(lp, cx - 6.f, cy - 1.f, 12.f, 9.f, 2.f);
    g.DrawPath(&p, &lp);
}

static void IconCheck(Graphics& g, float cx, float cy, Color c, float w = 1.8f) {
    Pen p(c, w);
    p.SetStartCap(LineCapRound);
    p.SetEndCap(LineCapRound);
    p.SetLineJoin(LineJoinRound);
    PointF pts[3] = { PointF(cx - 4.f, cy), PointF(cx - 1.f, cy + 3.f), PointF(cx + 4.f, cy - 3.f) };
    g.DrawLines(&p, pts, 3);
}

static void IconArrow(Graphics& g, float cx, float cy, bool up, Color c) {
    Pen p(c, 1.6f);
    p.SetStartCap(LineCapRound);
    p.SetEndCap(LineCapRound);
    p.SetLineJoin(LineJoinRound);
    float s = up ? -1.f : 1.f;
    g.DrawLine(&p, cx, cy - 4.f * s, cx, cy + 4.f * s);
    PointF head[3] = { PointF(cx - 3.f, cy + 1.f * s), PointF(cx, cy + 4.f * s), PointF(cx + 3.f, cy + 1.f * s) };
    g.DrawLines(&p, head, 3);
}

static void DrawAvatar(Graphics& g, float cx, float cy, float r) {
    if (!g_avatarBitmap) return;
    GraphicsPath disc;
    disc.AddEllipse(cx - r, cy - r, r * 2.f, r * 2.f);
    g.SetClip(&disc);
    g.SetInterpolationMode(InterpolationModeHighQualityBilinear);
    g.DrawImage(g_avatarBitmap, RectF(cx - r, cy - r, r * 2.f, r * 2.f));
    g.ResetClip();
}

static void DrawLogo(Graphics& g, float x, float y, float sz) {
    GraphicsPath lp;
    RoundRectPath(lp, x, y, sz, sz, sz * 0.3f);
    LinearGradientBrush lb(RectF(x, y, sz, sz), Color(255, 190, 120, 255),
        Color(255, 100, 45, 205), LinearGradientModeForwardDiagonal);
    g.FillPath(&lb, &lp);
    Font f(g_ff, sz * 0.62f, FontStyleBold, UnitPixel);
    Txt(g, L"N", &f, RectF(x, y, sz, sz), Color(255, 255, 255, 255));
}

static HICON MakeIcon(int size) {
    Bitmap bmp(size, size, PixelFormat32bppARGB);
    {
        Graphics g(&bmp);
        g.SetSmoothingMode(SmoothingModeAntiAlias);
        g.SetTextRenderingHint(TextRenderingHintAntiAlias);
        g.Clear(Color(0, 0, 0, 0));
        DrawLogo(g, 1.f, 1.f, (float)size - 2.f);
    }
    HICON h = nullptr;
    bmp.GetHICON(&h);
    return h;
}

static RECT BtnRect() { int w = 300, h = 76; int x = (W - w) / 2; return { x, 262, x + w, 262 + h }; }
static RECT CloseRect() { return { W - 50, 8, W - 14, 36 }; }
static RECT MinRect() { return { W - 92, 8, W - 56, 36 }; }
static RECT EditAvatarRect() { return { W / 2 + 22, 100 + 22, W / 2 + 52, 100 + 52 }; }
static RECT LoginBtnRect() { return { 110, 380, 410, 430 }; }
static RECT RegBtnRect() { return { 110, 460, 410, 490 }; }
static RECT FieldUserRect() { return { 110, 230, 410, 280 }; }
static RECT FieldPassRect() { return { 110, 300, 410, 350 }; }
static RECT ChipRect() { return { W - 266, 8, W - 98, 36 }; }
static RECT MenuPanelRect() { return { W - 14 - 190, TITLE_H + 2, W - 14, TITLE_H + 2 + 6 + 2 * 44 + 6 }; }
static RECT MenuItemRect(int i) {
    RECT p = MenuPanelRect();
    int top = p.top + 6 + i * 44;
    return { p.left + 6, top, p.right - 6, top + 44 };
}
static RECT BackRect() { return { 20, 54, 56, 90 }; }
static RECT PlanRect(int i) { int x = 40 + i * 151; return { x, 300, x + 138, 408 }; }
static RECT BuyRect() { return { 110, 428, 410, 474 }; }
static RECT KeyFieldRect() { return { 40, 512, 352, 560 }; }
static RECT ActivateRect() { return { 360, 512, 480, 560 }; }

static bool In(const RECT& r, POINT p) { return PtInRect(&r, p) != 0; }

static Hit HitTest(POINT p) {
    if (In(CloseRect(), p)) return H_CLOSE;
    if (In(MinRect(), p)) return H_MIN;
    switch (g_screen) {
    case SCR_LOGIN:
    case SCR_REGISTER:
        if (In(LoginBtnRect(), p)) return H_LOGIN;
        if (In(RegBtnRect(), p)) return H_SWITCH;
        if (In(FieldUserRect(), p)) return H_FIELD_USER;
        if (In(FieldPassRect(), p)) return H_FIELD_PASS;
        break;
    case SCR_MAIN:
        if (g_menuOpen) {
            if (In(MenuItemRect(0), p)) return H_MENU_SUB;
            if (In(MenuItemRect(1), p)) return H_MENU_OUT;
        }
        if (In(ChipRect(), p)) return H_CHIP;
        if (In(BtnRect(), p)) return H_CONNECT;
        if (In(EditAvatarRect(), p)) return H_EDIT_AVATAR;
        break;
    case SCR_SUB:
        if (In(BackRect(), p)) return H_BACK;
        if (In(BuyRect(), p)) return H_BUY;
        if (In(KeyFieldRect(), p)) return H_FIELD_KEY;
        if (In(ActivateRect(), p)) return H_ACTIVATE;
        for (int i = 0; i < 3; i++) if (In(PlanRect(i), p)) return (Hit)(H_PLAN0 + i);
        break;
    }
    return H_NONE;
}

static void GoTo(ScreenState s) {
    if (g_screen != s) {
        g_screen = s;
        g_scrTime = 0.f;
        g_menuOpen = false;
        if (s == SCR_SUB) { g_keyFocus = true; g_keyMsg.clear(); g_keyMsgT = 0.f; }
    }
}

static void AddPing(int p) {
    g_ping = p;
    g_pingFade = 0.f;
    g_pings.push_back(p);
    if (g_pings.size() > 40) g_pings.erase(g_pings.begin());
}

static void ShowError(int code) {
    switch (code) {
    case 1:  g_errMsg = L"Не найден tunnel.dll"; break;
    case 2:  g_errMsg = L"Нет файла null_vpn.conf"; break;
    case 3:  g_errMsg = L"Ошибка создания службы"; break;
    case 7:  g_errMsg = L"Подписка истекла"; break;
    case 8:  g_errMsg = g_errText.c_str(); break;
    default: g_errMsg = L"Сервер не отвечает"; break;
    }
    g_errUntil = GetTickCount64() + 7000;
}

static bool FileExists(const std::wstring& p) {
    DWORD a = GetFileAttributesW(p.c_str());
    return a != INVALID_FILE_ATTRIBUTES && !(a & FILE_ATTRIBUTE_DIRECTORY);
}

static bool LoadTunnelLib() {
    if (g_tunnelLib) return true;
    std::wstring dllPath = ExeDir() + L"\\tunnel.dll";
    g_tunnelLib = LoadLibraryW(dllPath.c_str());
    if (!g_tunnelLib) return false;
    g_tunnelProc = (PFN_WireGuardTunnelService)GetProcAddress(g_tunnelLib, "WireGuardTunnelService");
    return g_tunnelProc != nullptr;
}

static void RemoveTunnelService() {
    SC_HANDLE sc = OpenSCManagerW(nullptr, nullptr, SC_MANAGER_CONNECT);
    if (!sc) return;
    SC_HANDLE sv = OpenServiceW(sc, TUNNEL_SERVICE, DELETE | SERVICE_STOP | SERVICE_QUERY_STATUS);
    if (sv) {
        SERVICE_STATUS st;
        ControlService(sv, SERVICE_CONTROL_STOP, &st);
        DeleteService(sv);
        CloseServiceHandle(sv);
    }
    CloseServiceHandle(sc);
}

static bool InstallTunnelService(const std::wstring& exePath, const std::wstring& confPath) {
    SC_HANDLE sc = OpenSCManagerW(nullptr, nullptr, SC_MANAGER_CREATE_SERVICE);
    if (!sc) return false;
    std::wstring binPath = L"\"" + exePath + L"\" /service \"" + confPath + L"\"";
    SC_HANDLE sv = CreateServiceW(
        sc, TUNNEL_SERVICE, L"NULL VPN Tunnel",
        SERVICE_ALL_ACCESS, SERVICE_WIN32_OWN_PROCESS,
        SERVICE_DEMAND_START, SERVICE_ERROR_NORMAL,
        binPath.c_str(),
        nullptr, nullptr, L"Nsi\0TcpIp\0", nullptr, nullptr);
    bool ok = false;
    if (sv) {
        SERVICE_SID_INFO si = {};
        si.dwServiceSidType = SERVICE_SID_TYPE_UNRESTRICTED;
        ChangeServiceConfig2W(sv, SERVICE_CONFIG_SERVICE_SID_INFO, &si);
        CloseServiceHandle(sv);
        ok = true;
    }
    CloseServiceHandle(sc);
    return ok;
}

static bool StartTunnelService() {
    SC_HANDLE sc = OpenSCManagerW(nullptr, nullptr, SC_MANAGER_CONNECT);
    if (!sc) return false;
    SC_HANDLE sv = OpenServiceW(sc, TUNNEL_SERVICE, SERVICE_START | SERVICE_QUERY_STATUS);
    bool ok = false;
    if (sv) {
        if (StartServiceW(sv, 0, nullptr)) {
            SERVICE_STATUS st;
            for (int i = 0; i < 40; i++) {
                if (QueryServiceStatus(sv, &st) && st.dwCurrentState == SERVICE_RUNNING) { ok = true; break; }
                Sleep(250);
            }
        }
        CloseServiceHandle(sv);
    }
    CloseServiceHandle(sc);
    return ok;
}

static void StopTunnel() { RemoveTunnelService(); }

static void LoadPingTarget() {
    std::string dns, pt;
    FILE* f = nullptr;
    _wfopen_s(&f, g_confPath.c_str(), L"r");
    if (f) {
        char line[512];
        while (fgets(line, sizeof(line), f)) {
            std::string s = line;
            size_t st = s.find_first_not_of(" \t#");
            if (st == std::string::npos) continue;
            s = s.substr(st);
            size_t eq = s.find('=');
            if (eq == std::string::npos) continue;
            std::string key = s.substr(0, eq);
            while (!key.empty() && (key.back() == ' ' || key.back() == '\t')) key.pop_back();
            for (auto& c : key) c = (char)tolower((unsigned char)c);
            std::string val = s.substr(eq + 1);
            size_t a = val.find_first_not_of(" \t");
            if (a == std::string::npos) continue;
            size_t b = val.find_first_of(" ,\r\n\t", a);
            val = val.substr(a, b == std::string::npos ? std::string::npos : b - a);
            if (key == "pingtarget") pt = val;
            else if (key == "dns" && dns.empty()) dns = val;
        }
        fclose(f);
    }
    g_pingIp = !pt.empty() ? pt : (!dns.empty() ? dns : "1.1.1.1");
}

static int PingOnce(const std::string& ip, DWORD timeoutMs) {
    in_addr addr;
    if (inet_pton(AF_INET, ip.c_str(), &addr) != 1) inet_pton(AF_INET, "1.1.1.1", &addr);
    HANDLE h = IcmpCreateFile();
    if (h == INVALID_HANDLE_VALUE) return -1;
    char data[32] = {};
    char reply[sizeof(ICMP_ECHO_REPLY) + sizeof(data) + 64];
    DWORD n = IcmpSendEcho(h, addr.S_un.S_addr, data, sizeof(data), nullptr, reply, sizeof(reply), timeoutMs);
    int rtt = -1;
    if (n > 0) {
        ICMP_ECHO_REPLY* r = (ICMP_ECHO_REPLY*)reply;
        if (r->Status == IP_SUCCESS) rtt = (int)r->RoundTripTime;
        if (rtt == 0) rtt = 1;
    }
    IcmpCloseHandle(h);
    return rtt;
}

static void StartPing() {
    g_pingRun = true;
    g_pingThread = std::thread([] {
        while (g_pingRun) {
            int r = PingOnce(g_pingIp, 900);
            if (!g_pingRun) break;
            PostMessageW(g_hWnd, WM_APP_PING, (WPARAM)r, 0);
            for (int i = 0; i < 10 && g_pingRun; i++) Sleep(100);
        }
        });
}

static void StopPing() {
    g_pingRun = false;
    if (g_pingThread.joinable()) g_pingThread.join();
}

static bool ReadTunnelBytes(ULONG64& rx, ULONG64& tx) {
    PMIB_IF_TABLE2 t = nullptr;
    if (GetIfTable2(&t) != NO_ERROR || !t) return false;
    bool found = false;
    for (ULONG i = 0; i < t->NumEntries; i++) {
        const MIB_IF_ROW2& r = t->Table[i];
        if (_wcsicmp(r.Alias, TUNNEL_NAME) == 0) { rx = r.InOctets; tx = r.OutOctets; found = true; break; }
    }
    FreeMibTable(t);
    return found;
}

static void SampleTraffic(ULONGLONG now) {
    double secs = (double)(now - g_lastSample) / 1000.0;
    g_lastSample = now;
    ULONG64 rx = 0, tx = 0;
    if (!ReadTunnelBytes(rx, tx)) return;
    if (g_haveBase && rx >= g_lastRx && tx >= g_lastTx && secs > 0.0) {
        ULONG64 dr = rx - g_lastRx, dtx = tx - g_lastTx;
        g_downSpeed += ((float)(dr / secs) - g_downSpeed) * 0.35f;
        g_upSpeed += ((float)(dtx / secs) - g_upSpeed) * 0.35f;
    }
    g_lastRx = rx; g_lastTx = tx; g_haveBase = true;
}

static void ConnectWorker() {
    int err = 0;
    bool authLost = false;
    g_confPath = ExeDir() + L"\\null_vpn.conf";

    if (!LoadTunnelLib()) {
        err = 1;
    }
    else {
        std::string tok = g_token;
        ApiRes c = ApiCall("POST", L"/api/config", tok, "{\"device\":\"" + JEsc(DeviceId()) + "\"}");
        std::string cfg;
        if (!c.ok || !JStr(c.body, "config", cfg) || cfg.empty()) {
            authLost = (c.code == 401);
            g_errText = authLost ? L"Сессия истекла, войдите снова"
                      : (c.ok ? L"Сервер не выдал конфиг" : c.err);
            err = 8;
        }
        else {
            FILE* f = nullptr;
            _wfopen_s(&f, g_confPath.c_str(), L"wb");
            if (!f) err = 2;
            else { fwrite(cfg.data(), 1, cfg.size(), f); fclose(f); }
        }
    }

    if (err == 0) {
        LoadPingTarget();
        RemoveTunnelService();

        std::wstring exePath;
        {
            wchar_t b[MAX_PATH];
            GetModuleFileNameW(nullptr, b, MAX_PATH);
            exePath = b;
        }

        if (!InstallTunnelService(exePath, g_confPath)) {
            err = 3;
        }
        else if (!StartTunnelService()) {
            err = 3;
            RemoveTunnelService();
        }
        else {
            bool ok = false;
            for (int i = 0; i < 6 && !ok; i++) {
                ok = PingOnce(g_pingIp, 1000) >= 0;
                if (!ok) Sleep(300);
            }
            if (!ok) {
                err = 5;
                RemoveTunnelService();
            }
        }
    }
    PostMessageW(g_hWnd, WM_APP_CONNECTED, (WPARAM)err, 0);
    if (authLost) UiPost([] { ForceLogout(L"Сессия истекла, войдите снова"); });
    g_workers--;
}

static void ShutdownVpn() {
    for (int i = 0; i < 250 && g_workers > 0; i++) Sleep(100);
    StopPing();
    if (g_state != ST_OFF) StopTunnel();
}

static void ToggleConnection() {
    if (g_busy || g_closing || g_acc.user.empty()) return;

    if (g_state == ST_OFF) {
        if (!IsPremium()) { GoTo(SCR_SUB); return; }
        g_busy = true;
        g_errMsg = nullptr;
        g_workers++;
        g_state = ST_CONNECTING;
        std::thread(ConnectWorker).detach();
    }
    else {
        SampleTraffic(GetTickCount64());
        SaveCurrent();
        g_busy = true;
        g_errMsg = nullptr;
        g_workers++;
        g_state = ST_OFF;
        g_ping = -1;
        g_pings.clear();
        std::thread([] {
            StopPing();
            StopTunnel();
            g_busy = false;
            g_workers--;
            }).detach();
    }
}

static void SetError(const std::wstring& msg) { g_loginError = msg; g_shake = 1.f; }

static void ApplyMe(const std::string& j) {
    g_acc.subExpire = JNum(j, "sub_expire", g_acc.subExpire);
    g_acc.traffic = (ULONG64)JNum(j, "traffic", (long long)g_acc.traffic);
    g_acc.periodStart = JNum(j, "period_start", g_acc.periodStart);
}

static void EnterAccount(const std::string& me) {
    std::string login;
    JStr(me, "login", login);
    g_acc = Account();
    g_acc.user = FromUtf8(login);
    ApplyMe(me);
    g_acc.avatar = ReadLineFile(AvatarFile(g_acc.user));
    LoadAvatar(g_acc.avatar);
    g_inputUser.clear(); g_inputPass.clear(); g_loginError.clear();
    g_trafficShown = 0.f; g_periodBar = 0.f;
    g_priceShown = (float)PLANS[1].price; g_selPlan = 1;
    WriteSession(FromUtf8(g_token));
    GoTo(IsPremium() ? SCR_MAIN : SCR_SUB);
}

static void ForceLogout(const std::wstring& msg) {
    if (g_state == ST_ON) ToggleConnection();
    g_token.clear();
    WriteSession(L"");
    g_acc = Account();
    LoadAvatar(L"");
    g_inputFocusUser = true;
    GoTo(SCR_LOGIN);
    SetError(msg);
}

static void RefreshMe() {
    if (g_refreshing || g_token.empty() || g_acc.user.empty()) return;
    g_refreshing = true;
    std::string tok = g_token;
    std::thread([tok] {
        ApiRes r = ApiCall("GET", L"/api/me", tok, "");
        UiPost([r] {
            g_refreshing = false;
            if (r.ok) ApplyMe(r.body);
            else if (r.code == 401) ForceLogout(L"Сессия истекла, войдите снова");
        });
    }).detach();
}

static void Logout() {
    if (g_busy) return;
    if (g_state == ST_ON) ToggleConnection();
    std::string tok = g_token;
    g_token.clear();
    WriteSession(L"");
    g_acc = Account();
    LoadAvatar(L"");
    g_inputFocusUser = true;
    GoTo(SCR_LOGIN);
    std::thread([tok] { ApiCall("POST", L"/api/logout", tok, ""); }).detach();
}

static void SubmitAuth() {
    while (!g_inputUser.empty() && g_inputUser.back() == L' ') g_inputUser.pop_back();
    while (!g_inputUser.empty() && g_inputUser.front() == L' ') g_inputUser.erase(0, 1);
    if (g_inputUser.empty() || g_inputPass.empty() || g_authBusy) return;
    bool reg = (g_screen == SCR_REGISTER);
    if (reg && g_inputUser.length() < 3) { SetError(L"Логин: минимум 3 символа"); return; }
    if (reg && g_inputPass.length() < 6) { SetError(L"Пароль: минимум 6 символов"); return; }
    g_authBusy = true;
    std::string body = "{\"login\":\"" + JEsc(Utf8(g_inputUser)) + "\",\"password\":\"" + JEsc(Utf8(g_inputPass)) + "\"}";
    std::thread([reg, body] {
        ApiRes a = ApiCall("POST", reg ? L"/api/register" : L"/api/login", "", body);
        std::string tok;
        ApiRes m;
        std::wstring err = a.err;
        if (a.ok && JStr(a.body, "token", tok) && !tok.empty()) {
            m = ApiCall("GET", L"/api/me", tok, "");
            if (!m.ok) err = m.err;
        } else if (a.ok) err = L"Ошибка";
        bool good = a.ok && !tok.empty() && m.ok;
        UiPost([good, tok, m, err] {
            g_authBusy = false;
            if (good) { g_token = tok; EnterAccount(m.body); }
            else SetError(err);
        });
    }).detach();
}

static void RedeemKey() {
    if (g_inputKey.empty() || g_keyBusy) return;
    if (g_inputKey.length() != 20) { KeyResult(false, L"Неверный формат ключа"); return; }
    g_keyBusy = true;
    std::string tok = g_token, k = Utf8(g_inputKey);
    std::thread([tok, k] {
        ApiRes r = ApiCall("POST", L"/api/redeem", tok, "{\"key\":\"" + k + "\"}");
        UiPost([r] {
            g_keyBusy = false;
            if (r.ok) {
                g_acc.subExpire = JNum(r.body, "sub_expire", g_acc.subExpire);
                g_inputKey.clear();
                wchar_t b[96];
                swprintf_s(b, L"Ключ активирован: +%d дн.", (int)JNum(r.body, "days"));
                KeyResult(true, b);
                RefreshMe();
            } else if (r.code == 401) ForceLogout(L"Сессия истекла, войдите снова");
            else KeyResult(false, r.err);
        });
    }).detach();
}

static void RestoreSession() {
    std::string tok = Utf8(ReadSession());
    if (tok.empty()) return;
    g_token = tok;
    std::thread([tok] {
        ApiRes r = ApiCall("GET", L"/api/me", tok, "");
        UiPost([r] {
            if (r.ok) EnterAccount(r.body);
            else if (r.code == 401) { g_token.clear(); WriteSession(L""); }
            else SetError(r.err);
        });
    }).detach();
}

static void PasteInto(std::wstring& target, size_t maxLen) {
    if (!OpenClipboard(g_hWnd)) return;
    HANDLE h = GetClipboardData(CF_UNICODETEXT);
    if (h) {
        const wchar_t* p = (const wchar_t*)GlobalLock(h);
        if (p) {
            for (; *p && target.length() < maxLen; p++) {
                if (*p == L'\r' || *p == L'\n') break;
                if (*p >= 32 && *p != 127) target += *p;
            }
            GlobalUnlock(h);
        }
    }
    CloseClipboard();
}


static void KeyResult(bool ok, const std::wstring& msg) {
    g_keyMsgOk = ok;
    g_keyMsg = msg;
    g_keyMsgT = 6.f;
    if (!ok) g_keyShake = 1.f;
}


static void AddKeyChar(wchar_t c) {
    c = (wchar_t)towupper(c);
    bool valid = (c >= L'A' && c <= L'Z') || (c >= L'2' && c <= L'7');
    if (valid && g_inputKey.length() < 20) g_inputKey += c;
}

static void PasteKey() {
    if (!OpenClipboard(g_hWnd)) return;
    HANDLE h = GetClipboardData(CF_UNICODETEXT);
    if (h) {
        const wchar_t* p = (const wchar_t*)GlobalLock(h);
        if (p) {
            for (; *p; p++) AddKeyChar(*p);
            GlobalUnlock(h);
        }
    }
    CloseClipboard();
}

static void OpenBuyPage() {
    std::wstring url = std::wstring(BUY_URL) + L"?plan=" + PLANS[g_selPlan].code;
    ShellExecuteW(nullptr, L"open", url.c_str(), nullptr, nullptr, SW_SHOWNORMAL);
}

static void StartClose() { g_closing = true; }

static void Activate(Hit h) {
    switch (h) {
    case H_CLOSE: StartClose(); break;
    case H_MIN:   ShowWindow(g_hWnd, SW_MINIMIZE); break;
    case H_CHIP:  g_menuOpen = !g_menuOpen; break;
    case H_MENU_SUB: GoTo(SCR_SUB); break;
    case H_MENU_OUT: g_menuOpen = false; Logout(); break;
    case H_CONNECT: g_rip = 0.001f; ToggleConnection(); break;
    case H_EDIT_AVATAR: SelectAvatar(); break;
    case H_LOGIN: SubmitAuth(); break;
    case H_SWITCH:
        GoTo(g_screen == SCR_LOGIN ? SCR_REGISTER : SCR_LOGIN);
        g_loginError.clear();
        break;
    case H_BUY:  OpenBuyPage(); break;
    case H_ACTIVATE: RedeemKey(); break;
    case H_BACK: GoTo(SCR_MAIN); break;
    case H_PLAN0: case H_PLAN1: case H_PLAN2: g_selPlan = h - H_PLAN0; break;
    default: break;
    }
}

static void Update(float dt) {
    g_time += dt;
    g_scrTime += dt;
    ULONGLONG now = GetTickCount64();

    for (int i = 0; i < H_COUNT; i++) Approach(g_hv[i], g_hit == i ? 1.f : 0.f, 14.f, dt);
    Approach(g_press, (g_down == H_CONNECT && g_hit == H_CONNECT) ? 1.f : 0.f, 22.f, dt);
    Approach(g_conn, g_state == ST_ON ? 1.f : (g_state == ST_CONNECTING ? 0.35f : 0.f), 5.f, dt);
    if (g_screen != SCR_MAIN) g_menuOpen = false;
    Approach(g_menuAnim, g_menuOpen ? 1.f : 0.f, 16.f, dt);
    for (int i = 0; i < 3; i++) Approach(g_planSel[i], g_selPlan == i ? 1.f : 0.f, 12.f, dt);
    bool auth = g_screen == SCR_LOGIN || g_screen == SCR_REGISTER;
    Approach(g_focus[0], (auth && g_inputFocusUser) ? 1.f : 0.f, 12.f, dt);
    Approach(g_focus[1], (auth && !g_inputFocusUser) ? 1.f : 0.f, 12.f, dt);
    Approach(g_focus[2], (g_screen == SCR_SUB && g_keyFocus) ? 1.f : 0.f, 12.f, dt);
    Approach(g_priceShown, (float)PLANS[g_selPlan].price, 9.f, dt);
    g_shake = std::max(0.f, g_shake - dt * 2.2f);
    g_keyShake = std::max(0.f, g_keyShake - dt * 2.2f);
    if (g_keyMsgT > 0.f) g_keyMsgT = std::max(0.f, g_keyMsgT - dt);
    g_pingFade = std::min(1.f, g_pingFade + dt * 2.5f);
    if (g_rip > 0.f) { g_rip += dt * 1.5f; if (g_rip >= 1.f) g_rip = 0.f; }

    if (!g_acc.user.empty()) {
        Approach(g_trafficShown, (float)((double)g_acc.traffic / (double)GB), 6.f, dt);
        Approach(g_periodBar, IsPremium() ? PeriodFrac() : 0.f, 4.f, dt);
    }

    if (g_state == ST_ON && !g_busy && !IsPremium()) { ShowError(7); ToggleConnection(); }

    if (g_state == ST_ON && now - g_lastSample >= 1000) {
        SampleTraffic(now);
    }
    if (!g_acc.user.empty() && now - g_lastSave >= 15000) { g_lastSave = now; RefreshMe(); }
    if (g_state != ST_ON) { g_downSpeed *= 0.9f; g_upSpeed *= 0.9f; }

    float speedMul = 1.f + g_conn * 1.6f;
    for (auto& p : g_particles) {
        p.y -= p.speed * speedMul * dt;
        p.x += sinf(g_time * 0.8f + p.phase) * 10.f * dt;
        if (p.y < -5.f) { p.y = (float)H + 5.f; p.x = (float)(std::rand() % W); }
    }

    if (g_closing) {
        g_alpha -= dt * 5.f;
        if (g_alpha <= 0.f) { g_alpha = 0.f; DestroyWindow(g_hWnd); return; }
    }
    else if (g_alpha < 1.f) {
        g_alpha = std::min(1.f, g_alpha + dt * 4.f);
    }
    BYTE a = (BYTE)(g_alpha * 255.f);
    if (a != g_lastA) {
        SetLayeredWindowAttributes(g_hWnd, 0, a, LWA_ALPHA);
        g_lastA = a;
    }
}

static void DrawWinButton(Graphics& g, RECT r, float hv, bool close) {
    float x = (float)r.left, y = (float)r.top;
    float w = (float)(r.right - r.left), h = (float)(r.bottom - r.top);

    GraphicsPath p;
    RoundRectPath(p, x, y, w, h, 9.f);
    Color bgc = close ? Color((BYTE)(235 * hv), 215, 45, 110)
        : Color((BYTE)(45 * hv), 255, 255, 255);
    SolidBrush b(bgc);
    g.FillPath(&b, &p);

    Color ic = LerpC(Color(255, 190, 160, 235), Color(255, 255, 255, 255), hv);
    Pen pen(ic, 1.9f);
    pen.SetStartCap(LineCapRound);
    pen.SetEndCap(LineCapRound);
    float cx = x + w / 2.f, cy = y + h / 2.f;
    if (close) {
        float s = 5.f + hv * 0.8f;
        g.DrawLine(&pen, cx - s, cy - s, cx + s, cy + s);
        g.DrawLine(&pen, cx + s, cy - s, cx - s, cy + s);
    }
    else {
        g.DrawLine(&pen, cx - 6.f, cy + 2.f, cx + 6.f, cy + 2.f);
    }
}

static void PaintBackground(Graphics& g) {
    const float t = g_time, conn = g_conn;
    LinearGradientBrush bg(RectF(0.f, 0.f, (REAL)W, (REAL)H), Color(255, 12, 8, 24), Color(255, 2, 1, 6), LinearGradientModeVertical);
    g.FillRectangle(&bg, 0, 0, W, H);

    auto glow = [&](float gx, float gy, float rx, float ry, Color c) {
        GraphicsPath gp;
        gp.AddEllipse(gx - rx, gy - ry, rx * 2.f, ry * 2.f);
        PathGradientBrush pg(&gp);
        pg.SetCenterColor(c);
        Color sc(0, c.GetR(), c.GetG(), c.GetB());
        int cnt = 1;
        pg.SetSurroundColors(&sc, &cnt);
        g.FillPath(&pg, &gp);
    };
    float drift = sinf(t * 0.35f) * 18.f;
    glow(W / 2.f + drift, 110.f, 280.f, 220.f, Color((BYTE)(30.f + 40.f * conn), 130, 60, 230));
    if (conn > 0.02f && g_screen == SCR_MAIN)
        glow(W / 2.f - drift, 300.f, 250.f, 130.f, Color((BYTE)(42.f * conn), 170, 90, 255));
    if (g_screen == SCR_SUB)
        glow(W / 2.f - drift, 190.f, 260.f, 130.f, Color((BYTE)(IsPremium() ? 40 : 22), IsPremium() ? 225 : 150, IsPremium() ? 160 : 90, 255));

    for (auto& p : g_particles) {
        float tw = 0.5f + 0.5f * sinf(t * 1.4f + p.phase);
        BYTE pa = (BYTE)(110.f * p.alpha * (0.3f + 0.7f * tw) * (0.55f + 0.45f * conn));
        SolidBrush pb(Color(pa, 200, 150, 255));
        g.FillEllipse(&pb, p.x - p.r, p.y - p.r, p.r * 2.f, p.r * 2.f);
    }
}

static void PaintTitleBar(Graphics& g) {
    Pen sep(Color(255, 28, 20, 44), 1.f);
    g.DrawLine(&sep, 18, TITLE_H - 1, W - 18, TITLE_H - 1);

    DrawLogo(g, 14.f, 11.f, 22.f);
    Txt(g, L"NULL VPN", g_fBar, RectF(46, 0, 200, (REAL)TITLE_H), Color(255, 215, 190, 255), StringAlignmentNear);

    if (g_screen == SCR_MAIN) {
        RECT cr2 = ChipRect();
        float cx = (float)cr2.left, cy = (float)cr2.top;
        float cw = (float)(cr2.right - cr2.left), ch = (float)(cr2.bottom - cr2.top);
        float hv = std::max(g_hv[H_CHIP], g_menuAnim);
        bool prem = IsPremium();

        GraphicsPath chp;
        RoundRectPath(chp, cx, cy, cw, ch, ch / 2.f);
        SolidBrush chb(Color((BYTE)(26.f + 34.f * hv), 190, 140, 255));
        g.FillPath(&chb, &chp);
        Pen chpn(prem ? Color((BYTE)(110.f + 80.f * hv), 225, 165, 255) : Color((BYTE)(70.f + 90.f * hv), 150, 90, 235), 1.f);
        g.DrawPath(&chpn, &chp);

        float ar2 = 10.f, ax = cx + 5.f + ar2, ay = cy + ch / 2.f;
        if (g_avatarBitmap) {
            DrawAvatar(g, ax, ay, ar2);
        }
        else {
            GraphicsPath adisc;
            adisc.AddEllipse(ax - ar2, ay - ar2, ar2 * 2.f, ar2 * 2.f);
            LinearGradientBrush ab(RectF(ax - ar2, ay - ar2, ar2 * 2.f, ar2 * 2.f), Color(255, 190, 120, 255), Color(255, 100, 45, 205), LinearGradientModeForwardDiagonal);
            g.FillPath(&ab, &adisc);
            WCHAR ini[2] = { g_acc.user.empty() ? L'?' : (WCHAR)towupper(g_acc.user[0]), 0 };
            Txt(g, ini, g_fSmall, RectF(ax - ar2, ay - ar2, ar2 * 2.f, ar2 * 2.f), Color(255, 255, 255, 255));
        }

        float tw = cw - (ar2 * 2.f + 5.f + 8.f + 28.f) - (prem ? 18.f : 0.f);
        TxtEll(g, g_acc.user.c_str(), g_fSmall, RectF(ax + ar2 + 8.f, cy, tw, ch), Color(255, 232, 218, 255));

        if (prem) {
            GraphicsState s = g.Save();
            g.TranslateTransform(cx + cw - 38.f, cy + ch / 2.f);
            g.ScaleTransform(0.6f, 0.6f);
            IconCrown(g, 0.f, 0.f, Color(255, 235, 190, 255));
            g.Restore(s);
        }

        float ma = g_menuAnim;
        Pen chv(LerpC(Color(255, 170, 140, 225), Color(255, 255, 255, 255), hv), 1.8f);
        chv.SetStartCap(LineCapRound);
        chv.SetEndCap(LineCapRound);
        chv.SetLineJoin(LineJoinRound);
        float ccx = cx + cw - 16.f, ccy = cy + ch / 2.f;
        float sideY = ccy - 2.f + 4.f * ma, apexY = ccy + 2.f - 4.f * ma;
        PointF cv[3] = { PointF(ccx - 4.f, sideY), PointF(ccx, apexY), PointF(ccx + 4.f, sideY) };
        g.DrawLines(&chv, cv, 3);
    }

    DrawWinButton(g, MinRect(), g_hv[H_MIN], false);
    DrawWinButton(g, CloseRect(), g_hv[H_CLOSE], true);
}

static void PaintAuth(Graphics& g) {
    const float t = g_time;
    bool isReg = (g_screen == SCR_REGISTER);

    {
        Reveal rv(g, 0, RectF(W / 2.f - 60.f, 40.f, 120.f, 110.f));
        float ls = 72.f, lx = (W - ls) / 2.f, ly = 62.f;
        for (int i = 3; i >= 1; i--) {
            GraphicsPath gp;
            RoundRectPath(gp, lx - i * 5.f, ly - i * 5.f, ls + i * 10.f, ls + i * 10.f, ls * 0.3f + i * 5.f);
            SolidBrush gb(Color((BYTE)(16.f + 5.f * (3 - i) + 6.f * sinf(t * 1.6f)), 160, 90, 255));
            g.FillPath(&gb, &gp);
        }
        float bob = sinf(t * 1.3f) * 2.f;
        DrawLogo(g, lx, ly + bob, ls);
    }

    {
        Reveal rv(g, 1, RectF(0.f, 150.f, (REAL)W, 70.f));
        Txt(g, isReg ? L"СОЗДАТЬ АККАУНТ" : L"ВХОД В АККАУНТ", g_fBrand, RectF(0, 150, (REAL)W, 40), Color(255, 255, 255, 255));
        Txt(g, isReg ? L"Логин от 3 символов, пароль от 6" : L"Войдите, чтобы подключиться", g_fInput,
            RectF(0, 190, (REAL)W, 24), Color(255, 130, 108, 175));
    }

    float shakeX = sinf(g_shake * 28.f) * 8.f * g_shake;

    auto field = [&](int idx, float y, float f, bool isUser, const std::wstring& val, const WCHAR* placeholder, bool secret) {
        Reveal rv(g, idx, RectF(100.f, y - 4.f, 320.f, 58.f));
        GraphicsState gs = g.Save();
        g.TranslateTransform(shakeX, 0.f);

        GraphicsPath fp2;
        RoundRectPath(fp2, 110.f, y, 300.f, 50.f, 12.f);
        if (f > 0.02f) {
            for (int i = 2; i >= 1; i--) {
                Pen gp2(Color((BYTE)(f * 26 / i), 160, 90, 255), 2.f + i * 3.f);
                g.DrawPath(&gp2, &fp2);
            }
        }
        SolidBrush fb(LerpC(Color(255, 17, 12, 30), Color(255, 24, 16, 44), f));
        g.FillPath(&fb, &fp2);
        bool err = !g_loginError.empty() && g_shake > 0.01f;
        Color bc = err ? Color(255, 235, 80, 120) : LerpC(Color(255, 56, 42, 92), Color(255, 150, 80, 235), f);
        Pen fbd(bc, 1.f + 0.6f * f);
        g.DrawPath(&fbd, &fp2);

        Color icc = LerpC(Color(255, 110, 88, 160), Color(255, 190, 130, 255), f);
        if (isUser) IconUser(g, 134.f, y + 25.f, icc);
        else IconLock(g, 134.f, y + 25.f, icc);

        std::wstring shown = secret ? std::wstring(val.length(), L'\x2022') : val;
        if (val.empty())
            Txt(g, placeholder, g_fInput, RectF(160.f + 4.f * f, y, 240.f, 50.f), Color(255, 100, 80, 140), StringAlignmentNear);
        else
            Txt(g, shown.c_str(), g_fInput, RectF(160.f, y, 240.f, 50.f), Color(255, 255, 255, 255), StringAlignmentNear);

        if (f > 0.5f && fmodf(t, 1.0f) < 0.55f) {
            float cxp = 160.f + 3.f + TextW(g, shown, g_fInput);
            Pen cp2(Color(255, 220, 180, 255), 1.5f);
            g.DrawLine(&cp2, cxp, y + 15.f, cxp, y + 35.f);
        }
        g.Restore(gs);
    };
    field(2, 230.f, g_focus[0], true, g_inputUser, L"Логин", false);
    field(3, 300.f, g_focus[1], false, g_inputPass, L"Пароль", true);

    {
        Reveal rv(g, 4, RectF(100.f, 352.f, 320.f, 86.f));
        if (!g_loginError.empty())
            Txt(g, g_loginError.c_str(), g_fSmall, RectF(0, 356, (REAL)W, 20), Color(255, 255, 110, 130));

        RECT lbr = LoginBtnRect();
        float lhv = g_hv[H_LOGIN];
        bool ready = !g_inputUser.empty() && !g_inputPass.empty();
        float pr = (g_down == H_LOGIN && g_hit == H_LOGIN) ? 1.f : 0.f;
        float lbx = (float)lbr.left + pr * 2.f, lby = (float)lbr.top + pr * 1.5f;
        float lbw = (float)(lbr.right - lbr.left) - pr * 4.f, lbh = (float)(lbr.bottom - lbr.top) - pr * 3.f;
        GraphicsPath lbp;
        RoundRectPath(lbp, lbx, lby, lbw, lbh, 14.f);
        if (ready) {
            Glow(g, lbx, lby, lbw, lbh, 14.f, Color(255, 190, 100, 255), lhv);
            Color l1 = LerpC(Color(255, 118, 52, 214), Color(255, 150, 82, 244), lhv);
            Color l2 = LerpC(Color(255, 172, 80, 248), Color(255, 214, 118, 255), lhv);
            LinearGradientBrush lbb(RectF(lbx, lby, lbw, lbh), l1, l2, LinearGradientModeForwardDiagonal);
            g.FillPath(&lbb, &lbp);
            g.SetClip(&lbp);
            float sp = fmodf(t * 0.35f, 1.8f) - 0.4f;
            float sx = lbx + sp * lbw;
            LinearGradientBrush s1(PointF(sx, 0), PointF(sx + 40.f, 0), Color(0, 255, 255, 255), Color(45, 255, 255, 255));
            LinearGradientBrush s2(PointF(sx + 40.f, 0), PointF(sx + 80.f, 0), Color(45, 255, 255, 255), Color(0, 255, 255, 255));
            g.FillRectangle(&s1, sx, lby, 40.f, lbh);
            g.FillRectangle(&s2, sx + 40.f, lby, 40.f, lbh);
            g.ResetClip();
            Pen led(Color(70, 255, 255, 255), 1.f);
            g.DrawPath(&led, &lbp);
        }
        else {
            SolidBrush lbc(Color(255, 34, 26, 54));
            g.FillPath(&lbc, &lbp);
        }
        Txt(g, isReg ? L"ЗАРЕГИСТРИРОВАТЬСЯ" : L"ВОЙТИ", g_fBtn, RectF(lbx, lby, lbw, lbh),
            ready ? Color(255, 255, 255, 255) : Color(255, 105, 88, 140));
    }

    {
        Reveal rv(g, 5, RectF(100.f, 456.f, 320.f, 40.f));
        RECT rbr = RegBtnRect();
        Txt(g, isReg ? L"Уже есть аккаунт? Войти" : L"Нет аккаунта? Создать", g_fInput,
            RectF((REAL)rbr.left, (REAL)rbr.top, (REAL)(rbr.right - rbr.left), 30),
            LerpC(Color(255, 160, 130, 215), Color(255, 255, 255, 255), g_hv[H_SWITCH]));
    }
}

static void FmtSpeed(float bps, WCHAR* out, size_t n) {
    if (bps >= 1048576.f) swprintf_s(out, n, L"%.1f МБ/с", bps / 1048576.f);
    else swprintf_s(out, n, L"%.0f КБ/с", bps / 1024.f);
}

static void PaintMain(Graphics& g) {
    const float t = g_time, conn = g_conn;
    const ULONGLONG now = GetTickCount64();
    const bool prem = IsPremium();

    float acx = W / 2.f, acy = 100.f, ar = 38.f;

    {
        Reveal rv(g, 0, RectF(100.f, 52.f, 320.f, 134.f));
        float pulse = 0.5f + 0.5f * sinf(t * 2.f);
        Color ringc = LerpC(Color(255, 70, 50, 100), Color(255, 190, 110, 255), conn);

        if (conn > 0.05f) {
            for (int i = 3; i >= 1; i--) {
                Pen gp(Color((BYTE)(conn * (30.f / i) * (0.6f + 0.4f * pulse)), 190, 110, 255), 2.f + i * 3.f);
                g.DrawEllipse(&gp, acx - ar - 4.f, acy - ar - 4.f, (ar + 4.f) * 2.f, (ar + 4.f) * 2.f);
            }
        }

        if (g_avatarBitmap) {
            DrawAvatar(g, acx, acy, ar);
        }
        else {
            GraphicsPath disc;
            disc.AddEllipse(acx - ar, acy - ar, ar * 2.f, ar * 2.f);
            LinearGradientBrush db(RectF(acx - ar, acy - ar, ar * 2.f, ar * 2.f), Color(255, 30, 18, 50), Color(255, 8, 5, 16), LinearGradientModeVertical);
            g.FillPath(&db, &disc);
            g.SetClip(&disc);
            SolidBrush sil(LerpC(Color(255, 78, 58, 115), Color(255, 165, 110, 235), conn));
            g.FillEllipse(&sil, acx - 13.f, acy - 22.f, 26.f, 26.f);
            g.FillEllipse(&sil, acx - 28.f, acy + 8.f, 56.f, 46.f);
            g.ResetClip();
        }

        Pen ring(ringc, 2.5f);
        g.DrawEllipse(&ring, acx - ar - 4.f, acy - ar - 4.f, (ar + 4.f) * 2.f, (ar + 4.f) * 2.f);

        if (g_state != ST_OFF) {
            Pen arc(Color(255, 235, 180, 255), 2.5f);
            arc.SetStartCap(LineCapRound);
            arc.SetEndCap(LineCapRound);
            g.DrawArc(&arc, acx - ar - 4.f, acy - ar - 4.f, (ar + 4.f) * 2.f, (ar + 4.f) * 2.f, fmodf(t * (g_state == ST_CONNECTING ? 420.f : 150.f), 360.f), 80.f);
        }

        float dx = acx + ar * 0.74f, dy = acy + ar * 0.74f;
        SolidBrush blk(Color(255, 0, 0, 0));
        g.FillEllipse(&blk, dx - 9.f, dy - 9.f, 18.f, 18.f);
        Color dc = g_state == ST_ON ? Color(255, 110, 235, 160) : (g_state == ST_CONNECTING ? Color(255, 230, 190, 90) : Color(255, 110, 90, 140));
        SolidBrush dcb(dc);
        g.FillEllipse(&dcb, dx - 6.f, dy - 6.f, 12.f, 12.f);

        RECT ear = EditAvatarRect();
        float eacx = (ear.left + ear.right) / 2.f, eacy = (ear.top + ear.bottom) / 2.f;
        float eh = g_hv[H_EDIT_AVATAR];
        SolidBrush eb(LerpC(Color(255, 140, 60, 220), Color(255, 180, 80, 255), eh));
        float er = (ear.right - ear.left) / 2.f + eh * 1.5f;
        g.FillEllipse(&eb, eacx - er, eacy - er, er * 2.f, er * 2.f);
        Pen ep(Color(255, 255, 255, 255), 2.f);
        g.DrawRectangle(&ep, eacx - 5.f, eacy - 4.f, 10.f, 8.f);
        g.DrawRectangle(&ep, eacx - 2.f, eacy - 7.f, 4.f, 3.f);

        Txt(g, g_acc.user.c_str(), g_fBrand, RectF(0, 142, (REAL)W, 40), Color(255, 255, 255, 255));
    }

    bool showErr = g_errMsg && now < g_errUntil;
    bool noReply = g_state == ST_ON && g_failStreak >= 3;
    const WCHAR* st = showErr ? g_errMsg
        : (g_state == ST_ON ? (noReply ? L"НЕТ ОТВЕТА" : L"ЗАЩИЩЕНО")
            : (g_state == ST_CONNECTING ? L"ПОДКЛЮЧЕНИЕ" : (prem ? L"НЕ ПОДКЛЮЧЕНО" : L"НУЖНА ПОДПИСКА")));
    Color tc = showErr ? Color(255, 255, 120, 145)
        : (g_state == ST_ON ? (noReply ? Color(255, 240, 200, 90) : Color(255, 200, 255, 220))
            : (prem || g_state == ST_CONNECTING ? Color(255, 170, 140, 215) : Color(255, 240, 200, 110)));
    {
        Reveal rv(g, 1, RectF(120.f, 184.f, 280.f, 38.f));
        RectF sbnd;
        g.MeasureString(st, -1, g_fStatus, PointF(0.f, 0.f), &sbnd);
        float pw2 = sbnd.Width + 46.f, ph2 = 30.f;
        float px2 = (W - pw2) / 2.f, py2 = 188.f;
        GraphicsPath stp;
        RoundRectPath(stp, px2, py2, pw2, ph2, ph2 / 2.f);
        SolidBrush stb(Color(34, tc.GetR(), tc.GetG(), tc.GetB()));
        g.FillPath(&stb, &stp);
        Pen stpn(Color(90, tc.GetR(), tc.GetG(), tc.GetB()), 1.f);
        g.DrawPath(&stpn, &stp);
        float dotA = g_state == ST_CONNECTING ? (0.5f + 0.5f * sinf(t * 8.f)) : (g_state == ST_ON ? 0.75f + 0.25f * sinf(t * 2.5f) : 1.f);
        SolidBrush dotb2(Color((BYTE)(255.f * dotA), tc.GetR(), tc.GetG(), tc.GetB()));
        g.FillEllipse(&dotb2, px2 + 14.f, py2 + ph2 / 2.f - 3.5f, 7.f, 7.f);
        Txt(g, st, g_fStatus, RectF(px2 + 22.f, py2, pw2 - 22.f, ph2), tc);
    }

    {
        Reveal rv(g, 2, RectF(70.f, 246.f, 380.f, 108.f));
        RECT br = BtnRect();
        float bcx = (br.left + br.right) / 2.f, bcy = (br.top + br.bottom) / 2.f;
        float hover = g_hv[H_CONNECT];
        float scale = 1.f + hover * 0.04f - g_press * 0.06f;
        float bw = 300.f * scale, bh = 76.f * scale;
        float bx = bcx - bw / 2.f, by = bcy - bh / 2.f;

        if (conn > 0.02f) {
            for (int k = 0; k < 3; k++) {
                float ph = fmodf(t * 0.45f + k / 3.f, 1.f);
                float e = ph * 55.f;
                GraphicsPath rp;
                RoundRectPath(rp, bx - e, by - e, bw + 2 * e, bh + 2 * e, bh / 2.f + e);
                Pen rpn(Color((BYTE)((1.f - ph) * (1.f - ph) * 110.f * conn), 200, 120, 255), 2.f);
                g.DrawPath(&rpn, &rp);
            }
        }
        if (g_rip > 0.f) {
            float e = Ease(g_rip) * 38.f;
            GraphicsPath rp;
            RoundRectPath(rp, bx - e, by - e, bw + 2 * e, bh + 2 * e, bh / 2.f + e);
            Pen rpn(Color((BYTE)((1.f - g_rip) * 160.f), 235, 190, 255), 2.f);
            g.DrawPath(&rpn, &rp);
        }

        GraphicsPath bp;
        RoundRectPath(bp, bx, by, bw, bh, bh / 2.f);
        Color c1 = LerpC(LerpC(Color(255, 88, 36, 165), Color(255, 150, 64, 238), conn), Color(255, 175, 120, 255), hover * 0.25f);
        Color c2 = LerpC(LerpC(Color(255, 128, 62, 208), Color(255, 222, 98, 255), conn), Color(255, 210, 150, 255), hover * 0.25f);
        LinearGradientBrush body(RectF(bx, by, bw, bh), c1, c2, LinearGradientModeForwardDiagonal);
        g.FillPath(&body, &bp);

        g.SetClip(&bp);
        float sp = fmodf(t * (g_state == ST_CONNECTING ? 1.1f : 0.3f), 1.8f) - 0.4f;
        float sx = bx + sp * bw;
        LinearGradientBrush s1(PointF(sx, 0), PointF(sx + 45.f, 0), Color(0, 255, 255, 255), Color(55, 255, 255, 255));
        LinearGradientBrush s2(PointF(sx + 45.f, 0), PointF(sx + 90.f, 0), Color(55, 255, 255, 255), Color(0, 255, 255, 255));
        g.FillRectangle(&s1, sx, by, 45.f, bh);
        g.FillRectangle(&s2, sx + 45.f, by, 45.f, bh);
        g.ResetClip();

        Pen edge(Color(70, 255, 255, 255), 1.2f);
        g.DrawPath(&edge, &bp);

        const WCHAR* base = !prem && g_state == ST_OFF ? L"ОФОРМИТЬ ПОДПИСКУ"
            : (g_state == ST_ON ? L"ОТКЛЮЧИТЬ" : (g_state == ST_CONNECTING ? L"ПОДКЛЮЧЕНИЕ" : L"ПОДКЛЮЧИТЬ"));
        RectF bound;
        g.MeasureString(base, -1, g_fBtn, PointF(0, 0), &bound);
        float total = 22.f + 12.f + bound.Width;
        float icx = bcx - total / 2.f + 11.f;
        float tx = icx + 11.f + 12.f;

        Pen ip(Color(255, 255, 255, 255), 2.4f);
        ip.SetStartCap(LineCapRound);
        ip.SetEndCap(LineCapRound);
        if (g_state == ST_CONNECTING) {
            g.DrawArc(&ip, icx - 9.f, bcy - 9.f, 18.f, 18.f, fmodf(t * 420.f, 360.f), 260.f);
        }
        else if (!prem && g_state == ST_OFF) {
            GraphicsState s = g.Save();
            g.TranslateTransform(icx, bcy);
            g.ScaleTransform(1.25f, 1.25f);
            IconCrown(g, 0.f, 0.f, Color(255, 255, 255, 255));
            g.Restore(s);
        }
        else {
            g.DrawArc(&ip, icx - 9.f, bcy - 9.f, 18.f, 18.f, 300.f, 300.f);
            g.DrawLine(&ip, icx, bcy - 11.f, icx, bcy - 1.f);
        }

        WCHAR label[40];
        if (g_state == ST_CONNECTING) {
            int dots = ((int)(t * 3.f)) % 4;
            swprintf_s(label, L"%s%s", base, dots == 0 ? L"" : (dots == 1 ? L"." : (dots == 2 ? L".." : L"...")));
        }
        else {
            swprintf_s(label, L"%s", base);
        }
        Txt(g, label, g_fBtn, RectF(tx - 3.f, by, bound.Width + 40.f, bh), Color(255, 255, 255, 255), StringAlignmentNear);
    }

    {
        Reveal rv(g, 3, RectF(100.f, 352.f, 320.f, 60.f));
        WCHAR buf[32];
        if (g_state == ST_ON) {
            int s = (int)((now - g_connectedAt) / 1000);
            swprintf_s(buf, L"%02d:%02d:%02d", s / 3600, (s / 60) % 60, s % 60);
        }
        else {
            swprintf_s(buf, L"--:--:--");
        }
        Color tc2 = LerpC(Color(255, 95, 72, 130), Color(255, 220, 195, 255), conn);
        Txt(g, buf, g_fTimer, RectF(0, 356, (REAL)W, 34), tc2);
        Txt(g, L"ВРЕМЯ СЕССИИ", g_fSmall, RectF(0, 390, (REAL)W, 18), Color(255, 105, 82, 145));
    }

    {
        Reveal rv(g, 4, RectF(36.f, 436.f, 448.f, 148.f));
        float cx0 = 40.f, cy0 = 440.f, cw = (float)W - 80.f, ch = 140.f;
        Card(g, cx0, cy0, cw, ch, 18.f, Color(255, 20, 14, 36), Color(255, 9, 7, 16), Color(255, 52, 38, 84));

        Txt(g, L"PING", g_fSmall, RectF(cx0 + 20.f, cy0 + 12.f, 60.f, 28.f), Color(255, 150, 120, 200), StringAlignmentNear);

        if (g_state == ST_ON) {
            WCHAR sb[32];
            float sx0 = cx0 + 84.f, sy0 = cy0 + 26.f;
            IconArrow(g, sx0, sy0, false, Color(255, 120, 230, 160));
            FmtSpeed(g_downSpeed, sb, 32);
            Txt(g, sb, g_fSmall, RectF(sx0 + 8.f, cy0 + 12.f, 84.f, 28.f), Color(255, 215, 200, 245), StringAlignmentNear);
            IconArrow(g, sx0 + 102.f, sy0, true, Color(255, 200, 140, 255));
            FmtSpeed(g_upSpeed, sb, 32);
            Txt(g, sb, g_fSmall, RectF(sx0 + 110.f, cy0 + 12.f, 84.f, 28.f), Color(255, 215, 200, 245), StringAlignmentNear);
        }

        if (g_state == ST_ON && g_ping > 0) {
            int p = g_ping;
            Color pc = p < 80 ? Color(255, 120, 230, 160) : (p < 150 ? Color(255, 240, 200, 90) : Color(255, 240, 90, 120));
            Color shown = LerpC(Color(255, 150, 120, 200), pc, g_pingFade);
            WCHAR pbuf[32];
            swprintf_s(pbuf, L"%d ms", p);
            Txt(g, pbuf, g_fPing, RectF(cx0 + cw - 150.f, cy0 + 8.f, 130.f, 34.f), shown, StringAlignmentFar);
        }
        else {
            Txt(g, L"---", g_fPing, RectF(cx0 + cw - 150.f, cy0 + 8.f, 130.f, 34.f), Color(255, 110, 90, 150), StringAlignmentFar);
        }

        float gl = cx0 + 20.f, gr = cx0 + cw - 20.f, gt = cy0 + 52.f, gb = cy0 + ch - 18.f;
        Pen grid(Color(255, 24, 16, 38), 1.f);
        for (int i = 0; i < 3; i++) {
            float y = gt + (gb - gt) * i / 2.f;
            g.DrawLine(&grid, gl, y, gr, y);
        }

        int n = (int)g_pings.size();
        if (g_state == ST_ON && n >= 2) {
            float step = (gr - gl) / 39.f;
            std::vector<PointF> pts(n);
            for (int i = 0; i < n; i++)
                pts[i] = PointF(gr - (n - 1 - i) * step, gb - Clamp01(g_pings[i] / 220.f) * (gb - gt));
            GraphicsPath fp;
            fp.AddCurve(pts.data(), n, 0.4f);
            fp.AddLine(pts[n - 1].X, gb, pts[0].X, gb);
            fp.CloseFigure();
            LinearGradientBrush fb(PointF(0, gt), PointF(0, gb), Color(90, 170, 90, 255), Color(0, 170, 90, 255));
            g.FillPath(&fb, &fp);

            Pen lp(Color(255, 195, 125, 255), 2.2f);
            lp.SetLineJoin(LineJoinRound);
            g.DrawCurve(&lp, pts.data(), n, 0.4f);

            PointF last = pts[n - 1];
            float pr = 3.f + 2.f * (0.5f + 0.5f * sinf(t * 4.f));
            SolidBrush glowb(Color(60, 200, 130, 255));
            g.FillEllipse(&glowb, last.X - pr - 3.f, last.Y - pr - 3.f, (pr + 3.f) * 2.f, (pr + 3.f) * 2.f);
            SolidBrush dotb(Color(255, 255, 255, 255));
            g.FillEllipse(&dotb, last.X - 3.f, last.Y - 3.f, 6.f, 6.f);
        }
    }
}

static void IconKey(Graphics& g, float cx, float cy, Color c) {
    Pen p(c, 1.7f);
    p.SetStartCap(LineCapRound);
    p.SetEndCap(LineCapRound);
    g.DrawEllipse(&p, cx - 8.f, cy - 4.f, 8.f, 8.f);
    g.DrawLine(&p, cx, cy, cx + 8.f, cy);
    g.DrawLine(&p, cx + 5.f, cy, cx + 5.f, cy + 3.f);
    g.DrawLine(&p, cx + 8.f, cy, cx + 8.f, cy + 3.f);
}

static std::wstring FormatKey(const std::wstring& k) {
    std::wstring out;
    for (size_t i = 0; i < k.size(); i++) {
        if (i && i % 5 == 0) out += L'-';
        out += k[i];
    }
    return out;
}

static void PaintSub(Graphics& g) {
    const float t = g_time;
    const bool prem = IsPremium();
    const Color white(255, 255, 255, 255);

    {
        Reveal rv(g, 0, RectF(10.f, 48.f, 500.f, 50.f));
        RECT br = BackRect();
        float hv = g_hv[H_BACK];
        float bx = (float)br.left, by = (float)br.top;
        GraphicsPath p;
        RoundRectPath(p, bx, by, 36.f, 36.f, 18.f);
        SolidBrush fb(Color((BYTE)(40.f * hv), 190, 140, 255));
        g.FillPath(&fb, &p);
        Pen pn(LerpC(Color(255, 80, 58, 130), Color(255, 190, 130, 255), hv), 1.2f);
        g.DrawPath(&pn, &p);
        Pen ar(LerpC(Color(255, 170, 140, 225), white, hv), 1.9f);
        ar.SetStartCap(LineCapRound);
        ar.SetEndCap(LineCapRound);
        ar.SetLineJoin(LineJoinRound);
        float cx = bx + 18.f - hv * 1.5f, cy = by + 18.f;
        PointF pts[3] = { PointF(cx + 3.f, cy - 6.f), PointF(cx - 3.f, cy), PointF(cx + 3.f, cy + 6.f) };
        g.DrawLines(&ar, pts, 3);
        Txt(g, L"ПОДПИСКА", g_fBrand, RectF(0, 50, (REAL)W, 44), white);
    }

    {
        Reveal rv(g, 1, RectF(34.f, 98.f, 452.f, 104.f));
        float x = 40.f, y = 100.f, w = 440.f, h = 100.f;
        float breathe = 0.5f + 0.5f * sinf(t * 1.8f);
        Color border = prem ? LerpC(Color(255, 120, 70, 210), Color(255, 225, 160, 255), breathe)
            : LerpC(Color(255, 52, 38, 84), Color(255, 92, 66, 150), breathe);
        if (prem) Glow(g, x, y, w, h, 18.f, Color(255, 220, 150, 255), 0.5f + 0.5f * breathe);
        Card(g, x, y, w, h, 18.f,
            prem ? Color(255, 36, 24, 66) : Color(255, 20, 14, 36),
            prem ? Color(255, 16, 10, 32) : Color(255, 9, 7, 16),
            border, prem ? 1.6f : 1.f);

        GraphicsPath cp;
        RoundRectPath(cp, x, y, w, h, 18.f);
        g.SetClip(&cp);
        float sp = fmodf(t * (prem ? 0.2f : 0.12f), 1.7f) - 0.35f;
        float sx = x + sp * w;
        BYTE sa = prem ? 28 : 14;
        LinearGradientBrush s1(PointF(sx, 0), PointF(sx + 50.f, 0), Color(0, 255, 255, 255), Color(sa, 255, 255, 255));
        LinearGradientBrush s2(PointF(sx + 50.f, 0), PointF(sx + 100.f, 0), Color(sa, 255, 255, 255), Color(0, 255, 255, 255));
        g.FillRectangle(&s1, sx, y, 50.f, h);
        g.FillRectangle(&s2, sx + 50.f, y, 50.f, h);
        g.ResetClip();

        float icx = x + 48.f, icy = y + h / 2.f + sinf(t * 2.f) * 1.5f, ir = 28.f;
        if (prem) {
            for (int i = 2; i >= 1; i--) {
                Pen rp(Color((BYTE)((30 / i) * (0.5f + 0.5f * breathe)), 220, 150, 255), 2.f + i * 3.f);
                g.DrawEllipse(&rp, icx - ir, icy - ir, ir * 2.f, ir * 2.f);
            }
        }
        GraphicsPath disc;
        disc.AddEllipse(icx - ir, icy - ir, ir * 2.f, ir * 2.f);
        LinearGradientBrush ib(RectF(icx - ir, icy - ir, ir * 2.f, ir * 2.f),
            prem ? Color(255, 190, 120, 255) : Color(255, 40, 32, 60),
            prem ? Color(255, 100, 45, 205) : Color(255, 24, 18, 40), LinearGradientModeForwardDiagonal);
        g.FillPath(&ib, &disc);
        Pen irp(prem ? Color(255, 235, 190, 255) : Color(255, 70, 54, 110), 1.5f);
        g.DrawPath(&irp, &disc);
        {
            GraphicsState s = g.Save();
            g.TranslateTransform(icx, icy);
            g.ScaleTransform(1.7f, 1.7f);
            if (prem) IconCrown(g, 0.f, 0.f, white);
            else IconLock(g, 0.f, 0.f, Color(255, 130, 108, 180));
            g.Restore(s);
        }

        Txt(g, L"ТЕКУЩИЙ ПЛАН", g_fSmall, RectF(x + 92.f, y + 14.f, 200.f, 16.f), Color(255, 150, 120, 200), StringAlignmentNear);
        Txt(g, prem ? L"PREMIUM" : L"НЕТ ПОДПИСКИ", g_fPing, RectF(x + 92.f, y + 30.f, 230.f, 34.f),
            prem ? Color(255, 235, 190, 255) : Color(255, 200, 180, 235), StringAlignmentNear);

        WCHAR line[96];
        if (prem) swprintf_s(line, L"Активна до %s", FmtDate(g_acc.subExpire).c_str());
        else swprintf_s(line, L"Выберите тариф или введите ключ");
        Txt(g, line, g_fInput, RectF(x + 92.f, y + 64.f, 250.f, 24.f),
            prem ? Color(255, 215, 200, 245) : Color(255, 130, 108, 175), StringAlignmentNear);

        if (prem) {
            WCHAR dn[16];
            swprintf_s(dn, L"%d", DaysLeftSub());
            Txt(g, dn, g_fBrand, RectF(x + w - 110.f, y + 16.f, 90.f, 40.f), Color(255, 235, 190, 255));
            Txt(g, L"ДНЕЙ ОСТАЛОСЬ", g_fSmall, RectF(x + w - 130.f, y + 58.f, 130.f, 16.f), Color(255, 130, 108, 175));
        }
        else {
            float px = x + w - 118.f, py = y + h / 2.f - 13.f;
            GraphicsPath pp;
            RoundRectPath(pp, px, py, 98.f, 26.f, 13.f);
            SolidBrush pb(Color((BYTE)(26.f + 14.f * breathe), 255, 120, 145));
            g.FillPath(&pb, &pp);
            Pen ppn(Color(110, 255, 120, 145), 1.f);
            g.DrawPath(&ppn, &pp);
            Txt(g, L"НЕ АКТИВНА", g_fSmall, RectF(px, py, 98.f, 26.f), Color(255, 255, 140, 160));
        }
    }

    {
        Reveal rv(g, 2, RectF(36.f, 208.f, 448.f, 62.f));
        float x = 40.f, y = 208.f, w = 440.f, h = 62.f;
        float pulse = 0.5f + 0.5f * sinf(t * 2.f);
        Card(g, x, y, w, h, 16.f, Color(255, 20, 14, 36), Color(255, 9, 7, 16),
            LerpC(Color(255, 52, 38, 84), Color(255, 110, 76, 190), prem ? pulse : 0.f));

        long long el = g_acc.periodStart ? NowUnix() - g_acc.periodStart : 0;
        int day = prem ? (int)std::min(30LL, el / 86400 + 1) : 0;
        int left = prem ? (int)std::max(0LL, (PERIOD_SEC - el + 86399) / 86400) : 0;

        Txt(g, L"ТРАФИК \x2022 БЕЗЛИМИТ", g_fSmall, RectF(x + 20.f, y + 8.f, 200.f, 16.f), Color(255, 150, 120, 200), StringAlignmentNear);
        WCHAR rs[48];
        if (prem) swprintf_s(rs, L"Сброс через %d дн.", left);
        else swprintf_s(rs, L"Доступен с подпиской");
        Txt(g, rs, g_fSmall, RectF(x + w - 220.f, y + 8.f, 200.f, 16.f), Color(255, 130, 108, 175), StringAlignmentFar);

        WCHAR gb[32];
        swprintf_s(gb, L"%.1f ГБ", g_trafficShown);
        Txt(g, gb, g_fPing, RectF(x + 20.f, y + 20.f, 200.f, 30.f), prem ? white : Color(255, 120, 100, 165), StringAlignmentNear);

        WCHAR dd[48];
        if (prem) swprintf_s(dd, L"День %d из 30", day);
        else swprintf_s(dd, L"");
        Txt(g, dd, g_fSmall, RectF(x + w - 220.f, y + 26.f, 200.f, 18.f), Color(255, 190, 170, 235), StringAlignmentFar);

        float bx = x + 20.f, by = y + h - 12.f, bw = w - 40.f, bh = 6.f;
        GraphicsPath tp;
        RoundRectPath(tp, bx, by, bw, bh, 3.f);
        SolidBrush tb(Color(255, 40, 30, 60));
        g.FillPath(&tb, &tp);
        float prog = prem ? Clamp01(g_periodBar) : 0.f;
        if (prog > 0.01f) {
            float fw = std::max(bh, bw * prog);
            GraphicsPath fgp;
            RoundRectPath(fgp, bx, by, fw, bh, 3.f);
            LinearGradientBrush fgb(RectF(bx, by, fw, bh), Color(255, 120, 60, 230), Color(255, 215, 125, 255), LinearGradientModeHorizontal);
            g.FillPath(&fgb, &fgp);
            g.SetClip(&fgp);
            float sp = fmodf(t * 0.5f, 1.6f) - 0.3f;
            float sx = bx + sp * fw;
            LinearGradientBrush s1(PointF(sx, 0), PointF(sx + 24.f, 0), Color(0, 255, 255, 255), Color(70, 255, 255, 255));
            LinearGradientBrush s2(PointF(sx + 24.f, 0), PointF(sx + 48.f, 0), Color(70, 255, 255, 255), Color(0, 255, 255, 255));
            g.FillRectangle(&s1, sx, by, 24.f, bh);
            g.FillRectangle(&s2, sx + 24.f, by, 24.f, bh);
            g.ResetClip();
            SolidBrush gl(Color((BYTE)(50.f + 30.f * sinf(t * 3.f)), 215, 125, 255));
            g.FillEllipse(&gl, bx + fw - 8.f, by - 3.f, 16.f, bh + 6.f);
        }
        GraphicsPath tclip;
        RoundRectPath(tclip, bx, by, bw, bh, 3.f);
        g.SetClip(&tclip);
        Pen tick(Color(110, 12, 8, 24), 1.f);
        for (int i = 1; i < 30; i++) {
            float tx = bx + bw * i / 30.f;
            g.DrawLine(&tick, tx, by, tx, by + bh);
        }
        g.ResetClip();
    }

    {
        Reveal rv(g, 3, RectF(40.f, 278.f, 200.f, 18.f));
        Txt(g, L"ТАРИФЫ", g_fSmall, RectF(40.f, 280.f, 200.f, 16.f), Color(255, 150, 120, 200), StringAlignmentNear);
    }

    for (int i = 0; i < 3; i++) {
        RECT r = PlanRect(i);
        Reveal rv(g, 3 + i, RectF((float)r.left, (float)r.top - 10.f, 138.f, 118.f));
        float hv = g_hv[H_PLAN0 + i], sel = g_planSel[i];
        float px = (float)r.left, py = (float)r.top - hv * 3.f - sel * 2.f, pw = 138.f, ph = 108.f;
        const Plan& pl = PLANS[i];

        Glow(g, px, py, pw, ph, 16.f, Color(255, 180, 110, 255), sel + hv * 0.3f);
        Card(g, px, py, pw, ph, 16.f,
            LerpC(Color(255, 20, 14, 36), Color(255, 42, 28, 76), sel),
            LerpC(Color(255, 9, 7, 16), Color(255, 22, 14, 42), sel),
            LerpC(Color(255, 52, 38, 84), Color(255, 195, 125, 255), std::max(sel, hv * 0.5f)),
            1.f + sel * 0.8f);

        {
            float rx = px + 16.f, ry = py + 16.f;
            Pen op(LerpC(Color(255, 80, 62, 125), Color(255, 195, 125, 255), sel), 1.4f);
            g.DrawEllipse(&op, rx - 7.f, ry - 7.f, 14.f, 14.f);
            if (sel > 0.02f) {
                float rr = 7.f * sel;
                SolidBrush fb(Color((BYTE)(sel * 255.f), 150, 80, 235));
                g.FillEllipse(&fb, rx - rr, ry - rr, rr * 2.f, rr * 2.f);
                IconCheck(g, rx, ry, Color((BYTE)(sel * 255.f), 255, 255, 255), 1.5f);
            }
        }

        int pct = (int)std::lround(100.0 - 100.0 * pl.price / (pl.months * (double)BASE_PRICE));
        if (pct > 0) {
            WCHAR dsc[16];
            swprintf_s(dsc, L"-%d%%", pct);
            GraphicsPath cp;
            RoundRectPath(cp, px + pw - 46.f, py + 8.f, 38.f, 16.f, 8.f);
            SolidBrush cb(Color(40, 110, 235, 160));
            g.FillPath(&cb, &cp);
            Pen cpn(Color(110, 110, 235, 160), 1.f);
            g.DrawPath(&cpn, &cp);
            Txt(g, dsc, g_fSmall, RectF(px + pw - 46.f, py + 8.f, 38.f, 16.f), Color(255, 150, 245, 190));
        }

        WCHAR nb[16], pr[32], pm[32];
        swprintf_s(nb, L"%d", pl.months);
        swprintf_s(pr, L"%d \x20BD", pl.price);
        swprintf_s(pm, L"%d \x20BD/мес", (int)std::lround((double)pl.price / pl.months));
        const WCHAR* unit = pl.months == 1 ? L"МЕСЯЦ" : (pl.months < 5 ? L"МЕСЯЦА" : L"МЕСЯЦЕВ");

        Txt(g, nb, g_fBrand, RectF(px, py + 14.f, pw, 38.f), white);
        Txt(g, unit, g_fSmall, RectF(px, py + 50.f, pw, 16.f), Color(255, 150, 120, 200));
        Txt(g, pr, g_fBtn, RectF(px, py + 66.f, pw, 24.f), LerpC(white, Color(255, 235, 190, 255), sel));
        Txt(g, pm, g_fSmall, RectF(px, py + 88.f, pw, 16.f), Color(255, 130, 108, 175));

        if (i == 2) {
            float bxp = px + pw / 2.f - 36.f, byp = py - 8.f;
            GraphicsPath bp;
            RoundRectPath(bp, bxp, byp, 72.f, 16.f, 8.f);
            LinearGradientBrush bb(RectF(bxp, byp, 72.f, 16.f), Color(255, 235, 190, 255), Color(255, 190, 120, 255), LinearGradientModeHorizontal);
            g.FillPath(&bb, &bp);
            Txt(g, L"ВЫГОДНО", g_fSmall, RectF(bxp, byp, 72.f, 16.f), Color(255, 50, 24, 90));
        }
    }

    {
        Reveal rv(g, 6, RectF(100.f, 424.f, 320.f, 54.f));
        RECT r = BuyRect();
        float hv = g_hv[H_BUY];
        float pr = (g_down == H_BUY && g_hit == H_BUY) ? 1.f : 0.f;
        float bx = (float)r.left + pr * 2.f, by = (float)r.top - hv * 1.5f + pr * 2.f;
        float bw = (float)(r.right - r.left) - pr * 4.f, bh = (float)(r.bottom - r.top) - pr * 3.f;
        GraphicsPath bp;
        RoundRectPath(bp, bx, by, bw, bh, 14.f);
        Glow(g, bx, by, bw, bh, 14.f, Color(255, 190, 100, 255), hv + 0.25f * (0.5f + 0.5f * sinf(t * 2.2f)));
        Color l1 = LerpC(Color(255, 118, 52, 214), Color(255, 150, 82, 244), hv);
        Color l2 = LerpC(Color(255, 172, 80, 248), Color(255, 214, 118, 255), hv);
        LinearGradientBrush lbb(RectF(bx, by, bw, bh), l1, l2, LinearGradientModeForwardDiagonal);
        g.FillPath(&lbb, &bp);

        g.SetClip(&bp);
        float sp = fmodf(t * 0.4f, 1.8f) - 0.4f;
        float sx = bx + sp * bw;
        LinearGradientBrush s1(PointF(sx, 0), PointF(sx + 45.f, 0), Color(0, 255, 255, 255), Color(55, 255, 255, 255));
        LinearGradientBrush s2(PointF(sx + 45.f, 0), PointF(sx + 90.f, 0), Color(55, 255, 255, 255), Color(0, 255, 255, 255));
        g.FillRectangle(&s1, sx, by, 45.f, bh);
        g.FillRectangle(&s2, sx + 45.f, by, 45.f, bh);
        g.ResetClip();

        Pen led(Color(70, 255, 255, 255), 1.f);
        g.DrawPath(&led, &bp);

        WCHAR lab[64];
        swprintf_s(lab, L"%s  \x2022  %d \x20BD", prem ? L"ПРОДЛИТЬ" : L"КУПИТЬ", (int)std::lround(g_priceShown));
        Txt(g, lab, g_fBtn, RectF(bx, by, bw, bh), white);
    }

    {
        Reveal rv(g, 7, RectF(34.f, 486.f, 452.f, 106.f));
        Txt(g, L"ИЛИ ВВЕДИТЕ КЛЮЧ АКТИВАЦИИ", g_fSmall, RectF(40.f, 490.f, 440.f, 16.f), Color(255, 150, 120, 200), StringAlignmentNear);

        float f = g_focus[2];
        float shakeX = sinf(g_keyShake * 28.f) * 8.f * g_keyShake;
        {
            GraphicsState gs = g.Save();
            g.TranslateTransform(shakeX, 0.f);
            RECT kr = KeyFieldRect();
            float kx = (float)kr.left, ky = (float)kr.top;
            float kw = (float)(kr.right - kr.left), kh = (float)(kr.bottom - kr.top);
            GraphicsPath fp;
            RoundRectPath(fp, kx, ky, kw, kh, 12.f);
            if (f > 0.02f) {
                for (int i = 2; i >= 1; i--) {
                    Pen gp2(Color((BYTE)(f * 26 / i), 160, 90, 255), 2.f + i * 3.f);
                    g.DrawPath(&gp2, &fp);
                }
            }
            SolidBrush fb(LerpC(Color(255, 17, 12, 30), Color(255, 24, 16, 44), f));
            g.FillPath(&fb, &fp);
            bool err = g_keyMsgT > 0.f && !g_keyMsgOk && g_keyShake > 0.01f;
            Color bc = err ? Color(255, 235, 80, 120) : LerpC(Color(255, 56, 42, 92), Color(255, 150, 80, 235), f);
            Pen fbd(bc, 1.f + 0.6f * f);
            g.DrawPath(&fbd, &fp);

            IconKey(g, kx + 26.f, ky + kh / 2.f, LerpC(Color(255, 110, 88, 160), Color(255, 190, 130, 255), f));

            std::wstring shown = FormatKey(g_inputKey);
            if (g_inputKey.empty())
                Txt(g, L"XXXXX-XXXXX-XXXXX-XXXXX", g_fInput, RectF(kx + 48.f + 4.f * f, ky, kw - 56.f, kh), Color(255, 100, 80, 140), StringAlignmentNear);
            else
                Txt(g, shown.c_str(), g_fInput, RectF(kx + 48.f, ky, kw - 56.f, kh), white, StringAlignmentNear);
            if (f > 0.5f && fmodf(t, 1.0f) < 0.55f) {
                float cxp = kx + 48.f + 3.f + TextW(g, shown, g_fInput);
                Pen cp2(Color(255, 220, 180, 255), 1.5f);
                g.DrawLine(&cp2, cxp, ky + 14.f, cxp, ky + kh - 14.f);
            }
            g.Restore(gs);
        }

        {
            RECT ar = ActivateRect();
            float hv = g_hv[H_ACTIVATE];
            float pr = (g_down == H_ACTIVATE && g_hit == H_ACTIVATE) ? 1.f : 0.f;
            float ax = (float)ar.left + pr * 2.f, ay = (float)ar.top + pr * 1.5f;
            float aw = (float)(ar.right - ar.left) - pr * 4.f, ah = (float)(ar.bottom - ar.top) - pr * 3.f;
            bool ready = !g_inputKey.empty();
            GraphicsPath ap;
            RoundRectPath(ap, ax, ay, aw, ah, 12.f);
            if (ready) {
                Glow(g, ax, ay, aw, ah, 12.f, Color(255, 190, 100, 255), hv);
                Color l1 = LerpC(Color(255, 118, 52, 214), Color(255, 150, 82, 244), hv);
                Color l2 = LerpC(Color(255, 172, 80, 248), Color(255, 214, 118, 255), hv);
                LinearGradientBrush ab(RectF(ax, ay, aw, ah), l1, l2, LinearGradientModeForwardDiagonal);
                g.FillPath(&ab, &ap);
                Pen led(Color(70, 255, 255, 255), 1.f);
                g.DrawPath(&led, &ap);
            }
            else {
                SolidBrush ab(Color(255, 34, 26, 54));
                g.FillPath(&ab, &ap);
            }
            Txt(g, L"АКТИВИРОВАТЬ", g_fSmall, RectF(ax, ay, aw, ah), ready ? white : Color(255, 105, 88, 140));
        }

        if (g_keyMsgT > 0.f) {
            float a = Clamp01(g_keyMsgT);
            Color mc = g_keyMsgOk ? Color(255, 130, 240, 170) : Color(255, 255, 110, 130);
            Txt(g, g_keyMsg.c_str(), g_fSmall, RectF(0.f, 568.f, (REAL)W, 20.f), WithA(mc, a));
        }
    }
}

static void PaintMenu(Graphics& g) {
    float ma = g_menuAnim;
    RECT pr = MenuPanelRect();
    float mpx = (float)pr.left, mpy = (float)pr.top;
    float mpw = (float)(pr.right - pr.left), mph = (float)(pr.bottom - pr.top);

    GraphicsState gs = g.Save();
    float pivX = mpx + mpw, pivY = mpy;
    g.TranslateTransform(pivX, pivY - (1.f - ma) * 6.f);
    float sc = 0.92f + 0.08f * ma;
    g.ScaleTransform(sc, sc);
    g.TranslateTransform(-pivX, -pivY);

    GraphicsPath mpp;
    RoundRectPath(mpp, mpx, mpy, mpw, mph, 14.f);
    LinearGradientBrush mpb(RectF(mpx, mpy, mpw, mph), Color((BYTE)(ma * 250.f), 30, 20, 50), Color((BYTE)(ma * 250.f), 16, 10, 28), LinearGradientModeVertical);
    g.FillPath(&mpb, &mpp);
    Pen mpd(Color((BYTE)(ma * 255.f), 90, 60, 150), 1.f);
    g.DrawPath(&mpd, &mpp);

    for (int i = 0; i < 2; i++) {
        RECT ir = MenuItemRect(i);
        float ix = (float)ir.left, iy = (float)ir.top;
        float iw = (float)(ir.right - ir.left), ih = (float)(ir.bottom - ir.top);
        float hv = g_hv[i == 0 ? H_MENU_SUB : H_MENU_OUT];
        bool danger = (i == 1);
        if (hv > 0.02f) {
            GraphicsPath hp;
            RoundRectPath(hp, ix, iy, iw, ih, 10.f);
            SolidBrush hb(danger ? Color((BYTE)(ma * 60.f * hv), 235, 60, 110) : Color((BYTE)(ma * 60.f * hv), 160, 100, 255));
            g.FillPath(&hb, &hp);
        }
        Color icc = danger ? Color((BYTE)(ma * 255.f), 255, 125, 150) : Color((BYTE)(ma * 255.f), 200, 160, 255);
        Color tcl = LerpC(Color((BYTE)(ma * 235.f), 225, 210, 250), Color((BYTE)(ma * 255.f), 255, 255, 255), hv);
        float nudge = hv * 2.f;
        if (i == 0) IconCrown(g, ix + 24.f + nudge, iy + ih / 2.f, icc);
        else IconLogout(g, ix + 24.f + nudge, iy + ih / 2.f, icc);
        Txt(g, i == 0 ? L"Подписка" : L"Выйти", g_fBar, RectF(ix + 46.f + nudge, iy, iw - 52.f, ih), tcl, StringAlignmentNear);
    }
    g.Restore(gs);
}

static void Paint(HDC mem) {
    Graphics g(mem);
    g.SetSmoothingMode(SmoothingModeAntiAlias);
    g.SetTextRenderingHint(TextRenderingHintAntiAlias);

    PaintBackground(g);
    PaintTitleBar(g);

    switch (g_screen) {
    case SCR_LOGIN:
    case SCR_REGISTER: PaintAuth(g); break;
    case SCR_MAIN:     PaintMain(g); break;
    case SCR_SUB:      PaintSub(g);  break;
    }

    if (g_screen == SCR_MAIN && g_menuAnim > 0.01f) PaintMenu(g);

    GraphicsPath wp;
    RoundRectPath(wp, 0.5f, 0.5f, W - 1.f, H - 1.f, 20.f);
    Pen wpn(Color(255, 44, 30, 68), 1.f);
    g.DrawPath(&wpn, &wp);
}

static void UpdateHover(HWND hwnd, POINT pt) {
    g_hit = HitTest(pt);
    if (!g_tracking) {
        TRACKMOUSEEVENT tme = { sizeof(tme), TME_LEAVE, hwnd, 0 };
        TrackMouseEvent(&tme);
        g_tracking = true;
    }
}


LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {

    case WM_CREATE: {
        g_hWnd = hwnd;
        g_ff = new FontFamily(L"Segoe UI");
        if (!g_ff->IsAvailable()) { delete g_ff; g_ff = new FontFamily(L"Arial"); }
        g_fBrand = new Font(g_ff, 32.f, FontStyleBold, UnitPixel);
        g_fBtn = new Font(g_ff, 18.f, FontStyleBold, UnitPixel);
        g_fStatus = new Font(g_ff, 14.f, FontStyleBold, UnitPixel);
        g_fTimer = new Font(g_ff, 26.f, FontStyleBold, UnitPixel);
        g_fSmall = new Font(g_ff, 11.f, FontStyleBold, UnitPixel);
        g_fBar = new Font(g_ff, 14.f, FontStyleBold, UnitPixel);
        g_fPing = new Font(g_ff, 24.f, FontStyleBold, UnitPixel);
        g_fInput = new Font(g_ff, 16.f, FontStyleRegular, UnitPixel);

        g_icoBig = MakeIcon(64);
        g_icoSmall = MakeIcon(32);
        SendMessageW(hwnd, WM_SETICON, ICON_BIG, (LPARAM)g_icoBig);
        SendMessageW(hwnd, WM_SETICON, ICON_SMALL, (LPARAM)g_icoSmall);
        g_lastTick = GetTickCount64();
        g_lastSave = g_lastTick;
        g_priceShown = (float)PLANS[1].price;
        SetLayeredWindowAttributes(hwnd, 0, 0, LWA_ALPHA);
        SetWindowRgn(hwnd, CreateRoundRectRgn(0, 0, W + 1, H + 1, 40, 40), TRUE);
        SetTimer(hwnd, 1, 16, nullptr);

        for (int i = 0; i < 34; i++) {
            Particle p;
            p.x = (float)(std::rand() % W);
            p.y = (float)(std::rand() % H);
            p.r = 0.8f + (std::rand() % 100) / 100.f * 1.6f;
            p.speed = 6.f + (std::rand() % 100) / 100.f * 16.f;
            p.phase = (std::rand() % 628) / 100.f;
            p.alpha = 0.25f + (std::rand() % 100) / 100.f * 0.75f;
            g_particles.push_back(p);
        }

        RestoreSession();
        g_scrTime = 0.f;
        break;
    }

    case WM_TIMER: {
        if (wParam == 1) {
            ULONGLONG now = GetTickCount64();
            float dt = (now - g_lastTick) / 1000.f;
            g_lastTick = now;
            if (dt > 0.05f) dt = 0.05f;
            Update(dt);
            if (IsWindow(hwnd) && !IsIconic(hwnd)) InvalidateRect(hwnd, nullptr, FALSE);
        }
        break;
    }

    case WM_CHAR: {
        if (wParam == VK_ESCAPE) {
            if (g_menuOpen) g_menuOpen = false;
            else if (g_screen == SCR_SUB) GoTo(SCR_MAIN);
            break;
        }
        if (g_screen == SCR_LOGIN || g_screen == SCR_REGISTER) {
            std::wstring* target = g_inputFocusUser ? &g_inputUser : &g_inputPass;
            size_t maxLen = g_inputFocusUser ? 20 : 32;
            if (wParam == VK_RETURN) {
                SubmitAuth();
            }
            else {
                if (wParam == VK_BACK) { if (!target->empty()) target->pop_back(); }
                else if (wParam == VK_TAB) g_inputFocusUser = !g_inputFocusUser;
                else if (wParam == 22) PasteInto(*target, maxLen);
                else if (wParam >= 32 && wParam != 127 && target->length() < maxLen) *target += (wchar_t)wParam;
                g_loginError.clear();
            }
        }
        else if (g_screen == SCR_SUB && g_keyFocus) {
            if (wParam == VK_RETURN) RedeemKey();
            else if (wParam == VK_BACK) { if (!g_inputKey.empty()) g_inputKey.pop_back(); }
            else if (wParam == 22) PasteKey();
            else AddKeyChar((wchar_t)wParam);
        }
        break;
    }

    case WM_NCHITTEST: {
        POINT pt = { GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam) };
        ScreenToClient(hwnd, &pt);
        Hit h = HitTest(pt);
        if (h == H_CLOSE || h == H_MIN || h == H_CHIP) return HTCLIENT;
        if (pt.y >= 0 && pt.y < TITLE_H) return HTCAPTION;
        return HTCLIENT;
    }

    case WM_SETCURSOR:
        if (LOWORD(lParam) == HTCLIENT && g_hit != H_NONE) {
            bool field = (g_hit == H_FIELD_USER || g_hit == H_FIELD_PASS || g_hit == H_FIELD_KEY);
            SetCursor(LoadCursor(nullptr, field ? IDC_IBEAM : IDC_HAND));
            return TRUE;
        }
        return DefWindowProcW(hwnd, msg, wParam, lParam);

    case WM_MOUSEMOVE: {
        POINT pt = { GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam) };
        UpdateHover(hwnd, pt);
        break;
    }

    case WM_NCMOUSEMOVE:
        g_hit = H_NONE;
        return DefWindowProcW(hwnd, msg, wParam, lParam);

    case WM_MOUSELEAVE:
        g_tracking = false;
        g_hit = H_NONE;
        break;

    case WM_LBUTTONDOWN: {
        if (g_closing) break;
        POINT pt = { GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam) };
        UpdateHover(hwnd, pt);

        if (g_menuOpen && g_hit != H_CHIP && g_hit != H_MENU_SUB && g_hit != H_MENU_OUT) { g_menuOpen = false; break; }

        if (g_hit == H_FIELD_USER) { g_inputFocusUser = true; break; }
        if (g_hit == H_FIELD_PASS) { g_inputFocusUser = false; break; }
        if (g_hit == H_FIELD_KEY) { g_keyFocus = true; break; }

        g_down = g_hit;
        if (g_down != H_NONE) SetCapture(hwnd);
        break;
    }

    case WM_LBUTTONUP: {
        POINT pt = { GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam) };
        UpdateHover(hwnd, pt);
        if (GetCapture() == hwnd) ReleaseCapture();
        Hit pressed = g_down;
        g_down = H_NONE;
        if (pressed != H_NONE && pressed == g_hit) Activate(pressed);
        break;
    }

    case WM_APP_CONNECTED:
        if ((int)wParam == 0) {
            g_state = ST_ON;
            g_connectedAt = GetTickCount64();
            g_lastSample = g_connectedAt;
            g_haveBase = false;
            g_downSpeed = g_upSpeed = 0.f;
            g_failStreak = 0;
            g_ping = -1;
            g_pings.clear();
            StartPing();
        }
        else {
            g_state = ST_OFF;
            ShowError((int)wParam);
        }
        g_busy = false;
        break;

    case WM_APP_UI: {
        auto* f = (std::function<void()>*)lParam;
        (*f)();
        delete f;
        break;
    }

    case WM_APP_PING:
        if (g_state == ST_ON) {
            int r = (int)wParam;
            if (r >= 0) { g_failStreak = 0; AddPing(r); }
            else { g_failStreak++; g_ping = -1; }
        }
        break;

    case WM_ACTIVATE:
        if (LOWORD(wParam) == WA_INACTIVE) g_menuOpen = false;
        return DefWindowProcW(hwnd, msg, wParam, lParam);

    case WM_CLOSE:
        StartClose();
        return 0;

    case WM_PAINT: {
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hwnd, &ps);
        RECT rc;
        GetClientRect(hwnd, &rc);

        HDC mem = CreateCompatibleDC(hdc);
        HBITMAP bmp = CreateCompatibleBitmap(hdc, rc.right, rc.bottom);
        HGDIOBJ oldBmp = SelectObject(mem, bmp);

        Paint(mem);

        BitBlt(hdc, 0, 0, rc.right, rc.bottom, mem, 0, 0, SRCCOPY);
        SelectObject(mem, oldBmp);
        DeleteObject(bmp);
        DeleteDC(mem);
        EndPaint(hwnd, &ps);
        break;
    }

    case WM_ERASEBKGND:
        return 1;

    case WM_DESTROY:
        KillTimer(hwnd, 1);
        if (g_state == ST_ON) SampleTraffic(GetTickCount64());
        SaveCurrent();
        ShutdownVpn();
        delete g_fBrand; delete g_fBtn; delete g_fStatus; delete g_fTimer;
        delete g_fSmall; delete g_fBar; delete g_fPing; delete g_fInput;
        delete g_ff;
        g_ff = nullptr;
        delete g_avatarBitmap;
        g_avatarBitmap = nullptr;
        if (g_icoBig) DestroyIcon(g_icoBig);
        if (g_icoSmall) DestroyIcon(g_icoSmall);
        PostQuitMessage(0);
        break;

    default:
        return DefWindowProcW(hwnd, msg, wParam, lParam);
    }
    return 0;
}

static bool IsElevated() {
    BOOL elevated = FALSE;
    HANDLE tok = nullptr;
    if (OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &tok)) {
        TOKEN_ELEVATION te;
        DWORD sz = sizeof(te);
        if (GetTokenInformation(tok, TokenElevation, &te, sizeof(te), &sz))
            elevated = te.TokenIsElevated;
        CloseHandle(tok);
    }
    return elevated != FALSE;
}

int WINAPI wWinMain(HINSTANCE hInst, HINSTANCE, PWSTR, int nCmdShow) {
    int argc = 0;
    LPWSTR* argv = CommandLineToArgvW(GetCommandLineW(), &argc);
    if (argv && argc == 3 && !wcscmp(argv[1], L"/service")) {
        if (LoadTunnelLib()) {
            int ret = g_tunnelProc(argv[2]);
            LocalFree(argv);
            return ret;
        }
        LocalFree(argv);
        return 1;
    }
    if (argv) LocalFree(argv);
    
    if (!IsElevated()) {
        wchar_t exe[MAX_PATH];
        GetModuleFileNameW(nullptr, exe, MAX_PATH);
        ShellExecuteW(nullptr, L"runas", exe, nullptr, nullptr, nCmdShow);
        return 0;
    }

    SetProcessDPIAware();
    std::srand((unsigned)std::time(nullptr));

    GdiplusStartupInput gsi;
    GdiplusStartup(&g_gdipToken, &gsi, nullptr);

    WNDCLASSW wc = {};
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hInst;
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wc.lpszClassName = L"NullVpnClass";
    wc.hbrBackground = nullptr;
    RegisterClassW(&wc);

    int sx = (GetSystemMetrics(SM_CXSCREEN) - W) / 2;
    int sy = (GetSystemMetrics(SM_CYSCREEN) - H) / 2;

    HWND hwnd = CreateWindowExW(
        WS_EX_LAYERED | WS_EX_APPWINDOW, L"NullVpnClass", L"NULL VPN",
        WS_POPUP | WS_MINIMIZEBOX | WS_SYSMENU,
        sx, sy, W, H,
        nullptr, nullptr, hInst, nullptr);

    ShowWindow(hwnd, nCmdShow);
    UpdateWindow(hwnd);

    MSG msg;
    while (GetMessageW(&msg, nullptr, 0, 0) > 0) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }

    GdiplusShutdown(g_gdipToken);
    return (int)msg.wParam;
}
