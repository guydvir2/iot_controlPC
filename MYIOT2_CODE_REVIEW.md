# myIOT2.cpp - Complete Code Review
**Date:** February 16, 2026  
**File:** `.pio/libdeps/esp12e/myIOT2/src/myIOT2.cpp`

---

## CRITICAL ISSUES TABLE

| # | Issue | Severity | Line(s) | Fix Strategy |
|---|-------|----------|---------|--------------|
| 1 | Buffer overflow in MQTT callback — `incoming_msg[30]` no bounds check | **CRITICAL** | 363–370 | Add bounds: `i < 29` in loop |
| 2 | Unsafe `strcpy()` without length checks in `start_services()` | **HIGH** | 9–13 | Replace with `strncpy()` + null terminator |
| 3 | `sprintf()` into 50-byte buffer without bounds in WiFi handlers | **HIGH** | 131, 136 | Use `snprintf(b, sizeof(b), ...)` |
| 4 | Memory leaks from `new char` allocations in topic management | **MEDIUM** | 919–921 | Implement destructor/cleanup or use std::string |
| 5 | Hard-coded buffer size `DynamicJsonDocument(1250)` with FIX comment | **MEDIUM** | 886 | Define constant `#define JSON_BUFFER_SIZE 1250` |
| 6 | `sprintf()` into 50-byte buffer in `_pub_telemetry()` | **HIGH** | 930 | Use `snprintf()` with size check |
| 7 | Typo: `_endRun_notofications()` should be `_endRun_notifications()` | **LOW** | 1054 | Rename function (cosmetic) |
| 8 | Unsafe `sprintf()` in `sendReset()` with variable-length header | **HIGH** | 1008 | Use fixed buffer: `char temp[60]` |
| 9 | `sprintf()` into static 100-byte buffer in multiple commands | **HIGH** | 380–500+ | Replace all with `snprintf(msg, sizeof(msg), ...)` |
| 10 | Unsafe variable-length array `char t[x]` in `pub_state()` | **MEDIUM** | 915 | Use fixed size: `char t[100]` |

---

## DETAILED ANALYSIS

### **1. TOPICS: Definition & Storage**

#### Current Problems:

| Problem | Location | Impact |
|---------|----------|--------|
| Dynamic allocation without cleanup | `_add_topic()` line 919–921 | Memory leaks on long runtimes |
| No max topic count validation | Add functions | Buffer overflow if > array size |
| Topics stored as global arrays | Header file | No encapsulation, global state |
| Duplicate concat logic | `_concate()` line 343 | Code duplication in pub functions |
| No topic validation/sanitization | Add functions | Invalid MQTT topic names accepted |
| Counter overflow risk | `_sub/pub_topic_counter` | No bounds check on increment |

#### Recommended Implementation:

```cpp
// Use encapsulated manager instead of globals
struct TopicManager {
  static const uint8_t MAX_TOPICS = 20;
  const char* topics[MAX_TOPICS];
  uint8_t count = 0;
  
  bool add(const char* topic) {
    if (count >= MAX_TOPICS) {
      PRNTL("[Error]: Topic limit reached");
      return false;
    }
    
    uint8_t len = strlen(topic);
    char* t = new char[len + 1];
    strcpy(t, topic);
    topics[count++] = t;
    return true;
  }
  
  void cleanup() {
    for (uint8_t i = 0; i < count; i++) {
      if (topics[i]) delete[] topics[i];
    }
    count = 0;
  }
  
  ~TopicManager() { cleanup(); }  // Destructor ensures cleanup
};
```

---

### **2. TIME: Handling & Timestamps**

#### Current Problems:

| Problem | Location | Impact |
|---------|----------|--------|
| `sprintf()` into unbounded buffers | Lines 195, 201 | Buffer overflow if timestamp overruns |
| NTP hardcoded timezone (Asia/Jerusalem) | Line 195 | Not portable; hardcoded region |
| Epoch timestamp format inconsistent | Various places | Mixing seconds since boot vs Unix time |
| No time sync validation before use | `get_timeStamp()` | Stale/incorrect time gets published |
| Telemetry timestamp mismatch | Line 930 | Log entries use inconsistent formats |
| No time update callback | Line 197 | Can't alert app when NTP syncs |

#### Recommended Implementation:

```cpp
// At top of file, define constants
#define TIMESTAMP_BUFFER_SIZE 30
#define TIME_DELTA_BUFFER_SIZE 20

// Safe timestamp generation with size parameter
void myIOT2::get_timeStamp(char ret[], size_t ret_size, time_t t)
{
  if (!isTimeValid()) {
    snprintf(ret, ret_size, "TIME_SYNC_PENDING");
    return;
  }
  
  if (t == 0) t = now();
  struct tm *tm = localtime(&t);
  
  snprintf(ret, ret_size, "%04d-%02d-%02d %02d:%02d:%02d", 
    tm->tm_year + 1900, tm->tm_mon + 1, tm->tm_mday, 
    tm->tm_hour, tm->tm_min, tm->tm_sec);
}

// Safe time delta calculation
void myIOT2::convert_epoch2clock(long t1, long t2, 
                                  char time_str[], size_t ts_size,
                                  char days_str[], size_t ds_size)
{
  uint8_t days = 0, hours = 0, minutes = 0, seconds = 0;

  const uint8_t sec2minutes = 60;
  const int sec2hours = sec2minutes * 60;
  const int sec2days = sec2hours * 24;
  long time_delta = t1 - t2;

  days = (int)(time_delta / sec2days);
  hours = (int)((time_delta - days * sec2days) / sec2hours);
  minutes = (int)((time_delta - days * sec2days - hours * sec2hours) / sec2minutes);
  seconds = (int)(time_delta - days * sec2days - hours * sec2hours - minutes * sec2minutes);
  
  if (days_str != nullptr) {
    snprintf(days_str, ds_size, "%01dd", days);
  }
  snprintf(time_str, ts_size, "%02d:%02d:%02d", hours, minutes, seconds);
}

// Validate time is set
bool myIOT2::isTimeValid() {
  return now() > 1640803233;  // After Jan 1, 2022
}
```

---

### **3. MESSAGES: Publishing & Operations**

#### Current Problems:

| Problem | Location | Impact |
|---------|----------|--------|
| Inconsistent buffer sizes | Lines 379–500+ | 50, 100, 200 byte buffers mixed |
| `sprintf()` overflow risk everywhere | Throughout `_MQTTcb()` | Any long command crashes device |
| No message validation/sanitization | Line 365–373 | Malformed payloads unchecked |
| MQTT callback too large & monolithic | Lines 358–507 | 150+ lines of if-else chains, hard to maintain |
| No retry logic on pub failure | `pub_msg()`, etc. | Failed publishes silently ignored |
| Message queue not implemented | Throughout | If MQTT disconnects, messages lost |
| Hard-coded MQTT buffer scaling | Line 929 `mqtt_len` | Dynamic resizing wastes memory |

#### Recommended Implementation:

```cpp
// Define message constants at top
#define MSG_BUFFER_MAX 256
#define CMD_BUFFER_MAX 64
#define TOPIC_BUFFER_MAX 100

// Safe publisher with validation
void myIOT2::pub_safe(const char *topic, const char *msg)
{
  if (!isMqttConnected()) {
    if (useSerial) PRNTL("[Warn]: MQTT not connected, message dropped");
    return;
  }
  
  if (strlen(topic) >= TOPIC_BUFFER_MAX || strlen(msg) >= MSG_BUFFER_MAX) {
    PRNTL("[Error]: Message or topic exceeds max length");
    return;
  }
  
  _pub_generic(topic, msg, false);
}

// Command dispatch table (replaces 150-line if-else)
typedef void (*CommandHandler)(char *incoming_msg);

void cmd_ota(char *msg) {
  char response[MSG_BUFFER_MAX];
  snprintf(response, sizeof(response), "OTA allowed for %d seconds", 
           OTA_upload_interval * MS2MINUTES / 1000);
  pub_msg(response);
  allowOTA_clock = millis();
}

void cmd_ver(char *msg) {
  char response[MSG_BUFFER_MAX];
  snprintf(response, sizeof(response), "[ver]: IOTlib: [%s]", ver);
  pub_msg(response);
}

// ... other command handlers ...

const struct {
  const char *cmd;
  CommandHandler handler;
} commands[] = {
  {"ota", cmd_ota},
  {"reset", [](char *m) { sendReset("MQTT"); }},
  {"ver", cmd_ver},
  {"services", cmd_services},
  {"help", cmd_help},
  {"network", cmd_network},
  {"free_mem", cmd_free_mem},
  {"topics", cmd_topics},
  {nullptr, nullptr}
};

// Process incoming MQTT - safe version
void myIOT2::_MQTTcb(char *topic, uint8_t *payload, unsigned int length)
{
  // Validate length
  if (length >= CMD_BUFFER_MAX) {
    PRNTL("[Error]: MQTT message too long, dropped");
    return;
  }
  
  // Safe copy with null terminator
  char incoming_msg[CMD_BUFFER_MAX];
  memcpy(incoming_msg, payload, length);
  incoming_msg[length] = '\0';
  
  PRNT(F("Message arrived ["));
  PRNT(topic);
  PRNT(F("] "));
  PRNTL(incoming_msg);
  
  // Dispatch to registered handler
  for (int i = 0; commands[i].cmd != nullptr; i++) {
    if (strcmp(incoming_msg, commands[i].cmd) == 0) {
      commands[i].handler(incoming_msg);
      return;
    }
  }
  
  // Unknown command -> pass to external handler
  num_p = inline_read(incoming_msg);
  if (num_p > 1 && strcmp(inline_param[0], "update_flash") == 0 && useFlashP) {
    _cmdline_flashUpdate(inline_param[1], inline_param[2]);
  } else {
    ext_mqtt(incoming_msg, topic);
  }
}
```

---

## SUMMARY: Key Fixes by Priority

### **Priority 1 (CRITICAL - Do First):**
- [ ] Fix MQTT buffer overflow: Add bounds check in loop at line 369
- [ ] Replace all `sprintf()` → `snprintf()` in MQTT callback (lines 380–500+)
- [ ] Fix `strcpy()` overflow in `start_services()` (lines 9–13)

### **Priority 2 (HIGH - Do Soon):**
- [ ] Add size bounds to `get_timeStamp()` and `convert_epoch2clock()`
- [ ] Replace WiFi handler `sprintf()` with `snprintf()` (lines 131, 136)
- [ ] Implement topic bounds checking (line 919–921)
- [ ] Fix `sendReset()` buffer (line 1008)

### **Priority 3 (MEDIUM - Refactor):**
- [ ] Replace `_MQTTcb()` if-else chain with dispatch table
- [ ] Implement `TopicManager` class for encapsulation
- [ ] Add cleanup/destructor for memory management
- [ ] Define global buffer size constants at top

### **Priority 4 (LOW - Polish):**
- [ ] Fix typo: `notofications` → `notifications`
- [ ] Add time sync validation check
- [ ] Add error logging for failed operations

---

## Implementation Notes

**Scope:** These are issues in the **external library** (`myIOT2`). Your `main.cpp` is already refactored well.

**Recommendation:** Contact library maintainer or create a patched version in your project. These fixes are **non-breaking** and improve safety without changing API.

**Testing:** After fixes, test with:
- Long MQTT messages (>50 chars) to verify no crashes
- Rapid publish/subscribe cycles
- NTP sync delay scenarios
