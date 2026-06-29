# Abfahrtsmonitor fuer Lilygo T5 4.7" S3 E-Ink

Ein batterie- oder netzbetriebener Abfahrtsmonitor fuer das **Lilygo T5 4.7" S3 V2.4** E-Ink Display (ESP32-S3).

Das Geraet verbindet sich per WLAN mit einer oeffentlichen Verkehrs-API, laedt die naechsten Abfahrten einer Haltestelle und zeigt sie auf dem E-Paper Display an. Anschliessend geht der ESP32 in den Deepsleep, um Strom zu sparen.

## Features

* WLAN-Verbindung mit automatischer NTP-Zeitsynchronisation
* Unterstuetzte APIs:
  * `transport.rest` (BVG Berlin, VBB, ...)
  * VRR EFA rapidJSON (z. B. Castrop-Rauxel)
* Anzeige von Linie, Richtung, Restminuten und Verspaetung
* E-Ink Display mit 4 Graustufen (960 x 540 px)
* Deepsleep zwischen den Aktualisierungen (konfigurierbares Intervall)

## Projektstruktur

```
.
├── include/
│   ├── config.h          # WLAN, Haltestelle, API-Einstellungen
│   ├── departure.h       # Datenstruktur fuer Abfahrten
│   ├── departure_api.h   # API-Client
│   ├── display_manager.h # Display-Wrapper
│   └── wifi_manager.h    # WLAN-Helper
├── src/
│   ├── main.cpp          # Hauptablauf
│   ├── config.cpp        # Linienfilter-Definition
│   ├── departure_api.cpp
│   ├── display_manager.cpp
│   └── wifi_manager.cpp
├── platformio.ini        # PlatformIO Konfiguration
└── README.md
```

## Voraussetzungen

* Python 3
* Das Projekt bringt PlatformIO in einem lokalen virtuellen Environment mit (`.venv`).
* Lilygo T5 4.7" S3 V2.4 E-Ink Board
* USB-C Kabel

## Konfiguration

Oeffne `include/config.h` und passe mindestens folgende Werte an:

### VRR EFA (Standard, z. B. Castrop-Rauxel)

```cpp
#define API_TYPE       API_VRR_EFA
#define API_BASE_URL   "https://openservice-test.vrr.de/openservice"

#define STOP_PLACE     "Castrop-Rauxel"
#define STOP_ID        "Wesselstraße"
#define STOP_NAME      "Castrop-Rauxel, Wesselstraße"
```

### transport.rest (z. B. BVG Berlin)

```cpp
#define API_TYPE       API_TRANSPORT_REST
#define API_BASE_URL   "https://v6.bvg.transport.rest"

#define STOP_ID        "900000100003"
#define STOP_NAME      "Alexanderplatz"
```

### WLAN

```cpp
#define WIFI_SSID       "DEIN_WLAN_SSID"
#define WIFI_PASSWORD   "DEIN_WLAN_PASSWORT"
```

### Nur bestimmte Linien anzeigen

In `src/config.cpp` koennen die erlaubten Linien eingeschraenkt werden:

```cpp
const char* ALLOWED_LINES[] = {"E482", "482"};
const size_t ALLOWED_LINES_COUNT = sizeof(ALLOWED_LINES) / sizeof(ALLOWED_LINES[0]);
```

Leer lassen, um alle Linien anzuzeigen.

## Kompilieren & Flashen

Im Projektverzeichnis:

```bash
# Build
.venv/Scripts/pio run

# Upload (Board per USB anschliessen)
.venv/Scripts/pio run --target upload

# Serial Monitor
.venv/Scripts/pio device monitor
```

## Hinweise zur VRR EFA API

* Der eingetragene `API_BASE_URL` ist der **VRR OpenService Testserver**. Er ist ohne Registrierung nutzbar und fuer Hobby-/Testzwecke gedacht.
* Wenn der Monitor dauerhaft und oeffentlich laufen soll, muss der **Produktivserver** beantragt werden. Eine kurze E-Mail mit Projektbeschreibung an `opendata-oepnv@vrr.de` reicht aus.
* Die Testserver-URL kann jederzeit ohne Ankuendigung geaendert oder eingeschraenkt werden.

## Stromverbrauch

Der ESP32 geht nach jedem Update in den Deepsleep. Das Intervall ist in `include/config.h` einstellbar:

```cpp
#define UPDATE_INTERVAL_SEC 60
```

Fuer einen dauerhaft netzbetriebenen Monitor kann das Intervall kleiner sein (z. B. 30 s). Bei Batteriebetrieb empfiehlt sich ein laengeres Intervall (z. B. 120-300 s).

## Fehlersuche

* **Display bleibt schwarz:** Serial Monitor oeffnen und pruefen, ob WLAN und API verbunden sind.
* **WLAN funktioniert nicht:** SSID und Passwort in `config.h` pruefen (Gross-/Kleinschreibung beachten).
* **API liefert keine Daten:** Pruefen, ob `STOP_ID` / `STOP_PLACE` korrekt sind. Die VRR-Test-API kann zeitweise Antworten verzoegern oder leere Ergebnisse liefern.

## Lizenz

MIT
