#include <Arduino.h>
#include <myIOT2.h>

// #define JSON_DOC_SIZE 1200
// #define ACT_JSON_DOC_SIZE 800
// #define READ_PARAMTERS_FROM_FLASH true /* Flash or HardCoded Parameters */
// #define veboseMode true
#define use_PSU_state false

#define RESET_CMD_GPIO 5
#define POWER_CMD_GPIO 4
#define GET_MOTHERBOARD_ON_STATE 12
#define GET_MOTHERBOARD_POWER_STATE 13

#define LONG_PRESS_MS 4500
#define SHORT_PRESS_MS 1000

#define RESET_DURATION_MS SHORT_PRESS_MS
#define POWEROFF_DURATION_MS LONG_PRESS_MS
#define POWERON_DURATION_MS SHORT_PRESS_MS

#define SWITCH_ON HIGH
#define SWITCH_OFF !SWITCH_ON

myIOT2 iot;
enum SystemStates
{
  MOTHERBOARD_NOT_POWERED = 0,
  MOTHERBOARD_POWERED = 1,
  MOTHERBOARD_ON = 2,
  ERROR_STATE = 3
};
SystemStates systemState = MOTHERBOARD_NOT_POWERED;
const char *systemStateStr[] = {"NOT_POWERED", "POWERED", "ON", "ERROR"};
const char *verApp = "iot_ControlPC_v0.1";

// ~~~~~~~ Init & loop functions ~~~~~~~
void generic_Press_cmd(uint8_t gpio, int press_duration)
{
  digitalWrite(gpio, SWITCH_ON);
  delay(press_duration);
  digitalWrite(gpio, SWITCH_OFF);
}
bool get_powerSW_state()
{
  return digitalRead(GET_MOTHERBOARD_POWER_STATE);
}
bool get_resetSW_state()
{
  return digitalRead(GET_MOTHERBOARD_ON_STATE);
}
bool send_PowerON_cmd()
{
  if (get_powerSW_state() == SWITCH_OFF && systemState == MOTHERBOARD_POWERED)
  {
    generic_Press_cmd(POWER_CMD_GPIO, POWERON_DURATION_MS);
    return true;
  }
  else
  {
    return false;
  }
}
bool send_PowerOFF_cmd()
{
  if (get_powerSW_state() == SWITCH_OFF && systemState == MOTHERBOARD_ON)
  {
    generic_Press_cmd(POWER_CMD_GPIO, POWEROFF_DURATION_MS);
    return true;
  }
  else
  {
    return false;
  }
}
bool send_Reset_cmd()
{
  if (get_resetSW_state() == SWITCH_OFF && (systemState == MOTHERBOARD_ON || systemState == MOTHERBOARD_POWERED))
  {
    generic_Press_cmd(RESET_CMD_GPIO, SHORT_PRESS_MS);
    systemState = MOTHERBOARD_ON;
    return true;
  }
  else
  {
    return false;
  }
}
bool get_motherboard_ON_state()
{
  return digitalRead(GET_MOTHERBOARD_ON_STATE);
}
bool get_motherboard_POWER_state()
{
  return digitalRead(GET_MOTHERBOARD_POWER_STATE);
}

void init_GPIOs()
{
  pinMode(RESET_CMD_GPIO, OUTPUT);
  pinMode(POWER_CMD_GPIO, OUTPUT);
  pinMode(GET_MOTHERBOARD_ON_STATE, INPUT_PULLUP);
  pinMode(GET_MOTHERBOARD_POWER_STATE, INPUT_PULLUP);
}
void calc_system_state()
{
  if (get_motherboard_ON_state() == SWITCH_ON)
  {
    if (get_motherboard_POWER_state() == SWITCH_ON)
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
    if (get_motherboard_POWER_state() == SWITCH_ON)
    {
      systemState = MOTHERBOARD_POWERED;
    }
    else
    {
      systemState = MOTHERBOARD_NOT_POWERED;
    }
  }
}

// ~~~~~~~ Create iot instance ~~~~~~~
void extMQTT(char *incoming_msg, char *_topic)
{
  char msg[270];
  if (strcmp(incoming_msg, "status") == 0)
  {
    sprintf(msg, "[Status]: %s", systemStateStr[systemState]);
    iot.pub_msg(msg);
  }
  else if (strcmp(incoming_msg, "help2") == 0)
  {
    sprintf(msg, "help #2:{status; help2; ver2; poweroff; poweron; reset}");
    iot.pub_msg(msg);
  }
  else if (strcmp(incoming_msg, "ver2") == 0)
  {
    sprintf(msg, "ver #2: %s", verApp);
    iot.pub_msg(msg);
  }
  else if (strcmp(incoming_msg, "poweroff") == 0)
  {
    if (send_PowerOFF_cmd())
    {
      sprintf(msg, "[Commands]: PowerOFF command sent successfully");
    }
    else
    {
      sprintf(msg, "[Commands]: PowerOFF command failed");
    }
    iot.pub_msg(msg);
  }
  else if (strcmp(incoming_msg, "poweron") == 0)
  {
    if (send_PowerON_cmd())
    {
      sprintf(msg, "[Commands]: PowerON command sent successfully");
    }
    else
    {
      sprintf(msg, "[Commands]: PowerON command failed");
    }
    iot.pub_msg(msg);
  }
  else if (strcmp(incoming_msg, "reset") == 0)
  {
    if (send_Reset_cmd())
    {
      sprintf(msg, "[Commands]: Reset command sent successfully");
    }
    else
    {
      sprintf(msg, "[Commands]: Reset command failed");
    }
    iot.pub_msg(msg);
  }
  else if (strcmp(incoming_msg, "debug") == 0)
  {
    sprintf(msg, "[debug]: ON=%s, POWER=%s, POWER_SW=%s, RESET_SW=%s", get_motherboard_ON_state() == SWITCH_ON ? "ON" : "OFF", get_motherboard_POWER_state() == SWITCH_ON ? "ON" : "OFF", get_powerSW_state() == SWITCH_ON ? "ON" : "OFF", get_resetSW_state() == SWITCH_ON ? "ON" : "OFF");
    iot.pub_debug(msg);
  }
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
  calc_system_state();
  iot.looper();
  delay(50);
}