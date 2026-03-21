
#pragma once

bool wait_respose_wrap(TinyGsm& modem, int timeout, const char* cmd){
    bool ok;
    if (modem.waitResponse(timeout) != 1) {
        Serial.print("command failed: ");
        ok = false;
    } else {
        Serial.print("command succeded: ");
        ok = true;
    }
    Serial.println(cmd);
    return ok;
}

bool run_AT_cmd(TinyGsm& modem, const char* cmd, int timeout=1000, int retry=0){
    bool success = false;
    for (int i=0; i<=retry; i++){
        modem.sendAT(cmd);
        success = wait_respose_wrap(modem, timeout, cmd);
        if (success){
            break;
        }
    }
    return success;
}

String cmd_get_return(TinyGsm& modem, String cmd, uint32_t timeout = 2000) {
    String response = "";
    Serial.print("Sending: "); Serial.println(cmd);
    
    // 1. Clear the incoming buffer to avoid old "garbage"
    while(modem.stream.available()) modem.stream.read();

    // 2. Send the command
    modem.stream.println(cmd);

    // 3. Capture response until timeout or termination
    uint32_t start = millis();
    while (millis() - start < timeout) {
        while (modem.stream.available()) {
            char c = modem.stream.read();
            response += c;
        }
        // 4. Check if we reached a concluding state
        if (response.indexOf("OK") != -1) break;
        if (response.indexOf("ERROR") != -1) break;
        if (response.indexOf(">") != -1) break; // For CFSWFILE downloads
    }

    Serial.print("Response: "); Serial.println(response);
    return response;
}

void flushModem(TinyGsm& modem) {
    Serial.println("Flushing Modem AT Parser...");
    // 1. Send many 'A's and newlines to break out of any pending 'DOWNLOAD' or 'DATA' modes
    for(int i=0; i<5; i++) {
        modem.stream.println("AT");
        delay(200);
        while(modem.stream.available()) {
            Serial.print((char)modem.stream.read()); // Let's see if it's spitting out errors
        }
    }
    Serial.println("Done Flushing Modem");
}
