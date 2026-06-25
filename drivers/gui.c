/* ==============================================================
   AETHEROS — GUI Desktop Manager
   ============================================================== */

#include "gui.h"
#include "vesa.h"
#include "mouse.h"
#include "pmm.h"
#include "pit.h"
#include "task.h"
#include "io.h"
#include "font.h"

/* Global terminal buffer */
uint16_t terminal_buffer[80 * 25];

/* ---- All Windows ---- */
static gui_window_t console_window  = {  20,  60, 560, 380, "Aether Console",     1, 0, 0, 0, 0, 0, 0, 0};
static gui_window_t monitor_window  = {600,  60, 380, 380, "System Telemetry",   0, 0, 0, 0, 0, 0, 0, 0};
static gui_window_t files_window    = {  60,  40, 500, 360, "File Manager",       0, 0, 0, 0, 0, 0, 0, 1};
static gui_window_t notepad_window  = { 120,  80, 480, 340, "Notepad",            0, 0, 0, 0, 0, 0, 0, 1};
static gui_window_t calc_window     = { 350, 120, 260, 340, "Calculator",         0, 0, 0, 0, 0, 0, 0, 1};
static gui_window_t settings_window = { 200,  60, 440, 360, "Settings",           0, 0, 0, 0, 0, 0, 0, 1};
static gui_window_t browser_window  = {  30,  30, 620, 400, "Google Chrome",      0, 0, 0, 0, 0, 0, 0, 1};
static gui_window_t wps_window      = {  80,  80, 540, 380, "WPS Writer",         0, 0, 0, 0, 0, 0, 0, 1};
static gui_window_t openoffice_window = {100, 100, 540, 380, "OpenOffice Writer", 0, 0, 0, 0, 0, 0, 0, 1};
static gui_window_t installer_window = {240, 150, 400, 300, "AetherSetup Wizard", 0, 0, 0, 0, 0, 0, 0, 1};

#define MAX_WINS 10
static gui_window_t* window_order[MAX_WINS];

static void gui_fill_rounded(int x, int y, int w, int h, uint32_t col);
static void format_stat(char* b, const char* label, uint32_t val, const char* suffix);
static void init_snake_game(void);
static void gui_draw_snake_game(int mx, int my);
static void read_rtc(int *second, int *minute, int *hour, int *day, int *month, int *year);
static void get_live_clock_string(char* time_buf, char* date_buf);
static void get_lockscreen_date_string(char* buf);
static void settings_change_password(void);

/* Bring a window to top of Z-order */
static void bring_to_front(gui_window_t* win) {
    /* Deactivate all, find win, pull it to last slot (drawn on top) */
    int idx = -1;
    for (int i = 0; i < MAX_WINS; i++) {
        window_order[i]->active = 0;
        if (window_order[i] == win) idx = i;
    }
    if (idx >= 0) {
        /* shift everything after idx one slot left */
        for (int i = idx; i < MAX_WINS - 1; i++)
            window_order[i] = window_order[i + 1];
        window_order[MAX_WINS - 1] = win;
    }
    win->active = 1;
}

/* Dialog boxes */
typedef struct {
    int x, y, w, h;
    const char* title;
    const char* message;
    int show;
} gui_dialog_t;

static gui_dialog_t info_dialog = {340, 210, 340, 220, "About AetherOS",
    "AetherOS-64 v1.2\n-----------------\n64-bit Freestanding Kernel\nVESA LFB Graphics Driver\nPS/2 Interrupt Mouse (IRQ12)\nPreemptive Multitasking\nCustom HUD Shell", 0};
static gui_dialog_t trash_dialog = {380, 260, 280, 120, "Recycle Bin",
    "Recycle Bin is empty.\nNo files found on device.", 0};

/* Thread metrics */
extern volatile uint32_t thread1_ticks;
extern volatile uint32_t thread2_ticks;

/* CPU wave graph */
#define WAVE_POINTS 80
static int wave_history[WAVE_POINTS];
static int wave_index = 0;

/* Drag state */
static int drag_offset_x = 0;
static int drag_offset_y = 0;
static gui_window_t* dragging_win = 0;
static uint8_t prev_buttons = 0;

/* State machine */
typedef enum { GUI_STATE_LOCKSCREEN, GUI_STATE_LOGINSCREEN, GUI_STATE_DESKTOP } gui_state_t;
static gui_state_t gui_state = GUI_STATE_LOCKSCREEN;

static char password_buf[32];
static int password_len = 0;
static int login_failed = 0;
static int start_menu_open = 0;

/* Context Menu State */
typedef struct {
    int active;
    int x, y;
    int type; // 0 = Desktop empty space, 1 = File Explorer file, 2 = File Explorer empty space
    int target_idx; // index in file_system if type == 1
    int sub_active; // 0 or 1
    int sub_type; // 2 = New document types
} context_menu_t;
static context_menu_t ctx_menu = {0, 0, 0, 0, -1, 0, 0};

/* Installation States & Installer Wizard */
static int wps_installed = 0;
static int openoffice_installed = 0;
static int chrome_installed = 0;
static int installer_step = 0; // 0=welcome, 1=installing, 2=complete
static int installer_app_type = 0; // 0=WPS, 1=OpenOffice, 2=Chrome
static int installer_progress = 0; // 0 to 100
static uint64_t installer_last_tick = 0;

/* System Passcode & Settings variables */
static char system_password[32] = "aether";
static char settings_cur_pass[32] = "";
static int settings_cur_pass_len = 0;
static char settings_new_pass[32] = "";
static int settings_new_pass_len = 0;
static char settings_pass_status[64] = "";

/* Focus management */
typedef enum {
    FOCUS_NONE,
    FOCUS_NOTEPAD,
    FOCUS_FILES_SEARCH,
    FOCUS_BROWSER_URL,
    FOCUS_BROWSER_SEARCH,
    FOCUS_START_SEARCH,
    FOCUS_SETTINGS_SEARCH,
    FOCUS_SETTINGS_CUR_PASS,
    FOCUS_SETTINGS_NEW_PASS,
    FOCUS_NOTEPAD_SAVE_PATH,
    FOCUS_NOTEPAD_SAVE_NAME
} gui_focus_t;
static gui_focus_t gui_focus = FOCUS_NONE;

/* File entry structure */
typedef struct {
    char name[32];
    int size_kb;
    int is_dir;
    int dir_id;      /* 0=root, 1=bin, 2=boot, 3=dev, 4=etc, 5=home, 99=Recycle Bin */
    int file_type;   /* 0=text, 1=exe, 2=sys */
    char content[256];
    int deleted;
} file_entry_t;

static int is_executable_file(file_entry_t* f);

#define MAX_FILES 32
static file_entry_t file_system[MAX_FILES];
static int file_system_count = 0;
static int files_selected_idx = -1;
static char files_search_query[32];
static int files_search_len = 0;

/* Browser state */
static int browser_active_tab = 0; /* 0=Google, 1=Home, 2=Wiki */
static char browser_url[128];
static int browser_url_len = 0;
static char browser_search_query[64];
static int browser_search_len = 0;
static int browser_has_searched = 0;
static char browser_last_search[64];

/* Browser navigation */
static int browser_youtube_mode = 0;  /* 0=normal, 1=youtube page, 2=yt video playing */
static char browser_yt_search[64];
static int browser_yt_search_len = 0;
static int browser_yt_hovered = -1;
static int browser_can_go_back = 0;
static int browser_loading = 0;
static int browser_load_ticks = 0;

/* Audio / video playback simulation */
static int audio_playing = 0;
static int audio_video_idx = 0;
static char audio_title[64];
static int audio_volume = 80;  /* 0-100 */


/* Settings state */
static int settings_active_category = 0; /* 0=System, 1=Personalization, 2=Network, 3=Security, 4=About */
static int settings_update_stage = 0; /* 0=idle, 1=checking, 2=downloading, 3=installing, 4=completed */
static int settings_update_progress = 0;
static int settings_wifi_enabled = 1;
static int taskbar_centered = 1;
static int wallpaper_style = 0; /* 0=Grid, 1=Void, 2=Matrix Rain */
static int taskbar_position = 0; // 0 = Top, 1 = Bottom
static int active_workspace = 0; // 0 or 1
static int quick_settings_open = 0;
static int night_light_active = 0;
static int bluetooth_enabled = 0;
static int system_volume = 70; // 0 to 100
static int performance_mode = 1; // 0 = Eco, 1 = Balanced, 2 = Turbo
static int lockscreen_style = 0; // 0 = Default HUD, 1 = Cyber Glitch, 2 = Clean Matrix

typedef struct {
    int x, y;
    int minimized;
    int closed;
} window_state_t;

typedef struct {
    window_state_t wins[MAX_WINS];
} workspace_t;

static workspace_t workspaces[2];
static gui_window_t* all_windows[MAX_WINS] = {
    &console_window,
    &monitor_window,
    &files_window,
    &notepad_window,
    &calc_window,
    &settings_window,
    &browser_window,
    &wps_window,
    &openoffice_window,
    &installer_window
};

static void save_workspace(int ws) {
    if (ws < 0 || ws > 1) return;
    for (int i = 0; i < MAX_WINS; i++) {
        workspaces[ws].wins[i].x = all_windows[i]->x;
        workspaces[ws].wins[i].y = all_windows[i]->y;
        workspaces[ws].wins[i].minimized = all_windows[i]->minimized;
        workspaces[ws].wins[i].closed = all_windows[i]->closed;
    }
}

static void load_workspace(int ws) {
    if (ws < 0 || ws > 1) return;
    for (int i = 0; i < MAX_WINS; i++) {
        all_windows[i]->x = workspaces[ws].wins[i].x;
        all_windows[i]->y = workspaces[ws].wins[i].y;
        all_windows[i]->minimized = workspaces[ws].wins[i].minimized;
        all_windows[i]->closed = workspaces[ws].wins[i].closed;
    }
    active_workspace = ws;
}

static char settings_search_query[32];
static int settings_search_len = 0;

static char start_search_query[32];
static int start_search_len = 0;

/* Browser Download State */
static int browser_download_active = 0;
static int browser_download_progress = 0;
static int browser_download_finished = 0;
static char browser_download_filename[32];
static int browser_download_type = 0; /* 0=text, 1=exe */
static int browser_download_size = 0;
static const char* browser_download_content_ref = 0;

/* Snake Game State */
static int snake_active = 0;
static int snake_x[32];
static int snake_y[32];
static int snake_len = 3;
static int snake_dir = 1; /* 0=up, 1=right, 2=down, 3=left */
static int snake_food_x = 5;
static int snake_food_y = 5;
static uint64_t snake_last_tick = 0;
static int snake_score = 0;
static int snake_game_over = 0;

/* Substring search matching helper */
static int str_contains_nocase(const char* haystack, const char* needle) {
    if (!needle || needle[0] == '\0') return 1;
    for (int i = 0; haystack[i] != '\0'; i++) {
        int match = 1;
        for (int j = 0; needle[j] != '\0'; j++) {
            if (haystack[i + j] == '\0') { match = 0; break; }
            char hc = haystack[i + j];
            char nc = needle[j];
            if (hc >= 'A' && hc <= 'Z') hc = hc - 'A' + 'a';
            if (nc >= 'A' && nc <= 'Z') nc = nc - 'A' + 'a';
            if (hc != nc) { match = 0; break; }
        }
        if (match) return 1;
    }
    return 0;
}

static void add_mock_file(const char* name, int is_dir, int file_type, int dir_id, int size_kb, const char* content) {
    if (file_system_count >= MAX_FILES) return;
    file_entry_t* f = &file_system[file_system_count];
    int i = 0;
    for (; name[i] != '\0' && i < 31; i++) f->name[i] = name[i];
    f->name[i] = '\0';
    f->is_dir = is_dir;
    f->file_type = file_type;
    f->dir_id = dir_id;
    f->size_kb = size_kb;
    f->deleted = 0;
    
    i = 0;
    if (content) {
        for (; content[i] != '\0' && i < 255; i++) f->content[i] = content[i];
    }
    f->content[i] = '\0';
    file_system_count++;
}

typedef struct {
    const char* n;
    int sh;
    uint32_t col;
} start_app_entry_t;

static start_app_entry_t start_apps[12] = {
    {"PC",     0, 0x00FF88},
    {"Shell",  1, 0x00FF88},
    {"Mon",    2, 0x44FF99},
    {"Trash",  3, 0xFF3333},
    {"Files",  4, 0xFFAA00},
    {"Notes",  5, 0x88DDFF},
    {"Calc",   6, 0xFF88FF},
    {"Chrome", 8, 0x88DDFF},
    {"Cfg",    7, 0xFFFF44},
    {"Reboot", 3, 0xFF3333},
    {"WPS",    5, 0x0088FF},
    {"OpenOff",5, 0x00DDFF}
};

/* Interactive Apps State */
static char notepad_buf[1024];
static int notepad_len = 0;
static int notepad_editing_idx = -1;
static char info_dialog_msg_buf[256];

/* Notepad Save Dialog State */
static int notepad_save_dialog_open = 0;
static char notepad_save_path[64] = "/home/documents";
static int notepad_save_path_len = 15;
static char notepad_save_name[32] = "notes.txt";
static int notepad_save_name_len = 9;
static int notepad_save_focus = 0; // 0 = path, 1 = name
static char notepad_save_content_buf[1024];

static char calc_buf[32];
static int calc_len = 1;
static int calc_value1 = 0;
static char calc_op = '\0';
static int calc_clear_on_next = 0;

static int video_playing = 0;
static int video_fullscreen = 0;
static int video_play_pos = 0;
static int video_is_playing = 1;
static int video_active_idx = 0;
static int browser_video_selected_idx = -1;

static int get_folder_dir_id(file_entry_t* f, int idx) {
    const char* dir_names[] = {"", "bin", "boot", "dev", "etc", "home", "downloads", "desktop", "documents", "pictures", "music", "videos", "Local Disk (C:)"};
    int dir_ids[] = {0, 1, 2, 3, 4, 5, 6, 28, 29, 30, 31, 32, 0};
    for (int d = 1; d <= 12; d++) {
        int match_name = 1;
        for (int c = 0; f->name[c] != '\0' || dir_names[d][c] != '\0'; c++) {
            if (f->name[c] != dir_names[d][c]) { match_name = 0; break; }
        }
        if (match_name) return dir_ids[d];
    }
    return idx + 10;
}


/* Forward declarations needed by draw_ascii_frame (defined later) */
static uint32_t gui_accent_col = 0x00FF88;
static void draw_string_scaled(const char* str, int x, int y, int scale, uint32_t color);

static void draw_ascii_frame(int x, int y, int w, int h, int video_idx, uint64_t ticks, int scale) {
    if (video_idx == 0) {
        int spacing = 16 * scale;
        int bar_w = 8 * scale;
        int start_x = x + (w - 16 * spacing) / 2;
        int max_h = h - 20;
        for (int bar = 0; bar < 16; bar++) {
            int phase = (ticks / 3 + bar * 4) % 12;
            int bh = (phase > 6 ? (12 - phase) : phase) * (max_h / 6);
            if (bh < 4) bh = 4;
            if (!video_is_playing) bh = 4;
            int bx = start_x + bar * spacing;
            int by = y + h - bh - 10;
            vesa_draw_rect(bx, by, bar_w, bh, gui_accent_col);
            vesa_draw_rect_outline(bx, by, bar_w, bh, 0xFFFFFF);
        }
        draw_string_scaled("LOFI STREAMING...", x + (w - 17 * 8 * scale) / 2, y + 10, scale, 0x00FF88);
    } else if (video_idx == 1) {
        int radius = (int)((ticks / 2) % 30) + 10;
        if (!video_is_playing) radius = 25;
        int cx = x + w / 2;
        int cy = y + h / 2;
        vesa_draw_rect_outline(cx - radius * scale, cy - radius * scale, radius * 2 * scale, radius * 2 * scale, gui_accent_col);
        vesa_draw_rect_outline(cx - (radius/2) * scale, cy - (radius/2) * scale, radius * scale, radius * scale, 0x00AA55);
        draw_string_scaled("AETHER OS 64", x + (w - 12 * 8 * scale) / 2, cy - 4 * scale, scale, 0xFFFFFF);
    } else {
        const char* code_lines[] = {
            "mov rax, 0x1000",
            "mov cr3, rax",
            "push rbp",
            "mov rbp, rsp",
            "sub rsp, 32",
            "call kernel_main",
            "cli",
            "hlt",
            "jmp $",
            "int 0x20",
            "iretq",
            "pop rbp",
            "ret"
        };
        int start_line = (int)(ticks / 8) % 13;
        if (!video_is_playing) start_line = 0;
        int draw_y = y + 10;
        for (int row = 0; row < 6; row++) {
            if (draw_y + 10 * scale >= y + h) break;
            int line_idx = (start_line + row) % 13;
            draw_string_scaled(code_lines[line_idx], x + 10, draw_y, scale, 0x00FF88);
            draw_y += 12 * scale;
        }
    }
}

static void gui_draw_fullscreen_video(int mx, int my) {
    int w = vesa_get_width();
    int h = vesa_get_height();
    uint64_t ticks = pit_get_ticks();
    vesa_draw_rect(0, 0, w, h, 0x000000);
    draw_ascii_frame(20, 20, w - 40, h - 120, video_active_idx, ticks, 4);
    int ctrl_y = h - 60;
    vesa_draw_rect(0, ctrl_y, w, 60, 0x050E07);
    vesa_draw_line(0, ctrl_y, w, ctrl_y, gui_accent_col);
    int sb_x = (w - 800) / 2;
    int sb_y = ctrl_y + 10;
    int sb_w = 800;
    int sb_h = 8;
    vesa_draw_rect(sb_x, sb_y, sb_w, sb_h, 0x001105);
    vesa_draw_rect_outline(sb_x, sb_y, sb_w, sb_h, 0x005522);
    int fill_w = (sb_w * video_play_pos) / 100;
    vesa_draw_rect(sb_x, sb_y, fill_w, sb_h, gui_accent_col);
    vesa_draw_rect(sb_x + fill_w - 4, sb_y - 2, 8, 12, 0xFFFFFF);
    int cur_y = ctrl_y + 26;
    char time_str[32];
    int total_seconds = (video_active_idx == 0) ? 220 : ((video_active_idx == 1) ? 75 : 150);
    int cur_seconds = (video_play_pos * total_seconds) / 100;
    int cur_min = cur_seconds / 60;
    int cur_sec = cur_seconds % 60;
    int tot_min = total_seconds / 60;
    int tot_sec = total_seconds % 60;
    format_stat(time_str, "Time: 0", cur_min, "");
    int t_len = 0;
    while (time_str[t_len]) t_len++;
    time_str[t_len++] = ':';
    time_str[t_len++] = '0' + (cur_sec / 10);
    time_str[t_len++] = '0' + (cur_sec % 10);
    time_str[t_len++] = ' ';
    time_str[t_len++] = '/';
    time_str[t_len++] = ' ';
    time_str[t_len++] = '0' + tot_min;
    time_str[t_len++] = ':';
    time_str[t_len++] = '0' + (tot_sec / 10);
    time_str[t_len++] = '0' + (tot_sec % 10);
    time_str[t_len] = '\0';
    vesa_draw_string(time_str, sb_x, cur_y + 4, 0x00FF88);
    int ctrl_x = sb_x + 400;
    int hov_play = (mx >= ctrl_x && mx < ctrl_x + 80 && my >= cur_y && my < cur_y + 20);
    vesa_draw_string(video_is_playing ? "[Pause]" : "[Play] ", ctrl_x, cur_y + 4, hov_play ? 0xFFFFFF : 0x00AA55);
    vesa_draw_string("[Exit Fullscreen]", ctrl_x + 90, cur_y + 4, (mx >= ctrl_x + 90 && mx < ctrl_x + 230 && my >= cur_y && my < cur_y + 20) ? 0xFFFFFF : 0x00AA55);
}

static void gui_handle_fullscreen_video_click(int mx, int my) {
    int w = vesa_get_width();
    int h = vesa_get_height();
    int ctrl_y = h - 60;
    int sb_x = (w - 800) / 2;
    int sb_y = ctrl_y + 10;
    if (mx >= sb_x && mx < sb_x + 800 && my >= sb_y && my < sb_y + 8) {
        video_play_pos = ((mx - sb_x) * 100) / 800;
        return;
    }
    int cur_y = ctrl_y + 26;
    if (my >= cur_y && my < cur_y + 20) {
        int ctrl_x = sb_x + 400;
        if (mx >= ctrl_x && mx < ctrl_x + 80) {
            video_is_playing = !video_is_playing;
        } else if (mx >= ctrl_x + 90 && mx < ctrl_x + 230) {
            video_fullscreen = 0;
        }
    }
}

static int files_current_dir = 0;
static int browser_page = 0;
/* gui_accent_col declared above draw_ascii_frame */

void gui_init(void) {
    for (int i = 0; i < 80 * 25; i++) terminal_buffer[i] = (0x07 << 8) | ' ';
    for (int i = 0; i < WAVE_POINTS; i++) wave_history[i] = 0;
    wave_index = 0;

    /* Z-order: index 0 = bottom, MAX_WINS-1 = top */
    window_order[0] = &browser_window;
    window_order[1] = &settings_window;
    window_order[2] = &calc_window;
    window_order[3] = &notepad_window;
    window_order[4] = &files_window;
    window_order[5] = &monitor_window;
    window_order[6] = &console_window;
    window_order[7] = &wps_window;
    window_order[8] = &openoffice_window;
    window_order[9] = &installer_window;

    dragging_win = 0;
    prev_buttons = 0;
    start_menu_open = 0;
    info_dialog.show = 0;
    trash_dialog.show = 0;
    gui_state = GUI_STATE_LOCKSCREEN;
    password_len = 0;
    login_failed = 0;

    /* Initialize interactive app buffers */
    const char* default_note = "AetherOS system notes.\nType anything here to write notes!\n";
    notepad_len = 0;
    while (default_note[notepad_len]) {
        notepad_buf[notepad_len] = default_note[notepad_len];
        notepad_len++;
    }
    notepad_buf[notepad_len] = '\0';

    calc_buf[0] = '0';
    calc_buf[1] = '\0';
    calc_len = 1;
    calc_value1 = 0;
    calc_op = '\0';
    calc_clear_on_next = 0;

    files_current_dir = 100; // Default to "This PC" at startup!
    browser_page = 0;
    gui_accent_col = 0x00FF88;
    gui_focus = FOCUS_NONE;

    settings_search_query[0] = '\0';
    settings_search_len = 0;
    start_search_query[0] = '\0';
    start_search_len = 0;

    browser_download_active = 0;
    browser_download_progress = 0;
    browser_download_finished = 0;
    snake_active = 0;

    /* Initialize Mock File System */
    file_system_count = 0;
    add_mock_file("bin",       1, 2, 0, 0, 0);
    add_mock_file("boot",      1, 2, 0, 0, 0);
    add_mock_file("dev",       1, 2, 0, 0, 0);
    add_mock_file("etc",       1, 2, 0, 0, 0);
    /* home dir removed: desktop/documents/etc moved to root level (dir_id=0) */
        add_mock_file("downloads", 1, 2, 0, 0, 0);
    
    add_mock_file("reboot",  0, 1, 1, 4, 0);
    add_mock_file("sysinfo", 0, 1, 1, 8, 0);
    add_mock_file("shell",   0, 1, 1, 12, 0);
    
    add_mock_file("grub.cfg",   0, 0, 2, 1, "set gfxmode=1024x768x32\nset gfxpayload=keep\n\nmenuentry 'AetherOS-64' {\n  multiboot2 /boot/aetheros.bin\n}\n");
    add_mock_file("kernel.sys", 0, 2, 2, 512, "ELF64 binary data header\n");
    
    add_mock_file("mouse",    0, 2, 3, 0, 0);
    add_mock_file("keyboard", 0, 2, 3, 0, 0);
    add_mock_file("vesa",     0, 2, 3, 0, 0);
    
    add_mock_file("version.txt", 0, 0, 4, 1, "AetherOS-64 v1.2 (Freestanding kernel)\n");
    add_mock_file("hosts",       0, 0, 4, 1, "127.0.0.1 localhost\n192.168.1.1 gateway\n");
    
    add_mock_file("welcome.txt", 0, 0, 29, 2, "Welcome to AetherOS-64!\nDouble click any text file to open it in Notepad.\nDouble click Chrome Setup.exe to install Chrome.");
    add_mock_file("todo.txt",    0, 0, 29, 1, "- Optimize GUI frame rates\n- Add sound driver\n- Add TCP/IP stack\n");

    add_mock_file("desktop",   1, 2, 0, 0, 0);
    add_mock_file("documents", 1, 2, 0, 0, 0);
    add_mock_file("pictures",  1, 2, 0, 0, 0);
    add_mock_file("music",     1, 2, 0, 0, 0);
    add_mock_file("videos",    1, 2, 0, 0, 0);

    add_mock_file("desktop_welcome.txt", 0, 0, 28, 1, "Welcome to AetherOS-64 Desktop!\nCreate files here by right-clicking on empty desktop space!\n");
    add_mock_file("welcome.txt",         0, 0, 29, 2, "Welcome to AetherOS-64!\nDouble click any text file to open it in Notepad.\nDouble click Chrome Setup.exe to install Chrome.");
    add_mock_file("todo.txt",            0, 0, 29, 1, "- Optimize GUI frame rates\n- Add sound driver\n- Add TCP/IP stack\n");
    add_mock_file("Chrome Setup.exe",    0, 1, 28, 2048, "Google Chrome Browser Installer\nDouble click to begin installation.\n");
    add_mock_file("Chrome Setup.exe",    0, 1, 6, 2048, "Google Chrome Browser Installer\nDouble click to begin installation.\n");
    add_mock_file("wps_setup.exe",        0, 1, 6, 4096, "WPS Office Setup Wizard\nDouble click to begin installation.\n");
    add_mock_file("openoffice_setup.exe", 0, 1, 6, 2048, "OpenOffice 4.1 Setup Wizard\nDouble click to begin installation.\n");
    add_mock_file("Local Disk (C:)", 1, 2, 100, 0, 0);

    /* Initialize workspaces */
    for (int ws = 0; ws < 2; ws++) {
        for (int i = 0; i < MAX_WINS; i++) {
            workspaces[ws].wins[i].x = all_windows[i]->x;
            workspaces[ws].wins[i].y = all_windows[i]->y;
            workspaces[ws].wins[i].minimized = (ws == 0) ? all_windows[i]->minimized : 1;
            workspaces[ws].wins[i].closed = all_windows[i]->closed;
        }
    }
    active_workspace = 0;
    taskbar_position = 0; // Default to Top to prevent VirtualBox bottom cut-off
    quick_settings_open = 0;
    night_light_active = 0;
    bluetooth_enabled = 0;
    system_volume = 70;
    performance_mode = 1;
    lockscreen_style = 0;
}

static void init_snake_game(void) {
    snake_len = 3;
    snake_x[0] = 10; snake_y[0] = 10;
    snake_x[1] = 9;  snake_y[1] = 10;
    snake_x[2] = 8;  snake_y[2] = 10;
    snake_dir = 1;
    snake_score = 0;
    snake_game_over = 0;
    snake_active = 1;
    snake_last_tick = pit_get_ticks();
    
    uint64_t ticks = pit_get_ticks();
    snake_food_x = (int)(ticks % 18) + 1;
    snake_food_y = (int)((ticks / 3) % 18) + 1;
}

static void gui_draw_snake_game(int mx, int my) {
    int sw = 320, sh = 350;
    int sx = (1024 - sw) / 2;
    int sy = (768 - sh) / 2;
    
    gui_fill_rounded(sx, sy, sw, sh, 0x050E07);
    vesa_draw_rect_outline(sx, sy, sw, sh, gui_accent_col);
    
    vesa_draw_rect(sx + 1, sy + 1, sw - 2, 24, 0x121B14);
    vesa_draw_string("Aether Snake Arcade", sx + 8, sy + 8, gui_accent_col);
    
    int close_x = sx + sw - 20;
    int close_y = sy + 6;
    int hov_close = (mx >= close_x && mx < close_x + 12 && my >= close_y && my < close_y + 12);
    vesa_draw_rect(close_x, close_y, 12, 12, hov_close ? 0xFF3333 : 0x441111);
    vesa_draw_rect_outline(close_x, close_y, 12, 12, hov_close ? 0xFFFFFF : 0x880000);
    
    int board_x = sx + 40;
    int board_y = sy + 40;
    vesa_draw_rect(board_x, board_y, 240, 240, 0x020A05);
    vesa_draw_rect_outline(board_x, board_y, 240, 240, 0x1A4A2A);
    
    for (int y = 0; y < 20; y++) {
        for (int x = 0; x < 20; x++) {
            vesa_put_pixel(board_x + x * 12 + 6, board_y + y * 12 + 6, 0x071D0F);
        }
    }
    
    if (snake_game_over) {
        vesa_draw_rect(board_x + 20, board_y + 80, 200, 80, 0x150505);
        vesa_draw_rect_outline(board_x + 20, board_y + 80, 200, 80, 0xFF3333);
        vesa_draw_string("GAME OVER", board_x + 80, board_y + 96, 0xFF3333);
        
        char score_str[32];
        format_stat(score_str, "Score: ", snake_score, "");
        vesa_draw_string(score_str, board_x + 88, board_y + 114, 0xFFFFFF);
        vesa_draw_string("[Press SPACE to restart]", board_x + 28, board_y + 136, 0x88DDFF);
    } else {
        for (int i = 1; i < snake_len; i++) {
            int bx = board_x + snake_x[i] * 12 + 1;
            int by = board_y + snake_y[i] * 12 + 1;
            vesa_draw_rect(bx, by, 10, 10, 0x00AA55);
            vesa_draw_rect_outline(bx, by, 10, 10, 0x004422);
        }
        
        int hx = board_x + snake_x[0] * 12 + 1;
        int hy = board_y + snake_y[0] * 12 + 1;
        vesa_draw_rect(hx, hy, 10, 10, gui_accent_col);
        vesa_draw_rect_outline(hx, hy, 10, 10, 0xFFFFFF);
        
        int fx = board_x + snake_food_x * 12 + 2;
        int fy = board_y + snake_food_y * 12 + 2;
        vesa_draw_rect(fx, fy, 8, 8, 0xFF3333);
        vesa_draw_rect_outline(fx, fy, 8, 8, 0xFFFFFF);
    }
    
    char score_lbl[32];
    format_stat(score_lbl, "Score: ", snake_score, "");
    vesa_draw_string(score_lbl, board_x, board_y + 248, 0x88DDFF);
    vesa_draw_string("Controls: WASD", board_x + 110, board_y + 248, 0x00AA55);
    
    int ctrl_y = board_y + 270;
    const char* arrows[] = {" ^ ", " < ", " v ", " > "};
    int btn_w = 32;
    int btn_h = 24;
    int btn_xs[] = {board_x + 92, board_x + 50, board_x + 92, board_x + 134};
    int btn_ys[] = {ctrl_y, ctrl_y + 20, ctrl_y + 20, ctrl_y + 20};
    
    for (int i = 0; i < 4; i++) {
        int bx = btn_xs[i];
        int by = btn_ys[i];
        int hov = (mx >= bx && mx < bx + btn_w && my >= by && my < by + btn_h);
        vesa_draw_rect(bx, by, btn_w, btn_h, hov ? 0x0A2010 : 0x020A05);
        vesa_draw_rect_outline(bx, by, btn_w, btn_h, hov ? gui_accent_col : 0x1A4A2A);
        vesa_draw_string(arrows[i], bx + 4, by + 8, hov ? gui_accent_col : 0x00AA55);
    }
}

/* gui_reboot: Hardware reboot via the keyboard controller */
static void gui_reboot(void) {
    uint8_t status;
    do {
        status = inb(0x64);
    } while (status & 0x02);
    outb(0x64, 0xFE);
    
    // Fallback: Triple fault
    struct {
        uint16_t limit;
        uint64_t base;
    } __attribute__((packed)) idt_ptr = {0, 0};
    __asm__ volatile("lidt %0; int $3" : : "m"(idt_ptr));
}

static void gui_shutdown(void) {
    int w = vesa_get_width();
    int h = vesa_get_height();
    
    // Clear screen to solid black
    vesa_draw_rect(0, 0, w, h, 0x000000);
    
    // Draw cyan shutdown HUD
    vesa_draw_string("Shutting down AetherOS...", w / 2 - 96, h / 2 - 10, 0x00FF88);
    vesa_flush();
    
    // Brief busy delay for simulated hardware flush
    for (volatile int i = 0; i < 50000000; i++) {
        // busy wait
    }
    
    // Halt CPU
    __asm__ volatile("cli; hlt");
}

/* Custom scalable font character drawer */
static void draw_char_scaled(char c, int x, int y, int scale, uint32_t color) {
    if (c < 32 || c > 126) {
        c = ' ';
    }
    int idx = c - 32;
    for (int row = 0; row < 8; row++) {
        uint8_t row_bits = font8x8[idx][row];
        for (int col = 0; col < 8; col++) {
            if (row_bits & (1 << (7 - col))) {
                // Draw pixel block representing the scaled pixel
                for (int sy = 0; sy < scale; sy++) {
                    for (int sx = 0; sx < scale; sx++) {
                        vesa_put_pixel(x + col * scale + sx, y + row * scale + sy, color);
                    }
                }
            }
        }
    }
}

/* Scalable font string drawer */
static void draw_string_scaled(const char* str, int x, int y, int scale, uint32_t color) {
    for (int i = 0; str[i] != '\0'; i++) {
        draw_char_scaled(str[i], x + i * 8 * scale, y, scale, color);
    }
}

/* Processes keyboard inputs depending on lock, login, or desktop state */
void gui_handle_key(char c) {
    if (video_fullscreen) {
        if (c == 27) {
            video_fullscreen = 0;
            return;
        }
        if (c == ' ') {
            video_is_playing = !video_is_playing;
            return;
        }
    }
    if (snake_active) {
        if (c == 'w' || c == 'W') { if (snake_dir != 2) snake_dir = 0; return; }
        if (c == 'd' || c == 'D') { if (snake_dir != 3) snake_dir = 1; return; }
        if (c == 's' || c == 'S') { if (snake_dir != 0) snake_dir = 2; return; }
        if (c == 'a' || c == 'A') { if (snake_dir != 1) snake_dir = 3; return; }
        if (c == ' ' || c == '\n') {
            if (snake_game_over) {
                init_snake_game();
            }
            return;
        }
        if (c == 27 || c == 'q' || c == 'Q') {
            snake_active = 0;
            return;
        }
        return;
    }
    if (gui_state == GUI_STATE_LOCKSCREEN) {
        gui_state = GUI_STATE_LOGINSCREEN;
        password_len = 0;
        login_failed = 0;
    }
    else if (gui_state == GUI_STATE_LOGINSCREEN) {
        login_failed = 0; // Reset failure state on typing
        if (c == '\n') {
            // Verify password ("aether")
            if (password_len > 0) {
                int match = 1;
                const char* secret = system_password;
                for (int i = 0; i < password_len; i++) {
                    if (secret[i] == '\0' || password_buf[i] != secret[i]) {
                        match = 0;
                        break;
                    }
                }
                if (secret[password_len] != '\0') match = 0;
                
                if (match) {
                    gui_state = GUI_STATE_DESKTOP;
                } else {
                    login_failed = 1;
                    password_len = 0;
                }
            }
        }
        else if (c == '\b') {
            if (password_len > 0) {
                password_len--;
            }
        }
        else if (c >= 32 && c <= 126) {
            if (password_len < 31) {
                password_buf[password_len++] = c;
            }
        }
    }
    else if (gui_state == GUI_STATE_DESKTOP) {
        if (c == 27) { // Escape key toggles Start Menu
            start_menu_open = !start_menu_open;
            return;
        }
        if (gui_focus == FOCUS_NOTEPAD) {
            if (c == '\b') {
                if (notepad_len > 0) {
                    notepad_len--;
                    notepad_buf[notepad_len] = '\0';
                }
            }
            else if (c == '\n') {
                if (notepad_len < 1023) {
                    notepad_buf[notepad_len++] = '\n';
                    notepad_buf[notepad_len] = '\0';
                }
            }
            else if (c >= 32 && c <= 126) {
                if (notepad_len < 1023) {
                    notepad_buf[notepad_len++] = c;
                    notepad_buf[notepad_len] = '\0';
                }
            }
        }
        else if (gui_focus == FOCUS_FILES_SEARCH) {
            if (c == '\b') {
                if (files_search_len > 0) {
                    files_search_len--;
                    files_search_query[files_search_len] = '\0';
                }
            }
            else if (c == '\n') {
                // Submit search, keep focus
            }
            else if (c >= 32 && c <= 126) {
                if (files_search_len < 31) {
                    files_search_query[files_search_len++] = c;
                    files_search_query[files_search_len] = '\0';
                }
            }
        }
        else if (gui_focus == FOCUS_BROWSER_URL) {
            if (c == '\b') {
                if (browser_url_len > 0) {
                    browser_url_len--;
                    browser_url[browser_url_len] = '\0';
                }
            }
            else if (c == '\n') {
                if (str_contains_nocase(browser_url, "google")) {
                    browser_active_tab = 0;
                    browser_has_searched = 0;
                    browser_search_len = 0;
                    browser_search_query[0] = '\0';
                } else if (str_contains_nocase(browser_url, "home") || str_contains_nocase(browser_url, "portal")) {
                    browser_active_tab = 1;
                    browser_page = 0;
                } else if (str_contains_nocase(browser_url, "store") || str_contains_nocase(browser_url, "app")) {
                    browser_active_tab = 1;
                    browser_page = 4;
                } else if (str_contains_nocase(browser_url, "wiki")) {
                    browser_active_tab = 2;
                } else if (str_contains_nocase(browser_url, "yout") ||
                           str_contains_nocase(browser_url, "tube") ||
                           str_contains_nocase(browser_url, "yautub")) {
                    browser_active_tab = 0;
                    browser_has_searched = 1;
                    const char* yt_q = "youtube";
                    int yi = 0;
                    for (; yt_q[yi]; yi++) {
                        browser_last_search[yi] = yt_q[yi];
                        browser_search_query[yi] = yt_q[yi];
                    }
                    browser_last_search[yi] = '\0';
                    browser_search_query[yi] = '\0';
                    browser_search_len = yi;
                    browser_url_len = 0;
                    browser_url[0] = '\0';
                } else {
                    browser_active_tab = 0;
                    browser_has_searched = 1;
                    int i = 0;
                    for (; i < browser_url_len && i < 63; i++) {
                        browser_last_search[i] = browser_url[i];
                        browser_search_query[i] = browser_url[i];
                    }
                    browser_last_search[i] = '\0';
                    browser_search_query[i] = '\0';
                    browser_search_len = i;
                    browser_url_len = 0;
                    browser_url[0] = '\0';
                }
                gui_focus = FOCUS_NONE;
            }
            else if (c >= 32 && c <= 126) {
                if (browser_url_len < 127) {
                    browser_url[browser_url_len++] = c;
                    browser_url[browser_url_len] = '\0';
                }
            }
        }
        else if (gui_focus == FOCUS_BROWSER_SEARCH) {
            if (c == '\b') {
                if (browser_search_len > 0) {
                    browser_search_len--;
                    browser_search_query[browser_search_len] = '\0';
                }
            }
            else if (c == '\n') {
                if (browser_search_len > 0) {
                    browser_has_searched = 1;
                    int i = 0;
                    for (; i < browser_search_len; i++) browser_last_search[i] = browser_search_query[i];
                    browser_last_search[i] = '\0';
                    gui_focus = FOCUS_NONE;
                }
            }
            else if (c >= 32 && c <= 126) {
                if (browser_search_len < 63) {
                    browser_search_query[browser_search_len++] = c;
                    browser_search_query[browser_search_len] = '\0';
                }
            }
        }
        else if (gui_focus == FOCUS_START_SEARCH) {
            if (c == '\b') {
                if (start_search_len > 0) {
                    start_search_len--;
                    start_search_query[start_search_len] = '\0';
                }
            }
            else if (c == '\n') {
                gui_focus = FOCUS_NONE;
            }
            else if (c >= 32 && c <= 126) {
                if (start_search_len < 31) {
                    start_search_query[start_search_len++] = c;
                    start_search_query[start_search_len] = '\0';
                }
            }
        }
        else if (gui_focus == FOCUS_SETTINGS_SEARCH) {
            if (c == '\b') {
                if (settings_search_len > 0) {
                    settings_search_len--;
                    settings_search_query[settings_search_len] = '\0';
                }
            }
            else if (c == '\n') {
                gui_focus = FOCUS_NONE;
            }
            else if (c >= 32 && c <= 126) {
                if (settings_search_len < 31) {
                    settings_search_query[settings_search_len++] = c;
                    settings_search_query[settings_search_len] = '\0';
                }
            }
        }
        else if (gui_focus == FOCUS_SETTINGS_CUR_PASS) {
            if (c == '\b') {
                if (settings_cur_pass_len > 0) {
                    settings_cur_pass_len--;
                    settings_cur_pass[settings_cur_pass_len] = '\0';
                }
            }
            else if (c == '\n') {
                gui_focus = FOCUS_SETTINGS_NEW_PASS;
            }
            else if (c >= 32 && c <= 126) {
                if (settings_cur_pass_len < 31) {
                    settings_cur_pass[settings_cur_pass_len++] = c;
                    settings_cur_pass[settings_cur_pass_len] = '\0';
                }
            }
        }
        else if (gui_focus == FOCUS_SETTINGS_NEW_PASS) {
            if (c == '\b') {
                if (settings_new_pass_len > 0) {
                    settings_new_pass_len--;
                    settings_new_pass[settings_new_pass_len] = '\0';
                }
            }
            else if (c == '\n') {
                settings_change_password();
            }
            else if (c >= 32 && c <= 126) {
                if (settings_new_pass_len < 31) {
                    settings_new_pass[settings_new_pass_len++] = c;
                    settings_new_pass[settings_new_pass_len] = '\0';
                }
            }
        }
        else if (gui_focus == FOCUS_NOTEPAD_SAVE_PATH) {
            if (c == '\b') {
                if (notepad_save_path_len > 0) {
                    notepad_save_path_len--;
                    notepad_save_path[notepad_save_path_len] = '\0';
                }
            }
            else if (c == '\t') {
                gui_focus = FOCUS_NOTEPAD_SAVE_NAME;
            }
            else if (c >= 32 && c <= 126) {
                if (notepad_save_path_len < 63) {
                    notepad_save_path[notepad_save_path_len++] = c;
                    notepad_save_path[notepad_save_path_len] = '\0';
                }
            }
        }
        else if (gui_focus == FOCUS_NOTEPAD_SAVE_NAME) {
            if (c == '\b') {
                if (notepad_save_name_len > 0) {
                    notepad_save_name_len--;
                    notepad_save_name[notepad_save_name_len] = '\0';
                }
            }
            else if (c == '\t') {
                gui_focus = FOCUS_NOTEPAD_SAVE_PATH;
            }
            else if (c >= 32 && c <= 126) {
                if (notepad_save_name_len < 31) {
                    notepad_save_name[notepad_save_name_len++] = c;
                    notepad_save_name[notepad_save_name_len] = '\0';
                }
            }
        }
        else {
            extern void shell_input_char(char c);
            shell_input_char(c);
        }
    }
}

/* Techy AetherOS wallpaper: dark navy base + scanline grid + bloom glow */
static void gui_draw_wallpaper(void) {
    int w = vesa_get_width();
    int h = vesa_get_height();
    uint32_t* bb = vesa_get_backbuffer();
    int cx = w / 2, cy = h / 2;

    if (wallpaper_style == 1) {
        /* Void mode: very dark subtle tech gradient */
        for (int y = 0; y < h; y++) {
            int dy = y - cy;
            uint32_t row = (uint32_t)y * (uint32_t)w;
            for (int x = 0; x < w; x++) {
                int dx = x - cx;
                int dist = (dx*dx + dy*dy) >> 10;
                if (dist > 255) dist = 255;
                int inv = 255 - dist;
                
                int r = 1 + (inv * 1) / 255;
                int g = 4 + (inv * 6) / 255;
                int b = 10 + (inv * 10) / 255;
                
                bb[row + x] = ((uint32_t)r << 16) | ((uint32_t)g << 8) | (uint32_t)b;
            }
        }
        return;
    }
    
    if (wallpaper_style == 2) {
        /* Matrix Rain Mode: solid black background first, falling characters drawn on top in gui_draw */
        for (int i = 0; i < w * h; i++) {
            bb[i] = 0x000000;
        }
        /* Draw falling characters */
        uint64_t ticks = pit_get_ticks();
        for (int col = 0; col < 40; col++) {
            int lx = 20 + col * 25;
            int ly = (int)((ticks * (col + 3) * 2) % (h + 200)) - 150;
            for (int row = 0; row < 15; row++) {
                int y_pos = ly + row * 14;
                if (y_pos >= 0 && y_pos < h - 48) {
                    int alpha = 255 - row * 15;
                    if (alpha > 0) {
                        char mc = '0' + ((ticks + col * 7 + row * 3) % 10);
                        uint32_t c2 = (uint32_t)((alpha * (gui_accent_col >> 16 & 0xFF)) / 255) << 16 |
                                      (uint32_t)((alpha * (gui_accent_col >> 8 & 0xFF)) / 255) << 8 |
                                      (uint32_t)((alpha * (gui_accent_col & 0xFF)) / 255);
                        vesa_draw_char(mc, lx, y_pos, c2);
                    }
                }
            }
        }
        return;
    }

    if (wallpaper_style == 3) {
        /* Synthwave Red: Dark crimson base, orange horizontal sunset grid, sun glow in center */
        for (int y = 0; y < h; y++) {
            int dy = y - cy;
            uint32_t row = (uint32_t)y * (uint32_t)w;
            for (int x = 0; x < w; x++) {
                int dx = x - cx;
                int dist = (dx*dx + dy*dy) >> 8;
                if (dist > 255) dist = 255;
                int inv = 255 - dist;

                int r = 16 + (inv * 60) / 255;
                int g = 2  + (inv * 15) / 255;
                int b = 4  + (inv * 2)  / 255;

                if ((y % 24) == 0) { r += 20; g += 10; b += 2; }
                if ((x % 64) == 0) { r += 10; g += 5; }

                if (r > 255) r = 255;
                if (g > 255) g = 255;
                if (b > 255) b = 255;

                bb[row + x] = ((uint32_t)r << 16) | ((uint32_t)g << 8) | (uint32_t)b;
            }
        }
        return;
    }

    if (wallpaper_style == 4) {
        /* Quantum Nebula: Dark space with dynamic fractal-like gas clouds colored by user's accent color */
        uint8_t acc_r = (gui_accent_col >> 16) & 0xFF;
        uint8_t acc_g = (gui_accent_col >> 8) & 0xFF;
        uint8_t acc_b = gui_accent_col & 0xFF;

        for (int y = 0; y < h; y++) {
            int dy = y - cy;
            uint32_t row = (uint32_t)y * (uint32_t)w;
            for (int x = 0; x < w; x++) {
                int dx = x - cx;
                
                int val = (dx * dy) >> 12;
                if (val < 0) val = -val;
                val = (val + (dx >> 2) + (dy >> 2)) & 0xFF;
                
                int dist = (dx*dx + dy*dy) >> 10;
                if (dist > 255) dist = 255;
                
                int r = 1 + (val * acc_r) / 1024;
                int g = 2 + (val * acc_g) / 1024;
                int b = 6 + (val * acc_b) / 1024;

                if ((x * y + x) % 1013 == 0) { r = 200; g = 210; b = 255; }

                if (r > 255) r = 255;
                if (g > 255) g = 255;
                if (b > 255) b = 255;

                bb[row + x] = ((uint32_t)r << 16) | ((uint32_t)g << 8) | (uint32_t)b;
            }
        }
        return;
    }

    for (int y = 0; y < h; y++) {
        int dy = y - cy;
        uint32_t row = (uint32_t)y * (uint32_t)w;
        for (int x = 0; x < w; x++) {
            int dx = x - cx;
            int dist = (dx*dx + dy*dy) >> 9;
            if (dist > 255) dist = 255;
            int inv = 255 - dist;

            /* Dark navy base with subtle teal center */
            int r = 2  + (inv * 2)  / 255;
            int g = 12 + (inv * 40) / 255;
            int b = 28 + (inv * 60) / 255;

            /* Scanline grid: horizontal lines every 4px */
            if ((y & 3) == 0) { r = r/2; g = g/2 + 2; b = b/2 + 4; }
            /* Vertical grid every 32px — subtle column markers */
            if ((x % 32) == 0) { g += 4; b += 6; }
            /* Horizontal grid every 32px */
            if ((y % 32) == 0 && (y & 3) == 0) { g += 6; b += 10; }

            if (r > 255) r = 255;
            if (g > 255) g = 255;
            if (b > 255) b = 255;

            bb[row + x] = ((uint32_t)r << 16) | ((uint32_t)g << 8) | (uint32_t)b;
        }
    }
}

/* Draw a filled rounded rectangle (corner radius = 8px) */
static void gui_fill_rounded(int x, int y, int w, int h, uint32_t col) {
    int r = 8;
    /* center fill */
    vesa_draw_rect(x + r, y,     w - 2*r, h,     col);
    vesa_draw_rect(x,     y + r, r,       h - 2*r, col);
    vesa_draw_rect(x + w - r, y + r, r, h - 2*r, col);
    /* approximate corners */
    for (int cy2 = 0; cy2 < r; cy2++) {
        for (int cx2 = 0; cx2 < r; cx2++) {
            int dd = (r-1-cx2)*(r-1-cx2) + (r-1-cy2)*(r-1-cy2);
            if (dd <= r*r) {
                vesa_put_pixel(x      + cx2, y      + cy2, col);
                vesa_put_pixel(x+w-1 - cx2, y      + cy2, col);
                vesa_put_pixel(x      + cx2, y+h-1 - cy2, col);
                vesa_put_pixel(x+w-1 - cx2, y+h-1 - cy2, col);
            }
        }
    }
}

/* Draw Win11 four-square Start grid icon */
static void draw_win_grid(int cx, int cy, uint32_t col) {
    int s = 5, g = 2;
    vesa_draw_rect(cx - s - g, cy - s - g, s, s, col);
    vesa_draw_rect(cx      + g, cy - s - g, s, s, col);
    vesa_draw_rect(cx - s - g, cy      + g, s, s, col);
    vesa_draw_rect(cx      + g, cy      + g, s, s, col);
}

/* Techy HUD window: neon border, corner brackets, centered title, Win-style controls */
static void gui_draw_window_frame(gui_window_t* win) {
    uint32_t tb_col  = win->active ? 0x0D1117 : 0x090D11;
    uint32_t bdr_col = win->active ? 0x00FF88 : 0x1A4A2A;
    uint32_t acc     = win->active ? 0x00FF88 : 0x1A4A2A;

    /* --- Window shadow --- */
    vesa_draw_line(win->x + 4, win->y + win->h, win->x + win->w - 4, win->y + win->h, 0x050505);
    vesa_draw_line(win->x + win->w, win->y + 4, win->x + win->w, win->y + win->h - 4, 0x050505);

    /* --- Title bar background --- */
    gui_fill_rounded(win->x, win->y, win->w, 32, tb_col);

    /* --- Window body --- */
    vesa_draw_rect(win->x, win->y + 32, win->w, win->h - 32, 0x0B0F14);

    /* --- Neon border all around --- */
    vesa_draw_line(win->x,            win->y,     win->x,            win->y + win->h, bdr_col);
    vesa_draw_line(win->x + win->w-1, win->y,     win->x + win->w-1, win->y + win->h, bdr_col);
    vesa_draw_line(win->x,            win->y,     win->x + win->w-1, win->y,           bdr_col);
    vesa_draw_line(win->x,            win->y + win->h, win->x + win->w-1, win->y + win->h, bdr_col);

    /* --- HUD corner brackets (top-left) --- */
    vesa_draw_rect(win->x,   win->y,   8, 2, acc);
    vesa_draw_rect(win->x,   win->y,   2, 8, acc);
    /* top-right */
    vesa_draw_rect(win->x + win->w - 9, win->y, 8, 2, acc);
    vesa_draw_rect(win->x + win->w - 2, win->y, 2, 8, acc);
    /* bottom-left */
    vesa_draw_rect(win->x,   win->y + win->h - 2, 8, 2, acc);
    vesa_draw_rect(win->x,   win->y + win->h - 9, 2, 8, acc);
    /* bottom-right */
    vesa_draw_rect(win->x + win->w - 9, win->y + win->h - 2, 8, 2, acc);
    vesa_draw_rect(win->x + win->w - 2, win->y + win->h - 9, 2, 8, acc);

    /* --- Accent separator under title bar --- */
    vesa_draw_line(win->x + 1, win->y + 32, win->x + win->w - 2, win->y + 32, acc);
    /* glowing double line effect */
    vesa_draw_line(win->x + 4, win->y + 33, win->x + win->w - 5, win->y + 33, win->active ? 0x003322 : 0x0A1A10);

    /* --- Centered window title (monospace techy look) --- */
    int title_len = 0;
    while (win->title[title_len]) title_len++;
    int title_x = win->x + (win->w - title_len * 8) / 2;
    vesa_draw_string(win->title, title_x, win->y + 10, win->active ? 0x00FF88 : 0x2A6644);

    /* --- Control buttons: — □ x (right side) --- */
    int bx = win->x + win->w - 10;
    int by = win->y + 8;
    /* Close x — red on active */
    vesa_draw_rect(bx - 22, by - 2, 20, 14, 0x1A0000);
    vesa_draw_rect_outline(bx - 22, by - 2, 20, 14, win->active ? 0xFF3333 : 0x441111);
    vesa_draw_string("x", bx - 16, by + 2, win->active ? 0xFF3333 : 0x441111);
    /* Maximize [] */
    vesa_draw_rect(bx - 46, by - 2, 20, 14, 0x001A08);
    vesa_draw_rect_outline(bx - 46, by - 2, 20, 14, win->active ? 0x00FF88 : 0x1A4A2A);
    vesa_draw_rect_outline(bx - 42, by + 1, 10, 8, win->active ? 0x00FF88 : 0x1A4A2A);
    /* Minimize — */
    vesa_draw_rect(bx - 70, by - 2, 20, 14, 0x001A08);
    vesa_draw_rect_outline(bx - 70, by - 2, 20, 14, win->active ? 0x00FF88 : 0x1A4A2A);
    vesa_draw_line(bx - 66, by + 5, bx - 54, by + 5, win->active ? 0x00FF88 : 0x1A4A2A);
}

/* Redraws the terminal buffer contents inside the console window */
static void gui_draw_terminal(void) {
    int text_x = console_window.x + 10;
    int text_y = console_window.y + 35;
    
    for (int r = 0; r < 25; r++) {
        for (int c = 0; c < 80; c++) {
            uint16_t entry = terminal_buffer[r * 80 + c];
            char ch = entry & 0xFF;
            uint8_t attr = (entry >> 8) & 0xFF;
            
            // Map VGA 4-bit colors to vibrant 32-bit hex colors
            uint32_t color = 0xFFFFFF; // Default white
            uint8_t fg = attr & 0x0F;
            if (fg == 0x1) color = 0x2979FF;      // Blue
            else if (fg == 0x2) color = 0x39FF14; // Neon Green
            else if (fg == 0x3) color = 0x00E5FF; // Cyber Cyan
            else if (fg == 0x4) color = 0xFF1744; // Neon Red
            else if (fg == 0x5) color = 0xD500F9; // Magenta
            else if (fg == 0x6) color = 0xFF9100; // Orange
            else if (fg == 0x7) color = 0xCFD8DC; // Light grey
            else if (fg == 0x8) color = 0x78909C; // Dark grey
            else if (fg == 0x9) color = 0x82B1FF; // Light blue
            else if (fg == 0xA) color = 0xB2FF59; // Light green
            else if (fg == 0xB) color = 0x84FFFF; // Light cyan
            else if (fg == 0xC) color = 0xFF8A80; // Light red
            else if (fg == 0xD) color = 0xFF80AB; // Pink
            else if (fg == 0xE) color = 0xFFFF00; // Yellow
            
            if (ch != ' ' && ch != '\0') {
                vesa_draw_char(ch, text_x + c * 8, text_y + r * 14, color);
            }
        }
    }
}

/* Custom inline integer to string conversion helper */
static void format_stat(char* b, const char* label, uint32_t val, const char* suffix) {
    int l = 0;
    while (label[l] != '\0') { b[l] = label[l]; l++; }
    uint32_t v = val;
    char temp[16];
    int ti = 0;
    if (v == 0) { temp[ti++] = '0'; }
    else {
        while (v > 0) { temp[ti++] = '0' + (v % 10); v /= 10; }
    }
    for (int j = ti - 1; j >= 0; j--) { b[l++] = temp[j]; }
    int s = 0;
    while (suffix[s] != '\0') { b[l++] = suffix[s]; s++; }
    b[l] = '\0';
}

/* Helper to format system uptime clock */
static void format_time(char* buf, uint32_t h, uint32_t m, uint32_t s) {
    buf[0] = '0' + (h / 10);
    buf[1] = '0' + (h % 10);
    buf[2] = ':';
    buf[3] = '0' + (m / 10);
    buf[4] = '0' + (m % 10);
    buf[5] = ':';
    buf[6] = '0' + (s / 10);
    buf[7] = '0' + (s % 10);
    buf[8] = '\0';
}

/* Renders widgets, charts, and metrics in the Telemetry window */
static void gui_draw_telemetry(void) {
    int start_x = monitor_window.x + 15;
    int start_y = monitor_window.y + 40;
    
    // Retrieve PMM metrics
    uint32_t total_blocks = pmm_get_total_blocks();
    uint32_t free_blocks = pmm_get_free_blocks();
    uint32_t used_blocks = total_blocks - free_blocks;
    
    // 1. RAM Utilization Header
    vesa_draw_string("--- MEMORY MANAGED ---", start_x, start_y, 0x78909C);
    start_y += 20;
    
    // Print stats
    char buf[64];
    
    format_stat(buf, "TOTAL: ", total_blocks, " BLOCKS");
    vesa_draw_string(buf, start_x, start_y, 0xFFFFFF);
    start_y += 16;
    
    format_stat(buf, "FREE:  ", free_blocks, " BLOCKS");
    vesa_draw_string(buf, start_x, start_y, 0x39FF14);
    start_y += 16;
    
    format_stat(buf, "USED:  ", used_blocks, " BLOCKS");
    vesa_draw_string(buf, start_x, start_y, 0xFF1744);
    start_y += 20;
    
    // Segmented Bar Chart
    int bar_width = 200;
    int used_width = total_blocks > 0 ? (int)((used_blocks * (uint64_t)bar_width) / total_blocks) : 0;
    // Draw outer outline
    vesa_draw_rect_outline(start_x, start_y, bar_width, 14, 0x42464D);
    // Draw used RAM segment (Red/Orange)
    if (used_width > 0) {
        vesa_draw_rect(start_x + 1, start_y + 1, used_width - 2, 12, 0xFF5722);
    }
    // Draw free RAM segment (Green)
    if (bar_width - used_width > 0) {
        vesa_draw_rect(start_x + used_width + 1, start_y + 1, bar_width - used_width - 2, 12, 0x4CAF50);
    }
    start_y += 35;
    
    // 2. Thread states
    vesa_draw_string("--- TASK SCHEDULE ---", start_x, start_y, 0x78909C);
    start_y += 20;
    
    uint64_t ticks = pit_get_ticks();
    format_stat(buf, "SYS TICKS: ", (uint32_t)ticks, "");
    vesa_draw_string(buf, start_x, start_y, 0xFFFF00);
    start_y += 16;
    
    // Thread 1
    int t1_susp = task_is_suspended(1);
    format_stat(buf, "T1 TICKS:  ", thread1_ticks, t1_susp ? " [SUSP]" : " [RUN]");
    vesa_draw_string(buf, start_x, start_y, t1_susp ? 0xFF3333 : 0xFF80AB);
    
    // Draw button next to T1
    int btn_x1 = start_x + 220;
    int btn_y1 = start_y - 2;
    extern int mouse_get_x(void);
    extern int mouse_get_y(void);
    int mx = mouse_get_x();
    int my = mouse_get_y();
    int hov_t1 = (mx >= btn_x1 && mx < btn_x1 + 60 && my >= btn_y1 && my < btn_y1 + 14);
    vesa_draw_rect(btn_x1, btn_y1, 60, 14, hov_t1 ? 0x0A2010 : 0x000000);
    vesa_draw_rect_outline(btn_x1, btn_y1, 60, 14, t1_susp ? 0x00FF88 : 0xFF3333);
    vesa_draw_string(t1_susp ? "Resume" : "Suspend", btn_x1 + 6, btn_y1 + 3, t1_susp ? 0x00FF88 : 0xFF3333);
    start_y += 16;
    
    // Thread 2
    int t2_susp = task_is_suspended(2);
    format_stat(buf, "T2 TICKS:  ", thread2_ticks, t2_susp ? " [SUSP]" : " [RUN]");
    vesa_draw_string(buf, start_x, start_y, t2_susp ? 0xFF3333 : 0x82B1FF);
    
    // Draw button next to T2
    int btn_x2 = start_x + 220;
    int btn_y2 = start_y - 2;
    int hov_t2 = (mx >= btn_x2 && mx < btn_x2 + 60 && my >= btn_y2 && my < btn_y2 + 14);
    vesa_draw_rect(btn_x2, btn_y2, 60, 14, hov_t2 ? 0x0A2010 : 0x000000);
    vesa_draw_rect_outline(btn_x2, btn_y2, 60, 14, t2_susp ? 0x00FF88 : 0xFF3333);
    vesa_draw_string(t2_susp ? "Resume" : "Suspend", btn_x2 + 6, btn_y2 + 3, t2_susp ? 0x00FF88 : 0xFF3333);
    start_y += 24;
    
    // 3. CPU Oscilloscope Wave Graph
    vesa_draw_string("--- CPU LOAD OSCILL ---", start_x, start_y, 0x78909C);
    start_y += 50;
    
    // Redraw oscilloscope graph grid box
    vesa_draw_rect_outline(start_x, start_y - 30, WAVE_POINTS * 2 + 10, 60, 0x2A2E33);
    vesa_draw_line(start_x, start_y, start_x + WAVE_POINTS * 2 + 10, start_y, 0x2A2E33); // center line
    
    // Update and draw wave history
    int new_value = (int)(ticks % 25);
    if (ticks % 3 == 0) {
        new_value = 15 - (new_value % 30);
    } else {
        new_value = (new_value % 20) - 10;
    }
    
    // Add point to circular/shifting buffer
    wave_history[wave_index] = new_value;
    wave_index = (wave_index + 1) % WAVE_POINTS;
    
    // Draw lines connecting points
    int prev_x = start_x + 5;
    int prev_y = start_y + wave_history[wave_index];
    for (int i = 1; i < WAVE_POINTS; i++) {
        int idx = (wave_index + i) % WAVE_POINTS;
        int cx = start_x + 5 + i * 2;
        int cy = start_y + wave_history[idx];
        
        // Draw cyber green connection line
        vesa_draw_line(prev_x, prev_y, cx, cy, 0x39FF14);
        
        prev_x = cx;
        prev_y = cy;
    }
}

/* Renders modal dialog box layout */
static void gui_draw_dialog(gui_dialog_t* dlg) {
    // Outer border
    vesa_draw_rect_outline(dlg->x, dlg->y, dlg->w, dlg->h, 0x2A2E33);
    // Header
    vesa_draw_rect(dlg->x + 1, dlg->y + 1, dlg->w - 2, 24, 0x1A2535);
    vesa_draw_rect(dlg->x + 1, dlg->y + 24, dlg->w - 2, 1, 0x00E5FF); // Cyber Cyan accent
    vesa_draw_string(dlg->title, dlg->x + 8, dlg->y + 8, 0xFFFFFF);
    // Close button (Red)
    vesa_draw_rect(dlg->x + dlg->w - 20, dlg->y + 6, 12, 12, 0xFF1744);
    vesa_draw_rect_outline(dlg->x + dlg->w - 20, dlg->y + 6, 12, 12, 0xFFFFFF);
    
    // Background
    vesa_draw_rect(dlg->x + 1, dlg->y + 25, dlg->w - 2, dlg->h - 26, 0x121417);
    
    // Message parsing newlines
    int cx = dlg->x + 15;
    int cy = dlg->y + 45;
    char line_buf[64];
    int li = 0;
    for (int i = 0; dlg->message[i] != '\0'; i++) {
        if (dlg->message[i] == '\n') {
            line_buf[li] = '\0';
            vesa_draw_string(line_buf, cx, cy, 0xCFD8DC);
            cy += 18;
            li = 0;
        } else {
            if (li < 63) {
                line_buf[li++] = dlg->message[i];
            }
        }
    }
    if (li > 0) {
        line_buf[li] = '\0';
        vesa_draw_string(line_buf, cx, cy, 0xCFD8DC);
    }
    
    // OK Button at the bottom
    int btn_w = 60;
    int btn_h = 22;
    int btn_x = dlg->x + (dlg->w - btn_w) / 2;
    int btn_y = dlg->y + dlg->h - 35;
    vesa_draw_rect(btn_x, btn_y, btn_w, btn_h, 0x00E5FF);
    vesa_draw_string("OK", btn_x + 22, btn_y + 6, 0x000000);
}

/* Draw a single desktop icon */
static void draw_desktop_icon(int x, int y, uint32_t icon_col, int shape,
                               const char* label, int mx, int my) {
    int is_hover = (mx >= x - 2 && mx < x + 34 && my >= y && my < y + 50);
    uint32_t bg = is_hover ? 0x0A3020 : 0x061209;
    if (is_hover) vesa_draw_rect_outline(x - 2, y - 2, 36, 52, 0x00FF88);
    gui_fill_rounded(x, y, 32, 32, bg);
    switch (shape) {
        case 0: /* monitor */
            vesa_draw_rect_outline(x+4, y+5, 24, 15, icon_col);
            vesa_draw_line(x+16, y+20, x+16, y+24, icon_col);
            vesa_draw_line(x+10, y+24, x+22, y+24, icon_col); break;
        case 1: /* terminal */
            vesa_draw_rect(x+4, y+4, 24, 16, 0x020A05);
            vesa_draw_char('>', x+6, y+8,  icon_col);
            vesa_draw_char('_', x+14, y+8, icon_col); break;
        case 2: /* chart */
            vesa_draw_rect(x+5,  y+20, 5, 8,  icon_col);
            vesa_draw_rect(x+13, y+14, 5, 14, icon_col);
            vesa_draw_rect(x+21, y+8,  5, 20, icon_col); break;
        case 3: /* trash */
            vesa_draw_rect_outline(x+8, y+7, 16, 17, icon_col);
            vesa_draw_line(x+5, y+7, x+27, y+7, icon_col);
            vesa_draw_line(x+11, y+5, x+21, y+5, icon_col); break;
        case 4: /* folder */
            vesa_draw_rect(x+4, y+11, 24, 16, icon_col);
            vesa_draw_rect(x+4, y+8,  10, 5,  icon_col); break;
        case 5: /* notepad */
            vesa_draw_rect_outline(x+6, y+4, 20, 24, icon_col);
            vesa_draw_line(x+10, y+10, x+22, y+10, icon_col);
            vesa_draw_line(x+10, y+15, x+22, y+15, icon_col);
            vesa_draw_line(x+10, y+20, x+18, y+20, icon_col); break;
        case 6: /* calculator */
            vesa_draw_rect_outline(x+6, y+4, 20, 24, icon_col);
            vesa_draw_line(x+8, y+10, x+24, y+10, icon_col);
            vesa_draw_char('+', x+8,  y+13, icon_col);
            vesa_draw_char('-', x+16, y+13, icon_col); break;
        case 7: /* settings gear */
            vesa_draw_rect_outline(x+9, y+9, 14, 14, icon_col);
            vesa_put_pixel(x+16, y+5,  icon_col);
            vesa_put_pixel(x+16, y+27, icon_col);
            vesa_put_pixel(x+5,  y+16, icon_col);
            vesa_put_pixel(x+27, y+16, icon_col); break;
        case 8: /* browser globe */
            vesa_draw_rect_outline(x+5, y+5, 22, 22, icon_col);
            vesa_draw_line(x+16, y+5,  x+16, y+27, icon_col);
            vesa_draw_line(x+5,  y+16, x+27, y+16, icon_col); break;
    }
    int ll = 0; while (label[ll]) ll++;
    vesa_draw_string(label, x + 16 - ll * 4, y + 36, is_hover ? 0x00FF88 : 0x88AA88);
}

/* Desktop icons — 9 total in left column */
static void gui_draw_icons(int mx, int my) {
    draw_desktop_icon(16, 20,  0x00FF88, 0, "PC",      mx, my);
    draw_desktop_icon(16, 90,  0x00DD77, 1, "Shell",   mx, my);
    draw_desktop_icon(16, 160, 0x44FF99, 2, "Monitor", mx, my);
    draw_desktop_icon(16, 230, 0xFF4444, 3, "Trash",   mx, my);
    draw_desktop_icon(16, 300, 0xFFAA00, 4, "Files",   mx, my);
    draw_desktop_icon(16, 370, 0x88DDFF, 5, "Notes",   mx, my);
    draw_desktop_icon(16, 440, 0xFF88FF, 6, "Calc",    mx, my);
    draw_desktop_icon(16, 510, 0xFFFF44, 7, "Settings",mx, my);
    if (chrome_installed) {
        draw_desktop_icon(16, 580, 0x44AAFF, 8, "Chrome",  mx, my);
    }
}

/* ---- New App Window Renderers ---- */

static void calc_press_key(char k) {
    if (k == 'C') {
        calc_buf[0] = '0';
        calc_buf[1] = '\0';
        calc_len = 1;
        calc_value1 = 0;
        calc_op = '\0';
        calc_clear_on_next = 0;
    }
    else if (k >= '0' && k <= '9') {
        if (calc_clear_on_next || (calc_len == 1 && calc_buf[0] == '0')) {
            calc_buf[0] = k;
            calc_buf[1] = '\0';
            calc_len = 1;
            calc_clear_on_next = 0;
        } else {
            if (calc_len < 15) {
                calc_buf[calc_len++] = k;
                calc_buf[calc_len] = '\0';
            }
        }
    }
    else if (k == '+' || k == '-' || k == '*' || k == '/') {
        // parse current value
        int val = 0;
        for (int i = 0; i < calc_len; i++) val = val * 10 + (calc_buf[i] - '0');
        calc_value1 = val;
        calc_op = k;
        calc_clear_on_next = 1;
    }
    else if (k == '=') {
        if (calc_op != '\0') {
            int val2 = 0;
            for (int i = 0; i < calc_len; i++) val2 = val2 * 10 + (calc_buf[i] - '0');
            int res = 0;
            if (calc_op == '+') res = calc_value1 + val2;
            else if (calc_op == '-') res = calc_value1 - val2;
            else if (calc_op == '*') res = calc_value1 * val2;
            else if (calc_op == '/') {
                if (val2 != 0) res = calc_value1 / val2;
                else res = 999999;
            }
            
            // Format result back to calc_buf
            if (res < 0) {
                calc_buf[0] = '-';
                int val = -res;
                int idx = 1;
                char temp[16];
                int ti = 0;
                while (val > 0) { temp[ti++] = '0' + (val % 10); val /= 10; }
                if (ti == 0) temp[ti++] = '0';
                for (int j = ti - 1; j >= 0; j--) calc_buf[idx++] = temp[j];
                calc_buf[idx] = '\0';
                calc_len = idx;
            } else {
                int val = res;
                char temp[16];
                int ti = 0;
                while (val > 0) { temp[ti++] = '0' + (val % 10); val /= 10; }
                if (ti == 0) temp[ti++] = '0';
                int idx = 0;
                for (int j = ti - 1; j >= 0; j--) calc_buf[idx++] = temp[j];
                calc_buf[idx] = '\0';
                calc_len = idx;
            }
            calc_op = '\0';
            calc_clear_on_next = 1;
        }
    }
}

static void gui_draw_files(void) {
    int wx = files_window.x, wy = files_window.y, ww = files_window.w, wh = files_window.h;
    int sidebar_w = 110;
    int top_h = 44;
    extern int mouse_get_x(void);
    extern int mouse_get_y(void);
    int mx = mouse_get_x();
    int my = mouse_get_y();
    
    // Draw sidebar background
    vesa_draw_rect(wx + 1, wy + 33, sidebar_w, wh - 34, 0x070B0F);
    vesa_draw_line(wx + sidebar_w + 1, wy + 33, wx + sidebar_w + 1, wy + wh - 2, 0x1A4A2A);
    
    // Sidebar items
    const char* sb_labels[] = {"This PC  ", "Desktop  ", "Documents", "Pictures ", "Music    ", "Videos   ", "Downloads", "Recycled "};
    int sb_dirs[] = {100, 28, 29, 30, 31, 32, 6, 99};
    for (int i = 0; i < 8; i++) {
        int iy = wy + 42 + i * 24;
        int active = (files_current_dir == sb_dirs[i]);
        if (active) {
            vesa_draw_rect(wx + 4, iy, sidebar_w - 6, 20, 0x002211);
            vesa_draw_rect_outline(wx + 4, iy, sidebar_w - 6, 20, gui_accent_col);
        }
        
        // Hover highlight
        if (mx >= wx + 4 && mx < wx + sidebar_w - 2 && my >= iy && my < iy + 20) {
            if (!active) vesa_draw_rect(wx + 4, iy, sidebar_w - 6, 20, 0x0A2010);
        }
        
        vesa_draw_string(sb_labels[i], wx + 8, iy + 6, active ? gui_accent_col : 0x00AA55);
    }
    
    // Top Bar (breadcrumb, search, buttons)
    int content_x = wx + sidebar_w + 8;
    int top_y = wy + 36;
    
    // Breadcrumbs
    char path_buf[128] = "";
    int path_len = 0;
    int path_stack[16];
    int stack_ptr = 0;
    int curr = files_current_dir;
    
    while (curr >= 10 && stack_ptr < 16) {
        path_stack[stack_ptr++] = curr;
        int idx = curr - 10;
        curr = file_system[idx].dir_id;
    }
    path_stack[stack_ptr++] = curr;
    
    int first = path_stack[--stack_ptr];
    const char* base_name = "Root";
    if (first == 1) base_name = "bin";
    else if (first == 2) base_name = "boot";
    else if (first == 3) base_name = "dev";
    else if (first == 4) base_name = "etc";
    else if (first == 5) base_name = "home";
    else if (first == 6) base_name = "downloads";
    else if (first == 28) base_name = "Desktop";
    else if (first == 29) base_name = "Documents";
    else if (first == 30) base_name = "Pictures";
    else if (first == 31) base_name = "Music";
    else if (first == 32) base_name = "Videos";
    else if (first == 99) base_name = "Recycle Bin";
    else if (first == 100) base_name = "This PC";
    
    const char* pc = "This PC > ";
    while (*pc) path_buf[path_len++] = *pc++;
    while (*base_name) path_buf[path_len++] = *base_name++;
    
    while (stack_ptr > 0) {
        int next_comp = path_stack[--stack_ptr];
        int idx = next_comp - 10;
        if (idx >= 0 && idx < file_system_count) {
            const char* sep = " > ";
            while (*sep) path_buf[path_len++] = *sep++;
            const char* name = file_system[idx].name;
            while (*name) path_buf[path_len++] = *name++;
        }
    }
    path_buf[path_len] = '\0';
    vesa_draw_string(path_buf, content_x, top_y, 0x88DDFF);
    
    // Search input bar
    int search_w = 120;
    int search_x = wx + ww - search_w - 10;
    int search_y = top_y - 2;
    vesa_draw_rect(search_x, search_y, search_w, 18, 0x020A05);
    vesa_draw_rect_outline(search_x, search_y, search_w, 18, (gui_focus == FOCUS_FILES_SEARCH) ? gui_accent_col : 0x1A4A2A);
    
    if (files_search_len == 0) {
        vesa_draw_string("Search... ", search_x + 6, search_y + 5, 0x1A4A2A);
    } else {
        vesa_draw_string(files_search_query, search_x + 6, search_y + 5, gui_accent_col);
        // Blinking cursor
        uint64_t ticks = pit_get_ticks();
        if ((ticks / 30) % 2 == 0 && gui_focus == FOCUS_FILES_SEARCH) {
            vesa_draw_rect(search_x + 6 + files_search_len * 8, search_y + 4, 2, 10, gui_accent_col);
        }
    }
    
    // Action Buttons
    int btn_y = top_y + 20;
    // [ + New ] button
    int new_btn_x = content_x;
    int hov_new = (mx >= new_btn_x && mx < new_btn_x + 44 && my >= btn_y && my < btn_y + 16);
    vesa_draw_rect(new_btn_x, btn_y, 44, 16, hov_new ? 0x0A2010 : 0x001105);
    vesa_draw_rect_outline(new_btn_x, btn_y, 44, 16, hov_new ? gui_accent_col : 0x005522);
    vesa_draw_string("+ New", new_btn_x + 4, btn_y + 4, 0x00AA55);
    
    // [ + Folder ] button
    int folder_btn_x = content_x + 50;
    int hov_folder = (mx >= folder_btn_x && mx < folder_btn_x + 60 && my >= btn_y && my < btn_y + 16);
    vesa_draw_rect(folder_btn_x, btn_y, 60, 16, hov_folder ? 0x0A2010 : 0x001105);
    vesa_draw_rect_outline(folder_btn_x, btn_y, 60, 16, hov_folder ? gui_accent_col : 0x005522);
    vesa_draw_string("+ Folder", folder_btn_x + 4, btn_y + 4, 0x00AA55);
    
    // [ Delete ] / [ Restore ] / [ Empty ] buttons
    if (files_current_dir == 99) {
        // [ Restore ]
        int res_btn_x = content_x + 116;
        int hov_res = (mx >= res_btn_x && mx < res_btn_x + 54 && my >= btn_y && my < btn_y + 16);
        if (files_selected_idx >= 0) {
            vesa_draw_rect(res_btn_x, btn_y, 54, 16, hov_res ? 0x0A2010 : 0x001105);
            vesa_draw_rect_outline(res_btn_x, btn_y, 54, 16, hov_res ? gui_accent_col : 0x005522);
            vesa_draw_string("Restore", res_btn_x + 4, btn_y + 4, 0x00AA55);
        }
        
        // [ Empty ]
        int emp_btn_x = content_x + 176;
        int hov_emp = (mx >= emp_btn_x && mx < emp_btn_x + 44 && my >= btn_y && my < btn_y + 16);
        vesa_draw_rect(emp_btn_x, btn_y, 44, 16, hov_emp ? 0x220505 : 0x110000);
        vesa_draw_rect_outline(emp_btn_x, btn_y, 44, 16, hov_emp ? 0xFF3333 : 0x550000);
        vesa_draw_string("Empty", emp_btn_x + 4, btn_y + 4, 0xFF3333);
    } else {
        // [ Delete ]
        int del_btn_x = content_x + 116;
        int hov_del = (mx >= del_btn_x && mx < del_btn_x + 50 && my >= btn_y && my < btn_y + 16);
        if (files_selected_idx >= 0) {
            vesa_draw_rect(del_btn_x, btn_y, 50, 16, hov_del ? 0x220505 : 0x110000);
            vesa_draw_rect_outline(del_btn_x, btn_y, 50, 16, hov_del ? 0xFF3333 : 0x550000);
            vesa_draw_string("Delete", del_btn_x + 4, btn_y + 4, 0xFF3333);
        }
    }
    
    // Separator line
    int list_y = top_y + 42;
    vesa_draw_line(content_x, list_y, wx + ww - 10, list_y, 0x1A4A2A);
    list_y += 6;
    
    // List file items
    int list_row_idx = 0;
    for (int i = 0; i < file_system_count; i++) {
        file_entry_t* f = &file_system[i];
        
        // Filter criteria
        int match = 0;
        if (files_search_len > 0) {
            // Search query active
            int found_sub = 0;
            for (int si = 0; f->name[si] != '\0'; si++) {
                int matched_sub = 1;
                for (int sj = 0; sj < files_search_len; sj++) {
                    if (f->name[si + sj] == '\0') { matched_sub = 0; break; }
                    char fsc = f->name[si + sj];
                    if (fsc >= 'A' && fsc <= 'Z') fsc = fsc - 'A' + 'a';
                    char ssc = files_search_query[sj];
                    if (ssc >= 'A' && ssc <= 'Z') ssc = ssc - 'A' + 'a';
                    if (fsc != ssc) { matched_sub = 0; break; }
                }
                if (matched_sub) { found_sub = 1; break; }
            }
            if (files_current_dir == 99) {
                match = found_sub && f->deleted;
            } else {
                match = found_sub && !f->deleted && !f->is_dir;
            }
        } else {
            // Directory view
            if (files_current_dir == 99) {
                match = f->deleted;
            } else {
                match = (!f->deleted && f->dir_id == files_current_dir);
            }
        }
        
        if (!match) continue;
        
        int ry = list_y + list_row_idx * 16;
        if (ry + 16 > wy + wh - 10) break;
        
        int selected = (files_selected_idx == i);
        extern int mouse_get_x(void);
        extern int mouse_get_y(void);
        int mx = mouse_get_x();
        int my = mouse_get_y();
        int hov_row = (mx >= content_x && mx < wx + ww - 10 && my >= ry && my < ry + 16);
        
        if (selected) {
            vesa_draw_rect(content_x, ry, ww - sidebar_w - 24, 15, 0x002211);
            vesa_draw_rect_outline(content_x, ry, ww - sidebar_w - 24, 15, gui_accent_col);
        } else if (hov_row) {
            vesa_draw_rect(content_x, ry, ww - sidebar_w - 24, 15, 0x0A2010);
        }
        
        uint32_t name_col = selected ? gui_accent_col : (hov_row ? 0x00FF88 : 0x88DDFF);
        char entry_str[80];
        int len = 0;
        
        if (f->is_dir) {
            const char* type_lbl = " [dir]  ";
            while (*type_lbl) entry_str[len++] = *type_lbl++;
            const char* name_ptr = f->name;
            while (*name_ptr) entry_str[len++] = *name_ptr++;
            entry_str[len] = '\0';
            vesa_draw_string(entry_str, content_x + 4, ry + 4, 0xFFAA00);
        } else {
            const char* type_lbl = " [file] ";
            while (*type_lbl) entry_str[len++] = *type_lbl++;
            const char* name_ptr = f->name;
            while (*name_ptr) entry_str[len++] = *name_ptr++;
            
            const char* sz_lbl = "   [";
            while (*sz_lbl) entry_str[len++] = *sz_lbl++;
            int size = f->size_kb;
            char size_str[10];
            int s_idx = 0;
            if (size == 0) size_str[s_idx++] = '0';
            else {
                while (size > 0 && s_idx < 9) {
                    size_str[s_idx++] = '0' + (size % 10);
                    size /= 10;
                }
            }
            for (int k = s_idx - 1; k >= 0; k--) entry_str[len++] = size_str[k];
            const char* sz_sfx = " KB]";
            while (*sz_sfx) entry_str[len++] = *sz_sfx++;
            
            if (files_search_len > 0 && files_current_dir != 99) {
                const char* dir_lbl = "  (in /";
                while (*dir_lbl) entry_str[len++] = *dir_lbl++;
                const char* dir_names[] = {"", "bin", "boot", "dev", "etc", "home", "downloads"};
                if (f->dir_id >= 0 && f->dir_id <= 6) {
                    const char* dname = dir_names[f->dir_id];
                    while (*dname) entry_str[len++] = *dname++;
                }
                entry_str[len++] = ')';
            }
            entry_str[len] = '\0';
            vesa_draw_string(entry_str, content_x + 4, ry + 4, name_col);
        }
        
        list_row_idx++;
    }
    
    vesa_draw_line(content_x, wy + wh - 22, wx + ww - 10, wy + wh - 22, 0x1A4A2A);
    char status_msg[64];
    format_stat(status_msg, "Total Files: ", file_system_count, " | AetherFS v1.1");
    vesa_draw_string(status_msg, content_x, wy + wh - 14, 0x007744);
}

static void gui_draw_notepad(void) {
    int mx = mouse_get_x();
    int my = mouse_get_y();
    int tb_y = notepad_window.y + 32;
    vesa_draw_rect(notepad_window.x + 4, tb_y, notepad_window.w - 8, 20, 0x05130A);
    vesa_draw_rect_outline(notepad_window.x + 4, tb_y, notepad_window.w - 8, 20, 0x007744);
    
    int hov_save = (mx >= notepad_window.x + 12 && mx < notepad_window.x + 62 && my >= tb_y + 2 && my < tb_y + 18);
    vesa_draw_string("[Save]", notepad_window.x + 12, tb_y + 4, hov_save ? gui_accent_col : 0x00AA55);
    
    int hov_save_as = (mx >= notepad_window.x + 72 && mx < notepad_window.x + 162 && my >= tb_y + 2 && my < tb_y + 18);
    vesa_draw_string("[Save As]", notepad_window.x + 72, tb_y + 4, hov_save_as ? gui_accent_col : 0x00AA55);

    int cx = notepad_window.x + 10, cy = notepad_window.y + 60;
    
    char line_buf[64];
    int li = 0;
    for (int i = 0; i < notepad_len; i++) {
        if (notepad_buf[i] == '\n') {
            line_buf[li] = '\0';
            vesa_draw_string(line_buf, cx, cy, 0x88DDFF);
            cy += 16;
            li = 0;
        } else {
            if (li < 63) line_buf[li++] = notepad_buf[i];
        }
    }
    if (li > 0) {
        line_buf[li] = '\0';
        vesa_draw_string(line_buf, cx, cy, 0x88DDFF);
    }
    
    /* Draw blinking text cursor */
    uint64_t ticks = pit_get_ticks();
    if ((ticks / 30) % 2 == 0 && gui_focus == FOCUS_NOTEPAD) {
        int cursor_x = cx + li * 8;
        vesa_draw_rect(cursor_x, cy, 8, 12, gui_accent_col);
    }

    if (notepad_save_dialog_open) {
        int dw = 320, dh = 180;
        int dx = notepad_window.x + (notepad_window.w - dw) / 2;
        int dy = notepad_window.y + (notepad_window.h - dh) / 2;
        
        vesa_draw_rect(dx, dy, dw, dh, 0x050F0A);
        vesa_draw_rect_outline(dx, dy, dw, dh, gui_accent_col);
        
        vesa_draw_string("Save File As...", dx + 10, dy + 10, gui_accent_col);
        vesa_draw_line(dx + 10, dy + 25, dx + dw - 10, dy + 25, 0x007744);
        
        vesa_draw_string("Directory Path:", dx + 10, dy + 35, 0x00AA55);
        vesa_draw_rect(dx + 10, dy + 50, dw - 20, 20, 0x020A05);
        vesa_draw_rect_outline(dx + 10, dy + 50, dw - 20, 20, (gui_focus == FOCUS_NOTEPAD_SAVE_PATH) ? gui_accent_col : 0x005522);
        vesa_draw_string(notepad_save_path, dx + 14, dy + 56, (gui_focus == FOCUS_NOTEPAD_SAVE_PATH) ? gui_accent_col : 0x00AA55);
        if ((ticks / 30) % 2 == 0 && gui_focus == FOCUS_NOTEPAD_SAVE_PATH) {
            int plen = 0; while (notepad_save_path[plen]) plen++;
            vesa_draw_line(dx + 14 + plen * 8, dy + 52, dx + 14 + plen * 8, dy + 68, gui_accent_col);
        }
        
        vesa_draw_string("File Name:", dx + 10, dy + 80, 0x00AA55);
        vesa_draw_rect(dx + 10, dy + 95, dw - 20, 20, 0x020A05);
        vesa_draw_rect_outline(dx + 10, dy + 95, dw - 20, 20, (gui_focus == FOCUS_NOTEPAD_SAVE_NAME) ? gui_accent_col : 0x005522);
        vesa_draw_string(notepad_save_name, dx + 14, dy + 101, (gui_focus == FOCUS_NOTEPAD_SAVE_NAME) ? gui_accent_col : 0x00AA55);
        if ((ticks / 30) % 2 == 0 && gui_focus == FOCUS_NOTEPAD_SAVE_NAME) {
            int nlen = 0; while (notepad_save_name[nlen]) nlen++;
            vesa_draw_line(dx + 14 + nlen * 8, dy + 97, dx + 14 + nlen * 8, dy + 113, gui_accent_col);
        }
        
        int btn_y = dy + 135;
        int s_hov = (mx >= dx + 50 && mx < dx + 130 && my >= btn_y && my < btn_y + 22);
        vesa_draw_rect(dx + 50, btn_y, 80, 22, s_hov ? 0x0A2010 : 0x010D05);
        vesa_draw_rect_outline(dx + 50, btn_y, 80, 22, s_hov ? gui_accent_col : 0x00AA55);
        vesa_draw_string("SAVE", dx + 74, btn_y + 6, s_hov ? gui_accent_col : 0x00AA55);
        
        int c_hov = (mx >= dx + 190 && mx < dx + 270 && my >= btn_y && my < btn_y + 22);
        vesa_draw_rect(dx + 190, btn_y, 80, 22, c_hov ? 0x0A2010 : 0x010D05);
        vesa_draw_rect_outline(dx + 190, btn_y, 80, 22, c_hov ? gui_accent_col : 0x00AA55);
        vesa_draw_string("CANCEL", dx + 208, btn_y + 6, c_hov ? gui_accent_col : 0x00AA55);
    }
}

static void gui_draw_wps(void) {
    int cx = wps_window.x + 10, cy = wps_window.y + 40;
    vesa_draw_string("WPS Office Writer - Document1", cx, cy, 0x3366FF); cy += 20;
    vesa_draw_rect(wps_window.x + 8, cy, wps_window.w - 16, 2, 0x3366FF); cy += 10;
    
    int paper_x = wps_window.x + 40;
    int paper_y = cy;
    int paper_w = wps_window.w - 80;
    int paper_h = wps_window.h - (cy - wps_window.y) - 20;
    vesa_draw_rect(paper_x, paper_y, paper_w, paper_h, 0xFFFFFF);
    vesa_draw_rect_outline(paper_x, paper_y, paper_w, paper_h, 0xCCCCCC);
    
    vesa_draw_string("AetherOS-64 Premium Office Suite", paper_x + 20, paper_y + 30, 0x111111);
    vesa_draw_string("--------------------------------", paper_x + 20, paper_y + 45, 0x555555);
    vesa_draw_string("This document is generated by WPS Writer.", paper_x + 20, paper_y + 60, 0x333333);
    vesa_draw_string("All features are fully operational.", paper_x + 20, paper_y + 75, 0x333333);
}

static void gui_draw_openoffice(void) {
    int cx = openoffice_window.x + 10, cy = openoffice_window.y + 40;
    vesa_draw_string("OpenOffice Writer - Untitled1", cx, cy, 0x00A2E8); cy += 20;
    vesa_draw_rect(openoffice_window.x + 8, cy, openoffice_window.w - 16, 2, 0x00A2E8); cy += 10;
    
    int paper_x = openoffice_window.x + 40;
    int paper_y = cy;
    int paper_w = openoffice_window.w - 80;
    int paper_h = openoffice_window.h - (cy - openoffice_window.y) - 20;
    vesa_draw_rect(paper_x, paper_y, paper_w, paper_h, 0xFAFAFA);
    vesa_draw_rect_outline(paper_x, paper_y, paper_w, paper_h, 0xCCCCCC);
    
    vesa_draw_string("OpenOffice 4.1.13 - Free Office Suite", paper_x + 20, paper_y + 30, 0x222222);
    vesa_draw_string("=====================================", paper_x + 20, paper_y + 45, 0x888888);
    vesa_draw_string("Welcome to your open source office.", paper_x + 20, paper_y + 60, 0x444444);
}

static void gui_draw_installer(void) {
    int cx = installer_window.x + 20, cy = installer_window.y + 40;
    const char* app_name = (installer_app_type == 0) ? "WPS Writer" : (installer_app_type == 1 ? "OpenOffice Writer" : "Google Chrome");
    
    if (installer_step == 0) {
        vesa_draw_string("Welcome to the Setup Wizard", cx, cy, gui_accent_col); cy += 24;
        
        char msg1[64];
        format_stat(msg1, "This wizard will install ", 0, "");
        int mi = 0;
        while (msg1[mi]) mi++;
        const char* suffix = app_name;
        while (*suffix && mi < 63) msg1[mi++] = *suffix++;
        msg1[mi] = '\0';
        
        vesa_draw_string(msg1, cx, cy, 0x00AA55); cy += 18;
        vesa_draw_string("on your AetherOS system.", cx, cy, 0x00AA55); cy += 36;
        vesa_draw_string("Click Next to continue or close to cancel.", cx, cy, 0x007744);
        
        int btn_w = 80, btn_h = 24;
        int btn_x = installer_window.w - btn_w - 20;
        int btn_y = installer_window.h - btn_h - 20;
        int mx = mouse_get_x();
        int my = mouse_get_y();
        int hov = (mx >= installer_window.x + btn_x && mx < installer_window.x + btn_x + btn_w && my >= installer_window.y + btn_y && my < installer_window.y + btn_y + btn_h);
        vesa_draw_rect(installer_window.x + btn_x, installer_window.y + btn_y, btn_w, btn_h, hov ? 0x0A2010 : 0x010D05);
        vesa_draw_rect_outline(installer_window.x + btn_x, installer_window.y + btn_y, btn_w, btn_h, hov ? gui_accent_col : 0x00AA55);
        vesa_draw_string("Next >", installer_window.x + btn_x + 18, installer_window.y + btn_y + 7, hov ? gui_accent_col : 0x00AA55);
    }
    else if (installer_step == 1) {
        vesa_draw_string("Installing files...", cx, cy, gui_accent_col); cy += 30;
        
        char msg2[64];
        format_stat(msg2, "Extracting components for ", 0, "");
        int mi = 0;
        while (msg2[mi]) mi++;
        const char* suffix = app_name;
        while (*suffix && mi < 63) msg2[mi++] = *suffix++;
        msg2[mi] = '\0';
        vesa_draw_string(msg2, cx, cy, 0x00AA55); cy += 36;
        
        int bar_w = installer_window.w - 80;
        int bar_h = 16;
        int bar_x = installer_window.x + 40;
        int bar_y = cy;
        vesa_draw_rect(bar_x, bar_y, bar_w, bar_h, 0x020A05);
        vesa_draw_rect_outline(bar_x, bar_y, bar_w, bar_h, 0x005522);
        int fill_w = (bar_w * installer_progress) / 100;
        if (fill_w > 0) {
            vesa_draw_rect(bar_x + 2, bar_y + 2, fill_w - 4, bar_h - 4, gui_accent_col);
        }
    }
    else if (installer_step == 2) {
        vesa_draw_string("Installation Complete!", cx, cy, gui_accent_col); cy += 24;
        
        char msg3[64];
        format_stat(msg3, "Successfully installed ", 0, "");
        int mi = 0;
        while (msg3[mi]) mi++;
        const char* suffix = app_name;
        while (*suffix && mi < 63) msg3[mi++] = *suffix++;
        msg3[mi] = '\0';
        vesa_draw_string(msg3, cx, cy, 0x00AA55); cy += 18;
        vesa_draw_string("A desktop shortcut has been created.", cx, cy, 0x007744); cy += 36;
        vesa_draw_string("Click Finish to close this wizard.", cx, cy, 0x007744);
        
        int btn_w = 80, btn_h = 24;
        int btn_x = installer_window.w - btn_w - 20;
        int btn_y = installer_window.h - btn_h - 20;
        int mx = mouse_get_x();
        int my = mouse_get_y();
        int hov = (mx >= installer_window.x + btn_x && mx < installer_window.x + btn_x + btn_w && my >= installer_window.y + btn_y && my < installer_window.y + btn_y + btn_h);
        vesa_draw_rect(installer_window.x + btn_x, installer_window.y + btn_y, btn_w, btn_h, hov ? 0x0A2010 : 0x010D05);
        vesa_draw_rect_outline(installer_window.x + btn_x, installer_window.y + btn_y, btn_w, btn_h, hov ? gui_accent_col : 0x00AA55);
        vesa_draw_string("Finish", installer_window.x + btn_x + 18, installer_window.y + btn_y + 7, hov ? gui_accent_col : 0x00AA55);
    }
}

static void gui_draw_calculator(void) {
    int cx = calc_window.x + 10, cy = calc_window.y + 40;
    /* display */
    vesa_draw_rect(cx, cy, calc_window.w - 20, 32, 0x020A05);
    vesa_draw_rect_outline(cx, cy, calc_window.w - 20, 32, gui_accent_col);
    
    // Draw the current calc_buf right-aligned
    int len = 0;
    while (calc_buf[len]) len++;
    vesa_draw_string(calc_buf, cx + calc_window.w - 30 - len * 8, cy + 12, gui_accent_col);
    cy += 42;
    
    /* button grid 4x5 */
    const char* keys[] = {"C", "/", "*", "<",
                          "7", "8", "9", "-",
                          "4", "5", "6", "+",
                          "1", "2", "3", "=",
                          " ", "0", "00", "="};
    int bw = (calc_window.w - 20) / 4, bh = 32;
    for (int r = 0; r < 5; r++) {
        for (int c2 = 0; c2 < 4; c2++) {
            int idx = r * 4 + c2;
            if (idx >= 20) break;
            int bx2 = cx + c2 * bw, by2 = cy + r * (bh + 4);
            
            int is_eq = (keys[idx][0] == '=');
            uint32_t bcol = is_eq ? 0x002211 : 0x020A05;
            uint32_t fcol = is_eq ? gui_accent_col : 0x00AA55;
            
            extern int mouse_get_x(void);
            extern int mouse_get_y(void);
            int mx = mouse_get_x();
            int my = mouse_get_y();
            int hov = (mx >= bx2 && mx < bx2 + bw - 2 && my >= by2 && my < by2 + bh);
            if (hov) {
                bcol = is_eq ? 0x004422 : 0x0A2010;
                fcol = gui_accent_col;
            }
            
            vesa_draw_rect(bx2, by2, bw - 2, bh, bcol);
            vesa_draw_rect_outline(bx2, by2, bw - 2, bh, fcol);
            int kl = 0; while (keys[idx][kl]) kl++;
            vesa_draw_string(keys[idx], bx2 + (bw - 2)/2 - kl*4, by2 + 12, fcol);
        }
    }
}

static void gui_draw_settings(void) {
    int wx = settings_window.x, wy = settings_window.y, ww = settings_window.w, wh = settings_window.h;
    int sidebar_w = 90;
    int content_x = wx + sidebar_w + 10;
    int start_y = wy + 40;
    extern int mouse_get_x(void);
    extern int mouse_get_y(void);
    int mx = mouse_get_x();
    int my = mouse_get_y();
    
    // Perform automatic page matching on settings search box input
    if (settings_search_len > 0) {
        if (str_contains_nocase("update kb503412 install restart patches system info OS RAM CPU Uptime", settings_search_query)) {
            settings_active_category = 0;
        } else if (str_contains_nocase("accent personalization theme color taskbar alignment center left background void matrix grid", settings_search_query)) {
            settings_active_category = 1;
        } else if (str_contains_nocase("wifi network internet gateway connection lan adapters starlink connected", settings_search_query)) {
            settings_active_category = 2;
        } else if (str_contains_nocase("security password user admin lock policy passcode active", settings_search_query)) {
            settings_active_category = 3;
        } else if (str_contains_nocase("about version author compiler build gcc architecture multiboot", settings_search_query)) {
            settings_active_category = 4;
        }
    }

    // Draw sidebar background
    vesa_draw_rect(wx + 1, wy + 33, sidebar_w, wh - 34, 0x070B0F);
    vesa_draw_line(wx + sidebar_w + 1, wy + 33, wx + sidebar_w + 1, wy + wh - 2, 0x1A4A2A);
    
    // Sidebar categories
    const char* cat_labels[] = {"System ", "Personal ", "Network ", "Security ", "About "};
    for (int i = 0; i < 5; i++) {
        int iy = wy + 42 + i * 24;
        int active = (settings_active_category == i);
        if (active) {
            vesa_draw_rect(wx + 4, iy, sidebar_w - 6, 20, 0x002211);
            vesa_draw_rect_outline(wx + 4, iy, sidebar_w - 6, 20, gui_accent_col);
        }
        
        extern int mouse_get_x(void);
        extern int mouse_get_y(void);
        int mx = mouse_get_x();
        int my = mouse_get_y();
        if (mx >= wx + 4 && mx < wx + sidebar_w - 2 && my >= iy && my < iy + 20) {
            if (!active) vesa_draw_rect(wx + 4, iy, sidebar_w - 6, 20, 0x0A2010);
        }
        
        vesa_draw_string(cat_labels[i], wx + 8, iy + 6, active ? gui_accent_col : 0x00AA55);
    }
    
    // Draw Settings search box in upper right corner of content panel
    int sbox_w = 130;
    int sbox_x = wx + ww - sbox_w - 10;
    int sbox_y = wy + 36;
    vesa_draw_rect(sbox_x, sbox_y, sbox_w, 18, 0x020A05);
    vesa_draw_rect_outline(sbox_x, sbox_y, sbox_w, 18, (gui_focus == FOCUS_SETTINGS_SEARCH) ? gui_accent_col : 0x1A4A2A);
    if (settings_search_len == 0) {
        vesa_draw_string("Search... ", sbox_x + 6, sbox_y + 5, 0x1A4A2A);
    } else {
        vesa_draw_string(settings_search_query, sbox_x + 6, sbox_y + 5, gui_accent_col);
        uint64_t ticks = pit_get_ticks();
        if ((ticks / 30) % 2 == 0 && gui_focus == FOCUS_SETTINGS_SEARCH) {
            vesa_draw_rect(sbox_x + 6 + settings_search_len * 8, sbox_y + 4, 2, 10, gui_accent_col);
        }
    }

    if (settings_active_category == 0) {
        vesa_draw_string("--- SYSTEM INFORMATION ---", content_x, start_y, 0x78909C);
        start_y += 18;
        
        uint64_t ticks = pit_get_ticks();
        uint32_t total_sec = (uint32_t)(ticks / 100);
        uint32_t sec = total_sec % 60;
        uint32_t min = (total_sec / 60) % 60;
        uint32_t hour = (total_sec / 3600) % 24;
        char up_str[16];
        format_time(up_str, hour, min, sec);
        
        char sys_time_str[32];
        char time_c[16], date_c[16];
        get_live_clock_string(time_c, date_c);
        int s_idx = 0;
        const char* d_ptr = date_c;
        while (*d_ptr) sys_time_str[s_idx++] = *d_ptr++;
        sys_time_str[s_idx++] = ' ';
        const char* t_ptr = time_c;
        while (*t_ptr) sys_time_str[s_idx++] = *t_ptr++;
        sys_time_str[s_idx] = '\0';
        
        struct { const char* key; const char* val; uint32_t col; } rows[] = {
            {"OS Name    ", "AetherOS-64 v1.2",     gui_accent_col},
            {"RAM        ", "4 GB Identity Map",   0x88DDFF},
            {"CPU        ", "x86_64 Freestanding", 0x88DDFF},
            {"Uptime     ", up_str,                  0xFFFF44},
            {"System Time", sys_time_str,            0x00FF88},
        };
        
        for (int i = 0; i < 5; i++) {
            vesa_draw_string(rows[i].key, content_x, start_y, 0x007744);
            vesa_draw_string(rows[i].val, content_x + 92, start_y, rows[i].col);
            start_y += 15;
            vesa_draw_line(content_x, start_y - 2, wx + ww - 10, start_y - 2, 0x0A2010);
        }
        
        start_y += 10;
        vesa_draw_string("--- AETHEROS UPDATE ---", content_x, start_y, 0x78909C);
        start_y += 18;
        
        extern int mouse_get_x(void);
        extern int mouse_get_y(void);
        int mx = mouse_get_x();
        int my = mouse_get_y();
        
        if (settings_update_stage == 0) {
            vesa_draw_string("Status: Updates Available (KB503412) ", content_x, start_y, 0xFFFF44);
            start_y += 14;
            
            int check_x = content_x;
            int hov_chk = (mx >= check_x && mx < check_x + 130 && my >= start_y && my < start_y + 20);
            vesa_draw_rect(check_x, start_y, 130, 20, hov_chk ? 0x0A2010 : 0x020A05);
            vesa_draw_rect_outline(check_x, start_y, 130, 20, hov_chk ? gui_accent_col : 0x00AA55);
            vesa_draw_string("Check & Install", check_x + 8, start_y + 6, 0x00FF88);
        }
        else if (settings_update_stage > 0 && settings_update_stage < 4) {
            if (ticks % 2 == 0 && settings_update_progress < 100) {
                settings_update_progress++;
                if (settings_update_progress == 30) settings_update_stage = 2;
                if (settings_update_progress == 75) settings_update_stage = 3;
            }
            if (settings_update_progress >= 100) {
                settings_update_stage = 4;
            }
            
            const char* stage_lbls[] = {"", "Checking for updates... ", "Downloading KB503412... ", "Installing patches... "};
            vesa_draw_string(stage_lbls[settings_update_stage], content_x, start_y, gui_accent_col);
            start_y += 14;
            
            int bar_w = 200;
            vesa_draw_rect_outline(content_x, start_y, bar_w, 14, 0x1A4A2A);
            int fill_w = (settings_update_progress * bar_w) / 100;
            if (fill_w > 0) {
                vesa_draw_rect(content_x + 1, start_y + 1, fill_w - 2, 12, gui_accent_col);
            }
        }
        else if (settings_update_stage == 4) {
            vesa_draw_string("Status: Install Complete. Restart Required. ", content_x, start_y, 0xFF3333);
            start_y += 14;
            
            int rst_x = content_x;
            int hov_rst = (mx >= rst_x && mx < rst_x + 110 && my >= start_y && my < start_y + 20);
            vesa_draw_rect(rst_x, start_y, 110, 20, hov_rst ? 0x220505 : 0x110000);
            vesa_draw_rect_outline(rst_x, start_y, 110, 20, hov_rst ? 0xFF3333 : 0x550000);
            vesa_draw_string("Restart Now", rst_x + 12, start_y + 6, 0xFF3333);
        }

        // Screen Resolution switcher
        start_y += 36;
        vesa_draw_string("--- DISPLAY RESOLUTION ---", content_x, start_y, 0x78909C);
        start_y += 18;
        
        const char* res_names[] = {"1024x768", "800x600 ", "640x480 "};
        uint32_t res_w[] = {1024, 800, 640};
        uint32_t res_h[] = {768, 600, 480};
        for (int i = 0; i < 3; i++) {
            int bx = content_x + i * 86;
            int hov_res = (mx >= bx && mx < bx + 80 && my >= start_y && my < start_y + 20);
            int active = (vesa_get_width() == res_w[i] && vesa_get_height() == res_h[i]);
            
            vesa_draw_rect(bx, start_y, 80, 20, active ? 0x0A2010 : (hov_res ? 0x0A2010 : 0x020A05));
            vesa_draw_rect_outline(bx, start_y, 80, 20, active ? gui_accent_col : 0x1A4A2A);
            vesa_draw_string(res_names[i], bx + 8, start_y + 6, active ? gui_accent_col : 0x00AA55);
        }
    }
    else if (settings_active_category == 1) {
        vesa_draw_string("--- PERSONALIZATION ---", content_x, start_y, 0x78909C);
        start_y += 18;
        
        vesa_draw_string("ACCENT THEME COLOR: ", content_x, start_y, 0x007744);
        start_y += 16;
        
        const char* color_names[] = {"GREEN ", "CYAN ", "AMBER ", "RED "};
        uint32_t colors[] = {0x00FF88, 0x00E5FF, 0xFF9100, 0xFF3333};
        uint32_t bgs[] = {0x002211, 0x001B2B, 0x241100, 0x220505};
        uint32_t hov_bgs[] = {0x0C2512, 0x0A253A, 0x301E0A, 0x3A0A0A};
        
        extern int mouse_get_x(void);
        extern int mouse_get_y(void);
        int mx = mouse_get_x();
        int my = mouse_get_y();
        
        for (int i = 0; i < 4; i++) {
            int bx = content_x + i * 72;
            int hov_c = (mx >= bx && mx < bx + 64 && my >= start_y && my < start_y + 20);
            int active = (gui_accent_col == colors[i]);
            
            vesa_draw_rect(bx, start_y, 64, 20, active ? hov_bgs[i] : (hov_c ? hov_bgs[i] : bgs[i]));
            vesa_draw_rect_outline(bx, start_y, 64, 20, active ? gui_accent_col : 0x1A4A2A);
            vesa_draw_string(color_names[i], bx + 12, start_y + 6, colors[i]);
        }
        
        start_y += 34;
        vesa_draw_string("TASKBAR ALIGNMENT: ", content_x, start_y, 0x007744);
        start_y += 16;
        
        int align_x1 = content_x;
        int align_x2 = content_x + 90;
        int hov_a1 = (mx >= align_x1 && mx < align_x1 + 80 && my >= start_y && my < start_y + 20);
        int hov_a2 = (mx >= align_x2 && mx < align_x2 + 80 && my >= start_y && my < start_y + 20);
        
        vesa_draw_rect(align_x1, start_y, 80, 20, taskbar_centered ? 0x0A2010 : (hov_a1 ? 0x0A2010 : 0x020A05));
        vesa_draw_rect_outline(align_x1, start_y, 80, 20, taskbar_centered ? gui_accent_col : 0x1A4A2A);
        vesa_draw_string("Center", align_x1 + 16, start_y + 6, taskbar_centered ? gui_accent_col : 0x00AA55);
        
        vesa_draw_rect(align_x2, start_y, 80, 20, !taskbar_centered ? 0x0A2010 : (hov_a2 ? 0x0A2010 : 0x020A05));
        vesa_draw_rect_outline(align_x2, start_y, 80, 20, !taskbar_centered ? gui_accent_col : 0x1A4A2A);
        vesa_draw_string("Left", align_x2 + 24, start_y + 6, !taskbar_centered ? gui_accent_col : 0x00AA55);
        
        start_y += 34;
        vesa_draw_string("DESKTOP BACKGROUND: ", content_x, start_y, 0x007744);
        start_y += 16;
        
        const char* wp_names[] = {"Tech Grid ", "Void Mode ", "Matrix ", "Cyberpunk ", "Nebula "};
        for (int i = 0; i < 5; i++) {
            int bx = content_x + (i % 3) * 96;
            int by = start_y + (i / 3) * 24;
            int hov_wp = (mx >= bx && mx < bx + 90 && my >= by && my < by + 20);
            int active = (wallpaper_style == i);
            
            vesa_draw_rect(bx, by, 90, 20, active ? 0x0A2010 : (hov_wp ? 0x0A2010 : 0x020A05));
            vesa_draw_rect_outline(bx, by, 90, 20, active ? gui_accent_col : 0x1A4A2A);
            vesa_draw_string(wp_names[i], bx + 6, by + 6, active ? gui_accent_col : 0x00AA55);
        }

        // Taskbar Position (Top vs Bottom)
        start_y += 48;
        vesa_draw_string("TASKBAR POSITION: ", content_x, start_y, 0x007744);
        start_y += 16;
        
        int pos_x1 = content_x;
        int pos_x2 = content_x + 90;
        int hov_p1 = (mx >= pos_x1 && mx < pos_x1 + 80 && my >= start_y && my < start_y + 20);
        int hov_p2 = (mx >= pos_x2 && mx < pos_x2 + 80 && my >= start_y && my < start_y + 20);
        
        vesa_draw_rect(pos_x1, start_y, 80, 20, (taskbar_position == 0) ? 0x0A2010 : (hov_p1 ? 0x0A2010 : 0x020A05));
        vesa_draw_rect_outline(pos_x1, start_y, 80, 20, (taskbar_position == 0) ? gui_accent_col : 0x1A4A2A);
        vesa_draw_string("Top", pos_x1 + 28, start_y + 6, (taskbar_position == 0) ? gui_accent_col : 0x00AA55);
        
        vesa_draw_rect(pos_x2, start_y, 80, 20, (taskbar_position == 1) ? 0x0A2010 : (hov_p2 ? 0x0A2010 : 0x020A05));
        vesa_draw_rect_outline(pos_x2, start_y, 80, 20, (taskbar_position == 1) ? gui_accent_col : 0x1A4A2A);
        vesa_draw_string("Bottom", pos_x2 + 16, start_y + 6, (taskbar_position == 1) ? gui_accent_col : 0x00AA55);

        // Lockscreen Style (Default vs Glitch vs Matrix)
        start_y += 34;
        vesa_draw_string("LOCK SCREEN STYLE: ", content_x, start_y, 0x007744);
        start_y += 16;
        
        const char* ls_names[] = {"Default HUD ", "Cyber Glitch ", "Matrix style "};
        for (int i = 0; i < 3; i++) {
            int bx = content_x + i * 96;
            int hov_ls = (mx >= bx && mx < bx + 90 && my >= start_y && my < start_y + 20);
            int active = (lockscreen_style == i);
            
            vesa_draw_rect(bx, start_y, 90, 20, active ? 0x0A2010 : (hov_ls ? 0x0A2010 : 0x020A05));
            vesa_draw_rect_outline(bx, start_y, 90, 20, active ? gui_accent_col : 0x1A4A2A);
            vesa_draw_string(ls_names[i], bx + 6, start_y + 6, active ? gui_accent_col : 0x00AA55);
        }
    }
    else if (settings_active_category == 2) {
        vesa_draw_string("--- NETWORK ADAPTERS ---", content_x, start_y, 0x78909C);
        start_y += 18;
        
        vesa_draw_string("Wi-Fi Connection Toggle: ", content_x, start_y, 0x007744);
        start_y += 16;
        
        extern int mouse_get_x(void);
        extern int mouse_get_y(void);
        int mx = mouse_get_x();
        int my = mouse_get_y();
        
        int tog_x = content_x;
        int hov_tog = (mx >= tog_x && mx < tog_x + 90 && my >= start_y && my < start_y + 22);
        vesa_draw_rect(tog_x, start_y, 90, 22, settings_wifi_enabled ? 0x002211 : (hov_tog ? 0x220505 : 0x020A05));
        vesa_draw_rect_outline(tog_x, start_y, 90, 22, settings_wifi_enabled ? gui_accent_col : 0xFF3333);
        vesa_draw_string(settings_wifi_enabled ? "[ WI-FI ON ]" : "[ WI-FI OFF ]", tog_x + 6, start_y + 7, settings_wifi_enabled ? gui_accent_col : 0xFF3333);
        
        start_y += 34;
        if (settings_wifi_enabled) {
            vesa_draw_string("CONNECTED NETWORKS: ", content_x, start_y, 0x78909C);
            start_y += 16;
            vesa_draw_string("Aether_Gateway_5G   - Connected", content_x, start_y, gui_accent_col); start_y += 15;
            vesa_draw_string("Qemu_Virtual_LAN    - Saved", content_x, start_y, 0x00AA55); start_y += 15;
            vesa_draw_string("Starlink_Satellite  - Signal 40%", content_x, start_y, 0x005522);
        } else {
            vesa_draw_string("Network adapters disabled. ", content_x, start_y, 0x550000);
        }
    }
    else if (settings_active_category == 3) {
        vesa_draw_string("--- SECURITY & KEY ---", content_x, start_y, 0x78909C);
        start_y += 18;
        vesa_draw_string("Secure Boot Status: Enabled (VBIOS) ", content_x, start_y, 0x00AA55); start_y += 16;
        vesa_draw_string("Current User:       Aether_Admin ", content_x, start_y, 0x00AA55); start_y += 16;
        
        extern int mouse_get_x(void);
        extern int mouse_get_y(void);
        int mx = mouse_get_x();
        int my = mouse_get_y();
        uint64_t ticks = pit_get_ticks();

        // Current Password Field
        vesa_draw_string("Current Password:", content_x, start_y, 0x007744);
        int cur_pass_box_y = start_y + 12;
        vesa_draw_rect(content_x, cur_pass_box_y, 180, 18, 0x020A05);
        vesa_draw_rect_outline(content_x, cur_pass_box_y, 180, 18, (gui_focus == FOCUS_SETTINGS_CUR_PASS) ? gui_accent_col : 0x1A4A2A);
        
        // Render asterisks
        for (int i = 0; i < settings_cur_pass_len; i++) {
            vesa_draw_char('*', content_x + 6 + i * 8, cur_pass_box_y + 5, gui_accent_col);
        }
        if ((ticks / 30) % 2 == 0 && gui_focus == FOCUS_SETTINGS_CUR_PASS) {
            vesa_draw_rect(content_x + 6 + settings_cur_pass_len * 8, cur_pass_box_y + 4, 2, 10, gui_accent_col);
        }
        start_y += 36;

        // New Password Field
        vesa_draw_string("New Password:", content_x, start_y, 0x007744);
        int new_pass_box_y = start_y + 12;
        vesa_draw_rect(content_x, new_pass_box_y, 180, 18, 0x020A05);
        vesa_draw_rect_outline(content_x, new_pass_box_y, 180, 18, (gui_focus == FOCUS_SETTINGS_NEW_PASS) ? gui_accent_col : 0x1A4A2A);
        
        // Render asterisks
        for (int i = 0; i < settings_new_pass_len; i++) {
            vesa_draw_char('*', content_x + 6 + i * 8, new_pass_box_y + 5, gui_accent_col);
        }
        if ((ticks / 30) % 2 == 0 && gui_focus == FOCUS_SETTINGS_NEW_PASS) {
            vesa_draw_rect(content_x + 6 + settings_new_pass_len * 8, new_pass_box_y + 4, 2, 10, gui_accent_col);
        }
        start_y += 36;

        // Change Password Button
        int change_btn_x = content_x;
        int change_btn_y = start_y + 6;
        int change_btn_w = 120;
        int change_btn_h = 20;
        int hov_change = (mx >= change_btn_x && mx < change_btn_x + change_btn_w && my >= change_btn_y && my < change_btn_y + change_btn_h);
        vesa_draw_rect(change_btn_x, change_btn_y, change_btn_w, change_btn_h, hov_change ? 0x0A2010 : 0x020A05);
        vesa_draw_rect_outline(change_btn_x, change_btn_y, change_btn_w, change_btn_h, gui_accent_col);
        vesa_draw_string("Change Pass ", change_btn_x + 12, change_btn_y + 6, gui_accent_col);

        // Feedback status message
        if (settings_pass_status[0] != '\0') {
            uint32_t status_col = str_contains_nocase(settings_pass_status, "Success") ? 0x00FF88 : 0xFF3333;
            vesa_draw_string(settings_pass_status, content_x, change_btn_y + 26, status_col);
        }
    }
    else if (settings_active_category == 4) {
        vesa_draw_string("--- ABOUT AETHEROS ---", content_x, start_y, 0x78909C);
        start_y += 18;
        vesa_draw_string("AetherOS-64 Operating System ", content_x, start_y, gui_accent_col); start_y += 16;
        vesa_draw_string("Version 1.2 (Freestanding kernel) ", content_x, start_y, 0x00AA55); start_y += 16;
        vesa_draw_string("Custom graphical environment & double buffer ", content_x, start_y, 0x00AA55); start_y += 16;
        vesa_draw_string("Compiler: gcc (Ubuntu 11.4.0) x86_64 ", content_x, start_y, 0x005522); start_y += 16;
        vesa_draw_string("Architecture: Multiboot2, GRUB2 loaders ", content_x, start_y, 0x005522);
        
        start_y += 24;
        vesa_draw_string("--- HARDWARE DEVICE MANAGER ---", content_x, start_y, 0x78909C);
        start_y += 18;
        
        struct { const char* dev; const char* status; } devices[] = {
            {"CPU Processor  ", "x86_64 Core (1 thread)"},
            {"Video Adapter  ", "VESA Linear Framebuffer"},
            {"Pointer Device ", "PS/2 Mouse (IRQ 12)"},
            {"Input Keyboard ", "PS/2 Keyboard (IRQ 1)"},
            {"System Timer   ", "PIT 8253 (IRQ 0)"},
            {"Memory dynamic ", "PMM Frame Allocator"}
        };
        for (int i = 0; i < 6; i++) {
            vesa_draw_string(devices[i].dev, content_x, start_y, 0x007744);
            vesa_draw_string(devices[i].status, content_x + 120, start_y, gui_accent_col);
            start_y += 14;
        }
    }
}

static void gui_draw_browser(void) {
    int wx = browser_window.x, wy = browser_window.y, ww = browser_window.w, wh = browser_window.h;
    int content_x = wx + 10;

    extern int mouse_get_x(void);
    extern int mouse_get_y(void);
    int mx = mouse_get_x();
    int my = mouse_get_y();
    extern uint64_t pit_get_ticks(void);
    uint64_t ticks = pit_get_ticks();
    (void)ticks;

    /* ── Chrome-style tab strip ── */
    int tab_strip_y = wy + 32;
    int tab_strip_h = 20;
    vesa_draw_rect(wx, tab_strip_y, ww, tab_strip_h, 0x202124); /* dark tab strip bg */

    /* Define tabs: Google / YouTube (when active) / Aether OS / Wiki */
    const char* tab_labels[4];
    int tab_count = 3;
    tab_labels[0] = browser_youtube_mode ? " youtube.com " : " google.com ";
    tab_labels[1] = " aether://home ";
    tab_labels[2] = " aether://wiki ";
    int tab_widths[4] = {104, 116, 116, 0};

    int tx = wx + 4;
    for (int i = 0; i < tab_count; i++) {
        int active = (browser_active_tab == i);
        int tw = tab_widths[i];
        uint32_t tab_bg = active ? 0x35363A : 0x202124;
        uint32_t tab_fg = active ? 0xE8EAED : 0x9AA0A6;

        if (active) {
            vesa_draw_rect(tx, tab_strip_y, tw, tab_strip_h, tab_bg);
            /* bottom highlight on active tab */
            vesa_draw_rect(tx, tab_strip_y + tab_strip_h - 2, tw, 2, 0x8AB4F8);
        } else {
            int hov_t = (mx >= tx && mx < tx + tw && my >= tab_strip_y && my < tab_strip_y + tab_strip_h);
            if (hov_t) vesa_draw_rect(tx, tab_strip_y, tw, tab_strip_h, 0x292B2D);
        }
        /* favicon dot */
        if (i == 0 && !browser_youtube_mode)
            vesa_draw_rect(tx + 6, tab_strip_y + 8, 6, 6, 0x4285F4);
        else if (i == 0 && browser_youtube_mode)
            vesa_draw_rect(tx + 6, tab_strip_y + 8, 6, 6, 0xFF0000);
        else
            vesa_draw_rect(tx + 6, tab_strip_y + 8, 6, 6, 0x00AA55);

        vesa_draw_string(tab_labels[i], tx + 16, tab_strip_y + 6, tab_fg);
        /* X close button */
        vesa_draw_string("x", tx + tw - 12, tab_strip_y + 6, active ? 0x9AA0A6 : 0x5F6368);
        tx += tw + 2;
    }
    /* New tab + button */
    vesa_draw_string("+", tx + 4, tab_strip_y + 6, 0x9AA0A6);

    /* ── Navigation toolbar ── */
    int nav_y = wy + 52;
    int nav_h = 28;
    vesa_draw_rect(wx, nav_y, ww, nav_h, 0x35363A); /* toolbar bg */

    /* Back button */
    int back_x = wx + 6;
    int hov_back = (mx >= back_x && mx < back_x + 22 && my >= nav_y + 2 && my < nav_y + nav_h - 2);
    vesa_draw_rect(back_x, nav_y + 4, 22, 20, hov_back && browser_can_go_back ? 0x4A4B4F : 0x35363A);
    vesa_draw_string(browser_can_go_back ? "<" : "<", back_x + 7, nav_y + 10,
                     browser_can_go_back ? 0xE8EAED : 0x5F6368);

    /* Forward button */
    int fwd_x = back_x + 24;
    int hov_fwd = (mx >= fwd_x && mx < fwd_x + 22 && my >= nav_y + 2 && my < nav_y + nav_h - 2);
    vesa_draw_rect(fwd_x, nav_y + 4, 22, 20, hov_fwd ? 0x4A4B4F : 0x35363A);
    vesa_draw_string(">", fwd_x + 7, nav_y + 10, 0x5F6368);

    /* Refresh button */
    int ref_x = fwd_x + 26;
    int hov_ref = (mx >= ref_x && mx < ref_x + 22 && my >= nav_y + 2 && my < nav_y + nav_h - 2);
    vesa_draw_rect(ref_x, nav_y + 4, 22, 20, hov_ref ? 0x4A4B4F : 0x35363A);
    if (browser_loading) {
        int lf = (int)(ticks / 3) % 8;
        const char* spin[] = {"-","\\","|","/","-","\\","|","/"};
        vesa_draw_string(spin[lf], ref_x + 7, nav_y + 10, 0x8AB4F8);
    } else {
        vesa_draw_string("o", ref_x + 7, nav_y + 10, 0xE8EAED);
    }

    /* Home button */
    int home_x = ref_x + 26;
    int hov_home = (mx >= home_x && mx < home_x + 22 && my >= nav_y + 2 && my < nav_y + nav_h - 2);
    vesa_draw_rect(home_x, nav_y + 4, 22, 20, hov_home ? 0x4A4B4F : 0x35363A);
    vesa_draw_string("^", home_x + 7, nav_y + 10, hov_home ? 0xE8EAED : 0xBDC1C6);

    /* Address bar */
    int addr_x = home_x + 28;
    int addr_w = ww - (addr_x - wx) - 54;
    int addr_h = 22;
    int addr_y_off = nav_y + 3;
    uint32_t addr_bg = (gui_focus == FOCUS_BROWSER_URL) ? 0x282A2D : 0x202124;
    uint32_t addr_bd = (gui_focus == FOCUS_BROWSER_URL) ? 0x8AB4F8 : 0x5F6368;
    vesa_draw_rect(addr_x, addr_y_off, addr_w, addr_h, addr_bg);
    vesa_draw_rect_outline(addr_x, addr_y_off, addr_w, addr_h, addr_bd);

    /* Lock icon */
    vesa_draw_string("A", addr_x + 6, addr_y_off + 7, 0x81C995); /* green = secure */

    /* URL text */
    char disp_url[128];
    if (gui_focus == FOCUS_BROWSER_URL && browser_url_len > 0) {
        int dl = 0; const char* p = browser_url;
        while (*p && dl < 126) disp_url[dl++] = *p++;
        disp_url[dl] = '\0';
        vesa_draw_string(disp_url, addr_x + 18, addr_y_off + 7, 0xE8EAED);
        if ((ticks / 25) % 2 == 0)
            vesa_draw_rect(addr_x + 18 + browser_url_len * 8, addr_y_off + 5, 2, 12, 0x8AB4F8);
    } else {
        /* Show current page URL */
        if (browser_youtube_mode == 1) {
            if (browser_yt_search_len > 0) {
                int dl = 0; const char* p = "https://www.youtube.com/results?search_query=";
                while (*p && dl < 126) disp_url[dl++] = *p++;
                p = browser_yt_search;
                while (*p && dl < 126) disp_url[dl++] = *p++;
                disp_url[dl] = '\0';
            } else {
                int dl = 0; const char* p = "https://www.youtube.com";
                while (*p && dl < 126) disp_url[dl++] = *p++;
                disp_url[dl] = '\0';
            }
            vesa_draw_string(disp_url, addr_x + 18, addr_y_off + 7, 0xE8EAED);
        } else if (browser_youtube_mode == 2) {
            vesa_draw_string("https://www.youtube.com/watch?v=aetherOS64", addr_x + 18, addr_y_off + 7, 0xE8EAED);
        } else if (browser_has_searched && browser_active_tab == 0) {
            int dl = 0; const char* p = "https://www.google.com/search?q=";
            while (*p && dl < 80) disp_url[dl++] = *p++;
            p = browser_last_search;
            while (*p && dl < 126) disp_url[dl++] = *p++;
            disp_url[dl] = '\0';
            vesa_draw_string(disp_url, addr_x + 18, addr_y_off + 7, 0xE8EAED);
        } else if (browser_active_tab == 0) {
            vesa_draw_string("https://www.google.com", addr_x + 18, addr_y_off + 7, 0xE8EAED);
        } else if (browser_active_tab == 1) {
            vesa_draw_string("aether://home", addr_x + 18, addr_y_off + 7, 0xE8EAED);
        } else {
            vesa_draw_string("aether://wiki", addr_x + 18, addr_y_off + 7, 0xE8EAED);
        }
    }

    /* Menu (⋮) button */
    int menu_x = addr_x + addr_w + 4;
    int hov_menu = (mx >= menu_x && mx < menu_x + 28 && my >= nav_y + 2 && my < nav_y + nav_h - 2);
    vesa_draw_rect(menu_x, nav_y + 4, 28, 20, hov_menu ? 0x4A4B4F : 0x35363A);
    vesa_draw_string("...", menu_x + 8, nav_y + 10, hov_menu ? 0xE8EAED : 0xBDC1C6);

    /* Separator line under toolbar */
    vesa_draw_line(wx, nav_y + nav_h, wx + ww, nav_y + nav_h, 0x1A1A1A);

    int content_y = nav_y + nav_h + 6;

    /* ── Audio status bar (shown when audio is playing) ── */
    if (audio_playing) {
        int ab_y = wy + wh - 26;
        vesa_draw_rect(wx, ab_y, ww, 26, 0x1A1A2E);
        vesa_draw_rect_outline(wx, ab_y, ww, 26, 0x333355);
        /* Equalizer bars animation */
        for (int b = 0; b < 12; b++) {
            int phase = (int)(ticks / 2 + b * 7) % 10;
            int bh = (phase > 5 ? (10 - phase) : phase) * 2 + 2;
            int bx = wx + 10 + b * 10;
            int by = ab_y + 22 - bh;
            vesa_draw_rect(bx, by, 7, bh, b % 3 == 0 ? 0xFF3333 : (b % 3 == 1 ? 0xFF9900 : 0xFF3333));
        }
        /* Track title */
        vesa_draw_string(audio_title, wx + 140, ab_y + 9, 0xFFFFFF);
        /* Volume */
        int vol_x = wx + ww - 100;
        vesa_draw_string("Vol:", vol_x, ab_y + 9, 0x888888);
        int vbar_x = vol_x + 36;
        vesa_draw_rect(vbar_x, ab_y + 12, 60, 4, 0x333333);
        vesa_draw_rect(vbar_x, ab_y + 12, (60 * audio_volume) / 100, 4, 0xFF3333);
        /* Pause symbol */
        vesa_draw_string("[||]", wx + 100, ab_y + 9, 0xEEEEEE);
    }


    if (video_playing) {
        vesa_draw_string("AetherTube Media Player", content_x, content_y, 0x88DDFF);
        int back_btn_x = wx + ww - 140;
        int hov_back_search = (mx >= back_btn_x && mx < back_btn_x + 120 && my >= content_y - 2 && my < content_y + 14);
        vesa_draw_string("[Back to list]", back_btn_x, content_y, hov_back_search ? 0xFF3333 : 0x00AA55);
        content_y += 20;
        int play_w = 480;
        int play_h = 220;
        int play_x = wx + (ww - play_w) / 2;
        int play_y = content_y;
        vesa_draw_rect(play_x, play_y, play_w, play_h, 0x020A05);
        vesa_draw_rect_outline(play_x, play_y, play_w, play_h, gui_accent_col);
        draw_ascii_frame(play_x + 10, play_y + 10, play_w - 20, play_h - 20, video_active_idx, ticks, 1);
        content_y += play_h + 10;
        int sb_x = wx + (ww - 400) / 2;
        int sb_y = content_y;
        int sb_w = 400;
        int sb_h = 8;
        vesa_draw_rect(sb_x, sb_y, sb_w, sb_h, 0x001105);
        vesa_draw_rect_outline(sb_x, sb_y, sb_w, sb_h, 0x005522);
        int fill_w = (sb_w * video_play_pos) / 100;
        vesa_draw_rect(sb_x, sb_y, fill_w, sb_h, gui_accent_col);
        vesa_draw_rect(sb_x + fill_w - 4, sb_y - 2, 8, 12, 0xFFFFFF);
        content_y += sb_h + 10;
        char time_str[32];
        int total_seconds = (video_active_idx == 0) ? 220 : ((video_active_idx == 1) ? 75 : 150);
        int cur_seconds = (video_play_pos * total_seconds) / 100;
        int cur_min = cur_seconds / 60;
        int cur_sec = cur_seconds % 60;
        int tot_min = total_seconds / 60;
        int tot_sec = total_seconds % 60;
        format_stat(time_str, "Time: 0", cur_min, "");
        int t_len = 0;
        while (time_str[t_len]) t_len++;
        time_str[t_len++] = ':';
        time_str[t_len++] = '0' + (cur_sec / 10);
        time_str[t_len++] = '0' + (cur_sec % 10);
        time_str[t_len++] = ' ';
        time_str[t_len++] = '/';
        time_str[t_len++] = ' ';
        time_str[t_len++] = '0' + tot_min;
        time_str[t_len++] = ':';
        time_str[t_len++] = '0' + (tot_sec / 10);
        time_str[t_len++] = '0' + (tot_sec % 10);
        time_str[t_len] = '\0';
        vesa_draw_string(time_str, sb_x, content_y, 0x00FF88);
        int ctrl_x = sb_x + 200;
        int hov_play = (mx >= ctrl_x && mx < ctrl_x + 60 && my >= content_y - 4 && my < content_y + 14);
        int hov_fs   = (mx >= ctrl_x + 70 && mx < ctrl_x + 160 && my >= content_y - 4 && my < content_y + 14);
        vesa_draw_string(video_is_playing ? "[Pause]" : "[Play] ", ctrl_x, content_y, hov_play ? 0xFFFFFF : 0x00AA55);
        vesa_draw_string("[Fullscreen]", ctrl_x + 70, content_y, hov_fs ? 0xFFFFFF : 0x00AA55);
        return;
    }

    // 3. Render Tab Contents
    if (!settings_wifi_enabled && browser_active_tab == 0) {
        // Draw Chrome-style offline dinosaur page
        int err_x = wx + (ww - 240) / 2;
        int err_y = wy + 110;
        vesa_draw_rect(err_x, err_y, 240, 150, 0x110505);
        vesa_draw_rect_outline(err_x, err_y, 240, 150, 0xFF3333);
        
        // draw a small green ASCII dinosaur
        vesa_draw_string("  __       ", err_x + 80, err_y + 15, 0x00FF88);
        vesa_draw_string(" / _)  O_O ", err_x + 80, err_y + 25, 0x00FF88);
        vesa_draw_string("| (_/\\  L  ", err_x + 80, err_y + 35, 0x00FF88);
        vesa_draw_string(" \\__) \\    ", err_x + 80, err_y + 45, 0x00FF88);
        vesa_draw_string("  / /  / / ", err_x + 80, err_y + 55, 0x00FF88);
        
        vesa_draw_string("ERR_INTERNET_DISCONNECTED", err_x + 28, err_y + 85, 0xFF3333);
        vesa_draw_string("AetherNet connection is offline.", err_x + 10, err_y + 105, 0x888888);
        vesa_draw_string("Turn on Wi-Fi in settings tray.", err_x + 15, err_y + 122, 0x00FF88);
        return;
    }

    /* ═══════════════════════════════════════════════════════
     * YOUTUBE PAGE  (browser_youtube_mode 1 or 2)
     * ═══════════════════════════════════════════════════════ */
    if (browser_youtube_mode >= 1) {

        /* YouTube header bar */
        int yt_hdr_y = content_y;
        int yt_hdr_h = 36;
        vesa_draw_rect(wx, yt_hdr_y, ww, yt_hdr_h, 0x0F0F0F);
        vesa_draw_line(wx, yt_hdr_y + yt_hdr_h, wx + ww, yt_hdr_y + yt_hdr_h, 0x272727);

        /* YouTube logo: red [YOU] white TUBE */
        vesa_draw_rect(content_x, yt_hdr_y + 8, 48, 20, 0xFF0000);
        vesa_draw_string("YOU", content_x + 6, yt_hdr_y + 14, 0xFFFFFF);
        vesa_draw_string("TUBE", content_x + 54, yt_hdr_y + 14, 0xFFFFFF);

        /* YouTube search bar */
        int yt_sb_x = content_x + 110;
        int yt_sb_w  = ww - 260;
        int yt_sb_h  = 24;
        int yt_sb_y  = yt_hdr_y + 6;
        vesa_draw_rect(yt_sb_x, yt_sb_y, yt_sb_w, yt_sb_h, 0x121212);
        vesa_draw_rect_outline(yt_sb_x, yt_sb_y, yt_sb_w, yt_sb_h,
            (gui_focus == FOCUS_BROWSER_SEARCH) ? 0x3EA6FF : 0x303030);
        if (browser_yt_search_len == 0) {
            vesa_draw_string("Search", yt_sb_x + 10, yt_sb_y + 8, 0x717171);
        } else {
            vesa_draw_string(browser_yt_search, yt_sb_x + 10, yt_sb_y + 8, 0xFFFFFF);
        }
        /* Search icon button */
        int yt_srch_btn_x = yt_sb_x + yt_sb_w;
        vesa_draw_rect(yt_srch_btn_x, yt_sb_y, 34, yt_sb_h, 0x222222);
        vesa_draw_rect_outline(yt_srch_btn_x, yt_sb_y, 34, yt_sb_h, 0x303030);
        vesa_draw_string("O/", yt_srch_btn_x + 10, yt_sb_y + 8, 0xAAAAAA);

        content_y = yt_hdr_y + yt_hdr_h + 8;

        if (browser_youtube_mode == 2) {
            /* ── VIDEO PLAYER PAGE ── */
            /* Determine video content based on search */
            const char* vid_titles[8] = {
                "Lofi Hip Hop Radio - Beats to Study",
                "Top 10 Songs This Week 2025",
                "How to Build a 64-bit OS - Full Guide",
                "Minecraft Survival - Episode 1",
                "Coding a Browser in C from Scratch",
                "Fortnite Season 12 Highlights",
                "Lo-Fi Chill Mix | 2 Hours",
                "AetherOS Desktop Tour"
            };
            int vi = audio_video_idx % 8;

            /* Player area */
            int pl_w = ww - 280;
            int pl_h = pl_w * 9 / 16;
            if (pl_h > 300) pl_h = 300;
            int pl_x = content_x;
            int pl_y = content_y;
            vesa_draw_rect(pl_x, pl_y, pl_w, pl_h, 0x050505);
            vesa_draw_rect_outline(pl_x, pl_y, pl_w, pl_h, 0x272727);

            /* Animated video frame (equalizer/animation) */
            if (audio_playing) {
                for (int b = 0; b < 20; b++) {
                    int ph = (int)(ticks / 2 + b * 9) % 12;
                    int bh = (ph > 6 ? (12-ph) : ph) * (pl_h / 14) + 6;
                    int bx = pl_x + 10 + b * ((pl_w - 20) / 20);
                    int by = pl_y + pl_h - bh - 4;
                    uint32_t bc = (b % 3 == 0) ? 0xFF3333 : (b % 3 == 1) ? 0xFF9900 : 0xFF6600;
                    vesa_draw_rect(bx, by, (pl_w - 20) / 22, bh, bc);
                }
                vesa_draw_string("NOW PLAYING", pl_x + pl_w/2 - 44, pl_y + 10, 0xFF3333);
            } else {
                /* Paused - show play button */
                int pp_x = pl_x + pl_w/2 - 20;
                int pp_y = pl_y + pl_h/2 - 20;
                vesa_draw_rect(pp_x, pp_y, 40, 40, 0xCC0000);
                vesa_draw_string(">", pp_x + 14, pp_y + 14, 0xFFFFFF);
            }

            /* Controls bar below player */
            int ctrl_y = pl_y + pl_h + 4;
            /* Progress bar */
            int pb_w = pl_w;
            vesa_draw_rect(pl_x, ctrl_y, pb_w, 4, 0x272727);
            int filled = (pb_w * video_play_pos) / 100;
            vesa_draw_rect(pl_x, ctrl_y, filled, 4, 0xFF0000);
            vesa_draw_rect(pl_x + filled - 5, ctrl_y - 3, 10, 10, 0xFF0000);
            ctrl_y += 10;

            /* Control buttons */
            int hov_pp = (mx >= pl_x + 6 && mx < pl_x + 50 && my >= ctrl_y && my < ctrl_y + 20);
            vesa_draw_string(audio_playing ? "[  ||  ]" : "[  |>  ]", pl_x + 6, ctrl_y + 4,
                             hov_pp ? 0xFFFFFF : 0xCCCCCC);
            /* Volume bar */
            vesa_draw_string("Vol", pl_x + 90, ctrl_y + 4, 0x888888);
            vesa_draw_rect(pl_x + 116, ctrl_y + 10, 80, 4, 0x272727);
            vesa_draw_rect(pl_x + 116, ctrl_y + 10, (80 * audio_volume) / 100, 4, 0xAAAAAA);
            /* Fullscreen */
            int hov_fs = (mx >= pl_x + pl_w - 40 && mx < pl_x + pl_w && my >= ctrl_y && my < ctrl_y + 20);
            vesa_draw_string("[  ]", pl_x + pl_w - 36, ctrl_y + 4, hov_fs ? 0xFFFFFF : 0x888888);

            ctrl_y += 26;
            /* Title */
            vesa_draw_string(vid_titles[vi], pl_x, ctrl_y, 0xFFFFFF);
            ctrl_y += 16;
            vesa_draw_string("1.2M views  3 days ago", pl_x, ctrl_y, 0xAAAAAA);
            ctrl_y += 14;
            /* Channel */
            vesa_draw_rect(pl_x, ctrl_y, 32, 32, 0x333333);
            vesa_draw_string("CH", pl_x + 8, ctrl_y + 11, 0x888888);
            vesa_draw_string("AetherOS Channel  Subscribe", pl_x + 40, ctrl_y + 10, 0xEEEEEE);
            ctrl_y += 40;
            vesa_draw_line(pl_x, ctrl_y, pl_x + pl_w, ctrl_y, 0x272727);
            ctrl_y += 8;
            /* Description */
            vesa_draw_string("Watch the latest video from AetherOS. Like, share, and subscribe!", pl_x, ctrl_y, 0xAAAAAA);

            /* Right sidebar - Up Next */
            int sb2_x = pl_x + pl_w + 10;
            int sb2_w = ww - pl_w - 30;
            vesa_draw_string("Up next", sb2_x, content_y, 0xFFFFFF);
            content_y += 16;
            const char* up_next[] = {
                "Lofi Mix 24/7",
                "OS Dev Ep.12",
                "Top Songs 2025",
                "Build a Compiler",
                "Best Games 2025"
            };
            for (int u = 0; u < 5; u++) {
                int uy = content_y + u * 54;
                int hov_u = (mx >= sb2_x && mx < sb2_x + sb2_w && my >= uy && my < uy + 48);
                vesa_draw_rect(sb2_x, uy, sb2_w, 48, hov_u ? 0x1A1A1A : 0x0F0F0F);
                vesa_draw_rect(sb2_x, uy, 80, 48, 0x1A1A1A);
                vesa_draw_rect_outline(sb2_x, uy, 80, 48, 0x272727);
                vesa_draw_string(">", sb2_x + 34, uy + 19, 0x666666);
                vesa_draw_string(up_next[u], sb2_x + 88, uy + 6, hov_u ? 0xFFFFFF : 0xCCCCCC);
                vesa_draw_string("AetherOS", sb2_x + 88, uy + 22, 0x717171);
                vesa_draw_string("3:30", sb2_x + 88, uy + 36, 0x717171);
            }

        } else {
            /* ── YOUTUBE BROWSE / SEARCH RESULTS PAGE ── */

            /* Determine what videos to show based on yt_search or last Google search */
            int show_music   = str_contains_nocase(browser_yt_search_len > 0 ? browser_yt_search : browser_last_search, "music") ||
                               str_contains_nocase(browser_yt_search_len > 0 ? browser_yt_search : browser_last_search, "lofi") ||
                               str_contains_nocase(browser_yt_search_len > 0 ? browser_yt_search : browser_last_search, "song");
            int show_gaming  = str_contains_nocase(browser_yt_search_len > 0 ? browser_yt_search : browser_last_search, "game") ||
                               str_contains_nocase(browser_yt_search_len > 0 ? browser_yt_search : browser_last_search, "minecraft") ||
                               str_contains_nocase(browser_yt_search_len > 0 ? browser_yt_search : browser_last_search, "fortnite");
            int show_coding  = str_contains_nocase(browser_yt_search_len > 0 ? browser_yt_search : browser_last_search, "code") ||
                               str_contains_nocase(browser_yt_search_len > 0 ? browser_yt_search : browser_last_search, "program") ||
                               str_contains_nocase(browser_yt_search_len > 0 ? browser_yt_search : browser_last_search, "linux");

            /* Category pills */
            const char* cats[] = {"All", "Music", "Gaming", "Coding", "Live", "News"};
            int cat_x = content_x;
            for (int c = 0; c < 6; c++) {
                int cw = 0; const char* p = cats[c]; while (*p++) cw++;
                cw = cw * 8 + 16;
                int hov_c = (mx >= cat_x && mx < cat_x + cw && my >= content_y && my < content_y + 20);
                int sel_c = (c == 0 && !show_music && !show_gaming && !show_coding) ||
                            (c == 1 && show_music) || (c == 2 && show_gaming) || (c == 3 && show_coding);
                vesa_draw_rect(cat_x, content_y, cw, 20, sel_c ? 0xFFFFFF : (hov_c ? 0x2A2A2A : 0x1A1A1A));
                vesa_draw_rect_outline(cat_x, content_y, cw, 20, sel_c ? 0xFFFFFF : 0x303030);
                vesa_draw_string(cats[c], cat_x + 8, content_y + 6, sel_c ? 0x0F0F0F : 0xCCCCCC);
                cat_x += cw + 8;
            }
            content_y += 28;

            /* Video grid: 3 columns */
            const char* vid_list[9][3] = {
                /* title,  views,  duration */
                {"Lofi Hip Hop Radio - Beats to Study", "12.4M views", "LIVE"},
                {"Top 10 Songs This Week 2025",         "8.1M views",  "12:34"},
                {"Minecraft Survival Let's Play Ep.1",  "3.2M views",  "28:14"},
                {"How to Build an OS in C - Full",      "1.8M views",  "2:04:12"},
                {"Fortnite Season 12 Epic Moments",     "5.6M views",  "18:43"},
                {"Coding a TCP/IP Stack from Scratch",  "900K views",  "1:23:05"},
                {"Best Songs of 2025 Playlist",         "22M views",   "1:02:34"},
                {"Gaming PC Build Guide 2025",          "4.4M views",  "24:22"},
                {"Python for Beginners - Full Course",  "15M views",   "6:13:28"},
            };
            /* Reorder based on category */
            int order[9] = {0,1,2,3,4,5,6,7,8};
            if (show_music)  { order[0]=0; order[1]=6; order[2]=1; order[3]=2; order[4]=3; order[5]=4; order[6]=5; order[7]=7; order[8]=8; }
            if (show_gaming) { order[0]=2; order[1]=4; order[2]=7; order[3]=0; order[4]=1; order[5]=3; order[6]=5; order[7]=6; order[8]=8; }
            if (show_coding) { order[0]=3; order[1]=5; order[2]=8; order[3]=0; order[4]=1; order[5]=2; order[6]=4; order[7]=6; order[8]=7; }

            int cols = 3;
            int card_w = (ww - 40 - (cols - 1) * 10) / cols;
            int card_h = card_w * 9 / 16 + 50;
            int thumb_h = card_w * 9 / 16;

            for (int v = 0; v < 9; v++) {
                int col = v % cols;
                int row = v / cols;
                int vx = content_x + col * (card_w + 10);
                int vy = content_y + row * (card_h + 16);

                int hov_v = (mx >= vx && mx < vx + card_w && my >= vy && my < vy + card_h);
                int idx = order[v];

                /* Thumbnail */
                uint32_t thumb_colors[] = {0x0D1117, 0x0A1628, 0x1A0D1A, 0x0D1A1A, 0x1A1A0D,
                                           0x0D0D1A, 0x1A0D0D, 0x0D1A0D, 0x1A1A1A};
                vesa_draw_rect(vx, vy, card_w, thumb_h, thumb_colors[idx % 9]);
                vesa_draw_rect_outline(vx, vy, card_w, thumb_h, hov_v ? 0x3EA6FF : 0x272727);

                /* Play button overlay */
                int pbx = vx + card_w/2 - 16;
                int pby = vy + thumb_h/2 - 12;
                vesa_draw_rect(pbx, pby, 32, 24, 0xCC0000);
                vesa_draw_string(">", pbx + 11, pby + 8, 0xFFFFFF);

                /* Duration badge */
                int dur_x = vx + card_w - 40;
                int dur_y = vy + thumb_h - 14;
                vesa_draw_rect(dur_x, dur_y, 38, 12, 0x000000);
                vesa_draw_string(vid_list[idx][2], dur_x + 2, dur_y + 2, 0xFFFFFF);

                /* Channel icon */
                int ci_y = vy + thumb_h + 8;
                vesa_draw_rect(vx, ci_y, 28, 28, 0x333333);
                vesa_draw_string("YT", vx + 6, ci_y + 10, 0xAAAAAA);

                /* Title & meta */
                int txt_x = vx + 36;
                vesa_draw_string(vid_list[idx][0], txt_x, ci_y, hov_v ? 0xFFFFFF : 0xE8EAED);
                vesa_draw_string(vid_list[idx][1], txt_x, ci_y + 14, 0x717171);
            }
        }
        return; /* don't draw google content when in youtube mode */
    }

    if (browser_active_tab == 0) {


        /* ============================================================
         * GOOGLE COLORS
         * G=blue  o=red  o=yellow  g=blue  l=green  e=red
         * ============================================================ */
        static const uint32_t GCOL_BLUE   = 0x4285F4;
        static const uint32_t GCOL_RED    = 0xDB4437;
        static const uint32_t GCOL_YELLOW = 0xF4B400;
        static const uint32_t GCOL_GREEN  = 0x0F9D58;

        /* Helper: draw one char using the 8x8 font scaled */
        /* (draw_string_scaled handles this fine) */

        if (!browser_has_searched) {
            /* ========== GOOGLE HOMEPAGE ========== */
            content_y += 30;

            /* Big colored Google logo at scale 3 (each char = 24px wide) */
            int logo_char_w = 8 * 3;
            int logo_total_w = 6 * logo_char_w;
            int logo_x = wx + (ww - logo_total_w) / 2;
            int logo_y = content_y;
            { char ch[2]; ch[1]='\0';
              uint32_t cols[] = {GCOL_BLUE, GCOL_RED, GCOL_YELLOW, GCOL_BLUE, GCOL_GREEN, GCOL_RED};
              const char* word = "Google";
              for (int li = 0; li < 6; li++) {
                  ch[0] = word[li];
                  draw_string_scaled(ch, logo_x + li * logo_char_w, logo_y, 3, cols[li]);
              }
            }
            content_y += 38;

            /* Search box */
            int sb_w = 460;
            int sb_x = wx + (ww - sb_w) / 2;
            int sb_h = 28;
            vesa_draw_rect(sb_x, content_y, sb_w, sb_h, 0x1C1C1E);
            uint32_t sb_outline = (gui_focus == FOCUS_BROWSER_SEARCH) ? GCOL_BLUE : 0x444444;
            vesa_draw_rect_outline(sb_x, content_y, sb_w, sb_h, sb_outline);
            /* magnifier icon area */
            vesa_draw_string("O/", sb_x + 8, content_y + 10, 0x888888);
            if (browser_search_len == 0) {
                vesa_draw_string("Search Google or type a URL", sb_x + 30, content_y + 10, 0x666666);
            } else {
                vesa_draw_string(browser_search_query, sb_x + 30, content_y + 10, 0xEEEEEE);
                if (gui_focus == FOCUS_BROWSER_SEARCH) {
                    vesa_draw_rect(sb_x + 30 + browser_search_len * 8, content_y + 6, 2, 14,
                                   GCOL_BLUE);
                }
            }
            /* mic icon area */
            vesa_draw_string("(M)", sb_x + sb_w - 30, content_y + 10, 0x888888);
            content_y += sb_h + 14;

            /* Buttons */
            int btn_w_s = 112;
            int btn_w_l = 130;
            int btn_x1 = wx + ww / 2 - btn_w_s - 10;
            int btn_x2 = wx + ww / 2 + 10;
            int btn_y  = content_y;
            int hov_s1 = (mx >= btn_x1 && mx < btn_x1 + btn_w_s && my >= btn_y && my < btn_y + 24);
            int hov_s2 = (mx >= btn_x2 && mx < btn_x2 + btn_w_l && my >= btn_y && my < btn_y + 24);
            vesa_draw_rect(btn_x1, btn_y, btn_w_s, 24, hov_s1 ? 0x303134 : 0x222426);
            vesa_draw_rect_outline(btn_x1, btn_y, btn_w_s, 24, hov_s1 ? 0x5F6368 : 0x3C4043);
            vesa_draw_string("Google Search", btn_x1 + 6, btn_y + 8, hov_s1 ? 0xE8EAED : 0xBDC1C6);
            vesa_draw_rect(btn_x2, btn_y, btn_w_l, 24, hov_s2 ? 0x303134 : 0x222426);
            vesa_draw_rect_outline(btn_x2, btn_y, btn_w_l, 24, hov_s2 ? 0x5F6368 : 0x3C4043);
            vesa_draw_string("I'm Feeling Lucky", btn_x2 + 6, btn_y + 8, hov_s2 ? 0xE8EAED : 0xBDC1C6);
            content_y += 40;

            /* Footer languages line */
            vesa_draw_string("Google offered in:  English  Espanol  Francais  Deutsch",
                             wx + (ww - 55 * 8) / 2, content_y, 0x3C4043);

        } else {
            /* ========== GOOGLE SEARCH RESULTS PAGE ========== */

            /* Top bar: small Google logo + search box */
            int logo_char_sm = 8 * 2;
            int logo_x_sm = content_x;
            int logo_y_sm = content_y - 2;
            { char ch[2]; ch[1]='\0';
              uint32_t cols[] = {GCOL_BLUE, GCOL_RED, GCOL_YELLOW, GCOL_BLUE, GCOL_GREEN, GCOL_RED};
              const char* word = "Google";
              for (int li = 0; li < 6; li++) {
                  ch[0] = word[li];
                  draw_string_scaled(ch, logo_x_sm + li * logo_char_sm, logo_y_sm, 2, cols[li]);
              }
            }

            int sbar_x = content_x + 6 * logo_char_sm + 14;
            int sbar_w  = ww - (sbar_x - wx) - 60;
            int sbar_h  = 22;
            vesa_draw_rect(sbar_x, content_y, sbar_w, sbar_h, 0x1C1C1E);
            vesa_draw_rect_outline(sbar_x, content_y, sbar_w, sbar_h,
                                   (gui_focus == FOCUS_BROWSER_SEARCH) ? GCOL_BLUE : 0x444444);
            vesa_draw_string(browser_search_query, sbar_x + 8, content_y + 7, 0xE8EAED);
            if (gui_focus == FOCUS_BROWSER_SEARCH) {
                vesa_draw_rect(sbar_x + 8 + browser_search_len * 8, content_y + 4, 2, 14,
                               GCOL_BLUE);
            }

            /* Back button */
            int back_x = sbar_x + sbar_w + 6;
            int hov_bk = (mx >= back_x && mx < back_x + 44 && my >= content_y && my < content_y + sbar_h);
            vesa_draw_string("[Back]", back_x, content_y + 7, hov_bk ? GCOL_RED : 0x888888);

            content_y += sbar_h + 6;
            vesa_draw_line(wx + 4, content_y, wx + ww - 4, content_y, 0x333333);
            content_y += 6;

            /* Tabs: All  Images  Videos  News  Maps */
            const char* rtabs[] = {"All", "Images", "Videos", "News", "Maps"};
            int rtab_x = content_x;
            for (int ri = 0; ri < 5; ri++) {
                int tw = 0; const char* p = rtabs[ri]; while(*p++) tw++;
                vesa_draw_string(rtabs[ri], rtab_x, content_y,
                                 ri == 0 ? GCOL_BLUE : 0x888888);
                if (ri == 0) {
                    vesa_draw_line(rtab_x, content_y + 12, rtab_x + tw * 8, content_y + 12, GCOL_BLUE);
                }
                rtab_x += tw * 8 + 20;
            }
            content_y += 20;
            vesa_draw_line(wx + 4, content_y, wx + ww - 4, content_y, 0x333333);
            content_y += 8;

            /* Result count line */
            vesa_draw_string("About 4,180,000,000 results  (0.48 seconds)", content_x, content_y, 0x9AA0A6);
            content_y += 18;

            /* ---- Helper macro: draw one result card ----
             * title_col = GCOL_BLUE-ish link
             * url_col   = GCOL_GREEN-ish
             * snip_col  = light gray 0xBDC1C6
             */
            #define DRAW_RESULT(title, url, snip1, snip2) \
                vesa_draw_string(title, content_x, content_y, 0x8AB4F8); content_y += 14; \
                vesa_draw_string(url,   content_x, content_y, 0x81C995); content_y += 12; \
                vesa_draw_string(snip1, content_x, content_y, 0xBDC1C6); content_y += 12; \
                vesa_draw_string(snip2, content_x, content_y, 0xBDC1C6); content_y += 20;

            /* ---- Keyword routing for results ---- */
            int is_yt    = str_contains_nocase(browser_last_search, "yout") ||
                           str_contains_nocase(browser_last_search, "tube");
            int is_news  = str_contains_nocase(browser_last_search, "news") ||
                           str_contains_nocase(browser_last_search, "headline");
            int is_wx    = str_contains_nocase(browser_last_search, "weather") ||
                           str_contains_nocase(browser_last_search, "forecast");
            int is_prog  = str_contains_nocase(browser_last_search, "code") ||
                           str_contains_nocase(browser_last_search, "python") ||
                           str_contains_nocase(browser_last_search, "javascript") ||
                           str_contains_nocase(browser_last_search, "linux") ||
                           str_contains_nocase(browser_last_search, "programming");
            int is_os    = str_contains_nocase(browser_last_search, "aether") ||
                           str_contains_nocase(browser_last_search, "kernel") ||
                           str_contains_nocase(browser_last_search, "reboot");
            int is_game  = str_contains_nocase(browser_last_search, "game") ||
                           str_contains_nocase(browser_last_search, "minecraft") ||
                           str_contains_nocase(browser_last_search, "fortnite");
            int is_social= str_contains_nocase(browser_last_search, "facebook") ||
                           str_contains_nocase(browser_last_search, "instagram") ||
                           str_contains_nocase(browser_last_search, "twitter") ||
                           str_contains_nocase(browser_last_search, "reddit") ||
                           str_contains_nocase(browser_last_search, "tiktok");
            int is_music = str_contains_nocase(browser_last_search, "music") ||
                           str_contains_nocase(browser_last_search, "spotify") ||
                           str_contains_nocase(browser_last_search, "song");
            int is_ai    = str_contains_nocase(browser_last_search, "chatgpt") ||
                           str_contains_nocase(browser_last_search, "gemini") ||
                           str_contains_nocase(browser_last_search, "artificial intelligence");
            int is_goog  = str_contains_nocase(browser_last_search, "google");
            int is_maps  = str_contains_nocase(browser_last_search, "maps") ||
                           str_contains_nocase(browser_last_search, "location") ||
                           str_contains_nocase(browser_last_search, "directions");

            if (is_yt) {
                DRAW_RESULT(
                    "YouTube - Broadcast Yourself",
                    "https://www.youtube.com",
                    "Enjoy the videos and music you love, upload original content",
                    "and share it all with friends, family and the world on YouTube."
                )
                DRAW_RESULT(
                    "YouTube Music - Free Listening",
                    "https://music.youtube.com",
                    "A new music service with official albums, singles, videos,",
                    "remixes, live performances and more for Android, iOS and desktop."
                )
                DRAW_RESULT(
                    "YouTube Studio - Creator Tools",
                    "https://studio.youtube.com",
                    "Manage your channel, check analytics, upload videos,",
                    "and respond to comments from your fans."
                )
                DRAW_RESULT(
                    "YouTube Premium - Ad-free streaming",
                    "https://www.youtube.com/premium",
                    "Enjoy YouTube without ads, play videos in the background,",
                    "and download videos for offline viewing."
                )

                /* AetherTube video list is shown here */
                vesa_draw_line(content_x, content_y, wx + ww - 10, content_y, 0x333333);
                content_y += 8;
                vesa_draw_string("AetherTube (Built-in) - Video Results:", content_x, content_y, 0x9AA0A6);
                content_y += 18;
                const char* vid_titles[] = {"Lofi Beats to Code / Study to", "AetherOS-64 Cinematic Trailer", "How to Build a 64-bit OS from Scratch"};
                const char* vid_dur[]    = {"03:40", "01:15", "02:30"};
                const char* vid_views[]  = {"12K views", "9.2K views", "15.4K views"};
                for (int i = 0; i < 3; i++) {
                    int iy = content_y + i * 62;
                    int sel = (browser_video_selected_idx == i);
                    int hov_v = (mx >= content_x && mx < wx + ww - 10 && my >= iy && my < iy + 56);
                    vesa_draw_rect(content_x, iy, ww - 30, 56, sel ? 0x1C2B3A : (hov_v ? 0x1A1A1A : 0x111111));
                    vesa_draw_rect_outline(content_x, iy, ww - 30, 56, sel ? GCOL_BLUE : (hov_v ? 0x444444 : 0x2A2A2A));
                    int th_x = content_x + 6;
                    int th_y = iy + 6;
                    vesa_draw_rect(th_x, th_y, 70, 44, 0x0A0A0A);
                    vesa_draw_rect_outline(th_x, th_y, 70, 44, 0x333333);
                    vesa_draw_string("  >  ", th_x + 24, th_y + 18, sel ? GCOL_BLUE : 0x666666);
                    vesa_draw_rect(th_x + 32, th_y + 32, 36, 10, 0x000000);
                    vesa_draw_string(vid_dur[i], th_x + 34, th_y + 34, 0xEEEEEE);
                    int tx2 = th_x + 80;
                    uint32_t ttl_col = sel ? 0x8AB4F8 : (hov_v ? 0xCCCCCC : 0xE8EAED);
                    vesa_draw_string(vid_titles[i], tx2, iy + 8, ttl_col);
                    vesa_draw_string(vid_views[i], tx2, iy + 24, 0x9AA0A6);
                    vesa_draw_string("Double-click to play", tx2, iy + 38, 0x5F6368);
                }
            } else if (is_goog) {
                DRAW_RESULT(
                    "Google - About Google",
                    "https://about.google",
                    "Google's mission is to organize the world's information and make it",
                    "universally accessible and useful."
                )
                DRAW_RESULT(
                    "Google Search - How Search Works",
                    "https://www.google.com/search/howsearchworks",
                    "Learn how Google Search works and how we organize the world's",
                    "information to provide the most relevant results."
                )
                DRAW_RESULT(
                    "Google Products & Services",
                    "https://www.google.com/about/products",
                    "Gmail, Maps, YouTube, Drive, Chrome, Translate, Meet, Photos,",
                    "and hundreds more products designed to help you in your daily life."
                )
            } else if (is_news) {
                DRAW_RESULT(
                    "Google News - Top Stories",
                    "https://news.google.com",
                    "Comprehensive, up-to-date news coverage, aggregated from sources",
                    "all over the world by Google News."
                )
                DRAW_RESULT(
                    "BBC News - Breaking News",
                    "https://www.bbc.com/news",
                    "Visit BBC News for up-to-the-minute news, breaking news, video,",
                    "audio and feature stories. BBC News provides trusted world and UK news."
                )
                DRAW_RESULT(
                    "Reuters - News Agency",
                    "https://www.reuters.com",
                    "Find latest news from every corner of the globe at Reuters.com,",
                    "your online source for breaking international news coverage."
                )
                DRAW_RESULT(
                    "CNN - Breaking News",
                    "https://www.cnn.com",
                    "View the latest news and breaking news today for U.S., world,",
                    "weather, entertainment, politics and health at CNN."
                )
            } else if (is_wx) {
                DRAW_RESULT(
                    "Weather.com - Weather Forecast",
                    "https://weather.com",
                    "Today: Partly cloudy, 24 C. Wind 12 km/h NW. Humidity 68%.",
                    "Tomorrow: 27 C, mostly sunny. UV index: 6 (High)."
                )
                DRAW_RESULT(
                    "AccuWeather - 10-Day Forecast",
                    "https://www.accuweather.com",
                    "Get the latest weather forecasts: hourly, daily, and extended.",
                    "Mon 24C  Tue 26C  Wed 22C  Thu 19C  Fri 21C  Sat 25C"
                )
                DRAW_RESULT(
                    "National Weather Service",
                    "https://www.weather.gov",
                    "Official forecasts, warnings, meteorological products for",
                    "forecasting the weather throughout the United States and its territories."
                )
            } else if (is_prog) {
                DRAW_RESULT(
                    "Stack Overflow - Developer Community",
                    "https://stackoverflow.com",
                    "Stack Overflow is the largest, most trusted online community for",
                    "developers to learn and share their programming knowledge."
                )
                DRAW_RESULT(
                    "GitHub - Where the world builds software",
                    "https://github.com",
                    "GitHub is where over 100 million developers shape the future of",
                    "software, together. Contribute to the open source community."
                )
                DRAW_RESULT(
                    "MDN Web Docs - Mozilla",
                    "https://developer.mozilla.org",
                    "The MDN Web Docs site provides information about Open Web technologies",
                    "including HTML, CSS, and APIs for both Web sites and progressive web apps."
                )
                DRAW_RESULT(
                    "W3Schools - Web Development Tutorials",
                    "https://www.w3schools.com",
                    "W3Schools is optimized for learning and training. Examples might be",
                    "simplified to improve reading and learning."
                )
            } else if (is_os) {
                DRAW_RESULT(
                    "AetherOS-64 - Open Source 64-bit OS",
                    "https://aetheros.dev",
                    "AetherOS is a custom 64-bit x86 OS written in C. Features include",
                    "VESA GUI, virtual filesystem, snake game, browser, and more."
                )
                DRAW_RESULT(
                    "OSDev Wiki - OS Development",
                    "https://wiki.osdev.org",
                    "OSDev.org community provides tutorials on OS development,",
                    "including GRUB bootloading, protected mode, GDT, IDT and VESA."
                )
                DRAW_RESULT(
                    "Linux Kernel - kernel.org",
                    "https://www.kernel.org",
                    "The Linux Kernel Organization is a California Public Benefit Corporation",
                    "dedicated to the maintenance of the Linux kernel."
                )
            } else if (is_game) {
                DRAW_RESULT(
                    "Minecraft Official Site",
                    "https://www.minecraft.net",
                    "Explore infinite worlds and build everything from the simplest of",
                    "homes to the grandest of castles. Java & Bedrock editions available."
                )
                DRAW_RESULT(
                    "Fortnite - Epic Games",
                    "https://www.fortnite.com",
                    "Fortnite is the free-to-play Battle Royale game with Lego Fortnite,",
                    "Rocket Racing and Fortnite Festival. Available on all platforms."
                )
                DRAW_RESULT(
                    "IGN - Game Reviews and News",
                    "https://www.ign.com",
                    "IGN is the leading internet media company for video game, film,",
                    "TV and comic enthusiasts. Reviews, trailers, upcoming releases."
                )
                DRAW_RESULT(
                    "Steam - The Game Library",
                    "https://store.steampowered.com",
                    "Steam is the ultimate destination for playing, discussing, and",
                    "creating games. 50,000+ games available."
                )
            } else if (is_social) {
                DRAW_RESULT(
                    "Facebook - Connect with Friends",
                    "https://www.facebook.com",
                    "Create an account or log into Facebook. Connect with friends,",
                    "family and other people you know. Share photos and videos."
                )
                DRAW_RESULT(
                    "Instagram - Photos and Videos",
                    "https://www.instagram.com",
                    "Create an account or log in to Instagram - A simple, fun & creative",
                    "way to capture, edit & share photos, videos & messages with friends."
                )
                DRAW_RESULT(
                    "Reddit - The Front Page of the Internet",
                    "https://www.reddit.com",
                    "Reddit is home to thousands of communities, endless conversation,",
                    "and authentic human connection. Whether you're into gaming or news..."
                )
                DRAW_RESULT(
                    "Twitter/X - Social Network",
                    "https://x.com",
                    "From breaking news and entertainment to sports and politics, get the",
                    "full story with all the live commentary from the real X community."
                )
            } else if (is_music) {
                DRAW_RESULT(
                    "Spotify - Music for Everyone",
                    "https://www.spotify.com",
                    "Spotify is a digital music, podcast, and video service that gives",
                    "you access to millions of songs and other content from creators worldwide."
                )
                DRAW_RESULT(
                    "YouTube Music - Free Streaming",
                    "https://music.youtube.com",
                    "A new music streaming and video service. Official albums, singles,",
                    "remixes, live performances and more. Free with ads."
                )
                DRAW_RESULT(
                    "Apple Music - Subscribe Now",
                    "https://music.apple.com",
                    "Listen to 100 million songs, ad-free on Apple Music. Listen online",
                    "or off. Start your 3-month free trial today."
                )
            } else if (is_ai) {
                DRAW_RESULT(
                    "Google Gemini - AI Assistant",
                    "https://gemini.google.com",
                    "Gemini is Google's best AI model yet, built to be multimodal from",
                    "the ground up. Reason across text, images, code, and more."
                )
                DRAW_RESULT(
                    "ChatGPT - OpenAI",
                    "https://chat.openai.com",
                    "ChatGPT is an AI system that can have a natural conversation with",
                    "you, write code, essays, answer questions, and much more."
                )
                DRAW_RESULT(
                    "Claude - Anthropic AI",
                    "https://claude.ai",
                    "Claude is a next-generation AI assistant built by Anthropic.",
                    "Thoughtful, reliable, and nuanced - trained to be helpful and harmless."
                )
            } else if (is_maps) {
                DRAW_RESULT(
                    "Google Maps - Navigate & Explore",
                    "https://maps.google.com",
                    "Find local businesses, view maps and get driving directions in Google Maps.",
                    "Real-time GPS navigation, traffic, transit, and details about millions of places."
                )
                DRAW_RESULT(
                    "Google Maps - Satellite View",
                    "https://maps.google.com/maps?t=k",
                    "View detailed satellite and aerial imagery of any location on Earth.",
                    "Zoom in to see buildings, streets, landmarks and terrain."
                )
                DRAW_RESULT(
                    "OpenStreetMap - Free World Map",
                    "https://www.openstreetmap.org",
                    "OpenStreetMap is a free, editable map of the whole world built",
                    "by a community of mappers. Free to use under an open license."
                )
            } else {
                /* ---- Generic Google-style results for ANY query ---- */
                /* Build dynamic result strings using the search query */
                char t1[90]; char t2[90]; char t3[90]; char t4[90];
                char u1[90]; char u2[90]; char u3[90]; char u4[90];
                int tl; const char *p, *q2;

                /* Result 1: Wikipedia */
                tl=0; p="Wikipedia - "; while(*p && tl<88) t1[tl++]=*p++;
                q2=browser_last_search; while(*q2 && tl<78) t1[tl++]=*q2++;
                t1[tl]='\0';
                tl=0; p="https://en.wikipedia.org/wiki/"; while(*p && tl<88) u1[tl++]=*p++;
                q2=browser_last_search; while(*q2 && tl<78) u1[tl++]=*q2++;
                u1[tl]='\0';

                /* Result 2: Reddit */
                tl=0; p="Reddit - "; while(*p && tl<88) t2[tl++]=*p++;
                q2=browser_last_search; while(*q2 && tl<72) t2[tl++]=*q2++;
                p=" Discussion"; while(*p && tl<88) t2[tl++]=*p++;
                t2[tl]='\0';
                tl=0; p="https://www.reddit.com/search?q="; while(*p && tl<88) u2[tl++]=*p++;
                q2=browser_last_search; while(*q2 && tl<78) u2[tl++]=*q2++;
                u2[tl]='\0';

                /* Result 3: Official site */
                tl=0; q2=browser_last_search; while(*q2 && tl<72) t3[tl++]=*q2++;
                p=" - Official Website"; while(*p && tl<88) t3[tl++]=*p++;
                t3[tl]='\0';
                tl=0; p="https://www."; while(*p && tl<88) u3[tl++]=*p++;
                q2=browser_last_search; while(*q2 && tl<70) u3[tl++]=*q2++;
                p=".com"; while(*p && tl<88) u3[tl++]=*p++;
                u3[tl]='\0';

                /* Result 4: Google Images */
                tl=0; p="Google Images - "; while(*p && tl<88) t4[tl++]=*p++;
                q2=browser_last_search; while(*q2 && tl<74) t4[tl++]=*q2++;
                t4[tl]='\0';
                tl=0; p="https://images.google.com/search?q="; while(*p && tl<88) u4[tl++]=*p++;
                q2=browser_last_search; while(*q2 && tl<78) u4[tl++]=*q2++;
                u4[tl]='\0';

                DRAW_RESULT(t1, u1,
                    "Wikipedia is a free online encyclopedia that anyone can edit.",
                    "Over 60 million articles in more than 300 languages worldwide."
                )
                DRAW_RESULT(t2, u2,
                    "Top discussions, community answers and expert opinions on Reddit.",
                    "Upvoted by thousands of users. Join the conversation now."
                )
                DRAW_RESULT(t3, u3,
                    "Official website with products, services, news and contact information.",
                    "Trusted source - last indexed by Google 2 hours ago."
                )
                DRAW_RESULT(t4, u4,
                    "Search Google Images for visual results related to your query.",
                    "Browse photos, illustrations, vectors and more."
                )
            }
            #undef DRAW_RESULT
        }
    }
    else if (browser_active_tab == 1) {
        if (browser_page == 4) {
            vesa_draw_string("AETHER DOWNLOAD CENTER & STORE", content_x, content_y, gui_accent_col); content_y += 20;
            vesa_draw_string("==================================", content_x, content_y, 0x1A4A2A); content_y += 16;
            vesa_draw_string("Select apps & tools to download to /downloads: ", content_x, content_y, 0x88DDFF); content_y += 24;
            
            // Return Home button in upper right
            int ret_x = ww - 140;
            int ret_y = content_y - 40;
            int hov_ret = (mx >= wx + ret_x && mx < wx + ret_x + 110 && my >= wy + ret_y && my < wy + ret_y + 20);
            vesa_draw_rect(wx + ret_x, wy + ret_y, 110, 20, hov_ret ? 0x0A2010 : 0x020A05);
            vesa_draw_rect_outline(wx + ret_x, wy + ret_y, 110, 20, hov_ret ? gui_accent_col : 0x00AA55);
            vesa_draw_string("Return Home", wx + ret_x + 12, wy + ret_y + 6, hov_ret ? gui_accent_col : 0x00AA55);
            
            const char* titles[] = {"Retro Snake Game (snake.exe)", "Matrix Screensaver (matrix.exe)", "Source Reference (sources.txt)"};
            const char* descs[]  = {"Play retro arcade snake inside a popup overlay.", "Instantly activates falling green matrix streams.", "Freestanding kernel module architecture listing."};
            const char* sizes[]  = {"18 KB", "12 KB", "8 KB"};
            
            for (int i = 0; i < 3; i++) {
                int ry = content_y + i * 56;
                vesa_draw_rect(content_x, ry, ww - 30, 48, 0x050D08);
                vesa_draw_rect_outline(content_x, ry, ww - 30, 48, 0x004422);
                
                vesa_draw_string(titles[i], content_x + 8, ry + 6, gui_accent_col);
                vesa_draw_string(descs[i], content_x + 8, ry + 22, 0x00AA55);
                vesa_draw_string(sizes[i], content_x + ww - 160, ry + 6, 0x88DDFF);
                
                int btn_x = wx + ww - 110;
                int btn_y = wy + ry + 12;
                int hov_dl = (mx >= btn_x && mx < btn_x + 80 && my >= btn_y && my < btn_y + 24);
                vesa_draw_rect(btn_x, btn_y, 80, 24, hov_dl ? 0x0C2512 : 0x021A0A);
                vesa_draw_rect_outline(btn_x, btn_y, 80, 24, hov_dl ? gui_accent_col : 0x00AA55);
                vesa_draw_string("Download", btn_x + 8, btn_y + 8, hov_dl ? gui_accent_col : 0x00AA55);
            }
        } else {
            vesa_draw_string("AETHERNET HOME PAGE", content_x, content_y, gui_accent_col); content_y += 20;
            vesa_draw_string("==========================", content_x, content_y, 0x1A4A2A); content_y += 16;
            vesa_draw_string("Welcome to AetherOS Portal! Quick access bookmarks: ", content_x, content_y, 0x88DDFF); content_y += 24;
            
            const char* b_titles[] = {" System Telemetry ", " File Manager ", " Shell Console ", " App Store "};
            for (int i = 0; i < 4; i++) {
                int bx = content_x + i * 144;
                int hov_b = (mx >= bx && mx < bx + 130 && my >= content_y && my < content_y + 40);
                
                vesa_draw_rect(bx, content_y, 130, 40, hov_b ? 0x0A2010 : 0x020A05);
                vesa_draw_rect_outline(bx, content_y, 130, 40, hov_b ? gui_accent_col : 0x00AA55);
                vesa_draw_string(b_titles[i], bx + 8, content_y + 16, hov_b ? gui_accent_col : 0x00AA55);
            }
            content_y += 56;
            vesa_draw_string("System Status: Online | Gateway: 192.168.1.1", content_x, content_y, 0x007744);
        }
    }
    else if (browser_active_tab == 2) {
        vesa_draw_string("AETHEROS SYSTEM WIKI", content_x, content_y, gui_accent_col); content_y += 20;
        vesa_draw_string("==========================", content_x, content_y, 0x1A4A2A); content_y += 16;
        
        vesa_draw_string("OS Architecture Details: ", content_x, content_y, 0x88DDFF); content_y += 16;
        vesa_draw_string("* Bootloader: Multiboot2 (grub2 compatible)", content_x, content_y, 0x00AA55); content_y += 14;
        vesa_draw_string("* Processor Mode: 64-bit Long Mode (paging active)", content_x, content_y, 0x00AA55); content_y += 14;
        vesa_draw_string("* Graphics: VESA Linear Frame Buffer, 1024x768x32bpp", content_x, content_y, 0x00AA55); content_y += 14;
        vesa_draw_string("* PMM: Identity Paged Physical Memory Manager (4GB cap)", content_x, content_y, 0x00AA55); content_y += 14;
        vesa_draw_string("* Mouse Driver: Interrupt handling via IRQ12 on PS/2", content_x, content_y, 0x00AA55); content_y += 14;
        vesa_draw_string("* Shell: Monospace CLI executing custom executable paths", content_x, content_y, 0x00AA55);
    }
}

static void gui_draw_start_menu(int mx, int my) {
    int sw = 380, sh = 380;
    int sx = (1024 - sw) / 2;
    int sy = (taskbar_position == 0) ? (48 + 8) : (768 - 48 - sh - 8);  /* 8px gap relative to taskbar */

    /* Panel background (Dark Techy Green-Black) */
    gui_fill_rounded(sx, sy, sw, sh, 0x050E07);
    /* Accent border */
    vesa_draw_rect_outline(sx, sy, sw, sh, gui_accent_col);

    /* Draw 4 corner HUD brackets on Start Menu */
    int br_sz = 12;
    vesa_draw_line(sx, sy, sx + br_sz, sy, gui_accent_col);
    vesa_draw_line(sx, sy, sx, sy + br_sz, gui_accent_col);
    vesa_draw_line(sx + sw - 1, sy, sx + sw - 1 - br_sz, sy, gui_accent_col);
    vesa_draw_line(sx + sw - 1, sy, sx + sw - 1, sy + br_sz, gui_accent_col);
    vesa_draw_line(sx, sy + sh - 1, sx + br_sz, sy + sh - 1, gui_accent_col);
    vesa_draw_line(sx, sy + sh - 1, sx, sy + sh - 1 - br_sz, gui_accent_col);
    vesa_draw_line(sx + sw - 1, sy + sh - 1, sx + sw - 1 - br_sz, sy + sh - 1, gui_accent_col);
    vesa_draw_line(sx + sw - 1, sy + sh - 1, sx + sw - 1, sy + sh - 1 - br_sz, gui_accent_col);

    /* Search bar */
    vesa_draw_rect(sx + 20, sy + 18, sw - 40, 28, 0x020A05);
    vesa_draw_rect_outline(sx + 20, sy + 18, sw - 40, 28, (gui_focus == FOCUS_START_SEARCH) ? gui_accent_col : 0x00AA55);
    vesa_draw_char('/', sx + 28, sy + 24, 0x007744);
    if (start_search_len == 0) {
        vesa_draw_string("Search apps", sx + 40, sy + 26, 0x007744);
    } else {
        vesa_draw_string(start_search_query, sx + 40, sy + 26, gui_accent_col);
        uint64_t ticks = pit_get_ticks();
        if ((ticks / 30) % 2 == 0 && gui_focus == FOCUS_START_SEARCH) {
            vesa_draw_rect(sx + 40 + start_search_len * 8, sy + 22, 2, 16, gui_accent_col);
        }
    }
    vesa_draw_line(sx + 20, sy + 46, sx + sw - 20, sy + 46, 0x0A2010);

    /* Vertical divider between left (Apps) and right (Folders) */
    vesa_draw_line(sx + 250, sy + 48, sx + 250, sy + sh - 36, 0x0A2010);

    /* LEFT SECTION: Pinned Apps */
    vesa_draw_string("Pinned Apps", sx + 20, sy + 58, gui_accent_col);
    
    int gcols = 3;
    int cell_w = 72;
    int rendered_count = 0;
    for (int i = 0; i < 12; i++) {
        if (i == 7 && !chrome_installed) continue;
        if (i == 10 && !wps_installed) continue;
        if (i == 11 && !openoffice_installed) continue;
        
        if (start_search_len > 0) {
            if (!str_contains_nocase(start_apps[i].n, start_search_query)) continue;
        }
        
        int col = rendered_count % gcols, row = rendered_count / gcols;
        int ax = sx + 20 + col * cell_w;
        int ay = sy + 76  + row * 66;
        if (ay + 60 >= sy + sh - 36) break; // stay within bounds
        
        int hov = (mx >= ax && mx < ax + cell_w && my >= ay && my < ay + 60);
        if (hov) vesa_draw_rect(ax, ay, cell_w, 60, 0x0A2010);
        
        gui_fill_rounded(ax + (cell_w - 28) / 2, ay + 4, 28, 28, hov ? 0x0C2512 : 0x020A05);
        int ix = ax + (cell_w - 28) / 2, iy = ay + 4;
        uint32_t icol = hov ? gui_accent_col : start_apps[i].col;
        
        if (start_apps[i].sh == 0) {
            vesa_draw_rect_outline(ix+3, iy+4, 22, 14, icol);
            vesa_draw_line(ix+14, iy+18, ix+14, iy+22, icol);
            vesa_draw_line(ix+8, iy+22, ix+20, iy+22, icol);
        } else if (start_apps[i].sh == 1) {
            vesa_draw_char('>', ix+4, iy+10, icol);
            vesa_draw_char('_', ix+12, iy+10, icol);
        } else if (start_apps[i].sh == 2) {
            vesa_draw_rect(ix+4,  iy+16, 4, 8, icol);
            vesa_draw_rect(ix+12, iy+10, 4, 14, icol);
            vesa_draw_rect(ix+20, iy+4,  4, 20, icol);
        } else if (start_apps[i].sh == 3) {
            vesa_draw_rect_outline(ix+6, iy+6, 16, 16, icol);
            vesa_draw_line(ix+3, iy+6, ix+25, iy+6, icol);
            vesa_draw_line(ix+10, iy+10, ix+10, iy+18, icol);
            vesa_draw_line(ix+18, iy+10, ix+18, iy+18, icol);
        } else if (start_apps[i].sh == 4) {
            vesa_draw_rect_outline(ix+2, iy+6, 24, 16, icol);
            vesa_draw_rect(ix+2, iy+2, 8, 4, icol);
        } else if (start_apps[i].sh == 5) {
            vesa_draw_rect_outline(ix+4, iy+2, 20, 24, icol);
            vesa_draw_line(ix+8, iy+8, ix+20, iy+8, icol);
            vesa_draw_line(ix+8, iy+14, ix+20, iy+14, icol);
            vesa_draw_line(ix+8, iy+20, ix+16, iy+20, icol);
        } else if (start_apps[i].sh == 6) {
            vesa_draw_rect_outline(ix+4, iy+2, 20, 24, icol);
            vesa_draw_line(ix+14, iy+6, ix+14, iy+22, icol);
            vesa_draw_line(ix+6, iy+14, ix+22, iy+14, icol);
        } else if (start_apps[i].sh == 7) {
            vesa_draw_rect_outline(ix+6, iy+6, 16, 16, icol);
            vesa_draw_rect(ix+11, iy+11, 6, 6, icol);
            vesa_put_pixel(ix+14, iy+3, icol);
            vesa_put_pixel(ix+14, iy+24, icol);
            vesa_put_pixel(ix+3, iy+14, icol);
            vesa_put_pixel(ix+24, iy+14, icol);
        } else if (start_apps[i].sh == 8) {
            vesa_draw_rect_outline(ix+4, iy+4, 20, 20, icol);
            vesa_draw_line(ix+14, iy+4,  ix+14, iy+24, icol);
            vesa_draw_line(ix+4,  iy+14, ix+24, iy+14, icol);
        }
        
        int nl = 0; while (start_apps[i].n[nl]) nl++;
        vesa_draw_string(start_apps[i].n, ax + (cell_w - nl * 8) / 2, ay + 36, hov ? gui_accent_col : 0x00AA55);
        rendered_count++;
    }

    /* RIGHT SECTION: Windows Folders */
    vesa_draw_string("Folders", sx + 258, sy + 58, gui_accent_col);
    
    const char* folders[] = {"Documents", "Pictures", "Music", "Downloads", "Videos", "Control Pnl", "This PC"};
    for (int i = 0; i < 7; i++) {
        int iy = sy + 76 + i * 32;
        int hov = (mx >= sx + 254 && mx < sx + sw - 6 && my >= iy && my < iy + 26);
        if (hov) {
            vesa_draw_rect(sx + 254, iy, sw - 254 - 6, 26, 0x0A2010);
            vesa_draw_rect_outline(sx + 254, iy, sw - 254 - 6, 26, gui_accent_col);
        }
        vesa_draw_string(folders[i], sx + 262, iy + 8, hov ? gui_accent_col : 0x00AA55);
    }

    /* Bottom user bar */
    int userY = sy + sh - 36;
    vesa_draw_rect(sx + 1, userY, sw - 2, 35, 0x020A05);
    
    /* circular avatar */
    int av_hov = (mx >= sx + 16 && mx < sx + 38 && my >= userY + 7 && my < userY + 29);
    for (int ay2 = 0; ay2 < 22; ay2++) {
        for (int ax2 = 0; ax2 < 22; ax2++) {
            int dd = (ax2-11)*(ax2-11)+(ay2-11)*(ay2-11);
            if (dd <= 121)
                vesa_put_pixel(sx + 16 + ax2, userY + 7 + ay2, av_hov ? gui_accent_col : 0x00AA55);
        }
    }
    vesa_draw_string("Aether User", sx + 44, userY + 10, gui_accent_col);

    /* Power Buttons: Shutdown (S) / Restart (R) */
    int r_hov = (mx >= sx + sw - 56 && mx < sx + sw - 36 && my >= userY + 6 && my < userY + 26);
    if (r_hov) {
        vesa_draw_rect(sx + sw - 56, userY + 6, 20, 20, 0x221100);
        vesa_draw_rect_outline(sx + sw - 56, userY + 6, 20, 20, 0xFFAA00);
    }
    vesa_draw_char('R', sx + sw - 50, userY + 12, r_hov ? 0xFFAA00 : 0xBB8800);

    int s_hov = (mx >= sx + sw - 28 && mx < sx + sw - 8 && my >= userY + 6 && my < userY + 26);
    if (s_hov) {
        vesa_draw_rect(sx + sw - 28, userY + 6, 20, 20, 0x220505);
        vesa_draw_rect_outline(sx + sw - 28, userY + 6, 20, 20, 0xFF3333);
    }
    vesa_draw_char('S', sx + sw - 22, userY + 12, s_hov ? 0xFF3333 : 0xAA2222);
}

/* Renders time strings in clock format */
static void get_clock_string(char* buf, uint64_t ticks) {
    uint32_t total_sec = (uint32_t)(ticks / 100);
    uint32_t sec = total_sec % 60;
    uint32_t min = (total_sec / 60) % 60;
    uint32_t hour = (total_sec / 3600) % 24;
    format_time(buf, hour, min, sec);
}

static void gui_draw_quick_settings(int mx, int my) {
    int w = vesa_get_width();
    int h = vesa_get_height();
    int qsw = 240, qsh = 210;
    int qsx = w - qsw - 10;
    int qsy = (taskbar_position == 0) ? (48 + 8) : (h - 48 - qsh - 8);

    // Background
    gui_fill_rounded(qsx, qsy, qsw, qsh, 0x050E07);
    vesa_draw_rect_outline(qsx, qsy, qsw, qsh, gui_accent_col);

    // Title
    vesa_draw_string("Quick Settings", qsx + 12, qsy + 10, gui_accent_col);
    vesa_draw_line(qsx + 10, qsy + 24, qsx + qsw - 10, qsy + 24, 0x1A4A2A);

    // 1. Wi-Fi Button
    int hov_wifi = (mx >= qsx + 15 && mx < qsx + 115 && my >= qsy + 32 && my < qsy + 54);
    uint32_t wifi_bg = settings_wifi_enabled ? 0x002211 : 0x110000;
    uint32_t wifi_fg = settings_wifi_enabled ? gui_accent_col : 0xFF3333;
    vesa_draw_rect(qsx + 15, qsy + 32, 100, 22, hov_wifi ? 0x0A2010 : wifi_bg);
    vesa_draw_rect_outline(qsx + 15, qsy + 32, 100, 22, wifi_fg);
    vesa_draw_string(settings_wifi_enabled ? "Wi-Fi: ON" : "Wi-Fi: OFF", qsx + 23, qsy + 39, wifi_fg);

    // 2. Bluetooth Button
    int hov_bt = (mx >= qsx + 125 && mx < qsx + 225 && my >= qsy + 32 && my < qsy + 54);
    uint32_t bt_bg = bluetooth_enabled ? 0x002211 : 0x110000;
    uint32_t bt_fg = bluetooth_enabled ? gui_accent_col : 0xFF3333;
    vesa_draw_rect(qsx + 125, qsy + 32, 100, 22, hov_bt ? 0x0A2010 : bt_bg);
    vesa_draw_rect_outline(qsx + 125, qsy + 32, 100, 22, bt_fg);
    vesa_draw_string(bluetooth_enabled ? "BT: ON" : "BT: OFF", qsx + 145, qsy + 39, bt_fg);

    // 3. Night Light Button
    int hov_nl = (mx >= qsx + 15 && mx < qsx + 115 && my >= qsy + 62 && my < qsy + 84);
    uint32_t nl_bg = night_light_active ? 0x002211 : 0x000000;
    uint32_t nl_fg = night_light_active ? gui_accent_col : 0x00AA55;
    vesa_draw_rect(qsx + 15, qsy + 62, 100, 22, hov_nl ? 0x0A2010 : nl_bg);
    vesa_draw_rect_outline(qsx + 15, qsy + 62, 100, 22, nl_fg);
    vesa_draw_string(night_light_active ? "N-Light: ON" : "N-Light: OFF", qsx + 20, qsy + 69, nl_fg);

    // 4. Performance Mode
    int hov_perf = (mx >= qsx + 125 && mx < qsx + 225 && my >= qsy + 62 && my < qsy + 84);
    const char* perf_modes[] = {"Mode: ECO", "Mode: BAL", "Mode: TUR"};
    vesa_draw_rect(qsx + 125, qsy + 62, 100, 22, hov_perf ? 0x0A2010 : 0x020A05);
    vesa_draw_rect_outline(qsx + 125, qsy + 62, 100, 22, gui_accent_col);
    vesa_draw_string(perf_modes[performance_mode], qsx + 135, qsy + 69, gui_accent_col);

    // 5. Volume Slider
    vesa_draw_string("Volume:", qsx + 15, qsy + 96, 0x00AA55);
    int slider_x = qsx + 15, slider_y = qsy + 108, slider_w = 210, slider_h = 10;
    vesa_draw_rect(slider_x, slider_y, slider_w, slider_h, 0x020A05);
    vesa_draw_rect_outline(slider_x, slider_y, slider_w, slider_h, 0x1A4A2A);
    int fill_w = (system_volume * slider_w) / 100;
    if (fill_w > 0) {
        vesa_draw_rect(slider_x + 1, slider_y + 1, fill_w - 2, slider_h - 2, gui_accent_col);
    }
    // Handle volume percentage string
    char vol_str[16];
    format_stat(vol_str, "", system_volume, "%");
    vesa_draw_string(vol_str, qsx + 180, qsy + 96, gui_accent_col);

    // 6. Battery Status
    vesa_draw_string("Battery: 98% (Discharging)", qsx + 15, qsy + 136, 0x00AA55);
    // Draw micro battery outline
    vesa_draw_rect_outline(qsx + 15, qsy + 150, 40, 14, 0x1A4A2A);
    vesa_draw_rect(qsx + 16, qsy + 151, 36, 12, 0x00FF88); // 98% full
    vesa_draw_rect(qsx + 55, qsy + 154, 2, 6, 0x1A4A2A);  // battery tip
    
    // Status text at bottom
    vesa_draw_string("AetherOS Quick Panel v1.0", qsx + 15, qsy + 185, 0x005522);
}

static void gui_adjust_window_bounds(void) {
    int w = vesa_get_width();
    int h = vesa_get_height();
    for (int i = 0; i < MAX_WINS; i++) {
        gui_window_t* win = all_windows[i];
        if (win->x + win->w > w) {
            win->x = w - win->w;
            if (win->x < 56) win->x = 56;
        }
        if (taskbar_position == 0) {
            if (win->y < 48) win->y = 48;
            if (win->y + win->h > h) {
                win->y = h - win->h;
                if (win->y < 48) win->y = 48;
            }
        } else {
            if (win->y < 0) win->y = 0;
            if (win->y + win->h > h - 48) {
                win->y = h - 48 - win->h;
                if (win->y < 0) win->y = 0;
            }
        }
    }
}

static int str_equal(const char* s1, const char* s2) {
    int i = 0;
    while (s1[i] != '\0' || s2[i] != '\0') {
        if (s1[i] != s2[i]) return 0;
        i++;
    }
    return 1;
}

static int file_exists_in_dir(const char* name, int dir_id) {
    for (int i = 0; i < file_system_count; i++) {
        file_entry_t* f = &file_system[i];
        if (!f->deleted && f->dir_id == dir_id) {
            if (str_equal(f->name, name)) return 1;
        }
    }
    return 0;
}

static void create_file_extension(const char* ext) {
    int target_dir = (ctx_menu.type == 2) ? files_current_dir : 28; // 28 = Desktop folder
    char new_name[64];
    int count = 1;
    
    if (ext[0] == '\0') { // Folder
        do {
            if (count == 1) {
                int c = 0; const char* src = "New Folder";
                while (src[c]) { new_name[c] = src[c]; c++; } new_name[c] = '\0';
            } else {
                int c = 0; const char* src = "New Folder (";
                while (src[c]) { new_name[c] = src[c]; c++; }
                int val = count;
                if (val >= 10) {
                    new_name[c++] = '0' + (val / 10);
                    new_name[c++] = '0' + (val % 10);
                } else {
                    new_name[c++] = '0' + val;
                }
                new_name[c++] = ')';
                new_name[c] = '\0';
            }
            count++;
        } while (file_exists_in_dir(new_name, target_dir));
        add_mock_file(new_name, 1, 2, target_dir, 0, "");
    } else {
        const char* base = "New Text Document";
        if (str_equal(ext, ".docx")) base = "New Document";
        else if (str_equal(ext, ".xlsx")) base = "New Worksheet";
        else if (str_equal(ext, ".odt")) base = "New Text";
        else if (str_equal(ext, ".ods")) base = "New Sheet";
        else if (str_equal(ext, ".lnk")) base = "New Shortcut";

        do {
            if (count == 1) {
                int c = 0;
                while (base[c]) { new_name[c] = base[c]; c++; }
                int ec = 0;
                while (ext[ec]) { new_name[c++] = ext[ec++]; }
                new_name[c] = '\0';
            } else {
                int c = 0;
                while (base[c]) { new_name[c] = base[c]; c++; }
                new_name[c++] = ' ';
                int val = count;
                if (val >= 10) {
                    new_name[c++] = '0' + (val / 10);
                    new_name[c++] = '0' + (val % 10);
                } else {
                    new_name[c++] = '0' + val;
                }
                int ec = 0;
                while (ext[ec]) { new_name[c++] = ext[ec++]; }
                new_name[c] = '\0';
            }
            count++;
        } while (file_exists_in_dir(new_name, target_dir));
        add_mock_file(new_name, 0, 0, target_dir, 1, "Created via New context menu.\n");
    }
}

static void gui_draw_context_menu(int mx, int my) {
    if (!ctx_menu.active) return;
    
    int w = 140;
    int h = (ctx_menu.type == 0 || ctx_menu.type == 2) ? 80 : 100;
    int menu_x = ctx_menu.x;
    int menu_y = ctx_menu.y;
    if (menu_x + w > 1024) menu_x = 1024 - w;
    if (menu_y + h > 768) menu_y = 768 - h;
    
    gui_fill_rounded(menu_x, menu_y, w, h, 0x050E07);
    vesa_draw_rect_outline(menu_x, menu_y, w, h, gui_accent_col);
    
    if (ctx_menu.type == 0 || ctx_menu.type == 2) {
        const char* opts[] = {"View", "Sort by", "Refresh", "New >"};
        for (int i = 0; i < 4; i++) {
            int iy = menu_y + 4 + i * 18;
            int hov = (mx >= menu_x && mx < menu_x + w && my >= iy && my < iy + 18);
            if (hov) {
                vesa_draw_rect(menu_x + 2, iy, w - 4, 18, 0x0A2010);
                if (i == 3) {
                    ctx_menu.sub_active = 1;
                    ctx_menu.sub_type = 2;
                }
            }
            vesa_draw_string(opts[i], menu_x + 8, iy + 4, hov ? gui_accent_col : 0x00AA55);
        }
        
        if (ctx_menu.sub_active && ctx_menu.sub_type == 2) {
            int sw = 150;
            int sh = 58;
            if (wps_installed) sh += 36;
            if (openoffice_installed) sh += 36;
            
            int sub_x = menu_x + w;
            int sub_y = menu_y + 58;
            if (sub_x + sw > 1024) sub_x = menu_x - sw;
            if (sub_y + sh > 768) sub_y = 768 - sh;
            
            gui_fill_rounded(sub_x, sub_y, sw, sh, 0x050E07);
            vesa_draw_rect_outline(sub_x, sub_y, sw, sh, gui_accent_col);
            
            const char* sub_opts[8];
            int s_cnt = 0;
            sub_opts[s_cnt++] = "Folder";
            sub_opts[s_cnt++] = "Shortcut";
            sub_opts[s_cnt++] = "Text Document";
            if (wps_installed) {
                sub_opts[s_cnt++] = "DOCX Document";
                sub_opts[s_cnt++] = "XLSX Document";
            }
            if (openoffice_installed) {
                sub_opts[s_cnt++] = "ODT Document";
                sub_opts[s_cnt++] = "ODS Document";
            }
            
            for (int i = 0; i < s_cnt; i++) {
                int iy = sub_y + 4 + i * 18;
                int hov = (mx >= sub_x && mx < sub_x + sw && my >= iy && my < iy + 18);
                if (hov) vesa_draw_rect(sub_x + 2, iy, sw - 4, 18, 0x0A2010);
                vesa_draw_string(sub_opts[i], sub_x + 8, iy + 4, hov ? gui_accent_col : 0x00AA55);
            }
        }
    } else {
        const char* opts[] = {"Open", "Cut/Copy", "Rename", "Delete", "Copy as path", "Properties"};
        for (int i = 0; i < 6; i++) {
            int iy = menu_y + 4 + i * 16;
            int hov = (mx >= menu_x && mx < menu_x + w && my >= iy && my < iy + 16);
            if (hov) vesa_draw_rect(menu_x + 2, iy, w - 4, 16, 0x0A2010);
            vesa_draw_string(opts[i], menu_x + 8, iy + 3, hov ? gui_accent_col : 0x00AA55);
        }
    }
}

/* gui_draw: Main GUI render loop */
void gui_draw(void) {
    int w = vesa_get_width();
    int h = vesa_get_height();
    int mx = mouse_get_x();
    int my = mouse_get_y();
    uint8_t buttons = mouse_get_buttons();
    uint64_t ticks = pit_get_ticks();
    int group_x = taskbar_centered ? (w/2 - 200) : 140;

    if (video_playing && video_is_playing) {
        static uint64_t last_video_tick = 0;
        if (ticks - last_video_tick >= 100) {
            last_video_tick = ticks;
            video_play_pos++;
            if (video_play_pos > 100) video_play_pos = 0;
        }
    }
    
    if (video_fullscreen) {
        gui_draw_fullscreen_video(mx, my);
        if ((buttons & 1) && !(prev_buttons & 1)) {
            gui_handle_fullscreen_video_click(mx, my);
        }
        prev_buttons = buttons;
        vesa_flush();
        return;
    }
    
    if (browser_download_active && !browser_download_finished) {
        if (ticks % 2 == 0) {
            browser_download_progress++;
            if (browser_download_progress >= 100) {
                browser_download_progress = 100;
                browser_download_finished = 1;
                add_mock_file(browser_download_filename, 0, browser_download_type, 6, browser_download_size, browser_download_content_ref);
            }
        }
    }
    
    if (!installer_window.closed && installer_step == 1) {
        if (ticks - installer_last_tick >= 10) {
            installer_last_tick = ticks;
            installer_progress += 4;
            if (installer_progress >= 100) {
                installer_progress = 100;
                installer_step = 2;
                if (installer_app_type == 0) {
                    wps_installed = 1;
                    add_mock_file("WPS Writer", 0, 3, 28, 12, "Launch WPS Writer document creator.\n");
                } else if (installer_app_type == 1) {
                    openoffice_installed = 1;
                    add_mock_file("OpenOffice Writer", 0, 3, 28, 8, "Launch OpenOffice Writer document creator.\n");
                } else if (installer_app_type == 2) {
                    chrome_installed = 1;
                    add_mock_file("chrome.exe",          0, 1, 28, 15, "Google Chrome Browser\nPre-installed executable.\n");
                    add_mock_file("chrome.exe",          0, 1, 1, 15, "Google Chrome Browser\nPre-installed executable.\n");
                }
            }
        }
    }
    
    if (snake_active && !snake_game_over) {
        if (ticks - snake_last_tick >= 12) {
            snake_last_tick = ticks;
            int next_x = snake_x[0];
            int next_y = snake_y[0];
            if (snake_dir == 0) next_y--;
            else if (snake_dir == 1) next_x++;
            else if (snake_dir == 2) next_y++;
            else if (snake_dir == 3) next_x--;
            
            if (next_x < 0 || next_x >= 20 || next_y < 0 || next_y >= 20) {
                snake_game_over = 1;
            } else {
                for (int i = 0; i < snake_len; i++) {
                    if (snake_x[i] == next_x && snake_y[i] == next_y) {
                        snake_game_over = 1;
                        break;
                    }
                }
            }
            
            if (!snake_game_over) {
                int ate = (next_x == snake_food_x && next_y == snake_food_y);
                int limit = ate ? snake_len : snake_len - 1;
                for (int i = limit; i > 0; i--) {
                    snake_x[i] = snake_x[i-1];
                    snake_y[i] = snake_y[i-1];
                }
                snake_x[0] = next_x;
                snake_y[0] = next_y;
                
                if (ate) {
                    snake_len++;
                    if (snake_len > 32) snake_len = 32;
                    snake_score += 10;
                    
                    int food_ok = 0;
                    int attempts = 0;
                    while (!food_ok && attempts < 20) {
                        int fx = (int)((ticks + attempts * 7) % 20);
                        int fy = (int)(((ticks / 3) + attempts * 13) % 20);
                        int collision = 0;
                        for (int i = 0; i < snake_len; i++) {
                            if (snake_x[i] == fx && snake_y[i] == fy) {
                                collision = 1;
                                break;
                            }
                        }
                        if (!collision) {
                            snake_food_x = fx;
                            snake_food_y = fy;
                            food_ok = 1;
                        }
                        attempts++;
                    }
                    if (!food_ok) {
                        snake_food_x = (snake_x[0] + 5) % 20;
                        snake_food_y = (snake_y[0] + 5) % 20;
                    }
                }
            }
        }
    }
    
    // -------------------------------------------------------------
    // STATE 1: LOCKSCREEN
    // -------------------------------------------------------------
    if (gui_state == GUI_STATE_LOCKSCREEN) {
        if ((buttons & 1) && !(prev_buttons & 1)) {
            gui_state = GUI_STATE_LOGINSCREEN;
            password_len = 0;
            login_failed = 0;
            prev_buttons = buttons;
            return;
        }
        prev_buttons = buttons;
        
        if (lockscreen_style == 2) {
            // Fill screen with black
            vesa_draw_rect(0, 0, w, h, 0x000000);
            
            // Clean Matrix style: rain streams cascading across the screen
            for (int col = 0; col < 40; col++) {
                int lx = col * 26;
                int ly = (int)((ticks * (col + 3) * 2) % (h + 300)) - 150;
                for (int row = 0; row < 18; row++) {
                    int y_pos = ly + row * 15;
                    if (y_pos >= 0 && y_pos < h) {
                        int alpha = 255 - row * 14;
                        if (alpha > 0) {
                            char mc = '0' + ((ticks + col * 9 + row * 4) % 10);
                            uint32_t c2 = (uint32_t)((alpha * 0) / 255) << 16 |
                                          (uint32_t)((alpha * 190) / 255) << 8 |
                                          (uint32_t)((alpha * 0) / 255);
                            if (row == 0) c2 = 0xFFFFFF; // bright head
                            vesa_draw_char(mc, lx, y_pos, c2);
                        }
                    }
                }
            }
        } else {
            gui_draw_wallpaper();

            /* Animated matrix rain lines (left side) */
            for (int col = 0; col < 8; col++) {
                int lx = 40 + col * 12;
                int ly = (int)((ticks * (col + 1) * 3) % 500);
                for (int row = 0; row < 20; row++) {
                    int alpha = 200 - row * 10;
                    if (alpha > 0) {
                        char mc = '0' + ((ticks + col * 7 + row * 3) % 10);
                        uint32_t c2 = (uint32_t)((alpha * 0) / 255) << 16 |
                                      (uint32_t)((alpha * (gui_accent_col >> 8 & 0xFF)) / 255) << 8 |
                                      (uint32_t)((alpha * (gui_accent_col & 0xFF)) / 255);
                        vesa_draw_char(mc, lx, (ly + row * 14) % 700, c2);
                    }
                }
            }
        }

        /* Large centered clock */
        char clock_str[16];
        char date_ls[16];
        get_live_clock_string(clock_str, date_ls);
        draw_string_scaled(clock_str, 1024/2 - 6*4*4 + 1, 201, 4, 0x001105);
        draw_string_scaled(clock_str, 1024/2 - 6*4*4,     200, 4, gui_accent_col);

        /* Date */
        char lock_date_str[32];
        get_lockscreen_date_string(lock_date_str);
        vesa_draw_string(lock_date_str, 1024/2 - 68, 298, 0x00AA55);

        /* HUD bracket frame around clock - only draw for default/glitch style */
        if (lockscreen_style != 2) {
            vesa_draw_rect(1024/2 - 130, 185, 12, 2, gui_accent_col);
            vesa_draw_rect(1024/2 - 130, 185, 2, 10, gui_accent_col);
            vesa_draw_rect(1024/2 + 118, 185, 12, 2, gui_accent_col);
            vesa_draw_rect(1024/2 + 128, 185, 2, 10, gui_accent_col);
            vesa_draw_rect(1024/2 - 130, 308, 12, 2, gui_accent_col);
            vesa_draw_rect(1024/2 - 130, 302, 2, 8, gui_accent_col);
            vesa_draw_rect(1024/2 + 118, 308, 12, 2, gui_accent_col);
            vesa_draw_rect(1024/2 + 128, 302, 2, 8, gui_accent_col);
        }

        /* Bottom sign-in prompt card */
        int lc_w = 320, lc_h = 60;
        int lc_x = (1024 - lc_w) / 2, lc_y = 768 - lc_h - 36;
        vesa_draw_rect(lc_x, lc_y, lc_w, lc_h, 0x050D08);
        vesa_draw_rect_outline(lc_x, lc_y, lc_w, lc_h, gui_accent_col);
        /* corner brackets */
        vesa_draw_rect(lc_x, lc_y, 10, 2, gui_accent_col);
        vesa_draw_rect(lc_x, lc_y, 2, 10, gui_accent_col);
        vesa_draw_rect(lc_x + lc_w - 10, lc_y, 10, 2, gui_accent_col);
        vesa_draw_rect(lc_x + lc_w - 2,  lc_y, 2, 10, gui_accent_col);
        /* avatar */
        for (int ay2 = -16; ay2 <= 16; ay2++) {
            for (int ax2 = -16; ax2 <= 16; ax2++) {
                if (ax2*ax2 + ay2*ay2 <= 256)
                    vesa_put_pixel(lc_x + 34 + ax2, lc_y + 30 + ay2, 0x00AA55);
            }
        }
        vesa_draw_string("AETHER_ADMIN", lc_x + 60, lc_y + 18, gui_accent_col);
        uint32_t blink = ((ticks / 40) % 2 == 0) ? 0x00AA55 : 0x003322;
        vesa_draw_string("[ CLICK OR PRESS ANY KEY TO SIGN IN ]", lc_x + 60, lc_y + 36, blink);

        if (lockscreen_style == 1) {
            // Apply cyber glitch horizontal shift offsets to the backbuffer before flushing
            uint32_t* bb = vesa_get_backbuffer();
            if ((ticks % 32) < 4) {
                int band1_y = (int)((ticks * 17) % (h - 40));
                int band1_h = 10 + (int)((ticks * 3) % 30);
                int shift = ((ticks % 2) == 0) ? 8 : -8;
                for (int y = band1_y; y < band1_y + band1_h && y < h; y++) {
                    uint32_t row_offset = (uint32_t)y * (uint32_t)w;
                    if (shift > 0) {
                        uint32_t temp[8];
                        for (int i = 0; i < shift; i++) temp[i] = bb[row_offset + w - shift + i];
                        for (int x = w - 1; x >= shift; x--) bb[row_offset + x] = bb[row_offset + x - shift];
                        for (int x = 0; x < shift; x++) bb[row_offset + x] = temp[x];
                    } else {
                        int s = -shift;
                        uint32_t temp[8];
                        for (int i = 0; i < s; i++) temp[i] = bb[row_offset + i];
                        for (int x = 0; x < w - s; x++) bb[row_offset + x] = bb[row_offset + x + s];
                        for (int x = w - s; x < w; x++) bb[row_offset + x] = temp[x - (w - s)];
                    }
                }
                int bar_y = (int)((ticks * 7) % (h - 20));
                int bar_x = (int)((ticks * 23) % (w - 100));
                int bar_w = 40 + (int)((ticks * 11) % 120);
                int bar_h = 2 + (int)(ticks % 6);
                uint32_t glitch_col = ((ticks % 3) == 0) ? 0xFF00FF : (((ticks % 3) == 1) ? 0x00FFFF : 0x00FF88);
                vesa_draw_rect(bar_x, bar_y, bar_w, bar_h, glitch_col);
            }
        }

        vesa_flush();
        return;
    }
    
    // -------------------------------------------------------------
    // STATE 2: LOGINSCREEN
    // -------------------------------------------------------------
    if (gui_state == GUI_STATE_LOGINSCREEN) {
        int btn_hover = (mx >= 512 - 60 && mx < 512 + 60 && my >= 360 && my < 390);
        
        if ((buttons & 1) && !(prev_buttons & 1)) {
            if (btn_hover && password_len > 0) {
                int match = 1;
                const char* secret = system_password;
                for (int i = 0; i < password_len; i++) {
                    if (secret[i] == '\0' || password_buf[i] != secret[i]) {
                        match = 0;
                        break;
                    }
                }
                if (secret[password_len] != '\0') match = 0;
                
                if (match) {
                    gui_state = GUI_STATE_DESKTOP;
                } else {
                    login_failed = 1;
                    password_len = 0;
                }
            }
        }
        prev_buttons = buttons;
        
        gui_draw_wallpaper();

        /* Center login card with HUD border */
        int cc_w = 300, cc_h = 320;
        int cc_x = (1024 - cc_w) / 2, cc_y = 210;
        vesa_draw_rect(cc_x, cc_y, cc_w, cc_h, 0x050D08);
        vesa_draw_rect_outline(cc_x, cc_y, cc_w, cc_h, gui_accent_col);
        /* HUD corners */
        vesa_draw_rect(cc_x, cc_y, 14, 2, gui_accent_col);
        vesa_draw_rect(cc_x, cc_y, 2, 14, gui_accent_col);
        vesa_draw_rect(cc_x+cc_w-14, cc_y, 14, 2, gui_accent_col);
        vesa_draw_rect(cc_x+cc_w-2,  cc_y, 2, 14, gui_accent_col);
        vesa_draw_rect(cc_x, cc_y+cc_h-2, 14, 2, gui_accent_col);
        vesa_draw_rect(cc_x, cc_y+cc_h-14, 2, 14, gui_accent_col);
        vesa_draw_rect(cc_x+cc_w-14, cc_y+cc_h-2, 14, 2, gui_accent_col);
        vesa_draw_rect(cc_x+cc_w-2,  cc_y+cc_h-14, 2, 14, gui_accent_col);

        /* Avatar circle */
        int av_cx = 1024 / 2, av_cy = cc_y + 60;
        for (int ay2 = -36; ay2 <= 36; ay2++) {
            for (int ax2 = -36; ax2 <= 36; ax2++) {
                int d2 = ax2*ax2 + ay2*ay2;
                if (d2 <= 36*36 && d2 >= 28*28)
                    vesa_put_pixel(av_cx + ax2, av_cy + ay2, gui_accent_col);
                else if (d2 < 28*28)
                    vesa_put_pixel(av_cx + ax2, av_cy + ay2, 0x041A0B);
            }
        }
        /* head */
        for (int ay2 = -10; ay2 <= 10; ay2++) {
            for (int ax2 = -10; ax2 <= 10; ax2++) {
                if (ax2*ax2 + ay2*ay2 <= 100)
                    vesa_put_pixel(av_cx + ax2, av_cy + ay2, 0x00BB66);
            }
        }

        /* Username */
        vesa_draw_string("AETHER_ADMIN",   av_cx - 48, av_cy + 46, gui_accent_col);
        vesa_draw_string("Local Account",  av_cx - 52, av_cy + 60, 0x005522);

        /* Separator */
        vesa_draw_line(cc_x + 20, cc_y + 140, cc_x + cc_w - 20, cc_y + 140, gui_accent_col);
        vesa_draw_char('>', cc_x + 20, cc_y + 143, 0x00AA55);
        vesa_draw_char('<', cc_x + cc_w - 28, cc_y + 143, 0x00AA55);

        /* Password field */
        int pf_x = cc_x + 20, pf_y = cc_y + 158, pf_w = cc_w - 40, pf_h = 28;
        vesa_draw_rect(pf_x, pf_y, pf_w, pf_h, 0x020A05);
        vesa_draw_rect_outline(pf_x, pf_y, pf_w, pf_h,
            login_failed ? 0xFF3333 : (password_len > 0 ? gui_accent_col : 0x1A4A2A));
        vesa_draw_line(pf_x+1, pf_y+pf_h-1, pf_x+pf_w-2, pf_y+pf_h-1,
            password_len > 0 ? gui_accent_col : 0x1A4A2A);
        vesa_draw_string(">", pf_x + 6, pf_y + 10, gui_accent_col);
        if (password_len == 0) {
            vesa_draw_string("password_", pf_x + 18, pf_y + 10, 0x1A4A2A);
        } else {
            for (int i = 0; i < password_len && i < 16; i++) {
                vesa_draw_rect(pf_x + 20 + i*10 + 3, pf_y + 11, 5, 5, gui_accent_col);
            }
        }
        if ((ticks / 30) % 2 == 0) {
            int cur_x = pf_x + 18 + password_len * 10;
            vesa_draw_line(cur_x, pf_y + 4, cur_x, pf_y + pf_h - 4, gui_accent_col);
        }

        /* Sign in button */
        int sb_x = pf_x, sb_y = pf_y + 38, sb_w = pf_w, sb_h = 28;
        uint32_t sb_bg = btn_hover ? (gui_accent_col == 0x00FF88 ? 0x003322 : 0x0C2512) : 0x010D05;
        vesa_draw_rect(sb_x, sb_y, sb_w, sb_h, sb_bg);
        vesa_draw_rect_outline(sb_x, sb_y, sb_w, sb_h, btn_hover ? gui_accent_col : 0x00AA55);
        vesa_draw_string("[ SIGN IN ]", sb_x + sb_w/2 - 44, sb_y + 10,
            btn_hover ? gui_accent_col : 0x00AA55);

        if (login_failed) {
            vesa_draw_string("! INCORRECT  hint: aether", cc_x + 20, cc_y + cc_h - 28, 0xFF3333);
        }

        vesa_flush();
        return;
    }
    
    // -------------------------------------------------------------
    // STATE 3: DESKTOP
    // -------------------------------------------------------------
    
    // 1. Process right clicks
    if ((buttons & 2) && !(prev_buttons & 2)) {
        start_menu_open = 0;
        quick_settings_open = 0;
        ctx_menu.active = 0;
        ctx_menu.sub_active = 0;
        
        int window_clicked = 0;
        for (int i = MAX_WINS - 1; i >= 0; i--) {
            gui_window_t* win = window_order[i];
            if (win->minimized || win->closed) continue;
            
            if (mx >= win->x && mx < win->x + win->w &&
                my >= win->y && my < win->y + win->h) {
                
                window_clicked = 1;
                bring_to_front(win);
                
                if (win == &files_window) {
                    int fy = my - (win->y + 40);
                    int fx = mx - win->x;
                    if (fx >= 110 && fx < win->w && fy >= 44 && fy < win->h - 10) {
                        int row_clicked = (fy - 44) / 16;
                        int match_row_idx = 0;
                        int found_idx = -1;
                        for (int k = 0; k < file_system_count; k++) {
                            file_entry_t* f = &file_system[k];
                            int match = 0;
                            if (files_search_len > 0) {
                                int found_sub = 0;
                                for (int si = 0; f->name[si] != '\0'; si++) {
                                    int matched_sub = 1;
                                    for (int sj = 0; sj < files_search_len; sj++) {
                                        if (f->name[si + sj] == '\0') { matched_sub = 0; break; }
                                        char fsc = f->name[si + sj];
                                        if (fsc >= 'A' && fsc <= 'Z') fsc = fsc - 'A' + 'a';
                                        char ssc = files_search_query[sj];
                                        if (ssc >= 'A' && ssc <= 'Z') ssc = ssc - 'A' + 'a';
                                        if (fsc != ssc) { matched_sub = 0; break; }
                                    }
                                    if (matched_sub) { found_sub = 1; break; }
                                }
                                if (files_current_dir == 99) match = found_sub && f->deleted;
                                else match = found_sub && !f->deleted && !f->is_dir;
                            } else {
                                if (files_current_dir == 99) match = f->deleted;
                                else match = (!f->deleted && f->dir_id == files_current_dir);
                            }
                            
                            if (match) {
                                if (match_row_idx == row_clicked) {
                                    found_idx = k;
                                    break;
                                }
                                match_row_idx++;
                            }
                        }
                        
                        if (found_idx >= 0) {
                            ctx_menu.active = 1;
                            ctx_menu.x = mx;
                            ctx_menu.y = my;
                            ctx_menu.type = 1; // File item
                            ctx_menu.target_idx = found_idx;
                            ctx_menu.sub_active = 0;
                        } else {
                            ctx_menu.active = 1;
                            ctx_menu.x = mx;
                            ctx_menu.y = my;
                            ctx_menu.type = 2; // Empty folder
                            ctx_menu.target_idx = -1;
                            ctx_menu.sub_active = 0;
                        }
                    }
                }
                break;
            }
        }
        
        if (!window_clicked) {
            ctx_menu.active = 1;
            ctx_menu.x = mx;
            ctx_menu.y = my;
            ctx_menu.type = 0; // Empty Desktop space
            ctx_menu.target_idx = -1;
            ctx_menu.sub_active = 0;
        }
        
        prev_buttons = buttons;
        return;
    }

    // 2. Process left clicks
    if ((buttons & 1) && !(prev_buttons & 1)) {
        if (ctx_menu.active) {
            int menu_w = 140;
            int menu_h = (ctx_menu.type == 0 || ctx_menu.type == 2) ? 80 : 100;
            int menu_x = ctx_menu.x;
            int menu_y = ctx_menu.y;
            if (menu_x + menu_w > 1024) menu_x = 1024 - menu_w;
            if (menu_y + menu_h > 768) menu_y = 768 - menu_h;
            
            int sub_clicked = 0;
            if (ctx_menu.sub_active && ctx_menu.sub_type == 2) {
                int sw = 150;
                int sh = 58;
                if (wps_installed) sh += 36;
                if (openoffice_installed) sh += 36;
                int sub_x = menu_x + menu_w;
                int sub_y = menu_y + 58;
                if (sub_x + sw > 1024) sub_x = menu_x - sw;
                if (sub_y + sh > 768) sub_y = 768 - sh;
                
                if (mx >= sub_x && mx < sub_x + sw && my >= sub_y && my < sub_y + sh) {
                    sub_clicked = 1;
                    int sub_row = (my - (sub_y + 4)) / 18;
                    const char* sub_exts[8];
                    int s_cnt = 0;
                    sub_exts[s_cnt++] = ""; // folder
                    sub_exts[s_cnt++] = ".lnk"; // shortcut
                    sub_exts[s_cnt++] = ".txt"; // text doc
                    if (wps_installed) {
                        sub_exts[s_cnt++] = ".docx";
                        sub_exts[s_cnt++] = ".xlsx";
                    }
                    if (openoffice_installed) {
                        sub_exts[s_cnt++] = ".odt";
                        sub_exts[s_cnt++] = ".ods";
                    }
                    if (sub_row >= 0 && sub_row < s_cnt) {
                        create_file_extension(sub_exts[sub_row]);
                    }
                }
            }
            
            if (!sub_clicked && mx >= menu_x && mx < menu_x + menu_w && my >= menu_y && my < menu_y + menu_h) {
                int menu_row = (my - (menu_y + 4)) / 18;
                if (ctx_menu.type == 0 || ctx_menu.type == 2) {
                    if (menu_row == 2) { // Refresh
                        start_menu_open = 0;
                    }
                } else {
                    menu_row = (my - (menu_y + 4)) / 16;
                    if (menu_row == 0) { // Open
                        file_entry_t* f = &file_system[ctx_menu.target_idx];
                        if (f->is_dir) {
                            files_current_dir = get_folder_dir_id(f, ctx_menu.target_idx);
                            
                            files_window.minimized = 0;
                            files_window.closed = 0;
                            bring_to_front(&files_window);
                        } else if (is_executable_file(f)) {
                            if (str_equal(f->name, "reboot")) gui_reboot();
                            else if (str_equal(f->name, "sysinfo")) { monitor_window.minimized = 0; monitor_window.closed = 0; bring_to_front(&monitor_window); }
                            else if (str_equal(f->name, "shell")) { console_window.minimized = 0; console_window.closed = 0; bring_to_front(&console_window); }
                            else if (str_equal(f->name, "snake.exe")) { init_snake_game(); }
                            else if (str_equal(f->name, "matrix.exe")) { wallpaper_style = 2; }
                            else if (str_equal(f->name, "chrome.exe")) { browser_window.closed = 0; browser_window.minimized = 0; bring_to_front(&browser_window); }
                            else if (str_equal(f->name, "Chrome Setup.exe")) {
                                installer_step = 0;
                                installer_app_type = 2;
                                installer_progress = 0;
                                installer_window.closed = 0;
                                installer_window.minimized = 0;
                                bring_to_front(&installer_window);
                            }
                            else if (str_equal(f->name, "wps_setup.exe")) {
                                installer_step = 0;
                                installer_app_type = 0;
                                installer_progress = 0;
                                installer_window.closed = 0;
                                installer_window.minimized = 0;
                                bring_to_front(&installer_window);
                            }
                            else if (str_equal(f->name, "openoffice_setup.exe")) {
                                installer_step = 0;
                                installer_app_type = 1;
                                installer_progress = 0;
                                installer_window.closed = 0;
                                installer_window.minimized = 0;
                                bring_to_front(&installer_window);
                            }
                        } else {
                            notepad_len = 0;
                            for (int c = 0; f->content[c] != '\0' && c < 1023; c++) {
                                notepad_buf[notepad_len++] = f->content[c];
                            }
                            notepad_buf[notepad_len] = '\0';
                            notepad_window.closed = 0;
                            notepad_window.minimized = 0;
                            bring_to_front(&notepad_window);
                            gui_focus = FOCUS_NOTEPAD;
                            notepad_editing_idx = ctx_menu.target_idx;
                        }
                    }
                    else if (menu_row == 2) { // Rename
                        file_entry_t* f = &file_system[ctx_menu.target_idx];
                        int l = 0; while (f->name[l]) l++;
                        if (l < 20) {
                            f->name[l++] = '2'; f->name[l] = '\0';
                        }
                    }
                    else if (menu_row == 3) { // Delete
                        file_system[ctx_menu.target_idx].deleted = 1;
                    }
                    else if (menu_row == 4) { // Copy as path
                        file_entry_t* f = &file_system[ctx_menu.target_idx];
                        notepad_len = 0;
                        const char* prefix = "/home/documents/";
                        while (*prefix && notepad_len < 100) notepad_buf[notepad_len++] = *prefix++;
                        int c = 0;
                        while (f->name[c] && notepad_len < 1023) notepad_buf[notepad_len++] = f->name[c++];
                        notepad_buf[notepad_len] = '\0';
                    }
                    else if (menu_row == 5) { // Properties
                        file_entry_t* f = &file_system[ctx_menu.target_idx];
                        char* dest = info_dialog_msg_buf;
                        for (int k = 0; k < 256; k++) dest[k] = '\0';
                        
                        const char* l1 = "File Properties\n================\nName: ";
                        while (*l1) *dest++ = *l1++;
                        const char* n = f->name;
                        while (*n) *dest++ = *n++;
                        const char* l2 = "\nType: ";
                        while (*l2) *dest++ = *l2++;
                        const char* t = f->is_dir ? "Folder" : "Document File";
                        while (*t) *dest++ = *t++;
                        const char* l3 = "\nSize: ";
                        while (*l3) *dest++ = *l3++;
                        int sz = f->size_kb;
                        if (sz == 0) *dest++ = '0';
                        else {
                            char temp[10]; int ti = 0;
                            while (sz > 0) { temp[ti++] = '0' + (sz % 10); sz /= 10; }
                            for (int j = ti-1; j >= 0; j--) *dest++ = temp[j];
                        }
                        const char* l4 = " KB\nStatus: System Verified\n";
                        while (*l4) *dest++ = *l4++;
                        
                        info_dialog.title = "Properties";
                        info_dialog.message = info_dialog_msg_buf;
                        info_dialog.show = 1;
                    }
                }
            }
            
            ctx_menu.active = 0;
            ctx_menu.sub_active = 0;
            prev_buttons = buttons;
            return;
        }
        if (quick_settings_open) {
            int qsw = 240, qsh = 210;
            int qsx = w - qsw - 10;
            int qsy = (taskbar_position == 0) ? (48 + 8) : (h - 48 - qsh - 8);

            if (mx >= qsx && mx < qsx + qsw && my >= qsy && my < qsy + qsh) {
                // Clicked inside Quick Settings
                // 1. Wi-Fi Toggle
                if (mx >= qsx + 15 && mx < qsx + 115 && my >= qsy + 32 && my < qsy + 54) {
                    settings_wifi_enabled = !settings_wifi_enabled;
                }
                // 2. Bluetooth Toggle
                else if (mx >= qsx + 125 && mx < qsx + 225 && my >= qsy + 32 && my < qsy + 54) {
                    bluetooth_enabled = !bluetooth_enabled;
                }
                // 3. Night Light Toggle
                else if (mx >= qsx + 15 && mx < qsx + 115 && my >= qsy + 62 && my < qsy + 84) {
                    night_light_active = !night_light_active;
                }
                // 4. Performance Mode cycle
                else if (mx >= qsx + 125 && mx < qsx + 225 && my >= qsy + 62 && my < qsy + 84) {
                    performance_mode = (performance_mode + 1) % 3;
                }
                // 5. Volume Slider click
                else if (mx >= qsx + 15 && mx < qsx + 225 && my >= qsy + 108 && my < qsy + 118) {
                    int clicked_pct = ((mx - (qsx + 15)) * 100) / 210;
                    if (clicked_pct < 0) clicked_pct = 0;
                    if (clicked_pct > 100) clicked_pct = 100;
                    system_volume = clicked_pct;
                }
                
                prev_buttons = buttons;
                return; // Consume click
            } else {
                // Clicked outside Quick Settings, close it
                quick_settings_open = 0;
            }
        }
        if (snake_active) {
            int sw = 320, sh = 350;
            int sx = (1024 - sw) / 2;
            int sy = (768 - sh) / 2;
            
            int close_x = sx + sw - 20;
            int close_y = sy + 6;
            if (mx >= close_x && mx < close_x + 12 && my >= close_y && my < close_y + 18) {
                snake_active = 0;
                prev_buttons = buttons;
                return;
            }
            
            int board_x = sx + 40;
            int board_y = sy + 40;
            if (snake_game_over) {
                if (mx >= board_x && mx < board_x + 240 && my >= board_y && my < board_y + 240) {
                    init_snake_game();
                    prev_buttons = buttons;
                    return;
                }
            }
            
            int ctrl_y = board_y + 270;
            int btn_w = 32;
            int btn_h = 24;
            int btn_xs[] = {board_x + 92, board_x + 50, board_x + 92, board_x + 134};
            int btn_ys[] = {ctrl_y, ctrl_y + 20, ctrl_y + 20, ctrl_y + 20};
            int dirs_val[] = {0, 3, 2, 1};
            
            for (int i = 0; i < 4; i++) {
                if (mx >= btn_xs[i] && mx < btn_xs[i] + btn_w && my >= btn_ys[i] && my < btn_ys[i] + btn_h) {
                    int new_dir = dirs_val[i];
                    if (new_dir == 0 && snake_dir != 2) snake_dir = 0;
                    else if (new_dir == 1 && snake_dir != 3) snake_dir = 1;
                    else if (new_dir == 2 && snake_dir != 0) snake_dir = 2;
                    else if (new_dir == 3 && snake_dir != 1) snake_dir = 3;
                    prev_buttons = buttons;
                    return;
                }
            }
            
            if (mx >= sx && mx < sx + sw && my >= sy && my < sy + sh) {
                prev_buttons = buttons;
                return;
            }
        }
        if (info_dialog.show) {
            if (mx >= info_dialog.x + info_dialog.w - 20 && mx < info_dialog.x + info_dialog.w - 8 &&
                my >= info_dialog.y + 6 && my < info_dialog.y + 18) {
                info_dialog.show = 0;
            } else {
                int btn_w = 60;
                int btn_h = 22;
                int btn_x = info_dialog.x + (info_dialog.w - btn_w) / 2;
                int btn_y = info_dialog.y + info_dialog.h - 35;
                if (mx >= btn_x && mx < btn_x + btn_w && my >= btn_y && my < btn_y + btn_h) {
                    info_dialog.show = 0;
                }
            }
        }
        else if (trash_dialog.show) {
            if (mx >= trash_dialog.x + trash_dialog.w - 20 && mx < trash_dialog.x + trash_dialog.w - 8 &&
                my >= trash_dialog.y + 6 && my < trash_dialog.y + 18) {
                trash_dialog.show = 0;
            } else {
                int btn_w = 60;
                int btn_h = 22;
                int btn_x = trash_dialog.x + (trash_dialog.w - btn_w) / 2;
                int btn_y = trash_dialog.y + trash_dialog.h - 35;
                if (mx >= btn_x && mx < btn_x + btn_w && my >= btn_y && my < btn_y + btn_h) {
                    trash_dialog.show = 0;
                }
            }
        }
        else if (start_menu_open) {
            int sm_sx = (1024 - 380) / 2;
            int sm_sy = (taskbar_position == 0) ? (48 + 8) : (768 - 48 - 380 - 8);
            if (mx >= sm_sx && mx < sm_sx + 380 && my >= sm_sy && my < sm_sy + 380) {
                if (mx >= sm_sx + 20 && mx < sm_sx + 360 && my >= sm_sy + 18 && my < sm_sy + 46) {
                    gui_focus = FOCUS_START_SEARCH;
                    prev_buttons = buttons;
                    return; // keep open
                }
                
                if (mx >= sm_sx + 20 && mx < sm_sx + 236 && my >= sm_sy + 76 && my < sm_sy + 340) {
                    int col = (mx - (sm_sx + 20)) / 72;
                    int row = (my - (sm_sy + 76)) / 66;
                    int clicked_cell = row * 3 + col;
                    
                    int actual_idx = -1;
                    int rendered_count = 0;
                    for (int i = 0; i < 12; i++) {
                        if (i == 7 && !chrome_installed) continue;
                        if (i == 10 && !wps_installed) continue;
                        if (i == 11 && !openoffice_installed) continue;
                        if (start_search_len > 0) {
                            if (!str_contains_nocase(start_apps[i].n, start_search_query)) continue;
                        }
                        if (rendered_count == clicked_cell) {
                            actual_idx = i;
                            break;
                        }
                        rendered_count++;
                    }
                    
                    if (actual_idx == 0) { files_window.minimized = 0; files_window.closed = 0; files_current_dir = 100; bring_to_front(&files_window); }
                    else if (actual_idx == 1) { console_window.minimized = 0; console_window.closed = 0; bring_to_front(&console_window); }
                    else if (actual_idx == 2) { monitor_window.minimized = 0; monitor_window.closed = 0; bring_to_front(&monitor_window); }
                    else if (actual_idx == 3) { trash_dialog.show = 1; }
                    else if (actual_idx == 4) { files_window.minimized = 0; files_window.closed = 0; files_current_dir = 100; bring_to_front(&files_window); }
                    else if (actual_idx == 5) { notepad_window.minimized = 0; notepad_window.closed = 0; bring_to_front(&notepad_window); }
                    else if (actual_idx == 6) { calc_window.minimized = 0; calc_window.closed = 0; bring_to_front(&calc_window); }
                    else if (actual_idx == 7) { browser_window.minimized = 0; browser_window.closed = 0; bring_to_front(&browser_window); }
                    else if (actual_idx == 8) { settings_window.minimized = 0; settings_window.closed = 0; bring_to_front(&settings_window); }
                    else if (actual_idx == 9) { gui_reboot(); }
                    else if (actual_idx == 10) { wps_window.minimized = 0; wps_window.closed = 0; bring_to_front(&wps_window); }
                    else if (actual_idx == 11) { openoffice_window.minimized = 0; openoffice_window.closed = 0; bring_to_front(&openoffice_window); }
                }
                
                if (mx >= sm_sx + 254 && mx < sm_sx + 374 && my >= sm_sy + 76 && my < sm_sy + 76 + 7 * 32) {
                    int f_row = (my - (sm_sy + 76)) / 32;
                    if (f_row == 0) { files_window.minimized = 0; files_window.closed = 0; files_current_dir = 29; bring_to_front(&files_window); }
                    else if (f_row == 1) { files_window.minimized = 0; files_window.closed = 0; files_current_dir = 30; bring_to_front(&files_window); }
                    else if (f_row == 2) { files_window.minimized = 0; files_window.closed = 0; files_current_dir = 31; bring_to_front(&files_window); }
                    else if (f_row == 3) { files_window.minimized = 0; files_window.closed = 0; files_current_dir = 6; bring_to_front(&files_window); }
                    else if (f_row == 4) { files_window.minimized = 0; files_window.closed = 0; files_current_dir = 32; bring_to_front(&files_window); }
                    else if (f_row == 5) { settings_window.minimized = 0; settings_window.closed = 0; bring_to_front(&settings_window); }
                    else if (f_row == 6) { files_window.minimized = 0; files_window.closed = 0; files_current_dir = 100; bring_to_front(&files_window); }
                }
                
                int userY = sm_sy + 380 - 36;
                if (mx >= sm_sx + 380 - 56 && mx < sm_sx + 380 - 36 && my >= userY + 6 && my < userY + 26) {
                    gui_reboot();
                }
                else if (mx >= sm_sx + 380 - 28 && mx < sm_sx + 380 - 8 && my >= userY + 6 && my < userY + 26) {
                    gui_shutdown();
                }
                
                prev_buttons = buttons;
                return;
            }
            start_menu_open = 0;
        }
        else {
            /* Taskbar click dynamic offset */
            int tb_y = (taskbar_position == 0) ? 0 : (h - 48);
            int tb_top = tb_y + 4,  tb_bot = tb_y + 44;
            int sb_cx  = group_x;

            int bxs[8];
            bxs[0] = sb_cx;
            bxs[1] = sb_cx + 46;
            bxs[2] = sb_cx + 46*2;
            bxs[3] = sb_cx + 46*3;
            bxs[4] = sb_cx + 46*4;
            bxs[5] = sb_cx + 46*5;
            bxs[6] = sb_cx + 46*6;
            bxs[7] = sb_cx + 46*7;

            if (my >= tb_top && my < tb_bot) {
                if (mx >= 64 && mx < 96) {
                    save_workspace(active_workspace);
                    load_workspace(0);
                }
                else if (mx >= 100 && mx < 132) {
                    save_workspace(active_workspace);
                    load_workspace(1);
                }
                else if (mx >= w - 110 && mx < w) {
                    quick_settings_open = !quick_settings_open;
                }
                else if (mx >= bxs[0] && mx < bxs[0]+42) {
                    start_menu_open = !start_menu_open;
                }
                else {
                    struct {
                        gui_window_t* win;
                    } click_tbapps[10] = {
                        {&console_window},
                        {&monitor_window},
                        {&files_window},
                        {&notepad_window},
                        {&calc_window},
                        {&settings_window},
                        {&browser_window},
                        {&wps_window},
                        {&openoffice_window},
                        {&installer_window}
                    };
                    int click_open_count = 0;
                    int bslot = 46;
                    for (int ti = 0; ti < 10; ti++) {
                        gui_window_t* tw = click_tbapps[ti].win;
                        if (tw->closed) continue;
                        int bx3 = group_x + (click_open_count + 1) * bslot;
                        if (mx >= bx3 && mx < bx3 + 42) {
                            if (!tw->minimized && tw->active) {
                                tw->minimized = 1;
                                tw->active = 0;
                            } else {
                                tw->minimized = 0;
                                bring_to_front(tw);
                            }
                            break;
                        }
                        click_open_count++;
                    }
                }
            }
            else {
                /* Check open windows Z-order clicks */
                int window_clicked = 0;
                for (int i = MAX_WINS - 1; i >= 0; i--) {
                    gui_window_t* win = window_order[i];
                    if (win->minimized || win->closed) continue;
                    
                    if (mx >= win->x && mx < win->x + win->w &&
                        my >= win->y && my < win->y + win->h) {
                        
                        window_clicked = 1;
                        
                        int close_x = win->x + win->w - 32;
                        int max_x = win->x + win->w - 56;
                        int min_x = win->x + win->w - 80;
                        if (mx >= close_x && mx < close_x + 20 &&
                            my >= win->y + 6 && my < win->y + 20) {
                            win->closed = 1; win->active = 0;
                            if (gui_focus == FOCUS_NOTEPAD && win == &notepad_window) gui_focus = FOCUS_NONE;
                        }
                        else if (mx >= min_x && mx < min_x + 20 &&
                                 my >= win->y + 6 && my < win->y + 20) {
                            win->minimized = 1; win->active = 0;
                            if (gui_focus == FOCUS_NOTEPAD && win == &notepad_window) gui_focus = FOCUS_NONE;
                        }
                        else if (mx >= max_x && mx < max_x + 20 &&
                                 my >= win->y + 6 && my < win->y + 20) {
                            if (win->maximized) {
                                win->maximized = 0;
                                win->x = win->prev_x;
                                win->y = win->prev_y;
                                win->w = win->prev_w;
                                win->h = win->prev_h;
                            } else {
                                win->maximized = 1;
                                win->prev_x = win->x;
                                win->prev_y = win->y;
                                win->prev_w = win->w;
                                win->prev_h = win->h;
                                win->x = 56;
                                win->y = (taskbar_position == 0) ? 48 : 0;
                                win->w = w - 56;
                                win->h = h - 48;
                            }
                        }
                        else if (mx >= win->x && mx < win->x + win->w &&
                                 my >= win->y && my < win->y + 32) {
                            bring_to_front(win);
                            if (!win->maximized) {
                                dragging_win   = win;
                                drag_offset_x  = mx - win->x;
                                drag_offset_y  = my - win->y;
                            }
                        }
                        else {
                            bring_to_front(win);
                            
                            /* Files Window interaction click mapping */
                            if (win == &files_window) {
                                int fy = my - (win->y + 40);
                                int fx = mx - win->x;
                                if (fx >= 1 && fx < 110) {
                                    int sb_row = (fy - 2) / 24;
                                    int sb_dirs[] = {100, 28, 29, 30, 31, 32, 6, 99};
                                    if (sb_row >= 0 && sb_row < 8) {
                                        files_current_dir = sb_dirs[sb_row];
                                        files_selected_idx = -1;
                                        files_search_len = 0;
                                        files_search_query[0] = '\0';
                                        gui_focus = FOCUS_NONE;
                                    }
                                } else {
                                    int search_w = 120;
                                    int search_x = win->w - search_w - 10;
                                    if (fx >= search_x && fx < search_x + search_w && fy >= -4 && fy < 14) {
                                        gui_focus = FOCUS_FILES_SEARCH;
                                    }
                                    else if (fx >= 118 && fx < 118 + 44 && fy >= 16 && fy < 32) {
                                        if (files_current_dir != 99) {
                                            char new_name[32];
                                            format_stat(new_name, "newfile_", file_system_count - 12, ".txt");
                                            add_mock_file(new_name, 0, 0, files_current_dir, 0, "Empty notes file. Type anything here!\n");
                                            files_selected_idx = file_system_count - 1;
                                        }
                                    }
                                    else if (fx >= 168 && fx < 168 + 60 && fy >= 16 && fy < 32) {
                                        if (files_current_dir != 99) {
                                            char new_name[32];
                                            format_stat(new_name, "folder_", file_system_count - 12, "");
                                            add_mock_file(new_name, 1, 2, files_current_dir, 0, "");
                                            files_selected_idx = file_system_count - 1;
                                        }
                                    }
                                    else if (files_current_dir == 99) {
                                        int res_btn_x = 118 + 116;
                                        if (files_selected_idx >= 0 && fx >= res_btn_x && fx < res_btn_x + 54 && fy >= 16 && fy < 32) {
                                            file_system[files_selected_idx].deleted = 0;
                                            file_system[files_selected_idx].dir_id = 5;
                                            files_selected_idx = -1;
                                        }
                                        int emp_btn_x = 118 + 176;
                                        if (fx >= emp_btn_x && fx < emp_btn_x + 44 && fy >= 16 && fy < 32) {
                                            int r_ptr = 0;
                                            for (int w_ptr = 0; w_ptr < file_system_count; w_ptr++) {
                                                if (!file_system[w_ptr].deleted) {
                                                    file_system[r_ptr++] = file_system[w_ptr];
                                                }
                                            }
                                            file_system_count = r_ptr;
                                            files_selected_idx = -1;
                                        }
                                    } else {
                                        int del_btn_x = 118 + 116;
                                        if (files_selected_idx >= 0 && fx >= del_btn_x && fx < del_btn_x + 50 && fy >= 16 && fy < 32) {
                                            file_system[files_selected_idx].deleted = 1;
                                            file_system[files_selected_idx].dir_id = 99;
                                            files_selected_idx = -1;
                                        }
                                    }
                                    
                                    int list_start_y = 78;
                                    if (fy >= list_start_y) {
                                        int row_clicked = (fy - list_start_y) / 16;
                                        int match_row_idx = 0;
                                        int found_idx = -1;
                                        for (int i = 0; i < file_system_count; i++) {
                                            file_entry_t* f = &file_system[i];
                                            int match = 0;
                                            if (files_search_len > 0) {
                                                int found_sub = 0;
                                                for (int si = 0; f->name[si] != '\0'; si++) {
                                                    int matched_sub = 1;
                                                    for (int sj = 0; sj < files_search_len; sj++) {
                                                        if (f->name[si + sj] == '\0') { matched_sub = 0; break; }
                                                        char fsc = f->name[si + sj];
                                                        if (fsc >= 'A' && fsc <= 'Z') fsc = fsc - 'A' + 'a';
                                                        char ssc = files_search_query[sj];
                                                        if (ssc >= 'A' && ssc <= 'Z') ssc = ssc - 'A' + 'a';
                                                        if (fsc != ssc) { matched_sub = 0; break; }
                                                    }
                                                    if (matched_sub) { found_sub = 1; break; }
                                                }
                                                if (files_current_dir == 99) match = found_sub && f->deleted;
                                                else match = found_sub && !f->deleted && !f->is_dir;
                                            } else {
                                                if (files_current_dir == 99) match = f->deleted;
                                                else match = (!f->deleted && f->dir_id == files_current_dir);
                                            }
                                            
                                            if (match) {
                                                if (match_row_idx == row_clicked) {
                                                    found_idx = i;
                                                    break;
                                                }
                                                match_row_idx++;
                                            }
                                        }
                                        if (found_idx >= 0) {
                                            if (files_selected_idx == found_idx) {
                                                file_entry_t* f = &file_system[found_idx];
                                                if (f->is_dir) {
                                                    files_current_dir = get_folder_dir_id(f, found_idx);
                                                    files_selected_idx = -1;
                                                    files_search_len = 0;
                                                    files_search_query[0] = '\0';
                                                } else {
                                                    if (is_executable_file(f)) {
                                                        int is_reboot = 1;
                                                        const char* r_str = "reboot";
                                                        for (int c = 0; f->name[c] != '\0' || r_str[c] != '\0'; c++) {
                                                            if (f->name[c] != r_str[c]) { is_reboot = 0; break; }
                                                        }
                                                        int is_sysinfo = 1;
                                                        const char* s_str = "sysinfo";
                                                        for (int c = 0; f->name[c] != '\0' || s_str[c] != '\0'; c++) {
                                                            if (f->name[c] != s_str[c]) { is_sysinfo = 0; break; }
                                                        }
                                                        int is_shell = 1;
                                                        const char* sh_str = "shell";
                                                        for (int c = 0; f->name[c] != '\0' || sh_str[c] != '\0'; c++) {
                                                            if (f->name[c] != sh_str[c]) { is_shell = 0; break; }
                                                        }
                                                        int is_snake = 1;
                                                        const char* sn_str = "snake.exe";
                                                        for (int c = 0; f->name[c] != '\0' || sn_str[c] != '\0'; c++) {
                                                            if (f->name[c] != sn_str[c]) { is_snake = 0; break; }
                                                        }
                                                        int is_matrix = 1;
                                                        const char* mt_str = "matrix.exe";
                                                        for (int c = 0; f->name[c] != '\0' || mt_str[c] != '\0'; c++) {
                                                            if (f->name[c] != mt_str[c]) { is_matrix = 0; break; }
                                                        }
                                                        if (is_reboot) gui_reboot();
                                                        else if (is_sysinfo) { monitor_window.minimized = 0; bring_to_front(&monitor_window); }
                                                        else if (is_shell) { console_window.minimized = 0; bring_to_front(&console_window); }
                                                        else if (is_snake) { init_snake_game(); }
                                                        else if (is_matrix) { wallpaper_style = 2; }
                                                        else if (str_equal(f->name, "chrome.exe")) { browser_window.closed = 0; browser_window.minimized = 0; bring_to_front(&browser_window); }
                                                        else if (str_equal(f->name, "Chrome Setup.exe")) {
                                                             installer_step = 0;
                                                             installer_app_type = 2;
                                                             installer_progress = 0;
                                                             installer_window.closed = 0;
                                                             installer_window.minimized = 0;
                                                             bring_to_front(&installer_window);
                                                         }
                                                        else if (str_equal(f->name, "wps_setup.exe")) {
                                                             installer_step = 0;
                                                             installer_app_type = 0;
                                                             installer_progress = 0;
                                                             installer_window.closed = 0;
                                                             installer_window.minimized = 0;
                                                             bring_to_front(&installer_window);
                                                         }
                                                         else if (str_equal(f->name, "openoffice_setup.exe")) {
                                                             installer_step = 0;
                                                             installer_app_type = 1;
                                                             installer_progress = 0;
                                                             installer_window.closed = 0;
                                                             installer_window.minimized = 0;
                                                             bring_to_front(&installer_window);
                                                         }
                                                         else if (str_equal(f->name, "WPS Writer")) {
                                                             wps_window.closed = 0;
                                                             wps_window.minimized = 0;
                                                             bring_to_front(&wps_window);
                                                         }
                                                         else if (str_equal(f->name, "OpenOffice Writer")) {
                                                             openoffice_window.closed = 0;
                                                             openoffice_window.minimized = 0;
                                                             bring_to_front(&openoffice_window);
                                                         }
                                                    } else {
                                                        notepad_len = 0;
                                                        for (int c = 0; f->content[c] != '\0' && c < 1023; c++) {
                                                            notepad_buf[notepad_len++] = f->content[c];
                                                        }
                                                        notepad_buf[notepad_len] = '\0';
                                                        notepad_window.minimized = 0;
                                                        bring_to_front(&notepad_window);
                                                        gui_focus = FOCUS_NOTEPAD;
                                                    }
                                                }
                                            } else {
                                                files_selected_idx = found_idx;
                                            }
                                        } else {
                                            files_selected_idx = -1;
                                        }
                                    }
                                }
                            }
                            /* Telemetry Window click actions */
                            else if (win == &monitor_window) {
                                int fy = my - (win->y + 40);
                                int fx = mx - win->x;
                                if (fx >= 235 && fx < 295) {
                                    if (fy >= 155 && fy < 171) {
                                        if (task_is_suspended(1)) task_resume(1);
                                        else task_suspend(1);
                                    }
                                    else if (fy >= 171 && fy < 187) {
                                        if (task_is_suspended(2)) task_resume(2);
                                        else task_suspend(2);
                                    }
                                }
                            }
                            /* Notepad click actions */
                            else if (win == &notepad_window) {
                                if (notepad_save_dialog_open) {
                                    int dw = 320, dh = 180;
                                    int cx = notepad_window.x + 10;
                                    int dx = cx + (notepad_window.w - 20 - dw) / 2;
                                    int dy = notepad_window.y + (notepad_window.h - dh) / 2;
                                    
                                    if (mx >= dx + 10 && mx < dx + dw - 10 && my >= dy + 50 && my < dy + 70) {
                                        gui_focus = FOCUS_NOTEPAD_SAVE_PATH;
                                    }
                                    else if (mx >= dx + 10 && mx < dx + dw - 10 && my >= dy + 95 && my < dy + 115) {
                                        gui_focus = FOCUS_NOTEPAD_SAVE_NAME;
                                    }
                                    else if (mx >= dx + 50 && mx < dx + 130 && my >= dy + 135 && my < dy + 157) {
                                        int target_dir_id = -1;
                                        if (str_equal(notepad_save_path, "/home/documents")) target_dir_id = 29;
                                        else if (str_equal(notepad_save_path, "/downloads")) target_dir_id = 6;
                                        else if (str_equal(notepad_save_path, "/home/desktop")) target_dir_id = 28;
                                        else if (str_equal(notepad_save_path, "/home/pictures")) target_dir_id = 30;
                                        else if (str_equal(notepad_save_path, "/home/music")) target_dir_id = 31;
                                        else if (str_equal(notepad_save_path, "/home/videos")) target_dir_id = 32;
                                        else if (str_equal(notepad_save_path, "/bin")) target_dir_id = 1;
                                        else if (str_equal(notepad_save_path, "/boot")) target_dir_id = 2;
                                        else if (str_equal(notepad_save_path, "/dev")) target_dir_id = 3;
                                        else if (str_equal(notepad_save_path, "/etc")) target_dir_id = 4;
                                        else if (str_equal(notepad_save_path, "/home")) target_dir_id = 5;
                                        else if (str_equal(notepad_save_path, "/")) target_dir_id = 100;
                                        
                                        if (target_dir_id == -1) {
                                            target_dir_id = 29;
                                        }
                                        
                                        int existing_idx = -1;
                                        for (int k = 0; k < file_system_count; k++) {
                                            if (!file_system[k].deleted && file_system[k].dir_id == target_dir_id) {
                                                if (str_equal(file_system[k].name, notepad_save_name)) {
                                                    existing_idx = k;
                                                    break;
                                                }
                                            }
                                        }
                                        
                                        if (existing_idx >= 0) {
                                            file_entry_t* f = &file_system[existing_idx];
                                            int l = 0;
                                            while (notepad_buf[l] && l < 1023) {
                                                f->content[l] = notepad_buf[l];
                                                l++;
                                            }
                                            f->content[l] = '\0';
                                            f->size_kb = (l + 1023) / 1024;
                                            notepad_editing_idx = existing_idx;
                                        } else {
                                            int l = 0;
                                            while (notepad_buf[l] && l < 1023) {
                                                notepad_save_content_buf[l] = notepad_buf[l];
                                                l++;
                                            }
                                            notepad_save_content_buf[l] = '\0';
                                            int size_kb = (l + 1023) / 1024;
                                            add_mock_file(notepad_save_name, 0, 0, target_dir_id, size_kb, notepad_save_content_buf);
                                            notepad_editing_idx = file_system_count - 1;
                                        }
                                        
                                        notepad_save_dialog_open = 0;
                                        gui_focus = FOCUS_NOTEPAD;
                                        
                                        info_dialog.title = "Save Successful";
                                        info_dialog.message = "File content has been saved successfully.";
                                        info_dialog.show = 1;
                                    }
                                    else if (mx >= dx + 190 && mx < dx + 270 && my >= dy + 135 && my < dy + 157) {
                                        notepad_save_dialog_open = 0;
                                        gui_focus = FOCUS_NOTEPAD;
                                    }
                                } else {
                                    gui_focus = FOCUS_NOTEPAD;
                                    int fx = mx - win->x;
                                    int fy = my - win->y;
                                    if (fy >= 32 && fy < 52) {
                                        if (fx >= 12 && fx < 62) {
                                            if (notepad_editing_idx >= 0 && notepad_editing_idx < file_system_count) {
                                                file_entry_t* f = &file_system[notepad_editing_idx];
                                                int l = 0;
                                                while (notepad_buf[l] && l < 1023) {
                                                    f->content[l] = notepad_buf[l];
                                                    l++;
                                                }
                                                f->content[l] = '\0';
                                                f->size_kb = (l + 1023) / 1024;
                                                
                                                info_dialog.title = "Save Successful";
                                                info_dialog.message = "File content has been saved successfully.";
                                                info_dialog.show = 1;
                                            } else {
                                                notepad_save_dialog_open = 1;
                                                gui_focus = FOCUS_NOTEPAD_SAVE_NAME;
                                            }
                                        }
                                        else if (fx >= 72 && fx < 162) {
                                            notepad_save_dialog_open = 1;
                                            gui_focus = FOCUS_NOTEPAD_SAVE_NAME;
                                        }
                                    }
                                }
                            }
                            else if (win == &wps_window) {
                                // Focused
                            }
                            else if (win == &openoffice_window) {
                                // Focused
                            }
                            else if (win == &installer_window) {
                                int fx = mx - win->x;
                                int fy = my - win->y;
                                int btn_w = 80, btn_h = 24;
                                int btn_x = win->w - btn_w - 20;
                                int btn_y = win->h - btn_h - 20;
                                if (fx >= btn_x && fx < btn_x + btn_w && fy >= btn_y && fy < btn_y + btn_h) {
                                    if (installer_step == 0) {
                                        installer_step = 1;
                                        installer_progress = 0;
                                        installer_last_tick = ticks;
                                    }
                                    else if (installer_step == 2) {
                                        installer_window.closed = 1;
                                    }
                                }
                            }
                            /* Calculator click actions */
                            else if (win == &calc_window) {
                                int grid_y = win->y + 82;
                                int bw = (win->w - 20) / 4;
                                if (my >= grid_y && my < grid_y + 5 * 36) {
                                    int c2 = (mx - (win->x + 10)) / bw;
                                    int r = (my - grid_y) / 36;
                                    if (c2 >= 0 && c2 < 4 && r >= 0 && r < 5) {
                                        int idx = r * 4 + c2;
                                        if (idx < 20) {
                                            const char* keys[] = {"C", "/", "*", "<",
                                                                  "7", "8", "9", "-",
                                                                  "4", "5", "6", "+",
                                                                  "1", "2", "3", "=",
                                                                  " ", "0", "00", "="};
                                            char k = keys[idx][0];
                                            if (idx == 3) {
                                                if (calc_len > 1) { calc_len--; calc_buf[calc_len] = '\0'; }
                                                else { calc_buf[0] = '0'; calc_buf[1] = '\0'; calc_len = 1; }
                                            } else if (idx == 18) {
                                                calc_press_key('0'); calc_press_key('0');
                                            } else if (idx != 16) {
                                                calc_press_key(k);
                                            }
                                        }
                                    }
                                }
                            }
                            /* Browser Window click actions */
                            else if (win == &browser_window) {
                                int fy = my - win->y;
                                int fx = mx - win->x;
                                
                                // Check Downloads Shelf clicks first
                                if (browser_download_active || browser_download_finished) {
                                    int shelf_h = 36;
                                    int shelf_y = win->h - shelf_h;
                                    if (fy >= shelf_y && fy < win->h) {
                                        int cx_x = win->w - 32;
                                        int cx_y = shelf_y + 10;
                                        if (fx >= cx_x && fx < cx_x + 16 && fy >= cx_y && fy < cx_y + 26) {
                                            browser_download_active = 0;
                                            browser_download_finished = 0;
                                        }
                                        if (browser_download_finished) {
                                            int btn_x = win->w - 180;
                                            int btn_y = shelf_y + 8;
                                            if (fx >= btn_x && fx < btn_x + 120 && fy >= btn_y && fy < btn_y + 28) {
                                                files_window.minimized = 0;
                                                bring_to_front(&files_window);
                                                files_current_dir = 6;
                                                files_selected_idx = -1;
                                                files_search_len = 0;
                                                files_search_query[0] = '\0';
                                                gui_focus = FOCUS_NONE;
                                            }
                                        }
                                        prev_buttons = buttons;
                                        return;
                                    }
                                }
                                
                                if (fy >= 34 && fy < 52) {
                                    int tx = 8;
                                    int tab_w[] = {76, 92, 108};
                                    for (int i = 0; i < 3; i++) {
                                        if (fx >= tx && fx < tx + tab_w[i]) {
                                            browser_active_tab = i;
                                            if (i == 1) browser_page = 0;
                                            gui_focus = FOCUS_NONE;
                                            browser_url_len = 0;
                                            browser_url[0] = '\0';
                                            break;
                                        }
                                        tx += tab_w[i] + 4;
                                    }
                                }
                                else if (fy >= 56 && fy < 76) {
                                    gui_focus = FOCUS_BROWSER_URL;
                                }
                                else if (fy >= 82) {
                                    if (video_playing) {
                                        int back_btn_x = win->w - 140;
                                        int play_y = 102;
                                        int content_y = play_y + 220 + 10 + 8 + 10;
                                        if (fx >= back_btn_x && fx < (win->w - 20) && fy >= 82 && fy < 98) {
                                            video_playing = 0;
                                            browser_video_selected_idx = -1;
                                            prev_buttons = buttons;
                                            return;
                                        }
                                        int sb_x = (win->w - 400) / 2;
                                        int sb_y = play_y + 220 + 10;
                                        if (fx >= sb_x && fx < sb_x + 400 && fy >= sb_y && fy < sb_y + 8) {
                                            video_play_pos = ((fx - sb_x) * 100) / 400;
                                            prev_buttons = buttons;
                                            return;
                                        }
                                        int sb_x_center = (win->w - 400) / 2;
                                        int ctrl_x = sb_x_center + 200;
                                        if (fy >= content_y - 4 && fy < content_y + 14) {
                                            if (fx >= ctrl_x && fx < ctrl_x + 60) {
                                                video_is_playing = !video_is_playing;
                                            } else if (fx >= ctrl_x + 70 && fx < ctrl_x + 160) {
                                                video_fullscreen = 1;
                                            }
                                        }
                                        prev_buttons = buttons;
                                        return;
                                    }
                                     if (browser_youtube_mode >= 1) {
                                        /* ─── YouTube page clicks ─── */

                                        /* Back button in nav bar → go back to Google */
                                        int bk_x = win->x + 6;
                                        if (fx >= bk_x && fx < bk_x + 22 && fy >= 52 && fy < 80) {
                                            if (browser_youtube_mode == 2) {
                                                browser_youtube_mode = 1;
                                                audio_playing = 0;
                                            } else {
                                                browser_youtube_mode = 0;
                                                browser_can_go_back = 0;
                                                browser_yt_search_len = 0;
                                                browser_yt_search[0] = '\0';
                                            }
                                            prev_buttons = buttons;
                                            return;
                                        }

                                        if (browser_youtube_mode == 1) {
                                            /* Click on video card → go to player */
                                            int yt_content_y_est = 138; /* approx after header+cats */
                                            int cols3 = 3;
                                            int card_w3 = (win->w - 40 - (cols3-1)*10) / cols3;
                                            int thumb_h3 = card_w3 * 9 / 16;
                                            int card_h3 = thumb_h3 + 50;
                                            for (int v3 = 0; v3 < 9; v3++) {
                                                int c3 = v3 % cols3;
                                                int r3 = v3 / cols3;
                                                int vx3 = 10 + c3 * (card_w3 + 10);
                                                int vy3 = yt_content_y_est + r3 * (card_h3 + 16);
                                                if (fx >= vx3 && fx < vx3 + card_w3 &&
                                                    fy >= vy3 && fy < vy3 + card_h3) {
                                                    audio_video_idx = v3;
                                                    browser_youtube_mode = 2;
                                                    audio_playing = 1;
                                                    video_play_pos = 0;
                                                    /* Set audio title */
                                                    const char* vt[] = {
                                                        "Lofi Hip Hop Radio - Beats to Study",
                                                        "Top 10 Songs This Week 2025",
                                                        "Minecraft Survival Let's Play Ep.1",
                                                        "How to Build an OS in C - Full",
                                                        "Fortnite Season 12 Epic Moments",
                                                        "Coding a TCP/IP Stack from Scratch",
                                                        "Best Songs of 2025 Playlist",
                                                        "Gaming PC Build Guide 2025",
                                                        "Python for Beginners - Full Course"
                                                    };
                                                    int ti = 0;
                                                    const char* src = vt[v3 % 9];
                                                    while (*src && ti < 62) audio_title[ti++] = *src++;
                                                    audio_title[ti] = '\0';
                                                    prev_buttons = buttons;
                                                    return;
                                                }
                                            }
                                        } else if (browser_youtube_mode == 2) {
                                            /* Play/pause button click */
                                            int pl_w2 = win->w - 280;
                                            int pl_h2 = pl_w2 * 9 / 16;
                                            if (pl_h2 > 300) pl_h2 = 300;
                                            int ctrl_y2 = 138 + pl_h2 + 14;
                                            if (fx >= 10 + 6 && fx < 10 + 50 && fy >= ctrl_y2 && fy < ctrl_y2 + 20) {
                                                audio_playing = !audio_playing;
                                                prev_buttons = buttons;
                                                return;
                                            }
                                        }

                                    } else
                                    if (browser_active_tab == 0) {
                                        if (!browser_has_searched) {
                                            /* Click search box */
                                            int sb_half_w = 230;
                                            int sbox_x2 = (win->w - sb_half_w * 2) / 2;
                                            if (fx >= sbox_x2 && fx < sbox_x2 + sb_half_w * 2 && fy >= 120 && fy < 148) {
                                                gui_focus = FOCUS_BROWSER_SEARCH;
                                            }
                                            /* Google Search button */
                                            int btn_x1_new = win->w / 2 - 122;
                                            int btn_x2_new = win->w / 2 + 10;
                                            if (fy >= 155 && fy < 179) {
                                                if (fx >= btn_x1_new && fx < btn_x1_new + 112) {
                                                    if (browser_search_len > 0) {
                                                        browser_has_searched = 1;
                                                        browser_can_go_back = 1;
                                                        int i = 0;
                                                        for (; i < browser_search_len; i++) browser_last_search[i] = browser_search_query[i];
                                                        browser_last_search[i] = '\0';
                                                    }
                                                }
                                                if (fx >= btn_x2_new && fx < btn_x2_new + 130) {
                                                    browser_active_tab = 2; /* I'm Feeling Lucky → wiki */
                                                }
                                            }
                                        } else {
                                            /* Back button (new Chrome toolbar) */
                                            int bk_x2 = win->x + 6;
                                            if (fx >= bk_x2 && fx < bk_x2 + 22 && fy >= 52 && fy < 80) {
                                                if (browser_can_go_back) {
                                                    browser_has_searched = 0;
                                                    browser_search_len = 0;
                                                    browser_search_query[0] = '\0';
                                                }
                                                prev_buttons = buttons;
                                                return;
                                            }
                                            /* Click on YouTube result → navigate to YouTube */
                                            int matches_youtube = str_contains_nocase(browser_last_search, "yout") ||
                                                                  str_contains_nocase(browser_last_search, "tube");
                                            if (matches_youtube) {
                                                /* Check if user clicked a YouTube result card or the youtube.com link */
                                                /* Blue title links are at approx content_y+18 each result */
                                                /* First result "YouTube - Broadcast Yourself" */
                                                if (fy >= 190 && fy < 204 && fx >= 10 && fx < 10 + 30*8) {
                                                    browser_youtube_mode = 1;
                                                    browser_can_go_back = 1;
                                                    browser_yt_search_len = 0;
                                                    browser_yt_search[0] = '\0';
                                                    prev_buttons = buttons;
                                                    return;
                                                }
                                            }
                                            /* Click search bar on results page */
                                            if (fy >= 86 && fy < 108) {
                                                gui_focus = FOCUS_BROWSER_SEARCH;
                                            }
                                        }
                                    }

                                    else if (browser_active_tab == 1) {
                                        if (browser_page == 4) {
                                            int ret_x = win->w - 140;
                                            int ret_y = 102;
                                            if (fx >= ret_x && fx < ret_x + 110 && fy >= ret_y && fy < ret_y + 20) {
                                                browser_page = 0;
                                                browser_url_len = 0;
                                                browser_url[0] = '\0';
                                            }
                                            
                                            int content_y_start = 142;
                                            for (int i = 0; i < 3; i++) {
                                                int ry = content_y_start + i * 56;
                                                int btn_x = win->w - 110;
                                                int btn_y = ry + 12;
                                                if (fx >= btn_x && fx < btn_x + 80 && fy >= btn_y && fy < btn_y + 24) {
                                                    browser_download_active = 1;
                                                    browser_download_progress = 0;
                                                    browser_download_finished = 0;
                                                    
                                                    const char* fname = (i == 0) ? "snake.exe" : ((i == 1) ? "matrix.exe" : "sources.txt");
                                                    int k = 0;
                                                    for (; fname[k] != '\0' && k < 31; k++) {
                                                        browser_download_filename[k] = fname[k];
                                                    }
                                                    browser_download_filename[k] = '\0';
                                                    
                                                    browser_download_type = (i == 2) ? 0 : 1;
                                                    browser_download_size = (i == 0) ? 18 : ((i == 1) ? 12 : 8);
                                                    browser_download_content_ref = (i == 2) ? "AetherOS-64 Kernel Source\n==========================\nWritten in C and Assembly.\nMain files:\n- boot.asm: Early page tables\n- kernel.c: Multi-threading scheduler\n- gui.c: VESA window server\n- pmm.c: Physical allocator\n" : "";
                                                }
                                            }
                                        } else {
                                            if (fy >= 142 && fy < 182) {
                                                gui_window_t* w_ptrs[] = {&monitor_window, &files_window, &console_window};
                                                for (int i = 0; i < 4; i++) {
                                                    int bx = 10 + i * 144;
                                                    if (fx >= bx && fx < bx + 130) {
                                                        if (i < 3) {
                                                            w_ptrs[i]->minimized = 0;
                                                            bring_to_front(w_ptrs[i]);
                                                        } else {
                                                            browser_page = 4;
                                                        }
                                                        break;
                                                    }
                                                }
                                            }
                                        }
                                    }
                                }
                            }
                            /* Settings Window click actions */
                            else if (win == &settings_window) {
                                int fy = my - win->y;
                                int fx = mx - win->x;
                                if (fx >= 1 && fx < 90) {
                                    int sb_row = (fy - 42) / 24;
                                    if (sb_row >= 0 && sb_row < 5) {
                                        settings_active_category = sb_row;
                                        settings_search_len = 0;
                                        settings_search_query[0] = '\0';
                                        gui_focus = FOCUS_NONE;
                                    }
                                } else {
                                    int sbox_w = 130;
                                    int sbox_x = win->w - sbox_w - 10;
                                    if (fx >= sbox_x && fx < sbox_x + sbox_w && fy >= 36 && fy < 54) {
                                        gui_focus = FOCUS_SETTINGS_SEARCH;
                                    }
                                    int content_y = fy - 40;
                                    int rx = fx - 100;
                                    if (settings_active_category == 0) {
                                        if (settings_update_stage == 0) {
                                            if (content_y >= 116 && content_y < 140) {
                                                settings_update_stage = 1;
                                                settings_update_progress = 0;
                                            }
                                        } else if (settings_update_stage == 4) {
                                            if (content_y >= 116 && content_y < 140) {
                                                gui_reboot();
                                            }
                                        }
                                        if (content_y >= 190 && content_y < 212) {
                                            if (rx >= 0 && rx < 80) { vesa_set_resolution(1024, 768); gui_adjust_window_bounds(); }
                                            else if (rx >= 86 && rx < 166) { vesa_set_resolution(800, 600); gui_adjust_window_bounds(); }
                                            else if (rx >= 172 && rx < 252) { vesa_set_resolution(640, 480); gui_adjust_window_bounds(); }
                                        }
                                    }
                                    else if (settings_active_category == 1) {
                                        if (content_y >= 30 && content_y < 54) {
                                            uint32_t colors[] = {0x00FF88, 0x00E5FF, 0xFF9100, 0xFF3333};
                                            for (int i = 0; i < 4; i++) {
                                                int bx = i * 72;
                                                if (rx >= bx && rx < bx + 64) {
                                                    gui_accent_col = colors[i];
                                                    break;
                                                }
                                            }
                                        }
                                        else if (content_y >= 82 && content_y < 104) {
                                            if (rx >= 0 && rx < 80) taskbar_centered = 1;
                                            else if (rx >= 90 && rx < 170) taskbar_centered = 0;
                                        }
                                        else if (content_y >= 132 && content_y < 154) {
                                            if (rx >= 0 && rx < 90) wallpaper_style = 0;
                                            else if (rx >= 96 && rx < 186) wallpaper_style = 1;
                                            else if (rx >= 192 && rx < 282) wallpaper_style = 2;
                                        }
                                        else if (content_y >= 156 && content_y < 178) {
                                            if (rx >= 0 && rx < 90) wallpaper_style = 3;
                                            else if (rx >= 96 && rx < 186) wallpaper_style = 4;
                                        }
                                        else if (content_y >= 196 && content_y < 218) {
                                            if (rx >= 0 && rx < 80) taskbar_position = 0;
                                            else if (rx >= 90 && rx < 170) taskbar_position = 1;
                                        }
                                        else if (content_y >= 248 && content_y < 270) {
                                            if (rx >= 0 && rx < 90) lockscreen_style = 0;
                                            else if (rx >= 96 && rx < 186) lockscreen_style = 1;
                                            else if (rx >= 192 && rx < 282) lockscreen_style = 2;
                                        }
                                    }
                                    else if (settings_active_category == 2) {
                                        if (content_y >= 30 && content_y < 54) {
                                            settings_wifi_enabled = !settings_wifi_enabled;
                                        }
                                    }
                                    else if (settings_active_category == 3) {
                                         if (fx >= 100 && fx < 280 && content_y >= 86 && content_y < 104) {
                                             gui_focus = FOCUS_SETTINGS_CUR_PASS;
                                         }
                                         else if (fx >= 100 && fx < 280 && content_y >= 122 && content_y < 140) {
                                             gui_focus = FOCUS_SETTINGS_NEW_PASS;
                                         }
                                         else if (fx >= 100 && fx < 220 && content_y >= 152 && content_y < 172) {
                                             settings_change_password();
                                         }
                                    }
                                }
                            }
                        }
                        break;
                    }
                }
                
                if (!window_clicked) {
                    if (mx >= 10 && mx < 52) {
                        if      (my>=20  && my<68)  { info_dialog.show=1; }
                        else if (my>=90  && my<138) { console_window.minimized=0;  console_window.closed=0; bring_to_front(&console_window); }
                        else if (my>=160 && my<208) { monitor_window.minimized=0;  monitor_window.closed=0; bring_to_front(&monitor_window); }
                        else if (my>=230 && my<278) { trash_dialog.show=1; }
                        else if (my>=300 && my<348) { files_window.minimized=0;    files_window.closed=0; bring_to_front(&files_window); }
                        else if (my>=370 && my<418) { notepad_window.minimized=0;  notepad_window.closed=0; bring_to_front(&notepad_window); }
                        else if (my>=440 && my<488) { calc_window.minimized=0;     calc_window.closed=0; bring_to_front(&calc_window); }
                        else if (my>=510 && my<558) { settings_window.minimized=0; settings_window.closed=0; bring_to_front(&settings_window); }
                        else if (my>=580 && my<628 && chrome_installed) { browser_window.minimized=0;  browser_window.closed=0; bring_to_front(&browser_window); }
                    }
                }
            }
        }
    }
    
    // Drag update
    if (buttons & 1) {
        if (dragging_win) {
            dragging_win->x = mx - drag_offset_x;
            dragging_win->y = my - drag_offset_y;
            if (dragging_win->x < 56)  dragging_win->x = 56;
            if (dragging_win->x + dragging_win->w > w) dragging_win->x = w - dragging_win->w;
            if (taskbar_position == 0) {
                if (dragging_win->y < 48)   dragging_win->y = 48;
                if (dragging_win->y + dragging_win->h > h) dragging_win->y = h - dragging_win->h;
            } else {
                if (dragging_win->y < 0)   dragging_win->y = 0;
                if (dragging_win->y + dragging_win->h > h - 48) dragging_win->y = h - 48 - dragging_win->h;
            }
        }
    } else {
        dragging_win = 0;
    }
    
    prev_buttons = buttons;
    
    /* 2. Wallpaper */
    gui_draw_wallpaper();

    /* 3. Techy dot grid overlay */
    if (wallpaper_style == 0) {
        for (int dy = 24; dy < h - 52; dy += 32) {
            for (int dx = 24; dx < w; dx += 32) {
                vesa_put_pixel(dx, dy, 0x0A2010);
            }
        }
        for (int dy = 24; dy < h - 52; dy += 128) {
            for (int dx = 24; dx < w; dx += 128) {
                vesa_put_pixel(dx,   dy,   gui_accent_col);
                vesa_put_pixel(dx+1, dy,   gui_accent_col);
                vesa_put_pixel(dx,   dy+1, gui_accent_col);
            }
        }
    }

    /* 4. Desktop icons */
    gui_draw_icons(mx, my);

    /* 5. Windows in Z-order */
    for (int i = 0; i < MAX_WINS; i++) {
        gui_window_t* win = window_order[i];
        if (win->minimized || win->closed) continue;
        gui_draw_window_frame(win);
        if      (win == &console_window)  gui_draw_terminal();
        else if (win == &monitor_window)  gui_draw_telemetry();
        else if (win == &files_window)    gui_draw_files();
        else if (win == &notepad_window)  gui_draw_notepad();
        else if (win == &calc_window)     gui_draw_calculator();
        else if (win == &settings_window) gui_draw_settings();
        else if (win == &browser_window)  gui_draw_browser();
        else if (win == &wps_window)      gui_draw_wps();
        else if (win == &openoffice_window) gui_draw_openoffice();
        else if (win == &installer_window) gui_draw_installer();
    }

    /* 6. Dialogs & Snake Game Overlay */
    if (info_dialog.show)   gui_draw_dialog(&info_dialog);
    else if (trash_dialog.show) gui_draw_dialog(&trash_dialog);
    
    if (snake_active) gui_draw_snake_game(mx, my);

    /* 7. Start Menu */
    if (start_menu_open) gui_draw_start_menu(mx, my);

    /* 8. Taskbar */
    int tb_y = (taskbar_position == 0) ? 0 : (h - 48);
    int tb_top = tb_y + 4, tb_bot = tb_y + 44;
    vesa_draw_rect(0, tb_y, w, 48, 0x050E07);
    if (taskbar_position == 0) {
        vesa_draw_line(0, 47, w, 47, gui_accent_col);
        vesa_draw_line(0, 46, w, 46, 0x00AA55);
    } else {
        vesa_draw_line(0, tb_y,     w, tb_y,     gui_accent_col);
        vesa_draw_line(0, tb_y + 1, w, tb_y + 1, 0x00AA55);
    }
    vesa_draw_string("SYS:OK", 6, tb_y + 20, 0x007744);

    // Draw Workspace Switcher
    int ws_x = 64;
    int ws_w = 32;
    int ws_h = 22;
    
    // WS1
    int hov_ws1 = (mx >= ws_x && mx < ws_x + ws_w && my >= tb_y + 12 && my < tb_y + 34);
    uint32_t ws1_bg = (active_workspace == 0) ? 0x002211 : (hov_ws1 ? 0x0A2010 : 0x000000);
    uint32_t ws1_fg = (active_workspace == 0) ? gui_accent_col : 0x00AA55;
    vesa_draw_rect(ws_x, tb_y + 12, ws_w, ws_h, ws1_bg);
    vesa_draw_rect_outline(ws_x, tb_y + 12, ws_w, ws_h, ws1_fg);
    vesa_draw_string("W1", ws_x + 8, tb_y + 19, ws1_fg);
    
    // WS2
    int hov_ws2 = (mx >= ws_x + ws_w + 4 && mx < ws_x + ws_w + 4 + ws_w && my >= tb_y + 12 && my < tb_y + 34);
    uint32_t ws2_bg = (active_workspace == 1) ? 0x002211 : (hov_ws2 ? 0x0A2010 : 0x000000);
    uint32_t ws2_fg = (active_workspace == 1) ? gui_accent_col : 0x00AA55;
    vesa_draw_rect(ws_x + ws_w + 4, tb_y + 12, ws_w, ws_h, ws2_bg);
    vesa_draw_rect_outline(ws_x + ws_w + 4, tb_y + 12, ws_w, ws_h, ws2_fg);
    vesa_draw_string("W2", ws_x + ws_w + 12, tb_y + 19, ws2_fg);

    int btn_top2 = tb_y + 5;
    int btn_h3   = 38;
    int bslot    = 46;

    {
        int bx3 = group_x;
        int hov = (mx >= bx3 && mx < bx3+42 && my >= btn_top2 && my < btn_top2+btn_h3);
        vesa_draw_rect(bx3, btn_top2, 42, btn_h3, (hov || start_menu_open) ? 0x0A2010 : 0x000000);
        vesa_draw_rect_outline(bx3, btn_top2, 42, btn_h3, (hov||start_menu_open) ? gui_accent_col : 0x006633);
        draw_win_grid(bx3+21, btn_top2+19, (hov||start_menu_open) ? gui_accent_col : 0x00AA55);
        if (start_menu_open) vesa_draw_rect(bx3+14, btn_top2+btn_h3-3, 14, 2, gui_accent_col);
    }
    
    struct {
        gui_window_t* win;
        int   shape;
        uint32_t col;
        const char* label;
    } tbapps[10] = {
        {&console_window,  1, 0x00FF88, "Shell"},
        {&monitor_window,  2, 0x44FF99, "Mon"},
        {&files_window,    4, 0xFFAA00, "Files"},
        {&notepad_window,  5, 0x88DDFF, "Notes"},
        {&calc_window,     6, 0xFF88FF, "Calc"},
        {&settings_window, 7, 0xFFFF44, "Set"},
        {&browser_window,  8, 0x88DDFF, "Chrome"},
        {&wps_window,      9, 0x3366FF, "WPS"},
        {&openoffice_window, 10, 0x00A2E8, "OOo"},
        {&installer_window, 11, 0xFF5533, "Setup"}
    };
    int open_count = 0;
    for (int ti = 0; ti < 10; ti++) {
        gui_window_t* tw = tbapps[ti].win;
        if (tw->closed) continue;
        int bx3 = group_x + (open_count + 1)*bslot;
        int open = !tw->minimized;
        int act  = tw->active && open;
        int hov  = (mx >= bx3 && mx < bx3+42 && my >= btn_top2 && my < btn_top2+btn_h3);
        uint32_t bcol = act ? 0x0A2010 : (hov ? 0x061209 : 0x000000);
        uint32_t ecol = act ? tbapps[ti].col : (open ? 0x006633 : 0x1A3020);
        vesa_draw_rect(bx3, btn_top2, 42, btn_h3, bcol);
        vesa_draw_rect_outline(bx3, btn_top2, 42, btn_h3, ecol);
        int ix = bx3 + 5, iy = btn_top2 + 7;
        uint32_t ic = act ? tbapps[ti].col : ecol;
        switch (tbapps[ti].shape) {
            case 1: vesa_draw_char('>', ix+2, iy+2, ic); vesa_draw_char('_', ix+10, iy+2, ic); break;
            case 2: vesa_draw_rect(ix+0,iy+10,4,5,ic); vesa_draw_rect(ix+6,iy+6,4,9,ic); vesa_draw_rect(ix+12,iy+2,4,13,ic); break;
            case 4: vesa_draw_rect(ix,iy+5,18,10,ic); vesa_draw_rect(ix,iy+2,7,4,ic); break;
            case 5: vesa_draw_rect_outline(ix+2,iy,14,18,ic); vesa_draw_line(ix+4,iy+5,ix+14,iy+5,ic); vesa_draw_line(ix+4,iy+9,ix+14,iy+9,ic); break;
            case 6: vesa_draw_rect_outline(ix+2,iy,14,18,ic); vesa_draw_char('+',ix+3,iy+6,ic); break;
            case 7: vesa_draw_rect_outline(ix+4,iy+4,10,10,ic); vesa_put_pixel(ix+9,iy+1,ic); vesa_put_pixel(ix+9,iy+16,ic); break;
            case 8: vesa_draw_rect_outline(ix+2,iy+2,14,14,ic); vesa_draw_line(ix+2,iy+6,ix+16,iy+6,ic); break;
            case 9: vesa_draw_char('W', ix+4, iy+2, ic); vesa_draw_char('P', ix+12, iy+2, ic); break;
            case 10: vesa_draw_char('O', ix+4, iy+2, ic); vesa_draw_char('O', ix+12, iy+2, ic); break;
            case 11: vesa_draw_char('S', ix+4, iy+2, ic); vesa_draw_char('t', ix+12, iy+2, ic); break;
        }
        int ll = 0; while(tbapps[ti].label[ll]) ll++;
        vesa_draw_string(tbapps[ti].label, bx3 + 21 - ll*4, btn_top2 + 24, act ? tbapps[ti].col : ecol);
        if (act) vesa_draw_rect(bx3+9, btn_top2+btn_h3-3, 24, 2, tbapps[ti].col);
        open_count++;
    }
 
     /* System Tray */
     char clk[16];
     char date_tb[16];
     get_live_clock_string(clk, date_tb);
     int tray_hov = (mx >= w - 110 && mx < w && my >= tb_top && my < tb_bot);
     if (tray_hov || quick_settings_open) {
         vesa_draw_rect(w - 110, tb_y + 4, 106, 40, 0x0A2010);
         vesa_draw_rect_outline(w - 110, tb_y + 4, 106, 40, gui_accent_col);
     }
     vesa_draw_string(clk,        w - 78, tb_y + 10, gui_accent_col);
     vesa_draw_string(date_tb,    w - 82, tb_y + 26, 0x007744);
     uint32_t dc = ((ticks/25)%2==0) ? gui_accent_col : 0x003311;
     vesa_draw_rect(w - 100, tb_y + 18, 6, 6, dc);
 
     /* 9. Quick Settings modal panel */
     if (quick_settings_open) {
         gui_draw_quick_settings(mx, my);
     }
 
     /* 10. Night Light warm screen filter overlay */
     if (night_light_active) {
         uint32_t* bb = vesa_get_backbuffer();
         uint32_t size = w * h;
         if (size > 1024 * 768) size = 1024 * 768;
         for (uint32_t i = 0; i < size; i++) {
             uint32_t pixel = bb[i];
             uint32_t r = (pixel >> 16) & 0xFF;
             uint32_t g = (pixel >> 8) & 0xFF;
             uint32_t b = pixel & 0xFF;
             
             // Warm shift: boost red/orange, drop blue
             r = (r * 9) / 8;   if (r > 255) r = 255;
             g = (g * 17) / 16; if (g > 255) g = 255;
             b = (b * 3) / 4;
             
             bb[i] = (r << 16) | (g << 8) | b;
         }
     }

    /* 9. Context Menu */
    if (ctx_menu.active) {
        gui_draw_context_menu(mx, my);
    }

    /* Arrow Cursor */
    for (int i = 0; i < 14; i++) {
        int row_w = i < 8 ? i : 14 - i;
        for (int j = -1; j <= row_w; j++) {
            vesa_put_pixel(mx + j,     my + i,     0x000000);
            vesa_put_pixel(mx + j + 1, my + i,     0x000000);
            vesa_put_pixel(mx + j,     my + i + 1, 0x000000);
        }
    }
    for (int i = 0; i < 13; i++) {
        int row_w = i < 7 ? i : 12 - i;
        for (int j = 0; j <= row_w; j++) {
            vesa_put_pixel(mx + j + 1, my + i + 1, 0xFFFFFF);
        }
    }
    
    vesa_flush();
}

static int is_executable_file(file_entry_t* f) {
    const char* exes[] = {"reboot", "sysinfo", "shell", "snake.exe", "matrix.exe", "wps_setup.exe", "openoffice_setup.exe", "WPS Writer", "OpenOffice Writer", "chrome.exe", "Chrome Setup.exe"};
    for (int i = 0; i < 11; i++) {
        int match = 1;
        for (int c = 0; f->name[c] != '\0' || exes[i][c] != '\0'; c++) {
            if (f->name[c] != exes[i][c]) { match = 0; break; }
        }
        if (match) return 1;
    }
    return 0;
}

static int get_update_in_progress_flag(void) {
    outb(0x70, 0x0A);
    return (inb(0x71) & 0x80);
}

static uint8_t get_rtc_register(int reg) {
    outb(0x70, reg);
    return inb(0x71);
}

static void read_rtc(int *second, int *minute, int *hour, int *day, int *month, int *year) {
    while (get_update_in_progress_flag());
    
    *second = get_rtc_register(0x00);
    *minute = get_rtc_register(0x02);
    *hour = get_rtc_register(0x04);
    *day = get_rtc_register(0x07);
    *month = get_rtc_register(0x08);
    *year = get_rtc_register(0x09);
    
    uint8_t registerB = get_rtc_register(0x0B);
    
    if (!(registerB & 0x04)) {
        *second = ((*second & 0xF0) >> 4) * 10 + (*second & 0x0F);
        *minute = ((*minute & 0xF0) >> 4) * 10 + (*minute & 0x0F);
        *hour = ((*hour & 0x70) >> 4) * 10 + (*hour & 0x0F) + (*hour & 0x80);
        *day = ((*day & 0xF0) >> 4) * 10 + (*day & 0x0F);
        *month = ((*month & 0xF0) >> 4) * 10 + (*month & 0x0F);
        *year = ((*year & 0xF0) >> 4) * 10 + (*year & 0x0F);
    }
    
    if (!(registerB & 0x02)) {
        int pm = *hour & 0x80;
        *hour &= 0x7F;
        *hour %= 12;
        if (pm) *hour += 12;
    }
    
    *year += 2000;
}

static void get_live_clock_string(char* time_buf, char* date_buf) {
    int second, minute, hour, day, month, year;
    read_rtc(&second, &minute, &hour, &day, &month, &year);
    
    time_buf[0] = '0' + (hour / 10);
    time_buf[1] = '0' + (hour % 10);
    time_buf[2] = ':';
    time_buf[3] = '0' + (minute / 10);
    time_buf[4] = '0' + (minute % 10);
    time_buf[5] = ':';
    time_buf[6] = '0' + (second / 10);
    time_buf[7] = '0' + (second % 10);
    time_buf[8] = '\0';
    
    date_buf[0] = '0' + (year / 1000);
    date_buf[1] = '0' + ((year / 100) % 10);
    date_buf[2] = '0' + ((year / 10) % 10);
    date_buf[3] = '0' + (year % 10);
    date_buf[4] = '-';
    date_buf[5] = '0' + (month / 10);
    date_buf[6] = '0' + (month % 10);
    date_buf[7] = '-';
    date_buf[8] = '0' + (day / 10);
    date_buf[9] = '0' + (day % 10);
    date_buf[10] = '\0';
}

static const char* get_day_of_week_str(int y, int m, int d) {
    static int t[] = {0, 3, 2, 5, 0, 3, 5, 1, 4, 6, 2, 4};
    y -= m < 3;
    int dow = (y + y/4 - y/100 + y/400 + t[m-1] + d) % 7;
    switch(dow) {
        case 0: return "SUN";
        case 1: return "MON";
        case 2: return "TUE";
        case 3: return "WED";
        case 4: return "THU";
        case 5: return "FRI";
        case 6: return "SAT";
    }
    return "MON";
}

static const char* get_month_str(int m) {
    switch(m) {
        case 1: return "JAN";
        case 2: return "FEB";
        case 3: return "MAR";
        case 4: return "APR";
        case 5: return "MAY";
        case 6: return "JUN";
        case 7: return "JUL";
        case 8: return "AUG";
        case 9: return "SEP";
        case 10: return "OCT";
        case 11: return "NOV";
        case 12: return "DEC";
    }
    return "JUN";
}

static void get_lockscreen_date_string(char* buf) {
    int second, minute, hour, day, month, year;
    read_rtc(&second, &minute, &hour, &day, &month, &year);
    const char* dow = get_day_of_week_str(year, month, day);
    const char* mstr = get_month_str(month);
    
    buf[0] = dow[0]; buf[1] = dow[1]; buf[2] = dow[2];
    buf[3] = ' '; buf[4] = ' ';
    buf[5] = '0' + (day / 10);
    buf[6] = '0' + (day % 10);
    buf[7] = ' ';
    buf[8] = mstr[0]; buf[9] = mstr[1]; buf[10] = mstr[2];
    buf[11] = ' ';
    buf[12] = '0' + (year / 1000);
    buf[13] = '0' + ((year / 100) % 10);
    buf[14] = '0' + ((year / 10) % 10);
    buf[15] = '0' + (year % 10);
    buf[16] = '\0';
}

static void settings_change_password(void) {
    int cur_match = 1;
    for (int i = 0; i < 32; i++) {
        if (settings_cur_pass[i] != system_password[i]) {
            cur_match = 0;
            break;
        }
        if (system_password[i] == '\0') break;
    }
    
    if (!cur_match) {
        const char* err = "Error: Current password incorrect";
        int i = 0;
        while (err[i] && i < 63) { settings_pass_status[i] = err[i]; i++; }
        settings_pass_status[i] = '\0';
        return;
    }
    
    if (settings_new_pass_len == 0) {
        const char* err = "Error: Password cannot be empty";
        int i = 0;
        while (err[i] && i < 63) { settings_pass_status[i] = err[i]; i++; }
        settings_pass_status[i] = '\0';
        return;
    }
    
    for (int i = 0; i < 32; i++) {
        system_password[i] = settings_new_pass[i];
        if (settings_new_pass[i] == '\0') break;
    }
    
    const char* ok = "Success: Password updated";
    int i = 0;
    while (ok[i] && i < 63) { settings_pass_status[i] = ok[i]; i++; }
    settings_pass_status[i] = '\0';
    
    settings_cur_pass[0] = '\0';
    settings_cur_pass_len = 0;
    settings_new_pass[0] = '\0';
    settings_new_pass_len = 0;
}

/* Redirect getters for terminal mapping */
uint16_t* gui_get_terminal_buffer(void) {
    return terminal_buffer;
}
