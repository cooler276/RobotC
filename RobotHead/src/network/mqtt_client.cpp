#include "network/mqtt_client.h"
#include <iostream>
#include <cstring>

MQTTClient::MQTTClient() 
    : mosq_(nullptr), broker_port_(1883), connected_(false) {
    mosquitto_lib_init();
}

MQTTClient::~MQTTClient() {
    if (mosq_) {
        mosquitto_destroy(mosq_);
    }
    mosquitto_lib_cleanup();
}

bool MQTTClient::init(const std::string& broker_host, int broker_port, const std::string& client_id) {
    broker_host_ = broker_host;
    broker_port_ = broker_port;
    
    mosq_ = mosquitto_new(client_id.c_str(), true, this);
    if (!mosq_) {
        std::cerr << "Failed to create mosquitto client" << std::endl;
        return false;
    }
    
    mosquitto_connect_callback_set(mosq_, on_connect_wrapper);
    mosquitto_disconnect_callback_set(mosq_, on_disconnect_wrapper);
    mosquitto_message_callback_set(mosq_, on_message_wrapper);
    
    return true;
}

bool MQTTClient::connect() {
    if (!mosq_) return false;
    
    int rc = mosquitto_connect(mosq_, broker_host_.c_str(), broker_port_, 60);
    if (rc != MOSQ_ERR_SUCCESS) {
        std::cerr << "Failed to connect to MQTT broker: " << mosquitto_strerror(rc) << std::endl;
        return false;
    }
    
    mosquitto_loop_start(mosq_);
    return true;
}

void MQTTClient::disconnect() {
    if (mosq_) {
        mosquitto_loop_stop(mosq_, true);
        mosquitto_disconnect(mosq_);
    }
    connected_ = false;
}

bool MQTTClient::isConnected() const {
    return connected_;
}

bool MQTTClient::subscribe(const std::string& topic) {
    if (!mosq_) return false;
    
    int rc = mosquitto_subscribe(mosq_, nullptr, topic.c_str(), 0);
    return (rc == MOSQ_ERR_SUCCESS);
}

bool MQTTClient::publish(const std::string& topic, const std::string& message) {
    if (!mosq_ || !connected_) return false;
    
    int rc = mosquitto_publish(mosq_, nullptr, topic.c_str(), 
                               message.length(), message.c_str(), 0, false);
    return (rc == MOSQ_ERR_SUCCESS);
}

void MQTTClient::loop() {
    // mosquitto_loop_start()使用時は不要だが、互換性のため残す
}

void MQTTClient::setMessageCallback(std::function<void(const std::string&, const std::string&)> callback) {
    message_callback_ = callback;
}

void MQTTClient::setConnectionCallback(std::function<void(bool)> callback) {
    connection_callback_ = callback;
}

// Static wrappers
void MQTTClient::on_connect_wrapper(struct mosquitto* mosq, void* obj, int rc) {
    static_cast<MQTTClient*>(obj)->on_connect(rc);
}

void MQTTClient::on_disconnect_wrapper(struct mosquitto* mosq, void* obj, int rc) {
    static_cast<MQTTClient*>(obj)->on_disconnect(rc);
}

void MQTTClient::on_message_wrapper(struct mosquitto* mosq, void* obj, const struct mosquitto_message* msg) {
    std::string topic(msg->topic);
    std::string payload((char*)msg->payload, msg->payloadlen);
    static_cast<MQTTClient*>(obj)->on_message(topic, payload);
}

// Callbacks
void MQTTClient::on_connect(int rc) {
    connected_ = (rc == 0);
    std::cout << "MQTT " << (connected_ ? "connected" : "connection failed") << std::endl;
    
    if (connection_callback_) {
        connection_callback_(connected_);
    }
}

void MQTTClient::on_disconnect(int rc) {
    connected_ = false;
    std::cout << "MQTT disconnected" << std::endl;
    
    if (connection_callback_) {
        connection_callback_(false);
    }
}

void MQTTClient::on_message(const std::string& topic, const std::string& payload) {
    if (message_callback_) {
        message_callback_(topic, payload);
    }
}
