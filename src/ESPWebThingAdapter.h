/**
 * ESPWebThingAdapter.h
 *
 * Exposes the Web Thing API based on provided ThingDevices.
 * Suitable for ESP32 and ESP8266 using ESPAsyncWebServer and ESPAsyncTCP
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#ifndef ESP_WEBTHING_ADAPTER_H
#define ESP_WEBTHING_ADAPTER_H

#define ARDUINOJSON_USE_LONG_LONG 1

#include <ArduinoJson.h>
#include <ESPAsyncWebServer.h>

#ifdef ESP8266
#include <ESP8266mDNS.h>
#else
#include <ESPmDNS.h>
#endif
#include "Thing.h"

#define ESP_MAX_PUT_BODY_SIZE 512

#ifndef LARGE_JSON_DOCUMENT_SIZE
#ifdef LARGE_JSON_BUFFERS
#define LARGE_JSON_DOCUMENT_SIZE 4096
#else
#define LARGE_JSON_DOCUMENT_SIZE 1024
#endif
#endif

#ifndef SMALL_JSON_DOCUMENT_SIZE
#ifdef LARGE_JSON_BUFFERS
#define SMALL_JSON_DOCUMENT_SIZE 1024
#else
#define SMALL_JSON_DOCUMENT_SIZE 256
#endif
#endif

class WebThingAdapter {
public:
  WebThingAdapter(String _name, IPAddress _ip, uint16_t _port = 80,
                  bool _disableHostValidation = false);

  void begin();
  void update();
  void addDevice(ThingDevice *device);

private:
  AsyncWebServer server;

  String name;
  String ip;
  uint16_t port;
  bool disableHostValidation;
  ThingDevice *firstDevice = nullptr;
  ThingDevice *lastDevice = nullptr;
  char body_data[ESP_MAX_PUT_BODY_SIZE];
  bool b_has_body_data = false;

  bool verifyHost(AsyncWebServerRequest *request);

  void handleUnknown(AsyncWebServerRequest *request);

  void handleOptions(AsyncWebServerRequest *request);

  void handleThings(AsyncWebServerRequest *request);

  void handleThing(AsyncWebServerRequest *request, ThingDevice *&device);

  void handleThingPropertyGet(AsyncWebServerRequest *request, ThingItem *item);

  void handleThingActionGet(AsyncWebServerRequest *request,
                            ThingDevice *device, ThingAction *action);

  void handleThingActionDelete(AsyncWebServerRequest *request,
                               ThingDevice *device, ThingAction *action);

  void handleThingActionPost(AsyncWebServerRequest *request,
                             ThingDevice *device, ThingAction *action);

  void handleThingEventGet(AsyncWebServerRequest *request, ThingDevice *device,
                           ThingItem *item);

  void handleThingPropertiesGet(AsyncWebServerRequest *request,
                                ThingItem *rootItem);

  void handleThingActionsGet(AsyncWebServerRequest *request,
                             ThingDevice *device);

  void handleThingActionsPost(AsyncWebServerRequest *request,
                              ThingDevice *device);

  void handleThingEventsGet(AsyncWebServerRequest *request,
                            ThingDevice *device);

  void handleBody(AsyncWebServerRequest *request, uint8_t *data, size_t len,
                  size_t index, size_t total);

  void handleThingPropertyPut(AsyncWebServerRequest *request,
                              ThingDevice *device, ThingProperty *property);

};

#endif //ESP_WEBTHING_ADAPTER_H