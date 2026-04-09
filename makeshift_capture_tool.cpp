#define _USE_MATH_DEFINES
#ifndef BOOST_ALL_NO_LIB
#define BOOST_ALL_NO_LIB
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif

#include <math.h>
#include <windows.h>
#include <windowsx.h>
#include <shellapi.h>
#include <shlobj.h>
#include <commctrl.h>
#include <objbase.h>
#include <objidl.h>
#include <gdiplus.h>
#pragma comment(lib,"gdiplus.lib")
using namespace Gdiplus;

#include <algorithm>
#include <string>
#include <vector>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
using std::min;
using std::max;
using std::string;

#include <boost/filesystem.hpp>
namespace bfs = boost::filesystem;

#define NK_INCLUDE_FIXED_TYPES
#define NK_INCLUDE_STANDARD_IO
#define NK_INCLUDE_DEFAULT_ALLOCATOR
#define NK_INCLUDE_VERTEX_BUFFER_OUTPUT
#define NK_INCLUDE_FONT_BAKING
#define NK_INCLUDE_DEFAULT_FONT
#define NK_IMPLEMENTATION
#include "nuklear.h"

/* Gamepad serial library – always present next to this .cpp */
#include "gamepad_serial.h"

#ifdef _WIN64
#pragma comment(lib, "gamepad_serial_x64.lib")
#else
#pragma comment(lib, "gamepad_serial_x86.lib")
#endif

#pragma comment(lib,"gdi32.lib")
#pragma comment(lib,"user32.lib")
#pragma comment(lib,"shell32.lib")
#pragma comment(lib,"comctl32.lib")
#pragma comment(lib,"ole32.lib")
#pragma comment(lib,"advapi32.lib")

#define APP_VERSION  "v1.0"
#define APP_NAME     "Makeshift Capture Tool with N64 Controller Support"
#define APP_NAME_SHORT "Makeshift Capture Tool with N64 Controller Support"
#define APP_CLASS    "MakeshiftCaptureToolWithN64ControllerSupportWindow"
#define IDR_LOGO     100

/* ==================================================================
   COLOUR PALETTE
   ================================================================== */
static const struct nk_color C_BG     = {10,  9,  16, 255};
static const struct nk_color C_SURF   = {18, 16,  28, 255};
static const struct nk_color C_CARD   = {26, 23,  40, 255};
static const struct nk_color C_CARD2  = {34, 30,  52, 255};
static const struct nk_color C_BORDER = {50, 44,  72, 255};
static const struct nk_color C_TEXT   = {238,234,255, 255};
static const struct nk_color C_MUTED  = {140,130,180, 255};
static const struct nk_color C_DIM    = { 80, 70,110, 255};
static const struct nk_color C_PUR    = {139, 92,246, 255};
static const struct nk_color C_PUR_L  = {167,139,250, 255};
static const struct nk_color C_PUR_D  = { 76, 29,149, 255};
static const struct nk_color C_GREEN  = { 52,211,153, 255};
static const struct nk_color C_RED    = {248, 113,113, 255};
static const struct nk_color C_AMBER  = {251,191, 36, 255};

/* ==================================================================
   FONT HELPERS
   ================================================================== */
struct nk_gdi_font {
    HFONT handle;
    struct nk_user_font nk;
};
static nk_gdi_font g_fXs, g_fSm, g_fMd, g_fTitle;

static void DrawTextUTF8(HDC hdc, const char* str, int len,
                         RECT* rc, UINT fmt, HFONT hfont)
{
    if (!str || len <= 0) return;
    int wlen = MultiByteToWideChar(CP_UTF8, 0, str, len, NULL, 0);
    std::vector<wchar_t> ws(wlen + 1, 0);
    MultiByteToWideChar(CP_UTF8, 0, str, len, ws.data(), wlen);
    HFONT old = (HFONT)SelectObject(hdc, hfont);
    DrawTextW(hdc, ws.data(), wlen, rc, fmt);
    SelectObject(hdc, old);
}

static float nk_font_width(nk_handle handle, float h, const char* str, int len)
{
    if (!str || len <= 0) return 0.f;
    nk_gdi_font* f = (nk_gdi_font*)handle.ptr;
    HDC dc = CreateCompatibleDC(NULL);
    HFONT old = (HFONT)SelectObject(dc, f->handle);
    int wlen = MultiByteToWideChar(CP_UTF8, 0, str, len, NULL, 0);
    std::vector<wchar_t> ws(wlen + 1, 0);
    MultiByteToWideChar(CP_UTF8, 0, str, len, ws.data(), wlen);
    SIZE sz = {};
    GetTextExtentPoint32W(dc, ws.data(), wlen, &sz);
    SelectObject(dc, old); DeleteDC(dc);
    return (float)sz.cx;
}

static nk_gdi_font make_font(int sz, int weight, const char* face)
{
    nk_gdi_font f = {};
    f.handle = CreateFontA(-sz, 0, 0, 0, weight, 0, 0, 0,
        DEFAULT_CHARSET, 0, 0, CLEARTYPE_QUALITY, 0, face);
    f.nk.userdata = nk_handle_ptr(&f);
    f.nk.height   = (float)sz;
    f.nk.width    = nk_font_width;
    return f;
}

static void patch_fonts()
{
    /* nothing extra needed; fonts are patched via make_font */
}

static int g_lang = 0;

/* ==================================================================
   LOCALIZATION
   ================================================================== */
#define LANG_COUNT 2
struct LangMeta { const char *code; const char *displayName; };
static const LangMeta g_langs[LANG_COUNT] = {
    { "en", "English" },
    { "it", "Italiano" },
};
enum StrID {
    /* tabs */
    S_TAB_CAPTURE, S_TAB_SHOT, S_TAB_SETTINGS, S_TAB_ABOUT, S_TAB_GAMEPAD,
    /* capture mode names */
    S_MODE_RECT, S_MODE_FULL, S_MODE_FIXED,
    /* actions */
    S_ACT_SHOT,
    /* hotkey section labels */
    S_HK_LABEL_RECT, S_HK_LABEL_FULL, S_HK_LABEL_FIXED,
    S_HK_PRESS_TO_SET, S_HK_CURRENT, S_HK_CLEAR, S_HK_NONE,
    /* settings field names */
    S_SET_OUTDIR, S_SET_SHOT_PFX, S_SET_TIMESTAMP,
    S_SET_TOP, S_SET_TRAY, S_SET_NOTIF, S_SET_CLIPBOARD, S_SET_LANG, S_SET_SAVE,
    S_FIXED_XYWH,
    /* status */
    S_STATUS, S_READY, S_CANCELLED, S_SHOT_SAVED, S_SHOT_FAIL,
    S_SHOT_COPIED,
    S_SETTINGS_SAVED,
    /* selection overlay */
    S_SEL_DRAG,
    /* tray */
    S_TRAY_OPEN, S_TRAY_EXIT,
    S_TRAY_RECT, S_TRAY_FULL, S_TRAY_FIXED,
    S_OPEN_FOLDER,
    /* about */
    S_ABOUT_DESC, S_ABOUT_GITHUB,
    S_ABOUT_TPLIBS, S_ABOUT_LICENSE,
    S_ABOUT_MADEBY, S_ABOUT_USING, S_ABOUT_AND,
    /* gamepad tab */
    S_GP_TITLE, S_GP_NOT_CONNECTED, S_GP_CONNECTED, S_GP_WAITING,
    S_GP_FAILED, S_GP_SELECT_PORT, S_GP_SCAN, S_GP_NO_PORTS,
    S_GP_TICK_RATE, S_GP_ACTIVE_HZ, S_GP_IDLE_HZ,
    S_GP_HINT_ACTIVE, S_GP_HINT_IDLE,
    S_GP_CONNECT, S_GP_RECONNECT, S_GP_DISCONNECT,
    S_GP_LIVE, S_GP_STICK, S_GP_BUTTONS,
    S_GP_AXIS, S_GP_FLIP_X, S_GP_FLIP_Y,
    S_GP_FLIP_DPAD_X, S_GP_FLIP_DPAD_Y,
    S_GP_SMOOTHING, S_GP_DEADZONE, S_GP_SPEED_MIN, S_GP_SPEED_MAX, S_GP_ACCEL,
    S_GP_SMOOTH_HINT,
    S_GP_REMAP, S_GP_OV_TITLE, S_GP_OV_HINT, S_GP_OV_PRIMARY, S_GP_OV_SECONDARY,
    S_GP_OV_FOOTER,
    S_GP_OV_CANCEL_TITLE, S_GP_OV_CANCEL_HINT,
    S_GP_CONTROLS, S_GP_CTRL_NONE,
    /* controls table row labels */
    S_GP_CTRL_1ST, S_GP_CTRL_2ND,
    S_GP_CTRL_RESET, S_GP_CTRL_EXIT,
    S_GP_CTRL_STICK, S_GP_CTRL_DPAD,
    S_GP_CTRL_1ST_VAL, S_GP_CTRL_2ND_VAL,
    S_GP_CTRL_RESET_VAL, S_GP_CTRL_EXIT_VAL,
    S_GP_CTRL_STICK_VAL, S_GP_CTRL_DPAD_VAL,
    /* overlay hint lines */
    S_OV_HINT_IDLE, S_OV_HINT_ANCHOR,
    STR_COUNT
};

static const char *LANG_TABLE[LANG_COUNT][STR_COUNT] = {
    /* ---- English ---- */
    {
        /* tabs */
        "Capture","Screenshot","Settings","About","Gamepad",
        /* modes */
        "Rectangular Capture","Full Screen","Preset Area",
        /* action */
        "Screenshot",
        /* hk labels */
        "Rectangular Capture","Full Screen","Preset Area",
        /* hk ui */
        "Press any key combination...","Current:","Clear","None",
        /* settings */
        "Output folder","File prefix","Timestamp filenames",
        "Always on top","Minimize to tray","Notifications","Copy to clipboard","Language","Save",
        /* fixed region */
        "Preset region  X, Y, Width, Height",
        /* status */
        "STATUS","Ready.","Cancelled.","Saved: ","Capture failed!",
        "Copied to clipboard.",
        "Settings saved.",
        /* sel overlay */
        "Drag to select   |   ESC to cancel",
        /* tray */
        "Open","Exit",
        "Rectangular Capture","Full Screen","Preset Area",
        /* folder */
        "Open Output Folder",
        /* about */
        "Screenshot capture tool for Windows 10/11",
        "GitHub",
        "Third-party Licenses","License",
        /* credits */
        "Made by","using","and",
        /* gamepad tab */
        "Gamepad  (Arduino N64 Controller)",
        "Not connected  -  use panel below",
        "Connected  (Arduino responding)",
        "Port open - waiting for Arduino...",
        "Connection failed  -  check port / cable",
        "Select a COM port and click Connect",
        "Scan COM ports","No COM ports found - check USB cable.",
        "TICK RATE  (polls per second)",
        "Active (1-240 Hz)","Idle   (1-60 Hz)",
        "  Active : while rect-select overlay is open (60-120 recommended)",
        "  Idle   : background poll for buttons when window is hidden",
        "Connect","Reconnect last: ","Disconnect",
        "LIVE STATE","Stick","Buttons:",
        "AXIS  (flip if crosshair/DPad moves wrong direction)",
        "Stick: Flip X","Stick: Flip Y",
        "DPad: Flip X","DPad: Flip Y",
        "STICK SMOOTHING",
        "Dead-zone","Min speed","Max speed","Accel time",
        "  Dead-zone: stick range ignored  |  Min/Max: speed px/frame  |  Accel: ramp-up time",
        "BUTTON REMAP  (global actions)",
        "OVERLAY ANCHOR / CONFIRM BUTTONS",
        "  Which buttons act as Anchor (1st) / Confirm (2nd) inside rect-select:",
        "Primary:","Secondary:",
        "  Secondary is optional.  Outside the overlay these buttons keep their normal action.",
        "CANCEL / RESET BUTTON",
        "  Which button resets selection or exits the overlay (default: B):",
        "OVERLAY CONTROLS  (inside rect-select)","(none)",
        /* controls table row labels */
        "%s  (1st press)","% s  (2nd press)",
        "Reset selection","Exit mode",
        "Left stick","DPad",
        "Set anchor corner","Confirm - capture",
        "Reset selection","Exit overlay",
        "Move crosshair  (accelerated)","Nudge corner +/-1 px",
        /* overlay hints */
        "Drag to select  |  ESC to cancel",
        "Stick=move  confirm/reset  DPad=nudge 1px",
    },
    /* ---- Italiano ---- */
    {
        /* tabs */
        "Cattura","Screenshot","Impostazioni","Info","Gamepad",
        /* modes */
        "Cattura rettangolare","Schermo intero","Area predefinita",
        /* action */
        "Screenshot",
        /* hk labels */
        "Cattura rettangolare","Schermo intero","Area predefinita",
        /* hk ui */
        "Premi una combinazione...","Attuale:","Cancella","Nessuno",
        /* settings */
        "Cartella output","Prefisso file","Timestamp nel nome",
        "Sempre in primo piano","Minimizza nel vassoio","Notifiche","Copia negli appunti","Lingua","Salva",
        /* fixed region */
        "Area predefinita  X, Y, Larghezza, Altezza",
        /* status */
        "STATO","Pronto.","Annullato.","Salvato: ","Acquisizione fallita!",
        "Copiato negli appunti.",
        "Impostazioni salvate.",
        /* sel overlay */
        "Trascina per selezionare   |   ESC per annullare",
        /* tray */
        "Apri","Esci",
        "Cattura rettangolare","Schermo intero","Area predefinita",
        /* folder */
        "Apri cartella output",
        /* about */
        "Strumento di acquisizione schermo per Windows 10/11",
        "GitHub",
        "Licenze di terze parti","Licenza",
        /* credits */
        "Creato da","usando","e",
        /* gamepad tab */
        "Gamepad  (Controller N64 su Arduino)",
        "Non connesso  -  usa il pannello sotto",
        "Connesso  (Arduino risponde)",
        "Porta aperta - in attesa di Arduino...",
        "Connessione fallita  -  controlla porta / cavo",
        "Seleziona una porta COM e premi Connetti",
        "Scansiona porte COM","Nessuna porta COM trovata - controlla il cavo USB.",
        "FREQUENZA  (sondaggi al secondo)",
        "Attiva (1-240 Hz)","Inattiva (1-60 Hz)",
        "  Attiva : mentre la selezione rettangolare e aperta (60-120 consigliato)",
        "  Inattiva : sondaggio in background quando la finestra e nascosta",
        "Connetti","Riconnetti: ","Disconnetti",
        "STATO LIVE","Stick","Pulsanti:",
        "ASSI  (inverti se il mirino/DPad si muove nella direzione sbagliata)",
        "Stick: Inverti X","Stick: Inverti Y",
        "DPad: Inverti X","DPad: Inverti Y",
        "SMOOTHING STICK",
        "Zona morta","Velocita min","Velocita max","Tempo accel.",
        "  Zona morta: range stick ignorato  |  Min/Max: velocita px/frame  |  Accel: tempo rampa",
        "RIMAPPATURA PULSANTI  (azioni globali)",
        "PULSANTI ANCORAGGIO / CONFERMA OVERLAY",
        "  Quali pulsanti agiscono come Ancoraggio (1.) / Conferma (2.) nella selezione:",
        "Principale:","Secondario:",
        "  Il secondario e opzionale. Fuori dall'overlay i pulsanti mantengono l'azione normale.",
        "PULSANTE ANNULLA / RESET",
        "  Quale pulsante azzera la selezione o esce dall'overlay (default: B):",
        "CONTROLLI OVERLAY  (nella selezione rettangolare)","(nessuno)",
        /* controls table row labels */
        "%s  (1a pressione)","%s  (2a pressione)",
        "Reset selezione","Esci modalita",
        "Stick sinistro","DPad",
        "Imposta angolo di ancoraggio","Conferma - cattura",
        "Reset selezione","Esci dall'overlay",
        "Muovi mirino  (accelerato)","Regola angolo +/-1 px",
        /* overlay hints */
        "Trascina per selezionare  |  ESC per annullare",
        "Stick=muovi  conferma/reset  DPad=regola 1px",
    },
};

#define T(id) (LANG_TABLE[g_lang][(id)])

/* ==================================================================
   HOTKEY DEF
   ================================================================== */
struct HkDef { int vk, mod; char label[64]; };

static void vk_to_str(int vk, int mod, char *out, int sz){
    if(!vk){ strncpy_s(out,sz,"",_TRUNCATE); return; }
    char b[128]={};
    if(mod&MOD_WIN)     strcat_s(b,"Win+");
    if(mod&MOD_CONTROL) strcat_s(b,"Ctrl+");
    if(mod&MOD_ALT)     strcat_s(b,"Alt+");
    if(mod&MOD_SHIFT)   strcat_s(b,"Shift+");
    struct{int v;const char*n;}named[]={
        {VK_F1,"F1"},{VK_F2,"F2"},{VK_F3,"F3"},{VK_F4,"F4"},
        {VK_F5,"F5"},{VK_F6,"F6"},{VK_F7,"F7"},{VK_F8,"F8"},
        {VK_F9,"F9"},{VK_F10,"F10"},{VK_F11,"F11"},{VK_F12,"F12"},
        {VK_F13,"F13"},{VK_F14,"F14"},{VK_F15,"F15"},{VK_F16,"F16"},
        {VK_F17,"F17"},{VK_F18,"F18"},{VK_F19,"F19"},{VK_F20,"F20"},
        {VK_F21,"F21"},{VK_F22,"F22"},{VK_F23,"F23"},{VK_F24,"F24"},
        {VK_NUMPAD0,"Num0"},{VK_NUMPAD1,"Num1"},{VK_NUMPAD2,"Num2"},
        {VK_NUMPAD3,"Num3"},{VK_NUMPAD4,"Num4"},{VK_NUMPAD5,"Num5"},
        {VK_NUMPAD6,"Num6"},{VK_NUMPAD7,"Num7"},{VK_NUMPAD8,"Num8"},
        {VK_NUMPAD9,"Num9"},{VK_MULTIPLY,"Num*"},{VK_ADD,"Num+"},
        {VK_SUBTRACT,"Num-"},{VK_DECIMAL,"Num."},{VK_DIVIDE,"Num/"},
        {VK_PRIOR,"PgUp"},{VK_NEXT,"PgDn"},{VK_HOME,"Home"},{VK_END,"End"},
        {VK_INSERT,"Ins"},{VK_DELETE,"Del"},{VK_RETURN,"Enter"},
        {VK_SPACE,"Space"},{VK_TAB,"Tab"},{VK_ESCAPE,"Esc"},
        {VK_PAUSE,"Pause"},{VK_SCROLL,"ScrollLock"},{VK_SNAPSHOT,"PrtSc"},
        {VK_LEFT,"Left"},{VK_RIGHT,"Right"},{VK_UP,"Up"},{VK_DOWN,"Down"},
        {VK_XBUTTON1,"Mouse4"},{VK_XBUTTON2,"Mouse5"},
        {VK_VOLUME_UP,"Vol+"},{VK_VOLUME_DOWN,"Vol-"},{VK_VOLUME_MUTE,"Mute"},
        {VK_MEDIA_NEXT_TRACK,"NextTrack"},{VK_MEDIA_PREV_TRACK,"PrevTrack"},
        {VK_MEDIA_PLAY_PAUSE,"PlayPause"},
        {0,NULL}
    };
    bool found=false;
    for(int i=0;named[i].n;i++) if(named[i].v==vk){strcat_s(b,named[i].n);found=true;break;}
    if(!found){
        if(vk>='A'&&vk<='Z'){char t[4]={(char)vk,0};strcat_s(b,t);}
        else if(vk>='0'&&vk<='9'){char t[4]={(char)vk,0};strcat_s(b,t);}
        else{char t[12];sprintf_s(t,"VK_%02X",vk);strcat_s(b,t);}
    }
    strncpy_s(out,sz,b,_TRUNCATE);
}

/* ==================================================================
   SETTINGS
   ================================================================== */
#define HK_RECT  0
#define HK_FULL  1
#define HK_FIXED 2
#define HK_COUNT 3

/* N64 button names in the order we store remap */
#define GP_BTN_COUNT 14
static const char* GP_BTN_NAMES[GP_BTN_COUNT] = {
    "A","B","Z","Start","DUp","DDown","DLeft","DRight",
    "L","R","CUp","CDown","CLeft","CRight"
};
/* Default actions for each physical button (indices into GpAction enum) */
enum GpAction {
    GPA_NONE=0,
    GPA_RECT_CAPTURE,      /* open rect-select overlay         */
    GPA_FULL_CAPTURE,      /* full-screen shot                 */
    GPA_FIXED_CAPTURE,     /* preset-area shot                 */
    GPA_TOGGLE_WIN,        /* toggle window/tray               */
    GPA_OPEN_FOLDER,       /* open captures folder             */
    /* overlay-only (only meaningful when rect-select is active) */
    GPA_OV_ANCHOR_CONFIRM, /* first press = anchor, second = capture */
    GPA_COUNT
};
static const char* GPA_NAMES[GPA_COUNT]={
    "None","Rect Capture","Full Screen","Preset Area",
    "Toggle Window","Open Folder","Overlay: Anchor/Capture"
};

/* Two additional slots: which physical buttons act as anchor/confirm in the overlay.
   Defaults: A (idx 0) and Z (idx 2).  Any button can be assigned here independently
   of its normal action mapping — the overlay takes priority when active. */
#define GP_OV_SLOTS 2   /* slot 0 = primary, slot 1 = secondary (optional) */

struct AppSettings {
    HkDef hk[HK_COUNT];
    int   shotFx,shotFy,shotFw,shotFh;
    char  outputDir[MAX_PATH];
    char  shotPfx[64];
    int   useTimestamp;
    int   alwaysOnTop, minimizeToTray, enableNotif, copyToClipboard;
    int   winW, winH, winX, winY;   /* -1/-1 w/h = maximized */
    int   winMaximized;
    int   language;
    /* gamepad */
    char  gpPort[256];
    int   gpHz;
    int   gpIdleHz;
    int   gpFlipX;          /* invert stick X axis */
    int   gpFlipY;          /* invert stick Y axis */
    int   gpDpadFlipX;      /* invert DPad X direction in overlay */
    int   gpDpadFlipY;      /* invert DPad Y direction in overlay */
    int   gpBtnAction[GP_BTN_COUNT]; /* GpAction per physical button */
    int   gpOvBtn[GP_OV_SLOTS];     /* anchor/confirm buttons (-1=none) */
    int   gpOvBtnCancel;    /* cancel/reset button index (-1=none → default B=idx 1) */
    /* stick smoothing */
    float gpDeadzone;       /* magnitude dead-zone (0..40) */
    float gpSpeedMin;       /* px/frame at dead-zone edge */
    float gpSpeedMax;       /* px/frame at full deflection+hold */
    float gpAccelMs;        /* ms to reach full speed */
};
static AppSettings g_cfg;

static void get_exe_dir(char *b,size_t s){
    GetModuleFileNameA(NULL,b,(DWORD)s);
    for(int i=(int)strlen(b)-1;i>=0;--i)if(b[i]=='\\'||b[i]=='/'){b[i]=0;break;}
}
static string ini_path(){char d[MAX_PATH];get_exe_dir(d,sizeof(d));return string(d)+"\\makeshift_capture_tool_settings.ini";}
static string def_outdir(){char d[MAX_PATH];get_exe_dir(d,sizeof(d));return string(d)+"\\Captures";}

static void settings_defaults(AppSettings &s){
    memset(s.hk,0,sizeof(s.hk));
    s.hk[HK_RECT].vk=VK_F8;
    vk_to_str(VK_F8,0,s.hk[HK_RECT].label,sizeof(s.hk[HK_RECT].label));
    s.shotFx=0;s.shotFy=0;s.shotFw=800;s.shotFh=600;
    strcpy_s(s.outputDir,def_outdir().c_str());
    strcpy_s(s.shotPfx,"screenshot");
    s.useTimestamp=1;s.alwaysOnTop=0;s.minimizeToTray=1;s.enableNotif=1;s.copyToClipboard=0;
    s.winW=620;s.winH=580;s.winX=CW_USEDEFAULT;s.winY=CW_USEDEFAULT;
    s.winMaximized=0;
    s.language=0;
    s.gpPort[0]=0;s.gpHz=120;s.gpIdleHz=10;
    s.gpFlipX=0;s.gpFlipY=1; /* stick Y flipped by default */
    s.gpDpadFlipX=0;s.gpDpadFlipY=0; /* DPad not flipped by default */
    /* Default button actions */
    for(int i=0;i<GP_BTN_COUNT;i++) s.gpBtnAction[i]=GPA_NONE;
    s.gpBtnAction[2]=GPA_FULL_CAPTURE;  /* Z → full screen */
    s.gpBtnAction[3]=GPA_TOGGLE_WIN;    /* Start → toggle */
    s.gpBtnAction[8]=GPA_FIXED_CAPTURE; /* L → preset */
    s.gpBtnAction[9]=GPA_RECT_CAPTURE;  /* R → rect */
    s.gpBtnAction[11]=GPA_OPEN_FOLDER;  /* CDown (idx 11) → open folder */
    /* Overlay anchor/confirm: A (slot0=primary) and Z (slot1=secondary) */
    s.gpOvBtn[0]=0;   /* index 0 = "A" in GP_BTN_NAMES */
    s.gpOvBtn[1]=2;   /* index 2 = "Z" in GP_BTN_NAMES */
    s.gpOvBtnCancel=1; /* index 1 = "B" in GP_BTN_NAMES */
    /* stick */
    s.gpDeadzone=10.f;s.gpSpeedMin=0.5f;s.gpSpeedMax=40.f;s.gpAccelMs=800.f;
}
static void ini_load(AppSettings &s){
    settings_defaults(s);
    FILE *f=nullptr; if(fopen_s(&f,ini_path().c_str(),"r")||!f)return;
    char line[1024],sec[64]="";
    while(fgets(line,sizeof(line),f)){
        int L=(int)strlen(line);
        while(L>0&&(line[L-1]=='\r'||line[L-1]=='\n'))line[--L]=0;
        if(!L||line[0]==';')continue;
        if(line[0]=='['){char*e=strchr(line+1,']');if(e){*e=0;strcpy_s(sec,line+1);}continue;}
        char*eq=strchr(line,'=');if(!eq)continue;
        *eq=0;const char*val=eq+1;char k[128];strcpy_s(k,line);
        int kl=(int)strlen(k);while(kl>0&&k[kl-1]==' ')k[--kl]=0;
        while(*val==' ')++val;
#define _I(sc,ky,fld) if(!strcmp(sec,sc)&&!strcmp(k,ky))s.fld=atoi(val);
#define _F(sc,ky,fld) if(!strcmp(sec,sc)&&!strcmp(k,ky))s.fld=(float)atof(val);
#define _S(sc,ky,buf) if(!strcmp(sec,sc)&&!strcmp(k,ky))strcpy_s(s.buf,val);
        for(int i=0;i<HK_COUNT;i++){
            char kv[16],km[16];sprintf_s(kv,"hk%dvk",i);sprintf_s(km,"hk%dmod",i);
            if(!strcmp(sec,"hk")&&!strcmp(k,kv))s.hk[i].vk=atoi(val);
            if(!strcmp(sec,"hk")&&!strcmp(k,km))s.hk[i].mod=atoi(val);
        }
        for(int i=0;i<GP_BTN_COUNT;i++){
            char kb[16];sprintf_s(kb,"btn%d",i);
            if(!strcmp(sec,"gp")&&!strcmp(k,kb))s.gpBtnAction[i]=atoi(val);
        }
        for(int i=0;i<GP_OV_SLOTS;i++){
            char kb[16];sprintf_s(kb,"ovbtn%d",i);
            if(!strcmp(sec,"gp")&&!strcmp(k,kb))s.gpOvBtn[i]=atoi(val);
        }
        _I("shot","fx",shotFx)_I("shot","fy",shotFy)_I("shot","fw",shotFw)_I("shot","fh",shotFh)
        _S("out","dir",outputDir)_S("out","sp",shotPfx)_I("out","ts",useTimestamp)
        _I("ui","top",alwaysOnTop)_I("ui","tray",minimizeToTray)
        _I("ui","notif",enableNotif)_I("ui","clip",copyToClipboard)_I("ui","w",winW)_I("ui","h",winH)
        _I("ui","x",winX)_I("ui","y",winY)_I("ui","max",winMaximized)
        _I("ui","lang",language)
        _S("gp","port",gpPort)_I("gp","hz",gpHz)_I("gp","idlehz",gpIdleHz)
        _I("gp","flipx",gpFlipX)_I("gp","flipy",gpFlipY)
        _I("gp","dpadflipx",gpDpadFlipX)_I("gp","dpadflipy",gpDpadFlipY)
        _I("gp","ovcancelBtn",gpOvBtnCancel)
        _F("gp","dz",gpDeadzone)_F("gp","spmin",gpSpeedMin)
        _F("gp","spmax",gpSpeedMax)_F("gp","accel",gpAccelMs)
#undef _I
#undef _F
#undef _S
    }
    fclose(f);
    for(int i=0;i<HK_COUNT;i++)
        vk_to_str(s.hk[i].vk,s.hk[i].mod,s.hk[i].label,sizeof(s.hk[i].label));
}
static void ini_save(const AppSettings &s){
    FILE *f=nullptr;if(fopen_s(&f,ini_path().c_str(),"w")||!f)return;
    fprintf(f,"[hk]\n");
    for(int i=0;i<HK_COUNT;i++)fprintf(f,"hk%dvk=%d\nhk%dmod=%d\n",i,s.hk[i].vk,i,s.hk[i].mod);
    fprintf(f,"[shot]\nfx=%d\nfy=%d\nfw=%d\nfh=%d\n",s.shotFx,s.shotFy,s.shotFw,s.shotFh);
    fprintf(f,"[out]\ndir=%s\nsp=%s\nts=%d\n",s.outputDir,s.shotPfx,s.useTimestamp);
    fprintf(f,"[ui]\ntop=%d\ntray=%d\nnotif=%d\nclip=%d\nw=%d\nh=%d\nx=%d\ny=%d\nmax=%d\nlang=%d\n",
        s.alwaysOnTop,s.minimizeToTray,s.enableNotif,s.copyToClipboard,
        s.winW,s.winH,s.winX,s.winY,s.winMaximized,s.language);
    fprintf(f,"[gp]\nport=%s\nhz=%d\nidlehz=%d\nflipx=%d\nflipy=%d\ndpadflipx=%d\ndpadflipy=%d\novcancelBtn=%d\n",
        s.gpPort,s.gpHz,s.gpIdleHz,s.gpFlipX,s.gpFlipY,s.gpDpadFlipX,s.gpDpadFlipY,s.gpOvBtnCancel);
    fprintf(f,"dz=%.2f\nspmin=%.2f\nspmax=%.2f\naccel=%.0f\n",
        s.gpDeadzone,s.gpSpeedMin,s.gpSpeedMax,s.gpAccelMs);
    for(int i=0;i<GP_BTN_COUNT;i++) fprintf(f,"btn%d=%d\n",i,s.gpBtnAction[i]);
    for(int i=0;i<GP_OV_SLOTS;i++)  fprintf(f,"ovbtn%d=%d\n",i,s.gpOvBtn[i]);
    fclose(f);
}

/* ==================================================================
   HOTKEY REGISTRATION
   ================================================================== */
#define HK_BASE 0xC001
static HWND g_hwnd=NULL;

static void register_hotkeys(){
    for(int i=0;i<HK_COUNT;i++){
        UnregisterHotKey(g_hwnd,HK_BASE+i);
        if(g_cfg.hk[i].vk)
            RegisterHotKey(g_hwnd,HK_BASE+i,g_cfg.hk[i].mod,g_cfg.hk[i].vk);
    }
}

/* ==================================================================
   LOGO
   ================================================================== */
static HBITMAP g_logoBmp=NULL;
static int     g_logoW=0, g_logoH=0;
struct BlitReq{bool valid;int x,y,w,h;HBITMAP hbm;int sw,sh;};
static BlitReq g_logoBlit={};

static void load_logo(){
    HRSRC res=FindResourceA(NULL,MAKEINTRESOURCEA(IDR_LOGO),"PNG");if(!res)return;
    HGLOBAL hg=LoadResource(NULL,res);if(!hg)return;
    DWORD sz=SizeofResource(NULL,res);void*data=LockResource(hg);if(!data||!sz)return;
    HGLOBAL hmem=GlobalAlloc(GMEM_MOVEABLE,sz);
    void*p=GlobalLock(hmem);memcpy(p,data,sz);GlobalUnlock(hmem);
    IStream*stream=NULL;CreateStreamOnHGlobal(hmem,TRUE,&stream);
    if(stream){
        Bitmap*bmp=Bitmap::FromStream(stream);
        if(bmp&&bmp->GetLastStatus()==Ok){
            g_logoW=(int)bmp->GetWidth();g_logoH=(int)bmp->GetHeight();
            bmp->GetHBITMAP(Color(0,0,0,0),&g_logoBmp);
        }
        delete bmp;stream->Release();
    }
}
static HICON make_icon(){
    /* First: try the proper multi-size icon embedded via resource.rc (IDI_APPICON).
       This is what appears in Explorer, taskbar, alt-tab, and the title bar. */
    HICON ico = LoadIconA(GetModuleHandle(NULL), MAKEINTRESOURCEA(1)); /* IDI_APPICON = 1 */
    if (ico) return ico;

    /* Second: build a 16x16 icon from the PNG resource if available */
    if (g_logoBmp) {
        HDC s = GetDC(NULL);
        HDC sdc = CreateCompatibleDC(s);
        HDC ddc = CreateCompatibleDC(s);
        HBITMAP d16 = CreateCompatibleBitmap(s, 16, 16);
        SelectObject(sdc, g_logoBmp);
        SelectObject(ddc, d16);
        SetStretchBltMode(ddc, HALFTONE);
        StretchBlt(ddc, 0, 0, 16, 16, sdc, 0, 0, g_logoW, g_logoH, SRCCOPY);
        DeleteDC(sdc);
        DeleteDC(ddc);
        ReleaseDC(NULL, s);
        ICONINFO ii = { TRUE, 0, 0, d16, d16 };
        ico = CreateIconIndirect(&ii);
        DeleteObject(d16);
        if (ico) return ico;
    }

    /* Fallback: draw a minimal camera icon in purple */
    HDC dc = CreateCompatibleDC(NULL);
    HBITMAP bm = CreateBitmap(16, 16, 1, 32, NULL);
    HBITMAP old = (HBITMAP)SelectObject(dc, bm);
    RECT r = { 0, 0, 16, 16 };
    HBRUSH bg = CreateSolidBrush(RGB(10, 9, 16));
    FillRect(dc, &r, bg);
    DeleteObject(bg);
    HBRUSH ac = CreateSolidBrush(RGB(139, 92, 246));
    RECT body = { 2, 5, 14, 12 };
    FillRect(dc, &body, ac);
    RECT lens = { 5, 7, 10, 11 };
    HBRUSH wh = CreateSolidBrush(RGB(238, 234, 255));
    FillRect(dc, &lens, wh);
    DeleteObject(wh);
    RECT pupil = { 6, 8, 9, 10 };
    FillRect(dc, &pupil, ac);
    RECT hump = { 5, 3, 9, 5 };
    FillRect(dc, &hump, ac);
    DeleteObject(ac);
    SelectObject(dc, old);
    DeleteDC(dc);
    ICONINFO ii = { TRUE, 0, 0, bm, bm };
    ico = CreateIconIndirect(&ii);
    DeleteObject(bm);
    return ico ? ico : LoadIcon(NULL, IDI_APPLICATION);
}

/* ==================================================================
   TRAY
   ================================================================== */
#define WM_TRAYICON  (WM_USER+1)
#define ID_TRAY_OPEN      5001
#define ID_TRAY_RECT      5002
#define ID_TRAY_FULL      5003
#define ID_TRAY_FIXED     5004
#define ID_TRAY_EXIT      5099

static NOTIFYICONDATA g_nid={};
static bool g_trayVis=false;

static void tray_add(){
    if(g_trayVis)return;
    g_nid.cbSize=sizeof(g_nid);g_nid.hWnd=g_hwnd;g_nid.uID=1;
    g_nid.uFlags=NIF_ICON|NIF_TIP|NIF_MESSAGE;g_nid.uCallbackMessage=WM_TRAYICON;
    g_nid.hIcon=make_icon();strcpy_s(g_nid.szTip,APP_NAME_SHORT);
    Shell_NotifyIconA(NIM_ADD,&g_nid);g_trayVis=true;
}
static void tray_remove(){if(!g_trayVis)return;Shell_NotifyIconA(NIM_DELETE,&g_nid);g_trayVis=false;}
static void show_toast(const char*title,const char*msg){
    if(!g_cfg.enableNotif||!g_trayVis)return;
    NOTIFYICONDATA n=g_nid;n.uFlags|=NIF_INFO;n.dwInfoFlags=NIIF_INFO;n.uTimeout=3000;
    strcpy_s(n.szInfoTitle,title);strcpy_s(n.szInfo,msg);Shell_NotifyIconA(NIM_MODIFY,&n);
}
static void tray_menu(){
    HMENU m=CreatePopupMenu();
    AppendMenuA(m,MF_STRING,ID_TRAY_OPEN, T(S_TRAY_OPEN));
    AppendMenuA(m,MF_SEPARATOR,0,NULL);
    AppendMenuA(m,MF_STRING,ID_TRAY_RECT, T(S_TRAY_RECT));
    AppendMenuA(m,MF_STRING,ID_TRAY_FULL, T(S_TRAY_FULL));
    AppendMenuA(m,MF_STRING,ID_TRAY_FIXED,T(S_TRAY_FIXED));
    AppendMenuA(m,MF_SEPARATOR,0,NULL);
    AppendMenuA(m,MF_STRING,ID_TRAY_EXIT, T(S_TRAY_EXIT));
    POINT pt;GetCursorPos(&pt);SetForegroundWindow(g_hwnd);
    TrackPopupMenu(m,TPM_BOTTOMALIGN|TPM_RIGHTBUTTON,pt.x,pt.y,0,g_hwnd,NULL);
    DestroyMenu(m);
}

/* ==================================================================
   WINDOW PLACEMENT  – stored in RAM so screenshot can restore exactly
   ================================================================== */
static WINDOWPLACEMENT g_savedPlacement={sizeof(WINDOWPLACEMENT)};
static bool            g_placementValid=false;

/* Call once on startup and on every WM_SIZE/WM_MOVE to keep RAM in sync */
static void wnd_save_placement(){
    if(!g_hwnd) return;
    WINDOWPLACEMENT wpl={sizeof(wpl)};
    if(!GetWindowPlacement(g_hwnd,&wpl)) return;
    g_savedPlacement=wpl;
    g_placementValid=true;
    /* Keep cfg in sync (for INI persistence on close/destroy) */
    bool isMax=(wpl.showCmd==SW_SHOWMAXIMIZED);
    g_cfg.winMaximized=isMax?1:0;
    if(!isMax){
        RECT r=wpl.rcNormalPosition;
        g_cfg.winW=r.right-r.left;
        g_cfg.winH=r.bottom-r.top;
        g_cfg.winX=r.left;
        g_cfg.winY=r.top;
    }
}

/* ==================================================================
   WINDOW VISIBILITY / SCREENSHOT HIDE-RESTORE
   ================================================================== */
static bool g_windowVisible=true;

/* Was the window visible (not hidden/tray) before this capture started?
   Set in hide_for_capture(), consumed in do_screenshot(). */
static bool g_wasVisibleBeforeCapture=false;

static void hide_for_capture(){
    g_wasVisibleBeforeCapture = g_windowVisible; /* remember state */
    wnd_save_placement();          /* snapshot before hiding */
    ShowWindow(g_hwnd,SW_HIDE);
    g_windowVisible=false;
    typedef HRESULT(WINAPI*PFN)();
    static PFN pfn=NULL;
    if(!pfn){HMODULE h=LoadLibraryA("dwmapi.dll");if(h)pfn=(PFN)GetProcAddress(h,"DwmFlush");}
    if(pfn)pfn();
    Sleep(200);
}
static void show_after_capture(){
    if(g_placementValid){
        /* Restore to exact pre-capture placement */
        SetWindowPlacement(g_hwnd,&g_savedPlacement);
        if(g_savedPlacement.showCmd==SW_SHOWMAXIMIZED)
            ShowWindow(g_hwnd,SW_SHOWMAXIMIZED);
        else
            ShowWindow(g_hwnd,SW_SHOWNOACTIVATE);
    } else {
        ShowWindow(g_hwnd,SW_SHOWNOACTIVATE);
    }
    g_windowVisible=true;
}
static void show_restore_from_tray(){
    if(g_placementValid){
        SetWindowPlacement(g_hwnd,&g_savedPlacement);
        if(g_savedPlacement.showCmd==SW_SHOWMAXIMIZED)
            ShowWindow(g_hwnd,SW_SHOWMAXIMIZED);
        else
            ShowWindow(g_hwnd,SW_RESTORE);
    } else {
        ShowWindow(g_hwnd,SW_RESTORE);
    }
    SetForegroundWindow(g_hwnd);
    g_windowVisible=true;
    wnd_save_placement();
}

/* ==================================================================
   GAMEPAD STATE  (thread-safe snapshot updated by background thread)
   ================================================================== */
static CRITICAL_SECTION g_gpCS;
static GamepadState      g_gpState={};
static volatile LONG     g_gpRunning=0;
static HANDLE            g_gpThread=NULL;

/* Button edge-detection: store previous pressed set */
#define MAX_BTN 16
struct GpEdge {
    char  prev[MAX_BTN][16];
    int   prevCount;
};
static GpEdge g_gpEdge={};

static bool btn_in_set(const char (*set)[16], int cnt, const char *name){
    for(int i=0;i<cnt;i++) if(!strcmp(set[i],name)) return true;
    return false;
}

/* Returns true if button transitioned from not-pressed -> pressed this frame */
static bool gp_just_pressed(const GamepadState &st, const char *name){
    bool wasDown = btn_in_set(g_gpEdge.prev, g_gpEdge.prevCount, name);
    bool isDown  = btn_in_set(st.pressed,    st.pressed_count,   name);
    return isDown && !wasDown;
}
static bool gp_pressed(const GamepadState &st, const char *name){
    return btn_in_set(st.pressed, st.pressed_count, name);
}

/* Update edge-detection after consuming a frame */
static void gp_edge_update(const GamepadState &st){
    g_gpEdge.prevCount = st.pressed_count;
    for(int i=0;i<st.pressed_count;i++)
        strcpy_s(g_gpEdge.prev[i], st.pressed[i]);
}

/* Custom polling thread so the gamepad is read even when window is hidden */
static DWORD WINAPI gp_poll_thread(LPVOID){
    GamepadState local={};
    while(g_gpRunning){
        gamepad_update(&local);
        EnterCriticalSection(&g_gpCS);
        g_gpState=local;
        LeaveCriticalSection(&g_gpCS);
        Sleep(16);   /* ~60 Hz mirror */
    }
    return 0;
}

static void gp_thread_start(){
    if(g_gpThread) return;
    InitializeCriticalSection(&g_gpCS);
    g_gpRunning=1;
    g_gpThread=CreateThread(NULL,0,gp_poll_thread,NULL,0,NULL);
}
static void gp_thread_stop(){
    g_gpRunning=0;
    if(g_gpThread){
        WaitForSingleObject(g_gpThread,2000);
        CloseHandle(g_gpThread);
        g_gpThread=NULL;
    }
    DeleteCriticalSection(&g_gpCS);
}

/* Get a safe snapshot */
static GamepadState gp_snapshot(){
    GamepadState s={};
    EnterCriticalSection(&g_gpCS);
    s=g_gpState;
    LeaveCriticalSection(&g_gpCS);
    return s;
}

/* ==================================================================
   GAMEPAD SETUP UI STATE
   ================================================================== */
enum GpSetupStep { GPS_IDLE=0, GPS_SELECT_PORT, GPS_CONNECTED, GPS_FAILED };
static GpSetupStep g_gpSetup=GPS_IDLE;
static char**      g_gpPorts=NULL;
static int         g_gpPortCount=0;
static int         g_gpPortSel=0;

static void gp_free_ports(){
    if(!g_gpPorts) return;
    for(int i=0;i<g_gpPortCount;i++) if(g_gpPorts[i]) free(g_gpPorts[i]);
    free(g_gpPorts); g_gpPorts=NULL; g_gpPortCount=0;
}
static void gp_try_connect(const char *port, int hz){
    GamepadConfig cfg={};
    snprintf(cfg.port,sizeof(cfg.port),"%s",port);
    cfg.hz=hz;
    if(gamepad_init(&cfg)){
        gp_thread_start();
        g_gpSetup=GPS_CONNECTED;
        strcpy_s(g_cfg.gpPort,port);
        g_cfg.gpHz=hz;
        ini_save(g_cfg);
    } else {
        g_gpSetup=GPS_FAILED;
    }
}

/* ==================================================================
   IN-APP VIRTUAL CURSOR  (crosshair driven by gamepad stick)
   ================================================================== */
struct VCursor {
    float x, y;           /* screen coords */
    bool  visible;
    /* velocity accumulator for smooth stick movement */
    float vx, vy;
    UINT64 deflectStart;  /* tick when stick left dead-zone */
    bool  inDeadZone;
};
static VCursor g_vc={};

/* ------------------------------------------------------------------
   Stick constants.
   Arduino encode_stick encodes the axis as:
     bit7 = direction (1=negative), bits6-0 = magnitude 0..126
   gamepad_serial.h decode_stick reconstructs the signed value.
   Max magnitude is 126 (Arduino caps at ±126).
   ------------------------------------------------------------------ */
#define VC_STICK_MAX   126.0f  /* Arduino caps stick at ±126 */

static void vc_init(){
    POINT p; GetCursorPos(&p);
    g_vc.x=(float)p.x; g_vc.y=(float)p.y;
    g_vc.visible=false;
    g_vc.vx=g_vc.vy=0.f;
    g_vc.inDeadZone=true;
}

/* Call every frame with decoded signed stick values (-126..126).
   Reads smoothing params live from g_cfg so changes take effect immediately.
   Applies gpFlipX / gpFlipY before any processing.
   Speed model:
     base_speed  = lerp(gpSpeedMin, gpSpeedMax, deflection²)
     time_factor = smoothstep(0, gpAccelMs, elapsed_ms)
     final_speed = base_speed × (0.15 + 0.85 × time_factor)
   15% floor ensures instant tiny response on frame 1.
*/
static void vc_update_stick(float sx, float sy){
    /* Apply axis flips from settings */
    if(g_cfg.gpFlipX) sx=-sx;
    if(g_cfg.gpFlipY) sy=-sy;

    float dz  = max(1.f, g_cfg.gpDeadzone);
    float spMn= max(0.f, g_cfg.gpSpeedMin);
    float spMx= max(spMn+0.1f, g_cfg.gpSpeedMax);
    float acMs= max(50.f, g_cfg.gpAccelMs);

    float mag=sqrtf(sx*sx+sy*sy);
    UINT64 now=GetTickCount64();

    if(mag<dz){
        g_vc.inDeadZone=true;
        g_vc.vx=g_vc.vy=0.f;
        return;
    }

    float nx=sx/mag, ny=sy/mag;
    float ratio=mag/VC_STICK_MAX;
    if(ratio>1.f) ratio=1.f;

    if(g_vc.inDeadZone){
        g_vc.inDeadZone=false;
        g_vc.deflectStart=now;
        g_vc.vx=g_vc.vy=0.f;
    }

    float t=(float)(now-g_vc.deflectStart)/acMs;
    if(t>1.f)t=1.f;
    float time_factor=t*t*(3.f-2.f*t);   /* smoothstep */

    float base_speed=spMn+(spMx-spMn)*ratio*ratio;
    float speed=base_speed*(0.15f+0.85f*time_factor);

    g_vc.vx=nx*speed;
    g_vc.vy=ny*speed;
}

/* Call every frame to apply accumulated velocity */
static void vc_apply(){
    g_vc.x+=g_vc.vx;
    g_vc.y+=g_vc.vy;
    /* clamp to virtual screen */
    float sx0=(float)GetSystemMetrics(SM_XVIRTUALSCREEN);
    float sy0=(float)GetSystemMetrics(SM_YVIRTUALSCREEN);
    float sw =(float)GetSystemMetrics(SM_CXVIRTUALSCREEN);
    float sh =(float)GetSystemMetrics(SM_CYVIRTUALSCREEN);
    if(g_vc.x<sx0)g_vc.x=sx0;
    if(g_vc.y<sy0)g_vc.y=sy0;
    if(g_vc.x>sx0+sw-1)g_vc.x=sx0+sw-1.f;
    if(g_vc.y>sy0+sh-1)g_vc.y=sy0+sh-1.f;
}

/* Snap cursor to actual mouse position */
static void vc_snap_to_mouse(){
    POINT p; GetCursorPos(&p);
    g_vc.x=(float)p.x; g_vc.y=(float)p.y;
}

/* ==================================================================
   RECT SELECTION OVERLAY  (extended with gamepad support)

   Mouse: left-button drag exactly as before (hold=draw, release=capture)
   Gamepad inside overlay:
     A (first press)  → anchor point (begin area)
     A (second press) → confirm and capture
     B (while dragging) → reset/restart selection
     B (idle, no selection started) → cancel / exit mode
     DPad up/down/left/right → nudge second point ±1 px
     Left stick → move crosshair (smooth accelerated)
   ================================================================== */

/* Extended selection state */
struct SelState {
    POINT a, b;
    bool drag;         /* mouse is being held */
    bool done;
    bool cancelled;
    /* gamepad sub-state */
    bool gpAnchorSet;  /* A was pressed once, anchor locked */
};
static SelState g_sel={};

/* The overlay window handle; NULL when not open */
static HWND g_selWnd=NULL;

/* Cross-hair render helper */
static void draw_crosshair(HDC hdc, int cx, int cy, COLORREF col){
    HPEN p=CreatePen(PS_SOLID,1,col);
    HPEN old=(HPEN)SelectObject(hdc,p);
    int arm=12;
    /* horizontal */
    MoveToEx(hdc,cx-arm,cy,NULL); LineTo(hdc,cx-3,cy);
    MoveToEx(hdc,cx+3,  cy,NULL); LineTo(hdc,cx+arm,cy);
    /* vertical */
    MoveToEx(hdc,cx,cy-arm,NULL); LineTo(hdc,cx,cy-3);
    MoveToEx(hdc,cx,cy+3,  NULL); LineTo(hdc,cx,cy+arm);
    /* centre dot */
    SelectObject(hdc,old); DeleteObject(p);
    HBRUSH br=CreateSolidBrush(col);
    RECT dot={cx-1,cy-1,cx+2,cy+2}; FillRect(hdc,&dot,br);
    DeleteObject(br);
}

#define WM_GP_TICK  (WM_USER+20)   /* posted by main timer to overlay */

static LRESULT CALLBACK SelProc(HWND hw,UINT msg,WPARAM wp,LPARAM lp){
    switch(msg){
    case WM_SETCURSOR:
        /* Restore the standard OS arrow cursor — it appears on top of our crosshair */
        SetCursor(LoadCursor(NULL, IDC_CROSS));
        return TRUE;

    case WM_PAINT:{
        PAINTSTRUCT ps;HDC hdc=BeginPaint(hw,&ps);
        RECT rc;GetClientRect(hw,&rc);

        /* Dark overlay */
        HBRUSH ov=CreateSolidBrush(RGB(8,6,14));FillRect(hdc,&rc,ov);DeleteObject(ov);

        /* Selection rectangle */
        bool hasRect = g_sel.drag || g_sel.gpAnchorSet;
        if(hasRect){
            int x1=min(g_sel.a.x,g_sel.b.x),y1=min(g_sel.a.y,g_sel.b.y);
            int x2=max(g_sel.a.x,g_sel.b.x),y2=max(g_sel.a.y,g_sel.b.y);
            int ox=GetSystemMetrics(SM_XVIRTUALSCREEN);
            int oy=GetSystemMetrics(SM_YVIRTUALSCREEN);
            x1-=ox; y1-=oy; x2-=ox; y2-=oy;
            RECT sr={x1,y1,x2,y2};
            HBRUSH cl=CreateSolidBrush(RGB(55,35,100));FillRect(hdc,&sr,cl);DeleteObject(cl);
            HPEN pn=CreatePen(PS_SOLID,2,RGB(139,92,246));
            SelectObject(hdc,pn);SelectObject(hdc,GetStockObject(NULL_BRUSH));
            Rectangle(hdc,x1,y1,x2,y2);DeleteObject(pn);
            /* Overlay info label inside the selection rect shows configured buttons */
            int W=abs(g_sel.b.x-g_sel.a.x), H=abs(g_sel.b.y-g_sel.a.y);
            char lb[120];
            if(g_sel.gpAnchorSet && !g_sel.drag){
                /* Show confirm and cancel button names */
                char anchorNames[32]="";
                for(int oi=0;oi<GP_OV_SLOTS;oi++){
                    int bi=g_cfg.gpOvBtn[oi];
                    if(bi>=0&&bi<GP_BTN_COUNT){
                        if(oi>0&&anchorNames[0]) strcat_s(anchorNames,"/");
                        strcat_s(anchorNames,GP_BTN_NAMES[bi]);
                    }
                }
                const char *cancelName=(g_cfg.gpOvBtnCancel>=0&&g_cfg.gpOvBtnCancel<GP_BTN_COUNT)
                    ?GP_BTN_NAMES[g_cfg.gpOvBtnCancel]:"B";
                sprintf_s(lb,"[%s]=capture  [%s]=reset  %dx%d  (%d,%d)",
                    anchorNames,cancelName,W,H,
                    min(g_sel.a.x,g_sel.b.x),min(g_sel.a.y,g_sel.b.y));
            } else {
                sprintf_s(lb,"%dx%d  (%d,%d)",W,H,
                    min(g_sel.a.x,g_sel.b.x),min(g_sel.a.y,g_sel.b.y));
            }
            HFONT fn=CreateFontA(-14,0,0,0,FW_SEMIBOLD,0,0,0,DEFAULT_CHARSET,0,0,CLEARTYPE_QUALITY,0,"Segoe UI");
            SelectObject(hdc,fn);SetTextColor(hdc,RGB(200,170,255));SetBkMode(hdc,TRANSPARENT);
            RECT lr={x1+6,y1+4,x1+500,y1+22};
            DrawTextA(hdc,lb,-1,&lr,DT_LEFT|DT_TOP|DT_SINGLELINE);DeleteObject(fn);
        }

        /* Stick crosshair drawn BEHIND the OS cursor.
           Colour #8B5CF6 (RGB 139,92,246).
           When the mouse moves, g_vc tracks the real cursor so both coincide. */
        {
            int ox=GetSystemMetrics(SM_XVIRTUALSCREEN);
            int oy=GetSystemMetrics(SM_YVIRTUALSCREEN);
            int cx=(int)g_vc.x-ox, cy=(int)g_vc.y-oy;
            /* Subtle dark shadow for contrast on light backgrounds */
            draw_crosshair(hdc,cx,cy,RGB(20,10,36));
            /* Main crosshair: #8B5CF6 */
            draw_crosshair(hdc,cx,cy,RGB(139,92,246));
        }

        /* Instructions bar — idle shows gamepad buttons; anchor shows confirm/cancel */
        HFONT fn=CreateFontA(-15,0,0,0,FW_NORMAL,0,0,0,DEFAULT_CHARSET,0,0,CLEARTYPE_QUALITY,0,"Segoe UI");
        SelectObject(hdc,fn);SetTextColor(hdc,RGB(180,150,240));SetBkMode(hdc,TRANSPARENT);
        char hintBuf[256];
        if(g_sel.gpAnchorSet){
            /* "ButtonName=capture  CancelName=reset  DPad=nudge 1px" */
            char anchorStr[32]="";
            for(int oi=0;oi<GP_OV_SLOTS;oi++){
                int bi=g_cfg.gpOvBtn[oi];
                if(bi>=0&&bi<GP_BTN_COUNT){
                    if(oi>0&&anchorStr[0]) strcat_s(anchorStr,"/");
                    strcat_s(anchorStr,GP_BTN_NAMES[bi]);
                }
            }
            const char *cancelStr=(g_cfg.gpOvBtnCancel>=0&&g_cfg.gpOvBtnCancel<GP_BTN_COUNT)
                ?GP_BTN_NAMES[g_cfg.gpOvBtnCancel]:"B";
            sprintf_s(hintBuf,"Stick=move  [%s]=capture  [%s]=reset  DPad=+/-1px",anchorStr,cancelStr);
        } else {
            /* Idle: show anchor button(s) + cancel, and keyboard shortcut */
            char anchorStr[32]="";
            for(int oi=0;oi<GP_OV_SLOTS;oi++){
                int bi=g_cfg.gpOvBtn[oi];
                if(bi>=0&&bi<GP_BTN_COUNT){
                    if(oi>0&&anchorStr[0]) strcat_s(anchorStr,"/");
                    strcat_s(anchorStr,GP_BTN_NAMES[bi]);
                }
            }
            if(anchorStr[0]=='\0') strcpy_s(anchorStr,"A");
            const char *cancelStr=(g_cfg.gpOvBtnCancel>=0&&g_cfg.gpOvBtnCancel<GP_BTN_COUNT)
                ?GP_BTN_NAMES[g_cfg.gpOvBtnCancel]:"B";
            sprintf_s(hintBuf,
                "Drag to select  |  [%s]=anchor  [%s]=exit  |  ESC to cancel",
                anchorStr,cancelStr);
        }
        RECT ir={0,0,rc.right,36};
        DrawTextA(hdc,hintBuf,-1,&ir,DT_CENTER|DT_VCENTER|DT_SINGLELINE);
        DeleteObject(fn);
        EndPaint(hw,&ps);
        return 0;
    }

    /* ---- Mouse (keep original drag behaviour) ---- */
    case WM_LBUTTONDOWN:{
        int ox=GetSystemMetrics(SM_XVIRTUALSCREEN);
        int oy=GetSystemMetrics(SM_YVIRTUALSCREEN);
        int sx_=GET_X_LPARAM(lp)+ox, sy_=GET_Y_LPARAM(lp)+oy;
        g_sel.a={sx_,sy_}; g_sel.b=g_sel.a;
        g_sel.drag=true; g_sel.done=false; g_sel.cancelled=false;
        g_sel.gpAnchorSet=false;
        /* Sync VC to mouse click position */
        g_vc.x=(float)sx_; g_vc.y=(float)sy_;
        SetCapture(hw);
        return 0;
    }
    case WM_MOUSEMOVE:{
        int ox=GetSystemMetrics(SM_XVIRTUALSCREEN);
        int oy=GetSystemMetrics(SM_YVIRTUALSCREEN);
        float nx=(float)(GET_X_LPARAM(lp)+ox);
        float ny=(float)(GET_Y_LPARAM(lp)+oy);
        /* Mouse MOVES the virtual cursor (doesn't snap-reset it) */
        g_vc.x=nx; g_vc.y=ny;
        g_vc.vx=0; g_vc.vy=0; /* cancel any stick momentum when mouse takes over */
        if(g_sel.drag){
            g_sel.b={(int)nx,(int)ny};
        }
        InvalidateRect(hw,NULL,TRUE);
        return 0;
    }
    case WM_LBUTTONUP:
        if(g_sel.drag){
            int ox=GetSystemMetrics(SM_XVIRTUALSCREEN);
            int oy=GetSystemMetrics(SM_YVIRTUALSCREEN);
            g_sel.b={GET_X_LPARAM(lp)+ox, GET_Y_LPARAM(lp)+oy};
            ReleaseCapture();
            g_sel.drag=false; g_sel.done=true;
            DestroyWindow(hw);
        }
        return 0;

    /* ---- Keyboard ---- */
    case WM_KEYDOWN:
        if(wp==VK_ESCAPE){g_sel.cancelled=true;g_sel.done=false;DestroyWindow(hw);}
        return 0;

    /* ---- Gamepad tick (16ms timer → WM_GP_TICK) ---- */
    case WM_GP_TICK:{
        GamepadState st=gp_snapshot();
        bool anyChanged=false;

        /* Stick → move virtual cursor */
        vc_update_stick((float)st.sx,(float)st.sy);
        vc_apply();
        anyChanged=true;

        /* DPad nudge: 1px per press, moves the crosshair regardless of anchor state.
           Uses gpDpadFlipX/Y (separate from stick flip).
           gp_just_pressed fires only on the rising edge so holding = no repeat. */
        {
            int dx=0,dy=0;
            if(gp_just_pressed(st,"DRight")) dx= g_cfg.gpDpadFlipX?-1:+1;
            if(gp_just_pressed(st,"DLeft"))  dx= g_cfg.gpDpadFlipX?+1:-1;
            if(gp_just_pressed(st,"DDown"))  dy= g_cfg.gpDpadFlipY?-1:+1;
            if(gp_just_pressed(st,"DUp"))    dy= g_cfg.gpDpadFlipY?+1:-1;
            if(dx||dy){
                g_vc.x+=(float)dx; g_vc.y+=(float)dy;
                /* clamp to virtual screen */
                float vx0=(float)GetSystemMetrics(SM_XVIRTUALSCREEN);
                float vy0=(float)GetSystemMetrics(SM_YVIRTUALSCREEN);
                float vw =(float)GetSystemMetrics(SM_CXVIRTUALSCREEN);
                float vh =(float)GetSystemMetrics(SM_CYVIRTUALSCREEN);
                if(g_vc.x<vx0)g_vc.x=vx0;
                if(g_vc.y<vy0)g_vc.y=vy0;
                if(g_vc.x>vx0+vw-1)g_vc.x=vx0+vw-1.f;
                if(g_vc.y>vy0+vh-1)g_vc.y=vy0+vh-1.f;
                anyChanged=true;
            }
        }

        /* While anchor set, free corner follows crosshair (base position).
           DPad already moved g_vc above, so g_sel.b picks that up here — no double nudge. */
        if(g_sel.gpAnchorSet){ g_sel.b={(int)g_vc.x,(int)g_vc.y}; anyChanged=true; }

        /* Anchor/confirm: use the two configurable overlay buttons */
        bool pressAZ = false;
        for(int oi=0;oi<GP_OV_SLOTS;oi++){
            int bi=g_cfg.gpOvBtn[oi];
            if(bi>=0&&bi<GP_BTN_COUNT && gp_just_pressed(st,GP_BTN_NAMES[bi]))
                pressAZ=true;
        }
        if(pressAZ){
            if(!g_sel.gpAnchorSet){
                g_sel.a={(int)g_vc.x,(int)g_vc.y};
                g_sel.b=g_sel.a;
                g_sel.gpAnchorSet=true;
                g_sel.done=false; g_sel.cancelled=false;
            } else {
                g_sel.b={(int)g_vc.x,(int)g_vc.y};
                g_sel.done=true; g_sel.drag=false;
                gp_edge_update(st);
                DestroyWindow(hw);
                return 0;
            }
            anyChanged=true;
        }
        /* Cancel/reset button: configurable, defaults to B (idx 1) */
        bool pressCancel = false;
        {
            int ci = g_cfg.gpOvBtnCancel;
            if(ci>=0&&ci<GP_BTN_COUNT) pressCancel=gp_just_pressed(st,GP_BTN_NAMES[ci]);
            else pressCancel=gp_just_pressed(st,"B"); /* fallback */
        }
        if(pressCancel){
            if(g_sel.gpAnchorSet){
                g_sel.gpAnchorSet=false; g_sel.done=false; anyChanged=true;
            } else {
                g_sel.cancelled=true; g_sel.done=false;
                gp_edge_update(st); DestroyWindow(hw); return 0;
            }
        }

        gp_edge_update(st);
        if(anyChanged) InvalidateRect(hw,NULL,TRUE);
        return 0;
    }

    case WM_RBUTTONUP:
        g_sel.cancelled=true; g_sel.done=false;
        DestroyWindow(hw);
        return 0;

    case WM_DESTROY:PostQuitMessage(0);return 0;
    }
    return DefWindowProc(hw,msg,wp,lp);
}

/* Timer ID for the overlay message pump */
#define SEL_TIMER_ID  1

static bool rect_select(RECT &out){
    WNDCLASSA wc={0};
    wc.lpfnWndProc=SelProc;
    wc.hInstance=GetModuleHandle(NULL);
    wc.hCursor=NULL;  /* no default cursor – we draw our own */
    wc.lpszClassName="MCT_Sel";
    RegisterClassA(&wc);

    g_sel={};
    /* Init virtual cursor at current mouse position */
    {POINT p;GetCursorPos(&p);g_vc.x=(float)p.x;g_vc.y=(float)p.y;}
    g_vc.visible=true;
    g_vc.vx=g_vc.vy=0.f;
    g_vc.inDeadZone=true;

    int sw=GetSystemMetrics(SM_CXVIRTUALSCREEN);
    int sh=GetSystemMetrics(SM_CYVIRTUALSCREEN);
    int sx=GetSystemMetrics(SM_XVIRTUALSCREEN);
    int sy=GetSystemMetrics(SM_YVIRTUALSCREEN);

    g_selWnd=CreateWindowExA(
        WS_EX_TOPMOST|WS_EX_LAYERED|WS_EX_TOOLWINDOW,
        "MCT_Sel",NULL,WS_POPUP|WS_VISIBLE,
        sx,sy,sw,sh,NULL,NULL,GetModuleHandle(NULL),NULL);
    if(!g_selWnd)return false;

    SetLayeredWindowAttributes(g_selWnd,0,170,LWA_ALPHA);
    SetForegroundWindow(g_selWnd);

    /* Timer to push gamepad ticks into the overlay's message loop */
    SetTimer(g_selWnd,SEL_TIMER_ID,16,NULL);

    MSG msg;
    while(GetMessage(&msg,NULL,0,0)){
        if(msg.message==WM_TIMER && msg.hwnd==g_selWnd && msg.wParam==SEL_TIMER_ID){
            SendMessage(g_selWnd,WM_GP_TICK,0,0);
        }
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    g_selWnd=NULL;
    g_vc.visible=false;
    if(!g_sel.done)return false;

    int x1=min(g_sel.a.x,g_sel.b.x), y1=min(g_sel.a.y,g_sel.b.y);
    int x2=max(g_sel.a.x,g_sel.b.x), y2=max(g_sel.a.y,g_sel.b.y);
    out={x1,y1,x2,y2};
    return (out.right-out.left)>=8 && (out.bottom-out.top)>=8;
}

/* ==================================================================
   CAPTURE HELPERS
   ================================================================== */
static RECT fullscreen_rect(){
    RECT r;
    r.left=GetSystemMetrics(SM_XVIRTUALSCREEN);r.top=GetSystemMetrics(SM_YVIRTUALSCREEN);
    r.right=r.left+GetSystemMetrics(SM_CXVIRTUALSCREEN);
    r.bottom=r.top+GetSystemMetrics(SM_CYVIRTUALSCREEN);return r;
}
static HBITMAP capture_hbm(RECT r){
    int w=r.right-r.left,h=r.bottom-r.top;if(w<=0||h<=0)return NULL;
    HDC scr=GetDC(NULL);HDC mem=CreateCompatibleDC(scr);
    HBITMAP bm=CreateCompatibleBitmap(scr,w,h);
    HBITMAP old=(HBITMAP)SelectObject(mem,bm);
    BitBlt(mem,0,0,w,h,scr,r.left,r.top,SRCCOPY|CAPTUREBLT);
    SelectObject(mem,old);DeleteDC(mem);ReleaseDC(NULL,scr);return bm;
}
static CLSID s_pngClsid;static bool s_pngSet=false;
static CLSID get_png_clsid(){
    if(s_pngSet)return s_pngClsid;
    UINT n=0,sz=0;GetImageEncodersSize(&n,&sz);
    std::vector<BYTE>buf(sz);ImageCodecInfo*ci=(ImageCodecInfo*)buf.data();GetImageEncoders(n,sz,ci);
    for(UINT i=0;i<n;i++)if(!wcscmp(ci[i].MimeType,L"image/png")){s_pngClsid=ci[i].Clsid;s_pngSet=true;break;}
    return s_pngClsid;
}
static bool save_png(HBITMAP hbm,const char*path){
    Bitmap bmp(hbm,NULL);CLSID c=get_png_clsid();
    int wn=MultiByteToWideChar(CP_UTF8,0,path,-1,NULL,0);
    std::vector<wchar_t>wp(wn);MultiByteToWideChar(CP_UTF8,0,path,-1,wp.data(),wn);
    return bmp.Save(wp.data(),&c,NULL)==Ok;
}
static string ts_str(){
    SYSTEMTIME t;GetLocalTime(&t);char b[32];
    sprintf_s(b,"%04d%02d%02d_%02d%02d%02d",t.wYear,t.wMonth,t.wDay,t.wHour,t.wMinute,t.wSecond);
    return b;
}
static string build_path(const char*dir,const char*pfx,const char*ext){
    try{bfs::create_directories(dir);}catch(...){}
    char b[MAX_PATH];
    if(g_cfg.useTimestamp)sprintf_s(b,"%s\\%s_%s.%s",dir,pfx,ts_str().c_str(),ext);
    else for(int n=1;n<99999;n++){sprintf_s(b,"%s\\%s_%04d.%s",dir,pfx,n,ext);if(!bfs::exists(b))break;}
    return b;
}

/* status */
static char g_statusMsg[256]="";static char g_statusCol[8]="muted";
static void set_status(const char*m,const char*c){strcpy_s(g_statusMsg,m);strcpy_s(g_statusCol,c);}
static struct nk_color status_col(const char*c){
    if(!strcmp(c,"green"))return C_GREEN;if(!strcmp(c,"red"))return C_RED;
    if(!strcmp(c,"amber"))return C_AMBER;if(!strcmp(c,"accent"))return C_PUR_L;
    return C_MUTED;
}

/* mode: 0=rect  1=full  2=fixed */
static void do_screenshot(int mode,bool fromTray){
    hide_for_capture();
    RECT r={};bool ok=false;
    switch(mode){
    case 0: ok=rect_select(r); break;
    case 1: r=fullscreen_rect();ok=true; break;
    case 2: r={g_cfg.shotFx,g_cfg.shotFy,
               g_cfg.shotFx+max(1,g_cfg.shotFw),
               g_cfg.shotFy+max(1,g_cfg.shotFh)};ok=true; break;
    }
    if(!ok){
        set_status(T(S_CANCELLED),"muted");
        /* Restore only if window was visible before capture */
        if(!fromTray && g_wasVisibleBeforeCapture) show_after_capture();
        return;
    }
    HBITMAP bm=capture_hbm(r);
    /* Copy to clipboard before saving, if setting is enabled */
    if(bm && g_cfg.copyToClipboard){
        if(OpenClipboard(g_hwnd)){
            EmptyClipboard();
            /* CF_BITMAP takes ownership of the handle – duplicate first */
            HDC dc=GetDC(NULL);
            HDC mdc=CreateCompatibleDC(dc);
            int w=r.right-r.left, h=r.bottom-r.top;
            HBITMAP dup=CreateCompatibleBitmap(dc,w,h);
            HBITMAP oold=(HBITMAP)SelectObject(mdc,dup);
            HDC sdc=CreateCompatibleDC(dc);
            HBITMAP oold2=(HBITMAP)SelectObject(sdc,bm);
            BitBlt(mdc,0,0,w,h,sdc,0,0,SRCCOPY);
            SelectObject(sdc,oold2); DeleteDC(sdc);
            SelectObject(mdc,oold); DeleteDC(mdc);
            ReleaseDC(NULL,dc);
            SetClipboardData(CF_BITMAP,dup);
            CloseClipboard();
            /* dup is now owned by clipboard; do NOT DeleteObject it */
        }
    }
    string path=build_path(g_cfg.outputDir,g_cfg.shotPfx,"png");
    bool saved=bm&&save_png(bm,path.c_str());
    if(bm)DeleteObject(bm);
    /* Restore window only if it was visible before capture started */
    if(!fromTray && g_wasVisibleBeforeCapture) show_after_capture();
    if(saved){
        bfs::path p(path);char b[300];
        sprintf_s(b,"%s%s",T(S_SHOT_SAVED),p.filename().string().c_str());
        set_status(b,"green");show_toast(APP_NAME,b);
    }else set_status(T(S_SHOT_FAIL),"red");
}

/* ==================================================================
   HOTKEY CAPTURE (low-level hook)
   ================================================================== */
static int   g_listenIdx=-1;
static HHOOK g_listenHook=NULL;

static LRESULT CALLBACK HkHook(int code,WPARAM wp,LPARAM lp){
    if(code>=0&&wp==WM_KEYDOWN){
        KBDLLHOOKSTRUCT*kb=(KBDLLHOOKSTRUCT*)lp;
        int vk=(int)kb->vkCode;
        if(vk!=VK_SHIFT&&vk!=VK_LSHIFT&&vk!=VK_RSHIFT&&
           vk!=VK_CONTROL&&vk!=VK_LCONTROL&&vk!=VK_RCONTROL&&
           vk!=VK_MENU&&vk!=VK_LMENU&&vk!=VK_RMENU&&
           vk!=VK_LWIN&&vk!=VK_RWIN){
            int mod=0;
            if(GetAsyncKeyState(VK_CONTROL)&0x8000)mod|=MOD_CONTROL;
            if(GetAsyncKeyState(VK_MENU)   &0x8000)mod|=MOD_ALT;
            if(GetAsyncKeyState(VK_SHIFT)  &0x8000)mod|=MOD_SHIFT;
            if(GetAsyncKeyState(VK_LWIN)   &0x8000||
               GetAsyncKeyState(VK_RWIN)   &0x8000)mod|=MOD_WIN;
            UnhookWindowsHookEx(g_listenHook);g_listenHook=NULL;
            PostMessageA(g_hwnd,WM_USER+10,(WPARAM)vk,(LPARAM)mod);
            return 1;
        }
    }
    return CallNextHookEx(g_listenHook,code,wp,lp);
}
static void hk_listen_start(int idx){
    if(g_listenHook){UnhookWindowsHookEx(g_listenHook);g_listenHook=NULL;}
    g_listenIdx=idx;
    g_listenHook=SetWindowsHookExA(WH_KEYBOARD_LL,HkHook,GetModuleHandle(NULL),0);
}
static void hk_listen_cancel(){
    if(g_listenHook){UnhookWindowsHookEx(g_listenHook);g_listenHook=NULL;}
    g_listenIdx=-1;
}
static void hk_apply(int idx,int vk,int mod){
    g_cfg.hk[idx].vk=vk;g_cfg.hk[idx].mod=mod;
    vk_to_str(vk,mod,g_cfg.hk[idx].label,sizeof(g_cfg.hk[idx].label));
    register_hotkeys();g_listenIdx=-1;
}

/* ==================================================================
   TAB STATE  (declared here so process_gamepad can reference it)
   ================================================================== */
enum TabID{T_CAPTURE=0,T_SHOT,T_SETTINGS,T_GAMEPAD,T_ABOUT,T_COUNT};
static int g_tab=T_CAPTURE;
static struct nk_scroll g_tabScroll[T_COUNT]={};

/* ==================================================================
   GAMEPAD PROCESSING  (called every main-loop iteration)
   ================================================================== */

/* helper: dispatch a GpAction */
static void gp_do_action(int action){
    switch(action){
    case GPA_RECT_CAPTURE:  do_screenshot(0,false); break;
    case GPA_FULL_CAPTURE:  do_screenshot(1,false); break;
    case GPA_FIXED_CAPTURE: do_screenshot(2,false); break;
    case GPA_TOGGLE_WIN:
        if(g_windowVisible){ wnd_save_placement();ShowWindow(g_hwnd,SW_HIDE);g_windowVisible=false; }
        else show_restore_from_tray();
        break;
    case GPA_OPEN_FOLDER:
        try{bfs::create_directories(g_cfg.outputDir);}catch(...){}
        ShellExecuteA(NULL,"open",g_cfg.outputDir,NULL,NULL,SW_SHOWNORMAL);
        break;
    default: break;
    }
}

static void process_gamepad(){
    if(g_gpSetup!=GPS_CONNECTED) return;

    GamepadState st=gp_snapshot();

    /* Dispatch remapped button actions */
    for(int i=0;i<GP_BTN_COUNT;i++){
        if(g_cfg.gpBtnAction[i]==GPA_NONE) continue;
        if(gp_just_pressed(st,GP_BTN_NAMES[i]))
            gp_do_action(g_cfg.gpBtnAction[i]);
    }

    /* ---- App navigation (only when window visible and no hotkey listening) ---- */
    if(g_windowVisible && g_listenIdx<0){
        /* DPad Left/Right → switch tabs */
        if(gp_just_pressed(st,"DLeft")){
            g_tab=(g_tab>0)?g_tab-1:T_COUNT-1;
        }
        if(gp_just_pressed(st,"DRight")){
            g_tab=(g_tab+1)%T_COUNT;
        }
        /* DPad Up/Down → scroll active tab */
        if(gp_pressed(st,"DUp")){
            if(g_tabScroll[g_tab].y>8) g_tabScroll[g_tab].y-=8;
            else g_tabScroll[g_tab].y=0;
        }
        if(gp_pressed(st,"DDown")){
            g_tabScroll[g_tab].y+=8;
        }
        /* A → left-click at centre of window (activates focused Nuklear widget) */
        if(gp_just_pressed(st,"A")){
            RECT r;GetClientRect(g_hwnd,&r);
            POINT ctr={(r.left+r.right)/2,(r.top+r.bottom)/2};
            ClientToScreen(g_hwnd,&ctr);
            /* Simulate click */
            SetCursorPos(ctr.x,ctr.y);
            mouse_event(MOUSEEVENTF_LEFTDOWN,0,0,0,0);
            mouse_event(MOUSEEVENTF_LEFTUP,0,0,0,0);
        }
        /* B → escape / cancel current operation (inject VK_ESCAPE) */
        if(gp_just_pressed(st,"B")){
            if(g_listenIdx>=0) hk_listen_cancel();
            else PostMessageA(g_hwnd,WM_KEYDOWN,VK_ESCAPE,0);
        }
    }

    gp_edge_update(st);
}

/* ==================================================================
   NUKLEAR THEME
   ================================================================== */
static struct nk_context g_nk;
static void apply_theme(){
    struct nk_color t[NK_COLOR_COUNT];
    for(int i=0;i<NK_COLOR_COUNT;i++)t[i]=nk_rgba(26,23,40,255);
    t[NK_COLOR_WINDOW]=C_BG;t[NK_COLOR_HEADER]=C_SURF;t[NK_COLOR_BORDER]=C_BORDER;
    t[NK_COLOR_TEXT]=C_TEXT;t[NK_COLOR_BUTTON]=C_CARD;t[NK_COLOR_BUTTON_HOVER]=C_CARD2;
    t[NK_COLOR_BUTTON_ACTIVE]=C_PUR_D;t[NK_COLOR_TOGGLE]=C_CARD;t[NK_COLOR_TOGGLE_HOVER]=C_CARD2;
    t[NK_COLOR_TOGGLE_CURSOR]=C_PUR;t[NK_COLOR_SELECT]=C_CARD;t[NK_COLOR_SELECT_ACTIVE]=C_PUR_D;
    t[NK_COLOR_SLIDER]=C_CARD;t[NK_COLOR_SLIDER_CURSOR]=C_PUR;
    t[NK_COLOR_SLIDER_CURSOR_HOVER]=C_PUR_L;t[NK_COLOR_SLIDER_CURSOR_ACTIVE]=nk_rgb(109,40,217);
    t[NK_COLOR_PROPERTY]=C_SURF;t[NK_COLOR_EDIT]=nk_rgb(14,12,22);t[NK_COLOR_EDIT_CURSOR]=C_PUR_L;
    t[NK_COLOR_COMBO]=C_CARD;t[NK_COLOR_CHART]=C_SURF;t[NK_COLOR_CHART_COLOR]=C_PUR;
    t[NK_COLOR_CHART_COLOR_HIGHLIGHT]=C_RED;t[NK_COLOR_SCROLLBAR]=C_SURF;
    t[NK_COLOR_SCROLLBAR_CURSOR]=C_BORDER;t[NK_COLOR_SCROLLBAR_CURSOR_HOVER]=C_PUR_D;
    t[NK_COLOR_SCROLLBAR_CURSOR_ACTIVE]=C_PUR;t[NK_COLOR_TAB_HEADER]=C_SURF;
    nk_style_from_table(&g_nk,t);
    g_nk.style.window.padding=nk_vec2(12,8);g_nk.style.window.spacing=nk_vec2(5,4);
    g_nk.style.button.rounding=4;g_nk.style.button.border=1.f;
    g_nk.style.button.border_color=C_BORDER;g_nk.style.button.padding=nk_vec2(6,4);
    g_nk.style.edit.border=1.f;g_nk.style.edit.border_color=C_BORDER;
    g_nk.style.edit.rounding=4;g_nk.style.edit.padding=nk_vec2(6,3);g_nk.style.edit.cursor_size=2;
    g_nk.style.checkbox.border=1;g_nk.style.checkbox.border_color=C_BORDER;
    g_nk.style.checkbox.padding=nk_vec2(2,2);
}

/* ==================================================================
   UI HELPERS
   ================================================================== */
static void section_label(const char*txt){
    nk_layout_row_dynamic(&g_nk,5,1);nk_spacing(&g_nk,1);
    nk_layout_row_dynamic(&g_nk,16,1);
    nk_style_push_font(&g_nk,&g_fXs.nk);
    g_nk.style.text.color=C_PUR_L;nk_label(&g_nk,txt,NK_TEXT_LEFT);
    g_nk.style.text.color=C_TEXT;nk_style_pop_font(&g_nk);
    nk_layout_row_dynamic(&g_nk,1,1);
    {struct nk_rect r;nk_widget(&r,&g_nk);nk_fill_rect(nk_window_get_canvas(&g_nk),r,0,C_BORDER);}
    nk_layout_row_dynamic(&g_nk,4,1);nk_spacing(&g_nk,1);
}
static bool action_btn(const char*lbl){
    struct nk_style_button sb=g_nk.style.button;
    g_nk.style.button.normal.data.color=C_PUR_D;
    g_nk.style.button.hover.data.color=nk_rgb(90,55,160);
    g_nk.style.button.border_color=C_PUR;g_nk.style.text.color=C_TEXT;
    nk_style_push_font(&g_nk,&g_fMd.nk);
    bool r=nk_button_label(&g_nk,lbl)!=0;
    nk_style_pop_font(&g_nk);g_nk.style.button=sb;g_nk.style.text.color=C_TEXT;return r;
}
static void row_str(const char*lbl,float lw,char*buf,int sz){
    float ww=nk_window_get_width(&g_nk)-24.f;
    nk_layout_row_begin(&g_nk,NK_STATIC,26,2);
    nk_layout_row_push(&g_nk,lw);nk_style_push_font(&g_nk,&g_fXs.nk);
    g_nk.style.text.color=C_MUTED;nk_label(&g_nk,lbl,NK_TEXT_LEFT);g_nk.style.text.color=C_TEXT;
    nk_layout_row_push(&g_nk,ww-lw-4);
    nk_edit_string_zero_terminated(&g_nk,NK_EDIT_FIELD,buf,sz,nk_filter_default);
    nk_style_pop_font(&g_nk);nk_layout_row_end(&g_nk);
}
static void row_xywh(char*bx,char*by,char*bw,char*bh){
    float ww=nk_window_get_width(&g_nk)-24.f;float fw=(ww-4*22)/4.f;
    nk_layout_row_begin(&g_nk,NK_STATIC,26,8);nk_style_push_font(&g_nk,&g_fXs.nk);
    const char*lb[]={"X","Y","W","H"};char*bu[]={bx,by,bw,bh};
    for(int i=0;i<4;i++){
        nk_layout_row_push(&g_nk,22);g_nk.style.text.color=C_MUTED;
        nk_label(&g_nk,lb[i],NK_TEXT_CENTERED);g_nk.style.text.color=C_TEXT;
        nk_layout_row_push(&g_nk,fw);
        nk_edit_string_zero_terminated(&g_nk,NK_EDIT_FIELD,bu[i],10,nk_filter_decimal);
    }
    nk_style_pop_font(&g_nk);nk_layout_row_end(&g_nk);
}
/* Forward declaration – defined after edit buffer globals */
static void flush_edits();

static void save_btn(){
    nk_layout_row_dynamic(&g_nk,10,1);nk_spacing(&g_nk,1);
    nk_layout_row_dynamic(&g_nk,34,1);
    struct nk_style_button sb=g_nk.style.button;
    g_nk.style.button.normal.data.color=C_PUR_D;
    g_nk.style.button.hover.data.color=nk_rgb(90,55,160);
    g_nk.style.button.border_color=C_PUR;nk_style_push_font(&g_nk,&g_fSm.nk);
    if(nk_button_label(&g_nk,T(S_SET_SAVE))){
        flush_edits();      /* apply edit buffers → g_cfg immediately */
        ini_save(g_cfg);
        register_hotkeys();
        SetWindowPos(g_hwnd,g_cfg.alwaysOnTop?HWND_TOPMOST:HWND_NOTOPMOST,0,0,0,0,SWP_NOMOVE|SWP_NOSIZE);
        if(g_cfg.enableNotif) show_toast(APP_NAME,T(S_SETTINGS_SAVED)); /* only when notif enabled */
        set_status(T(S_SETTINGS_SAVED),"green");
    }
    nk_style_pop_font(&g_nk);g_nk.style.button=sb;
    nk_layout_row_dynamic(&g_nk,21,1);nk_spacing(&g_nk,1); /* bottom padding */
}

/* edit buffers */
static char eb_fx[12],eb_fy[12],eb_fw[12],eb_fh[12];
static char eb_dir[MAX_PATH],eb_pfx[64];
static void sync_edits(){
    sprintf_s(eb_fx,"%d",g_cfg.shotFx);sprintf_s(eb_fy,"%d",g_cfg.shotFy);
    sprintf_s(eb_fw,"%d",g_cfg.shotFw);sprintf_s(eb_fh,"%d",g_cfg.shotFh);
    strcpy_s(eb_dir,g_cfg.outputDir);strcpy_s(eb_pfx,g_cfg.shotPfx);
}
static void flush_edits(){
    g_cfg.shotFx=max(0,atoi(eb_fx));g_cfg.shotFy=max(0,atoi(eb_fy));
    g_cfg.shotFw=max(1,atoi(eb_fw));g_cfg.shotFh=max(1,atoi(eb_fh));
    strcpy_s(g_cfg.outputDir,eb_dir);strcpy_s(g_cfg.shotPfx,eb_pfx);
}

/* ==================================================================
   TAB CONTENT FUNCTIONS
   ================================================================== */

/* ----  Capture tab  ---- */
static void tab_capture(){
    float ww=nk_window_get_width(&g_nk)-24.f;
    section_label(T(S_TAB_SHOT));

    nk_layout_row_dynamic(&g_nk,36,1);
    if(action_btn(T(S_MODE_RECT))) do_screenshot(0,false);
    nk_layout_row_dynamic(&g_nk,36,1);
    if(action_btn(T(S_MODE_FULL))) do_screenshot(1,false);
    nk_layout_row_dynamic(&g_nk,36,1);
    if(action_btn(T(S_MODE_FIXED)))do_screenshot(2,false);

    /* Status */
    section_label(T(S_STATUS));
    struct nk_color dc=status_col(g_statusCol);
    nk_layout_row_begin(&g_nk,NK_STATIC,22,2);
    nk_layout_row_push(&g_nk,14);
    {struct nk_rect dr;nk_widget(&dr,&g_nk);
     nk_fill_circle(nk_window_get_canvas(&g_nk),nk_rect(dr.x,dr.y+5,10,10),dc);}
    nk_layout_row_push(&g_nk,ww-18);
    nk_style_push_font(&g_nk,&g_fXs.nk);g_nk.style.text.color=dc;
    nk_label(&g_nk,g_statusMsg,NK_TEXT_LEFT);g_nk.style.text.color=C_TEXT;nk_style_pop_font(&g_nk);
    nk_layout_row_end(&g_nk);

    nk_layout_row_dynamic(&g_nk,8,1);nk_spacing(&g_nk,1);
    nk_layout_row_dynamic(&g_nk,28,1);
    {struct nk_style_button sb=g_nk.style.button;
     g_nk.style.button.normal.data.color=C_CARD;g_nk.style.button.border_color=C_BORDER;
     g_nk.style.text.color=C_MUTED;nk_style_push_font(&g_nk,&g_fXs.nk);
     if(nk_button_label(&g_nk,T(S_OPEN_FOLDER))){
         try{bfs::create_directories(g_cfg.outputDir);}catch(...){}
         ShellExecuteA(NULL,"open",g_cfg.outputDir,NULL,NULL,SW_SHOWNORMAL);}
     nk_style_pop_font(&g_nk);g_nk.style.button=sb;g_nk.style.text.color=C_TEXT;}
    nk_layout_row_dynamic(&g_nk,21,1);nk_spacing(&g_nk,1); /* bottom padding */
}

static float tw_xs(const char*s){
    if(!s||!*s)return 0.f;
    return g_fXs.nk.width(g_fXs.nk.userdata,13.f,s,(int)strlen(s))+2.f;
}

/* ----  Hotkey row  ---- */
static void hk_row(int idx, const char *seclabel){
    bool listening=(g_listenIdx==idx);

    nk_layout_row_dynamic(&g_nk,16,1);
    nk_style_push_font(&g_nk,&g_fXs.nk);
    g_nk.style.text.color=C_MUTED;
    nk_label(&g_nk,seclabel,NK_TEXT_LEFT);
    g_nk.style.text.color=C_TEXT;
    nk_style_pop_font(&g_nk);

    nk_layout_row_dynamic(&g_nk,26,1);
    nk_style_push_font(&g_nk,&g_fXs.nk);
    if(listening){
        g_nk.style.text.color=C_AMBER;
        nk_label(&g_nk,T(S_HK_PRESS_TO_SET),NK_TEXT_LEFT);
    }else{
        g_nk.style.text.color=g_cfg.hk[idx].vk?C_PUR_L:C_DIM;
        nk_label(&g_nk,g_cfg.hk[idx].vk?g_cfg.hk[idx].label:T(S_HK_NONE),NK_TEXT_LEFT);
    }
    g_nk.style.text.color=C_TEXT;
    nk_style_pop_font(&g_nk);

    nk_layout_row_dynamic(&g_nk,28,1);
    if(listening){
        struct nk_style_button sb=g_nk.style.button;
        g_nk.style.button.normal.data.color=nk_rgb(55,20,20);
        g_nk.style.button.border_color=C_RED;
        g_nk.style.text.color=C_RED;
        nk_style_push_font(&g_nk,&g_fXs.nk);
        if(nk_button_label(&g_nk,"Cancel"))hk_listen_cancel();
        nk_style_pop_font(&g_nk);
        g_nk.style.button=sb;g_nk.style.text.color=C_TEXT;
    }else{
        struct nk_style_button sb=g_nk.style.button;
        g_nk.style.button.normal.data.color=C_CARD2;
        g_nk.style.button.border_color=C_BORDER;
        g_nk.style.text.color=C_MUTED;
        nk_style_push_font(&g_nk,&g_fXs.nk);
        if(nk_button_label(&g_nk,"Set key..."))hk_listen_start(idx);
        nk_style_pop_font(&g_nk);
        g_nk.style.button=sb;g_nk.style.text.color=C_TEXT;
    }

    if(!listening&&g_cfg.hk[idx].vk){
        nk_layout_row_dynamic(&g_nk,22,1);
        struct nk_style_button sb=g_nk.style.button;
        g_nk.style.button.normal.data.color=C_CARD;
        g_nk.style.button.border_color=C_BORDER;
        g_nk.style.text.color=C_DIM;
        nk_style_push_font(&g_nk,&g_fXs.nk);
        if(nk_button_label(&g_nk,T(S_HK_CLEAR))){
            g_cfg.hk[idx].vk=0;g_cfg.hk[idx].mod=0;
            g_cfg.hk[idx].label[0]=0;register_hotkeys();}
        nk_style_pop_font(&g_nk);
        g_nk.style.button=sb;g_nk.style.text.color=C_TEXT;
    }

    nk_layout_row_dynamic(&g_nk,8,1);nk_spacing(&g_nk,1);
}

/* ----  Screenshot tab  ---- */
static void tab_shot(){
    hk_row(HK_RECT, T(S_HK_LABEL_RECT));
    hk_row(HK_FULL, T(S_HK_LABEL_FULL));
    hk_row(HK_FIXED,T(S_HK_LABEL_FIXED));

    section_label(T(S_FIXED_XYWH));
    row_xywh(eb_fx,eb_fy,eb_fw,eb_fh);

    section_label(T(S_SET_SHOT_PFX));
    row_str(T(S_SET_SHOT_PFX),120,eb_pfx,64);
    save_btn();
}

/* ----  Settings tab  ---- */
static void tab_settings(){
    float ww = nk_window_get_width(&g_nk) - 24.f;

    section_label(T(S_SET_OUTDIR));
    row_str("", 4, eb_dir, MAX_PATH);
    nk_style_push_font(&g_nk, &g_fXs.nk);
    g_nk.style.text.color = C_DIM;
    nk_layout_row_dynamic(&g_nk, 16, 1);
    nk_label(&g_nk, "  Default: Captures\\ next to .exe", NK_TEXT_LEFT);
    g_nk.style.text.color = C_TEXT;
    nk_style_pop_font(&g_nk);

    section_label(T(S_SET_LANG));
    nk_layout_row_dynamic(&g_nk, 26, 1);
    nk_style_push_font(&g_nk, &g_fXs.nk);
    const char* lnames[LANG_COUNT];
    for(int i = 0; i < LANG_COUNT; i++)
        lnames[i] = g_langs[i].displayName;
    int nl = nk_combo(&g_nk, lnames, LANG_COUNT, g_cfg.language, 22, nk_vec2(200,137));
    if(nl != g_cfg.language){ g_cfg.language = nl; g_lang = nl; }
    nk_style_pop_font(&g_nk);

    section_label("DISPLAY");
    nk_style_push_font(&g_nk, &g_fXs.nk);

    nk_layout_row_begin(&g_nk, NK_STATIC, 26, 1);
    nk_layout_row_push(&g_nk, ww);
    nk_checkbox_label(&g_nk, T(S_SET_TOP), &g_cfg.alwaysOnTop);
    nk_layout_row_end(&g_nk);

    nk_layout_row_begin(&g_nk, NK_STATIC, 26, 1);
    nk_layout_row_push(&g_nk, ww);
    nk_checkbox_label(&g_nk, T(S_SET_TRAY), &g_cfg.minimizeToTray);
    nk_layout_row_end(&g_nk);

    nk_layout_row_begin(&g_nk, NK_STATIC, 26, 1);
    nk_layout_row_push(&g_nk, ww);
    nk_checkbox_label(&g_nk, T(S_SET_NOTIF), &g_cfg.enableNotif);
    nk_layout_row_end(&g_nk);

    nk_layout_row_begin(&g_nk, NK_STATIC, 26, 1);
    nk_layout_row_push(&g_nk, ww);
    nk_checkbox_label(&g_nk, T(S_SET_TIMESTAMP), &g_cfg.useTimestamp);
    nk_layout_row_end(&g_nk);

    /* 🟢 Clipboard option — apply immediately when toggled */
    static int prevClipboard = -1; // Remember last state each frame
    nk_layout_row_begin(&g_nk, NK_STATIC, 26, 1);
    nk_layout_row_push(&g_nk, ww);
    nk_checkbox_label(&g_nk, T(S_SET_CLIPBOARD), &g_cfg.copyToClipboard);
    nk_layout_row_end(&g_nk);

    if(prevClipboard != g_cfg.copyToClipboard){
        prevClipboard = g_cfg.copyToClipboard;
        ini_save(g_cfg); // persist immediately
        set_status(T(S_SETTINGS_SAVED), "green"); // optional: visual feedback
    }

    nk_style_pop_font(&g_nk);
    save_btn();
}

/* ----  Gamepad tab  ---- */
/* Buffer for hz edit field */
static char eb_gpHz[8]    = "120";
static char eb_gpIdleHz[8]= "10";  /* Hz when window is hidden / idle */

static void tab_gamepad(){
    float ww=nk_window_get_width(&g_nk)-24.f;

    section_label(T(S_GP_TITLE));

    /* ---- Connection status dot + text ---- */
    nk_layout_row_begin(&g_nk,NK_STATIC,22,2);
    nk_layout_row_push(&g_nk,14);
    {
        struct nk_rect dr;nk_widget(&dr,&g_nk);
        struct nk_color dot=C_DIM;
        if(g_gpSetup==GPS_CONNECTED){
            GamepadState st=gp_snapshot();
            dot=st.connected?C_GREEN:C_AMBER;
        } else if(g_gpSetup==GPS_FAILED){
            dot=C_RED;
        }
        nk_fill_circle(nk_window_get_canvas(&g_nk),nk_rect(dr.x,dr.y+5,10,10),dot);
    }
    nk_layout_row_push(&g_nk,ww-18);
    nk_style_push_font(&g_nk,&g_fXs.nk);
    {
        const char *stTxt=T(S_GP_NOT_CONNECTED);
        struct nk_color stCol=C_DIM;
        if(g_gpSetup==GPS_CONNECTED){
            GamepadState st=gp_snapshot();
            stTxt=st.connected?T(S_GP_CONNECTED):T(S_GP_WAITING);
            stCol=st.connected?C_GREEN:C_AMBER;
        } else if(g_gpSetup==GPS_FAILED){
            stTxt=T(S_GP_FAILED);
            stCol=C_RED;
        } else if(g_gpSetup==GPS_SELECT_PORT){
            stTxt=T(S_GP_SELECT_PORT);
            stCol=C_MUTED;
        }
        g_nk.style.text.color=stCol;
        nk_label(&g_nk,stTxt,NK_TEXT_LEFT);
        g_nk.style.text.color=C_TEXT;
    }
    nk_style_pop_font(&g_nk);
    nk_layout_row_end(&g_nk);

    nk_layout_row_dynamic(&g_nk,6,1);nk_spacing(&g_nk,1);

    /* ================================================================
       NOT CONNECTED: show port scanner + hz config + connect button
       ================================================================ */
    if(g_gpSetup!=GPS_CONNECTED){

        section_label("COM PORT");

        /* Scan button */
        nk_layout_row_dynamic(&g_nk,28,1);
        {
            struct nk_style_button sb=g_nk.style.button;
            g_nk.style.button.normal.data.color=C_CARD2;
            g_nk.style.button.border_color=C_BORDER;
            g_nk.style.text.color=C_MUTED;
            nk_style_push_font(&g_nk,&g_fXs.nk);
            if(nk_button_label(&g_nk,T(S_GP_SCAN))){
                gp_free_ports();
                g_gpPorts=gamepad_get_ports(&g_gpPortCount);
                g_gpPortSel=0;
                g_gpSetup=GPS_SELECT_PORT;
            }
            nk_style_pop_font(&g_nk);
            g_nk.style.button=sb;g_nk.style.text.color=C_TEXT;
        }

        if(g_gpSetup==GPS_SELECT_PORT){
            if(g_gpPortCount==0){
                nk_layout_row_dynamic(&g_nk,22,1);
                nk_style_push_font(&g_nk,&g_fXs.nk);
                g_nk.style.text.color=C_AMBER;
                nk_label(&g_nk,T(S_GP_NO_PORTS),NK_TEXT_LEFT);
                g_nk.style.text.color=C_TEXT;
                nk_style_pop_font(&g_nk);
            } else {
                /* Port radio list */
                nk_layout_row_dynamic(&g_nk,6,1);nk_spacing(&g_nk,1);
                for(int i=0;i<g_gpPortCount;i++){
                    nk_layout_row_begin(&g_nk,NK_STATIC,26,2);
                    nk_layout_row_push(&g_nk,20);
                    {
                        struct nk_rect dr;nk_widget(&dr,&g_nk);
                        struct nk_color dc2=(i==g_gpPortSel)?C_PUR:C_BORDER;
                        nk_fill_circle(nk_window_get_canvas(&g_nk),nk_rect(dr.x+2,dr.y+6,12,12),dc2);
                    }
                    nk_layout_row_push(&g_nk,ww-24);
                    {
                        struct nk_style_button sb2=g_nk.style.button;
                        g_nk.style.button.normal.data.color=(i==g_gpPortSel)?C_CARD2:C_CARD;
                        g_nk.style.button.border_color=C_BORDER;
                        g_nk.style.text.color=(i==g_gpPortSel)?C_TEXT:C_MUTED;
                        nk_style_push_font(&g_nk,&g_fXs.nk);
                        if(nk_button_label(&g_nk,g_gpPorts[i])) g_gpPortSel=i;
                        nk_style_pop_font(&g_nk);
                        g_nk.style.button=sb2;g_nk.style.text.color=C_TEXT;
                    }
                    nk_layout_row_end(&g_nk);
                }
            }
        }

        /* ---- Tick rate config ---- */
        nk_layout_row_dynamic(&g_nk,6,1);nk_spacing(&g_nk,1);
        section_label(T(S_GP_TICK_RATE));

        auto hz_row=[&](const char *lbl, char *buf, int bufsz){
            float lw=tw_xs(lbl)+6.f;
            float fw=ww-lw-4.f;
            nk_layout_row_begin(&g_nk,NK_STATIC,26,2);
            nk_layout_row_push(&g_nk,lw);
            nk_style_push_font(&g_nk,&g_fXs.nk);
            g_nk.style.text.color=C_MUTED;nk_label(&g_nk,lbl,NK_TEXT_LEFT);g_nk.style.text.color=C_TEXT;
            nk_layout_row_push(&g_nk,fw);
            nk_edit_string_zero_terminated(&g_nk,NK_EDIT_FIELD,buf,bufsz,nk_filter_decimal);
            nk_style_pop_font(&g_nk);
            nk_layout_row_end(&g_nk);
        };
        hz_row(T(S_GP_ACTIVE_HZ), eb_gpHz,     sizeof(eb_gpHz));
        hz_row(T(S_GP_IDLE_HZ),   eb_gpIdleHz, sizeof(eb_gpIdleHz));

        nk_layout_row_dynamic(&g_nk,6,1);nk_spacing(&g_nk,1);
        nk_style_push_font(&g_nk,&g_fXs.nk);
        g_nk.style.text.color=C_DIM;
        nk_layout_row_dynamic(&g_nk,16,1);
        nk_label(&g_nk,T(S_GP_HINT_ACTIVE),NK_TEXT_LEFT);
        nk_layout_row_dynamic(&g_nk,16,1);
        nk_label(&g_nk,T(S_GP_HINT_IDLE),NK_TEXT_LEFT);
        g_nk.style.text.color=C_TEXT;
        nk_style_pop_font(&g_nk);

        /* Connect button */
        bool canConnect=(g_gpSetup==GPS_SELECT_PORT && g_gpPortCount>0);
        nk_layout_row_dynamic(&g_nk,8,1);nk_spacing(&g_nk,1);
        nk_layout_row_dynamic(&g_nk,34,1);
        {
            struct nk_style_button sb=g_nk.style.button;
            if(canConnect){
                g_nk.style.button.normal.data.color=C_PUR_D;
                g_nk.style.button.hover.data.color=nk_rgb(90,55,160);
                g_nk.style.button.border_color=C_PUR;
                g_nk.style.text.color=C_TEXT;
            } else {
                g_nk.style.button.normal.data.color=C_CARD;
                g_nk.style.button.border_color=C_BORDER;
                g_nk.style.text.color=C_DIM;
            }
            nk_style_push_font(&g_nk,&g_fSm.nk);
            if(nk_button_label(&g_nk,T(S_GP_CONNECT)) && canConnect){
                int hz=atoi(eb_gpHz);
                if(hz<1||hz>240) hz=120;
                gp_try_connect(g_gpPorts[g_gpPortSel],hz);
                g_cfg.gpHz=hz;
                g_cfg.gpIdleHz=atoi(eb_gpIdleHz);
                if(g_cfg.gpIdleHz<1||g_cfg.gpIdleHz>60) g_cfg.gpIdleHz=10;
                ini_save(g_cfg);
            }
            nk_style_pop_font(&g_nk);
            g_nk.style.button=sb;g_nk.style.text.color=C_TEXT;
        }
        /* 21px bottom padding */
        nk_layout_row_dynamic(&g_nk,21,1);nk_spacing(&g_nk,1);

        /* Quick-reconnect to saved port */
        if((g_gpSetup==GPS_IDLE||g_gpSetup==GPS_FAILED) && g_cfg.gpPort[0]){
            nk_layout_row_dynamic(&g_nk,26,1);
            {
                struct nk_style_button sb=g_nk.style.button;
                g_nk.style.button.normal.data.color=C_CARD2;
                g_nk.style.button.border_color=C_BORDER;
                g_nk.style.text.color=C_MUTED;
                nk_style_push_font(&g_nk,&g_fXs.nk);
                char lbl[80];sprintf_s(lbl,"%s%s @ %d Hz",T(S_GP_RECONNECT),g_cfg.gpPort,g_cfg.gpHz);
                if(nk_button_label(&g_nk,lbl)){
                    int hz=g_cfg.gpHz>0?g_cfg.gpHz:120;
                    sprintf_s(eb_gpHz,"%d",hz);
                    gp_try_connect(g_cfg.gpPort,hz);
                }
                nk_style_pop_font(&g_nk);
                g_nk.style.button=sb;g_nk.style.text.color=C_TEXT;
            }
        }
    }

    /* ================================================================
       CONNECTED: live state + axis flip + stick params + button remap + disconnect
       ================================================================ */
    if(g_gpSetup==GPS_CONNECTED){
        GamepadState st=gp_snapshot();

        section_label(T(S_GP_LIVE));
        nk_style_push_font(&g_nk,&g_fXs.nk);
        {
            float pctX=(float)st.sx/126.f, pctY=(float)st.sy/126.f;
            char stickBuf[80];
            sprintf_s(stickBuf,"%s  X:%+4d (%+.0f%%)   Y:%+4d (%+.0f%%)",
                T(S_GP_STICK),st.sx,(double)(pctX*100.f),st.sy,(double)(pctY*100.f));
            nk_layout_row_dynamic(&g_nk,20,1);
            g_nk.style.text.color=C_PUR_L;
            nk_label(&g_nk,stickBuf,NK_TEXT_LEFT);
            g_nk.style.text.color=C_TEXT;

            char btnBuf[256];
            strcpy_s(btnBuf,T(S_GP_BUTTONS));strcat_s(btnBuf,"  ");
            if(st.pressed_count==0) strcat_s(btnBuf,T(S_GP_CTRL_NONE));
            for(int i=0;i<st.pressed_count;i++){
                if(i>0)strcat_s(btnBuf,"  ");
                strcat_s(btnBuf,st.pressed[i]);
            }
            nk_layout_row_dynamic(&g_nk,20,1);
            g_nk.style.text.color=C_MUTED;
            nk_label(&g_nk,btnBuf,NK_TEXT_LEFT);
            g_nk.style.text.color=C_TEXT;
        }
        nk_style_pop_font(&g_nk);

        /* ---- Axis flip: 4 independent checkboxes ---- */
        section_label(T(S_GP_AXIS));
        nk_style_push_font(&g_nk,&g_fXs.nk);
        /* Row 1: Stick X / Stick Y */
        nk_layout_row_begin(&g_nk,NK_STATIC,24,2);
        nk_layout_row_push(&g_nk,ww/2.f);
        if(nk_checkbox_label(&g_nk,T(S_GP_FLIP_X),&g_cfg.gpFlipX)) ini_save(g_cfg);
        nk_layout_row_push(&g_nk,ww/2.f);
        if(nk_checkbox_label(&g_nk,T(S_GP_FLIP_Y),&g_cfg.gpFlipY)) ini_save(g_cfg);
        nk_layout_row_end(&g_nk);
        /* Row 2: DPad X / DPad Y */
        nk_layout_row_begin(&g_nk,NK_STATIC,24,2);
        nk_layout_row_push(&g_nk,ww/2.f);
        if(nk_checkbox_label(&g_nk,T(S_GP_FLIP_DPAD_X),&g_cfg.gpDpadFlipX)) ini_save(g_cfg);
        nk_layout_row_push(&g_nk,ww/2.f);
        if(nk_checkbox_label(&g_nk,T(S_GP_FLIP_DPAD_Y),&g_cfg.gpDpadFlipY)) ini_save(g_cfg);
        nk_layout_row_end(&g_nk);
        nk_style_pop_font(&g_nk);

        /* ---- Stick smoothing parameters ---- */
        section_label(T(S_GP_SMOOTHING));
        /* Helper: label shows "Name (value unit)" + full-width slider below */
        auto fslider=[&](const char *lbl, float *val, float lo, float hi, const char *unit){
            char fullLbl[80];
            sprintf_s(fullLbl,"%s (%.2f%s)",lbl,*val,unit);
            nk_layout_row_dynamic(&g_nk,16,1);
            nk_style_push_font(&g_nk,&g_fXs.nk);
            g_nk.style.text.color=C_MUTED;
            nk_label(&g_nk,fullLbl,NK_TEXT_LEFT);
            g_nk.style.text.color=C_TEXT;
            nk_style_pop_font(&g_nk);
            nk_layout_row_dynamic(&g_nk,20,1);
            float prev=*val;
            nk_slider_float(&g_nk,lo,val,hi,(hi-lo)/200.f);
            if(*val!=prev) ini_save(g_cfg);
        };
        fslider(T(S_GP_DEADZONE),  &g_cfg.gpDeadzone,  0.f,  40.f, " u");
        fslider(T(S_GP_SPEED_MIN), &g_cfg.gpSpeedMin,  0.f,  10.f, " px");
        fslider(T(S_GP_SPEED_MAX), &g_cfg.gpSpeedMax,  1.f,  80.f, " px");
        fslider(T(S_GP_ACCEL),     &g_cfg.gpAccelMs,  50.f,2000.f, " ms");
        nk_style_push_font(&g_nk,&g_fXs.nk);
        g_nk.style.text.color=C_DIM;
        nk_layout_row_dynamic(&g_nk,15,1);
        nk_label(&g_nk,T(S_GP_SMOOTH_HINT),NK_TEXT_LEFT);
        g_nk.style.text.color=C_TEXT;
        nk_style_pop_font(&g_nk);

        /* ---- Button remap ---- */
        section_label(T(S_GP_REMAP));
        nk_style_push_font(&g_nk,&g_fXs.nk);
        {
            float btnNameW=54.f, comboW=ww-btnNameW-8.f;
            const char* gpa_items[GPA_COUNT];
            for(int i=0;i<GPA_COUNT;i++) gpa_items[i]=GPA_NAMES[i];
            for(int i=0;i<GP_BTN_COUNT;i++){
                nk_layout_row_begin(&g_nk,NK_STATIC,26,2);
                nk_layout_row_push(&g_nk,btnNameW);
                g_nk.style.text.color=C_PUR_L;
                nk_label(&g_nk,GP_BTN_NAMES[i],NK_TEXT_LEFT);
                nk_layout_row_push(&g_nk,comboW);
                g_nk.style.text.color=C_TEXT;
                int prev=g_cfg.gpBtnAction[i];
                g_cfg.gpBtnAction[i]=nk_combo(&g_nk,gpa_items,GPA_COUNT,
                    g_cfg.gpBtnAction[i],22,nk_vec2(comboW,200));
                if(g_cfg.gpBtnAction[i]!=prev) ini_save(g_cfg);
                nk_layout_row_end(&g_nk);
            }
        }
        nk_style_pop_font(&g_nk);

        /* ---- Shared button list for overlay comboboxes ---- */
        static const char* ov_items[GP_BTN_COUNT+1];
        ov_items[0]=T(S_GP_CTRL_NONE);
        for(int i=0;i<GP_BTN_COUNT;i++) ov_items[i+1]=GP_BTN_NAMES[i];
        int ov_combo_count=GP_BTN_COUNT+1;
        const int OV_NONE_IDX=0;

        /* ---- Overlay anchor/confirm buttons ---- */
        section_label(T(S_GP_OV_TITLE));
        nk_style_push_font(&g_nk,&g_fXs.nk);
        g_nk.style.text.color=C_DIM;
        nk_layout_row_dynamic(&g_nk,15,1);
        nk_label(&g_nk,T(S_GP_OV_HINT),NK_TEXT_LEFT);
        g_nk.style.text.color=C_TEXT;
        {
            float slotLW=90.f;
            const char* slotLabels[GP_OV_SLOTS]={T(S_GP_OV_PRIMARY),T(S_GP_OV_SECONDARY)};
            for(int oi=0;oi<GP_OV_SLOTS;oi++){
                nk_layout_row_begin(&g_nk,NK_STATIC,26,2);
                nk_layout_row_push(&g_nk,slotLW);
                g_nk.style.text.color=C_MUTED;
                nk_label(&g_nk,slotLabels[oi],NK_TEXT_LEFT);
                nk_layout_row_push(&g_nk,ww-slotLW-8.f);
                g_nk.style.text.color=C_TEXT;
                int comboIdx=(g_cfg.gpOvBtn[oi]>=0)?g_cfg.gpOvBtn[oi]+1:OV_NONE_IDX;
                int newIdx=nk_combo(&g_nk,ov_items,ov_combo_count,
                    comboIdx,22,nk_vec2(ww-slotLW-8.f,200));
                if(newIdx!=comboIdx){
                    g_cfg.gpOvBtn[oi]=(newIdx==OV_NONE_IDX)?-1:newIdx-1;
                    ini_save(g_cfg);
                }
                nk_layout_row_end(&g_nk);
            }
        }
        g_nk.style.text.color=C_DIM;
        nk_layout_row_dynamic(&g_nk,15,1);
        nk_label(&g_nk,T(S_GP_OV_FOOTER),NK_TEXT_LEFT);
        g_nk.style.text.color=C_TEXT;
        nk_style_pop_font(&g_nk);

        /* ---- Cancel / reset button (remappable) ---- */
        section_label(T(S_GP_OV_CANCEL_TITLE));
        nk_style_push_font(&g_nk,&g_fXs.nk);
        g_nk.style.text.color=C_DIM;
        nk_layout_row_dynamic(&g_nk,15,1);
        nk_label(&g_nk,T(S_GP_OV_CANCEL_HINT),NK_TEXT_LEFT);
        g_nk.style.text.color=C_TEXT;
        {
            float cancelLW=90.f;
            nk_layout_row_begin(&g_nk,NK_STATIC,26,2);
            nk_layout_row_push(&g_nk,cancelLW);
            g_nk.style.text.color=C_MUTED;
            nk_label(&g_nk,T(S_GP_OV_PRIMARY),NK_TEXT_LEFT); /* reuse "Primary:" label */
            nk_layout_row_push(&g_nk,ww-cancelLW-8.f);
            g_nk.style.text.color=C_TEXT;
            /* gpOvBtnCancel: stored as direct index (-1=none fallback=B) */
            int cancelComboIdx=(g_cfg.gpOvBtnCancel>=0)?g_cfg.gpOvBtnCancel+1:1+1; /* +1 for "None" offset; default B=idx1→comboIdx=2 */
            /* Guard bounds */
            if(cancelComboIdx<0||cancelComboIdx>=ov_combo_count) cancelComboIdx=2; /* B */
            int newCancel=nk_combo(&g_nk,ov_items,ov_combo_count,
                cancelComboIdx,22,nk_vec2(ww-cancelLW-8.f,200));
            if(newCancel!=cancelComboIdx){
                g_cfg.gpOvBtnCancel=(newCancel==OV_NONE_IDX)?1:newCancel-1; /* None→B fallback */
                ini_save(g_cfg);
            }
            nk_layout_row_end(&g_nk);
        }
        nk_style_pop_font(&g_nk);

        /* ---- Controls quick-ref (dynamic: shows configured button names) ---- */
        section_label(T(S_GP_CONTROLS));
        nk_style_push_font(&g_nk,&g_fXs.nk);
        {
            /* Build label for anchor/confirm row from configured buttons */
            auto btn_name=[&](int idx)->const char*{
                return (idx>=0&&idx<GP_BTN_COUNT)?GP_BTN_NAMES[idx]:T(S_GP_CTRL_NONE);
            };
            /* Cancel button label */
            const char *cancelName = btn_name(g_cfg.gpOvBtnCancel);

            /* Build dynamic "A / Z  (1st press)" etc. from gpOvBtn */
            char anc1[64], anc2[64];
            int b0=g_cfg.gpOvBtn[0], b1=g_cfg.gpOvBtn[1];
            const char *n0=btn_name(b0);
            if(b1>=0&&b1<GP_BTN_COUNT)
                sprintf_s(anc1,"%s / %s",n0,GP_BTN_NAMES[b1]);
            else
                sprintf_s(anc1,"%s",n0);
            sprintf_s(anc2,"%s",anc1); /* same buttons for 2nd press */

            /* Key column width — wider to fit localized text */
            float kw=180.f;

            /* Row helper */
            auto ctrl_row=[&](const char* key, const char* val){
                nk_layout_row_begin(&g_nk,NK_STATIC,18,2);
                nk_layout_row_push(&g_nk,kw);
                g_nk.style.text.color=C_PUR_L; nk_label(&g_nk,key,NK_TEXT_LEFT);
                nk_layout_row_push(&g_nk,ww-kw-4.f);
                g_nk.style.text.color=C_DIM;   nk_label(&g_nk,val,NK_TEXT_LEFT);
                nk_layout_row_end(&g_nk);
            };

            /* Build localized key labels using the format strings from T() */
            char k1[80],k2[80];
            sprintf_s(k1,sizeof(k1),T(S_GP_CTRL_1ST),anc1);
            sprintf_s(k2,sizeof(k2),T(S_GP_CTRL_2ND),anc2);

            ctrl_row(k1,                  T(S_GP_CTRL_1ST_VAL));
            ctrl_row(k2,                  T(S_GP_CTRL_2ND_VAL));
            ctrl_row(cancelName,          T(S_GP_CTRL_RESET_VAL));
            ctrl_row(cancelName,          T(S_GP_CTRL_EXIT_VAL));
            ctrl_row(T(S_GP_CTRL_STICK),  T(S_GP_CTRL_STICK_VAL));
            ctrl_row(T(S_GP_CTRL_DPAD),   T(S_GP_CTRL_DPAD_VAL));
        }
        g_nk.style.text.color=C_TEXT;
        nk_style_pop_font(&g_nk);

        /* ---- Disconnect ---- */
        nk_layout_row_dynamic(&g_nk,10,1);nk_spacing(&g_nk,1);
        nk_layout_row_dynamic(&g_nk,28,1);
        {
            struct nk_style_button sb=g_nk.style.button;
            g_nk.style.button.normal.data.color=nk_rgb(55,20,20);
            g_nk.style.button.border_color=C_RED;
            g_nk.style.text.color=C_RED;
            nk_style_push_font(&g_nk,&g_fXs.nk);
            if(nk_button_label(&g_nk,T(S_GP_DISCONNECT))){
                gp_thread_stop(); gamepad_shutdown();
                g_gpSetup=GPS_IDLE;
                memset(&g_gpState,0,sizeof(g_gpState));
                memset(&g_gpEdge,0,sizeof(g_gpEdge));
            }
            nk_style_pop_font(&g_nk);
            g_nk.style.button=sb;g_nk.style.text.color=C_TEXT;
        }
        nk_layout_row_dynamic(&g_nk,21,1);nk_spacing(&g_nk,1); /* bottom padding */
    }
}

/* ----  About tab  ---- */
static struct nk_scroll g_tp_scroll={0,0};
static struct nk_scroll g_ap_scroll={0,0};

static void tab_about(){
    float ww=nk_window_get_width(&g_nk)-24.f;

    /* ---- measure "Made by ..." segments ---- */
    const char *mb=T(S_ABOUT_MADEBY),*us=T(S_ABOUT_USING),*an=T(S_ABOUT_AND);
    const char *au="Maxim Bortnikov",*t1="Claude Sonnet 4.6",*t2="Perplexity";
    float wMB=tw_xs(mb),wAU=tw_xs(au),wUS=tw_xs(us);
    float wT1=tw_xs(t1),wAN=tw_xs(an),wT2=tw_xs(t2);
    /* total width with 4px gaps between each segment */
    float creditW=wMB+4+wAU+4+wUS+4+wT1+4+wAN+4+wT2;
    /* hero card height: title(22) + gap(4) + desc(16) + gap(4) + credits(16) + padding(14) */
    float cardH=22+4+16+4+16+14.f;

    /* ---- Hero card ---- */
    nk_layout_row_dynamic(&g_nk,(int)cardH,1);
    struct nk_rect b;nk_widget(&b,&g_nk);
    struct nk_command_buffer*cb=nk_window_get_canvas(&g_nk);
    nk_fill_rect(cb,b,8,C_CARD);nk_stroke_rect(cb,b,8,1,C_BORDER);
    nk_fill_rect(cb,nk_rect(b.x+1,b.y+1,5,b.h-2),4,C_PUR);
    /* App name */
    nk_draw_text(cb,nk_rect(b.x+16,b.y+8,b.w-20,22),
        APP_NAME,(int)strlen(APP_NAME),&g_fTitle.nk,C_CARD,C_TEXT);
    /* Description */
    nk_draw_text(cb,nk_rect(b.x+16,b.y+34,b.w-20,16),
        T(S_ABOUT_DESC),(int)strlen(T(S_ABOUT_DESC)),&g_fXs.nk,C_CARD,C_MUTED);
    /* "Made by ..." drawn inline – left-aligned from card edge */
    float cx=b.x+16.f;
    float cy=b.y+cardH-20.f;  /* bottom of card, 20px up */
    /* Draw plain words */
    nk_draw_text(cb,nk_rect(cx,cy,wMB,16),mb,(int)strlen(mb),&g_fXs.nk,C_CARD,C_MUTED);cx+=wMB+4;
    nk_draw_text(cb,nk_rect(cx,cy,wAU,16),au,(int)strlen(au),&g_fXs.nk,C_CARD,C_PUR_L);cx+=wAU+4;
    nk_draw_text(cb,nk_rect(cx,cy,wUS,16),us,(int)strlen(us),&g_fXs.nk,C_CARD,C_MUTED);cx+=wUS+4;
    nk_draw_text(cb,nk_rect(cx,cy,wT1,16),t1,(int)strlen(t1),&g_fXs.nk,C_CARD,C_PUR_L);cx+=wT1+4;
    nk_draw_text(cb,nk_rect(cx,cy,wAN,16),an,(int)strlen(an),&g_fXs.nk,C_CARD,C_MUTED);cx+=wAN+4;
    nk_draw_text(cb,nk_rect(cx,cy,wT2,16),t2,(int)strlen(t2),&g_fXs.nk,C_CARD,C_PUR_L);
    /* Hover underlines */
    auto check_hover=[&](float lx,float ly,float lw,float lh)->bool{
        POINT pt;GetCursorPos(&pt);
        POINT cl=pt;ScreenToClient(g_hwnd,&cl);
        return (cl.x>=lx&&cl.x<=lx+lw&&cl.y>=ly&&cl.y<=ly+lh);
    };
    float ux=b.x+16+wMB+4; /* start of author */
    if(check_hover(ux,cy,wAU,16))
        nk_stroke_line(cb,ux,cy+15.f,ux+wAU,cy+15.f,1.f,C_PUR_L);
    ux+=wAU+4+wUS+4;        /* Claude */
    if(check_hover(ux,cy,wT1,16))
        nk_stroke_line(cb,ux,cy+15.f,ux+wT1,cy+15.f,1.f,C_PUR_L);
    ux+=wT1+4+wAN+4;        /* Perplexity */
    if(check_hover(ux,cy,wT2,16))
        nk_stroke_line(cb,ux,cy+15.f,ux+wT2,cy+15.f,1.f,C_PUR_L);

    /* ---- Invisible clickable hit areas overlapping the hero card credit line ----
       We use nk_layout_space to position 3 transparent buttons at pixel-exact
       positions matching where the drawn text is inside the card.
       The card ended at "b.y + cardH". The credit line is at cy = b.y+cardH-20.
       We emit a 16px-tall layout_space row directly after the card widget.
       Since the card was consumed by nk_widget, the next row starts right below.
       We reach UP by using a negative y offset inside nk_layout_space.
    */
    nk_layout_space_begin(&g_nk,NK_STATIC,16,3);
    {
        /* Positions relative to the space origin (which starts at b.y+cardH).
           The credit line is 20px above the bottom of the card, so
           dy = -20 relative to the bottom = -(cardH - (cardH-20)) = -20 */
        float dy=-20.f; /* move up into the card */
        float lx0=16+wMB+4;                          /* author start x (relative to card left=b.x) */
        float lx1=lx0+wAU+4+wUS+4;                   /* Claude start */
        float lx2=lx1+wT1+4+wAN+4;                   /* Perplexity start */
        /* nk_layout_space coords are relative to group content origin */

        nk_layout_space_push(&g_nk,nk_rect(lx0, dy, wAU, 16));
        {struct nk_style_button sbb=g_nk.style.button;
         g_nk.style.button.normal.data.color=nk_rgba(0,0,0,0);
         g_nk.style.button.hover.data.color=nk_rgba(0,0,0,0);
         g_nk.style.button.active.data.color=nk_rgba(0,0,0,0);
         g_nk.style.button.border=0;g_nk.style.button.padding=nk_vec2(0,0);
         g_nk.style.text.color=nk_rgba(0,0,0,0);
         nk_style_push_font(&g_nk,&g_fXs.nk);
         if(nk_button_label(&g_nk," "))
             ShellExecuteA(NULL,"open","https://maxim-bortnikov.netlify.app/",NULL,NULL,SW_SHOWNORMAL);
         nk_style_pop_font(&g_nk);g_nk.style.button=sbb;g_nk.style.text.color=C_TEXT;}

        nk_layout_space_push(&g_nk,nk_rect(lx1, dy, wT1, 16));
        {struct nk_style_button sbb=g_nk.style.button;
         g_nk.style.button.normal.data.color=nk_rgba(0,0,0,0);
         g_nk.style.button.hover.data.color=nk_rgba(0,0,0,0);
         g_nk.style.button.active.data.color=nk_rgba(0,0,0,0);
         g_nk.style.button.border=0;g_nk.style.button.padding=nk_vec2(0,0);
         g_nk.style.text.color=nk_rgba(0,0,0,0);
         nk_style_push_font(&g_nk,&g_fXs.nk);
         if(nk_button_label(&g_nk," "))
             ShellExecuteA(NULL,"open","https://claude.ai",NULL,NULL,SW_SHOWNORMAL);
         nk_style_pop_font(&g_nk);g_nk.style.button=sbb;g_nk.style.text.color=C_TEXT;}

        nk_layout_space_push(&g_nk,nk_rect(lx2, dy, wT2, 16));
        {struct nk_style_button sbb=g_nk.style.button;
         g_nk.style.button.normal.data.color=nk_rgba(0,0,0,0);
         g_nk.style.button.hover.data.color=nk_rgba(0,0,0,0);
         g_nk.style.button.active.data.color=nk_rgba(0,0,0,0);
         g_nk.style.button.border=0;g_nk.style.button.padding=nk_vec2(0,0);
         g_nk.style.text.color=nk_rgba(0,0,0,0);
         nk_style_push_font(&g_nk,&g_fXs.nk);
         if(nk_button_label(&g_nk," "))
             ShellExecuteA(NULL,"open","https://perplexity.ai",NULL,NULL,SW_SHOWNORMAL);
         nk_style_pop_font(&g_nk);g_nk.style.button=sbb;g_nk.style.text.color=C_TEXT;}
    }
    nk_layout_space_end(&g_nk);

    /* ---- separator ---- */
    nk_layout_row_dynamic(&g_nk,6,1);nk_spacing(&g_nk,1);
    nk_layout_row_dynamic(&g_nk,1,1);
    {struct nk_rect r;nk_widget(&r,&g_nk);nk_fill_rect(cb,r,0,C_BORDER);}
    nk_layout_row_dynamic(&g_nk,4,1);nk_spacing(&g_nk,1);

    /* ---- Info rows: "Label: Value" compact left-aligned ---- */
    auto info=[&](const char*k,const char*v){
        char kl[128]; sprintf_s(kl,"%s: ",k);
        float klw=tw_xs(kl)+2.f;
        float valw=ww-klw-4.f;
        nk_layout_row_begin(&g_nk,NK_STATIC,18,2);
        nk_layout_row_push(&g_nk,klw);
        nk_style_push_font(&g_nk,&g_fXs.nk);
        g_nk.style.text.color=C_MUTED;nk_label(&g_nk,kl,NK_TEXT_LEFT);
        nk_layout_row_push(&g_nk,valw);
        g_nk.style.text.color=C_PUR_L;nk_label(&g_nk,v,NK_TEXT_LEFT);
        g_nk.style.text.color=C_TEXT;nk_style_pop_font(&g_nk);nk_layout_row_end(&g_nk);
    };
    info("Version",     APP_VERSION);
    info("Platform",    "Windows 10 / 11  (Win32 API)");
    info("UI",          "Nuklear  (MIT / Unlicense)");
    info("Filesystem",  "Boost.Filesystem  (BSL-1.0)");
    info("Images",      "GDI+  (Windows SDK)");
    info("Config",      "makeshift_capture_tool_settings.ini");

    nk_layout_row_dynamic(&g_nk,6,1);nk_spacing(&g_nk,1);

    /* GitHub button */
    nk_layout_row_dynamic(&g_nk,28,1);
    {struct nk_style_button sb=g_nk.style.button;
     g_nk.style.button.normal.data.color=C_PUR_D;g_nk.style.button.border_color=C_PUR;
     g_nk.style.text.color=C_TEXT;nk_style_push_font(&g_nk,&g_fXs.nk);
     if(nk_button_label(&g_nk,T(S_ABOUT_GITHUB)))
         ShellExecuteA(NULL,"open","https://github.com/Northstrix/makeshift-capture-tool-with-n64-controller-support",NULL,NULL,SW_SHOWNORMAL);
     nk_style_pop_font(&g_nk);g_nk.style.button=sb;g_nk.style.text.color=C_TEXT;}

    nk_layout_row_dynamic(&g_nk,4,1);nk_spacing(&g_nk,1);

    /* ---- Third-party Licenses (auto-height scrollable) ---- */
    section_label(T(S_ABOUT_TPLIBS));
    static const char TPLIC[]=
        "------------------------------------------------------------------------------\n"
        "Nuklear is available under 2 licenses\n"
        "------------------------------------------------------------------------------\n"
        "ALTERNATIVE A - MIT License\n"
        "Copyright (c) 2017 Micha Mettke\n"
        "Permission is hereby granted, free of charge, to any person obtaining a copy of\n"
        "this software and associated documentation files (the \"Software\"), to deal in\n"
        "the Software without restriction, including without limitation the rights to\n"
        "use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies\n"
        "of the Software, and to permit persons to whom the Software is furnished to do\n"
        "so, subject to the following conditions:\n"
        "The above copyright notice and this permission notice shall be included in all\n"
        "copies or substantial portions of the Software.\n"
        "THE SOFTWARE IS PROVIDED \"AS IS\", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR\n"
        "IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,\n"
        "FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE\n"
        "AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER\n"
        "LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,\n"
        "OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE\n"
        "SOFTWARE.\n"
        "------------------------------------------------------------------------------\n"
        "ALTERNATIVE B - Public Domain (www.unlicense.org)\n"
        "This is free and unencumbered software released into the public domain.\n"
        "Anyone is free to copy, modify, publish, use, compile, sell, or distribute this\n"
        "software, either in source code form or as a compiled binary, for any purpose,\n"
        "commercial or non-commercial, and by any means.\n"
        "In jurisdictions that recognize copyright laws, the author or authors of this\n"
        "software dedicate any and all copyright interest in the software to the public\n"
        "domain. We make this dedication for the benefit of the public at large and to\n"
        "the detriment of our heirs and successors. We intend this dedication to be an\n"
        "overt act of relinquishment in perpetuity of all present and future rights to\n"
        "this software under copyright law.\n"
        "THE SOFTWARE IS PROVIDED \"AS IS\", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR\n"
        "IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,\n"
        "FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE\n"
        "AUTHORS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN\n"
        "ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION\n"
        "WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.\n"
        "-----------------------------------------------------------------------------\n"
        "\n"
        "Boost Software License - Version 1.0 - August 17th, 2003\n"
        "\n"
        "Permission is hereby granted, free of charge, to any person or organization\n"
        "obtaining a copy of the software and accompanying documentation covered by\n"
        "this license (the \"Software\") to use, reproduce, display, distribute,\n"
        "execute, and transmit the Software, and to prepare derivative works of the\n"
        "Software, and to permit third-parties to whom the Software is furnished to\n"
        "do so, all subject to the following:\n"
        "\n"
        "The copyright notices in the Software and this entire statement, including\n"
        "the above license grant, this restriction and the following disclaimer,\n"
        "must be included in all copies of the Software, in whole or in part, and\n"
        "all derivative works of the Software, unless such copies or derivative\n"
        "works are solely in the form of machine-executable object code generated by\n"
        "a source language processor.\n"
        "\n"
        "THE SOFTWARE IS PROVIDED \"AS IS\", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR\n"
        "IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,\n"
        "FITNESS FOR A PARTICULAR PURPOSE, TITLE AND NON-INFRINGEMENT. IN NO EVENT\n"
        "SHALL THE COPYRIGHT HOLDERS OR ANYONE DISTRIBUTING THE SOFTWARE BE LIABLE\n"
        "FOR ANY DAMAGES OR OTHER LIABILITY, WHETHER IN CONTRACT, TORT OR OTHERWISE,\n"
        "ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER\n"
        "DEALINGS IN THE SOFTWARE.\n"
        "\n"
        "Microsoft GDI+ (Windows SDK)\n"
        "Copyright © Microsoft Corporation. All rights reserved.\n"
        "Use of GDI+ is subject to the Windows SDK License Agreement.\n";
    {int lc=0;for(const char*p=TPLIC;*p;p++)if(*p=='\n')lc++;
     float ah=(float)(lc+1)*17.f+10.f;
     nk_layout_row_dynamic(&g_nk,(int)ah,1);
     if(nk_group_scrolled_begin(&g_nk,&g_tp_scroll,"TpLic",NK_WINDOW_BORDER)){
         nk_layout_row_dynamic(&g_nk,16,1);nk_style_push_font(&g_nk,&g_fXs.nk);
         const char*p=TPLIC;
         while(*p){const char*nl=strchr(p,'\n');int len=nl?(int)(nl-p):(int)strlen(p);
             char lb[200]={};strncpy_s(lb,p,min(len,199));
             g_nk.style.text.color=C_MUTED;nk_label(&g_nk,lb,NK_TEXT_LEFT);
             p+=(nl?(len+1):len);if(!nl)break;}
         g_nk.style.text.color=C_TEXT;nk_style_pop_font(&g_nk);
         nk_group_scrolled_end(&g_nk);}}

    /* ---- App License (auto-height scrollable) ---- */
    nk_layout_row_dynamic(&g_nk,4,1);nk_spacing(&g_nk,1);
    section_label(T(S_ABOUT_LICENSE));
    static const char APPLIC[]=
        "Makeshift Capture Tool with N64 Controller Support\n"
        "Copyright (c) 2026 Maxim Bortnikov\n"
        "MIT License\n"
        "\n"
        "Permission is hereby granted, free of charge, to any person obtaining a copy\n"
        "of this software and associated documentation files (the \"Software\"), to deal\n"
        "in the Software without restriction, including without limitation the rights\n"
        "to use, copy, modify, merge, publish, distribute, sublicense, and/or sell\n"
        "copies of the Software, and to permit persons to whom the Software is\n"
        "furnished to do so, subject to the following conditions:\n"
        "\n"
        "The above copyright notice and this permission notice shall be included in all\n"
        "copies or substantial portions of the Software.\n"
        "\n"
        "THE SOFTWARE IS PROVIDED \"AS IS\", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR\n"
        "IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,\n"
        "FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE\n"
        "AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER\n"
        "LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,\n"
        "OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE\n"
        "SOFTWARE.\n";
    {int lc=0;for(const char*p=APPLIC;*p;p++)if(*p=='\n')lc++;
     float ah=(float)(lc+1)*17.f+10.f;
     nk_layout_row_dynamic(&g_nk,(int)ah,1);
     if(nk_group_scrolled_begin(&g_nk,&g_ap_scroll,"ApLic",NK_WINDOW_BORDER)){
         nk_layout_row_dynamic(&g_nk,16,1);nk_style_push_font(&g_nk,&g_fXs.nk);
         const char*p=APPLIC;
         while(*p){const char*nl=strchr(p,'\n');int len=nl?(int)(nl-p):(int)strlen(p);
             char lb[200]={};strncpy_s(lb,p,min(len,199));
             g_nk.style.text.color=C_MUTED;nk_label(&g_nk,lb,NK_TEXT_LEFT);
             p+=(nl?(len+1):len);if(!nl)break;}
         g_nk.style.text.color=C_TEXT;nk_style_pop_font(&g_nk);
         nk_group_scrolled_end(&g_nk);}}

    nk_layout_row_dynamic(&g_nk,10,1);nk_spacing(&g_nk,1);
}

/* ==================================================================
   FULL FRAME
   ================================================================== */
static void draw_frame(float cw,float ch){
    if(!nk_begin(&g_nk,"Main",nk_rect(0,0,cw,ch),NK_WINDOW_NO_SCROLLBAR)){nk_end(&g_nk);return;}

    /* Title bar */
    nk_layout_row_dynamic(&g_nk,44,1);
    {struct nk_rect tb;nk_widget(&tb,&g_nk);
     struct nk_command_buffer*cb=nk_window_get_canvas(&g_nk);
     nk_fill_rect(cb,tb,0,C_SURF);nk_fill_rect(cb,nk_rect(tb.x,tb.y,4,tb.h),0,C_PUR);
     nk_draw_text(cb,nk_rect(tb.x+16,tb.y+7,tb.w-24,26),
         APP_NAME,(int)strlen(APP_NAME),&g_fTitle.nk,C_SURF,C_TEXT);}

    /* Tab bar */
    const char*tlbls[]={T(S_TAB_CAPTURE),T(S_TAB_SHOT),T(S_TAB_SETTINGS),T(S_TAB_GAMEPAD),T(S_TAB_ABOUT)};
    float tw=(nk_window_get_width(&g_nk)-24)/(float)T_COUNT;
    nk_layout_row_begin(&g_nk,NK_STATIC,30,T_COUNT);
    for(int i=0;i<T_COUNT;i++){
        nk_layout_row_push(&g_nk,tw);
        struct nk_style_button sb=g_nk.style.button;
        g_nk.style.button.rounding=0;g_nk.style.button.border=0;
        if(i==g_tab){
            g_nk.style.button.normal.data.color=C_CARD;
            g_nk.style.button.hover.data.color=C_CARD;
            g_nk.style.button.active.data.color=C_CARD;
            g_nk.style.text.color=C_TEXT;
        } else {
            g_nk.style.button.normal.data.color=C_SURF;
            g_nk.style.button.hover.data.color=C_CARD;
            g_nk.style.button.active.data.color=C_CARD;
            g_nk.style.text.color=C_MUTED;
        }
        nk_style_push_font(&g_nk,&g_fXs.nk);
        if(nk_button_label(&g_nk,tlbls[i]))g_tab=i;
        nk_style_pop_font(&g_nk);g_nk.style.button=sb;g_nk.style.text.color=C_TEXT;
    }
    nk_layout_row_end(&g_nk);
    nk_layout_row_dynamic(&g_nk,2,1);
    {struct nk_rect lr;nk_widget(&lr,&g_nk);
     nk_fill_rect(nk_window_get_canvas(&g_nk),nk_rect(lr.x+g_tab*tw,lr.y,tw,2),0,C_PUR);}

    /* Scrollable content */
    float ch2=ch-44-32-2;
    nk_layout_row_dynamic(&g_nk,(int)ch2,1);
    if(nk_group_scrolled_begin(&g_nk,&g_tabScroll[g_tab],"TC",NK_WINDOW_BORDER)){
        switch(g_tab){
        case T_CAPTURE:  tab_capture();  break;
        case T_SHOT:     tab_shot();     break;
        case T_SETTINGS: tab_settings(); break;
        case T_GAMEPAD:  tab_gamepad();  break;
        case T_ABOUT:    tab_about();    break;
        }
        nk_group_scrolled_end(&g_nk);
    }
    nk_end(&g_nk);
}

/* ==================================================================
   GDI RENDER
   ================================================================== */
static void nk_gdi_render(HDC hdc,RECT*rc){
    g_logoBlit.valid=false;
    HDC mDC=CreateCompatibleDC(hdc);
    HBITMAP mBM=CreateCompatibleBitmap(hdc,rc->right,rc->bottom);
    SelectObject(mDC,mBM);
    HBRUSH bg=CreateSolidBrush(RGB(10,9,16));FillRect(mDC,rc,bg);DeleteObject(bg);

    const struct nk_command*cmd;
    nk_foreach(cmd,&g_nk){
        switch(cmd->type){
        case NK_COMMAND_RECT_FILLED:{
            const struct nk_command_rect_filled*r=(const struct nk_command_rect_filled*)cmd;
            if(r->rounding>0){HBRUSH br=CreateSolidBrush(RGB(r->color.r,r->color.g,r->color.b));
                SelectObject(mDC,br);SelectObject(mDC,GetStockObject(NULL_PEN));
                RoundRect(mDC,(int)r->x,(int)r->y,(int)(r->x+r->w),(int)(r->y+r->h),(int)r->rounding*2,(int)r->rounding*2);DeleteObject(br);}
            else{HBRUSH br=CreateSolidBrush(RGB(r->color.r,r->color.g,r->color.b));
                RECT gr={(int)r->x,(int)r->y,(int)(r->x+r->w),(int)(r->y+r->h)};FillRect(mDC,&gr,br);DeleteObject(br);}
        }break;
        case NK_COMMAND_RECT:{
            const struct nk_command_rect*r=(const struct nk_command_rect*)cmd;
            HPEN p=CreatePen(PS_SOLID,r->line_thickness,RGB(r->color.r,r->color.g,r->color.b));
            SelectObject(mDC,p);SelectObject(mDC,GetStockObject(NULL_BRUSH));
            if(r->rounding>0)RoundRect(mDC,(int)r->x,(int)r->y,(int)(r->x+r->w),(int)(r->y+r->h),(int)r->rounding*2,(int)r->rounding*2);
            else Rectangle(mDC,(int)r->x,(int)r->y,(int)(r->x+r->w),(int)(r->y+r->h));
            DeleteObject(p);}break;
        case NK_COMMAND_CIRCLE_FILLED:{
            const struct nk_command_circle_filled*c=(const struct nk_command_circle_filled*)cmd;
            HBRUSH br=CreateSolidBrush(RGB(c->color.r,c->color.g,c->color.b));
            SelectObject(mDC,br);SelectObject(mDC,GetStockObject(NULL_PEN));
            Ellipse(mDC,(int)c->x,(int)c->y,(int)(c->x+c->w),(int)(c->y+c->h));DeleteObject(br);}break;
        case NK_COMMAND_LINE:{
            const struct nk_command_line*l=(const struct nk_command_line*)cmd;
            HPEN p=CreatePen(PS_SOLID,l->line_thickness,RGB(l->color.r,l->color.g,l->color.b));
            SelectObject(mDC,p);MoveToEx(mDC,(int)l->begin.x,(int)l->begin.y,NULL);
            LineTo(mDC,(int)l->end.x,(int)l->end.y);DeleteObject(p);}break;
        case NK_COMMAND_TEXT:{
            const struct nk_command_text*t=(const struct nk_command_text*)cmd;
            SetBkMode(mDC,TRANSPARENT);SetTextColor(mDC,RGB(t->foreground.r,t->foreground.g,t->foreground.b));
            RECT tr={(int)t->x,(int)t->y,(int)(t->x+t->w),(int)(t->y+t->h)};
            /* Nuklear already positions text at the exact pixel coord it wants.
               Never use DT_CENTER – it re-centres within the rect and shifts everything. */
            DrawTextUTF8(mDC,t->string,t->length,&tr,DT_LEFT|DT_TOP|DT_NOCLIP,
                         ((nk_gdi_font*)t->font->userdata.ptr)->handle);}break;
        case NK_COMMAND_SCISSOR:{
            const struct nk_command_scissor*s=(const struct nk_command_scissor*)cmd;
            HRGN rg=CreateRectRgn((int)s->x,(int)s->y,(int)(s->x+s->w),(int)(s->y+s->h));
            SelectClipRgn(mDC,rg);DeleteObject(rg);}break;
        default:break;
        }
    }
    if(g_logoBlit.valid&&g_logoBlit.hbm){
        HDC src=CreateCompatibleDC(mDC);HBITMAP old2=(HBITMAP)SelectObject(src,g_logoBlit.hbm);
        SetStretchBltMode(mDC,HALFTONE);
        StretchBlt(mDC,g_logoBlit.x,g_logoBlit.y,g_logoBlit.w,g_logoBlit.h,
                   src,0,0,g_logoBlit.sw,g_logoBlit.sh,SRCCOPY);
        SelectObject(src,old2);DeleteDC(src);g_logoBlit.valid=false;
    }
    BitBlt(hdc,0,0,rc->right,rc->bottom,mDC,0,0,SRCCOPY);
    DeleteObject(mBM);DeleteDC(mDC);
}

/* ==================================================================
   WINDOW PROCEDURE
   ================================================================== */
static HDC g_hdc=NULL;

static LRESULT CALLBACK WndProc(HWND hw,UINT msg,WPARAM wp,LPARAM lp){
    switch(msg){
    case WM_GETMINMAXINFO:{MINMAXINFO*mm=(MINMAXINFO*)lp;mm->ptMinTrackSize={400,400};return 0;}

    case WM_SIZE:
        if(wp==SIZE_MINIMIZED&&g_cfg.minimizeToTray){
            wnd_save_placement();
            ShowWindow(hw,SW_HIDE);g_windowVisible=false;
        } else if(wp!=SIZE_MINIMIZED){
            wnd_save_placement();
        }
        return 0;

    case WM_MOVE:
        wnd_save_placement();
        return 0;

    case WM_CLOSE:{
        wnd_save_placement();   /* ensure latest position captured */
        flush_edits();ini_save(g_cfg);
        if(g_cfg.minimizeToTray){ShowWindow(hw,SW_HIDE);g_windowVisible=false;}
        else DestroyWindow(hw);
        return 0;}

    case WM_DESTROY:
        hk_listen_cancel();tray_remove();
        for(int i=0;i<HK_COUNT;i++)UnregisterHotKey(hw,HK_BASE+i);
        wnd_save_placement();flush_edits();ini_save(g_cfg);
        gp_thread_stop();
        gamepad_shutdown();
        gp_free_ports();
        if(g_logoBmp)DeleteObject(g_logoBmp);
        PostQuitMessage(0);return 0;

    case WM_USER+10:
        if(g_listenIdx>=0)hk_apply(g_listenIdx,(int)wp,(int)lp);
        return 0;

    case WM_HOTKEY:{
        int idx=(int)(wp-HK_BASE);
        if(idx>=0&&idx<HK_COUNT)do_screenshot(idx,false);
        return 0;}

    case WM_TRAYICON:
        if(lp==WM_RBUTTONUP||lp==WM_CONTEXTMENU)tray_menu();
        else if(lp==WM_LBUTTONDBLCLK) show_restore_from_tray();
        return 0;

    case WM_COMMAND:
        switch(LOWORD(wp)){
        case ID_TRAY_OPEN:  show_restore_from_tray();break;
        case ID_TRAY_RECT:  do_screenshot(0,true);break;
        case ID_TRAY_FULL:  do_screenshot(1,true);break;
        case ID_TRAY_FIXED: do_screenshot(2,true);break;
        case ID_TRAY_EXIT:  DestroyWindow(hw);break;
        }
        return 0;

    case WM_LBUTTONDOWN:nk_input_button(&g_nk,NK_BUTTON_LEFT, GET_X_LPARAM(lp),GET_Y_LPARAM(lp),1);return 0;
    case WM_LBUTTONUP:  nk_input_button(&g_nk,NK_BUTTON_LEFT, GET_X_LPARAM(lp),GET_Y_LPARAM(lp),0);return 0;
    case WM_RBUTTONDOWN:nk_input_button(&g_nk,NK_BUTTON_RIGHT,GET_X_LPARAM(lp),GET_Y_LPARAM(lp),1);return 0;
    case WM_RBUTTONUP:  nk_input_button(&g_nk,NK_BUTTON_RIGHT,GET_X_LPARAM(lp),GET_Y_LPARAM(lp),0);return 0;
    case WM_MOUSEMOVE:
        nk_input_motion(&g_nk,GET_X_LPARAM(lp),GET_Y_LPARAM(lp));
        return 0;
    case WM_MOUSEWHEEL:
        nk_input_scroll(&g_nk,nk_vec2(0,(float)(GET_WHEEL_DELTA_WPARAM(wp)/WHEEL_DELTA)));return 0;
    case WM_CHAR:nk_input_char(&g_nk,(char)wp);return 0;
    case WM_KEYDOWN:case WM_KEYUP:{
        int dn=(msg==WM_KEYDOWN);
        if(wp==VK_SHIFT) nk_input_key(&g_nk,NK_KEY_SHIFT,dn);
        if(wp==VK_DELETE)nk_input_key(&g_nk,NK_KEY_DEL,dn);
        if(wp==VK_RETURN)nk_input_key(&g_nk,NK_KEY_ENTER,dn);
        if(wp==VK_TAB)   nk_input_key(&g_nk,NK_KEY_TAB,dn);
        if(wp==VK_BACK)  nk_input_key(&g_nk,NK_KEY_BACKSPACE,dn);
        if(wp==VK_LEFT)  nk_input_key(&g_nk,NK_KEY_LEFT,dn);
        if(wp==VK_RIGHT) nk_input_key(&g_nk,NK_KEY_RIGHT,dn);
        if(wp==VK_UP)    nk_input_key(&g_nk,NK_KEY_UP,dn);
        if(wp==VK_DOWN)  nk_input_key(&g_nk,NK_KEY_DOWN,dn);
        if(wp==VK_HOME)  nk_input_key(&g_nk,NK_KEY_TEXT_START,dn);
        if(wp==VK_END)   nk_input_key(&g_nk,NK_KEY_TEXT_END,dn);
        if(GetKeyState(VK_CONTROL)&0x8000){
            if(wp=='A')nk_input_key(&g_nk,NK_KEY_TEXT_SELECT_ALL,dn);
            if(wp=='C')nk_input_key(&g_nk,NK_KEY_COPY,dn);
            if(wp=='V')nk_input_key(&g_nk,NK_KEY_PASTE,dn);
            if(wp=='X')nk_input_key(&g_nk,NK_KEY_CUT,dn);
            if(wp=='Z')nk_input_key(&g_nk,NK_KEY_TEXT_UNDO,dn);
        }
        return 0;}
    case WM_ERASEBKGND:return 1;
    }
    return DefWindowProc(hw,msg,wp,lp);
}

/* ==================================================================
   WINMAIN
   ================================================================== */
int WINAPI WinMain(HINSTANCE hInst,HINSTANCE,LPSTR,int){
    CoInitialize(NULL);SetProcessDPIAware();
    GdiplusStartupInput gsi;ULONG_PTR gdipTok;GdiplusStartup(&gdipTok,&gsi,NULL);
    srand((unsigned)time(NULL));

    load_logo();
    ini_load(g_cfg);
    g_lang=g_cfg.language;
    sprintf_s(eb_gpHz,    "%d", g_cfg.gpHz>0    ? g_cfg.gpHz    : 120);
    sprintf_s(eb_gpIdleHz,"%d", g_cfg.gpIdleHz>0? g_cfg.gpIdleHz: 10);
    try{bfs::create_directories(g_cfg.outputDir);}catch(...){}

    WNDCLASSA wc={0};
    wc.style=CS_HREDRAW|CS_VREDRAW|CS_OWNDC;wc.lpfnWndProc=WndProc;
    wc.hInstance=hInst;wc.hCursor=LoadCursor(NULL,IDC_ARROW);
    wc.hbrBackground=(HBRUSH)GetStockObject(BLACK_BRUSH);
    wc.lpszClassName=APP_CLASS;wc.hIcon=make_icon();
    RegisterClassA(&wc);

    /* Restore last known size; position may be CW_USEDEFAULT on first launch */
    int W=(g_cfg.winW>0)?g_cfg.winW:620;
    int H=(g_cfg.winH>0)?g_cfg.winH:580;
    int X=(g_cfg.winX!=CW_USEDEFAULT)?g_cfg.winX:CW_USEDEFAULT;
    int Y=(g_cfg.winY!=CW_USEDEFAULT)?g_cfg.winY:CW_USEDEFAULT;
    g_hwnd=CreateWindowExA(
        g_cfg.alwaysOnTop?WS_EX_TOPMOST:0,
        APP_CLASS,APP_NAME,WS_OVERLAPPEDWINDOW,
        X,Y,W,H,NULL,NULL,hInst,NULL);

    g_hdc=GetDC(g_hwnd);

    g_fXs   =make_font(13,FW_NORMAL,  "Segoe UI");
    g_fSm   =make_font(15,FW_NORMAL,  "Segoe UI");
    g_fMd   =make_font(17,FW_SEMIBOLD,"Segoe UI");
    g_fTitle=make_font(19,FW_BOLD,    "Segoe UI Semibold");
    patch_fonts();

    nk_init_default(&g_nk,&g_fSm.nk);
    apply_theme();sync_edits();register_hotkeys();tray_add();
    set_status(T(S_READY),"muted");
    vc_init();

    /* Show window at saved state (maximized or normal) */
    if(g_cfg.winMaximized) ShowWindow(g_hwnd,SW_MAXIMIZE);
    else ShowWindow(g_hwnd,SW_SHOW);
    UpdateWindow(g_hwnd);

    /* Snapshot initial placement so first screenshot can restore correctly */
    wnd_save_placement();

    /* Auto-reconnect gamepad if port was saved in INI */
    if(g_cfg.gpPort[0]){
        gp_try_connect(g_cfg.gpPort, g_cfg.gpHz>0?g_cfg.gpHz:60);
    }

    while(1){
        MSG msg;
        nk_input_begin(&g_nk);
        while(PeekMessage(&msg,NULL,0,0,PM_REMOVE)){
            if(msg.message==WM_QUIT)goto cleanup;
            TranslateMessage(&msg);DispatchMessage(&msg);
        }
        nk_input_end(&g_nk);

        /* Process gamepad buttons every frame, even when window is hidden */
        process_gamepad();

        if(!g_windowVisible){Sleep(16);continue;}
        RECT rc;GetClientRect(g_hwnd,&rc);
        float cw=(float)rc.right,ch=(float)rc.bottom;
        if(cw<1||ch<1){Sleep(16);continue;}
        draw_frame(cw,ch);
        nk_gdi_render(g_hdc,&rc);
        nk_clear(&g_nk);
        Sleep(16);
    }
cleanup:
    GdiplusShutdown(gdipTok);
    DeleteObject(g_fXs.handle);DeleteObject(g_fSm.handle);
    DeleteObject(g_fMd.handle);DeleteObject(g_fTitle.handle);
    nk_free(&g_nk);ReleaseDC(g_hwnd,g_hdc);CoUninitialize();
    return 0;
}
