#include <Arduino.h>
#include <myIOT2.h>

#define use_PSU_state false

#define RESET_CMD_GPIO 4
#define POWER_CMD_GPIO 12
#define GET_MOTHERBOARD_ON_STATE_GPIO 12
#define GET_MOTHERBOARD_POWER_STATE_GPIO 13

#define LONG_PRESS_MS 4500
#define SHORT_PRESS_MS 500

#define RESET_DURATION_MS SHORT_PRESS_MS
#define POWEROFF_DURATION_MS LONG_PRESS_MS
#define POWERON_DURATION_MS SHORT_PRESS_MS

#define OPEN_SW LOW
#define CLOSED_SW !OPEN_SW
#define ON_STATE HIGH
#define OFF_STATE LOW

unsigned long lastCalc = 0;
const unsigned long calcInterval = 50;
const char *verApp = "iot_ControlPC_v0.2";

typedef void (*CommandHandler)(void);

void cmd_status();
void cmd_help();
void cmd_ver();
void cmd_poweroff();
void cmd_poweron();
void cmd_reset();
void cmd_debug();

struct Command
{
  const char *name;
  CommandHandler handler;
} commands[] = {
    {"status", cmd_status},
    {"help2", cmd_help},
    {"ver2", cmd_ver},
    {"poweroff_cmd", cmd_poweroff},
    {"poweron_cmd", cmd_poweron},
    {"reset_cmd", cmd_reset},
    {"debug", cmd_debug},
    {nullptr, nullptr}};

myIOT2 iot;

enum SystemStates
{
  MOTHERBOARD_NOT_POWERED = 0,
  MOTHERBOARD_POWERED = 1,
  MOTHERBOARD_ON = 2,
  ERROR_STATE = 3,
  UNKNOWN = 4
};
SystemStates systemState = MOTHERBOARD_NOT_POWERED;

const char *systemStateStr[] = {"NOT_POWERED", "POWERED", "ON", "ERROR", "UNKNOWN"};

// ~~~~~~~ Init & loop functions ~~~~~~~
void generic_Press_cmd(uint8_t gpio, int press_duration)
{
  digitalWrite(gpio, CLOSED_SW);
  delay(press_duration);
  digitalWrite(gpio, OPEN_SW);
}
bool get_powerSW_state()
{
  return digitalRead(POWER_CMD_GPIO);
}
bool get_resetSW_state()
{
  return digitalRead(RESET_CMD_GPIO);
}
bool send_PowerON_cmd()
{
  if (get_powerSW_state() == OPEN_SW &&
          (use_PSU_state && systemState == MOTHERBOARD_POWERED) ||
      (!use_PSU_state && systemState == UNKNOWN))
  {
    generic_Press_cmd(POWER_CMD_GPIO, POWERON_DURATION_MS);
    return true;
  }
  return false;
}
bool send_PowerOFF_cmd()
{
  if (get_powerSW_state() == OPEN_SW &&
          (use_PSU_state && systemState == MOTHERBOARD_ON) ||
      (!use_PSU_state && systemState == UNKNOWN))
  {
    generic_Press_cmd(POWER_CMD_GPIO, POWEROFF_DURATION_MS);
    return true;
  }
  return false;
}
bool send_Reset_cmd()
{
  if ((get_resetSW_state() == OPEN_SW) &&
      (use_PSU_state && (systemState == MOTHERBOARD_ON || (use_PSU_state && systemState == MOTHERBOARD_POWERED)) ||
       (!use_PSU_state && systemState == UNKNOWN)))
  {
    generic_Press_cmd(RESET_CMD_GPIO, SHORT_PRESS_MS);
    return true;
  }
  return false;
}
bool get_motherboard_ON_state()
{
  return digitalRead(GET_MOTHERBOARD_ON_STATE_GPIO);
}
bool get_motherboard_POWER_state()
{
  return digitalRead(GET_MOTHERBOARD_POWER_STATE_GPIO);
}

void init_GPIOs()
{
  pinMode(RESET_CMD_GPIO, OUTPUT);
  pinMode(POWER_CMD_GPIO, OUTPUT);
  digitalWrite(RESET_CMD_GPIO, OPEN_SW);
  digitalWrite(POWER_CMD_GPIO, OPEN_SW);

#if use_PSU_state
  pinMode(GET_MOTHERBOARD_ON_STATE_GPIO, INPUT_PULLUP);
  pinMode(GET_MOTHERBOARD_POWER_STATE_GPIO, INPUT_PULLUP);
#endif
}
void calc_system_state()
{
  // When PSU state monitoring is enabled, use both ON and POWER sensors.
#if use_PSU_state
  if (get_motherboard_ON_state() == ON_STATE)
  {
    if (get_motherboard_POWER_state() == ON_STATE)
    {
      systemState = MOTHERBOARD_ON;
    }
    else
    {
      systemState = ERROR_STATE;
    }
  }
  else
  {
    if (get_motherboard_POWER_state() == ON_STATE)
    {
      systemState = MOTHERBOARD_POWERED;
    }
    else
    {
      systemState = MOTHERBOARD_NOT_POWERED;
    }
  }
#else
  // PSU monitoring disabled: determine ON vs NOT_POWERED from ON sensor only.
  // if (get_motherboard_ON_state() == ON_STATE)
  // {
  //   systemState = MOTHERBOARD_ON;
  // }
  // else
  // {
  //   systemState = MOTHERBOARD_NOT_POWERED;
  // }
  systemState = UNKNOWN;
#endif
}

// ~~~~~~~ Create iot instance ~~~~~~~
void cmd_status()
{
  char msg[50];
  snprintf(msg, sizeof(msg), "[Status]: %s", systemStateStr[systemState]);
  iot.pub_msg(msg);
}
void cmd_help()
{
  iot.pub_msg("help #2:{status; help2; ver2; poweroff_cmd; poweron_cmd; reset_cmd}");
}
void cmd_ver()
{
  char msg[50];
  snprintf(msg, sizeof(msg), "ver #2: %s", verApp);
  iot.pub_msg(msg);
}
void cmd_poweroff()
{
  iot.pub_msg(send_PowerOFF_cmd() ? "[Commands]: PowerOFF sent" : "[Commands]: PowerOFF failed");
}
void cmd_poweron()
{
  iot.pub_msg(send_PowerON_cmd() ? "[Commands]: PowerON sent" : "[Commands]: PowerON failed");
}
void cmd_reset()
{
  iot.pub_msg(send_Reset_cmd() ? "[Commands]: Reset sent" : "[Commands]: Reset failed");
}
void cmd_debug()
{
  char msg[100];
  snprintf(msg, sizeof(msg), "[debug]: ON=%s, POWER=%s, POWER_SW=%s, RESET_SW=%s",
           get_motherboard_ON_state() == CLOSED_SW ? "ON" : "OFF",
           get_motherboard_POWER_state() == CLOSED_SW ? "ON" : "OFF",
           get_powerSW_state() == OPEN_SW ? "ON" : "OFF",
           get_resetSW_state() == OPEN_SW ? "ON" : "OFF");
  iot.pub_msg(msg);
}

void extMQTT(char *incoming_msg, char *_topic)
{
  for (int i = 0; commands[i].name; i++)
    if (strcmp(incoming_msg, commands[i].name) == 0)
      return commands[i].handler();
}

void start_iot2()
{
  iot.useSerial = true;
  iot.useFlashP = false;
  iot.noNetwork_reset = 2;
  iot.ignore_boot_msg = false;

  // /* fail read from flash  values */
  const char *t[] = {"DvirHome/Messages", "DvirHome/log", "DvirHome/debug"};
  const char *t2[] = {"DvirHome/Device", "DvirHome/All"};
  const char *t3[] = {"DvirHome/Device/Avail", "DvirHome/Device/State"};

  for (uint8_t i = 0; i < 3; i++)
  {
    iot.add_gen_pubTopic(t[i]);
  }
  for (uint8_t i = 0; i < 2; i++)
  {
    iot.add_subTopic(t2[i]);
  }
  for (uint8_t i = 0; i < 2; i++)
  {
    iot.add_pubTopic(t3[i]);
  }

  // else
  // {
  //   for (uint8_t i = 0; i < (DOC["gen_pubTopic"].size() /*| 3*/); i++)
  //   {
  //     iot.add_gen_pubTopic(DOC["gen_pubTopic"][i] /*| t[i]*/);
  //   }
  //   for (uint8_t i = 0; i < (DOC["subTopic"].size() /*| 2*/); i++)
  //   {
  //     iot.add_subTopic(DOC["subTopic"][i] /*| t2[i]*/);
  //   }
  //   for (uint8_t i = 0; i < (DOC["pubTopic"].size() /*| 3*/); i++)
  //   {
  //     iot.add_pubTopic(DOC["pubTopic"][i] /*| t3[i]*/);
  //   }
  // }

  iot.start_services(extMQTT);
}

void startService()
{
  init_GPIOs();
  start_iot2();
}

//~~~~~~~ Sketch Main ~~~~~~~
void setup()
{
  startService();
}
void loop()
{
  unsigned long now = millis();
  if (now - lastCalc >= calcInterval)
  {
    lastCalc = now;
    calc_system_state();
  }

  iot.looper();
}