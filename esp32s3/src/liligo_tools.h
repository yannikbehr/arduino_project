
#pragma once

#include "AT_utils.h" // run_AT_cmd and wait_respose_wrap

const size_t bufferSize = 10240; // 10KB buffer size
uint8_t buffer[bufferSize];

void fs_init(TinyGsm& modem, int retry=5){
    if (run_AT_cmd(modem, "+CFSINIT"), 1000, retry){
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


    // 2. THE WAKE-UP CALL: Check total drive space
    // This command forces the modem to physically talk to the Flash chip
    String driveStatus = cmd_get_return(modem, "AT+CFSGFIS=0");

    if (driveStatus.indexOf("ERROR") != -1) {
        Serial.println("Drive Mount Failed. Retrying with explicit U:/ prefix...");
        // Some firmwares need the specific drive name
        cmd_get_return(modem, "AT+CFSGFIS=1");
    }


    // This confirms we are looking at the User drive for Directory ID 0
    cmd_get_return(modem, "AT+CFSGFIS=0,\"test.txt\"");

    // Get the total size of the file
    size_t totalSize = file.size();
    size_t alreadySent = 0;
    bool firstChunk = true;

    // Loop for sending chunks
    while (totalSize > 0) {
        // Determine the size of the next chunk to send
        size_t chunkSize = std::min(totalSize, static_cast<size_t>(10000)); // Limit chunk size to 10,000 bytes

        // Prepare the file upload command
        String command = "+CFSWFILE=0,\"" + String(filename) + "\"," + String(firstChunk ? 0 : 1) + "," + String(chunkSize) + ",10000";
        String res = cmd_get_return(modem, command.c_str()); 

        if (res.indexOf(">") != -1) {
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
    const char* filenames[] = {"device.key", "AmazonRootCA1.pem", "device.crt"};

    // 3. UPLOAD all files while the session is open
    for (const char* fname : filenames) {
        fs::File f = LittleFS.open(String("/") + fname, FILE_READ);
        if (f) {

            //// first check if the file is already on the modem
            //size_t localSize = f.size();
            //if (isFileCorrectOnModem(modem, fname, localSize)) {
            //    f.close();
            //    continue; // Skip to the next file
            //}
            flushModem(modem);
            Serial.printf("Uploading %s...\n", fname);
            sendFileToModem(modem, f, fname);
            f.close();
            // Let the modem breathe between files
            delay(1000);
        }
    }
    
    Serial.println("All uploads finished. Giving the modem 5 seconds to index everything...");
    delay(5000); // The "Universal Fix" for the last file
    Serial.println("OK -----------\n\n");
    return true;
}

