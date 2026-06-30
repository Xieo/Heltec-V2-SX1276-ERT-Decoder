

#define FW_VERSION "Heltec-V2-SX1276-ERT-1.0.0"

#include <Arduino.h>
#include <Preferences.h>
#include <WiFi.h>
#include <ESPAsyncWebServer.h>
#include <ArduinoOTA.h>
#include <SPI.h>
#include <rtl_433_ESP.h>
#include <WiFiClient.h>
#include <PubSubClient.h>
#include <SSD1306Wire.h>
#include "Credentials.h"

extern SX1276 radio;

#define PIN_GDO0   34
#define PIN_DIO0   26
#define PIN_DIO1   35
#define VEXT_CTRL  21
#define OLED_SDA    4
#define OLED_SCL   15
#define OLED_RST   16

#define PULSE_SAMPLES 2000
static volatile bool     pulseMonEnabled = false;
static volatile uint16_t pulseHead = 0;
static uint8_t           pulseBuf[PULSE_SAMPLES];

static float    rxFreq      = 913.95f;
static int      rssiGate    = -95;
static uint8_t  rxBwIdx     = 0;

static const float   BW_KHZ[7] = {250.0f, 200.0f, 166.7f, 125.0f, 100.0f, 83.3f, 62.5f};
static const uint8_t BW_REGS[7] = {0x01,   0x09,   0x11,   0x02,   0x0A,   0x12,  0x03};

static uint8_t  ookPeak        = 0x0B;
static uint8_t  ookFix         = 0x0C;
static uint8_t  ookAvg         = 0x12;
static uint8_t  rssiThreshReg  = 0xF0;
static int      libRssiThresh  = 3;
static uint32_t goodDecodes = 0;
static uint32_t rawCallbacks = 0;
static uint32_t rejectedModel = 0;
static uint32_t rejectedRssi  = 0;
static uint32_t rejectedPlaus = 0;
static uint32_t lastStatusMs = 0;
static volatile uint32_t guiPauseUntil = 0;
static bool     radioOk      = false;
static uint8_t  radioVer     = 0;
static char     radioErrMsg[80] = "not initialized";

static SSD1306Wire display(0x3c, OLED_SDA, OLED_SCL);
static uint32_t lastOledMs  = 0;
static char     oledLastId[16] = "--";
static int      oledLastRssi   = 0;

#define MAX_PKT 20
struct Pkt { char json[320]; uint32_t ts; };
static Pkt  pkts[MAX_PKT];
static int  pktHead  = 0;
static int  pktCount = 0;
static SemaphoreHandle_t pktMutex;

static WiFiClient   wifiClient;
static PubSubClient mqtt(wifiClient);

static uint32_t mqttIds[16];
static char     mqttLastJson[16][320];
static int      mqttIdCount = 0;
static SemaphoreHandle_t mqttMutex;

#define MQTT_PUB_QUEUE 4
struct MqttPubMsg { char topic[80]; char payload[320]; };
static MqttPubMsg        mqttPubQ[MQTT_PUB_QUEUE];
static volatile int      mqttPubHead = 0;
static volatile int      mqttPubTail = 0;
static SemaphoreHandle_t mqttPubMutex;
static SemaphoreHandle_t nvsMutex;
static bool              mqttDiscoveryDone  = false;
static volatile bool     mqttNeedsDiscovery = false;
static volatile bool     mqttJsonDirty      = false;
static char              espName[32]        = "Heltec V2";
static char              deviceNames[16][32];
static char              deviceUnits[16][16];
static float             deviceScales[16];
static uint32_t          lastSeenMs[16];
static time_t            lastSeenTime[16];
static volatile bool     lastSeenDirty[16];
static uint32_t          deviceDecodeCounts[16];
static volatile bool     countDirty = false;

#define MAX_SEEN 10
static uint32_t      seenIds[MAX_SEEN];
static char          seenLastJson[MAX_SEEN][200];
static time_t        seenLastTime[MAX_SEEN];
static uint32_t      seenDecodeCount[MAX_SEEN];
static int           seenIdCount = 0;
static volatile bool seenDirty   = false;
static bool          seenSlotDirty[MAX_SEEN] = {};

static Preferences prefs;

static bool mqttHasId(uint32_t id) {
    for (int i = 0; i < mqttIdCount; i++) if (mqttIds[i] == id) return true;
    return false;
}
static void nvsSave() {
    xSemaphoreTake(nvsMutex, portMAX_DELAY);
    prefs.begin("ert", false);
    prefs.putInt("count", mqttIdCount);
    for (int i = 0; i < mqttIdCount; i++) {
        char k[8]; snprintf(k, sizeof(k), "id%d", i);
        prefs.putULong(k, mqttIds[i]);
        char dn[9]; snprintf(dn, sizeof(dn), "dname%d", i);
        prefs.putString(dn, deviceNames[i]);
        char du[8]; snprintf(du, sizeof(du), "unit%d", i);
        prefs.putString(du, deviceUnits[i]);
        char ds[9]; snprintf(ds, sizeof(ds), "scale%d", i);
        prefs.putFloat(ds, deviceScales[i]);
    }
    prefs.putString("espname", espName);
    prefs.putFloat("freq", rxFreq);
    prefs.putUChar("bw", rxBwIdx);
    prefs.putUChar("ook1", ookPeak);
    prefs.putUChar("ook2", ookFix);
    prefs.putUChar("ook3", ookAvg);
    prefs.putUChar("rth",  rssiThreshReg);
    prefs.end();
    xSemaphoreGive(nvsMutex);
}
static void nvsLoad() {

    prefs.begin("ert", false);
    if (prefs.getUChar("cfgv", 0) < 32) {
        prefs.remove("ook1");
        prefs.remove("ook2");
        prefs.remove("ook3");
        prefs.remove("rth");
        prefs.remove("bw");
        prefs.putUChar("cfgv", 32);
        printf("[NVS] OOK + BW config wiped (v3.1 first boot)\n");
    }
    if (prefs.getUChar("cfgv", 0) < 33) {

        for (int i = 0; i < 16; i++) { char k[8]; snprintf(k, sizeof(k), "json%d", i); prefs.remove(k); }
        prefs.putUChar("cfgv", 33);
        printf("[NVS] freed last-decode JSON blobs (fixes name/device/ESP-name persistence)\n");
    }
    printf("[NVS] free entries: %u\n", (unsigned)prefs.freeEntries());
    prefs.end();
    prefs.begin("ert", true);
    mqttIdCount = prefs.getInt("count", 0);
    if (mqttIdCount > 16) mqttIdCount = 0;
    for (int i = 0; i < mqttIdCount; i++) {
        char k[8]; snprintf(k, sizeof(k), "id%d", i);
        mqttIds[i] = (uint32_t)prefs.getULong(k, 0);
        char kj[8]; snprintf(kj, sizeof(kj), "json%d", i);
        String js = prefs.getString(kj, "");
        strncpy(mqttLastJson[i], js.c_str(), sizeof(mqttLastJson[i]) - 1);
        mqttLastJson[i][sizeof(mqttLastJson[i]) - 1] = '\0';
        char dn[9]; snprintf(dn, sizeof(dn), "dname%d", i);
        String nm = prefs.getString(dn, "");
        strncpy(deviceNames[i], nm.c_str(), sizeof(deviceNames[i]) - 1);
        deviceNames[i][sizeof(deviceNames[i]) - 1] = '\0';
        char du[8]; snprintf(du, sizeof(du), "unit%d", i);
        String un = prefs.getString(du, "");
        strncpy(deviceUnits[i], un.c_str(), sizeof(deviceUnits[i]) - 1);
        deviceUnits[i][sizeof(deviceUnits[i]) - 1] = '\0';
        char ds[9]; snprintf(ds, sizeof(ds), "scale%d", i);
        deviceScales[i] = prefs.getFloat(ds, 1.0f);
        char lt[10]; snprintf(lt, sizeof(lt), "ltime%d", i);
        lastSeenTime[i] = (time_t)prefs.getULong(lt, 0);
        char ck[8]; snprintf(ck, sizeof(ck), "cnt%d", i);
        deviceDecodeCounts[i] = prefs.getULong(ck, 0);
    }
    String en = prefs.getString("espname", "Heltec V2");
    strncpy(espName, en.c_str(), sizeof(espName) - 1); espName[sizeof(espName) - 1] = '\0';
    rxFreq  = prefs.getFloat("freq",   913.95f);
    rxBwIdx = prefs.getUChar("bw",     0);  if (rxBwIdx >= 7) rxBwIdx = 0;

    ookPeak       = prefs.getUChar("ook1", 0x28);
    ookFix        = prefs.getUChar("ook2", 0x0C);
    ookAvg        = prefs.getUChar("ook3", 0x12);
    rssiThreshReg = prefs.getUChar("rth",  0xF0);
    prefs.end();
    printf("[NVS] loaded %d device(s), esp='%s', freq=%.3f\n", mqttIdCount, espName, rxFreq);
}
static void nvsSaveLastJson() {

    xSemaphoreTake(nvsMutex, portMAX_DELAY);
    prefs.begin("ert", false);
    for (int i = 0; i < mqttIdCount && i < 16; i++) {
        char k[10]; snprintf(k, sizeof(k), "json%d", i);
        if (mqttLastJson[i][0]) prefs.putString(k, mqttLastJson[i]);
    }
    prefs.end();
    xSemaphoreGive(nvsMutex);
    mqttJsonDirty = false;
}
static void nvsSaveLastSeen() {
    static time_t   snapTime[16];
    static uint32_t snapCnt[16];
    static bool     snapTDirty[16];
    bool snapCDirty; int cnt;
    xSemaphoreTake(mqttMutex, portMAX_DELAY);
    bool anyDirty = countDirty;
    if (!anyDirty) for (int i = 0; i < mqttIdCount; i++) if (lastSeenDirty[i]) { anyDirty = true; break; }
    if (!anyDirty) { xSemaphoreGive(mqttMutex); return; }
    cnt = mqttIdCount;
    for (int i = 0; i < cnt; i++) { snapTDirty[i] = lastSeenDirty[i]; lastSeenDirty[i] = false; snapTime[i] = lastSeenTime[i]; }
    snapCDirty = countDirty; countDirty = false;
    for (int i = 0; i < cnt; i++) snapCnt[i] = deviceDecodeCounts[i];
    xSemaphoreGive(mqttMutex);
    xSemaphoreTake(nvsMutex, portMAX_DELAY);
    prefs.begin("ert", false);
    for (int i = 0; i < cnt; i++) {
        if (snapTDirty[i]) { char lt[10]; snprintf(lt, sizeof(lt), "ltime%d", i); prefs.putULong(lt, (unsigned long)snapTime[i]); }
    }
    if (snapCDirty) {
        for (int i = 0; i < cnt; i++) { char ck[8]; snprintf(ck, sizeof(ck), "cnt%d", i); prefs.putULong(ck, snapCnt[i]); }
    }
    prefs.end();
    xSemaphoreGive(nvsMutex);
}
static void nvsLoadSeen() {

    prefs.begin("ertseen", false);
    if (prefs.getUChar("v", 0) < 27) {
        prefs.clear();
        prefs.putUChar("v", 27);
        prefs.end();
        seenIdCount = 0;
        printf("[NVS] ertseen wiped (v2.7 first boot)\n");
        return;
    }
    prefs.end();
    prefs.begin("ertseen", true);
    seenIdCount = prefs.getInt("scount", 0);
    if (seenIdCount > MAX_SEEN) seenIdCount = MAX_SEEN;
    for (int i = 0; i < seenIdCount; i++) {
        char k[8]; snprintf(k, sizeof(k), "sid%d", i);
        seenIds[i] = (uint32_t)prefs.getULong(k, 0);
        char jk[12], tk[12], ck[12];
        snprintf(jk, sizeof(jk), "sj%08lX", (unsigned long)seenIds[i]);
        snprintf(tk, sizeof(tk), "st%08lX", (unsigned long)seenIds[i]);
        snprintf(ck, sizeof(ck), "sc%08lX", (unsigned long)seenIds[i]);
        String js = prefs.getString(jk, "");
        strncpy(seenLastJson[i], js.c_str(), 199); seenLastJson[i][199] = '\0';
        seenLastTime[i] = (time_t)prefs.getULong(tk, 0);
        seenDecodeCount[i] = prefs.getULong(ck, 0);
    }
    prefs.end();
}
static void nvsSaveSeen() {
    static uint32_t snapIds[MAX_SEEN];
    int cnt;
    if (xSemaphoreTake(mqttMutex, pdMS_TO_TICKS(10)) != pdTRUE) return;
    cnt = seenIdCount;
    memcpy(snapIds, seenIds, cnt * sizeof(uint32_t));
    seenDirty = false;
    xSemaphoreGive(mqttMutex);
    xSemaphoreTake(nvsMutex, portMAX_DELAY);
    prefs.begin("ertseen", false);
    prefs.putInt("scount", cnt);
    for (int i = 0; i < cnt; i++) {
        char k[8]; snprintf(k, sizeof(k), "sid%d", i);
        prefs.putULong(k, snapIds[i]);
    }
    prefs.end();
    xSemaphoreGive(nvsMutex);
}
static void nvsSaveSeenDirty() {
    static uint32_t snapIds[MAX_SEEN];
    static char     snapJson[MAX_SEEN][200];
    static time_t   snapTime[MAX_SEEN];
    static uint32_t snapCnt[MAX_SEEN];
    static bool     snapDirty[MAX_SEEN];
    int cnt;
    if (xSemaphoreTake(mqttMutex, pdMS_TO_TICKS(10)) != pdTRUE) return;
    cnt = seenIdCount;
    memcpy(snapIds,  seenIds,        cnt * sizeof(uint32_t));
    for (int i = 0; i < cnt; i++) memcpy(snapJson[i], seenLastJson[i], 200);
    memcpy(snapTime, seenLastTime,   cnt * sizeof(time_t));
    memcpy(snapCnt,  seenDecodeCount,cnt * sizeof(uint32_t));
    memcpy(snapDirty,seenSlotDirty,  cnt * sizeof(bool));
    memset(seenSlotDirty, 0, sizeof(seenSlotDirty));
    xSemaphoreGive(mqttMutex);
    bool any = false;
    for (int i = 0; i < cnt; i++) if (snapDirty[i]) { any = true; break; }
    if (!any) return;
    xSemaphoreTake(nvsMutex, portMAX_DELAY);
    prefs.begin("ertseen", false);
    for (int i = 0; i < cnt; i++) {
        if (!snapDirty[i]) continue;
        char jk[12], tk[12], ck[12];
        snprintf(jk, sizeof(jk), "sj%08lX", (unsigned long)snapIds[i]);
        snprintf(tk, sizeof(tk), "st%08lX", (unsigned long)snapIds[i]);
        snprintf(ck, sizeof(ck), "sc%08lX", (unsigned long)snapIds[i]);
        prefs.putString(jk, snapJson[i]);
        prefs.putULong(tk, (unsigned long)snapTime[i]);
        prefs.putULong(ck, snapCnt[i]);
    }
    prefs.end();
    xSemaphoreGive(nvsMutex);
}
static void seenAddId(uint32_t id, const char* json = nullptr, time_t ts = 0) {
    if (mqttHasId(id)) return;
    for (int i = 0; i < seenIdCount; i++) {
        if (seenIds[i] == id) {
            uint32_t tid = seenIds[i];
            char tj[200]; memcpy(tj, seenLastJson[i], 200);
            time_t tt = seenLastTime[i];
            uint32_t tc = seenDecodeCount[i];
            for (int j = i; j > 0; j--) {
                seenIds[j] = seenIds[j-1];
                memcpy(seenLastJson[j], seenLastJson[j-1], 200);
                seenLastTime[j] = seenLastTime[j-1];
                seenDecodeCount[j] = seenDecodeCount[j-1];
            }
            seenIds[0] = tid;
            memcpy(seenLastJson[0], tj, 200);
            seenLastTime[0] = tt;
            seenDecodeCount[0] = tc;
            if (json) { strncpy(seenLastJson[0], json, 199); seenLastJson[0][199] = '\0'; seenDecodeCount[0]++; seenSlotDirty[0] = true; }
            if (ts > 0) seenLastTime[0] = ts;
            return;
        }
    }
    int end = (seenIdCount < MAX_SEEN) ? seenIdCount : MAX_SEEN - 1;
    for (int i = end; i > 0; i--) {
        seenIds[i] = seenIds[i-1];
        memcpy(seenLastJson[i], seenLastJson[i-1], 200);
        seenLastTime[i] = seenLastTime[i-1];
        seenDecodeCount[i] = seenDecodeCount[i-1];
    }
    seenIds[0] = id;
    seenLastJson[0][0] = '\0';
    if (json) { strncpy(seenLastJson[0], json, 199); seenLastJson[0][199] = '\0'; seenSlotDirty[0] = true; }
    seenLastTime[0] = ts;
    seenDecodeCount[0] = (json ? 1 : 0);
    if (seenIdCount < MAX_SEEN) seenIdCount++;
}
static void seenRemoveId(uint32_t id) {
    for (int i = 0; i < seenIdCount; i++) {
        if (seenIds[i] == id) {
            seenIdCount--;
            for (int j = i; j < seenIdCount; j++) {
                seenIds[j] = seenIds[j+1];
                memcpy(seenLastJson[j], seenLastJson[j+1], 200);
                seenLastTime[j] = seenLastTime[j+1];
                seenDecodeCount[j] = seenDecodeCount[j+1];
            }
            seenDirty = true; return;
        }
    }
}
static void mqttAddId(uint32_t id) {
    if (!mqttHasId(id) && mqttIdCount < 16) {
        int slot = mqttIdCount;
        mqttLastJson[slot][0] = '\0';
        lastSeenTime[slot]       = 0;
        deviceDecodeCounts[slot] = 0;
        for (int i = 0; i < seenIdCount; i++) {
            if (seenIds[i] == id) {
                if (seenLastJson[i][0]) { strncpy(mqttLastJson[slot], seenLastJson[i], sizeof(mqttLastJson[slot])-1); mqttLastJson[slot][sizeof(mqttLastJson[slot])-1]='\0'; }
                lastSeenTime[slot]       = seenLastTime[i];
                deviceDecodeCounts[slot] = seenDecodeCount[i];
                break;
            }
        }
        seenRemoveId(id);
        deviceNames[slot][0] = '\0';
        deviceUnits[slot][0] = '\0';
        deviceScales[slot]   = 1.0f;
        mqttIds[slot]        = id;
        mqttIdCount++;
        nvsSave();
        if (mqttLastJson[slot][0]) { mqttJsonDirty = true; lastSeenDirty[slot] = true; countDirty = true; }
        mqttNeedsDiscovery = true;
    }
}
static void slugify(const char* src, char* dst, size_t dstLen);
static void getEspLabel(char* out, size_t n);
static void getEspSlug(char* out, size_t n);
static void mqttRemoveId(uint32_t id) {
    for (int i = 0; i < mqttIdCount; i++) {
        if (mqttIds[i] == id) {

            if (mqtt.connected()) {
                char es[34], t[128]; getEspSlug(es,sizeof(es));
                snprintf(t,sizeof(t),"homeassistant/sensor/%s/device_id_%lu/config",es,(unsigned long)id); mqtt.publish(t,"",true);
                snprintf(t,sizeof(t),"homeassistant/sensor/%s/device_id_%lu_rssi/config",es,(unsigned long)id); mqtt.publish(t,"",true);
            }
            seenAddId(id); seenDirty = true; --mqttIdCount;
            mqttIds[i] = mqttIds[mqttIdCount];
            strncpy(mqttLastJson[i], mqttLastJson[mqttIdCount], 319);
            strncpy(deviceNames[i],  deviceNames[mqttIdCount],  31);
            strncpy(deviceUnits[i],  deviceUnits[mqttIdCount],  15);
            deviceScales[i]              = deviceScales[mqttIdCount];
            mqttLastJson[mqttIdCount][0] = '\0';
            deviceNames[mqttIdCount][0]  = '\0';
            deviceUnits[mqttIdCount][0]  = '\0';
            deviceScales[mqttIdCount]    = 1.0f;
            nvsSave(); return;
        }
    }
}
static void mqttEnqueue(const char* topic, const char* payload) {
    if (xSemaphoreTake(mqttPubMutex, pdMS_TO_TICKS(5)) == pdTRUE) {
        int next = (mqttPubHead + 1) % MQTT_PUB_QUEUE;
        if (next != mqttPubTail) {
            strncpy(mqttPubQ[mqttPubHead].topic,   topic,   79);
            strncpy(mqttPubQ[mqttPubHead].payload, payload, 319);
            mqttPubHead = next;
        }
        xSemaphoreGive(mqttPubMutex);
    }
}
static void mqttDrainQueue() {
    while (mqtt.connected() && mqttPubTail != mqttPubHead) {
        char topic[80], payload[320];
        if (xSemaphoreTake(mqttPubMutex, pdMS_TO_TICKS(5)) == pdTRUE) {
            strncpy(topic,   mqttPubQ[mqttPubTail].topic,   79);
            strncpy(payload, mqttPubQ[mqttPubTail].payload, 319);
            mqttPubTail = (mqttPubTail + 1) % MQTT_PUB_QUEUE;
            xSemaphoreGive(mqttPubMutex);
            mqtt.publish(topic, payload, true);
            printf("[MQTT] â†’ %s\n", topic);
        }
    }
}
static void slugify(const char* src, char* dst, size_t dstLen) {
    size_t j = 0;
    for (size_t i = 0; src[i] && j < dstLen - 1; i++) {
        char c = (char)tolower((unsigned char)src[i]);
        if (isalnum((unsigned char)c)) { dst[j++] = c; }
        else if (j > 0 && dst[j-1] != '_') { dst[j++] = '_'; }
    }
    while (j > 0 && dst[j-1] == '_') j--;
    dst[j] = '\0';
}
static void sanitizeName(char* s) {
    for (char* p = s; *p; p++) if (*p=='"'||*p=='\\'||*p=='\n'||*p=='\r') *p=' ';
}
static void getEspLabel(char* out, size_t n) {
    if (espName[0]) strncpy(out, espName, n-1);
    else            strncpy(out, "Heltec V2", n-1);
    out[n-1] = '\0';
}
static void getDevLabel(int idx, char* out, size_t n) {
    if (deviceNames[idx][0]) strncpy(out, deviceNames[idx], n-1);
    else snprintf(out, n, "Device ID %lu", (unsigned long)mqttIds[idx]);
    out[n-1] = '\0';
}
static void getEspSlug(char* out, size_t n) {

    char lbl[32]; getEspLabel(lbl, sizeof(lbl));
    slugify(lbl, out, n);
}
static void getStateTopic(int idx, char* out, size_t n) {

    char espSlug[34];
    getEspSlug(espSlug, sizeof(espSlug));
    snprintf(out, n, "%s/device_id_%lu", espSlug, (unsigned long)mqttIds[idx]);
}
static void publishDiscovery(uint32_t id, int idx) {
    if (!mqtt.connected()) return;
    char topic[128], payload[700], stateTopic[80], espLabel[32], devLabel[48];
    char espSlug[34], devSlug[50];
    getEspLabel(espLabel, sizeof(espLabel));
    getDevLabel(idx, devLabel, sizeof(devLabel));
    getEspSlug(espSlug, sizeof(espSlug));

    snprintf(devSlug, sizeof(devSlug), "device_id_%lu", (unsigned long)id);
    snprintf(stateTopic, sizeof(stateTopic), "%s/%s", espSlug, devSlug);

    char oldTopic[96];
    snprintf(oldTopic, sizeof(oldTopic), "homeassistant/sensor/ert_scm_%lu/config",      (unsigned long)id);
    mqtt.publish(oldTopic, "", true);
    snprintf(oldTopic, sizeof(oldTopic), "homeassistant/sensor/ert_scm_%lu_rssi/config", (unsigned long)id);
    mqtt.publish(oldTopic, "", true);

    char valTmpl[96];
    float sc = deviceScales[idx];
    if (sc == 1.0f) strncpy(valTmpl, "{{value_json.consumption_data}}", sizeof(valTmpl));
    else snprintf(valTmpl, sizeof(valTmpl), "{{value_json.consumption_data|float*%g}}", sc);

    snprintf(topic, sizeof(topic), "homeassistant/sensor/%s/%s/config", espSlug, devSlug);
    const char* unit = deviceUnits[idx];
    if (unit[0]) {
        snprintf(payload, sizeof(payload),
            "{\"name\":\"%s\",\"state_topic\":\"%s\",\"value_template\":\"%s\","
            "\"unit_of_measurement\":\"%s\",\"device_class\":\"gas\","
            "\"state_class\":\"total_increasing\",\"unique_id\":\"%s_ert_scm_%lu\","
            "\"device\":{\"identifiers\":[\"esp32_%s\"],\"name\":\"%s\","
            "\"model\":\"ERT Decoder\",\"manufacturer\":\"Itron\"}}",
            devLabel, stateTopic, valTmpl, unit,
            espSlug, (unsigned long)id, espSlug, espLabel);
    } else {
        snprintf(payload, sizeof(payload),
            "{\"name\":\"%s\",\"state_topic\":\"%s\",\"value_template\":\"%s\","
            "\"device_class\":\"gas\",\"state_class\":\"total_increasing\","
            "\"unique_id\":\"%s_ert_scm_%lu\","
            "\"device\":{\"identifiers\":[\"esp32_%s\"],\"name\":\"%s\","
            "\"model\":\"ERT Decoder\",\"manufacturer\":\"Itron\"}}",
            devLabel, stateTopic, valTmpl,
            espSlug, (unsigned long)id, espSlug, espLabel);
    }
    mqtt.publish(topic, payload, true);

    snprintf(topic, sizeof(topic), "homeassistant/sensor/%s/%s_rssi/config", espSlug, devSlug);
    snprintf(payload, sizeof(payload),
        "{\"name\":\"%s RSSI\",\"state_topic\":\"%s\","
        "\"value_template\":\"{{value_json.rssi}}\","
        "\"unit_of_measurement\":\"dBm\",\"device_class\":\"signal_strength\","
        "\"entity_category\":\"diagnostic\",\"unique_id\":\"%s_ert_scm_%lu_rssi\","
        "\"device\":{\"identifiers\":[\"esp32_%s\"],\"name\":\"%s\"}}",
        devLabel, stateTopic,
        espSlug, (unsigned long)id, espSlug, espLabel);
    mqtt.publish(topic, payload, true);
    printf("[MQTT] Discovery id=%lu topic=homeassistant/sensor/%s/%s/config\n",
           (unsigned long)id, espSlug, devSlug);
}
static void mqttEnsureConnected() {
    if (!mqtt.connected() && WiFi.status() == WL_CONNECTED)
        mqtt.connect(MQTT_CLIENT_ID, MQTT_USER, MQTT_PASSWORD);
}

static AsyncWebServer  server(80);
static AsyncEventSource events("/events");

static rtl_433_ESP rf;
static char        rfMsgBuf[512];

void rtl433Callback(char* message) {

    if (strstr(message, "\"model\":\"status\"")) return;
    rawCallbacks++;

    const char* rp = strstr(message, "\"rssi\":");
    if (rp && (int)strtol(rp + 7, nullptr, 10) < rssiGate) { rejectedRssi++; return; }

    xSemaphoreTake(pktMutex, portMAX_DELAY);
    strncpy(pkts[pktHead].json, message, sizeof(pkts[pktHead].json) - 1);
    pkts[pktHead].json[sizeof(pkts[pktHead].json) - 1] = '\0';
    pkts[pktHead].ts = millis();
    pktHead = (pktHead + 1) % MAX_PKT;
    if (pktCount < MAX_PKT) pktCount++;
    goodDecodes++;
    xSemaphoreGive(pktMutex);

    {
        printf("[ERT] %s\n", message);

        const char* idPos = strstr(message, "\"id\":");
        if (idPos) {
            uint32_t pktId = (uint32_t)strtoul(idPos + 5, nullptr, 10);
            if (pktId > 0) {
                events.send(message, "decode", millis());
                snprintf(oledLastId, sizeof(oledLastId), "%lu", (unsigned long)pktId);
                const char* rssiPos = strstr(message, "\"rssi\":");
                if (rssiPos) oledLastRssi = (int)strtol(rssiPos + 7, nullptr, 10);
                char pubTopic[80]; pubTopic[0] = '\0';
                xSemaphoreTake(mqttMutex, portMAX_DELAY);
                bool selected = false;
                for (int i = 0; i < mqttIdCount; i++) {
                    if (mqttIds[i] == pktId) {
                        selected = true;
                        strncpy(mqttLastJson[i], message, 319);
                        mqttJsonDirty = true;
                        lastSeenMs[i] = millis();
                        deviceDecodeCounts[i]++;
                        countDirty = true;
                        time_t _now; time(&_now);
                        if (_now > 1000000000L) { lastSeenTime[i] = _now; lastSeenDirty[i] = true; }
                        getStateTopic(i, pubTopic, sizeof(pubTopic));
                        break;
                    }
                }
                if (!selected) {
                    time_t _t; time(&_t);
                    seenAddId(pktId, message, (_t > 1000000000L) ? _t : 0);
                    seenDirty = true;
                }
                xSemaphoreGive(mqttMutex);
                if (selected) mqttEnqueue(pubTopic, message);
            }
        }
    }
}

static void oledUpdate() {
    display.clear();
    display.setFont(ArialMT_Plain_10);

    char row1[32];
    snprintf(row1, sizeof(row1), "ERT SX1276  %s", radioOk ? "OK" : "ERR");
    display.drawString(0, 0, row1);

    char row2[32];
    if (WiFi.status() == WL_CONNECTED)
        snprintf(row2, sizeof(row2), "%s", WiFi.localIP().toString().c_str());
    else
        strncpy(row2, "WiFi connecting...", sizeof(row2));
    display.drawString(0, 16, row2);

    char row3[32];
    uint32_t upSec = millis() / 1000;
    uint32_t upMin = upSec / 60;
    snprintf(row3, sizeof(row3), "RSSI:%ddBm  Up:%lum", rtl_433_ESP::signalRssi, (unsigned long)upMin);
    display.drawString(0, 32, row3);

    char row4[32];
    snprintf(row4, sizeof(row4), "ID:%-10s Dec:%lu", oledLastId, (unsigned long)goodDecodes);
    display.drawString(0, 48, row4);

    display.display();
}

#include "WebPage.h"
#include "DashGz.h"

static const char DASH[] = R"DASH(<!doctype html><html><head><meta charset=utf-8><meta name=viewport content="width=device-width,initial-scale=1"><title>ERT Heltec V2</title><style>
*{box-sizing:border-box}body{font:13px system-ui,sans-serif;background:#0a0e14;color:#cdd9e5;margin:0;padding:12px}
h2{font:600 12px system-ui;letter-spacing:2px;color:#8fb0c8;margin:0 0 10px}.st{color:#6f8aa0;font-size:11px;margin-bottom:10px}
.c{background:#0f1620;border:1px solid #1c2733;border-radius:10px;padding:12px;margin-bottom:14px}
.row{display:flex;flex-wrap:wrap;gap:14px;align-items:flex-end}.fld{display:flex;flex-direction:column;gap:3px}label{color:#6f8aa0;font-size:10px;letter-spacing:1px}
table{width:100%;border-collapse:collapse}th{text-align:left;color:#6f8aa0;font:600 10px system-ui;letter-spacing:1px;padding:6px;border-bottom:1px solid #1c2733}
td{padding:7px 6px;border-bottom:1px solid #131c27}
input,select{background:#0a0e14;color:#cde;border:1px solid #2a3a4a;border-radius:6px;padding:6px}.n{width:130px}.u{width:50px}.s{width:55px}.f{width:78px}.gt{width:55px}
button{border-radius:6px;padding:6px 11px;cursor:pointer;font:600 12px system-ui;background:#0e2436;color:#5cf;border:1px solid #245}
.pub{background:#2a2410;color:#fc6;border-color:#b93}.add{background:#0f2a1e;color:#3f6;border-color:#2f6}.ap{background:#11202e;color:#5cf;border-color:#246}
.id{color:#3fda7a;font-weight:700}.g{color:#3fda7a}.k{color:#6f8aa0}input[type=checkbox]{width:17px;height:17px;accent-color:#3fda7a}
#msg{position:fixed;top:12px;right:16px;background:#0f2a1e;color:#3fda7a;border:1px solid #2f6;border-radius:8px;padding:9px 15px;opacity:0;transition:opacity .25s;font-weight:600;z-index:9}
</style></head><body><div id=msg></div>
<div class=st id=stat>connecting&hellip;</div>
<div class=c><h2>DEVICE &amp; RADIO SETTINGS</h2><div class=row>
<span class=fld><label>ESP NAME</label><input id=en class=n></span><button onclick=SN()>Save Name</button>
<span class=fld><label>FREQUENCY MHz</label><input id=f class=f type=number step=any min=300 max=928></span><button class=ap onclick=AF()>Apply</button>
<span class=fld><label>RX BANDWIDTH</label><select id=bw><option value=0>250 kHz<option value=1>200 kHz<option value=2>166 kHz<option value=3>125 kHz<option value=4>100 kHz<option value=5>83 kHz<option value=6>62.5 kHz</select></span><button class=ap onclick=AB()>Apply</button>
<span class=fld><label>RSSI GATE dBm</label><input id=gt class=gt type=number step=1 value=-95></span><button class=ap onclick=AG()>Apply</button>
<span class=fld><label>HW THR dB</label><input id=ht class=gt type=number step=1 value=3></span><button class=ap onclick=AH()>Apply</button>
</div></div>
<div class=c><h2>&#10003; SELECTED DEVICES &mdash; MQTT ACTIVE</h2><table><thead><tr><th>MQTT<th>Custom Name<th>Unit<th>Scale<th><th><th>Last Seen<th>Model<th>ID<th>Consumption<th>RSSI<th>Msgs</tr></thead><tbody id=T></tbody></table></div>
<div class=c><h2>OTHER DETECTED DEVICES</h2><table><thead><tr><th>Sel<th>Last Seen<th>Model<th>ID<th>Msgs</tr></thead><tbody id=D></tbody></table></div>
<script>
const P=i=>document.getElementById(i),J=async u=>{try{return await(await fetch(u)).json()}catch(e){}}
const TT=t=>{let e=P('msg');e.innerHTML=t;e.style.opacity=1;setTimeout(()=>e.style.opacity=0,1900)}
const G=(u,t)=>fetch(u).then(()=>{TT(t||'Done');setTimeout(L,400)})
const A=s=>s<0?'-':s<60?s+'s':s<3600?(s/60|0)+'m':s<86400?(s/3600|0)+'h':(s/86400|0)+'d'
const K=n=>n>999?(n/1000).toFixed(1)+'k':n
let IN=0
async function L(){let m=await J('/mqtt-selected'),h=await J('/health'),st=await J('/status');if(!m)return
let SET=new Set(m.ids.map(String)),SC={};(m.seen||[]).forEach((d,j)=>SC[d]=m.seenCounts[j])
if(h){let fr=st?(+st.freq).toFixed(3):'?';P('stat').innerHTML=`good decodes <b class=g>${h.counters.good}</b> &middot; SX1276 @ <b class=g>${fr}</b> MHz &middot; bw idx ${h.bw} &middot; ${h.radio.ok?'OK':'FAULT'} &middot; heap ${h.heap} &middot; up ${h.uptime}s`
 if(document.activeElement!==P('bw'))P('bw').value=h.bw
 if(st&&document.activeElement!==P('f'))P('f').value=fr
 if(st&&st.gate!==undefined&&document.activeElement!==P('gt'))P('gt').value=st.gate
 if(st&&st.rssithr!==undefined&&document.activeElement!==P('ht'))P('ht').value=st.rssithr
 if(!IN){IN=1;P('en').value=m.espname||''}}
if(!P('T').contains(document.activeElement)){let t='';m.ids.forEach((d,i)=>{let l=m.last[i]||{};t+=`<tr><td><input type=checkbox checked onchange="G('/mqtt-toggle?id=${d}&on=0','Removed from MQTT')"><td><input class=n id=n${d} value="${m.names[i]||''}"><td><input class=u id=u${d} value="${m.units[i]||''}"><td><input class=s id=c${d} type=number step=any value="${m.scales[i]||1}"><td><button onclick=S(${d})>Save</button><td><button class=pub onclick="G('/mqtt-publish?id=${d}','Published &uarr;')">&#8593; Pub</button><td class=k>${A(m.ages[i])} ago<td>${l.model||''}<td class=id>${d}<td class=g>${l.consumption_data||''}<td>${l.rssi||''}<td class=g>${K(m.counts&&m.counts[i]!=null?m.counts[i]:0)}</tr>`})
 P('T').innerHTML=t||'<tr><td colspan=12 class=k>none selected yet</td></tr>'}
let x='';(m.seen||[]).forEach((d,i)=>{if(SET.has(String(d)))return;x+=`<tr><td><button class=add onclick="G('/mqtt-toggle?id=${d}&on=1','Added to MQTT')">+ Add</button><td class=k>${A(m.seenAges[i])} ago<td>${m.seenModels[i]||''}<td class=id>${d}<td class=g>${K(m.seenCounts[i]||0)}</tr>`})
P('D').innerHTML=x||'<tr><td colspan=5 class=k>Waiting for decode events&hellip;</td></tr>'}
function S(d){fetch('/ctrl?devname='+d+'&name='+encodeURIComponent(P('n'+d).value)).then(()=>fetch('/ctrl?devunit='+d+'&unit='+encodeURIComponent(P('u'+d).value))).then(()=>fetch('/ctrl?devscale='+d+'&scale='+P('c'+d).value)).then(()=>{TT('Saved &#10003;');setTimeout(L,500)})}
function SN(){fetch('/ctrl?espname='+encodeURIComponent(P('en').value)).then(()=>TT('ESP name saved &#10003;'))}
const AF=()=>{let v=parseFloat(P('f').value);if(isNaN(v)||v<300||v>928){TT('Enter frequency 300-928');P('f').focus();return}G('/ctrl?f='+v,'Frequency applied &#10003;')}
const AB=()=>G('/ctrl?bw='+P('bw').value,'Bandwidth applied &#10003;')
const AG=()=>{let v=parseInt(P('gt').value);if(isNaN(v)){TT('Enter a number for RSSI gate');P('gt').focus();return}G('/ctrl?gate='+v,'RSSI gate applied &#10003;')}
const AH=()=>{let v=parseInt(P('ht').value);if(isNaN(v)){TT('Enter a number for HW threshold');P('ht').focus();return}G('/ctrl?rssithr='+v,'HW threshold applied &#10003;')}
setInterval(L,5000);L()
</script></body></html>)DASH";

static void setupServer() {
    server.on("/", HTTP_GET, [](AsyncWebServerRequest* r){

        AsyncWebServerResponse* resp = r->beginResponse_P(200, "text/html", DASH_GZ, DASH_GZ_LEN);
        resp->addHeader("Content-Encoding", "gzip");
        r->send(resp);
    });

    server.on("/seen-clear", HTTP_GET, [](AsyncWebServerRequest* r){

        xSemaphoreTake(mqttMutex, portMAX_DELAY);
        seenIdCount = 0; seenDirty = true;
        xSemaphoreGive(mqttMutex);
        nvsSaveSeen();
        r->send(200, "application/json", "{\"ok\":1}");
    });
    server.on("/reboot", HTTP_GET, [](AsyncWebServerRequest* r){
        r->send(200, "application/json", "{\"ok\":1,\"msg\":\"rebooting\"}");
        delay(200); ESP.restart();
    });
    server.on("/status", HTTP_GET, [](AsyncWebServerRequest* r){

        int liveRssi = 0;
        if (radioOk) {
            uint8_t raw = radio.getMod()->SPIreadRegister(0x11);
            liveRssi = -((int)raw / 2);
        }

        int sigRssi = (rtl_433_ESP::signalRssi != 0) ? rtl_433_ESP::signalRssi : liveRssi;
        char buf[220];
        snprintf(buf, sizeof(buf),
            "{\"rssi\":%d,\"avgRssi\":%d,\"liveRssi\":%d,\"freq\":%.3f,\"gate\":%d,\"rssithr\":%d,\"bw\":%d,\"good\":%lu}",
            sigRssi, liveRssi, liveRssi, rxFreq, rssiGate, libRssiThresh, rxBwIdx, goodDecodes);
        r->send(200, "application/json", buf);
    });
    server.on("/decoded", HTTP_GET, [](AsyncWebServerRequest* r){
        xSemaphoreTake(pktMutex, portMAX_DELAY);
        static char buf[8192];
        int p = snprintf(buf, sizeof(buf), "{\"good\":%lu,\"packets\":[", goodDecodes);
        int start = (pktCount < MAX_PKT) ? 0 : pktHead;
        bool first = true;
        for (int i = pktCount - 1; i >= 0; i--) {

            if (p > 1100) break;
            int idx = ((start + i) % MAX_PKT);
            if (!first) buf[p++] = ',';
            p += snprintf(buf + p, sizeof(buf) - p, "{\"ts\":%lu,\"json\":", pkts[idx].ts);
            p += snprintf(buf + p, sizeof(buf) - p, "%s}", pkts[idx].json);
            first = false;
        }
        snprintf(buf + p, sizeof(buf) - p, "]}");
        xSemaphoreGive(pktMutex);
        r->send(200, "application/json", buf);
    });
    server.on("/ctrl", HTTP_GET, [](AsyncWebServerRequest* r){
        if (r->hasParam("f")) {
            float f = r->getParam("f")->value().toFloat();
            if (f >= 300 && f <= 928) {
                rxFreq = f;
                rtl_433_ESP::initReceiver(PIN_GDO0, f);
                rtl_433_ESP::enableReceiver();
                nvsSave();
                printf("[CTRL] Freq -> %.3f MHz\n", f);
            }
        }
        if (r->hasParam("gate")) {
            rssiGate = r->getParam("gate")->value().toInt();
            printf("[CTRL] Gate -> %d dBm\n", rssiGate);
        }
        if (r->hasParam("bw")) {
            int idx = r->getParam("bw")->value().toInt();
            if (idx >= 0 && idx < 7) {
                rxBwIdx = (uint8_t)idx;
                if (radioOk) {

                    rf.initReceiver(PIN_GDO0, rxFreq);
                    rf.enableReceiver();
                    rf.setRSSIThreshold(libRssiThresh);
                    radio.getMod()->SPIwriteRegister(0x12, BW_REGS[rxBwIdx]);
                }
                nvsSave();
                printf("[CTRL] BW idx=%d -> %.1f kHz (reg 0x%02X)\n", idx, BW_KHZ[rxBwIdx], BW_REGS[rxBwIdx]);
            }
        }
        if (r->hasParam("reg") && r->hasParam("val")) {
            int addr = strtol(r->getParam("reg")->value().c_str(), nullptr, 16);
            int val  = strtol(r->getParam("val")->value().c_str(), nullptr, 16);
            if (addr >= 0 && addr <= 0x7F && val >= 0 && val <= 0xFF && radioOk) {
                radio.getMod()->SPIwriteRegister((uint8_t)addr, (uint8_t)val);
                if (addr == 0x10) rssiThreshReg = (uint8_t)val;
                if (addr == 0x14) ookPeak       = (uint8_t)val;
                if (addr == 0x15) ookFix        = (uint8_t)val;
                if (addr == 0x16) ookAvg        = (uint8_t)val;
                nvsSave();
                printf("[CTRL] SX1276 reg 0x%02X = 0x%02X\n", addr, val);
            }
        }

        if (r->hasParam("rssithr")) {
            libRssiThresh = r->getParam("rssithr")->value().toInt();
            if (radioOk) rf.setRSSIThreshold(libRssiThresh);
            nvsSave();
            printf("[CTRL] libRssiThresh -> %d dB\n", libRssiThresh);
        }
        if (r->hasParam("espname")) {
            String nm = r->getParam("espname")->value(); nm.trim();
            if (nm.length() < 32) {
                xSemaphoreTake(mqttMutex, portMAX_DELAY);
                strncpy(espName, nm.c_str(), sizeof(espName) - 1); espName[sizeof(espName) - 1] = '\0';
                sanitizeName(espName); nvsSave(); mqttNeedsDiscovery = true;
                xSemaphoreGive(mqttMutex);
            }
        }
        if (r->hasParam("devname") && r->hasParam("name")) {
            uint32_t devId = (uint32_t)r->getParam("devname")->value().toInt();
            String nm = r->getParam("name")->value(); nm.trim();
            if (nm.length() < 32) {
                xSemaphoreTake(mqttMutex, portMAX_DELAY);
                for (int i = 0; i < mqttIdCount; i++) {
                    if (mqttIds[i] == devId) {
                        strncpy(deviceNames[i], nm.c_str(), sizeof(deviceNames[i]) - 1);
                        deviceNames[i][sizeof(deviceNames[i]) - 1] = '\0';
                        sanitizeName(deviceNames[i]); nvsSave(); mqttNeedsDiscovery = true; break;
                    }
                }
                xSemaphoreGive(mqttMutex);
            }
        }
        if (r->hasParam("devunit") && r->hasParam("unit")) {
            uint32_t devId = (uint32_t)r->getParam("devunit")->value().toInt();
            String u = r->getParam("unit")->value(); u.trim();
            if (u.length() < 16) {
                xSemaphoreTake(mqttMutex, portMAX_DELAY);
                for (int i = 0; i < mqttIdCount; i++) {
                    if (mqttIds[i] == devId) {
                        strncpy(deviceUnits[i], u.c_str(), sizeof(deviceUnits[i]) - 1);
                        deviceUnits[i][sizeof(deviceUnits[i]) - 1] = '\0';
                        sanitizeName(deviceUnits[i]); nvsSave(); mqttNeedsDiscovery = true; break;
                    }
                }
                xSemaphoreGive(mqttMutex);
            }
        }
        if (r->hasParam("devscale") && r->hasParam("scale")) {
            uint32_t devId = (uint32_t)r->getParam("devscale")->value().toInt();
            float sc = r->getParam("scale")->value().toFloat();
            if (sc <= 0.0f) sc = 1.0f;
            xSemaphoreTake(mqttMutex, portMAX_DELAY);
            for (int i = 0; i < mqttIdCount; i++) {
                if (mqttIds[i] == devId) {
                    deviceScales[i] = sc; nvsSave(); mqttNeedsDiscovery = true; break;
                }
            }
            xSemaphoreGive(mqttMutex);
        }
        r->send(200, "application/json", "{\"ok\":1}");
    });
    server.on("/health", HTTP_GET, [](AsyncWebServerRequest* r){

        uint8_t brM = radioOk ? radio.getMod()->SPIreadRegister(0x02) : 0;
        uint8_t brL = radioOk ? radio.getMod()->SPIreadRegister(0x03) : 0;
        uint16_t brRaw = ((uint16_t)brM << 8) | brL;
        float liveBitrate = brRaw ? (32000000.0f / brRaw / 1000.0f) : 0.0f;
        char buf[720];
        snprintf(buf, sizeof(buf),
            "{\"radio\":{\"ok\":%s,\"msg\":\"%s\",\"ver\":\"0x%02X\",\"expected\":\"0x12\",\"bitrateKbaud\":%.2f},"
            "\"wifi\":{\"ok\":%s,\"ip\":\"%s\"},"
            "\"mqtt\":{\"ok\":%s,\"broker\":\"%s\"},"
            "\"nvs\":{\"devices\":%d},"
            "\"bw\":%u,"
            "\"ookPeak\":%u,\"ookFix\":%u,\"ookAvg\":%u,"
            "\"counters\":{\"raw\":%lu,\"good\":%lu,\"rejModel\":%lu,\"rejRssi\":%lu,\"rejPlaus\":%lu},"
            "\"fw\":\"%s\","
            "\"heap\":%u,\"maxAlloc\":%u,"
            "\"uptime\":%lu}",
            radioOk ? "true" : "false", radioErrMsg, radioVer, liveBitrate,
            WiFi.status() == WL_CONNECTED ? "true" : "false",
            WiFi.localIP().toString().c_str(),
            mqtt.connected() ? "true" : "false", MQTT_HOST,
            mqttIdCount, rxBwIdx,
            ookPeak, ookFix, ookAvg,
            (unsigned long)rawCallbacks, (unsigned long)goodDecodes,
            (unsigned long)rejectedModel, (unsigned long)rejectedRssi, (unsigned long)rejectedPlaus,
            FW_VERSION, (unsigned)ESP.getFreeHeap(), (unsigned)ESP.getMaxAllocHeap(), millis() / 1000UL);
        r->send(200, "application/json", buf);
    });

    server.on("/registers", HTTP_GET, [](AsyncWebServerRequest* r){
        if (!radioOk) { r->send(503, "application/json", "{\"error\":\"radio not ready\"}"); return; }
        uint8_t t = radio.getMod()->SPIreadRegister(0x10);
        uint8_t p = radio.getMod()->SPIreadRegister(0x14);
        uint8_t f = radio.getMod()->SPIreadRegister(0x15);
        uint8_t a = radio.getMod()->SPIreadRegister(0x16);
        char buf[180];
        snprintf(buf, sizeof(buf),
            "{\"RegRssiThresh\":{\"addr\":\"0x10\",\"val\":%u},"
            "\"RegOokPeak\":{\"addr\":\"0x14\",\"val\":%u},"
            "\"RegOokFix\":{\"addr\":\"0x15\",\"val\":%u},"
            "\"RegOokAvg\":{\"addr\":\"0x16\",\"val\":%u}}",
            t, p, f, a);
        r->send(200, "application/json", buf);
    });

    server.on("/reset-defaults", HTTP_GET, [](AsyncWebServerRequest* r){
        rxFreq  = 914.224f;
        rssiGate = -95;
        rxBwIdx = 0;
        ookPeak       = 0x0B;
        ookFix        = 0x0C;
        ookAvg        = 0x12;
        rssiThreshReg = 0xF0;
        libRssiThresh = 3;
        if (radioOk) {

            rf.initReceiver(PIN_GDO0, rxFreq);
            rf.enableReceiver();
            rf.setRSSIThreshold(libRssiThresh);
            radio.getMod()->SPIwriteRegister(0x12, BW_REGS[rxBwIdx]);
        }
        nvsSave();
        printf("[RESET] All settings restored to working defaults\n");
        r->send(200, "application/json", "{\"ok\":1,\"bw\":4}");
    });

    server.on("/libstats", HTTP_GET, [](AsyncWebServerRequest* r){
        char b[360];
        snprintf(b, sizeof(b),
            "{\"currentRssi\":%d,\"averageRssi\":%d,\"rssiThreshold\":%d,\"signalRssi\":%d,"
            "\"totalSignals\":%d,\"ignoredSignals\":%d,\"unparsedSignals\":%d,\"messageCount\":%d}",
            rtl_433_ESP::currentRssi, rtl_433_ESP::averageRssi, rtl_433_ESP::rssiThreshold,
            rtl_433_ESP::signalRssi, rtl_433_ESP::totalSignals, rtl_433_ESP::ignoredSignals,
            rtl_433_ESP::unparsedSignals, rtl_433_ESP::messageCount);
        r->send(200, "application/json", b);
    });
    server.on("/diag", HTTP_GET, [](AsyncWebServerRequest* r){
        char buf[400];
        uint8_t opMode = radioOk ? radio.getMod()->SPIreadRegister(0x01) : 0;
        uint8_t mode   = opMode & 0x07;
        const char* modeName = "?";
        switch (mode) {
            case 0: modeName="Sleep"; break;
            case 1: modeName="Standby"; break;
            case 2: modeName="FSTX"; break;
            case 3: modeName="TX"; break;
            case 4: modeName="FSRX"; break;
            case 5: modeName="RxContinuous"; break;
            case 6: modeName="RxSingle"; break;
            case 7: modeName="CAD"; break;
        }
        int dio2Now = digitalRead(PIN_GDO0);

        uint32_t transitions = 0;
        int lastLevel = dio2Now;
        uint32_t tStart = micros();
        while (micros() - tStart < 200000UL) {
            int lvl = digitalRead(PIN_GDO0);
            if (lvl != lastLevel) { transitions++; lastLevel = lvl; }
        }
        int dio2Post = digitalRead(PIN_GDO0);
        snprintf(buf, sizeof(buf),
            "{\"pin\":%d,\"dio2Now\":%d,\"dio2Post\":%d,\"transitions200ms\":%lu,"
            "\"regOpMode\":\"0x%02X\",\"mode\":%u,\"modeName\":\"%s\","
            "\"longRangeMode\":%u,\"modulationType\":%u,"
            "\"verdict\":\"%s\"}",
            PIN_GDO0, dio2Now, dio2Post, (unsigned long)transitions,
            opMode, mode, modeName,
            (opMode >> 7) & 1, (opMode >> 5) & 3,
            (transitions == 0)
                ? (mode == 5 ? "DIO2 line dead — chip in RxContinuous but no OOK output (config/wiring fault)"
                             : "Chip NOT in RxContinuous mode — library never enabled receive")
                : "DIO2 alive — pulses arriving, check why library ISR drops them");
        r->send(200, "application/json", buf);
    });

    server.on("/ert-optimize", HTTP_GET, [](AsyncWebServerRequest* r){
        if (!radioOk) { r->send(503, "application/json", "{\"ok\":0,\"err\":\"radio not ready\"}"); return; }

        rxBwIdx = 0;
        radio.getMod()->SPIwriteRegister(0x12, BW_REGS[0]);
        radio.getMod()->SPIwriteRegister(0x10, 0xFF);
        radio.getMod()->SPIwriteRegister(0x14, 0x08);
        radio.getMod()->SPIwriteRegister(0x15, 0x06);
        radio.getMod()->SPIwriteRegister(0x02, 0x01);
        radio.getMod()->SPIwriteRegister(0x03, 0xE9);
        rssiThreshReg = 0xFF; ookPeak = 0x08; ookFix = 0x06; ookAvg = 0x12;
        nvsSave();
        printf("[ERT-OPT] Applied: BW=250kHz BR=65.46kBaud RssiThr=0xFF OokPeak=0x08 OokFix=0x06\n");
        r->send(200, "application/json", "{\"ok\":1,\"msg\":\"ERT-optimized config applied\"}");
    });

    server.on("/pulse-start", HTTP_GET, [](AsyncWebServerRequest* r){
        pulseMonEnabled = true;
        r->send(200, "application/json", "{\"on\":true}");
    });
    server.on("/pulse-stop", HTTP_GET, [](AsyncWebServerRequest* r){
        pulseMonEnabled = false;
        r->send(200, "application/json", "{\"on\":false}");
    });
    server.on("/pulses", HTTP_GET, [](AsyncWebServerRequest* r){

        static char buf[12000];
        int p = snprintf(buf, sizeof(buf),
            "{\"enabled\":%s,\"sampleRateHz\":20000,\"head\":%u,\"samples\":[",
            pulseMonEnabled ? "true" : "false", pulseHead);
        for (int i = 0; i < PULSE_SAMPLES; i++) {
            if (i) buf[p++] = ',';
            p += snprintf(buf + p, sizeof(buf) - p, "%u", pulseBuf[i]);
            if (p > (int)sizeof(buf) - 16) break;
        }
        snprintf(buf + p, sizeof(buf) - p, "]}");
        r->send(200, "application/json", buf);
    });

    server.on("/mqtt-status", HTTP_GET, [](AsyncWebServerRequest* r){
        r->send(200, "application/json", mqtt.connected() ? "{\"connected\":true}" : "{\"connected\":false}");
    });
    server.on("/mqtt-toggle", HTTP_GET, [](AsyncWebServerRequest* r){
        if (!r->hasParam("id") || !r->hasParam("on")) { r->send(400, "application/json", "{\"ok\":0}"); return; }
        uint32_t id = (uint32_t)r->getParam("id")->value().toInt();
        bool on = r->getParam("on")->value() == "1";
        xSemaphoreTake(mqttMutex, portMAX_DELAY);
        if (on) mqttAddId(id); else mqttRemoveId(id);
        xSemaphoreGive(mqttMutex);
        r->send(200, "application/json", "{\"ok\":1}");
    });
    server.on("/seen-delete", HTTP_GET, [](AsyncWebServerRequest* r){
        if (!r->hasParam("id")) { r->send(400, "application/json", "{\"ok\":0}"); return; }
        uint32_t delId = (uint32_t)r->getParam("id")->value().toInt();
        xSemaphoreTake(mqttMutex, portMAX_DELAY);
        seenRemoveId(delId);
        seenDirty = true;
        xSemaphoreGive(mqttMutex);
        r->send(200, "application/json", "{\"ok\":1}");
    });
    server.on("/mqtt-selected", HTTP_GET, [](AsyncWebServerRequest* r){
        xSemaphoreTake(mqttMutex, portMAX_DELAY);
        static char buf[11000];
        int p = snprintf(buf, sizeof(buf), "{\"ids\":[");
        for (int i = 0; i < mqttIdCount; i++) { if (i) buf[p++] = ','; p += snprintf(buf+p, sizeof(buf)-p, "%lu", (unsigned long)mqttIds[i]); }
        p += snprintf(buf+p, sizeof(buf)-p, "],\"last\":[");
        for (int i = 0; i < mqttIdCount; i++) { if (i) buf[p++] = ','; if (mqttLastJson[i][0]) p += snprintf(buf+p, sizeof(buf)-p, "%s", mqttLastJson[i]); else p += snprintf(buf+p, sizeof(buf)-p, "null"); }
        p += snprintf(buf+p, sizeof(buf)-p, "],\"names\":[");
        for (int i = 0; i < mqttIdCount; i++) { if (i) buf[p++] = ','; p += snprintf(buf+p, sizeof(buf)-p, "\"%s\"", deviceNames[i]); }
        p += snprintf(buf+p, sizeof(buf)-p, "],\"units\":[");
        for (int i = 0; i < mqttIdCount; i++) { if (i) buf[p++] = ','; p += snprintf(buf+p, sizeof(buf)-p, "\"%s\"", deviceUnits[i]); }
        p += snprintf(buf+p, sizeof(buf)-p, "],\"scales\":[");
        for (int i = 0; i < mqttIdCount; i++) { if (i) buf[p++] = ','; p += snprintf(buf+p, sizeof(buf)-p, "%g", deviceScales[i]); }
        p += snprintf(buf+p, sizeof(buf)-p, "],\"ages\":[");
        time_t nowEpoch; time(&nowEpoch); bool ntpOk = (nowEpoch > 1000000000L); uint32_t nowMs = millis();
        for (int i = 0; i < mqttIdCount; i++) { if (i) buf[p++] = ','; long age=-1; if(ntpOk&&lastSeenTime[i]>0)age=(long)(nowEpoch-lastSeenTime[i]); else if(lastSeenMs[i]>0)age=(long)((nowMs-lastSeenMs[i])/1000); p+=snprintf(buf+p,sizeof(buf)-p,"%ld",age); }
        p += snprintf(buf+p, sizeof(buf)-p, "],\"counts\":[");
        for (int i = 0; i < mqttIdCount; i++) { if (i) buf[p++] = ','; p += snprintf(buf+p, sizeof(buf)-p, "%lu", (unsigned long)deviceDecodeCounts[i]); }
        p += snprintf(buf+p, sizeof(buf)-p, "],\"espname\":\"%s\",\"seen\":[", espName);
        for (int i = 0; i < seenIdCount; i++) { if (i) buf[p++] = ','; p += snprintf(buf+p, sizeof(buf)-p, "%lu", (unsigned long)seenIds[i]); }
        p += snprintf(buf+p, sizeof(buf)-p, "],\"seenAges\":[");
        { time_t _ne; time(&_ne); bool _nok = (_ne > 1000000000L);
          for (int i = 0; i < seenIdCount; i++) { if (i) buf[p++] = ','; long _a = (_nok && seenLastTime[i] > 0) ? (long)(_ne - seenLastTime[i]) : -1L; p += snprintf(buf+p, sizeof(buf)-p, "%ld", _a); } }
        p += snprintf(buf+p, sizeof(buf)-p, "],\"seenCounts\":[");
        for (int i = 0; i < seenIdCount; i++) { if (i) buf[p++] = ','; p += snprintf(buf+p, sizeof(buf)-p, "%lu", (unsigned long)seenDecodeCount[i]); }
        p += snprintf(buf+p, sizeof(buf)-p, "],\"seenModels\":[");
        for (int i = 0; i < seenIdCount; i++) {
            if (i) buf[p++] = ',';
            const char* ms = strstr(seenLastJson[i], "\"model\":\"");
            if (ms) { ms += 9; const char* me = strchr(ms, '"'); if (me && (me-ms) < 32) { buf[p++]='"'; memcpy(buf+p,ms,me-ms); p+=me-ms; buf[p++]='"'; buf[p]='\0'; continue; } }
            p += snprintf(buf+p, sizeof(buf)-p, "\"?\"");
        }
        p += snprintf(buf+p, sizeof(buf)-p, "],\"seenConsumption\":[");
        for (int i = 0; i < seenIdCount; i++) {
            if (i) buf[p++] = ',';
            const char* cp = strstr(seenLastJson[i], "\"consumption_data\":");
            if (cp) p += snprintf(buf+p, sizeof(buf)-p, "%lu", (unsigned long)strtoul(cp + 19, nullptr, 10));
            else    p += snprintf(buf+p, sizeof(buf)-p, "null");
        }
        p += snprintf(buf+p, sizeof(buf)-p, "],\"seenRssi\":[");
        for (int i = 0; i < seenIdCount; i++) {
            if (i) buf[p++] = ',';
            const char* rp2 = strstr(seenLastJson[i], "\"rssi\":");
            if (rp2) p += snprintf(buf+p, sizeof(buf)-p, "%ld", strtol(rp2 + 7, nullptr, 10));
            else     p += snprintf(buf+p, sizeof(buf)-p, "null");
        }
        p += snprintf(buf+p, sizeof(buf)-p, "]}");
        xSemaphoreGive(mqttMutex);
        r->send(200, "application/json", buf);
    });
    server.on("/mqtt-publish", HTTP_GET, [](AsyncWebServerRequest* r){
        if (!r->hasParam("id")) { r->send(400, "application/json", "{\"ok\":0}"); return; }
        uint32_t devId = (uint32_t)r->getParam("id")->value().toInt();
        xSemaphoreTake(mqttMutex, portMAX_DELAY);
        bool found = false;
        for (int i = 0; i < mqttIdCount; i++) {
            if (mqttIds[i] == devId) {
                const char* js = mqttLastJson[i][0] ? mqttLastJson[i] : nullptr;
                if (!js) {
                    for (int s = 0; s < seenIdCount; s++)
                        if (seenIds[s] == devId && seenLastJson[s][0]) { js = seenLastJson[s]; break; }
                }
                if (js) { char pubTopic[80]; getStateTopic(i, pubTopic, sizeof(pubTopic)); mqttEnqueue(pubTopic, js); found = true; }
                break;
            }
        }
        xSemaphoreGive(mqttMutex);
        r->send(200, "application/json", found ? "{\"ok\":1}" : "{\"ok\":0,\"err\":\"no data yet\"}");
    });
    events.onConnect([](AsyncEventSourceClient* c){ printf("[SSE] Client connected\n"); });
    server.addHandler(&events);
    server.begin();
    printf("[HTTP] Server started\n");
}

static void nvsTask(void*);
void setup() {
    Serial.begin(115200);
    delay(500);

    pinMode(VEXT_CTRL, OUTPUT);
    digitalWrite(VEXT_CTRL, LOW);
    delay(100);

    pinMode(OLED_RST, OUTPUT);
    digitalWrite(OLED_RST, LOW); delay(50);
    digitalWrite(OLED_RST, HIGH);
    display.init();
    display.flipScreenVertically();
    display.setFont(ArialMT_Plain_10);
    display.clear();
    display.drawString(0, 0, "ERT SX1276 Booting...");
    display.drawString(0, 16, FW_VERSION);
    display.display();

    printf("\n[BOOT] %s\n", FW_VERSION);

    pktMutex     = xSemaphoreCreateMutex();
    mqttMutex    = xSemaphoreCreateMutex();
    mqttPubMutex = xSemaphoreCreateMutex();
    nvsMutex     = xSemaphoreCreateMutex();
    nvsLoad();
    nvsLoadSeen();

    WiFi.setAutoReconnect(true);
    WiFi.begin(WIFI_SSID, WIFI_PASS);
    printf("[WIFI] Connecting");
    display.clear();
    display.drawString(0, 0, "WiFi connecting...");
    display.display();
    uint32_t t0 = millis();
    while (WiFi.status() != WL_CONNECTED && millis() - t0 < 15000) { delay(500); printf("."); }
    if (WiFi.status() == WL_CONNECTED) {
        printf("\n[WIFI] %s\n", WiFi.localIP().toString().c_str());
        display.clear();
        display.drawString(0, 0, "WiFi OK");
        display.drawString(0, 16, WiFi.localIP().toString());
        display.display();
        configTime(0, 0, "pool.ntp.org", "time.nist.gov");
        time_t t = 0; int tries = 0;
        while (t < 1000000000L && tries++ < 20) { delay(200); time(&t); }
        printf("[NTP] %s\n", t > 1000000000L ? "OK" : "failed");
    } else {
        printf("\n[WIFI] Failed\n");
    }

    ArduinoOTA.setHostname("heltec-ert");
    ArduinoOTA.begin();

    mqtt.setServer(MQTT_HOST, MQTT_PORT);
    mqtt.setBufferSize(600);
    mqttEnsureConnected();
    printf("[MQTT] broker=%s:%d client=%s\n", MQTT_HOST, MQTT_PORT, MQTT_CLIENT_ID);

    setupServer();
    printf("[READY] %s  http://%s\n", FW_VERSION, WiFi.localIP().toString().c_str());

    rf.initReceiver(PIN_GDO0, rxFreq);
    rf.setCallback(rtl433Callback, rfMsgBuf, sizeof(rfMsgBuf));
    rf.enableReceiver();
    rf.setRSSIThreshold(libRssiThresh);

    radioVer = radio.getMod()->SPIreadRegister(0x42);
    if (radioVer == 0x12) {
        radioOk = true;

        radio.getMod()->SPIwriteRegister(0x12, BW_REGS[rxBwIdx]);

        uint8_t brM = radio.getMod()->SPIreadRegister(0x02);
        uint8_t brL = radio.getMod()->SPIreadRegister(0x03);
        printf("[SX1276] Bitrate set: 0x%02X%02X (raw=%u) target 0x01E9 (489)\n", brM, brL, ((uint16_t)brM<<8)|brL);
        uint16_t brRaw = ((uint16_t)brM << 8) | brL;
        float brk = brRaw ? 32000.0f / brRaw : 0.0f;
        snprintf(radioErrMsg, sizeof(radioErrMsg), "SX1276 OK ver=0x12 %.3f MHz, bitrate=%.2f kBaud", rxFreq, brk);
    } else {
        radioOk = false;
        snprintf(radioErrMsg, sizeof(radioErrMsg), "SX1276 NOT FOUND ver=0x%02X (expected 0x12) — check Vext/wiring/DIO2", radioVer);
        printf("[SX1276] FAULT: %s\n", radioErrMsg);
    }

    printf("[SX1276] %s  DIO2=GPIO%d  ver=0x%02X\n", radioErrMsg, PIN_GDO0, radioVer);
    uint8_t actBw = radioOk ? radio.getMod()->SPIreadRegister(0x12) : 0;
    uint8_t actOp = radioOk ? radio.getMod()->SPIreadRegister(0x14) : 0;
    printf("[SX1276] actual regs: RxBw(0x12)=0x%02X (%.1f kHz)  OokPeak(0x14)=0x%02X  BitSync=%s\n",
           actBw, (rxBwIdx < 7 ? BW_KHZ[rxBwIdx] : 0.0f), actOp, (actOp & 0x20) ? "ON" : "OFF");
    printf("[RTL433] MINIMUM_PULSE_LENGTH=%d us  MINIMUM_SIGNAL_LENGTH=%d us\n",
           MINIMUM_PULSE_LENGTH, MINIMUM_SIGNAL_LENGTH);

    xTaskCreatePinnedToCore(nvsTask, "nvsTask", 4096, nullptr, 1, nullptr, 0);

    pinMode(PIN_DIO0, INPUT);
    pinMode(PIN_DIO1, INPUT);
    xTaskCreatePinnedToCore([](void*){
        for (;;) {
            if (!pulseMonEnabled) { vTaskDelay(pdMS_TO_TICKS(100)); continue; }
            uint32_t t0 = micros();
            uint8_t s = 0;
            if (digitalRead(PIN_DIO0))  s |= 0x01;
            if (digitalRead(PIN_GDO0))  s |= 0x02;
            if (digitalRead(PIN_DIO1))  s |= 0x04;
            pulseBuf[pulseHead] = s;
            pulseHead = (pulseHead + 1) % PULSE_SAMPLES;

            while (micros() - t0 < 50) {  }
        }
    }, "pulseMon", 2048, nullptr, 1, nullptr, 1);

    oledUpdate();
}

static void nvsTask(void*) {
    uint32_t tick = 0;
    for (;;) {
        vTaskDelay(pdMS_TO_TICKS(10000));
        if (mqttJsonDirty) nvsSaveLastJson();
        nvsSaveLastSeen();
        nvsSaveSeenDirty();
        if (++tick >= 3) { tick = 0; if (seenDirty) nvsSaveSeen(); }
    }
}

void loop() {
    ArduinoOTA.handle();
    if (radioOk && millis() > guiPauseUntil) rf.loop();
    mqtt.loop();
    mqttDrainQueue();

    static bool rxReinitDone = false;
    if (!rxReinitDone && radioOk && millis() > 6000) {
        rxReinitDone = true;
        rf.initReceiver(PIN_GDO0, rxFreq);
        rf.enableReceiver();
        rf.setRSSIThreshold(libRssiThresh);
        radio.getMod()->SPIwriteRegister(0x12, BW_REGS[rxBwIdx]);
        printf("[SX1276] post-boot re-init @ %.3f MHz, RxBw idx %d (0x%02X)\n",
               rxFreq, rxBwIdx, BW_REGS[rxBwIdx]);
    }

    bool nowConnected = mqtt.connected();
    if (nowConnected && (!mqttDiscoveryDone || mqttNeedsDiscovery)) {
        xSemaphoreTake(mqttMutex, portMAX_DELAY);
        for (int i = 0; i < mqttIdCount; i++) {
            publishDiscovery(mqttIds[i], i);

            if (mqttLastJson[i][0]) { char st[80]; getStateTopic(i, st, sizeof(st)); mqttEnqueue(st, mqttLastJson[i]); }
        }
        xSemaphoreGive(mqttMutex);
        mqttDiscoveryDone  = true;
        mqttNeedsDiscovery = false;
    }
    if (!nowConnected) mqttDiscoveryDone = false;

    static uint32_t lastWifiRetry = 0;
    if (WiFi.status() != WL_CONNECTED) {
        uint32_t nowMs = millis();
        if (nowMs - lastWifiRetry > 30000) { lastWifiRetry = nowMs; WiFi.reconnect(); }
    }
    static uint32_t lastMqttRetry = 0;
    if (!mqtt.connected() && WiFi.status() == WL_CONNECTED) {
        uint32_t nowMs = millis();
        if (nowMs - lastMqttRetry > 10000) { lastMqttRetry = nowMs; mqttEnsureConnected(); }
    }

    uint32_t now = millis();
    if (radioOk && now - lastStatusMs >= 60000) { lastStatusMs = now; rf.getStatus(); }

    static uint32_t lastBrCheck = 0;
    if (false && radioOk && now - lastBrCheck >= 30000) {
        lastBrCheck = now;
        uint8_t brM = radio.getMod()->SPIreadRegister(0x02);
        uint8_t brL = radio.getMod()->SPIreadRegister(0x03);
        uint16_t cur = ((uint16_t)brM << 8) | brL;

        if (cur < 484 || cur > 494) {

            radio.getMod()->SPIwriteRegister(0x02, 0x01);
            radio.getMod()->SPIwriteRegister(0x03, 0xE9);
            printf("[REG] Bitrate drifted (raw=%u) — restored to 0x01E9 (65.46 kBaud)\n", cur);
        }
    }

    if (now - lastOledMs >= 5000) { lastOledMs = now; oledUpdate(); }
}
