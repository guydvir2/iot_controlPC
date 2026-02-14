#include <Arduino.h>
#include <myIOT2.h>

#define JSON_DOC_SIZE 1200
#define ACT_JSON_DOC_SIZE 800
#define READ_PARAMTERS_FROM_FLASH true /* Flash or HardCoded Parameters */
#define veboseMode true

#define RESET_GPIO 5
#define POWER_GPIO 4
#define SHORT_PRESS_MS 1000
#define LONG_PRESS_MS 4500
#define RESET_DURATION_MS SHORT_PRESS_MS
#define POWEROFF_DURATION_MS LONG_PRESS_MS
#define POWERON_DURATION_MS SHORT_PRESS_MS

#define SWITCH_ON HIGH
#define SWITCH_OFF !SWITCH_ON

myIOT2 iot;

const char *verApp = "iot_ControlPC_v0.1";

bool firstLoop = true;
bool bootSucceeded = false;

// ~~~~~~~ Create iot instance ~~~~~~~
void extMQTT(char *incoming_msg, char *_topic)
{
  char msg[270];
  if (strcmp(incoming_msg, "status") == 0)
  {
    // for (uint8_t i = 0; i < SW_inUse; i++)
    // {
    //   char msg2[160];

    //     else
    //     {
    //       iot.convert_epoch2clock((millis() - SW_Array[i]->telemtryMSG.clk_start) / 1000, 0, clk4);
    //       sprintf(msg2, "timeout: [%s], elapsed: [%s], remain: [%s], triggered: [%s]", "NA", clk4, "NA", trigs[SW_Array[i]->telemtryMSG.reason]);
    //     }

    //   iot.pub_msg(msg);
    // }
  }
  else if (strcmp(incoming_msg, "help2") == 0)
  {
    sprintf(msg, "help #2:{[i],on,[timeout],[pwm_percentage]},{[i],off}, {[i], add_time,[timeout]}, {[i], remain}, {[i], timeout}, {[i], elapsed}, {[i], show_params}");
    iot.pub_msg(msg);
  }
  else if (strcmp(incoming_msg, "ver2") == 0)
  {
    // sprintf(msg, "ver #2: %s %s", verApp, SW_Array[0]->ver);
    iot.pub_msg(msg);
  }
  else if (strcmp(incoming_msg, "show_configs") == 0)
  {
    // char dlist[200];
    // read_dirList(dlist);
    // sprintf(msg, "[Config_Dirs]: %s", dlist);
    // iot.pub_msg(msg);
  }
  else if (strcmp(incoming_msg, "show_params") == 0)
  {
    DynamicJsonDocument DOC(JSON_DOC_SIZE);

    // if (select_SWdefinition_src(DOC))
    // {
    //   char clk[25];
    //   char msg2[300];

    //   const int mqtt_size = iot.mqtt_len - 30;
    //   iot.get_timeStamp(clk, iot.now());
    //   sprintf(msg2, "\n~~~~~~ %s %s ~~~~~~", iot.topics_sub[0], clk);
    //   iot.pub_debug(msg2);

    //   serializeJson(DOC, msg2);
    //   uint8_t w = strlen(msg2) / mqtt_size;

    //   for (uint8_t i = 0; i <= w; i++)
    //   {
    //     int x = 0;
    //     char msg3[mqtt_size + 5];
    //     for (int n = i * mqtt_size; n < i * mqtt_size + mqtt_size; n++)
    //     {
    //       msg3[x++] = msg2[n];
    //     }
    //     msg3[x] = '\0';
    //     iot.pub_debug(msg3);
    //   }
    //   iot.pub_msg("[Paramters]: Published in debug");
    // }
  }
  else
  {
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

// ~~~~~~~ Init & loop functions ~~~~~~~
void generic_Press_cmd(uint8_t gpio, int press_duration)
{
  digitalWrite(gpio, SWITCH_ON);
  delay(press_duration);
  digitalWrite(gpio, SWITCH_OFF);
}
void send_PowerON_cmd()
{
  generic_Press_cmd(POWER_GPIO, SHORT_PRESS_MS);
}
void send_PowerOFF_cmd()
{
  generic_Press_cmd(POWER_GPIO, LONG_PRESS_MS);
}
void send_Reset_cmd()
{
  generic_Press_cmd(RESET_GPIO, SHORT_PRESS_MS);
}

bool get_Motherboard_ON_state(){
  
}
void startService()
{
  start_iot2();
}

//~~~~~~~ Sketch Main ~~~~~~~

void setup()
{
  Serial.begin(115200);
  startService();
}
void loop()
{
  iot.looper();
  delay(50);
}