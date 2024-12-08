#include "Arduino.h"
#include "DFRobotDFPlayerMini.h"
#include "WiFi.h"
#include "WebServer.h"

namespace Config {
constexpr char WIFI_SSID[] = "SSID";
constexpr char WIFI_PASSWORD[] = "PASSWORD";
constexpr uint16_t SERVER_PORT = 80;

constexpr uint32_t DFPLAYER_BAUD_RATE = 9600;
constexpr uint8_t DFPLAYER_RX_PIN = 2;
constexpr uint8_t DFPLAYER_TX_PIN = 3;
constexpr uint8_t DEFAULT_VOLUME = 20;
constexpr uint8_t MAX_VOLUME = 30;

constexpr uint32_t DEBUG_BAUD_RATE = 115200;

constexpr uint16_t WIFI_TIMEOUT_MS = 20000;
constexpr uint16_t CONNECTION_RETRY_DELAY_MS = 1000;
}

struct PlayerState {
  bool isPlaying = false;
  uint8_t currentVolume = Config::DEFAULT_VOLUME;
  uint16_t currentTrack = 1;
  uint16_t totalTracks = 0;
  bool isInitialized = false;
};

class DFPlayerController {
private:
  DFRobotDFPlayerMini player;
  WebServer server;
  PlayerState state;

  static constexpr char RESPONSE_OK[] = "Success: ";
  static constexpr char RESPONSE_ERROR[] = "Error: ";

  void setupEndpoints() {
    server.on("/play", HTTP_GET, [this]() {
      if (!state.isInitialized) {
        sendError("Player not initialized");
        return;
      }

      if (!state.isPlaying) {
        player.start();
        state.isPlaying = true;
        sendSuccess("Music started");
      } else {
        sendSuccess("Already playing");
      }
    });

    server.on("/pause", HTTP_GET, [this]() {
      if (!state.isInitialized) {
        sendError("Player not initialized");
        return;
      }

      if (state.isPlaying) {
        player.pause();
        state.isPlaying = false;
        sendSuccess("Music paused");
      } else {
        sendSuccess("Already paused");
      }
    });

    server.on("/next", HTTP_GET, [this]() {
      if (!state.isInitialized) {
        sendError("Player not initialized");
        return;
      }

      player.next();
      state.isPlaying = true;
      state.currentTrack = (state.currentTrack % state.totalTracks) + 1;
      sendSuccess("Playing next track: " + String(state.currentTrack));
    });

    server.on("/prev", HTTP_GET, [this]() {
      if (!state.isInitialized) {
        sendError("Player not initialized");
        return;
      }

      player.previous();
      state.isPlaying = true;
      state.currentTrack = (state.currentTrack > 1) ? state.currentTrack - 1 : state.totalTracks;
      sendSuccess("Playing previous track: " + String(state.currentTrack));
    });

    server.on("/volume", HTTP_GET, [this]() {
      if (!state.isInitialized) {
        sendError("Player not initialized");
        return;
      }

      if (server.hasArg("level")) {
        int level = server.arg("level").toInt();
        if (level >= 0 && level <= Config::MAX_VOLUME) {
          player.volume(level);
          state.currentVolume = level;
          sendSuccess("Volume set to " + String(level));
        } else {
          sendError("Volume must be between 0 and " + String(Config::MAX_VOLUME));
        }
      } else {
        sendError("Missing volume level parameter");
      }
    });

    server.on("/status", HTTP_GET, [this]() {
      String status = "{"
                      "\"isPlaying\":"
                      + String(state.isPlaying ? "true" : "false") + ","
                                                                     "\"currentTrack\":"
                      + String(state.currentTrack) + ","
                                                     "\"totalTracks\":"
                      + String(state.totalTracks) + ","
                                                    "\"volume\":"
                      + String(state.currentVolume) + ","
                                                      "\"initialized\":"
                      + String(state.isInitialized ? "true" : "false") + "}";
      server.send(200, "application/json", status);
    });
  }

  void sendSuccess(const String& message) {
    server.send(200, "text/plain", String(RESPONSE_OK) + message);
  }

  void sendError(const String& message) {
    server.send(400, "text/plain", String(RESPONSE_ERROR) + message);
  }

  bool initializeWiFi() {
    Serial.print("Connecting to WiFi");
    WiFi.begin(Config::WIFI_SSID, Config::WIFI_PASSWORD);

    unsigned long startTime = millis();
    while (WiFi.status() != WL_CONNECTED) {
      if (millis() - startTime > Config::WIFI_TIMEOUT_MS) {
        Serial.println("\nWiFi connection timeout!");
        return false;
      }
      delay(Config::CONNECTION_RETRY_DELAY_MS);
      Serial.print(".");
    }

    Serial.println("\nConnected to WiFi");
    Serial.print("IP Address: ");
    Serial.println(WiFi.localIP());
    return true;
  }

  bool initializeDFPlayer() {
    Serial.println("Initializing DFPlayer...");

    if (!player.begin(Serial1)) {
      Serial.println("Failed to initialize DFPlayer!");
      return false;
    }

    player.volume(state.currentVolume);
    state.totalTracks = player.readFileCounts();
    state.isInitialized = true;

    Serial.println("DFPlayer initialized successfully");
    Serial.println("Total tracks: " + String(state.totalTracks));
    return true;
  }

public:
  DFPlayerController()
    : server(Config::SERVER_PORT) {}

  void begin() {
    Serial.begin(Config::DEBUG_BAUD_RATE);
    Serial.println("\nStarting DFPlayer Controller...");

    Serial1.begin(Config::DFPLAYER_BAUD_RATE, SERIAL_8N1,
                  Config::DFPLAYER_RX_PIN, Config::DFPLAYER_TX_PIN);

    if (!initializeWiFi() || !initializeDFPlayer()) {
      Serial.println("Initialization failed!");
      return;
    }

    setupEndpoints();
    server.begin();
    Serial.println("Web server started");
  }

  void update() {
    server.handleClient();

    if (state.isInitialized && player.available()) {
      uint8_t type = player.readType();
      int value = player.read();

      switch (type) {
        case DFPlayerPlayFinished:
          state.currentTrack = (state.currentTrack % state.totalTracks) + 1;
          player.next();
          break;

        case DFPlayerError:
          Serial.println("DFPlayer Error: " + String(value));
          break;
      }
    }
  }
};

DFPlayerController controller;

void setup() {
  controller.begin();
}

void loop() {
  controller.update();
}
