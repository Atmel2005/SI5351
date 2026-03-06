#include <Arduino.h>
#include <Wire.h>
#include <WiFi.h>
#include <WebServer.h>
#include <Preferences.h>
#include <ArduinoJson.h>
#include "si5351.h"

// I2C pins for ESP32-C3 LOLIN SuperMini
static const int I2C_SDA = 1;
static const int I2C_SCL = 0;

// Access point settings
static const char *AP_SSID = "SI5351-AP";
static const char *AP_PASS = "12345678";
static const IPAddress AP_IP(192, 168, 0, 10);
// Use same gateway as AP IP so DHCP hands out usable route
static const IPAddress AP_GW(192, 168, 0, 10);
static const IPAddress AP_MASK(255, 255, 255, 0);

// Frequency limits (Hz)
static const uint64_t FREQ_MIN = 1ULL;
static const uint64_t FREQ_MAX = 160000000ULL;

struct ChannelConfig {
  uint64_t freq = 1000000;   // 1 MHz default
  uint8_t drive = SI5351_DRIVE_8MA;
  uint8_t phase = 0;         // phase steps (0-127); requires shared PLL for alignment
  uint8_t pll = SI5351_PLLA; // PLLA or PLLB
  uint8_t disable_state = SI5351_CLK_DISABLE_LOW;
  bool invert = false;
  bool int_mode = false;
  bool enabled = true;
};

struct AppConfig {
  bool sync_mode = false; // when true, all outputs follow ch0 frequency
  int32_t correction = 0; // ppb correction for crystal
  ChannelConfig ch[3];
};

Si5351 si5351;
Preferences prefs;
WebServer server(80);
AppConfig cfg;
uint8_t si5351_addr = 0x60;
bool si_found = false;
bool si_ready = false;

uint8_t scanSi5351() {
  uint8_t found = 0;
  for (uint8_t addr = 0x01; addr <= 0x7E; ++addr) { // 7-bit I2C addresses, exclude 0x00/0x7F reserved
    Wire.beginTransmission(addr);
    if (Wire.endTransmission() == 0) {
      found = addr;
      break;
    }
  }
  return found;
}

// Utility: clamp and set one channel
void applyChannel(uint8_t idx) {
  if (!si_ready) return;
  if (idx > 2) return;
  uint64_t target = cfg.sync_mode ? cfg.ch[0].freq : cfg.ch[idx].freq;
  if (target < FREQ_MIN) target = FREQ_MIN;
  if (target > FREQ_MAX) target = FREQ_MAX;

  // Etherkit library expects frequency in hundredths of Hz
  uint64_t freq_hundredths = target * 100ULL;
  si5351.set_ms_source(static_cast<si5351_clock>(idx), static_cast<si5351_pll>(cfg.ch[idx].pll));
  si5351.set_clock_invert(static_cast<si5351_clock>(idx), cfg.ch[idx].invert);
  si5351.set_clock_disable(static_cast<si5351_clock>(idx), static_cast<si5351_clock_disable>(cfg.ch[idx].disable_state));
  si5351.set_int(static_cast<si5351_clock>(idx), cfg.ch[idx].int_mode);
  si5351.drive_strength(static_cast<si5351_clock>(idx), static_cast<si5351_drive>(cfg.ch[idx].drive));
  si5351.set_freq(freq_hundredths, static_cast<si5351_clock>(idx));
  si5351.output_enable(static_cast<si5351_clock>(idx), cfg.ch[idx].enabled);

  // Optional phase offset (0-127). Phase alignment requires PLL reset after setting all.
  uint8_t phase = cfg.ch[idx].phase & 0x7F;
  si5351.set_phase(static_cast<si5351_clock>(idx), phase);
}

void applyAll() {
  if (!si_ready) return;
  si5351.set_correction(cfg.correction, SI5351_PLL_INPUT_XO);
  for (uint8_t i = 0; i < 3; ++i) {
    applyChannel(i);
  }
  // Reset both PLLs to apply phase relationship across outputs
  si5351.pll_reset(SI5351_PLLA);
  si5351.pll_reset(SI5351_PLLB);
}

void saveConfig() {
  prefs.begin("si5351", false);
  prefs.putBool("sync", cfg.sync_mode);
  prefs.putLong("corr", cfg.correction);
  prefs.putULong64("f0", cfg.ch[0].freq);
  prefs.putULong64("f1", cfg.ch[1].freq);
  prefs.putULong64("f2", cfg.ch[2].freq);
  prefs.putUChar("d0", cfg.ch[0].drive);
  prefs.putUChar("d1", cfg.ch[1].drive);
  prefs.putUChar("d2", cfg.ch[2].drive);
  prefs.putUChar("p0", cfg.ch[0].phase);
  prefs.putUChar("p1", cfg.ch[1].phase);
  prefs.putUChar("p2", cfg.ch[2].phase);
  prefs.putUChar("pl0", cfg.ch[0].pll);
  prefs.putUChar("pl1", cfg.ch[1].pll);
  prefs.putUChar("pl2", cfg.ch[2].pll);
  prefs.putUChar("ds0", cfg.ch[0].disable_state);
  prefs.putUChar("ds1", cfg.ch[1].disable_state);
  prefs.putUChar("ds2", cfg.ch[2].disable_state);
  prefs.putBool("iv0", cfg.ch[0].invert);
  prefs.putBool("iv1", cfg.ch[1].invert);
  prefs.putBool("iv2", cfg.ch[2].invert);
  prefs.putBool("im0", cfg.ch[0].int_mode);
  prefs.putBool("im1", cfg.ch[1].int_mode);
  prefs.putBool("im2", cfg.ch[2].int_mode);
  prefs.putBool("e0", cfg.ch[0].enabled);
  prefs.putBool("e1", cfg.ch[1].enabled);
  prefs.putBool("e2", cfg.ch[2].enabled);
  prefs.end();
}

void loadConfig() {
  prefs.begin("si5351", true);
  cfg.sync_mode = prefs.getBool("sync", false);
  cfg.correction = prefs.getLong("corr", 0);
  cfg.ch[0].freq = prefs.getULong64("f0", 1000000);
  cfg.ch[1].freq = prefs.getULong64("f1", 2000000);
  cfg.ch[2].freq = prefs.getULong64("f2", 3000000);
  cfg.ch[0].drive = prefs.getUChar("d0", SI5351_DRIVE_8MA);
  cfg.ch[1].drive = prefs.getUChar("d1", SI5351_DRIVE_8MA);
  cfg.ch[2].drive = prefs.getUChar("d2", SI5351_DRIVE_8MA);
  cfg.ch[0].phase = prefs.getUChar("p0", 0);
  cfg.ch[1].phase = prefs.getUChar("p1", 0);
  cfg.ch[2].phase = prefs.getUChar("p2", 0);
  cfg.ch[0].pll = prefs.getUChar("pl0", SI5351_PLLA);
  cfg.ch[1].pll = prefs.getUChar("pl1", SI5351_PLLA);
  cfg.ch[2].pll = prefs.getUChar("pl2", SI5351_PLLA);
  cfg.ch[0].disable_state = prefs.getUChar("ds0", SI5351_CLK_DISABLE_LOW);
  cfg.ch[1].disable_state = prefs.getUChar("ds1", SI5351_CLK_DISABLE_LOW);
  cfg.ch[2].disable_state = prefs.getUChar("ds2", SI5351_CLK_DISABLE_LOW);
  cfg.ch[0].invert = prefs.getBool("iv0", false);
  cfg.ch[1].invert = prefs.getBool("iv1", false);
  cfg.ch[2].invert = prefs.getBool("iv2", false);
  cfg.ch[0].int_mode = prefs.getBool("im0", false);
  cfg.ch[1].int_mode = prefs.getBool("im1", false);
  cfg.ch[2].int_mode = prefs.getBool("im2", false);
  cfg.ch[0].enabled = prefs.getBool("e0", true);
  cfg.ch[1].enabled = prefs.getBool("e1", true);
  cfg.ch[2].enabled = prefs.getBool("e2", true);
  prefs.end();
}

void buildStateJson(String &out) {
  JsonDocument doc;
  doc["sync_mode"] = cfg.sync_mode;
  doc["correction"] = cfg.correction;
  doc["si_found"] = si_found;
  doc["si_ready"] = si_ready;
  doc["si_addr"] = si5351_addr;
  JsonArray arr = doc["channels"].to<JsonArray>();
  for (uint8_t i = 0; i < 3; ++i) {
    JsonObject ch = arr.add<JsonObject>();
    ch["freq"] = cfg.ch[i].freq;
    ch["drive"] = cfg.ch[i].drive;
    ch["phase"] = cfg.ch[i].phase;
    ch["pll"] = cfg.ch[i].pll;
    ch["disable_state"] = cfg.ch[i].disable_state;
    ch["invert"] = cfg.ch[i].invert;
    ch["int_mode"] = cfg.ch[i].int_mode;
    ch["enabled"] = cfg.ch[i].enabled;
  }
  out.clear();
  serializeJson(doc, out);
}

void sendState() {
  String out;
  buildStateJson(out);
  server.send(200, "application/json", out);
}

void handleGetState() { sendState(); }

void handlePostState() {
  if (server.hasArg("plain")) {
    JsonDocument doc;
    DeserializationError err = deserializeJson(doc, server.arg("plain"));
    if (err) {
      server.send(400, "application/json", "{\"error\":\"bad json\"}");
      return;
    }
    bool persist = !server.hasArg("apply");
    cfg.sync_mode = doc["sync_mode"] | cfg.sync_mode;
    if (doc["correction"].is<int32_t>()) cfg.correction = doc["correction"].as<int32_t>();
    JsonArray arr = doc["channels"].as<JsonArray>();
    if (arr.size() == 3) {
      for (uint8_t i = 0; i < 3; ++i) {
        JsonObject ch = arr[i];
        if (ch["freq"].is<uint64_t>()) cfg.ch[i].freq = ch["freq"].as<uint64_t>();
        if (ch["drive"].is<uint8_t>()) cfg.ch[i].drive = ch["drive"].as<uint8_t>();
        if (ch["phase"].is<uint8_t>()) cfg.ch[i].phase = ch["phase"].as<uint8_t>();
        if (ch["pll"].is<uint8_t>()) cfg.ch[i].pll = ch["pll"].as<uint8_t>();
        if (ch["disable_state"].is<uint8_t>()) cfg.ch[i].disable_state = ch["disable_state"].as<uint8_t>();
        if (ch["invert"].is<bool>()) cfg.ch[i].invert = ch["invert"].as<bool>();
        if (ch["int_mode"].is<bool>()) cfg.ch[i].int_mode = ch["int_mode"].as<bool>();
        if (ch["enabled"].is<bool>()) cfg.ch[i].enabled = ch["enabled"].as<bool>();
      }
    }
    if (persist) saveConfig();
    applyAll();
    sendState();
  } else {
    server.send(400, "application/json", "{\"error\":\"no body\"}");
  }
}

const char PAGE[] PROGMEM = R"rawliteral(
<!doctype html>
<html><head><meta charset="utf-8" />
<title>SI5351 Control</title>
<style>
:root{--bg:#0a0d1a;--panel:#11162b;--panel-2:#162040;--accent:#3ec8ff;--accent-2:#8ef3c5;--text:#e8edf7;--muted:#9aa8c2;--border:rgba(255,255,255,0.08);--shadow:0 12px 30px rgba(0,0,0,0.35);} 
*{box-sizing:border-box;}body{margin:0;padding:20px;font-family:'Segoe UI',system-ui,-apple-system,'Helvetica Neue',sans-serif;background:#0a0d1a;color:var(--text);min-height:100vh;}
.layout{max-width:720px;margin:0 auto;display:flex;flex-direction:column;gap:14px;}
.bar{display:flex;flex-wrap:nowrap;gap:8px;align-items:center;background:var(--panel);padding:6px 10px;border-radius:12px;box-shadow:var(--shadow);border:1px solid var(--border);} 
.bar label{display:flex;flex-direction:row;gap:6px;align-items:center;font-size:14px;margin:0;white-space:nowrap;}
.pill{padding:6px 10px;border-radius:999px;background:rgba(62,200,255,0.12);color:var(--accent);font-weight:600;font-size:13px;flex-shrink:0;}
.sync-gap{margin-left:10px;}
.toggle-btn{border:1px solid var(--border);padding:6px 10px;border-radius:10px;background:rgba(255,255,255,0.06);color:var(--text);cursor:pointer;font-weight:600;flex-shrink:0;-webkit-tap-highlight-color:transparent;transition:background .15s ease,box-shadow .2s ease,border .15s ease,color .15s ease;}
.toggle-btn.active{background:linear-gradient(135deg,var(--accent),#4dd2ff);color:#07101f;box-shadow:0 6px 14px rgba(62,200,255,0.3);border-color:transparent;}
.lang{padding:6px;border-radius:10px;border:1px solid var(--border);background:rgba(255,255,255,0.06);color:var(--text);width:70px;text-align:center;margin-left:auto;margin-right:10px;flex-shrink:0;}
.status{display:flex;gap:10px;align-items:center;font-size:13px;color:var(--muted);flex-shrink:0;} 
.bar label input{width:90px;padding:6px;flex-shrink:0;}
.dot{width:10px;height:10px;border-radius:50%;background:var(--accent);box-shadow:0 0 0 6px rgba(62,200,255,0.15);} 
.card{background:linear-gradient(160deg,var(--panel),var(--panel-2));padding:14px;border-radius:12px;border:1px solid var(--border);box-shadow:var(--shadow);display:flex;gap:14px;}
.card-body{flex:1;display:flex;flex-direction:column;}
.slider-col{width:64px;display:flex;flex-direction:column;align-items:center;gap:10px;}
.freq-slider{writing-mode:vertical-lr;-webkit-appearance:slider-vertical;appearance:slider-vertical;height:260px;width:22px;background:transparent;}
.freq-slider::-webkit-slider-runnable-track{width:22px;height:260px;border-radius:10px;background:rgba(255,255,255,0.08);} 
.freq-slider::-webkit-slider-thumb{-webkit-appearance:none;width:18px;height:18px;border-radius:50%;background:var(--accent);margin-top:-2px;box-shadow:0 2px 8px rgba(0,0,0,0.4);} 
.freq-slider::-moz-range-track{width:22px;height:260px;border-radius:10px;background:rgba(255,255,255,0.08);} 
.freq-slider::-moz-range-thumb{width:18px;height:18px;border-radius:50%;background:var(--accent);box-shadow:0 2px 8px rgba(0,0,0,0.4);} 
.range-step{width:100%;display:flex;justify-content:center;background:transparent;}
.step-slider{writing-mode:vertical-lr;-webkit-appearance:slider-vertical;appearance:slider-vertical;height:140px;width:22px;background:transparent;margin:0 auto;border:none;accent-color:#2ecc71;}
.step-slider::-webkit-slider-runnable-track{width:22px;height:140px;border-radius:10px;background:rgba(255,255,255,0.08);} 
.step-slider::-webkit-slider-thumb{-webkit-appearance:none;width:16px;height:16px;border-radius:50%;background:#2ecc71;box-shadow:0 2px 8px rgba(0,0,0,0.4);} 
.step-slider::-moz-range-track{width:22px;height:140px;border-radius:10px;background:rgba(255,255,255,0.08);} 
.step-slider::-moz-range-thumb{width:16px;height:16px;border-radius:50%;background:#2ecc71;box-shadow:0 2px 8px rgba(0,0,0,0.4);} 
.slider-col select{width:100%;padding:8px;border-radius:10px;border:1px solid var(--border);background:rgba(255,255,255,0.03);color:var(--text);} 
.card h3{margin:0 0 10px;font-weight:600;}
.title-row{display:flex;align-items:center;justify-content:space-between;gap:10px;}
.title-unit{padding:8px 10px;border-radius:10px;border:1px solid var(--border);background:rgba(255,255,255,0.06);color:var(--text);cursor:pointer;font-size:13px;-webkit-tap-highlight-color:transparent;}
.title-unit:active{transform:translateY(1px);} 
label{display:flex;flex-direction:column;gap:6px;margin-top:10px;font-size:14px;color:var(--muted);} 
.chk{flex-direction:row;align-items:center;gap:8px;margin-top:8px;}
.freq-row{display:flex;align-items:center;gap:8px;}
.freq-row input{flex:1;min-width:0;}
.range-bar{display:flex;flex-direction:column;gap:4px;width:100%;align-items:center;}
.range-step{width:100%;display:flex;justify-content:center;background:transparent;}
.range-label{font-size:12px;color:var(--muted);} 
input[type=number],select{width:100%;padding:10px;border-radius:10px;border:1px solid var(--border);background:rgba(255,255,255,0.03);color:var(--text);font-size:15px;transition:border .2s,background .2s;} 
input[type=number]:focus,select:focus{outline:none;border:1px solid var(--accent);background:rgba(62,200,255,0.06);} 
button.primary{background:linear-gradient(135deg,var(--accent),#4dd2ff);color:#07101f;font-weight:600;border:none;border-radius:12px;padding:12px 18px;cursor:pointer;box-shadow:0 10px 25px rgba(62,200,255,0.35);transition:transform .1s ease,box-shadow .2s ease;} 
button.primary:active{transform:translateY(1px);box-shadow:0 4px 12px rgba(62,200,255,0.25);} 
.grid{display:grid;grid-template-columns:repeat(auto-fit,minmax(135px,1fr));gap:14px;}
.bar input[type=number]{padding:6px;}
.note{color:var(--muted);font-size:13px;}
@media(max-width:600px){body{padding:16px;}.status{width:100%;justify-content:flex-start;margin-left:0;}}
::-webkit-scrollbar{width:10px;height:10px;}::-webkit-scrollbar-track{background:rgba(255,255,255,0.03);}::-webkit-scrollbar-thumb{background:rgba(62,200,255,0.35);border-radius:8px;}
</style></head><body>
<div class="layout">
<div class="bar"><div class="pill" id="pill">ESP32C3 + SI5351</div>
<button type="button" id="syncbtn" class="toggle-btn">x3 F</button>
<div class="sync-gap"></div>
<input type="checkbox" id="sync" style="display:none;">
<label style="width:110px;" id="corr-label">Коррекция ppm<input type="number" id="corr" step="1" min="-10000" max="10000" value="0"></label>
<select id="lang" class="lang">
  <option value="de">DE</option>
  <option value="ru">RU</option>
  <option value="uk">UA</option>
  <option value="en">EN</option>
</select>
<div class="status"><span id="dot" class="dot ok"></span><span id="status">Готов</span></div></div>
<div id="channels" class="grid"></div>
<div class="bar" style="justify-content:space-between;"><div class="note">Настройки применяются сразу. Кнопка "Сохранить" пишет во флеш, "Сбросить" берёт текущее состояние.</div><div style="display:flex;gap:10px;"><button class="primary" style="background:rgba(255,255,255,0.08);color:var(--text);box-shadow:none;border:1px solid var(--border);" onclick="resetState()">Сбросить</button><button class="primary" onclick="savePersist()">Сохранить</button></div></div>
</div>

<script>
const drives=[2,4,6,8];const pll=["PLLA","PLLB"];const disableStates=["LOW","HIGH","HIGH_Z","NEVER"];let state;
const stepOptions=[1,10,100,1000,10000,100000,1000000];let steps=[1000,1000,1000];
const MIN_HZ=10000,MAX_HZ=160000000;
const CORR_MIN=-10000,CORR_MAX=10000; // ppm UI range
let lang='de';
const dict={
  de:{title:'ESP32C3 + SI5351',corr:'Korrektur ppm',syncBtn:'x3 F',statusReady:'Bereit',statusFail:'Keine Verbindung SI5351',noData:'Keine Daten',note:'Einstellungen greifen sofort. "Speichern" schreibt ins Flash, "Reset" nimmt aktuellen Zustand.',reset:'Zurücksetzen',save:'Speichern',ch:'Kanal',freq:'Frequenz',power:'Leistung',phase:'Phase (0-360)',pll:'PLL',disable:'Disable state',invert:'Invertieren',intMode:'Integer-Modus',enable:'Aktivieren',stepLabel:'Schritt',syncLabel:'x3 F'},
  ru:{title:'ESP32C3 + SI5351',corr:'Коррекция ppm',syncBtn:'x3 F',statusReady:'Готов',statusFail:'Нет связи SI5351',noData:'Нет данных',note:'Настройки применяются сразу. "Сохранить" пишет во флеш, "Сбросить" берёт текущее состояние.',reset:'Сбросить',save:'Сохранить',ch:'Канал',freq:'Частота',power:'Мощность',phase:'Фаза (0-360)',pll:'PLL',disable:'Disable state',invert:'Инвертировать',intMode:'Целочисленный режим',enable:'Включить',stepLabel:'Шаг',syncLabel:'x3 F'},
  uk:{title:'ESP32C3 + SI5351',corr:'Корекція ppm',syncBtn:'x3 F',statusReady:'Готово',statusFail:'Нема зв’язку SI5351',noData:'Немає даних',note:'Налаштування застосовуються одразу. "Зберегти" пише у флеш, "Скинути" бере поточний стан.',reset:'Скинути',save:'Зберегти',ch:'Канал',freq:'Частота',power:'Потужність',phase:'Фаза (0-360)',pll:'PLL',disable:'Disable state',invert:'Інвертувати',intMode:'Цілочисловий режим',enable:'Увімкнути',stepLabel:'Крок',syncLabel:'x3 F'},
  en:{title:'ESP32C3 + SI5351',corr:'Correction ppm',syncBtn:'x3 F',statusReady:'Ready',statusFail:'No SI5351 link',noData:'No data',note:'Settings apply immediately. "Save" writes to flash, "Reset" takes current state.',reset:'Reset',save:'Save',ch:'Channel',freq:'Frequency',power:'Drive',phase:'Phase (0-360)',pll:'PLL',disable:'Disable state',invert:'Invert',intMode:'Integer mode',enable:'Enable',stepLabel:'Step',syncLabel:'x3 F'}
};
function t(k){return (dict[lang]&&dict[lang][k])||k;}
const unitMult=[1,1e3,1e6];let unitModes=[0,0,0];
const units={de:["Hz","kHz","MHz"],ru:["Гц","кГц","МГц"],uk:["Гц","кГц","МГц"],en:["Hz","kHz","MHz"]};
const disableTexts={
  de:["Niedrig","Hoch","Hoher Z","Nie"],
  ru:["Низкий","Высокий","Выс. импеданс","Никогда"],
  uk:["Низький","Високий","Вис. імпеданс","Ніколи"],
  en:["LOW","HIGH","HIGH Z","NEVER"]
};
function unitLabel(idx){return (units[lang]&&units[lang][idx])||units['en'][idx]||'';}
function dsLabel(idx){return (disableTexts[lang]&&disableTexts[lang][idx])||disableStates[idx]||'';}
function formatDisplay(hz,mode){const v=hz/unitMult[mode];return mode===0?Math.round(v):Number(v.toFixed(3));}
function snapHz(hz,i){const step=steps[i]||1;let v=Math.round((hz||0)/step)*step;if(v<MIN_HZ)v=MIN_HZ;if(v>MAX_HZ)v=MAX_HZ;return v;}
function formatStep(val){return val>=1000000?(val/1000000+'M'):(val>=1000?(val/1000+'k'):val);} 
function updateUnitValue(idx){const s=document.getElementById('s'+idx);const uv=document.getElementById('uv'+idx);if(!s||!uv)return;const hz=Number(s.value);uv.textContent=formatDisplay(hz,unitModes[idx])+' '+unitLabel(unitModes[idx]);}
function stepIndex(val){const idx=stepOptions.indexOf(val);return idx>=0?idx:3;}
function updateStepUI(i){const rb=document.getElementById('r'+i);const rl=document.getElementById('rl'+i);if(rb)rb.value=stepIndex(steps[i]);if(rl)rl.textContent=t('stepLabel')+' '+formatStep(steps[i]);const fi=document.getElementById('f'+i);const sl=document.getElementById('s'+i);if(fi)fi.step=steps[i];if(sl)sl.step=steps[i];const hz=sl?snapHz(Number(sl.value),i):null;if(hz!==null){if(sl&&sl.value!=hz)sl.value=hz;if(fi)fi.value=formatDisplay(hz,unitModes[i]);}}
function mkChannel(idx,ch){return '<div class="card">'+
  '<div class="card-body">'+
    '<div class="title-row"><h3>'+t('ch')+' '+idx+'</h3><button type="button" class="title-unit" id="ut'+idx+'">'+unitLabel(unitModes[idx])+'</button></div>'+
    '<label>'+t('freq')+': <div class="freq-row"><input type="number" id="f'+idx+'" min="10000" max="160000000" step="'+steps[idx]+'" value="'+formatDisplay(ch.freq,unitModes[idx])+'" '+(idx>0&&state&&state.sync_mode?'disabled':'')+'></div></label>'+
    '<label>'+t('power')+': <select id="d'+idx+'">'+drives.map(v=>'<option '+(v==ch.drive?'selected':'')+'>'+v+'</option>').join('')+'</select></label>'+
    '<label>'+t('phase')+': <input type="number" id="p'+idx+'" min="0" max="360" step="1" value="'+Math.round(ch.phase*360/128)+'"></label>'+
    '<label>'+t('pll')+': <select id="pl'+idx+'">'+pll.map((v,i)=>'<option value="'+i+'" '+(i==ch.pll?'selected':'')+'>'+v+'</option>').join('')+'</select></label>'+
    '<label>'+t('disable')+': <select id="ds'+idx+'">'+disableStates.map((v,i)=>'<option value="'+i+'" '+(i==ch.disable_state?'selected':'')+'>'+dsLabel(i)+'</option>').join('')+'</select></label>'+
    '<label class="chk"><input type="checkbox" id="iv'+idx+'" '+(ch.invert?'checked':'')+'> '+t('invert')+'</label>'+
    '<label class="chk"><input type="checkbox" id="im'+idx+'" '+(ch.int_mode?'checked':'')+'> '+t('intMode')+'</label>'+
    '<label class="chk"><input type="checkbox" id="e'+idx+'" '+(ch.enabled?'checked':'')+'> '+t('enable')+'</label>'+
  '</div>'+
  '<div class="slider-col">'+
    '<input class="freq-slider" type="range" id="s'+idx+'" min="10000" max="160000000" step="'+steps[idx]+'" value="'+ch.freq+'" '+(idx>0&&state&&state.sync_mode?'disabled':'')+'> ' +
    '<div class="range-bar"><div class="range-step"><input class="step-slider" type="range" id="r'+idx+'" min="0" max="6" step="1" value="'+stepIndex(steps[idx])+'"></div><div class="range-label" id="rl'+idx+'">'+t('stepLabel')+' '+formatStep(steps[idx])+'</div></div>'+
  '</div>'+
'</div>'; }
function setStatus(ok){const dot=document.getElementById('dot');const st=document.getElementById('status');if(!dot||!st)return;if(ok){dot.style.background='#3ec8ff';dot.style.boxShadow='0 0 0 6px rgba(62,200,255,0.15)';st.textContent=t('statusReady');}else{dot.style.background='#ff5c5c';dot.style.boxShadow='0 0 0 6px rgba(255,92,92,0.15)';st.textContent=t('statusFail');}}
function updateSyncFields(){const sync=document.getElementById('sync').checked;for(let i=1;i<3;i++){const f=document.getElementById('f'+i);const s=document.getElementById('s'+i);if(f){f.disabled=sync;if(sync){f.value=document.getElementById('f0').value;}}if(s){s.disabled=sync;if(sync){s.value=document.getElementById('f0').value;}}}}
function attachHandlers(){const syncEl=document.getElementById('sync');const syncBtn=document.getElementById('syncbtn');const langSel=document.getElementById('lang');if(langSel&&!langSel.dataset.bound){langSel.addEventListener('change',()=>{lang=langSel.value||'de';render();});langSel.value=lang;langSel.dataset.bound='1';}if(syncBtn&&!syncBtn.dataset.bound){syncBtn.addEventListener('click',()=>{if(syncEl){syncEl.checked=!syncEl.checked;}syncBtn.classList.toggle('active',syncEl&&syncEl.checked);updateSyncFields();applyLive();});syncBtn.dataset.bound='1';}if(syncEl&&!syncEl.dataset.bound){syncEl.addEventListener('change',()=>{if(syncBtn)syncBtn.classList.toggle('active',syncEl.checked);updateSyncFields();applyLive();});syncEl.dataset.bound='1';}const f0=document.getElementById('f0');if(f0&&!f0.dataset.bound){f0.addEventListener('input',function(){if(document.getElementById('sync').checked){for(let i=1;i<3;i++){const f=document.getElementById('f'+i);const s=document.getElementById('s'+i);const hz=snapHz(Number(this.value)*unitMult[unitModes[i]],i);if(f){f.value=formatDisplay(hz,unitModes[i]);}if(s){s.value=hz;}}}applyLive();});f0.dataset.bound='1';}for(let i=0;i<3;i++){['d','p','pl','ds','iv','im','e'].forEach(prefix=>{const el=document.getElementById(prefix+ i);if(el&&!el.dataset.bound){el.addEventListener('change',applyLive);el.dataset.bound='1';}});const fi=document.getElementById('f'+i);if(fi&&!fi.dataset.bound){fi.addEventListener('input',function(){const hz=snapHz(Number(this.value)*unitMult[unitModes[i]],i);const s=document.getElementById('s'+i);if(s){s.value=hz;}this.value=formatDisplay(hz,unitModes[i]);applyLive();});fi.dataset.bound='1';}const sl=document.getElementById('s'+i);if(sl&&!sl.dataset.bound){sl.addEventListener('input',function(){const hz=snapHz(Number(this.value),i);this.value=hz;const fi=document.getElementById('f'+i);if(fi)fi.value=formatDisplay(hz,unitModes[i]);applyLive();});sl.dataset.bound='1';}const rb=document.getElementById('r'+i);if(rb&&!rb.dataset.bound){rb.addEventListener('input',function(){const pos=Math.max(0,Math.min(6,Number(this.value)||0));steps[i]=stepOptions[pos];updateStepUI(i);});rb.dataset.bound='1';}const ut=document.getElementById('ut'+i);if(ut&&!ut.dataset.bound){ut.addEventListener('click',function(){unitModes[i]=(unitModes[i]+1)%3;ut.textContent=unitLabel(unitModes[i]);const s=document.getElementById('s'+i);const hz=s?snapHz(Number(s.value),i):0;const fi=document.getElementById('f'+i);if(fi)fi.value=formatDisplay(hz,unitModes[i]);});ut.dataset.bound='1';}updateStepUI(i);} }
function render(){if(!state||!state.channels){document.getElementById('channels').innerHTML='<div class="card">'+t('noData')+'</div>';setStatus(false);return;}document.getElementById('sync').checked=state.sync_mode;const sb=document.getElementById('syncbtn');if(sb){sb.textContent=t('syncBtn');sb.classList.toggle('active',state.sync_mode);}const pill=document.getElementById('pill');if(pill)pill.textContent=t('title');const clabel=document.getElementById('corr-label');if(clabel){clabel.childNodes[0].textContent=t('corr')+' ';}document.getElementById('corr').value=(state.correction||0)/1000;document.getElementById('channels').innerHTML=state.channels.map((c,i)=>mkChannel(i,c)).join('');attachHandlers();updateSyncFields();setStatus(state.si_ready);const note=document.querySelector('.note');if(note)note.textContent=t('note');const btnReset=document.querySelector('button[onclick="resetState()"]');if(btnReset)btnReset.textContent=t('reset');const btnSave=document.querySelector('button[onclick="savePersist()"]');if(btnSave)btnSave.textContent=t('save');const utButtons=document.querySelectorAll('.title-unit');utButtons.forEach((b,idx)=>{b.textContent=unitLabel(unitModes[idx]||0);});const rangeLabels=document.querySelectorAll('.range-label');rangeLabels.forEach((el)=>{const txt=el.textContent.split(' ');if(txt.length>1){el.textContent=t('stepLabel')+' '+txt[txt.length-1];}});}
function buildPayload(){const payload={sync_mode:document.getElementById('sync').checked,channels:[]};let corrPpm=Number(document.getElementById('corr').value);if(isNaN(corrPpm))corrPpm=0;if(corrPpm<CORR_MIN)corrPpm=CORR_MIN;if(corrPpm>CORR_MAX)corrPpm=CORR_MAX;payload.correction=corrPpm*1000;for(let i=0;i<3;i++){const phaseDeg=Number(document.getElementById('p'+i).value);let phaseSteps=Math.round((phaseDeg/360)*128);if(phaseSteps<0)phaseSteps=0;if(phaseSteps>127)phaseSteps=127;const hz=Number(document.getElementById('s'+i).value);payload.channels.push({freq:hz,drive:Number(document.getElementById('d'+i).value),phase:phaseSteps,pll:Number(document.getElementById('pl'+i).value),disable_state:Number(document.getElementById('ds'+i).value),invert:document.getElementById('iv'+i).checked,int_mode:document.getElementById('im'+i).checked,enabled:document.getElementById('e'+i).checked});}return payload;}
let applyTimer=null;
async function applyLive(){clearTimeout(applyTimer);applyTimer=setTimeout(async()=>{const payload=buildPayload();try{const res=await fetch('/api/state?apply=1',{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify(payload)});state=await res.json();setStatus(state.si_ready);}catch(e){console.log('Apply error',e);}},200);}
function resetState(){
  state={sync_mode:false,correction:0,si_ready:true,channels:[
    {freq:10000,drive:8,phase:0,pll:0,disable_state:0,invert:false,int_mode:false,enabled:true},
    {freq:10000,drive:8,phase:0,pll:0,disable_state:0,invert:false,int_mode:false,enabled:true},
    {freq:10000,drive:8,phase:0,pll:0,disable_state:0,invert:false,int_mode:false,enabled:true}
  ]};
  render();
  applyLive();
}
async function savePersist(){const payload=buildPayload();try{const res=await fetch('/api/state',{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify(payload)});state=await res.json();render();}catch(e){alert('Ошибка сохранения: '+e);}}
async function load(){try{const res=await fetch('/api/state');state=await res.json();render();}catch(e){document.getElementById('channels').innerHTML='<div class="card">Не удалось загрузить состояние</div>';}}
window.onload=function(){load();};
</script></body></html>
)rawliteral";

void handleRoot() {
  server.sendHeader("Cache-Control", "no-cache, no-store, must-revalidate");
  server.sendHeader("Pragma", "no-cache");
  server.sendHeader("Expires", "0");
  server.setContentLength(CONTENT_LENGTH_UNKNOWN);
  server.send(200, "text/html", "");
  server.sendContent(FPSTR(PAGE));
  server.sendContent("");
}

void setupAP() {
  WiFi.mode(WIFI_AP);
  WiFi.setSleep(false);
  WiFi.softAPConfig(AP_IP, AP_GW, AP_MASK);
  WiFi.softAP(AP_SSID, AP_PASS);
}

void setupWeb() {
  server.on("/", HTTP_GET, handleRoot);
  server.on("/api/state", HTTP_GET, handleGetState);
  server.on("/api/state", HTTP_POST, handlePostState);
  server.begin();
}

void setup() {
  Serial.begin(115200);
  delay(200);
  Wire.begin(I2C_SDA, I2C_SCL, 100000);
  Wire.setPins(I2C_SDA, I2C_SCL);

  si5351_addr = scanSi5351();
  if (si5351_addr != 0) {
    si_found = true;
    Serial.printf("SI5351 found at 0x%02X\n", si5351_addr);
  } else {
    Serial.println("SI5351 not found on I2C (0x01-0x7E)");
  }
  // Initialize SI5351 (try 25 MHz crystal, then 27 MHz fallback)
  bool init_ok = false;
  if (si_found) {
    if (si5351.init(SI5351_CRYSTAL_LOAD_8PF, 25000000UL, 0)) {
      Serial.println("SI5351 init ok (25MHz)");
      init_ok = true;
    } else if (si5351.init(SI5351_CRYSTAL_LOAD_8PF, 27000000UL, 0)) {
      Serial.println("SI5351 init ok (27MHz)");
      init_ok = true;
    }
  }

  if (init_ok) {
    si_ready = true;
    si5351.output_enable(SI5351_CLK0, true);
    si5351.output_enable(SI5351_CLK1, true);
    si5351.output_enable(SI5351_CLK2, true);
  } else {
    Serial.println("SI5351 init failed");
    if (si_found) {
      si5351.update_status();
      Serial.printf("Status: SYS_INIT=%u LOL_A=%u LOL_B=%u LOS=%u REVID=%u\n",
        si5351.dev_status.SYS_INIT,
        si5351.dev_status.LOL_A,
        si5351.dev_status.LOL_B,
        si5351.dev_status.LOS,
        si5351.dev_status.REVID);
    }
  }

  loadConfig();
  setupAP();
  setupWeb();
  si5351.set_correction(cfg.correction, SI5351_PLL_INPUT_XO);
  applyAll();
  Serial.printf("AP: %s  IP: %s\n", AP_SSID, AP_IP.toString().c_str());
}

void loop() {
  server.handleClient();
}
