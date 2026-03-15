#ifndef UPLOAD_CERTS_H
#define UPLOAD_CERTS_H

// certs.h includes sensitive data and has to be created by hand and cannot be commited to the code base. 
// See the Makefile to see how the data was retrieved.
#include "certs.h"


// Helper to upload a file to the SIM7080G internal storage
bool uploadCertToModem(TinyGsm& modem, const char* filename, const char* content) {
  int attempts = 0;
  const int max_attempts = 10;
  bool success = false;

  while (attempts < max_attempts && !success) {
    Serial.printf("Attempting to upload %s (Attempt %d)...\n", filename, attempts + 1);

    // 1. Initialize File System
    modem.sendAT("+CFSINIT");
    if (modem.waitResponse() != 1) { // 1 = OK
      attempts++;
      delay(500);
      continue;
    }

    // 2. Write File: AT+CFSWFILE=mode,filename,location,filesize,timeout
    // mode 0: create/overwrite, location 0: internal flash
    modem.sendAT("+CFSWFILE=0,\"", filename, "\",0,", strlen(content), ",10000");
    delay(200);
    // 3. Stream the actual certificate content to the modem
    modem.stream.print(content);
    modem.stream.write(26);

    // Check if the write was successful
    if (modem.waitResponse(10000) == 1) {
      Serial.println("Upload successful!");
      success = true;
    } else {
      Serial.println("Upload failed.");
      attempts++;
    }

    // 4. Terminate File System
    modem.sendAT("+CFSTERM");
    modem.waitResponse();
    delay(500);
  }

  return success;
}

bool upload_all_certs(TinyGsm& modem){

  bool all_pass = true;

  if (uploadCertToModem(modem, "AmazonRootCA1.pem", rootCA)) {
    Serial.println("Root CA uploaded.");
  } else {
    Serial.println("Root CA upload failed.");
    all_pass = false;
  }
  
  if (uploadCertToModem(modem, "device.crt", clientCert)) {
    Serial.println("Client Cert uploaded.");
  } else {
    Serial.println("Client Cert upload failed.");
    all_pass = false;
  }
  
  if (uploadCertToModem(modem, "device.key", clientKey)) {
    Serial.println("Client Key uploaded.");
  } else {
    Serial.println("Client Key upload failed.");
    all_pass = false;
  }
  return all_pass;
}



#endif
