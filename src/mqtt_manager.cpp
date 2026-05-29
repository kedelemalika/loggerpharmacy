#include "mqtt_manager.h"

#include <cstring>

#include "config.h"
#include "topics.h"

MqttManager* MqttManager::instance_ = nullptr;

MqttManager::MqttManager() : transportClient_(&wifiClient_), client_(wifiClient_) {
  instance_ = this;
}

void MqttManager::begin(const RuntimeConfig& config, MqttSetCommandHandler setHandler,
                        MqttDoCommandHandler doHandler) {
  config_ = config;
  setHandler_ = setHandler;
  doHandler_ = doHandler;
  updateTopicCache();
  configureTransport();
  client_.setServer(config_.mqttHost.c_str(), config_.mqttPort);
  client_.setBufferSize(256);
  client_.setCallback(staticCallback);
  client_.setKeepAlive(30);
  client_.setSocketTimeout(config_.mqttUseTls ? 6 : 2);
}

void MqttManager::setConfig(const RuntimeConfig& config) {
  config_ = config;
  updateTopicCache();
  configureTransport();
  client_.setServer(config_.mqttHost.c_str(), config_.mqttPort);
  client_.setSocketTimeout(config_.mqttUseTls ? 6 : 2);
}

void MqttManager::loop(uint32_t nowMs, bool wifiConnected) {
  connectIfNeeded(nowMs, wifiConnected);
  if (client_.connected()) {
    client_.loop();
  }
}

bool MqttManager::isConnected() { return client_.connected(); }

MqttConnectionState MqttManager::connectionState() const { return state_; }

const char* MqttManager::connectionLabel() const { return stateLabel(state_); }

bool MqttManager::isTlsEnabled() const { return config_.mqttUseTls; }

bool MqttManager::isTlsInsecure() const { return config_.mqttTlsInsecure; }

const char* MqttManager::tlsModeLabel() const {
  if (!isTlsEnabled()) {
    return "TCP";
  }
  return isTlsInsecure() ? "TLS-INSECURE" : "TLS-VERIFY";
}

bool MqttManager::publishTelemetry(const String& payload) {
  return publish(QueueMessageType::Telemetry, telemetryTopic_, payload, false);
}

bool MqttManager::publishStatus(const String& payload) {
  return publish(QueueMessageType::Status, statusTopic_, payload, false);
}

bool MqttManager::publishException(const String& payload) {
  return publish(QueueMessageType::Exception, exceptionTopic_, payload, false);
}

bool MqttManager::publishAvailability(const char* payload, bool retained) {
  if (!client_.connected()) {
    return false;
  }
  return client_.publish(availabilityTopic_.c_str(), payload, retained);
}

bool MqttManager::publishCalibrationMetadata(const String& payload) {
  // Best-effort: log if not connected but do not block.
  // Calibration metadata is not queued — it is sent on best-effort basis.
  if (!client_.connected()) {
    Serial.println("[MQTT] Cal metadata skipped: not connected");
    return false;
  }
  int rc = client_.publish(calibrationTopic_.c_str(), payload.c_str(), false);
  if (rc == 0) {
    Serial.println("[MQTT] Cal metadata published");
    return true;
  }
  Serial.printf("[MQTT] Cal metadata publish failed rc=%d\n", rc);
  return false;
}

bool MqttManager::connectIfNeeded(uint32_t nowMs, bool wifiConnected) {
  if (!wifiConnected) {
    setState(MqttConnectionState::Disconnected, "WiFi disconnected");
    return false;
  }
  if (client_.connected()) {
    return true;
  }
  if ((nowMs - lastReconnectAttemptMs_) < Config::MQTT_RECONNECT_INTERVAL_MS) {
    return false;
  }

  lastReconnectAttemptMs_ = nowMs;
  configureTransport();
  bool connected = client_.connect(clientId_.c_str(), config_.mqttUsername.c_str(),
                                   config_.mqttPassword.c_str(), availabilityTopic_.c_str(), 0,
                                   true, "0");
  if (connected) {
    setState(config_.mqttUseTls ? (config_.mqttTlsInsecure ? MqttConnectionState::TlsInsecure
                                                           : MqttConnectionState::TlsVerified)
                                : MqttConnectionState::TcpReady,
             "MQTT connected");
    client_.subscribe(commandWildcardTopic_.c_str());
    client_.publish(availabilityTopic_.c_str(), "1", true);
    flushQueue();
    Serial.printf("[MQTT] Connected to %s:%u (%s, queue=%u)\n", config_.mqttHost.c_str(),
                  config_.mqttPort, tlsModeLabel(), queueCount_);
  } else {
    int8_t stateCode = client_.state();
    MqttConnectionState failureState = config_.mqttUseTls ? MqttConnectionState::TlsHandshakeFailed
                                                          : MqttConnectionState::ConnectFailed;
    char detail[16];
    snprintf(detail, sizeof(detail), "state=%d", stateCode);
    setState(failureState, detail);
    Serial.printf("[MQTT] Connect failed to %s:%u, pubsub state=%d, mode=%s\n",
                  config_.mqttHost.c_str(), config_.mqttPort, stateCode, tlsModeLabel());
  }
  return connected;
}

void MqttManager::configureTransport() {
  if (config_.mqttUseTls) {
    configureSecureClient();
    transportClient_ = &secureClient_;
  } else {
    transportClient_ = &wifiClient_;
    setState(MqttConnectionState::Disconnected, "TCP mode ready");
    Serial.println("[TLS] MQTT transport set to TCP");
  }
  client_.setClient(*transportClient_);
}

void MqttManager::configureSecureClient() {
  secureClient_.stop();
  secureClient_.setHandshakeTimeout(12);
  secureClient_.setTimeout(12000);

  if (config_.mqttTlsInsecure) {
    // Hanya untuk pengujian awal. Produksi sebaiknya memakai CA certificate valid.
    secureClient_.setInsecure();
    setState(MqttConnectionState::TlsInsecure, "TLS insecure mode");
    Serial.println("[TLS] Insecure TLS mode enabled");
    return;
  }

  if (strlen(Certs::MQTT_CA_CERT) < 64 ||
      strstr(Certs::MQTT_CA_CERT, "REPLACE-THIS-WITH-REAL-CA") != nullptr) {
    setState(MqttConnectionState::TlsConfigError, "CA certificate missing");
    Serial.println("[TLS] TLS enabled but CA certificate is missing or invalid.");
    return;
  }

  secureClient_.setCACert(Certs::MQTT_CA_CERT);
  setState(MqttConnectionState::TlsVerified, "TLS CA configured");
  Serial.println("[TLS] Verified TLS mode enabled with CA certificate");
}

void MqttManager::updateTopicCache() {
  telemetryTopic_ = Topics::telemetry(config_.siteId, config_.deviceId);
  statusTopic_ = Topics::status(config_.siteId, config_.deviceId);
  exceptionTopic_ = Topics::exceptionTopic(config_.siteId, config_.deviceId);
  availabilityTopic_ = Topics::availability(config_.siteId, config_.deviceId);
  calibrationTopic_ = Topics::withSuffix(config_.siteId, config_.deviceId, "/m/cal");
  commandWildcardTopic_ = Topics::commandWildcard(config_.siteId, config_.deviceId);
  commandSetTopic_ = Topics::commandSet(config_.siteId, config_.deviceId);
  commandDoTopic_ = Topics::commandDo(config_.siteId, config_.deviceId);
  clientId_.reserve(config_.siteId.length() + config_.deviceId.length() + 1);
  clientId_ = config_.siteId;
  clientId_ += '-';
  clientId_ += config_.deviceId;
}

bool MqttManager::publish(QueueMessageType type, const String& topic, const String& payload,
                          bool retained) {
  if (!client_.connected()) {
    queueMessage(type, topic, payload, retained);
    return false;
  }
  return client_.publish(topic.c_str(), payload.c_str(), retained);
}

void MqttManager::queueMessage(QueueMessageType type, const String& topic, const String& payload,
                               bool retained) {
  if (type != QueueMessageType::Exception) {
    overwriteNewestOfType(type, topic, payload, retained);
    return;
  }

  if (queueCount_ >= Config::MQTT_QUEUE_CAPACITY) {
    queueHead_ = (queueHead_ + 1) % Config::MQTT_QUEUE_CAPACITY;
    queueCount_--;
    Serial.println("[MQTT] Queue full, oldest message dropped");
  }

  uint8_t tail = (queueHead_ + queueCount_) % Config::MQTT_QUEUE_CAPACITY;
  queue_[tail].type = type;
  queue_[tail].topic = topic;
  queue_[tail].payload = payload;
  queue_[tail].retained = retained;
  queue_[tail].used = true;
  queueCount_++;
}

void MqttManager::overwriteNewestOfType(QueueMessageType type, const String& topic,
                                        const String& payload, bool retained) {
  for (int index = queueCount_ - 1; index >= 0; --index) {
    uint8_t slot = (queueHead_ + index) % Config::MQTT_QUEUE_CAPACITY;
    if (queue_[slot].used && queue_[slot].type == type) {
      queue_[slot].topic = topic;
      queue_[slot].payload = payload;
      queue_[slot].retained = retained;
      return;
    }
  }

  if (queueCount_ >= Config::MQTT_QUEUE_CAPACITY) {
    queueHead_ = (queueHead_ + 1) % Config::MQTT_QUEUE_CAPACITY;
    queueCount_--;
    Serial.println("[MQTT] Queue full, oldest message dropped");
  }

  uint8_t tail = (queueHead_ + queueCount_) % Config::MQTT_QUEUE_CAPACITY;
  queue_[tail].type = type;
  queue_[tail].topic = topic;
  queue_[tail].payload = payload;
  queue_[tail].retained = retained;
  queue_[tail].used = true;
  queueCount_++;
}

void MqttManager::flushQueue() {
  while (queueCount_ > 0 && client_.connected()) {
    QueuedMessage& message = queue_[queueHead_];
    if (message.used &&
        !client_.publish(message.topic.c_str(), message.payload.c_str(), message.retained)) {
      return;
    }
    message.used = false;
    message.topic = "";
    message.payload = "";
    queueHead_ = (queueHead_ + 1) % Config::MQTT_QUEUE_CAPACITY;
    queueCount_--;
  }
}

void MqttManager::setState(MqttConnectionState state, const String& detail) {
  state_ = state;
  lastErrorDetail_ = detail;
}

const char* MqttManager::stateLabel(MqttConnectionState state) const {
  switch (state) {
    case MqttConnectionState::TcpReady:
      return "OK";
    case MqttConnectionState::TlsInsecure:
      return "TLS";
    case MqttConnectionState::TlsVerified:
      return "TLS";
    case MqttConnectionState::ConnectFailed:
    case MqttConnectionState::TlsConfigError:
    case MqttConnectionState::TlsHandshakeFailed:
      return "FAIL";
    case MqttConnectionState::Disconnected:
    default:
      return "FAIL";
  }
}

void MqttManager::handleMessage(char* topic, uint8_t* payload, unsigned int length) {
  String topicStr(topic);
  String payloadStr;
  payloadStr.reserve(length + 1);
  for (unsigned int i = 0; i < length; ++i) {
    payloadStr += static_cast<char>(payload[i]);
  }

  if (topicStr == commandSetTopic_) {
    if (setHandler_ != nullptr) {
      setHandler_(payloadStr);
    }
    return;
  }

  if (topicStr == commandDoTopic_ && doHandler_ != nullptr) {
    doHandler_(payloadStr);
  }
}

void MqttManager::staticCallback(char* topic, uint8_t* payload, unsigned int length) {
  if (instance_ != nullptr) {
    instance_->handleMessage(topic, payload, length);
  }
}
