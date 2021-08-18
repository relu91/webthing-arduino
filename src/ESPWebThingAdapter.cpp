#include "ESPWebThingAdapter.h";


WebThingAdapter::WebThingAdapter(String _name, IPAddress _ip, uint16_t _port,
                  bool _disableHostValidation)
      : server(_port), name(_name), ip(_ip.toString()), port(_port), disableHostValidation(_disableHostValidation) {}

void WebThingAdapter::begin() {
    name.toLowerCase();
    if (MDNS.begin(this->name.c_str())) {
      Serial.println("MDNS responder started");
    }

    MDNS.addService("webthing", "tcp", port);
    MDNS.addServiceTxt("webthing", "tcp", "path", "/.well-known/wot-thing-description");

    DefaultHeaders::Instance().addHeader("Access-Control-Allow-Origin", "*");
    DefaultHeaders::Instance().addHeader("Access-Control-Allow-Methods",
                                         "GET, POST, PUT, DELETE, OPTIONS");
    DefaultHeaders::Instance().addHeader(
        "Access-Control-Allow-Headers",
        "Origin, X-Requested-With, Content-Type, Accept");

    this->server.onNotFound(std::bind(&WebThingAdapter::handleUnknown, this,
                                      std::placeholders::_1));

    this->server.on("/*", HTTP_OPTIONS,
                    std::bind(&WebThingAdapter::handleOptions, this,
                              std::placeholders::_1));
    this->server.on("/.well-known/wot-thing-description", HTTP_GET,
                    std::bind(&WebThingAdapter::handleThings, this,
                              std::placeholders::_1));

    ThingDevice *device = this->firstDevice;
    while (device != nullptr) {
      String deviceBase = "/things/" + device->id;

      ThingProperty *property = device->firstProperty;
      while (property != nullptr) {
        String propertyBase = deviceBase + "/properties/" + property->id;
        this->server.on(propertyBase.c_str(), HTTP_GET,
                        std::bind(&WebThingAdapter::handleThingPropertyGet,
                                  this, std::placeholders::_1, property));
        this->server.on(propertyBase.c_str(), HTTP_PUT,
                        std::bind(&WebThingAdapter::handleThingPropertyPut,
                                  this, std::placeholders::_1, device,
                                  property),
                        NULL,
                        std::bind(&WebThingAdapter::handleBody, this,
                                  std::placeholders::_1, std::placeholders::_2,
                                  std::placeholders::_3, std::placeholders::_4,
                                  std::placeholders::_5));

        property = (ThingProperty *)property->next;
      }

      ThingAction *action = device->firstAction;
      while (action != nullptr) {
        String actionBase = deviceBase + "/actions/" + action->id;
        this->server.on(actionBase.c_str(), HTTP_GET,
                        std::bind(&WebThingAdapter::handleThingActionGet, this,
                                  std::placeholders::_1, device, action));
        this->server.on(actionBase.c_str(), HTTP_POST,
                        std::bind(&WebThingAdapter::handleThingActionPost,
                                  this, std::placeholders::_1, device, action),
                        NULL,
                        std::bind(&WebThingAdapter::handleBody, this,
                                  std::placeholders::_1, std::placeholders::_2,
                                  std::placeholders::_3, std::placeholders::_4,
                                  std::placeholders::_5));
        this->server.on(actionBase.c_str(), HTTP_DELETE,
                        std::bind(&WebThingAdapter::handleThingActionDelete,
                                  this, std::placeholders::_1, device,
                                  action));
        action = (ThingAction *)action->next;
      }

      ThingEvent *event = device->firstEvent;
      while (event != nullptr) {
        String eventBase = deviceBase + "/events/" + event->id;
        this->server.on(eventBase.c_str(), HTTP_GET,
                        std::bind(&WebThingAdapter::handleThingEventGet, this,
                                  std::placeholders::_1, device, event));
        event = (ThingEvent *)event->next;
      }

      this->server.on((deviceBase + "/properties").c_str(), HTTP_GET,
                      std::bind(&WebThingAdapter::handleThingPropertiesGet,
                                this, std::placeholders::_1,
                                device->firstProperty));
      this->server.on((deviceBase + "/actions").c_str(), HTTP_GET,
                      std::bind(&WebThingAdapter::handleThingActionsGet, this,
                                std::placeholders::_1, device));
      this->server.on((deviceBase + "/actions").c_str(), HTTP_POST,
                      std::bind(&WebThingAdapter::handleThingActionsPost, this,
                                std::placeholders::_1, device),
                      NULL,
                      std::bind(&WebThingAdapter::handleBody, this,
                                std::placeholders::_1, std::placeholders::_2,
                                std::placeholders::_3, std::placeholders::_4,
                                std::placeholders::_5));
      this->server.on((deviceBase + "/events").c_str(), HTTP_GET,
                      std::bind(&WebThingAdapter::handleThingEventsGet, this,
                                std::placeholders::_1, device));
      this->server.on(deviceBase.c_str(), HTTP_GET,
                      std::bind(&WebThingAdapter::handleThing, this,
                                std::placeholders::_1, device));

      device = device->next;
    }

    this->server.begin();
}

void WebThingAdapter::update() {
  #ifdef ESP8266
    MDNS.update();
  #endif
}

void WebThingAdapter::addDevice(ThingDevice *device) {
    if (this->lastDevice == nullptr) {
      this->firstDevice = device;
      this->lastDevice = device;
    } else {
      this->lastDevice->next = device;
      this->lastDevice = device;
    }
}


bool WebThingAdapter::verifyHost(AsyncWebServerRequest *request) {
    if (disableHostValidation) {
      return true;
    }

    AsyncWebHeader *header = request->getHeader("Host");
    if (header == nullptr) {
      request->send(403);
      return false;
    }
    String value = header->value();
    int colonIndex = value.indexOf(':');
    if (colonIndex >= 0) {
      value.remove(colonIndex);
    }
    if (value.equalsIgnoreCase(name + ".local") || value == ip ||
        value == "localhost") {
      return true;
    }
    request->send(403);
    return false;
}

void WebThingAdapter::handleUnknown(AsyncWebServerRequest *request) {
    if (!verifyHost(request)) {
      return;
    }
    request->send(404);
}

void WebThingAdapter::handleOptions(AsyncWebServerRequest *request) {
    if (!verifyHost(request)) {
      return;
    }
    request->send(204);
}

void WebThingAdapter::handleThings(AsyncWebServerRequest *request) {
    if (!verifyHost(request)) {
      return;
    }
    AsyncResponseStream *response =
        request->beginResponseStream("application/json");

    DynamicJsonDocument buf(LARGE_JSON_DOCUMENT_SIZE);
    JsonObject thing = buf.to<JsonObject>();
    ThingDevice *device = this->firstDevice;
    while (device != nullptr) {
      device->serialize(thing, ip, port);
      thing["href"] = "/things/" + device->id;
      device = device->next;
    }

    serializeJson(thing, *response);
    request->send(response);
}

void WebThingAdapter::handleThing(AsyncWebServerRequest *request, ThingDevice *&device) {
    if (!verifyHost(request)) {
      return;
    }
    AsyncResponseStream *response =
        request->beginResponseStream("application/json");

    DynamicJsonDocument buf(LARGE_JSON_DOCUMENT_SIZE);
    JsonObject descr = buf.to<JsonObject>();
    device->serialize(descr, ip, port);

    serializeJson(descr, *response);
    request->send(response);
}

void WebThingAdapter::handleThingPropertyGet(AsyncWebServerRequest *request,
                              ThingItem *item) {
    if (!verifyHost(request)) {
      return;
    }
    AsyncResponseStream *response =
        request->beginResponseStream("application/json");

    DynamicJsonDocument doc(SMALL_JSON_DOCUMENT_SIZE);
    JsonObject prop = doc.to<JsonObject>();
    item->serializeValue(prop);
    serializeJson(prop, *response);
    request->send(response);
}

void WebThingAdapter::handleThingActionGet(AsyncWebServerRequest *request,
                            ThingDevice *device, ThingAction *action) {
    if (!verifyHost(request)) {
      return;
    }

    String url = request->url();
    String base = "/things/" + device->id + "/actions/" + action->id;
    if (url == base || url == base + "/") {
      AsyncResponseStream *response =
          request->beginResponseStream("application/json");
      DynamicJsonDocument doc(LARGE_JSON_DOCUMENT_SIZE);
      JsonArray queue = doc.to<JsonArray>();
      device->serializeActionQueue(queue, action->id);
      serializeJson(queue, *response);
      request->send(response);
    } else {
      String actionId = url.substring(base.length() + 1);
      const char *actionIdC = actionId.c_str();
      const char *slash = strchr(actionIdC, '/');

      if (slash) {
        actionId = actionId.substring(0, slash - actionIdC);
      }

      ThingActionObject *obj = device->findActionObject(actionId.c_str());
      if (obj == nullptr) {
        request->send(404);
        return;
      }

      AsyncResponseStream *response =
          request->beginResponseStream("application/json");
      DynamicJsonDocument doc(SMALL_JSON_DOCUMENT_SIZE);
      JsonObject o = doc.to<JsonObject>();
      obj->serialize(o, device->id);
      serializeJson(o, *response);
      request->send(response);
    }
}

void WebThingAdapter::handleThingActionDelete(AsyncWebServerRequest *request,
                               ThingDevice *device, ThingAction *action) {
    if (!verifyHost(request)) {
      return;
    }

    String url = request->url();
    String base = "/things/" + device->id + "/actions/" + action->id;
    if (url == base || url == base + "/") {
      request->send(404);
      return;
    }

    String actionId = url.substring(base.length() + 1);
    const char *actionIdC = actionId.c_str();
    const char *slash = strchr(actionIdC, '/');

    if (slash) {
      actionId = actionId.substring(0, slash - actionIdC);
    }

    device->removeAction(actionId);
    request->send(204);
}

void WebThingAdapter::handleThingActionPost(AsyncWebServerRequest *request,
                             ThingDevice *device, ThingAction *action) {
    if (!verifyHost(request)) {
      return;
    }

    if (!b_has_body_data) {
      request->send(422); // unprocessable entity (b/c no body)
      return;
    }

    DynamicJsonDocument *newBuffer =
        new DynamicJsonDocument(SMALL_JSON_DOCUMENT_SIZE);
    auto error = deserializeJson(*newBuffer, (const char *)body_data);
    if (error) { // unable to parse json
      b_has_body_data = false;
      memset(body_data, 0, sizeof(body_data));
      request->send(500);
      delete newBuffer;
      return;
    }

    JsonObject newAction = newBuffer->as<JsonObject>();

    if (!newAction.containsKey(action->id)) {
      b_has_body_data = false;
      memset(body_data, 0, sizeof(body_data));
      request->send(400);
      delete newBuffer;
      return;
    }

    ThingActionObject *obj = device->requestAction(newBuffer);

    if (obj == nullptr) {
      b_has_body_data = false;
      memset(body_data, 0, sizeof(body_data));
      request->send(500);
      delete newBuffer;
      return;
    }

    DynamicJsonDocument respBuffer(SMALL_JSON_DOCUMENT_SIZE);
    JsonObject item = respBuffer.to<JsonObject>();
    obj->serialize(item, device->id);
    String jsonStr;
    serializeJson(item, jsonStr);
    AsyncWebServerResponse *response =
        request->beginResponse(201, "application/json", jsonStr);
    request->send(response);

    b_has_body_data = false;
    memset(body_data, 0, sizeof(body_data));

    obj->start();
}

void WebThingAdapter::handleThingEventGet(AsyncWebServerRequest *request, ThingDevice *device,
                           ThingItem *item) {
    if (!verifyHost(request)) {
      return;
    }
    AsyncResponseStream *response =
        request->beginResponseStream("application/json");

    DynamicJsonDocument doc(LARGE_JSON_DOCUMENT_SIZE);
    JsonArray queue = doc.to<JsonArray>();
    device->serializeEventQueue(queue, item->id);
    serializeJson(queue, *response);
    request->send(response);
}

void WebThingAdapter::handleThingPropertiesGet(AsyncWebServerRequest *request,
                                ThingItem *rootItem) {
    if (!verifyHost(request)) {
      return;
    }
    AsyncResponseStream *response =
        request->beginResponseStream("application/json");

    DynamicJsonDocument doc(LARGE_JSON_DOCUMENT_SIZE);
    JsonObject prop = doc.to<JsonObject>();
    ThingItem *item = rootItem;
    while (item != nullptr) {
      item->serializeValue(prop);
      item = item->next;
    }
    serializeJson(prop, *response);
    request->send(response);
}

void WebThingAdapter::handleThingActionsGet(AsyncWebServerRequest *request,
                             ThingDevice *device) {
    if (!verifyHost(request)) {
      return;
    }
    AsyncResponseStream *response =
        request->beginResponseStream("application/json");

    DynamicJsonDocument doc(LARGE_JSON_DOCUMENT_SIZE);
    JsonArray queue = doc.to<JsonArray>();
    device->serializeActionQueue(queue);
    serializeJson(queue, *response);
    request->send(response);
}


void WebThingAdapter::handleThingActionsPost(AsyncWebServerRequest *request,
                              ThingDevice *device) {
    if (!verifyHost(request)) {
      return;
    }

    if (!b_has_body_data) {
      request->send(422); // unprocessable entity (b/c no body)
      return;
    }

    DynamicJsonDocument *newBuffer =
        new DynamicJsonDocument(SMALL_JSON_DOCUMENT_SIZE);
    auto error = deserializeJson(*newBuffer, (const char *)body_data);
    if (error) { // unable to parse json
      b_has_body_data = false;
      memset(body_data, 0, sizeof(body_data));
      request->send(500);
      delete newBuffer;
      return;
    }

    JsonObject newAction = newBuffer->as<JsonObject>();

    if (newAction.size() != 1) {
      b_has_body_data = false;
      memset(body_data, 0, sizeof(body_data));
      request->send(400);
      delete newBuffer;
      return;
    }

    ThingActionObject *obj = device->requestAction(newBuffer);

    if (obj == nullptr) {
      b_has_body_data = false;
      memset(body_data, 0, sizeof(body_data));
      request->send(500);
      delete newBuffer;
      return;
    }

    DynamicJsonDocument respBuffer(SMALL_JSON_DOCUMENT_SIZE);
    JsonObject item = respBuffer.to<JsonObject>();
    obj->serialize(item, device->id);
    String jsonStr;
    serializeJson(item, jsonStr);
    AsyncWebServerResponse *response =
        request->beginResponse(201, "application/json", jsonStr);
    request->send(response);

    b_has_body_data = false;
    memset(body_data, 0, sizeof(body_data));

    obj->start();
}

void WebThingAdapter::handleThingEventsGet(AsyncWebServerRequest *request,
                            ThingDevice *device) {
    if (!verifyHost(request)) {
      return;
    }
    AsyncResponseStream *response =
        request->beginResponseStream("application/json");

    DynamicJsonDocument doc(LARGE_JSON_DOCUMENT_SIZE);
    JsonArray queue = doc.to<JsonArray>();
    device->serializeEventQueue(queue);
    serializeJson(queue, *response);
    request->send(response);
}

void WebThingAdapter::handleBody(AsyncWebServerRequest *request, uint8_t *data, size_t len,
                  size_t index, size_t total) {
    if (total >= ESP_MAX_PUT_BODY_SIZE ||
        index + len >= ESP_MAX_PUT_BODY_SIZE) {
      return; // cannot store this size..
    }
    // copy to internal buffer
    memcpy(&body_data[index], data, len);
    b_has_body_data = true;
}

void WebThingAdapter::handleThingPropertyPut(AsyncWebServerRequest *request,
                              ThingDevice *device, ThingProperty *property) {
    if (!verifyHost(request)) {
      return;
    }
    if (!b_has_body_data) {
      request->send(422); // unprocessable entity (b/c no body)
      return;
    }

    DynamicJsonDocument newBuffer(SMALL_JSON_DOCUMENT_SIZE);
    auto error = deserializeJson(newBuffer, body_data);
    if (error) { // unable to parse json
      b_has_body_data = false;
      memset(body_data, 0, sizeof(body_data));
      request->send(500);
      return;
    }
    JsonObject newProp = newBuffer.as<JsonObject>();

    if (!newProp.containsKey(property->id)) {
      b_has_body_data = false;
      memset(body_data, 0, sizeof(body_data));
      request->send(400);
      return;
    }

    device->setProperty(property->id.c_str(), newProp[property->id]);

    AsyncResponseStream *response =
        request->beginResponseStream("application/json");
    serializeJson(newProp, *response);
    request->send(response);

    b_has_body_data = false;
    memset(body_data, 0, sizeof(body_data));
}
