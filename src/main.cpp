#include <Arduino.h>
#include <ESP8266WiFi.h> 
#include <DNSServer.h> 
#include <ESP8266WebServer.h> 
#include <EEPROM.h>
#include <ESP8266mDNS.h>
#include <ESP8266HTTPClient.h> 
#include <DHTesp.h>
#include <WiFiClient.h>

#define             EEPROM_LENGTH 1024
#define             RESET_PIN 0
ESP8266WebServer    webServer(80);
char                eRead[30];
char                ssid[30];
char                password[30]; 
char                influxdb[30]; 

DHTesp              dht;
int                 interval = 2000;
unsigned long       lastDHTReadMillis = 0;
float               humidity = 0;
float               temperature = 0;

HTTPClient http;
WiFiClient client;

String responseHTML = ""
    "<!DOCTYPE html><html><head><title>CaptivePortal</title></head><body><center>"
    "<p>Captive Sample Server App</p>"
    "<form action='/save'>"
    "<p><input type='text' name='ssid' placeholder='SSID' onblur='this.value=removeSpaces(this.value);'></p>"
    "<p><input type='text' name='password' placeholder='WLAN Password'></p>" 
    "<p><input type='text' name='influxdb' placeholder='Influxdb Address'></p>"//influxdb 받기
    "<p><input type='submit' value='Submit'></p></form>"
    "<p>This is a captive portal example</p></center></body>"
    "<script>function removeSpaces(string) {"
    "   return string.split(' ').join('');"
    "}</script></html>";

// Saves string to EEPROM
void SaveString(int startAt, const char* id) { 
    for (byte i = 0; i <= strlen(id); i++) {
        EEPROM.write(i + startAt, (uint8_t) id[i]);
    }
    EEPROM.commit();
}

// Reads string from EEPROM
void ReadString(byte startAt, byte bufor) {
    for (byte i = 0; i <= bufor; i++) {
        eRead[i] = (char)EEPROM.read(i + startAt);
    }
}

void save(){
    Serial.println("button pressed");
    Serial.println(webServer.arg("ssid"));
    SaveString( 0, (webServer.arg("ssid")).c_str());
    SaveString(30, (webServer.arg("password")).c_str());
    SaveString(60, (webServer.arg("influxdb")).c_str()); //influxdb 받아온 내용 save
    webServer.send(200, "text/plain", "OK");
    ESP.restart(); 
}

void configWiFi() {
    const byte DNS_PORT = 53;
    IPAddress apIP(192, 168, 1, 1);
    DNSServer dnsServer;
    
    WiFi.mode(WIFI_AP);
    WiFi.softAPConfig(apIP, apIP, IPAddress(255, 255, 255, 0));
    WiFi.softAP("wonazz");     // change this to your portal SSID
    
    dnsServer.start(DNS_PORT, "*", apIP);

    webServer.on("/save", save);

    webServer.onNotFound([]() {
        webServer.send(200, "text/html", responseHTML);
    });
    webServer.begin();
    while(true) {
        dnsServer.processNextRequest();
        webServer.handleClient();
        yield();
    }
}

void load_config_wifi() {
    ReadString(0, 30);
    if (!strcmp(eRead, "")) {
        Serial.println("Config Captive Portal started");
        configWiFi();
    } else {
        Serial.println("IOT Device started");
        strcpy(ssid, eRead);
        ReadString(30, 30);
        strcpy(password, eRead);
    }
}

IRAM_ATTR void GPIO0() {
    SaveString(0, ""); // blank out the SSID field in EEPROM
    ESP.restart();
} 

void readDHT22() {
    unsigned long currentMillis = millis();

    if(currentMillis - lastDHTReadMillis >= interval) {
        lastDHTReadMillis = currentMillis;

        humidity = dht.getHumidity();              // Read humidity (percent)
        temperature = dht.getTemperature();        // Read temperature as Fahrenheit
    }
}

void setup() {

    Serial.begin(115200);
    EEPROM.begin(EEPROM_LENGTH);
    pinMode(RESET_PIN, INPUT_PULLUP);
    attachInterrupt(RESET_PIN, GPIO0, FALLING);
    while(!Serial);
    Serial.println();

    load_config_wifi(); // load or config wifi if not configured
   
    WiFi.mode(WIFI_STA);
    WiFi.begin(ssid, password);
    Serial.println("");

    // Wait for connection
    int i = 0;
    while (WiFi.status() != WL_CONNECTED) {
        delay(500);
        Serial.print(".");
        if(i++ > 15) {
            configWiFi();
        }
    }
    Serial.print("Connected to "); Serial.println(ssid);
    Serial.print("IP address: "); Serial.println(WiFi.localIP());
    http.begin(client,""+String(influxdb));


     if (MDNS.begin("wonazz")) {
       Serial.println("MDNS responder started");
    }

    // your setup code here

    Serial.println("Runtime Starting");  

    Serial.begin(115200);
    dht.setup(14, DHTesp::DHT22); // Connect DHT sensor to GPIO 14
    delay(1000);

    Serial.println(); Serial.println("Humidity (%)\tTemperature (C)");
    delay(1000);


}


void loop() {
    MDNS.update();
    // your loop code here

    readDHT22();
    Serial.printf("%.1f\t %.1f\n", humidity, temperature);
    delay(1000);

    http.addHeader("Conetene-Type","text/plain");
    http.POST("wonaz,host=server01,region=us-west temp="+String(humidity));

    delay(1000);
    http.POST("wonaz,host=server01,region=us-west humi="+String(temperature));

    String payload = http.getString();
    Serial.println(payload);
    delay(1000);
    http.end();

    delay(5000);
    
}