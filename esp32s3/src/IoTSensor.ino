#include <Arduino.h>
#include "setup.h"
#include "upload_certs.h"
// copied tools from 
// https://github.com/Xinyuan-LilyGO/LilyGo-T-SIM7080G/blob/master/examples/ModemFileUploadExample/Imagetofiletest.cpp
#include "liligo_tools.h"

TinyGsm modem(SerialAT);
int num_msg = 0;

bool send_all_certs_to_modem(TinyGsm& modem) {
    const char* filenames[] = {"AmazonRootCA1.pem", "device.crt", "device.key"};
    
    // 1. Force a clean start
    modem.sendAT("+CFSTERM");
    modem.waitResponse(2000);
    modem.sendAT("+CFSINIT");
    if (modem.waitResponse(5000) != 1) return false;

    // 2. Force Upload (Ignore existence checks for now to rule them out)
    for (const char* fname : filenames) {
        fs::File f = LittleFS.open(String("/") + fname, FILE_READ);
        if (f) {
            Serial.printf("Uploading %s...\n", fname);
            // Re-use your robust LilyGO function, but remove the init/term from it
            // or just ensure your LilyGO function handles overwriting properly.
            sendFileToModem(modem, f, fname); 
            f.close();
            delay(1000); // Flash commit delay
        }
    }

    // 3. One-time Termination
    modem.sendAT("+CFSTERM");
    modem.waitResponse(5000);
    return true;
}

void setup()
{
    Serial.begin(115200);

    PMU_setup();
    basic_modem_setup(modem);

    // creating files on littleFS from certificates 
    // stored in certs.h
    prepareCertificates();

    connect_GSM_1nce(modem);

    send_all_certs_to_modem(modem);

    setup_ssl(modem);

    configure_endpoint(modem);
 
    // Check if the SSL configuration is accepted
    modem.sendAT("+CSSLCFG=\"clientcert\",0");
    if (modem.waitResponse(5000) != 1) 
      Serial.println("SSL Config Missing!");
    else Serial.println("SSL Config OK!");


    // Only proceed to connect if SSL is configured
    if (modem.waitResponse(5000) == 1) {
        Serial.println("SSL Config OK! Initiating MQTT...");

        // 1. Actually open the connection
        modem.sendAT("+SMCONN");
        if (modem.waitResponse(15000) == 1) {
            Serial.println("Connected to AWS IoT!");
        } else {
            Serial.println("Connection failed - Check Credentials/Policy");
        }
    }
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
    // Add this before modem.sendAT("+SMCONN");
    modem.sendAT("+CMEE=2"); // Enable verbose error reporting
    modem.waitResponse();
    Serial.println("Connection lost, reconnecting...");

    modem.sendAT("+CSSLCFG=\"clientcert\",0");
    if (modem.waitResponse(5000) != 1) {
        Serial.println("CRITICAL: SSL Config is still invalid!");
    }

    modem.sendAT("+SMCONN");
    // Change this to capture the response, not just wait
    if (modem.waitResponse(20000) != 1) { 
        Serial.println("SMCONN failed - check modem logs!");
        return false;
    }
    Serial.println("SMCONN successful!");
  }
  return publishMqttData(modem, "devices/SIM7080G_01/data", payload);
}

void loop()
{
    Serial.print("loop ");
    /*
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
    */
    delay(1000);
}


















