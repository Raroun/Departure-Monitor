#include "display_manager.h"

#include "epd_driver.h"
#include "firasans.h"

#include <stdlib.h>
#include <string.h>
#include <time.h>

static const char* TAG = "Display";

// 4-bit grayscale color values (0 = black, 255 = white)
#define C_BLACK 0x00
#define C_DARK  0x60
#define C_LIGHT 0xC0
#define C_WHITE 0xFF

#define ROW_HEIGHT       58
#define TITLE_HEIGHT     50
#define MARGIN           16
#define MAX_PER_SECTION  5

#define ICON_SIZE        24
#define BATTERY_W        28
#define BATTERY_H        14
#define WARNING_SIZE     22

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

void DisplayManager::showMessage(const char* title, const char* message, const char* detail) {
    size_t fb_size = EPD_WIDTH / 2 * EPD_HEIGHT;
    memset(framebuffer, 0xFF, fb_size);

    draw_text(MARGIN, MARGIN + 40, title, C_BLACK, C_WHITE, true);
    epd_draw_line(MARGIN, MARGIN + 55, EPD_WIDTH - MARGIN, MARGIN + 55, C_DARK, framebuffer);
    draw_text(MARGIN, MARGIN + 95, message, C_DARK, C_WHITE, true);
    if (detail != nullptr && detail[0] != '\0') {
        draw_text(MARGIN, MARGIN + 145, detail, C_LIGHT, C_WHITE, true);
    }

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
    showDepartures(stopName, departures, MAX_PER_SECTION, nullptr, {}, 0, nullptr, 0);
}

void DisplayManager::showDepartures(const char* title1, const std::vector<Departure>& deps1, size_t max1,
                                    const char* title2, const std::vector<Departure>& deps2, size_t max2,
                                    const char* lastUpdate,
                                    time_t lastUpdateEpoch) {
    size_t fb_size = EPD_WIDTH / 2 * EPD_HEIGHT;
    memset(framebuffer, 0xFF, fb_size);

    // Start a bit lower so nothing is clipped at the top edge.
    int y = MARGIN;

    // Section 1
    if (title1 != nullptr && title1[0] != '\0') {
        drawHeader(y, title1, lastUpdate, lastUpdateEpoch, true);
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
        drawHeader(y, title2, nullptr, 0, false);
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

void DisplayManager::setBatteryInfo(int percent, bool low) {
    batteryPercent_ = percent;
    batteryLow_ = low;
}

void DisplayManager::setErrorInfo(const char* error) {
    errorText_ = error;
}

void DisplayManager::drawHeader(int y, const char* title, const char* lastUpdate, time_t lastUpdateEpoch, bool showBattery) {
    epd_fill_rect(0, y, EPD_WIDTH, TITLE_HEIGHT, C_BLACK, framebuffer);

    // Vertical center of the header text (FiraSans is drawn from the baseline).
    int textY = y + 38;

    // Title on the left
    draw_text(MARGIN, textY, title, C_WHITE, C_BLACK, true);

    // Last update time centered
    if (lastUpdate != nullptr && lastUpdate[0] != '\0') {
        int updateW = text_width(lastUpdate);
        bool stale = false;
        if (lastUpdateEpoch > 0) {
            time_t now = time(nullptr);
            stale = difftime(now, lastUpdateEpoch) > 600;  // older than 10 minutes
        }
        uint8_t updateColor = stale ? C_LIGHT : C_WHITE;
        draw_text((EPD_WIDTH - updateW) / 2, textY, lastUpdate, updateColor, C_BLACK, true);
    }

    // Header elements are right-aligned.
    int rightX = EPD_WIDTH - MARGIN;

    // Warning icon if an error is set
    if (errorText_ != nullptr && errorText_[0] != '\0') {
        rightX -= WARNING_SIZE;
        drawWarningIcon(rightX, y + (TITLE_HEIGHT - WARNING_SIZE) / 2);
        rightX -= 8;
    }

    // Battery icon + percentage (only in the first header)
    if (showBattery && batteryPercent_ >= 0) {
        String pct = String(batteryPercent_) + "%";
        int pctW = text_width(pct.c_str());
        rightX -= pctW;
        uint8_t pctColor = batteryLow_ ? C_LIGHT : C_WHITE;
        draw_text(rightX, textY, pct.c_str(), pctColor, C_BLACK, true);

        rightX -= (BATTERY_W + 6);
        drawBatteryIcon(rightX, y + (TITLE_HEIGHT - BATTERY_H) / 2, batteryPercent_);
    }
}

void DisplayManager::drawVehicleIcon(int x, int y, const String& mode) {
    // Icons expect mode to be normalized to one of: bus, train, subway, tram.
    uint8_t color = C_BLACK;
    int cx = x + ICON_SIZE / 2;
    int cy = y + ICON_SIZE / 2;

    if (mode.equalsIgnoreCase("bus")) {
        // Bus front view
        int bodyX = x + 2;
        int bodyY = y + 3;
        int bodyW = ICON_SIZE - 4;
        int bodyH = ICON_SIZE - 10;
        epd_fill_rect(bodyX, bodyY, bodyW, bodyH, C_LIGHT, framebuffer);
        epd_draw_rect(bodyX, bodyY, bodyW, bodyH, color, framebuffer);
        // Windshield
        epd_fill_rect(bodyX + 3, bodyY + 2, bodyW - 6, 6, C_WHITE, framebuffer);
        // Wheels
        epd_fill_circle(bodyX + 5, bodyY + bodyH + 3, 3, color, framebuffer);
        epd_fill_circle(bodyX + bodyW - 5, bodyY + bodyH + 3, 3, color, framebuffer);
    } else if (mode.equalsIgnoreCase("train") ||
               mode.equalsIgnoreCase("subway") ||
               mode.equalsIgnoreCase("tram") ||
               mode.equalsIgnoreCase("suburban") ||
               mode.equalsIgnoreCase("regional") ||
               mode.equalsIgnoreCase("regionalExp") ||
               mode.equalsIgnoreCase("national") ||
               mode.equalsIgnoreCase("nationalExpress")) {
        // Train front view similar to the user-supplied icon.
        int bodyX = x + 2;
        int bodyW = ICON_SIZE - 4;
        int bodyH = ICON_SIZE - 10;
        int radius = bodyW / 2;
        int circleY = y + 2 + radius;

        // Rounded top body: circle on top + rectangle below
        epd_fill_circle(bodyX + radius, circleY, radius, color, framebuffer);
        epd_fill_rect(bodyX, circleY, bodyW, bodyH - radius, color, framebuffer);

        // Large front window
        int winX = bodyX + 3;
        int winY = circleY - radius + 3;
        int winW = bodyW - 6;
        int winH = bodyH - radius - 2;
        epd_fill_rect(winX, winY, winW, winH, C_WHITE, framebuffer);

        // Headlights
        int lightY = circleY + (bodyH - radius) / 2 + 2;
        epd_fill_circle(bodyX + 6, lightY, 3, C_WHITE, framebuffer);
        epd_fill_circle(bodyX + bodyW - 6, lightY, 3, C_WHITE, framebuffer);

        // Rails / sleepers (two diagonal lines)
        int railTopY = circleY + bodyH - radius;
        epd_draw_line(bodyX, railTopY, bodyX - 4, railTopY + 8, color, framebuffer);
        epd_draw_line(bodyX + bodyW, railTopY, bodyX + bodyW + 4, railTopY + 8, color, framebuffer);
    } else {
        // Generic circle for unknown modes
        epd_draw_circle(cx, cy, ICON_SIZE / 2 - 2, color, framebuffer);
    }
}

void DisplayManager::drawBatteryIcon(int x, int y, int percent) {
    uint8_t bodyColor = batteryLow_ ? C_LIGHT : C_WHITE;
    // Battery body
    epd_draw_rect(x, y, BATTERY_W - 3, BATTERY_H, bodyColor, framebuffer);
    // Positive terminal
    epd_fill_rect(x + BATTERY_W - 3, y + 4, 3, 6, bodyColor, framebuffer);

    // Fill level
    int fillW = ((BATTERY_W - 7) * percent) / 100;
    if (fillW < 1) fillW = 1;
    uint8_t fillColor = batteryLow_ ? C_DARK : C_WHITE;
    epd_fill_rect(x + 2, y + 2, fillW, BATTERY_H - 4, fillColor, framebuffer);
}

void DisplayManager::drawWarningIcon(int x, int y) {
    // Triangle with exclamation mark
    int cx = x + WARNING_SIZE / 2;
    epd_fill_triangle(cx, y, x, y + WARNING_SIZE, x + WARNING_SIZE, y + WARNING_SIZE, C_LIGHT, framebuffer);
    epd_draw_line(cx, y + 6, cx, y + 13, C_BLACK, framebuffer);
    epd_fill_rect(cx - 1, y + 15, 3, 3, C_BLACK, framebuffer);
}

void DisplayManager::drawDepartureRow(int y, const Departure& dep, int index) {
    uint8_t bg = (index % 2 == 1) ? C_LIGHT : C_WHITE;
    epd_fill_rect(0, y, EPD_WIDTH, ROW_HEIGHT, bg, framebuffer);

    int x = MARGIN;

    // Vehicle icon (shifted up slightly so it looks centred with the text)
    drawVehicleIcon(x, y + (ROW_HEIGHT - ICON_SIZE) / 2 - 4, dep.mode);
    x += ICON_SIZE + 8;

    // Line number
    draw_text(x, y + 38, dep.line.c_str(), C_BLACK, bg, true);
    int lineW = text_width(dep.line.c_str());
    x += lineW + 16;

    // Direction (truncated if necessary)
    String direction = dep.direction;
    if (direction.length() > 26) {
        direction = direction.substring(0, 23) + "...";
    }
    int maxDirW = EPD_WIDTH - MARGIN - x - 160;
    while (text_width(direction.c_str()) > maxDirW && direction.length() > 3) {
        direction = direction.substring(0, direction.length() - 4) + "...";
    }
    draw_text(x, y + 36, direction.c_str(), C_BLACK, bg, true);

    // Departure time right-aligned
    String timeStr = formatTime(dep.minutesUntil);
    int timeW = text_width(timeStr.c_str());
    int timeX = EPD_WIDTH - MARGIN - timeW;

    // Highlight imminent departures, but not for "Now".
    bool imminent = (dep.minutesUntil > 0 && dep.minutesUntil <= 5);
    if (dep.cancelled) {
        imminent = false;
    }

    if (imminent) {
        int pad = 8;
        int boxH = ROW_HEIGHT - 8;
        int boxY = y + 4;
        epd_fill_rect(timeX - pad, boxY, timeW + 2 * pad, boxH, C_BLACK, framebuffer);
        draw_text(timeX, y + 38, timeStr.c_str(), C_WHITE, C_BLACK, true);
    } else {
        draw_text(timeX, y + 38, timeStr.c_str(), C_BLACK, bg, true);
    }

    // Delay badge with black background, placed further left of the time.
    if (!dep.cancelled && dep.delayMinutes > 0) {
        String delayStr = "+" + String(dep.delayMinutes);
        int delayW = text_width(delayStr.c_str());
        int delayX = timeX - delayW - 20;
        int delayY = y + 12;
        int badgeH = 36;
        epd_fill_rect(delayX - 5, delayY, delayW + 10, badgeH, C_BLACK, framebuffer);
        draw_text(delayX, delayY + 26, delayStr.c_str(), C_WHITE, C_BLACK, true);
    }

    // Cancelled indicator
    if (dep.cancelled) {
        String cancelledStr = "cancelled";
        int cw = text_width(cancelledStr.c_str());
        int cx = EPD_WIDTH - MARGIN - cw;
        draw_text(cx, y + 38, cancelledStr.c_str(), C_DARK, bg, true);
    }

    // Thin separator line
    epd_draw_line(MARGIN, y + ROW_HEIGHT - 1, EPD_WIDTH - MARGIN, y + ROW_HEIGHT - 1, C_DARK, framebuffer);
}

String DisplayManager::formatTime(long minutesUntil) {
    if (minutesUntil <= 0) return String("Now");
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
