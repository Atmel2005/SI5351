# ESP32-C3 + Si5351 Signal Generator

<!-- markdownlint-disable MD024 -->

## Deutsch

Ein einfacher Web-basierter Signalgenerator auf einem ESP32-C3 (Lolin SuperMini) mit dem Si5351-Taktgenerator. Das Board spannt ein eigenes WLAN auf und bietet eine Web-Oberfläche zum Einstellen von Frequenz, Schrittweite, Phase und weiteren Parametern.

### Merkmale

- Drei Kanäle (CLK0–CLK2) des Si5351
- Frequenzbereich (praktisch nutzbar) ca. 10 kHz bis 160 MHz
- Web-UI mit Direktanwendung der Einstellungen
- Discrete 7-Schritt-Auflösung für Frequenzänderung (1, 10, 100, 1k, 10k, 100k, 1M Hz)
- Phasen-Offset in Grad (0–360, intern 0–127 Schritte)
- Korrektur in ppm für den Quarz
- Sync-Modus (alle Kanäle folgen Kanal 0)

### Verdrahtung (ESP32-C3 Lolin SuperMini ↔ Si5351-Modul)

- SDA → GPIO 1
- SCL → GPIO 0
- 3V3 → VCC
- GND → GND

### WLAN

- SSID: `SI5351-AP`
- Passwort: `12345678`
- IP: `192.168.0.10`

### Nutzung

1. Mit dem Access Point verbinden.
2. Browser öffnen und `http://192.168.0.10` aufrufen.
3. Frequenzen, Schrittweite, Phase und Einstellungen in der Web-Oberfläche anpassen. Änderungen werden live angewendet; "Speichern" schreibt in Flash.

**Serielle Ausgabe (Beispiel):**

```text
SI5351 found at 0x60
SI5351 init ok (25MHz)
AP: SI5351-AP  IP: 192.168.0.10
```

### Firmware bauen/flashen

- Arduino IDE oder PlatformIO
- Bibliotheken: `Etherkit Si5351`, `ArduinoJson`, `Preferences` (Core)
- Board: ESP32-C3 (Lolin SuperMini)
- I2C-Pins sind im Sketch auf SDA=1 / SCL=0 gesetzt.

### Hinweise

- Sehr niedrige Frequenzen (< einige kHz) sind mit dem Si5351 praktisch nicht sauber erreichbar; UI ist auf ≥10 kHz begrenzt.
- Für bessere Stabilität: sicherstellen, dass das Handy nur mit dem AP verbunden ist (mobile Daten ggf. ausschalten), damit Requests nicht auf Mobilnetz umgeleitet werden.

---

## Українська
Простой веб-генератор на ESP32-C3 (Lolin SuperMini) з генератором Si5351. Контролер піднімає власну Wi‑Fi точку доступу та дає веб-інтерфейс для налаштування частоти, кроку, фази й інших параметрів.

### Можливості

- Три канали (CLK0–CLK2) Si5351
- Робочий діапазон ~10 кГц … 160 МГц
- Веб-UI з миттєвим застосуванням змін
- Дискретний 7-кроковий крок частоти (1, 10, 100, 1k, 10k, 100k, 1M Гц)
- Фазовий зсув у градусах (0–360, всередині 0–127 кроків)
- Корекція кварцу в ppm
- Режим синхронізації (усі канали слідують за каналом 0)

### Підключення (ESP32-C3 Lolin SuperMini ↔ модуль Si5351)

- SDA → GPIO 1
- SCL → GPIO 0
- 3V3 → VCC
- GND → GND

### Wi‑Fi

- SSID: `SI5351-AP`
- Пароль: `12345678`
- IP: `192.168.0.10`

### Як користуватися

1. Підключитися до точки доступу.
2. Відкрити в браузері `http://192.168.0.10`.
3. Налаштувати частоти, крок, фазу та інші параметри у веб-інтерфейсі. Зміни застосовуються одразу; «Зберегти» пише у флеш.

**Приклад виводу в Serial:**

```text
SI5351 found at 0x60
SI5351 init ok (25MHz)
AP: SI5351-AP  IP: 192.168.0.10
```

### Збірка/прошивка

- Arduino IDE або PlatformIO
- Бібліотеки: `Etherkit Si5351`, `ArduinoJson`, `Preferences` (з core)
- Плата: ESP32-C3 (Lolin SuperMini)
- Лінії I2C задані в скетчі: SDA=1 / SCL=0.

### Примітки

- Дуже низькі частоти (< кількох кГц) на Si5351 практично нестабільні; UI обмежений мінімумом 10 кГц.
- Для стабільної роботи через телефон — вимкніть мобільні дані або використовуйте лише Wi‑Fi AP, щоб запити не йшли в LTE.

---

## English
A simple web-based signal generator on ESP32-C3 (Lolin SuperMini) with the Si5351 clock generator. The board starts its own Wi‑Fi AP and provides a web UI to set frequency, step, phase, and other parameters.

### Features

- Three Si5351 channels (CLK0–CLK2)
- Practical range ~10 kHz … 160 MHz
- Web UI with immediate application of settings
- Discrete 7-step frequency increments (1, 10, 100, 1k, 10k, 100k, 1M Hz)
- Phase offset in degrees (0–360, internally 0–127 steps)
- Crystal correction in ppm
- Sync mode (all channels follow channel 0)

### Wiring (ESP32-C3 Lolin SuperMini ↔ Si5351 module)

- SDA → GPIO 1
- SCL → GPIO 0
- 3V3 → VCC
- GND → GND

### Wi‑Fi

- SSID: `SI5351-AP`
- Password: `12345678`
- IP: `192.168.0.10`

### How to use

1. Connect to the access point.
2. Open `http://192.168.0.10` in the browser.
3. Adjust frequencies, step, phase, and other parameters in the web UI. Changes apply immediately; “Save” writes to flash.

**Serial output (example):**

```text
SI5351 found at 0x60
SI5351 init ok (25MHz)
AP: SI5351-AP  IP: 192.168.0.10
```

### Build/flash

- Arduino IDE or PlatformIO
- Libraries: `Etherkit Si5351`, `ArduinoJson`, `Preferences` (core)
- Board: ESP32-C3 (Lolin SuperMini)
- I2C lines set in the sketch: SDA=1 / SCL=0.

### Notes

- Very low frequencies (< a few kHz) on Si5351 are practically unstable; UI is limited to ≥10 kHz.
- For stability via phone: disable mobile data or use only the Wi‑Fi AP so requests don’t go to LTE.

---

## Русский
Простой веб-генератор на ESP32-C3 (Lolin SuperMini) с тактовым генератором Si5351. Контроллер поднимает свою точку доступа Wi‑Fi и предоставляет веб-интерфейс для настройки частоты, шага, фазы и прочих параметров.

### Возможности

- Три канала (CLK0–CLK2) Si5351
- Рабочий диапазон ~10 кГц … 160 МГц
- Веб-UI с мгновенным применением настроек
- Дискретный 7-ступенчатый шаг частоты (1, 10, 100, 1k, 10k, 100k, 1M Гц)
- Фазовый сдвиг в градусах (0–360, внутри 0–127 шагов)
- Коррекция кварца в ppm
- Режим синхронизации (все каналы следуют частоте канала 0)

### Подключение (ESP32-C3 Lolin SuperMini ↔ модуль Si5351)

- SDA → GPIO 1
- SCL → GPIO 0
- 3V3 → VCC
- GND → GND

### Wi‑Fi
- SSID: `SI5351-AP`
- Пароль: `12345678`
- IP: `192.168.0.10`

### Как пользоваться

1. Подключиться к точке доступа.
2. Открыть в браузере `http://192.168.0.10`.
3. Настроить частоты, шаг, фазу и прочие параметры в веб-интерфейсе. Изменения применяются сразу; «Сохранить» записывает во флэш.

**Пример вывода в Serial:**

```text
SI5351 found at 0x60
SI5351 init ok (25MHz)
AP: SI5351-AP  IP: 192.168.0.10
```

### Сборка/прошивка

- Arduino IDE или PlatformIO
- Библиотеки: `Etherkit Si5351`, `ArduinoJson`, `Preferences` (из core)
- Плата: ESP32-C3 (Lolin SuperMini)
- Линии I2C заданы в скетче: SDA=1 / SCL=0.

### Примечания

- Очень низкие частоты (< нескольких кГц) на Si5351 практически неустойчивы; UI ограничен минимумом 10 кГц.
- Для стабильной работы через телефон — отключите мобильные данные или используйте только Wi‑Fi AP, чтобы запросы не уходили в LTE.
