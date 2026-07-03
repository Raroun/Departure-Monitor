#include "display_manager.h"

#include "epd_driver.h"
#include "firasans.h"

#include <stdlib.h>
#include <string.h>

static const char* TAG = "Display";

// 4-bit grayscale color values (0 = black, 255 = white)
#define C_BLACK 0x00
#define C_DARK  0x60
#define C_LIGHT 0xC0
#define C_WHITE 0xFF

#define ROW_HEIGHT       58
#define TITLE_HEIGHT     42
#define MARGIN           16
#define MAX_PER_SECTION  5

static uint8_t* framebuffer = nullptr;

static FontProperties font_props(uint8_t fg, uint8_t bg, bool transparent_bg) {
    FontProperties props;
    props.fg_color = fg >> 4;
    props.bg_color = bg >> 4;
    props.fallback_glyph = 0x3F;
    props.flags = transparent_bg ? 0 : DRAW_BACKGROUND;
    return props;
}

static void draw_text(int x, int y, const char* text, uint8_t fg, uint8_t bg, bool transparent_bg) {
    int32_t cursor_x = x;
    int32_t cursor_y = y;
    FontProperties props = font_props(fg, bg, transparent_bg);
    write_mode(&FiraSans, text, &cursor_x, &cursor_y, framebuffer, BLACK_ON_WHITE, &props);
}

static int text_width(const char* text) {
    int32_t x = 0, y = 0, x1 = 0, y1 = 0;
    int32_t w = 0, h = 0;
    FontProperties props = font_props(C_BLACK, C_WHITE, true);
    get_text_bounds(&FiraSans, text, &x, &y, &x1, &y1, &w, &h, &props);
    return (int)w;
}

bool DisplayManager::begin() {
    Serial.printf("[%s] Initializing E-Ink display...\n", TAG);

    epd_init();
    epd_poweron();
    epd_clear();
    epd_poweroff();

    size_t fb_size = EPD_WIDTH / 2 * EPD_HEIGHT;
    framebuffer = (uint8_t*)heap_caps_malloc(fb_size, MALLOC_CAP_SPIRAM);
    if (framebuffer == nullptr) {
        Serial.printf("[%s] Failed to allocate framebuffer in PSRAM!\n", TAG);
        return false;
    }
    memset(framebuffer, 0xFF, fb_size);
    return true;
}

void DisplayManager::showMessage(const char* title, const char* message) {
    size_t fb_size = EPD_WIDTH / 2 * EPD_HEIGHT;
    memset(framebuffer, 0xFF, fb_size);

    draw_text(MARGIN, MARGIN + 40, title, C_BLACK, C_WHITE, true);
    epd_draw_line(MARGIN, MARGIN + 55, EPD_WIDTH - MARGIN, MARGIN + 55, C_DARK, framebuffer);
    draw_text(MARGIN, MARGIN + 95, message, C_DARK, C_WHITE, true);

    epd_poweron();
    epd_clear();
    epd_draw_grayscale_image(epd_full_screen(), framebuffer);
    epd_poweroff();
}

void DisplayManager::showNightMode(const char* timeStr, const char* dateStr) {
    (void)timeStr;  // Not used – screen stays static
    (void)dateStr;  // Not used – screen stays static

    size_t fb_size = EPD_WIDTH / 2 * EPD_HEIGHT;
    memset(framebuffer, 0x00, fb_size);  // black night sky

    // Deterministic stars so the screen looks identical after every wake-up.
    srand(0xBADC0FFE);
    for (int i = 0; i < 140; ++i) {
        int x = 10 + rand() % (EPD_WIDTH - 20);
        int y = 10 + rand() % (EPD_HEIGHT - 130);
        int r = rand() % 5;  // 0..2 px-ish
        uint8_t color = (rand() % 3 == 0) ? C_LIGHT : C_WHITE;
        if (r <= 1) {
            epd_fill_rect(x, y, 1, 1, color, framebuffer);
        } else {
            epd_fill_rect(x, y, r, r, color, framebuffer);
        }
    }

    // Moon (top right)
    const int mx = 780, my = 130, mr = 70;
    epd_fill_circle(mx, my, mr, C_LIGHT, framebuffer);
    // Craters
    epd_fill_circle(mx - 20, my - 15, 12, C_DARK, framebuffer);
    epd_fill_circle(mx + 15, my + 20, 9, C_DARK, framebuffer);
    epd_fill_circle(mx + 25, my - 25, 7, C_DARK, framebuffer);
    epd_fill_circle(mx - 10, my + 30, 6, C_DARK, framebuffer);

    // Ground silhouette at the bottom
    int groundY = EPD_HEIGHT - 45;
    int step = 50;
    for (int x = 0; x < EPD_WIDTH; x += step) {
        int nextX = x + step;
        if (nextX > EPD_WIDTH) nextX = EPD_WIDTH;
        int peakY = groundY - (10 + rand() % 26);
        epd_fill_triangle(x, EPD_HEIGHT, (x + nextX) / 2, peakY, nextX, EPD_HEIGHT, C_DARK, framebuffer);
    }

    // Title
    draw_text(MARGIN, MARGIN + 20, "Night Mode", C_WHITE, C_BLACK, true);

    // Large "Offline" text with a subtle outline
    const char* offline = "Offline";
    int offW = text_width(offline);
    int offX = (EPD_WIDTH - offW) / 2;
    int offY = (EPD_HEIGHT - 80) / 2;
    for (int dx = -1; dx <= 1; ++dx) {
        for (int dy = -1; dy <= 1; ++dy) {
            if (dx == 0 && dy == 0) continue;
            draw_text(offX + dx, offY + dy, offline, C_DARK, C_BLACK, true);
        }
    }
    draw_text(offX, offY, offline, C_WHITE, C_BLACK, true);

    // Info text directly below Offline
    const char* info = "Updates resume at 05:00";
    int infoW = text_width(info);
    int infoX = (EPD_WIDTH - infoW) / 2;
    draw_text(infoX, offY + 60, info, C_LIGHT, C_BLACK, true);

    epd_poweron();
    epd_clear();
    epd_draw_grayscale_image(epd_full_screen(), framebuffer);
    epd_poweroff();
}

void DisplayManager::showDepartures(const char* stopName, const std::vector<Departure>& departures) {
    showDepartures(stopName, departures, MAX_PER_SECTION, nullptr, {}, 0);
}

void DisplayManager::showDepartures(const char* title1, const std::vector<Departure>& deps1, size_t max1,
                                    const char* title2, const std::vector<Departure>& deps2, size_t max2,
                                    const char* lastUpdate) {
    size_t fb_size = EPD_WIDTH / 2 * EPD_HEIGHT;
    memset(framebuffer, 0xFF, fb_size);

    int y = MARGIN;

    // Section 1
    if (title1 != nullptr && title1[0] != '\0') {
        epd_fill_rect(0, y, EPD_WIDTH, TITLE_HEIGHT, C_BLACK, framebuffer);
        draw_text(MARGIN, y + 30, title1, C_WHITE, C_BLACK, true);

        // Show last update time in the top-right corner
        if (lastUpdate != nullptr && lastUpdate[0] != '\0') {
            int tw = text_width(lastUpdate);
            draw_text(EPD_WIDTH - MARGIN - tw, y + 30, lastUpdate, C_WHITE, C_BLACK, true);
        }

        y += TITLE_HEIGHT + 4;
    }

    size_t count1 = 0;
    for (const auto& dep : deps1) {
        drawDepartureRow(y, dep, count1);
        y += ROW_HEIGHT;
        count1++;
        if (count1 >= max1 || y > EPD_HEIGHT - ROW_HEIGHT - TITLE_HEIGHT) break;
    }
    if (deps1.empty()) {
        draw_text(MARGIN, y + 30, "No departures", C_DARK, C_WHITE, true);
        y += ROW_HEIGHT;
    }

    // Section 2
    if (title2 != nullptr && title2[0] != '\0') {
        y += 8;
        if (y + TITLE_HEIGHT + ROW_HEIGHT > EPD_HEIGHT) {
            // Not enough room for a second title -> skip it
            title2 = nullptr;
        }
    }

    if (title2 != nullptr && title2[0] != '\0') {
        epd_fill_rect(0, y, EPD_WIDTH, TITLE_HEIGHT, C_BLACK, framebuffer);
        draw_text(MARGIN, y + 30, title2, C_WHITE, C_BLACK, true);
        y += TITLE_HEIGHT + 4;

        size_t count2 = 0;
        for (const auto& dep : deps2) {
            drawDepartureRow(y, dep, count1 + count2);
            y += ROW_HEIGHT;
            count2++;
            if (count2 >= max2 || y > EPD_HEIGHT - ROW_HEIGHT) break;
        }
        if (deps2.empty()) {
            draw_text(MARGIN, y + 30, "No departures", C_DARK, C_WHITE, true);
        }
    }

    epd_poweron();
    epd_clear();
    epd_draw_grayscale_image(epd_full_screen(), framebuffer);
    epd_poweroff();
}

void DisplayManager::drawDepartureRow(int y, const Departure& dep, int index) {
    uint8_t bg = (index % 2 == 1) ? C_LIGHT : C_WHITE;
    epd_fill_rect(0, y, EPD_WIDTH, ROW_HEIGHT, bg, framebuffer);

    // Line
    draw_text(MARGIN, y + 38, dep.line.c_str(), C_BLACK, bg, true);

    // Direction (truncated if necessary)
    String direction = dep.direction;
    if (direction.length() > 30) {
        direction = direction.substring(0, 27) + "...";
    }
    draw_text(MARGIN + 110, y + 36, direction.c_str(), C_BLACK, bg, true);

    // Departure time right-aligned
    String timeStr = formatTime(dep.minutesUntil);
    if (dep.cancelled) {
        timeStr = "cancelled";
    } else if (dep.delayMinutes > 0) {
        timeStr += " +" + String(dep.delayMinutes);
    }

    int tw = text_width(timeStr.c_str());
    draw_text(EPD_WIDTH - MARGIN - tw, y + 38, timeStr.c_str(), C_BLACK, bg, true);

    // Thin separator line
    epd_draw_line(MARGIN, y + ROW_HEIGHT - 1, EPD_WIDTH - MARGIN, y + ROW_HEIGHT - 1, C_DARK, framebuffer);
}

String DisplayManager::formatTime(long minutesUntil) {
    if (minutesUntil <= 0) return String("now");
    if (minutesUntil < 60) return String(minutesUntil) + " min";

    long hours = minutesUntil / 60;
    long mins = minutesUntil % 60;
    String s = String(hours) + "h";
    if (mins < 10) s += "0";
    s += String(mins);
    return s;
}

void DisplayManager::fullRefresh() {
    epd_poweron();
    epd_clear();
    epd_poweroff();
}

void DisplayManager::sleep() {
    epd_poweroff_all();
}
