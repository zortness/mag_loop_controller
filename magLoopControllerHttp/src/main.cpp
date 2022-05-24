#include <Arduino.h>
#include <WiFi.h>
#include <ESPmDNS.h>
#include <ESPAsyncWebServer.h>
#include "A4988.h"

AsyncWebServer server(80);

const char* hostname = "magloopcontroller";
const char* ssid = "SSID";
const char* password = "PASSWORD";

const char* PARAM_MESSAGE = "message";

const char* PARAM_DIR = "dir";
const char* PARAM_STEPS = "steps";
const char* PARAM_DEGREES = "deg";

#define MOTOR_STEPS 200
#define RPM 1
#define MICROSTEPS 16
#define DIR 19
#define MS1 16
#define MS2 17
#define MS3 5
#define STEP 18
#define ENABLE 4
#define DEGREES_PER_STEP 1.8
#define NORMAL_DELAY_USEC 1000
#define LONG_DELAY_USEC 5000


A4988 stepper(MOTOR_STEPS, DIR, STEP, ENABLE, MS1, MS2, MS3);

void notFound(AsyncWebServerRequest *request) {
    request->send(404, "text/plain", "Not found");
}

void setup() {

    Serial.begin(115200);
    WiFi.mode(WIFI_STA);
    WiFi.setHostname(hostname);
    WiFi.begin(ssid, password);

    if (WiFi.waitForConnectResult() != WL_CONNECTED) {
        Serial.printf("WiFi Failed!\n");
        return;
    }

    Serial.print("IP Address: ");
    Serial.println(WiFi.localIP());

    if(!MDNS.begin(hostname)) {
        Serial.println("Error starting mDNS");
    } else {
        MDNS.addService("web control", "http", 80);
    }


    server.on("/", HTTP_GET, [](AsyncWebServerRequest *request){
        request->send(200, "text/plain", "Mag Loop Controller");
    });

    // Send a GET request to <IP>/get?message=<message>
    server.on("/get", HTTP_GET, [] (AsyncWebServerRequest *request) {
        String message;
        if (request->hasParam(PARAM_MESSAGE)) {
            message = request->getParam(PARAM_MESSAGE)->value();
        } else {
            message = "No message sent";
        }
        request->send(200, "text/plain", "Hello, GET: " + message);
    });

    server.on("/step", HTTP_GET, [](AsyncWebServerRequest *request){
        double steps = 0;
        if (request->hasParam(PARAM_STEPS)) {
            String stepsStr = request->getParam(PARAM_STEPS)->value();
            steps = stepsStr.toDouble();
        }
        if (request->hasParam(PARAM_DIR)) {
            String dir = request->getParam(PARAM_DIR)->value();
            if (dir.startsWith("ccw")) {
                steps *= -1;
            }
        }

        int fullSteps = floor(abs(steps));
        int partials = floor(((abs(steps) - fullSteps) * 100.0F) / 16.0F);

        String message = "Stepping ";
        message.concat(steps < 0 ? "CCW " : "CW ");
        message.concat(fullSteps);
        message.concat(" full steps");
        message.concat(", ");
        message.concat(partials);
        message.concat(" partial steps");
        Serial.println(message);

        //stepper.startMove(steps);
        request->send(200, "text/plain", message);
        stepper.enable();

        digitalWrite(DIR, (steps < 0 ? LOW : HIGH));


        stepper.setMicrostep(1);
        uint32_t delay = NORMAL_DELAY_USEC;
        if (fullSteps <= 50) {
            delay = LONG_DELAY_USEC;
        }
        for (int i = 0; i < fullSteps; i++) {
            digitalWrite(STEP, HIGH);
            delayMicroseconds(delay);
            digitalWrite(STEP, LOW);
            delayMicroseconds(delay);
        }

        if (partials > 0) {
            stepper.setMicrostep(16);
            for (int i = 0; i < partials; i++) {
                digitalWrite(STEP, HIGH);
                delayMicroseconds(LONG_DELAY_USEC);
                digitalWrite(STEP, LOW);
                delayMicroseconds(LONG_DELAY_USEC);
            }
        }

        stepper.disable();
        Serial.println("Done");
    });

    server.on("/rotate", HTTP_GET, [](AsyncWebServerRequest *request){
        double deg = 0;
        if (request->hasParam(PARAM_DEGREES)) {
            String degStr = request->getParam(PARAM_DEGREES)->value();
            deg = degStr.toDouble();
        }

        double bestStep = deg / DEGREES_PER_STEP;
        int fullSteps = floor(abs(bestStep));
        int partials = floor(((abs(bestStep) - fullSteps) * 100.0F) / 16.0F);

        String message = "Stepping ";
        message.concat(deg < 0 ? "CCW " : "CW ");
        message.concat(deg);
        message.concat(" deg (");
        message.concat(fullSteps);
        message.concat(" full steps, ");
        message.concat(partials);
        message.concat(" partials)");
        Serial.println(message);

        request->send(200, "text/plain", message);

        stepper.enable();

        digitalWrite(DIR, (deg < 0 ? LOW : HIGH));

        stepper.setMicrostep(1);
        uint32_t delay = NORMAL_DELAY_USEC;
        if (fullSteps <= 50) {
            delay = LONG_DELAY_USEC;
        }
        for (int x = 0; x < fullSteps; x++) {
            digitalWrite(STEP, HIGH);
            delayMicroseconds(delay);
            digitalWrite(STEP, LOW);
            delayMicroseconds(delay);
        }

        if (partials > 0) {
            stepper.setMicrostep(16);
            for (int i = 0; i < partials; i++) {
                digitalWrite(STEP, HIGH);
                delayMicroseconds(LONG_DELAY_USEC);
                digitalWrite(STEP, LOW);
                delayMicroseconds(LONG_DELAY_USEC);
            }
        }

        stepper.disable();
        Serial.println("Done");
    });

    server.onNotFound(notFound);
    server.begin();

    //stepper.begin(RPM, MICROSTEPS);
    stepper.setMicrostep(1);
    stepper.setRPM(1.0F);
    stepper.disable();
}

void loop() {
    // nothing here
}