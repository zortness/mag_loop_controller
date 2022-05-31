#include <Arduino.h>
#include <M5Stack.h>
#include <WiFi.h>
#include <ESPmDNS.h>
#include <HTTPClient.h>
#include <cppQueue.h>
#include "wifi_info.h"

const char* hostname = "magloopremote";

const char* hostOptions[] = {"magloopcontroller20", "magloopcontroller40"};
int lastSelectedHostOption = 0;
int hostOptionSize = 2;
const char * controllerHost = nullptr;
bool appSelectHostChanged = true;

//const char * controllerUrl = "http://magloopcontroller.local:80/";
const char * rotateCmd = ":80/rotate?deg=";

const float degOptions[] = {3.6, 5.0, 7.2, 10.0, 15.0, 30.0, 45.0, 90.0, 180.0};
int degOptionIndex = 4;
const int degOptionSize = 9;

#define IP_LOOKUP_CACHE_TIME 500000000  // 500 seconds
static unsigned long lastIpLookup = 0;
static String controllerIp("192.168.0.0");

// keep track of last run commands
cppQueue cmdHistory(sizeof(String *), 5, FIFO);


static void drawMenu() {
    M5.Lcd.clear(TFT_BLACK);
    M5.Lcd.setTextColor(TFT_WHITE);

    M5.Lcd.fillRoundRect(0,0,M5.Lcd.width(),28,3,TFT_NAVY);
    M5.Lcd.fillRoundRect(31,M5.Lcd.height()-28,60,28,3,TFT_NAVY);
    M5.Lcd.fillRoundRect(126,M5.Lcd.height()-28,60,28,3,TFT_NAVY);
    M5.Lcd.fillRoundRect(221,M5.Lcd.height()-28,60,28,3,TFT_NAVY);

    String top("MagLoop Step ");
    top.concat(degOptions[degOptionIndex]);
    M5.Lcd.drawCentreString(top,M5.Lcd.width()/2,6,2);

    M5.Lcd.drawCentreString("<",31+30,M5.Lcd.height()-28+6,2);
    M5.Lcd.drawCentreString("Step",126+30,M5.Lcd.height()-28+6,2);
    M5.Lcd.drawCentreString(">",221+30,M5.Lcd.height()-28+6,2);
}

static void drawHistory() {
    if (!cmdHistory.isEmpty()) {
        int height = 30;
        for (int i = 0; i < cmdHistory.getCount(); i++) {
            String * ref = nullptr;
            cmdHistory.peekIdx(&ref, i);
            if (ref != nullptr) {
                M5.Lcd.drawString(ref->c_str(), 5, height, 2);
                height += 22;
            } else {
                Serial.println("invalid command history entry");
            }
        }
    }
}

static void addHistory(String * cmd) {
    if (cmdHistory.isFull()) {
        String * popped;
        cmdHistory.pop(&popped);
        Serial.print("Removed: ");
        Serial.println(popped->c_str());
        free(popped);
    }
    cmdHistory.push(&cmd);
}

static String getControllerIp() {
    if (lastIpLookup < micros()) {
        lastIpLookup = micros() + IP_LOOKUP_CACHE_TIME;
        IPAddress host = MDNS.queryHost(controllerHost);
        controllerIp = host.toString();
        auto * cmd = new String("controller: ");
        cmd->concat(controllerIp);
        addHistory(cmd);
    }
    return controllerIp;
}

static void move(float degrees) {
    HTTPClient httpClient;
    String * cmd;
    String url("http://");
    url.concat(getControllerIp());
    url.concat(rotateCmd);
    url.concat(degrees);
    M5.Lcd.clear(TFT_BLACK);
    M5.Lcd.setTextColor(TFT_WHITE);
    String intent((degrees < 0 ? "CCW " : "CW "));
    intent.concat(degOptions[degOptionIndex]);
    M5.Lcd.drawCentreString(intent,M5.Lcd.width()/2,(M5.Lcd.height()/2)-10,2);
    Serial.print("calling ");
    Serial.println(url);
    httpClient.begin(url.c_str());
    int respCode = httpClient.GET();
    if (respCode > 0) {
        Serial.print("Response: ");
        Serial.println(respCode);
        String payload = httpClient.getString();
        Serial.print("Payload: ");
        Serial.println(payload);
        //M5.Lcd.drawCentreString(F(payload.c_str()),M5.Lcd.width()/2,(M5.Lcd.height()/2)+10,2);
        cmd = new String(intent);
    } else {
        Serial.print("Error code: ");
        Serial.println(respCode);
        //String err = "Error code ";
        //err.concat(respCode);
        //M5.Lcd.drawCentreString(F(err.c_str()),M5.Lcd.width()/2,(M5.Lcd.height()/2)+10,2);
        cmd = new String("Error code ");
        cmd->concat(respCode);
    }
    httpClient.end();
    addHistory(cmd);
    drawMenu();
    drawHistory();
    delay(250);
}

static void appMagLoop() {

    if (!WiFi.isConnected()) {
        drawMenu();
        WiFi.mode(WIFI_STA);
        M5.Lcd.drawCentreString(F("Connecting..."),M5.Lcd.width()/2,(M5.Lcd.height()/2)-10,2);
        WiFi.setHostname(hostname);
        WiFi.begin(ssid, password);
        if (WiFi.waitForConnectResult() != WL_CONNECTED) {
            Serial.printf("WiFi Failed!\n");
            M5.Lcd.drawCentreString(F("WiFi Failed"),M5.Lcd.width()/2,(M5.Lcd.height()/2)+10,2);
            delay(1000);
            return;
        }
        M5.Lcd.drawCentreString(F(WiFi.localIP().toString().c_str()),M5.Lcd.width()/2,(M5.Lcd.height()/2)+10,2);
        Serial.print("IP Address: ");
        Serial.println(WiFi.localIP());
        if(!MDNS.begin(hostname)) {
            Serial.println("Error starting mDNS");
        }
        delay(1000);
        drawMenu();
    }

    if (M5.BtnA.pressedFor(100)) {
        // CCW 90
        move(-1 * degOptions[degOptionIndex]);
    }

    if(M5.BtnB.pressedFor(100)) {
        if (degOptionIndex +1 >= degOptionSize) {
            degOptionIndex = 0;
        } else {
            degOptionIndex ++;
        }
        drawMenu();
        drawHistory();
        delay(250);
    }

    if (M5.BtnC.pressedFor(100)) {
        // CW 90
        move(degOptions[degOptionIndex]);
    }
}

static void appSelectHost() {
    if (M5.BtnA.pressedFor(100)) {
        // up
        if (lastSelectedHostOption > 0) {
            lastSelectedHostOption--;
            appSelectHostChanged = true;
        }
    }
    if (M5.BtnC.pressedFor(100)) {
        // down
        if (lastSelectedHostOption < (hostOptionSize-1)) {
            lastSelectedHostOption++;
            appSelectHostChanged = true;
        }
    }
    if (M5.BtnB.pressedFor(100)) {
        // select
        controllerHost = hostOptions[lastSelectedHostOption];
        appSelectHostChanged = true;
        return;
    }

    if (!appSelectHostChanged) {
        return;
    }

    M5.Lcd.clear(TFT_BLACK);
    M5.Lcd.setTextColor(TFT_WHITE);

    M5.Lcd.fillRoundRect(0,0,M5.Lcd.width(),28,3,TFT_NAVY);
    M5.Lcd.fillRoundRect(31,M5.Lcd.height()-28,60,28,3,TFT_NAVY);
    M5.Lcd.fillRoundRect(126,M5.Lcd.height()-28,60,28,3,TFT_NAVY);
    M5.Lcd.fillRoundRect(221,M5.Lcd.height()-28,60,28,3,TFT_NAVY);

    M5.Lcd.drawCentreString("Select Host",M5.Lcd.width()/2,6,2);

    // loop through options
    // highlight selected option
    int height = 30;
    int i = 0;
    for (i = 0; i < hostOptionSize; i++) {
        M5.Lcd.setTextColor(TFT_WHITE);
        if (i == lastSelectedHostOption) {
            M5.Lcd.setTextColor(TFT_ORANGE);
        }
        M5.Lcd.drawString(hostOptions[i], 5, height, 2);
        M5.Lcd.setTextColor(TFT_WHITE);
        height += 22;
    }

    M5.Lcd.drawCentreString("<",31+30,M5.Lcd.height()-28+6,2);
    M5.Lcd.drawCentreString("SELECT",126+30,M5.Lcd.height()-28+6,2);
    M5.Lcd.drawCentreString(">",221+30,M5.Lcd.height()-28+6,2);

    appSelectHostChanged = false;
}


static void appSleep(){
    if (WiFi.isConnected()) {
        WiFi.disconnect();
    }
    M5.Power.setWakeupButton(BUTTON_B_PIN);
    M5.Power.powerOFF();
}


void setup() {
    M5.begin();
    M5.lcd.setBrightness(195);
    Serial.begin(115200);
}

void loop() {
    M5.update();
    if (M5.BtnB.pressedFor(1000)) {
        Serial.println("Going to sleep");
        appSleep();
    }
    if (controllerHost == nullptr) {
        appSelectHost();
    } else {
        appMagLoop();
    }
}

