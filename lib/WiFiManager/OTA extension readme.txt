Required changes to original WiFiManager code
WiFiManager.h, add
  const char HTTP_UPDATE[] PROGMEM          = "<form method='POST' action='update' enctype='multipart/form-data'><input type='file' name='update'><input type='submit' value='Update'></form>";
  const char HTTP_UPDATE_FAI[] PROGMEM      = "Update Failed!";
  const char HTTP_UPDATE_SUC[] PROGMEM      = "Update Success! Rebooting...";
  const char HTTP_PORTAL_OPTIONS[] PROGMEM  = "... append <br/><form action=\"/u\" method=\"get\"><button>FW update</button></form>
WiFiManager.h, class WiFiManager, add to public
  void          setResetCallback( void (*func)(void) );
WiFiManager.h, class WiFiManager, add to private
  void (*_resetcallback)(void) = NULL;
  void          handleUpdate();
  void          handleUpdating();
  void          handleUpdateDone();
WiFiManager.h.cpp, void WiFiManager::setupConfigPortal, add
  server->on("/u", std::bind(&WiFiManagerOTA::handleUpdate, this)); // handler for the /update form page
  server->on("/update", HTTP_POST, std::bind(&WiFiManagerOTA::handleUpdateDone, this), std::bind(&WiFiManagerOTA::handleUpdating, this));
WiFiManager.h.cpp, void WiFiManager::handleReset(), add
  if ( _resetcallback != NULL) {
    _resetcallback();
  }
WiFiManager.h.cpp, add
00    void WiFiManager::setResetCallback( void (*func)(void) ) {
      _resetcallback = func;
    }
  void WiFiManager::handleUpdate() {
    DEBUG_WM(F("Handle update"));
    if (captivePortal()) { // If caprive portal redirect instead of displaying the page.
      return;
    }
    String page = FPSTR(HTTP_HEAD);
    page.replace("{v}", "Options");
    page += FPSTR(HTTP_SCRIPT);
    page += FPSTR(HTTP_STYLE);
    page += _customHeadElement;
    page += FPSTR(HTTP_HEAD_END);
    page += "<h1>";
    page += _apName;
    page += "</h1>";
    page += F("<h3>WiFiManager</h3>");
    page += FPSTR(HTTP_UPDATE);
    page += FPSTR(HTTP_END);
    server->send(200, "text/html", page);
  }

  void WiFiManager::handleUpdating() {
    if (captivePortal()) { // If caprive portal redirect instead of displaying the page.
      return;
    }
    // handler for the file upload, get's the sketch bytes, and writes
    // them through the Update object
    HTTPUpload& upload = server->upload();
    if(upload.status == UPLOAD_FILE_START){
      Serial.setDebugOutput(true);

      WiFiUDP::stopAll();
      Serial.printf("Update: %s\n", upload.filename.c_str());
      uint32_t maxSketchSpace = (ESP.getFreeSketchSpace() - 0x1000) & 0xFFFFF000;
      if(!Update.begin(maxSketchSpace)){//start with max available size
        Update.printError(Serial);
      }
    } else if(upload.status == UPLOAD_FILE_WRITE){
      Serial.printf(".");
      if(Update.write(upload.buf, upload.currentSize) != upload.currentSize){
        Update.printError(Serial);
      }
    } else if(upload.status == UPLOAD_FILE_END){
      if(Update.end(true)){ //true to set the size to the current progress
        Serial.printf("Update Success: %u\nRebooting...\n", upload.totalSize);
      } else {
        Update.printError(Serial);
      }
      Serial.setDebugOutput(false);
    } else if(upload.status == UPLOAD_FILE_ABORTED){
      Update.end();
      DEBUG_WM(F("Update was aborted"));
    }
    delay(0);
  }

  void WiFiManager::handleUpdateDone() {
    DEBUG_WM(F("Handle update done"));
    if (captivePortal()) { // If caprive portal redirect instead of displaying the page.
      return;
    }
    String page = FPSTR(HTTP_HEAD);
    page.replace("{v}", "Options");
    page += FPSTR(HTTP_SCRIPT);
    page += FPSTR(HTTP_STYLE);
    page += _customHeadElement;
    page += FPSTR(HTTP_HEAD_END);
    page += "<h1>";
    page += _apName;
    page += "</h1>";
    page += F("<h3>WiFiManager</h3>");
    if(Update.hasError()){
      page += FPSTR(HTTP_UPDATE_FAI);
      DEBUG_WM(F("update failed"));
    } else {
      page += FPSTR(HTTP_UPDATE_SUC);
      DEBUG_WM(F("update done"));
    }
    page += FPSTR(HTTP_END);
    server->send(200, "text/html", page);
    delay(1000); // send page
    ESP.restart();
  }
