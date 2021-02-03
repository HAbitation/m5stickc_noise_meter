#include <M5StickC.h>
#include <WiFiClient.h>
#include <driver/i2s.h>
#include <WiFi.h>
#include <HTTPClient.h>

#define PIN_CLK  0
#define PIN_DATA 34
#define READ_LEN (2 * 256)
#define GAIN_FACTOR 3
uint8_t BUFFER[READ_LEN] = {0};

uint16_t oldy[160];
int16_t *adcBuffer = NULL;

String wifi_ssid = ""; //configure your WiFi SSID
String wifi_password = ""; //configure your WiFi PSK

int threshold = 1500; //configure the threshold that has to be exceeded before an alert is being sent
String http_endpoint = ""; //configure the HTTP API endpoint to call when the threshold has been exceeded. Note that the exceeded value will be send as an additional argument in the URL

int custom_delay = 10000; //use this delay to rate-limit the HTTP requests and prevent spamming of the endpoint

bool debug = false; //debug mode, enable to show more info on the LCD

void i2sInit()
{
   i2s_config_t i2s_config = {
    .mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_RX | I2S_MODE_PDM),
    .sample_rate =  44100,
    .bits_per_sample = I2S_BITS_PER_SAMPLE_16BIT, // is fixed at 12bit, stereo, MSB
    .channel_format = I2S_CHANNEL_FMT_ALL_RIGHT,
    .communication_format = I2S_COMM_FORMAT_I2S,
    .intr_alloc_flags = ESP_INTR_FLAG_LEVEL1,
    .dma_buf_count = 2,
    .dma_buf_len = 128,
   };

   i2s_pin_config_t pin_config;
   pin_config.bck_io_num   = I2S_PIN_NO_CHANGE;
   pin_config.ws_io_num    = PIN_CLK;
   pin_config.data_out_num = I2S_PIN_NO_CHANGE;
   pin_config.data_in_num  = PIN_DATA;
  
   
   i2s_driver_install(I2S_NUM_0, &i2s_config, 0, NULL);
   i2s_set_pin(I2S_NUM_0, &pin_config);
   i2s_set_clk(I2S_NUM_0, 44100, I2S_BITS_PER_SAMPLE_16BIT, I2S_CHANNEL_MONO);
}

void send_http_request(int y)
{
  try
    {
      HTTPClient http;
      http.begin(http_endpoint+String(y));
      int httpCode = http.GET();
      http.end(); //Free the resources
      delay(custom_delay);
      if (debug) {
        M5.Lcd.fillScreen(BLACK);
        M5.Lcd.setCursor(1,3,1);
        M5.Lcd.print("Is is quiet again...");
      }
    }
  catch (...)
  {
    Serial.println("HTTP request failed");
  }
}

void mic_record_task (void* arg)
{   
  size_t bytesread;
  while(1){
    i2s_read(I2S_NUM_0,(char*) BUFFER, READ_LEN, &bytesread, (100 / portTICK_RATE_MS));
    adcBuffer = (int16_t *)BUFFER;

    measureSignal();
    vTaskDelay(100 / portTICK_RATE_MS);
  }
}

void setup() {
  M5.begin();
  Serial.begin(115200);
  M5.Lcd.setRotation(3);
  M5.Lcd.fillScreen(BLACK);
  M5.Lcd.setTextColor(WHITE, BLACK);
  M5.Lcd.println("-- NOISE SENSOR --");

  WiFi.begin(wifi_ssid.c_str(), wifi_password.c_str());

  checkConnection();

  if (debug) {
    M5.Lcd.fillScreen(BLACK);
    M5.Lcd.setCursor(1,3,1);
    M5.Lcd.print("Is is quiet again...");
  } else {
    M5.Axp.ScreenBreath(0);
  }

  i2sInit();
  xTaskCreate(mic_record_task, "mic_record_task", 2048, NULL, 1, NULL);
}

boolean checkConnection() {
  int count = 0;
  Serial.print("Waiting for Wi-Fi connection");
  M5.Lcd.println("Waiting for Wi-Fi connection");
  while ( count < 30 ) {
    if (WiFi.status() == WL_CONNECTED) {
      Serial.println("Connected!");
      return (true);
    }
    delay(500);
    count++;
  }
  Serial.print("Connection failed");
  return false;
}

void measureSignal(){
  int y;
  int max = -10000;
  for (int n = 0; n < 160; n++){
    y = adcBuffer[n] * GAIN_FACTOR;
    if (y > max) {
      max = y;
    }
  }
  if (max > threshold) {
    if (debug) {
      M5.Lcd.fillScreen(BLACK);
      M5.Lcd.setCursor(1,3,1);
      M5.Lcd.println("Noise!!");
      M5.Lcd.println("Amp value: "+String(max));
      M5.Lcd.println("Notification sent");
    }
    send_http_request(max);
  }
}

void loop() {
  vTaskDelay(1000 / portTICK_RATE_MS); // otherwise the main task wastes half of the cpu cycles
}
