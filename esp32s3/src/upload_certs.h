#ifndef UPLOAD_CERTS_H
#define UPLOAD_CERTS_H

// certs.h includes sensitive data and has to be created by hand and cannot be commited to the code base. 
// See the Makefile to see how the data was retrieved.
#include "certs.h"
#include <LittleFS.h>
#include "AT_utils.h" // run_AT_cmd and wait_respose_wrap

void prepareCertificates() {
    Serial.println("prepareCertificates");

    // Pass 'true' to format the LittleFS partition if it's corrupted
    if (!LittleFS.begin(true)) { 
        Serial.println("LittleFS Mount Failed. Formatting...");
        if(!LittleFS.format() || !LittleFS.begin()){
             Serial.println("CRITICAL: LittleFS Format failed.");
             Serial.println("FAILED -----------\n\n");
             return;
        }
    }

    // 1. Ensure LittleFS is mounted
    if (!LittleFS.begin(true)) {
        Serial.println("LittleFS Mount Failed");
        Serial.println("FAILED -----------\n\n");
        return;
    }

    // 2. Define your files and their content
    const char* files[] = {"/AmazonRootCA1.pem", "/device.crt", "/device.key"};
    const char* data[] = {rootCA, clientCert, clientKey};

    // 3. Guarded write
    for (int i = 0; i < 3; i++) {
        if (!LittleFS.exists(files[i])) {
            Serial.printf("File %s not found. Writing to LittleFS...\n", files[i]);
            fs::File f = LittleFS.open(files[i], FILE_WRITE);
            if (f) {
                f.print(data[i]);
                f.close();
                Serial.printf("Successfully wrote %s\n", files[i]);
            } else {
                Serial.printf("Failed to create file %s\n", files[i]);
            }
        } else {
            Serial.printf("File %s already exists. Skipping.\n", files[i]);
        }
    }
    Serial.println("OK -----------\n\n");
}

void syncTime(TinyGsm& modem) {
    Serial.println("syncTime");
    Serial.println("Setting NTP Server...");
    run_AT_cmd(modem, "+CNTP=\"pool.ntp.org\",0"); // Set NTP server

    Serial.println("Syncing...");
    run_AT_cmd(modem, "+CNTP"); // Start sync

    delay(2000); // Wait for update

    if (run_AT_cmd(modem, "+CCLK?")){
        Serial.println("OK -----------\n\n");
    } else {
        Serial.println("FAILED -----------\n\n");
    }
}

bool setup_ssl(TinyGsm& modem) {
    Serial.println("setup_ssl");
    //syncTime(modem);
    // Enable automatic time zone update from tower
    run_AT_cmd(modem, "+CTZU=1", 5000); 

    // 1. CLEAR the index first!
    run_AT_cmd(modem, "+CSSLCFG=\"sslversion\",0,3"); // TLS 1.2

    // 1. Force-close any hanging SSL/MQTT connections
    run_AT_cmd(modem, "+SMDISC"); // Disconnect MQTT
    
    run_AT_cmd(modem, "+SHDISC"); // Disconnect HTTP/SSL
    
    // 2. Now try the delete again
    for (int i=0; i<3; i++){
        if (!run_AT_cmd(modem, "+CSSLCFG=\"del\",0")){
           Serial.println("Still failed to clear... trying a full SSL reset.");

           // 1. Force the modem to detach from the network (Soft Reset)
           run_AT_cmd(modem, "+CFUN=0", 5000);

           // 2. Re-enable the RF and Stack
           run_AT_cmd(modem, "+CFUN=1", 5000);

           // This resets the entire SSL RAM bank
           run_AT_cmd(modem, "+CSSLCFG=\"factory\"");

           delay(1000);
        } else {
            Serial.println("Succeded to clear");
            break;
        }
    }
    delay(2000);

    bool all_found = true;
    for (int i=0; i<1; i++){
        all_found = true;        
        if (!run_AT_cmd(modem, "+CSSLCFG=\"clientcert\",0,\"U:/device.crt\"")){
            if (!run_AT_cmd(modem, "+CSSLCFG=\"clientcert\",0,\"device.crt\"")){
                all_found = false;
            }
        }
        delay(2000);
        
        if (!run_AT_cmd(modem, "+CSSLCFG=\"clientkey\",0,\"U:/device.key\"")){
            if (!run_AT_cmd(modem, "+CSSLCFG=\"clientkey\",0,\"device.key\"")){
                all_found = false;
            } 
        }
        delay(2000);

        if (!run_AT_cmd(modem, "+CSSLCFG=\"cacert\",0,\"U:/AmazonRootCA1.pem\"")){
            if (!run_AT_cmd(modem, "+CSSLCFG=\"cacert\",0,\"AmazonRootCA1.pem\"")){
                all_found = false;
            } 
        }
        if (all_found) break;
        delay(2000);
    }
    if (all_found){
        Serial.println("OK -----------\n\n");
        return true;
    }
    Serial.println("FAILED -----------\n\n");
    return false;
}

bool check_ssl(TinyGsm& modem){
    Serial.println("check_ssl");
    // Check if the SSL configuration is accepted
    if (!run_AT_cmd(modem, "+CSSLCFG=\"clientcert\",0")){
      Serial.println("SSL Config Missing!");
      Serial.println("FAILED -----------\n\n");
      return false;
    }
    Serial.println("SSL Config OK!");
    Serial.println("OK -----------\n\n");
    return true;
} 

bool setup_mqtt(TinyGsm& modem){
    Serial.println("setup_mqtt");
    // Only proceed to connect if SSL is configured
    Serial.println("Initiating MQTT...");

    // 1. Actually open the connection
    if(run_AT_cmd(modem, "+SMCONN", 15000)){
        Serial.println("Connected to AWS IoT!");
        Serial.println("OK -----------\n\n");
        return true;
    } 
    Serial.println("Connection failed - Check Credentials/Policy");
    Serial.println("FAILED -----------\n\n");
    return false;
}

#endif
