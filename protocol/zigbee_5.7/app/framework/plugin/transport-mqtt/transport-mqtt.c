// Copyright 2016 Silicon Laboratories, Inc.                                *80*

#include "app/framework/include/af.h"
#include "app/framework/util/util.h"
#include "app/framework/plugin/transport-mqtt/transport-mqtt.h"

// MQTT open-source-module include
#include "build/external/inc/MQTTAsync.h"

// MQTT definitions
// The broker address could be different if we connect to the cloud and may
// need paramaterization
#define MQTT_KEEP_ALIVE_INTERVAL_S 20 // seconds
#define MQTT_RETAINED 0 // not retained
#define MQTT_RECONNECT_RATE_MS 5000 // milliseconds

// MQTT objects and state variables
// 16 chars for EUI + prefix length + NULL
static char mqttClientIdString[EMBER_AF_PLUGIN_TRANSPORT_MQTT_CLIENT_ID_PREFIX_LENGTH + 17] = {0};
static bool mqttConnected = false;
static MQTTAsync mqttClient;
static MQTTAsync_connectOptions mqttConnectOptions = MQTTAsync_connectOptions_initializer;
static MQTTAsync_responseOptions mqttSubscribeResponseOptions = MQTTAsync_responseOptions_initializer;
static MQTTAsync_responseOptions mqttPublishResponseOptions = MQTTAsync_responseOptions_initializer;

// MQTT helper functions
static bool mqttConnect(void);

// MQTT client callback definitions
static MQTTAsync_onFailure mqttConnectFailureCallback;
static MQTTAsync_onSuccess mqttConnectSuccessCallback;
static MQTTAsync_connectionLost mqttConnectionLostCallback;
static MQTTAsync_onFailure mqttTopicSubscribeFailureCallack;
static MQTTAsync_onFailure mqttTopicPublishFailureCallack;
static MQTTAsync_messageArrived mqttMessageArrivedCallback;

// Event controls
EmberEventControl emberAfPluginTransportMqttBrokerReconnectEventControl;

void emberAfPluginTransportMqttInitCallback()	//-初始化MQTT客户端
{  
  emberSerialPrintfLine(APP_SERIAL, "MQTT Client Init");
  static EmberEUI64 eui64;
  char euiString[17] = {0}; // 16 chars + NULL

  // Save our EUI information
  emberAfGetEui64(eui64);
  sprintf(euiString, "%02X%02X%02X%02X%02X%02X%02X%02X",
          eui64[7],
          eui64[6],
          eui64[5],
          eui64[4],
          eui64[3],
          eui64[2],
          eui64[1],
          eui64[0]);

  strcat(mqttClientIdString, EMBER_AF_PLUGIN_TRANSPORT_MQTT_CLIENT_ID_PREFIX);
  strcat(mqttClientIdString, euiString);
  emberSerialPrintfLine(APP_SERIAL,
                        "MQTT Client ID = %s",
                        mqttClientIdString);
  //-客户端ID号是通过mac地址保证唯一的
  MQTTAsync_create(&mqttClient,
                   EMBER_AF_PLUGIN_TRANSPORT_MQTT_BROKER_ADDRESS,
                   mqttClientIdString,
                   MQTTCLIENT_PERSISTENCE_NONE,
                   NULL); // persistence_context is NULL since persistence is NONE

  MQTTAsync_setCallbacks(mqttClient,
                         NULL, // context is NULL, no app context used here
                         mqttConnectionLostCallback,
                         mqttMessageArrivedCallback,
                         NULL); // dc is NULL, MQTTAsync_deliveryComplete unusued

  mqttConnectOptions.keepAliveInterval = MQTT_KEEP_ALIVE_INTERVAL_S;
  mqttConnectOptions.cleansession = 1;
  mqttConnectOptions.onSuccess = mqttConnectSuccessCallback;
  mqttConnectOptions.onFailure = mqttConnectFailureCallback;
  mqttConnectOptions.context = mqttClient;

  mqttSubscribeResponseOptions.onFailure = mqttTopicSubscribeFailureCallack;
  mqttSubscribeResponseOptions.context = mqttClient;

  mqttPublishResponseOptions.onFailure = mqttTopicPublishFailureCallack;
  mqttPublishResponseOptions.context = mqttClient;

  // Attempt to connect
  if (!mqttConnect()) {
    emberEventControlSetDelayMS(emberAfPluginTransportMqttBrokerReconnectEventControl,
                                MQTT_RECONNECT_RATE_MS);
  }
}

static bool mqttConnect(void)
{
  int status = MQTTAsync_connect(mqttClient, &mqttConnectOptions);
  if (status != MQTTASYNC_SUCCESS) {
    emberSerialPrintfLine(APP_SERIAL, "MQTTAsync_connect failed, status=%d", status);
    return false;
  }
  return true;
}

static void mqttConnectFailureCallback(void* context, MQTTAsync_failureData* response)
{
  emberSerialPrintfLine(APP_SERIAL,
                        "MQTTAsync_connect failed, returned response=%d",
                        response ? response->code : 0);
  mqttConnected = FALSE;

  emberEventControlSetDelayMS(emberAfPluginTransportMqttBrokerReconnectEventControl,
                              MQTT_RECONNECT_RATE_MS);
}

void emberAfPluginTransportMqttBrokerReconnectEventHandler(void) 
{
  emberEventControlSetInactive(emberAfPluginTransportMqttBrokerReconnectEventControl);
  emberSerialPrintfLine(APP_SERIAL, "Attempting to reconnect to broker");
  // Attempt to reconnect
  if (!mqttConnect()) {
    emberEventControlSetDelayMS(emberAfPluginTransportMqttBrokerReconnectEventControl,
                                MQTT_RECONNECT_RATE_MS);
  }
}

static void mqttConnectSuccessCallback(void* context, MQTTAsync_successData* response)
{
  emberSerialPrintfLine(APP_SERIAL, "MQTT connected to broker");
  mqttConnected = TRUE;

  // Notify users of the plugin that a connection is complete
  emberAfPluginTransportMqttConnectedCallback();
}

static void mqttConnectionLostCallback(void *context, char *cause)
{
  emberSerialPrintfLine(APP_SERIAL, "MQTT connection lost, cause=%s", cause);

  // Notify users of the plugin that a disconnect occurred
  emberAfPluginTransportMqttDisconnectedCallback();

  emberSerialPrintfLine(APP_SERIAL, "MQTT attempting to reconnect");
  mqttConnectOptions.keepAliveInterval = MQTT_KEEP_ALIVE_INTERVAL_S;
  mqttConnectOptions.cleansession = 1;
  mqttConnected = FALSE;

  // Attempt to reconnect
  if (!mqttConnect()) {
    emberEventControlSetDelayMS(emberAfPluginTransportMqttBrokerReconnectEventControl,
                                MQTT_RECONNECT_RATE_MS);
  }
}

void emberAfPluginTransportMqttSubscribe(const char* topic)
{
  int status;

  if (mqttConnected) {
    emberSerialPrintfLine(APP_SERIAL,
                          "Subscribing to topic \"%s\" using QoS%d",
                          topic,
                          EMBER_AF_PLUGIN_TRANSPORT_MQTT_QOS);

    status = MQTTAsync_subscribe(mqttClient,
                                 topic,
                                 EMBER_AF_PLUGIN_TRANSPORT_MQTT_QOS,
                                 &mqttSubscribeResponseOptions);
    if (status != MQTTASYNC_SUCCESS) {
      emberSerialPrintfLine(APP_SERIAL, "MQTTAsync_subscribe failed, status=%d", status);
    }
  } else {
    emberSerialPrintfLine(APP_SERIAL, "MQTT not connected, cannot subscribe to: %s", topic);
  }
}

static void mqttTopicSubscribeFailureCallack(void* context, MQTTAsync_failureData* response)
{
  emberSerialPrintfLine(APP_SERIAL,
                        "MQTTAsync_subscribe failed, returned response=%d",
                        response ? response->code : 0);
}

static int mqttMessageArrivedCallback(void *context,
                                      char *topicName,
                                      int topicLen,
                                      MQTTAsync_message *message)
{
  if (emberAfPluginTransportMqttMessageArrivedCallback(topicName, message->payload)) {
    MQTTAsync_freeMessage(&message);
    MQTTAsync_free(topicName);
    return TRUE; // Return TRUE to let the MQTT library know we handled the message
  }
  return FALSE; // Return FALSE to let the MQTT library know we did'nt handle the message
}

void emberAfPluginTransportMqttPublish(const char* topic, const char* payload)
{
  int status;
  MQTTAsync_message mqttMessage = MQTTAsync_message_initializer;

  mqttMessage.payload = (char*)payload;
  mqttMessage.payloadlen = strlen(payload);
  mqttMessage.qos = EMBER_AF_PLUGIN_TRANSPORT_MQTT_QOS;
  mqttMessage.retained = MQTT_RETAINED;

  if (mqttConnected) {
    status = MQTTAsync_sendMessage(mqttClient,
                                   topic,
                                   &mqttMessage,
                                   &mqttPublishResponseOptions);
    if (status != MQTTASYNC_SUCCESS) {
      emberSerialPrintfLine(APP_SERIAL, "MQTTAsync_sendMessage failed, status=%d", status);
    }
  } else {
    emberSerialPrintfLine(APP_SERIAL,
                          "MQTT not connected, message not sent: %s - %s",
                          topic,
                          payload);
  }
}

static void mqttTopicPublishFailureCallack(void* context, MQTTAsync_failureData* response)
{
  emberSerialPrintfLine(APP_SERIAL,
                        "MQTTAsync_sendMessage failed, returned response=%d",
                        response ? response->code : 0);
}

