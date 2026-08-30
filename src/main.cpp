#include <stdint.h>
#include <M5Unified.h>
#include <utility/M5Timer.h>
#include <M5GFX.h>
#include <WiFi.h>
#include <WebServer.h>

#define BLACK       0x0000 /*   0,   0,   0 */
#define NAVY        0x000F /*   0,   0, 128 */
#define DARKGREEN   0x03E0 /*   0, 128,   0 */
#define DARKCYAN    0x03EF /*   0, 128, 128 */
#define MAROON      0x7800 /* 128,   0,   0 */
#define PURPLE      0x780F /* 128,   0, 128 */
#define OLIVE       0x7BE0 /* 128, 128,   0 */
#define LIGHTGREY   0xC618 /* 192, 192, 192 */
#define DARKGREY    0x7BEF /* 128, 128, 128 */
#define BLUE        0x001F /*   0,   0, 255 */
#define GREEN       0x07E0 /*   0, 255,   0 */
#define CYAN        0x07FF /*   0, 255, 255 */
#define RED         0xF800 /* 255,   0,   0 */
#define MAGENTA     0xF81F /* 255,   0, 255 */
#define YELLOW      0xFFE0 /* 255, 255,   0 */
#define WHITE       0xFFFF /* 255, 255, 255 */
#define ORANGE      0xFDA0 /* 255, 180,   0 */
#define GREENYELLOW 0xB7E0 /* 180, 255,   0 */
#define PINK        0xFC9F /* 255, 255,  16 */

// Wi-Fi credentials: replace with your network SSID and password
const char* ssid = "YOUR_SSID";
const char* password = "YOUR_PASSWORD";

// HTTP server running on port 80
WebServer server(80);

// Payload structure exchanged between the web task and the display loop
struct Data {
    int contextUsed;      // context usage percentage (0–100)
    char modelUsed[32];   // model name string
};

// FreeRTOS queue for data passing between tasks
static QueueHandle_t xQueue;

// Extract query parameters from the incoming GET request and push them
// into the display queue. The web server replies with HTTP 200.
void handleClaudeInfo(){

    Data data = {};

    int context = server.arg("context").toInt();
    if (context >=100) {
        data.contextUsed = 100;
    }else {
        data.contextUsed = context;
    }
    String model = server.arg("model");
    strncpy(data.modelUsed, model.c_str(), sizeof(data.modelUsed) - 1);
    data.modelUsed[sizeof(data.modelUsed)-1] = '\0';
    xQueueOverwrite(xQueue,&data);
    server.send(200);
}

// FreeRTOS task that runs the web server loop independently.
// It listens on /claude and dispatches handleClaudeInfo for each request.
void webServerTask(void *pvParameters){
    server.begin();
    server.on("/claude",handleClaudeInfo);
    for(;;){
        server.handleClient();
    }
}

// Display a fatal error message and reboot the device.
// 'reason' is shown on screen before restarting.
void fatalError(const char* reason) {
    M5.Lcd.fillScreen(BLACK);
    M5.Lcd.setTextColor(RED);
    M5.Display.setCursor(0, 10);
    M5.Display.println(reason);
    delay(3000);
    ESP.restart();
}


void setup(){
    auto cfg = M5.config();
    M5.begin(cfg);

    // Create a single-slot queue for the latest data update
    xQueue = xQueueCreate(1,sizeof(Data));

    // Wi-Fi connection with 10-second timeout
    WiFi.mode(WIFI_STA);
    WiFi.begin(ssid, password);

    int attempts = 0;
    while (WiFi.status() != WL_CONNECTED && attempts < 100) {
        delay(100);
        attempts++;
        if (attempts % 10 == 0) {
            M5.Display.print(".");
        }
    }

    M5.Display.setTextSize(2);

    // Restart if Wi-Fi connection failed
    if (WiFi.status() != WL_CONNECTED) {
        fatalError("WiFi FAILED!\nCheck SSID/PWD");
    }

    // Draw the static UI layout
    M5.Lcd.fillScreen(BLACK);
    M5.Display.setCursor(0, 10);
    M5.Display.println(WiFi.localIP());
    M5.Lcd.drawLine(0, 30 , 320, 30, WHITE);        // separator line
    M5.Lcd.drawRect(10, 200, 300, 30, WHITE);      // progress-bar border

    // Spawn the HTTP server on a separate FreeRTOS task
    xTaskCreate( webServerTask, "WebServerTask", 4096, NULL, 1, NULL);
}

void loop(){
    // Keep the IP address visible at the top of the screen
    M5.update();

    // If the connection drops after boot, show an error and restart
    if (WiFi.status() != WL_CONNECTED) {
        fatalError("Connection lost\nRestarting ...");
    }

    // Render new data if the web task posted an update
    if( xQueue != NULL ){
        Data data;
        if (xQueueReceive( xQueue, &data, 0)){
            // Erase previous model name area and print the new one
            M5.Display.setCursor(10, 100);
            M5.Display.setTextSize(3);
            M5.Lcd.setTextColor(GREENYELLOW);
            M5.Lcd.fillRect(0, 100, 320, 30,BLACK );
            M5.Display.println(data.modelUsed);

            // Erase previous percentage and redraw it
            M5.Display.setTextSize(2);
            M5.Lcd.setTextColor(WHITE);
            M5.Display.setCursor(10, 160);
            M5.Lcd.fillRect(10, 160, 290, 20,BLACK );
            M5.Display.printf("Context: %d %%",data.contextUsed);

            // Erase the old progress bar before drawing the new one
            M5.Lcd.fillRect(15, 205, 290, 20,BLACK );

            // Choose bar colour based on usage thresholds
            uint16_t barColor = GREEN;
            if (data.contextUsed >= 70) {
                barColor = RED;
                M5.Speaker.tone(1000, 200);  // audible warning
            } else if (data.contextUsed >= 30) {
                barColor = YELLOW;
            }

            // Draw the filled portion proportional to context usage
            M5.Lcd.fillRect(15, 205, (data.contextUsed*290)/100, 20, barColor);
        }
    }
}
