
#pragma once

const size_t bufferSize = 10240; // 10KB buffer size
uint8_t buffer[bufferSize];

void sendFileToModem(TinyGsm& modem, File file, const char* filename) {
    /*
    modem.sendAT("+CFSTERM"); // Close FS in case it's still initialized
    if (modem.waitResponse() != 1) {
        Serial.println("Failed to terminate file system");
        return;
    }

    modem.sendAT("+CFSINIT"); // Initialize FS
    if (modem.waitResponse() != 1) {
        Serial.println("Failed to initialize file system");
        return;
    }
    */

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
        Serial.println(command); // For reference
        modem.sendAT(command.c_str()); // Send file upload command
        // if (modem.waitResponse(30000UL, "ERROR") == 1) { // Wait for modem confirmation
        //     Serial.println("Modem did not respond with DOWNLOAD");
        //     return;
        // }
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

    // Terminate file system after sending the file
    modem.sendAT("+CFSTERM");
    if (modem.waitResponse() != 1) {
        Serial.println("Failed to terminate file system after sending the file");
        return;
    }
}

