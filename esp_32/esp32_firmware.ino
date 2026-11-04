#include <WiFi.h>
#include <HTTPClient.h>
#include <LittleFS.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SH110X.h>
#include "driver/i2s.h"

const char* WIFI_SSID = "YOUR_WIFI_NAME";
const char* WIFI_PASSWORD = "YOUR_WIFI_PASSWORD";
const char* SERVER_BASE = "http://YOUR_LAPTOP_IP:8000";

#define BUTTON_PIN 32
#define MIC_I2S I2S_NUM_0
#define SPK_I2S I2S_NUM_1
#define MIC_BCLK 26
#define MIC_WS   25
#define MIC_DIN  34
#define SPK_DOUT 27

#define SAMPLE_RATE 16000
#define RECORD_SECONDS 15
#define WAV_PATH "/heidi_recording.wav"
#define AUDIO_PATH "/heidi_reply.wav"

Adafruit_SH1106G display(128, 64, &Wire, -1);

bool micReady = false;

void showFace(const String& message) {
  display.clearDisplay();
  display.setTextColor(SH110X_WHITE);
  display.setTextSize(1);
  display.setCursor(0, 0);
  display.println("HEIDI");
  display.drawCircle(42, 29, 7, SH110X_WHITE);
  display.drawCircle(86, 29, 7, SH110X_WHITE);
  display.fillCircle(42, 29, 3, SH110X_WHITE);
  display.fillCircle(86, 29, 3, SH110X_WHITE);
  display.drawRoundRect(54, 43, 20, 7, 3, SH110X_WHITE);
  display.setCursor(0, 55);
  display.println(message.substring(0, 20));
  display.display();
}

void writeWavHeader(File &file, uint32_t dataSize) {
  uint32_t sampleRate = SAMPLE_RATE;
  uint16_t bitsPerSample = 16;
  uint16_t channels = 1;
  uint32_t byteRate = sampleRate * channels * bitsPerSample / 8;
  uint16_t blockAlign = channels * bitsPerSample / 8;
  uint32_t chunkSize = 36 + dataSize;

  file.seek(0);
  file.write((const uint8_t*)"RIFF", 4);
  file.write((uint8_t*)&chunkSize, 4);
  file.write((const uint8_t*)"WAVE", 4);
  file.write((const uint8_t*)"fmt ", 4);
  uint32_t subchunk1Size = 16;
  uint16_t audioFormat = 1;
  file.write((uint8_t*)&subchunk1Size, 4);
  file.write((uint8_t*)&audioFormat, 2);
  file.write((uint8_t*)&channels, 2);
  file.write((uint8_t*)&sampleRate, 4);
  file.write((uint8_t*)&byteRate, 4);
  file.write((uint8_t*)&blockAlign, 2);
  file.write((uint8_t*)&bitsPerSample, 2);
  file.write((const uint8_t*)"data", 4);
  file.write((uint8_t*)&dataSize, 4);
}

bool startMicrophone() {
  i2s_config_t config = {};
  config.mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_RX);
  config.sample_rate = SAMPLE_RATE;
  config.bits_per_sample = I2S_BITS_PER_SAMPLE_32BIT;
  config.channel_format = I2S_CHANNEL_FMT_ONLY_RIGHT;
  config.communication_format = I2S_COMM_FORMAT_STAND_I2S;
  config.intr_alloc_flags = ESP_INTR_FLAG_LEVEL1;
  config.dma_buf_count = 8;
  config.dma_buf_len = 512;
  config.use_apll = false;
  config.tx_desc_auto_clear = false;
  config.fixed_mclk = 0;

  i2s_pin_config_t pins = {};
  pins.bck_io_num = MIC_BCLK;
  pins.ws_io_num = MIC_WS;
  pins.data_out_num = I2S_PIN_NO_CHANGE;
  pins.data_in_num = MIC_DIN;

  esp_err_t e = i2s_driver_install(MIC_I2S, &config, 0, NULL);
  if (e != ESP_OK && e != ESP_ERR_INVALID_STATE) {
    Serial.printf("Mic install error: %d\n", e);
    return false;
  }
  e = i2s_set_pin(MIC_I2S, &pins);
  if (e != ESP_OK) {
    Serial.printf("Mic pin error: %d\n", e);
    return false;
  }
  i2s_zero_dma_buffer(MIC_I2S);
  micReady = true;
  return true;
}

void stopMicrophone() {
  if (micReady) {
    i2s_driver_uninstall(MIC_I2S);
    micReady = false;
  }
}

bool recordAudio() {
  stopMicrophone();
  if (!startMicrophone()) return false;

  File file = LittleFS.open(WAV_PATH, "w");
  if (!file) {
    stopMicrophone();
    Serial.println("Could not create recording file");
    return false;
  }

  uint8_t blank[44] = {0};
  file.write(blank, sizeof(blank));

  const uint32_t totalSamples = SAMPLE_RATE * RECORD_SECONDS;
  uint32_t writtenSamples = 0;
  int32_t raw[256];
  int16_t pcm[256];

  showFace("Listening...");
  Serial.println("Recording for 15 seconds");

  while (writtenSamples < totalSamples) {
    size_t bytesRead = 0;
    esp_err_t result = i2s_read(MIC_I2S, raw, sizeof(raw), &bytesRead, portMAX_DELAY);
    if (result != ESP_OK || bytesRead == 0) continue;

    size_t count = bytesRead / sizeof(int32_t);
    for (size_t i = 0; i < count; i++) {
      // INMP441 24-bit samples are commonly left-aligned in 32-bit words.
      pcm[i] = (int16_t)(raw[i] >> 14);
    }
    uint32_t remaining = totalSamples - writtenSamples;
    if (count > remaining) count = remaining;
    file.write((uint8_t*)pcm, count * sizeof(int16_t));
    writtenSamples += count;
  }

  uint32_t dataBytes = writtenSamples * sizeof(int16_t);
  writeWavHeader(file, dataBytes);
  file.close();
  stopMicrophone();
  Serial.printf("Recording saved: %lu bytes audio\n", (unsigned long)dataBytes);
  return true;
}

bool uploadRecording(String &audioUrl) {
  File file = LittleFS.open(WAV_PATH, "r");
  if (!file) return false;

  WiFiClient client;
  HTTPClient http;
  String endpoint = String(SERVER_BASE) + "/voice";
  if (!http.begin(client, endpoint)) {
    file.close();
    return false;
  }

  String boundary = "----HeidiBoundary7MA4YWxk";
  http.addHeader("Content-Type", "multipart/form-data; boundary=" + boundary);
  http.addHeader("Connection", "close");

  String head = "--" + boundary + "\r\n";
  head += "Content-Disposition: form-data; name=\"file\"; filename=\"heidi.wav\"\r\n";
  head += "Content-Type: audio/wav\r\n\r\n";
  String tail = "\r\n--" + boundary + "--\r\n";

  int contentLength = head.length() + file.size() + tail.length();
  http.addHeader("Content-Length", String(contentLength));

  WiFiClient *stream = http.getStreamPtr();
  // Use a raw POST stream so the WAV can be sent without loading it all into RAM.
  int code = http.POST((uint8_t*)"", 0);
  (void)code;
  http.end();

  // Re-open and send a manually assembled request using WiFiClient.
  file.seek(0);
  if (!client.connect(String(SERVER_BASE).substring(7).substring(0, String(SERVER_BASE).substring(7).indexOf(':')).c_str(), 8000)) {
    file.close();
    return false;
  }

  String hostPart = String(SERVER_BASE).substring(7);
  int slash = hostPart.indexOf('/');
  if (slash >= 0) hostPart = hostPart.substring(0, slash);
  client.print("POST /voice HTTP/1.1\r\n");
  client.print("Host: " + hostPart + "\r\n");
  client.print("Content-Type: multipart/form-data; boundary=" + boundary + "\r\n");
  client.print("Content-Length: " + String(contentLength) + "\r\n");
  client.print("Connection: close\r\n\r\n");
  client.print(head);

  uint8_t buffer[1024];
  while (file.available()) {
    size_t n = file.read(buffer, sizeof(buffer));
    client.write(buffer, n);
  }
  client.print(tail);
  file.close();

  String response;
  unsigned long timeout = millis() + 120000;
  while (millis() < timeout && (client.connected() || client.available())) {
    while (client.available()) response += (char)client.read();
    delay(10);
  }
  client.stop();

  int bodyAt = response.indexOf("\r\n\r\n");
  if (bodyAt < 0) {
    Serial.println("Invalid backend response");
    return false;
  }
  String body = response.substring(bodyAt + 4);
  Serial.println(body);

  int keyAt = body.indexOf("\"audio_url\"");
  if (keyAt < 0) return false;
  int colon = body.indexOf(':', keyAt);
  int q1 = body.indexOf('"', colon + 1);
  int q2 = body.indexOf('"', q1 + 1);
  if (q1 < 0 || q2 < 0) return false;
  audioUrl = body.substring(q1 + 1, q2);
  return true;
}

bool downloadAndPlay(const String &audioUrl) {
  String url = audioUrl.startsWith("http") ? audioUrl : String(SERVER_BASE) + audioUrl;
  WiFiClient client;
  HTTPClient http;
  if (!http.begin(client, url)) return false;
  int code = http.GET();
  if (code != HTTP_CODE_OK) {
    Serial.printf("Audio download failed: %d\n", code);
    http.end();
    return false;
  }

  File out = LittleFS.open(AUDIO_PATH, "w");
  if (!out) {
    http.end();
    return false;
  }
  WiFiClient *stream = http.getStreamPtr();
  uint8_t buf[1024];
  int remaining = http.getSize();
  while (http.connected() && (remaining > 0 || remaining == -1)) {
    size_t availableBytes = stream->available();
    if (availableBytes) {
      int n = stream->readBytes(buf, min((int)sizeof(buf), (int)availableBytes));
      out.write(buf, n);
      if (remaining > 0) remaining -= n;
    } else {
      delay(1);
    }
    if (remaining == 0) break;
  }
  out.close();
  http.end();

  return true;
}

bool playWavFile() {
  stopMicrophone();

  File file = LittleFS.open(AUDIO_PATH, "r");
  if (!file || file.size() <= 44) {
    if (file) file.close();
    return false;
  }
  file.seek(44);

  i2s_config_t config = {};
  config.mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_TX);
  config.sample_rate = 22050;
  config.bits_per_sample = I2S_BITS_PER_SAMPLE_16BIT;
  config.channel_format = I2S_CHANNEL_FMT_RIGHT_LEFT;
  config.communication_format = I2S_COMM_FORMAT_STAND_I2S;
  config.intr_alloc_flags = ESP_INTR_FLAG_LEVEL1;
  config.dma_buf_count = 8;
  config.dma_buf_len = 512;
  config.use_apll = false;
  config.tx_desc_auto_clear = true;
  config.fixed_mclk = 0;

  i2s_pin_config_t pins = {};
  pins.bck_io_num = MIC_BCLK;
  pins.ws_io_num = MIC_WS;
  pins.data_out_num = SPK_DOUT;
  pins.data_in_num = I2S_PIN_NO_CHANGE;

  i2s_driver_install(SPK_I2S, &config, 0, NULL);
  i2s_set_pin(SPK_I2S, &pins);
  i2s_zero_dma_buffer(SPK_I2S);

  showFace("Speaking...");
  uint8_t mono[1024];
  int16_t stereo[512];
  while (file.available()) {
    int n = file.read(mono, sizeof(mono));
    int samples = n / 2;
    int16_t *src = (int16_t*)mono;
    for (int i = 0; i < samples; i++) {
      stereo[i * 2] = src[i];
      stereo[i * 2 + 1] = src[i];
    }
    size_t bytesWritten = 0;
    i2s_write(SPK_I2S, stereo, samples * 4, &bytesWritten, portMAX_DELAY);
  }
  file.close();
  i2s_driver_uninstall(SPK_I2S);
  showFace("Ready!");
  return true;
}

void setup() {
  Serial.begin(115200);
  pinMode(BUTTON_PIN, INPUT_PULLUP);
  Wire.begin(21, 22);

  if (!display.begin(0x3C, true)) {
    Serial.println("OLED not found");
  } else {
    showFace("Starting...");
  }

  if (!LittleFS.begin(true)) {
    Serial.println("LittleFS mount failed");
    showFace("Storage error");
    while (true) delay(1000);
  }

  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  showFace("Connecting WiFi");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nWiFi connected");
  Serial.println(WiFi.localIP());
  showFace("Ready!");
}

void loop() {
  if (digitalRead(BUTTON_PIN) == LOW) {
    delay(40);
    if (digitalRead(BUTTON_PIN) == LOW) {
      while (digitalRead(BUTTON_PIN) == LOW) delay(10);

      if (WiFi.status() != WL_CONNECTED) {
        showFace("WiFi offline");
        return;
      }
      if (!recordAudio()) {
        showFace("Mic error");
        return;
      }
      showFace("Thinking...");
      String audioUrl;
      if (!uploadRecording(audioUrl)) {
        showFace("Upload error");
        return;
      }
      if (!downloadAndPlay(audioUrl)) {
        showFace("Audio error");
        return;
      }
      if (!playWavFile()) showFace("Playback error");
      delay(500);
    }
  }
  delay(10);
}
