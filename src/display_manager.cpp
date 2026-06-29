#include "display_manager.h"

#include "epd_driver.h"
#include "firasans.h"

#include <stdlib.h>
#include <string.h>

static const char* TAG = "Display";

// 4-Bit Graustufen-Farbwerte (0 = schwarz, 255 = weiss)
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
    Serial.printf("[%s] Initialisiere E-Ink Display...\n", TAG);

    epd_init();
    epd_poweron();
    epd_clear();
    epd_poweroff();

    size_t fb_size = EPD_WIDTH / 2 * EPD_HEIGHT;
    framebuffer = (uint8_t*)heap_caps_malloc(fb_size, MALLOC_CAP_SPIRAM);
    if (framebuffer == nullptr) {
        Serial.printf("[%s] Framebuffer konnte nicht im PSRAM alloziert werden!\n", TAG);
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
    (void)timeStr;  // Nicht verwendet – Screen bleibt statisch
    (void)dateStr;  // Nicht verwendet – Screen bleibt statisch

    size_t fb_size = EPD_WIDTH / 2 * EPD_HEIGHT;
    memset(framebuffer, 0xFF, fb_size);

    int y = MARGIN;

    // Schwarzer Kopfbalken mit Titel
    epd_fill_rect(0, y, EPD_WIDTH, TITLE_HEIGHT, C_BLACK, framebuffer);
    draw_text(MARGIN, y + 30, "Night Mode", C_WHITE, C_BLACK, true);
    y += TITLE_HEIGHT + 80;

    // Dekorativer Rahmen
    int boxY = y;
    int boxH = 220;
    epd_draw_rect(MARGIN, boxY, EPD_WIDTH - 2 * MARGIN, boxH, C_DARK, framebuffer);
    epd_draw_rect(MARGIN + 4, boxY + 4, EPD_WIDTH - 2 * MARGIN - 8, boxH - 8, C_LIGHT, framebuffer);

    // Grosser statischer Offline-Text
    const char* offline = "Offline";
    int offW = text_width(offline);
    int offX = (EPD_WIDTH - offW) / 2;
    int offY = boxY + 90;
    for (int dx = -1; dx <= 1; ++dx) {
        for (int dy = -1; dy <= 1; ++dy) {
            draw_text(offX + dx, offY + dy, offline, C_BLACK, C_WHITE, true);
        }
    }

    // Zusatzinfo unten
    const char* info = "Updates resume at 05:00";
    int infoW = text_width(info);
    int infoX = (EPD_WIDTH - infoW) / 2;
    draw_text(infoX, EPD_HEIGHT - MARGIN - 50, info, C_DARK, C_WHITE, true);

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

    // Abschnitt 1
    if (title1 != nullptr && title1[0] != '\0') {
        epd_fill_rect(0, y, EPD_WIDTH, TITLE_HEIGHT, C_BLACK, framebuffer);
        draw_text(MARGIN, y + 30, title1, C_WHITE, C_BLACK, true);

        // Letzte Aktualisierungszeit oben rechts anzeigen
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
        draw_text(MARGIN, y + 30, "Keine Abfahrten", C_DARK, C_WHITE, true);
        y += ROW_HEIGHT;
    }

    // Abschnitt 2
    if (title2 != nullptr && title2[0] != '\0') {
        y += 8;
        if (y + TITLE_HEIGHT + ROW_HEIGHT > EPD_HEIGHT) {
            // Nicht genug Platz fuer einen zweiten Titel -> ueberspringen
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
            draw_text(MARGIN, y + 30, "Keine Abfahrten", C_DARK, C_WHITE, true);
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

    // Linie
    draw_text(MARGIN, y + 38, dep.line.c_str(), C_BLACK, bg, true);

    // Richtung (ggf. gekuerzt)
    String direction = dep.direction;
    if (direction.length() > 30) {
        direction = direction.substring(0, 27) + "...";
    }
    draw_text(MARGIN + 110, y + 36, direction.c_str(), C_BLACK, bg, true);

    // Abfahrtszeit rechtsbuendig
    String timeStr = formatTime(dep.minutesUntil);
    if (dep.cancelled) {
        timeStr = "entfaellt";
    } else if (dep.delayMinutes > 0) {
        timeStr += " +" + String(dep.delayMinutes);
    }

    int tw = text_width(timeStr.c_str());
    draw_text(EPD_WIDTH - MARGIN - tw, y + 38, timeStr.c_str(), C_BLACK, bg, true);

    // Feine Trennlinie
    epd_draw_line(MARGIN, y + ROW_HEIGHT - 1, EPD_WIDTH - MARGIN, y + ROW_HEIGHT - 1, C_DARK, framebuffer);
}

String DisplayManager::formatTime(long minutesUntil) {
    if (minutesUntil <= 0) return String("jetzt");
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
