#ifndef UPLOAD_CERTS_H
#define UPLOAD_CERTS_H

// certs.h includes sensitive data and has to be created by hand and cannot be commited to the code base.
// See the Makefile to see how the data was retrieved.
#include "certs.h"
#include <LittleFS.h>
#include "AT_utils.h" // run_AT_cmd and wait_respose_wrap

void prepareCertificates() {
    Serial.println("prepareCertificates");

    if (!LittleFS.begin(true)) {
        Serial.println("LittleFS Mount Failed. Formatting...");
        if(!LittleFS.format() || !LittleFS.begin()){
             Serial.println("CRITICAL: LittleFS Format failed.");
             return;
        }
    }

    const char* files[] = {"/AmazonRootCA1.pem", "/device.crt", "/device.key"};
    const char* data[] = {rootCA, clientCert, clientKey};

    // Write PEM with CRLF line endings (required by modem TLS parser)
    auto writePem = [](fs::File& f, const char* pem) {
        const char* p = pem;
        while (*p) {
            if (*p == '\n') f.write('\r');
            f.write(*p++);
        }
    };

    for (int i = 0; i < 3; i++) {
        Serial.printf("Writing %s...\n", files[i]);
        fs::File f = LittleFS.open(files[i], FILE_WRITE);
        if (f) { writePem(f, data[i]); f.close(); }
    }
    Serial.println("OK -----------\n\n");
}

bool setup_ssl(TinyGsm& modem) {
    Serial.println("setup_ssl");
    run_AT_cmd(modem, "+CTZU=1", 5000);

    // Convert PEM files into modem's internal SSL format (requires FS session + dir=3)
    run_AT_cmd(modem, "+CFSINIT", 3000);
    run_AT_cmd(modem, "+CSSLCFG=\"CONVERT\",2,\"AmazonRootCA1.pem\"", 10000);
    run_AT_cmd(modem, "+CSSLCFG=\"CONVERT\",1,\"device.crt\",\"device.key\"", 10000);
    run_AT_cmd(modem, "+CFSTERM", 3000);

    // SSL context 0 — used by TinyGSM CAOPEN (TLS 1.2, ignore time, SNI)
    run_AT_cmd(modem, "+CSSLCFG=\"sslversion\",0,3", 5000);
    run_AT_cmd(modem, "+CSSLCFG=\"IGNORERTCTIME\",0,1", 5000);
    run_AT_cmd(modem, "+CSSLCFG=\"SNI\",0,\"a3fu7j5avgf87g-ats.iot.eu-west-3.amazonaws.com\"", 5000);

    // Pre-configure client cert and key for CAOPEN mux 0
    // TinyGSM only sets CACERT; we add CLIENTCERT/CLIENTKEY for mutual TLS
    run_AT_cmd(modem, "+CASSLCFG=0,\"CLIENTCERT\",\"device.crt\"", 5000);
    run_AT_cmd(modem, "+CASSLCFG=0,\"CLIENTKEY\",\"device.key\"", 5000);

    Serial.println("OK -----------\n\n");
    return true;
}

#endif
