
#pragma once

#include <LittleFS.h>
#include "AT_utils.h" // run_AT_cmd and wait_respose_wrap

const size_t bufferSize = 10240; // 10KB buffer size
uint8_t buffer[bufferSize];

void fs_init(TinyGsm& modem, int retry=5){
    if (run_AT_cmd(modem, "+CFSINIT", 1000, retry)){
        Serial.println("fs_init: +CFSINIT Success");
    }
}

void fs_term(TinyGsm& modem, int retry=5){
    if (run_AT_cmd(modem, "+CFSTERM", 1000, retry)){
        Serial.println("fs_term: succeeded");
    }
}


void sendFileToModem(TinyGsm& modem, File file, const char* filename) {
    // CLEAR any hanging sessions from previous failed runs
    fs_term(modem);
    // 2. START session
    fs_init(modem);


    // Get the total size of the file
    size_t totalSize = file.size();
    size_t alreadySent = 0;
    bool firstChunk = true;

    // Loop for sending chunks
    while (totalSize > 0) {
        // Determine the size of the next chunk to send
        size_t chunkSize = std::min(totalSize, static_cast<size_t>(10000)); // Limit chunk size to 10,000 bytes

        // Prepare the file upload command
        String command = "AT+CFSWFILE=3,\"" + String(filename) + "\"," + String(firstChunk ? 0 : 1) + "," + String(chunkSize) + ",10000";
        String res = cmd_get_return(modem, command.c_str()); 

        if (res.indexOf("DOWNLOAD") != -1 || res.indexOf(">") != -1) {
            Serial.println("OK: Modem ready for upload.");
        } else {
            Serial.println("ABORT: Modem rejected the file open command.");
            return;
        }

        delay(50);

        // Write the chunk of data to the modem
        size_t bytesRead = file.read(buffer, std::min(chunkSize, bufferSize)); // Read chunkSize bytes from the file
        if (bytesRead > 0) {
            size_t bytesWritten = modem.stream.write(buffer, bytesRead); // Write the read data to the modem's stream
            if (bytesWritten != bytesRead) {
                Serial.println("Failed to write chunk to modem");
                return;
            }
            alreadySent += bytesWritten;
            totalSize -= bytesWritten;

            Serial.printf("Sent %d bytes, %d bytes remaining\n", bytesWritten, totalSize);

            // Wait for the modem to respond with OK before sending the next chunk
            String ack = "";
            uint32_t t = millis();
            while (millis() - t < 10000) {
                while (modem.stream.available()) ack += (char)modem.stream.read();
                if (ack.indexOf("OK") != -1 || ack.indexOf("ERROR") != -1) break;
            }
            Serial.print("Write ack: "); Serial.println(ack);
        } else {
            Serial.println("Failed to read chunk from file");
            return;
        }

        firstChunk = false; // Update the flag after the first chunk
    }

    Serial.println("File upload completed");
    delay(500);
    fs_term(modem);
}

bool isFileCorrectOnModem(TinyGsm& modem, const char* filename, size_t expectedSize) {
    Serial.printf("\n--- Checking File: %s ---\n", filename);

    // 2. Query file info: +CFSGFIS=<dir>,<filename>
    // 0 = User Directory (U:)
    String command = "+CFSGFIS=0,\"" + String(filename) + "\"";
    run_AT_cmd(modem, command.c_str());
    
    String response;
    // We use a custom waiter to capture the string
    int8_t res = modem.waitResponse(3000, response);
    
    Serial.print("DIAG: Modem Raw Response: ");
    Serial.println(response);

    bool fileMatch = false;
    if (res == 1 && response.indexOf("+CFSGFIS:") != -1) {
        // Extract size from "+CFSGFIS: 1680"
        int colonIndex = response.indexOf(":");
        String sizeStr = response.substring(colonIndex + 1);
        sizeStr.trim();
        size_t actualSize = sizeStr.toInt();

        Serial.printf("DIAG: Found file. Actual Size: %d | Expected: %d\n", actualSize, expectedSize);

        if (actualSize == expectedSize) {
            Serial.println("DIAG: Size MATCH.");
            fileMatch = true;
        } else {
            Serial.println("DIAG: Size MISMATCH.");
        }
    } else {
        Serial.println("DIAG: File not found or command error.");
    }

    // 3. Terminate FS
    fs_term(modem);

    Serial.println("---------------------------\n");
    return fileMatch;
}


bool send_all_certs_to_modem(TinyGsm& modem) {
    Serial.println("send_all_certs_to_modem");
    // device_combined.pem = device.crt + device.key in one file, required by AT+SMSSL
    const char* filenames[] = {"device.key", "AmazonRootCA1.pem", "device.crt"};

    for (const char* fname : filenames) {
        fs::File f = LittleFS.open(String("/") + fname, FILE_READ);
        if (!f) continue;

        size_t localSize = f.size();

        Serial.printf("Uploading %s (%d bytes)...\n", fname, localSize);
        sendFileToModem(modem, f, fname);
        f.close();
        delay(500);
    }

    Serial.println("OK -----------\n\n");
    return true;
}

