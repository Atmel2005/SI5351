# ESP32-C3 + Si5351 Signal Generator

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
```
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
```
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
