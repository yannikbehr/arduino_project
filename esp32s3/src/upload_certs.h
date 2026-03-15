#ifndef UPLOAD_CERTS_H
#define UPLOAD_CERTS_H

// certs.h includes sensitive data and has to be created by hand and cannot be commited to the code base. 
// See the Makefile to see how the data was retrieved.
#include "certs.h"
#include <LittleFS.h>

void prepareCertificates() {

    // Pass 'true' to format the LittleFS partition if it's corrupted
    if (!LittleFS.begin(true)) { 
        Serial.println("LittleFS Mount Failed. Formatting...");
        if(!LittleFS.format() || !LittleFS.begin()){
             Serial.println("CRITICAL: LittleFS Format failed.");
             return;
        }
    }

    // 1. Ensure LittleFS is mounted
    if (!LittleFS.begin(true)) {
        Serial.println("LittleFS Mount Failed");
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
}

bool setup_ssl(TinyGsm& modem){

    // Sync clock before SSL attempt
    modem.sendAT("+CNTP=1"); // Request network time
    modem.waitResponse();
    delay(2000); 
    modem.sendAT("+CCLK?"); // Verify the time is correct (not 1980)
    modem.waitResponse();

    // 1. Set SSL version to TLS 1.2
    modem.sendAT("+CSSLCFG=\"sslversion\",0,3");
    modem.waitResponse();
    
    // 2. Point the modem to the files we just uploaded
    // Add "U:/" prefix to the filenames
    modem.sendAT("+CSSLCFG=\"cacert\",0,\"AmazonRootCA1.pem\"");
    if (modem.waitResponse() != 1) {
        Serial.println("RootCA path error - check filesystem");
    }

    modem.sendAT("+CSSLCFG=\"clientcert\",0,\"device.crt\"");
    if (modem.waitResponse() != 1) {
        Serial.println("clientcert path error");
    }
    
    modem.sendAT("+CSSLCFG=\"clientkey\",0,\"device.key\"");
    if (modem.waitResponse() != 1) {
        Serial.println("clientkey path error");
    }
    return true;
}

// Helper function to upload a single cert with verification
bool uploadCertToModem(TinyGsm& modem, const char* filename, const char* content) {
    // 1. Delete existing file to prevent collision
    modem.sendAT("+CFSDELE=\"", filename, "\"");
    modem.waitResponse();

    // 2. Prepare the write command
    // Use length of content and allow 10s timeout
    modem.sendAT("+CFSWFILE=0,\"", filename, "\",0,", strlen(content), ",10000");

    // 3. Wait for the ">" prompt before streaming data
    if (modem.waitResponse(5000, ">") != 1) {
        Serial.printf("Failed to get ready prompt for %s\n", filename);
        return false;
    }

    // 4. Stream data and terminate with Ctrl+Z
    modem.stream.print(content);
    modem.stream.write(26);

    // 5. Verify the modem accepted the file
    if (modem.waitResponse(10000) != 1) {
        Serial.printf("Modem rejected file: %s\n", filename);
        return false;
    }
    return true;
}

bool upload_all_certs(TinyGsm& modem) {
    Serial.println("Initializing Modem Filesystem...");
    modem.sendAT("+CFSINIT");
    if (modem.waitResponse(5000) != 1) {
        Serial.println("FS Init failed! Modem may be busy or partitioned.");
        return false;
    }

    bool all_pass = true;

    // Array of files and their content pointers
    const char* filenames[] = {"AmazonRootCA1.pem", "device.crt", "device.key"};
    const char* contents[] = {rootCA, clientCert, clientKey};

    for (int i = 0; i < 3; i++) {
        Serial.printf("Attempting to upload %s...\n", filenames[i]);
        if (uploadCertToModem(modem, filenames[i], contents[i])) {
            Serial.printf("Successfully uploaded %s\n", filenames[i]);
        } else {
            Serial.printf("Failed to upload %s\n", filenames[i]);
            all_pass = false;
        }
    }

    // Terminate FS session after batch upload
    modem.sendAT("+CFSTERM");
    modem.waitResponse();

    return all_pass;
}

#endif
