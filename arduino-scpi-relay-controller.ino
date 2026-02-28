#include "Arduino.h"
#include "Vrekrer_scpi_parser.h"


// scpi kodu 
// --------------------------------------
// Channel -> Arduino pin mapping
// CH0..CH6  =>  D2..D8  (Serial 0-1 kullanılmaz)
// --------------------------------------
static const uint8_t relayPins[] = {2, 3, 4, 5, 6, 7, 8};
static const uint8_t RELAY_COUNT = sizeof(relayPins) / sizeof(relayPins[0]);

SCPI_Parser scpi;

// Basit bir "uygulama hatası" saklayalım (parametre yanlış vs.)
static String app_error = "";

// --------------------------------------
// Helpers
// --------------------------------------
static int parseOutputSuffix(SCPI_C commands) {
  // OUTPut0:STATe  -> commands[0] muhtemelen "OUTPut0" olur
  // Kütüphane numeric suffix örneğinde sscanf kullanılmıştı.
  String first = String(commands[0]);
  first.toUpperCase();

  int suffix = -1;
  // "OUTPUT" kelimesinden sonra gelen sayıyı çek
  // OUTP gibi kısaltmalar da gelebilir; pratikte OUTPUT/OUTP ikisini de dene.
  if (first.startsWith("OUTP")) {
    // "OUTP0" veya "OUTPUT0"
    // %* ile OUTP/OUTPUT kısmını atlayıp sayıyı alacağız
    // "OUTPUT" için de çalışsın diye karakter kümesi kullanalım:
    // Not: sscanf pattern'ı basit tutuyoruz.
    if (first.startsWith("OUTPUT")) {
      sscanf(first.c_str(), "%*[OUTPUT]%d", &suffix);
    } else {
      sscanf(first.c_str(), "%*[OUTP]%d", &suffix);
    }
  }
  return suffix;
}

static bool parseBoolParam(const String& p, bool &outState) {
  String s = p;
  s.trim();
  s.toUpperCase();

  if (s == "1" || s == "ON" || s == "HIGH" || s == "TRUE") { outState = true;  return true; }
  if (s == "0" || s == "OFF"|| s == "LOW"  || s == "FALSE"){ outState = false; return true; }
  return false;
}

static void setAllRelays(bool state) {
  for (uint8_t i = 0; i < RELAY_COUNT; i++) {
    digitalWrite(relayPins[i], state ? HIGH : LOW);
  }
}

static void clearErrors() {
  scpi.last_error = SCPI_Parser::ErrorCode::NoError;
  app_error = "";
}

// --------------------------------------
// SCPI Callbacks
// --------------------------------------
void Identify(SCPI_C commands, SCPI_P parameters, Stream& interface) {
  // SCPI önerilen format: "Manufacturer,Model,Serial,Firmware"
  interface.println(F("HalilMutlu,ArduinoRelaySCPI,0001," VREKRER_SCPI_VERSION));
}

void ResetInstrument(SCPI_C commands, SCPI_P parameters, Stream& interface) {
  setAllRelays(false);
  clearErrors();
  // İstersen cevap verme; ama debug için kısa mesaj faydalı
  interface.println(F("OK"));
}

void ClearStatus(SCPI_C commands, SCPI_P parameters, Stream& interface) {
  clearErrors();
  interface.println(F("OK"));
}

void GetLastError(SCPI_C commands, SCPI_P parameters, Stream& interface) {
  // Önce parser kaynaklı hatalar
  switch (scpi.last_error) {
    case SCPI_Parser::ErrorCode::BufferOverflow:
      interface.println(F("SCPI:BUFFER_OVERFLOW"));
      scpi.last_error = SCPI_Parser::ErrorCode::NoError;
      return;
    case SCPI_Parser::ErrorCode::Timeout:
      interface.println(F("SCPI:TIMEOUT"));
      scpi.last_error = SCPI_Parser::ErrorCode::NoError;
      return;
    case SCPI_Parser::ErrorCode::UnknownCommand:
      interface.println(F("SCPI:UNKNOWN_COMMAND"));
      scpi.last_error = SCPI_Parser::ErrorCode::NoError;
      return;
    case SCPI_Parser::ErrorCode::NoError:
      break;
  }

  // Sonra bizim uygulama hataları
  if (app_error.length() > 0) {
    interface.println(app_error);
    app_error = "";
    return;
  }

  interface.println(F("0,NO_ERROR"));
}

void myErrorHandler(SCPI_C commands, SCPI_P parameters, Stream& interface) {
  // BufferOverflow olursa gelen mesajın kalanını flush etmek iyi olur
  if (scpi.last_error == SCPI_Parser::ErrorCode::BufferOverflow) {
    delay(2);
    while (interface.available()) {
      delay(2);
      interface.read();
    }
  }
}

// OUTPut#:STATe <0|1|ON|OFF>
void SetOutputState(SCPI_C commands, SCPI_P parameters, Stream& interface) {
  int ch = parseOutputSuffix(commands);
  if (ch < 0 || ch >= (int)RELAY_COUNT) {
    app_error = F("200,CHANNEL_OUT_OF_RANGE");
    return;
  }

  if (parameters.Size() < 1) {
    app_error = F("201,MISSING_PARAMETER");
    return;
  }

  bool state = false;
  if (!parseBoolParam(String(parameters[0]), state)) {
    app_error = F("202,INVALID_PARAMETER");
    return;
  }

  digitalWrite(relayPins[ch], state ? HIGH : LOW);
  // Set komutlarında genelde cevap şart değil; ama "OK" debug için iyi
  interface.println(F("OK"));
}

// OUTPut#:STATe?
void QueryOutputState(SCPI_C commands, SCPI_P parameters, Stream& interface) {
  int ch = parseOutputSuffix(commands);
  if (ch < 0 || ch >= (int)RELAY_COUNT) {
    app_error = F("200,CHANNEL_OUT_OF_RANGE");
    return;
  }

  int val = digitalRead(relayPins[ch]) == HIGH ? 1 : 0;
  interface.println(val); // SCPI’de state query genelde 1/0 döner
}

// OUTPut:ALL:STATe <0|1|ON|OFF>
void SetAllOutputState(SCPI_C commands, SCPI_P parameters, Stream& interface) {
  if (parameters.Size() < 1) {
    app_error = F("201,MISSING_PARAMETER");
    return;
  }

  bool state = false;
  if (!parseBoolParam(String(parameters[0]), state)) {
    app_error = F("202,INVALID_PARAMETER");
    return;
  }

  setAllRelays(state);
  interface.println(F("OK"));
}

// OUTPut:ALL:STATe?
void QueryAllOutputState(SCPI_C commands, SCPI_P parameters, Stream& interface) {
  // Hepsi 1 mi? Hepsi 0 mı? Karışık mı?
  bool anyHigh = false, anyLow = false;
  for (uint8_t i = 0; i < RELAY_COUNT; i++) {
    if (digitalRead(relayPins[i]) == HIGH) anyHigh = true;
    else anyLow = true;
  }

  if (anyHigh && anyLow) interface.println(F("MIXED"));
  else interface.println(anyHigh ? F("1") : F("0"));
}

// --------------------------------------
// Arduino
// --------------------------------------
void setup() {
  Serial.begin(9600);

  for (uint8_t i = 0; i < RELAY_COUNT; i++) {
    pinMode(relayPins[i], OUTPUT);
    digitalWrite(relayPins[i], LOW);
  }

  // Error handler
  scpi.SetErrorHandler(&myErrorHandler);

  // Core SCPI commands
  scpi.RegisterCommand(F("*IDN?"), &Identify);
  scpi.RegisterCommand(F("*RST"),  &ResetInstrument);
  scpi.RegisterCommand(F("*CLS"),  &ClearStatus);
  scpi.RegisterCommand(F("SYSTem:ERRor?"), &GetLastError);

  // Output control (numeric suffix)
  // Örnek kullanım:
  //   OUTP0:STAT ON
  //   OUTPut6:STATe 0
  //   OUTP3:STAT?
  scpi.RegisterCommand(F("OUTPut#:STATe"),  &SetOutputState);
  scpi.RegisterCommand(F("OUTPut#:STATe?"), &QueryOutputState);

  // All outputs
  //   OUTP:ALL:STAT ON
  //   OUTP:ALL:STAT?
  scpi.RegisterCommand(F("OUTPut:ALL:STATe"),  &SetAllOutputState);
  scpi.RegisterCommand(F("OUTPut:ALL:STATe?"), &QueryAllOutputState);
}

void loop() {
  // Hem \r hem \n kabul etsin
  scpi.ProcessInput(Serial, "\r\n");
}
