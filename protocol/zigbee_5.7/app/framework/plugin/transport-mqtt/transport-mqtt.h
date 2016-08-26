// Copyright 2016 Silicon Laboratories, Inc.                                *80*

#ifndef __TRANSPORT_MQTT_H
#define __TRANSPORT_MQTT_H

/** @brief MQTT Client Connected Callback
 *
 * This function will be called when the MQTT client for the gateway
 * successfully connects. This lets the application know it can start
 * subscribing to topics.
 */
void emberAfPluginTransportMqttConnectedCallback(void);

/** @brief MQTT Client Disconnected Callback
 *
 * This function will be called when the MQTT client for the gateway
 * disconnects. This lets the application know that the connection for the
 * broker has gone down.
 */
void emberAfPluginTransportMqttDisconnectedCallback(void);

/** @brief MQTT Subscribe
 *
 * This function should be called to subscribe to a specific topic.
 *
 * @param topic String containing the topic for a message subscription
 */
void emberAfPluginTransportMqttSubscribe(const char* topic);

/** @brief MQTT Message Arrived
 *
 * This function will be called when the MQTT client for the gateway receives
 * an incoming message on a topic. If the message is processed by the application
 * TRUE should be returned, if the message is not processed return FALSE.
 *
 * @param topic String containing the topic for the message that arrived
 * @param payload String containing the payload for the message that arrived
 */
boolean emberAfPluginTransportMqttMessageArrivedCallback(const char* topic,
	                                                     const char* payload);

/** @brief MQTT Publish
 *
 * This function should be called to publish to a specific topic.
 *
 * @param topic String containing the topic for the message to be published
 * @param content String containing the payload for the message to be published
 */
void emberAfPluginTransportMqttPublish(const char* topic, const char* paylaod);

#endif //__TRANSPORT_MQTT_H
