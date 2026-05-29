#pragma once

#include <Arduino.h>
#include <Client.h>
#include <PubSubClient.h>
#include <WiFiClient.h>
#include <WiFiClientSecure.h>

#include "certs.h"
#include "types.h"

typedef void (*MqttSetCommandHandler)(const String& payload);
typedef void (*MqttDoCommandHandler)(const String& payload);

class MqttManager {
 public:
  MqttManager();

  void begin(const RuntimeConfig& config, MqttSetCommandHandler setHandler,
             MqttDoCommandHandler doHandler);
  void loop(uint32_t nowMs, bool wifiConnected);
  bool isConnected();
  bool publishTelemetry(const String& payload);
  bool publishStatus(const String& payload);
  bool publishException(const String& payload);
  bool publishAvailability(const char* payload, bool retained);
  bool publishCalibrationMetadata(const String& payload);
  void setConfig(const RuntimeConfig& config);
  MqttConnectionState connectionState() const;
  const char* connectionLabel() const;
  bool isTlsEnabled() const;
  bool isTlsInsecure() const;
  const char* tlsModeLabel() const;

 private:
  struct QueuedMessage {
    QueueMessageType type = QueueMessageType::Telemetry;
    String topic;
    String payload;
    bool retained = false;
    bool used = false;
  };

  bool connectIfNeeded(uint32_t nowMs, bool wifiConnected);
  void configureTransport();
  void configureSecureClient();
  void updateTopicCache();
  bool publish(QueueMessageType type, const String& topic, const String& payload, bool retained);
  void queueMessage(QueueMessageType type, const String& topic, const String& payload, bool retained);
  void flushQueue();
  void overwriteNewestOfType(QueueMessageType type, const String& topic, const String& payload,
                             bool retained);
  void setState(MqttConnectionState state, const String& detail = "");
  const char* stateLabel(MqttConnectionState state) const;
  void handleMessage(char* topic, uint8_t* payload, unsigned int length);
  static void staticCallback(char* topic, uint8_t* payload, unsigned int length);

  WiFiClient wifiClient_;
  WiFiClientSecure secureClient_;
  Client* transportClient_ = nullptr;
  PubSubClient client_;
  RuntimeConfig config_;
  MqttSetCommandHandler setHandler_ = nullptr;
  MqttDoCommandHandler doHandler_ = nullptr;
  QueuedMessage queue_[Config::MQTT_QUEUE_CAPACITY];
  uint8_t queueHead_ = 0;
  uint8_t queueCount_ = 0;
  uint32_t lastReconnectAttemptMs_ = 0;
  MqttConnectionState state_ = MqttConnectionState::Disconnected;
  String lastErrorDetail_;
  String telemetryTopic_;
  String statusTopic_;
  String exceptionTopic_;
  String availabilityTopic_;
  String calibrationTopic_;
  String commandWildcardTopic_;
  String commandSetTopic_;
  String commandDoTopic_;
  String clientId_;

  static MqttManager* instance_;
};
