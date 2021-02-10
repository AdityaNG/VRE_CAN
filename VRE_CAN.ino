#include <Arduino.h>
#include <ESP32CAN.h>
#include <CAN_config.h>


String FAULTS[8] = {"Weak Cell", "Low Cell Voltage", "Weak Cell", "Open Cell Voltage", "Weak Pack", "Thermistor", "High Voltage Isolation", "Internal Logic Fault"};
String SIGNAL[5] = {"Ready Power Signal", "Charge Power Signal", "Depleted", "Balancing Active", "12V Power Supply Fault"};
const int FLT = 1, MSG = 2;

String get_message(byte a, int typ) {
  String RES = "";
  int n = 8;
  if (typ==MSG) {
    n = 5;
  }
  for (int i=0; i<n; i++) {
    byte b = a - B10 * (a>>1);
    a = a>>1;
    if (b==B1) {
      if (typ==FLT) {
        RES +=  FAULTS[i] + " | ";
      } else if (typ==MSG) {
        RES +=  SIGNAL[i] + " | ";
      }
    }
  }
  return RES;
}

CAN_device_t CAN_cfg;               // CAN Config
unsigned long previousMillis = 0;   // will store last time a CAN Message was send
const int interval = 1000;          // interval at which send CAN Messages (milliseconds)
const int rx_queue_size = 10;       // Receive Queue size

void setup() {
  Serial.begin(115200);
  Serial.println("Basic Demo - ESP32-Arduino-CAN");
  CAN_cfg.speed = CAN_SPEED_250KBPS;
  CAN_cfg.tx_pin_id = GPIO_NUM_2
  ;
  CAN_cfg.rx_pin_id = GPIO_NUM_4;
  CAN_cfg.rx_queue = xQueueCreate(rx_queue_size, sizeof(CAN_frame_t));
  // Init CAN Module
  ESP32Can.CANInit();
}

void loop() {

  CAN_frame_t rx_frame;

  unsigned long currentMillis = millis();

  // Receive next CAN frame from queue
  if (xQueueReceive(CAN_cfg.rx_queue, &rx_frame, 3 * portTICK_PERIOD_MS) == pdTRUE) {

    if (rx_frame.FIR.B.FF == CAN_frame_std) {
      //printf("New standard frame");
    }
    else {
      //printf("New extended frame");
    }

    if (rx_frame.FIR.B.RTR == CAN_RTR) {
      //printf(" RTR from 0x%08X, DLC %d\r\n", rx_frame.MsgID,  rx_frame.FIR.B.DLC);
    }
    else {
      //printf(" from 0x%08X, DLC %d, Data ", rx_frame.MsgID,  rx_frame.FIR.B.DLC);
      for (int i = 0; i < rx_frame.FIR.B.DLC; i++) {
        //printf("0x%02X ", rx_frame.data.u8[i]);
      }
      //printf("\n");
    }
  
    uint32_t ID = rx_frame.MsgID;
    byte *data = (byte*) malloc(rx_frame.FIR.B.DLC * sizeof(byte));
    String res = "";
    bool fault = false;
    for (int i=0; i<rx_frame.FIR.B.DLC; i++)
      data[i] = rx_frame.data.u8[i];
    
    if (ID==0x0A) {
      float current, volt, volt_12, SOC, DOD;
      current = (data[0] * B10000000 *2) + data[1]; // In amps;
        volt = data[2]; // Volts
        volt_12 = data[3] / 10.0; // Divide by 10 
        SOC = data[6];
        DOD = data[7];
        res = "0x0A|| current : " + String(current) + " | " + "volt : " + String(volt) + " | " + "volt_12 : " + String(volt_12) + " | " + "SOC : " + String(SOC) + " | " + "DOD : " + String(DOD) + " | ";
        if (data[4]!=0) { // CRITICAL
          fault = true;
          res+=get_message(data[4], FLT) +" | ";
        }
        if (data[5]!=0) { // NON CRITICAL
          res+=get_message(data[5], MSG) +" | ";
        }
        printf("0x0A|| current - %f | volt : %f | volt_12 : %f | SOC : %f | DOD : %f \n", current, volt, volt_12, SOC, DOD);
    } else if (ID==0x0B) {
        float resistance, DOD, summed_volt, avg_temp, health, low_volt_id, high_volt_id;
        resistance = data[0] / 1000.0; //Divide by 1000 
        health = data[1];
        summed_volt = (data[2] * B10000000 *2) + data[3]; // V
        avg_temp = data[4]; 
        low_volt_id = data[6];
        high_volt_id = data[7];
        res = "0x0B|| resistance : " + String(resistance) + " | " + "health : " + String(health) + " | " + "summed_volt : " + String(summed_volt) + " | " + "avg_temp : " + String(avg_temp) + " | " + "low_volt_id : " + String(low_volt_id) + " | " + "high_volt_id : " + String(high_volt_id) + " | ";
        printf("0x0A|| resistance - %f | health : %f | summed_volt : %f | avg_temp : %f | low_volt_id : %f | high_volt_id : %f \n", resistance, health, summed_volt, avg_temp, low_volt_id, high_volt_id);
    } else if (ID==0x0C) {
        float amp_hrs, DOD, CCL, DCL;
        amp_hrs = ((data[0] * B10000000 *2)  + data[1] )/10.0; // /10
        CCL = (data[2] * B10000000 *2) + data[3]; // V
        DCL = (data[4] * B10000000 *2) + data[5]; // V
        res = "0x0C|| amp_hrs : " + String(amp_hrs) + " | " + "CCL : " + String(CCL) + " | " + "DCL : " + String(DCL) + " | ";
        printf("0x0C|| amp_hrs - %f | CCL : %f | DCL : %f \n", amp_hrs, CCL, DCL);
    } else if (ID==0x0D) {
      float pack_id, internal_volt, resistance, open_volt; 
      pack_id = (int)data[0];
      internal_volt = ((data[1] * B10000000 *2) + data[3])/10000.0;
      resistance = ((data[3] * B10000000 *2) + data[4])/10000.0;
      open_volt = ((data[5] * B10000000 *2) + data[6])/10000.0;
      //res = "0x0D|| pack_id -" + String(pack_id) + " | " + "internal_volt : " + String(internal_volt) + " | " + "resistance : " + String(resistance) + " | " + "open_volt : " + String(open_volt) + " | "; 
      //printf("%s\n", res);
      //printf("0x0D|| pack_id - %f | internal_volt : %f | resistance : %f | open_volt : %f \n", pack_id, internal_volt, resistance, open_volt);
    }

    //printf("%s\n", res);

    free(data);
  }
  
}
