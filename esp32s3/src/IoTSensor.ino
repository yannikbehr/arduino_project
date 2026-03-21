#include <Arduino.h>
#include "setup.h"
#include "upload_certs.h"
// copied tools from 
// https://github.com/Xinyuan-LilyGO/LilyGo-T-SIM7080G/blob/master/examples/ModemFileUploadExample/Imagetofiletest.cpp
#include "liligo_tools.h"

TinyGsm modem(SerialAT);
int num_msg = 0;

void one_time_modem_fs_cleanup(TinyGsm& modem){
  // 1. Force the File System to release everything
  cmd_get_return(modem, "AT+CFSTERM");
  delay(1000);
  
  // 2. Format the User Storage (U: drive)
  // This is the only command that can break a UFS lock
  Serial.println("FORMATTING USER DRIVE...");
  cmd_get_return(modem, "AT+CFSFORMAT=0"); 
  delay(5000); // Give it a long time to wipe the flash sectors
  
  // 3. Restart and check
  cmd_get_return(modem, "AT+CFSINIT");
  String driveCheck = cmd_get_return(modem, "AT+CFSGFIS=0");
  
  if (driveCheck.indexOf("ERROR") == -1) {
      Serial.println("SUCCESS: Drive is back online!");
  } else {
      Serial.println("HARDWARE FAILURE: Flash is unresponsive.");
  }
}



void setup()
{
    Serial.begin(115200);

    PMU_setup();
    basic_modem_setup(modem);


    // 1. Kill the Radio immediately to release the Flash lock
  cmd_get_return(modem, "AT+CFUN=0");
  delay(2000);
  
  // 2. Now that the radio is OFF, try the resets
  Serial.println("RADIO OFF - Attempting Unlocks...");
  cmd_get_return(modem, "AT+SMDISC");
  cmd_get_return(modem, "AT+CSSLCFG=\"factory\"");
  cmd_get_return(modem, "AT+CSSLCFG=\"del\",0");
  
  // 3. Try to initialize and format the drive while OFFLINE
  cmd_get_return(modem, "AT+CFSTERM");


  // 1. Disable "Slow Clock" (This prevents the Flash from going into low-power mode)
  cmd_get_return(modem, "AT+CSCLK=0"); 
  
  // 2. Disable "Power Saving Mode" (PSM)
  cmd_get_return(modem, "AT+CPSMS=0");
  
  // 3. Disable "Extended Discontinuous Reception" (eDRX)
  cmd_get_return(modem, "AT+CEDRXS=0");
  
  delay(1000); // Give it a second to stabilize the voltage to the Flash chip



  cmd_get_return(modem, "AT+CFSINIT");
  String driveCheck = cmd_get_return(modem, "AT+CFSGFIS=0");
  
  if (driveCheck.indexOf("ERROR") != -1) {
      Serial.println("Still locked. Forcing Format while Radio is OFF...");
      cmd_get_return(modem, "AT+CFSFORMAT=0");
      delay(2000);
      cmd_get_return(modem, "AT+CFSINIT");
      cmd_get_return(modem, "AT+CFSGFIS=0");
  }
  
  // 4. ONLY AFTER THIS IS DONE, turn the radio back on
  cmd_get_return(modem, "AT+CFUN=1");

    // creating files on littleFS from certificates 
    // stored in certs.h
    prepareCertificates();

    send_all_certs_to_modem(modem);

    setup_ssl(modem);

    check_ssl(modem);

    connect_GSM_1nce(modem);

    configure_endpoint(modem);

    setup_mqtt(modem);
}


bool publishMqttData(TinyGsm& modem, const char* topic, const char* payload) {
    // 1. Construct the SMPUB command
    // Format: AT+SMPUB=<topic>,<payload_length>,<qos>,<retain>
    modem.sendAT("+SMPUB=\"", topic, "\",", strlen(payload), ",0,0");

    // 2. Wait for the ">" prompt from the modem
    // This indicates the modem is ready to receive the raw payload
    if (modem.waitResponse(5000, ">") != 1) {
        Serial.println("Failed to get ready prompt for SMPUB");
        return false;
    }

    // 3. Stream the payload directly to the modem
    modem.stream.print(payload);

    // 4. Terminate the stream with Ctrl+Z (ASCII 26)
    modem.stream.write(26);

    // 5. Wait for the final OK
    if (modem.waitResponse(10000) == 1) {
        Serial.println("Publish successful!");
        return true;
    } else {
        Serial.println("Publish failed.");
        return false;
    }
}

bool isConnected(TinyGsm& modem) {
  modem.sendAT("+SMSTATE?");
  // +SMSTATE: 1 means connected
  if (modem.waitResponse(1000, "+SMSTATE: 1") == 1) {
    return true;
  }
  return false;
}

bool safePublish(TinyGsm& modem, const char* payload) {
    if (!isConnected(modem)) {
        Serial.println("Connection lost. Resetting MQTT state...");

        // 1. Force disconnect to clear internal modem MQTT buffer
        modem.sendAT("+SMDISC");
        modem.waitResponse(5000);

        // 2. Re-connect
        modem.sendAT("+SMCONN");
        if (modem.waitResponse(20000) != 1) {
            Serial.println("SMCONN failed - check modem logs!");
            return false;
        }
        Serial.println("SMCONN successful!");
    }

    // 3. Proceed to publish
    return publishMqttData(modem, "devices/SIM7080G_01/data", payload);
}

void debugModemConfig(TinyGsm& modem) {
    Serial.println("--- MODEM CONFIG DUMP ---");
    
    // TinyGSM's sendAT() doesn't print the response, 
    // but you can read the response into the modem's internal buffer
    // and then print it if you know where it's stored.
    
    // The easiest way: Just send the AT command directly to the Serial port 
    // and read the result.
    
    const char* debugCmds[] = {
        "AT+SMCONF?", 
        "AT+CSSLCFG=\"cacert\",0", 
        "AT+CSSLCFG=\"clientcert\",0", 
        "AT+CSSLCFG=\"clientkey\",0"
    };

    for (const char* cmd : debugCmds) {
        Serial.print("Command: ");
        Serial.println(cmd);
        
        // Send the command directly through the modem stream
        modem.stream.println(cmd);
        
        // Wait for the response and print it to your Serial Monitor
        long start = millis();
        while (millis() - start < 2000) {
            if (modem.stream.available()) {
                Serial.write(modem.stream.read());
            }
        }
        Serial.println("\n-------------------------");
    }
}


void loop()
{
    Serial.print("loop ");
    debugModemConfig(modem);
    if (num_msg < 10){

      float temp = 1.1111;
      char payload[64];
      snprintf(payload, sizeof(payload), "{\"temp\": %.4f, \"msg\": %d}", temp, num_msg);

      if (safePublish(modem, payload)) {
          num_msg++;
      } else {
         Serial.println("Failed to send message");
      }
    }
    delay(30000);
}


















