#ifndef MQTT_CLIENT_H
#define MQTT_CLIENT_H

#include <string>
#include <functional>
#include <mosquitto.h>

class MQTTClient {
public:
    MQTTClient();
    ~MQTTClient();
    
    bool init(const std::string& broker_host, int broker_port, const std::string& client_id);
    bool connect();
    void disconnect();
    bool isConnected() const;
    
    bool subscribe(const std::string& topic);
    bool publish(const std::string& topic, const std::string& message);
    
    void loop();  // 定期呼び出し用
    
    // コールバック設定
    void setMessageCallback(std::function<void(const std::string&, const std::string&)> callback);
    void setConnectionCallback(std::function<void(bool)> callback);
    
private:
    static void on_connect_wrapper(struct mosquitto* mosq, void* obj, int rc);
    static void on_disconnect_wrapper(struct mosquitto* mosq, void* obj, int rc);
    static void on_message_wrapper(struct mosquitto* mosq, void* obj, const struct mosquitto_message* msg);
    
    void on_connect(int rc);
    void on_disconnect(int rc);
    void on_message(const std::string& topic, const std::string& payload);
    
    struct mosquitto* mosq_;
    std::string broker_host_;
    int broker_port_;
    bool connected_;
    
    std::function<void(const std::string&, const std::string&)> message_callback_;
    std::function<void(bool)> connection_callback_;
};

#endif // MQTT_CLIENT_H
