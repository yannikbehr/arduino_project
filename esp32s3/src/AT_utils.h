#pragma once

bool wait_response(TinyGsm& modem, int timeout, const char* cmd) {
    bool ok = modem.waitResponse(timeout) == 1;
    Serial.print(ok ? "command succeeded: " : "command failed: ");
    Serial.println(cmd);
    return ok;
}

bool run_AT_cmd(TinyGsm& modem, const char* cmd, int timeout = 1000, int retry = 0) {
    for (int i = 0; i <= retry; i++) {
        modem.sendAT(cmd);
        if (wait_response(modem, timeout, cmd)) return true;
    }
    return false;
}

String cmd_get_return(TinyGsm& modem, String cmd, uint32_t timeout = 2000) {
    String response = "";
    Serial.print("Sending: "); Serial.println(cmd);

    while (modem.stream.available()) modem.stream.read();  // clear stale data

    modem.stream.println(cmd);

    uint32_t start = millis();
    while (millis() - start < timeout) {
        while (modem.stream.available()) response += (char)modem.stream.read();
        if (response.indexOf("OK")    != -1) break;
        if (response.indexOf("ERROR") != -1) break;
        if (response.indexOf(">")     != -1) break;
    }

    Serial.print("Response: "); Serial.println(response);
    return response;
}
