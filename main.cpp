#include <Arduino.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <esp_camera.h>
#include <UniversalTelegramBot.h>
#include <ArduinoJson.h>

// Configurazione Wi-Fi 
const char* WIFI_SSID = "Nome Rete Wifi";
const char* WIFI_PASSWORD = "Password Rete Wifi";

// Configurazione Telegram 
#define BOT_TOKEN "TOKEN BOT"
const String authorizedIds[] = {"TOKEN ID 1", "TOKEN ID 2"};

// Funzione semplice per verificare autorizzazione
bool isAuthorized(String chat_id) {
    for (String id : authorizedIds) {
        if (chat_id == id) return true;
    }
    return false;
}


// Timer per interrogare il bot
const unsigned long BOT_CHECK_INTERVAL = 1000;  // Controlla nuovi messaggi ogni 1 secondo
unsigned long lastBotCheck = 0;

// Client sicuro per Telegram e bot
WiFiClientSecure telegramClient;
UniversalTelegramBot bot(BOT_TOKEN, telegramClient);

// Pin della camera AI-Thinker ESP32-CAM (definiti manualmente)
#define PWDN_GPIO_NUM     32  // Power down pin della camera
#define RESET_GPIO_NUM    -1  // Non usato (reset hardware camera)
#define XCLK_GPIO_NUM      0  // Clock pin
#define SIOD_GPIO_NUM     26  // I2C SDA (configurazione camera)
#define SIOC_GPIO_NUM     27  // I2C SCL
#define Y9_GPIO_NUM       35  // Y9
#define Y8_GPIO_NUM       34  // Y8
#define Y7_GPIO_NUM       39  // Y7
#define Y6_GPIO_NUM       36  // Y6
#define Y5_GPIO_NUM       21  // Y5
#define Y4_GPIO_NUM       19  // Y4
#define Y3_GPIO_NUM       18  // Y3
#define Y2_GPIO_NUM        5  // Y2
#define VSYNC_GPIO_NUM    25  // VSYNC
#define HREF_GPIO_NUM     23  // HREF
#define PCLK_GPIO_NUM     22  // PCLK
#define LED_FLASH_PIN      4  // LED flash (integrato sulla ESP32-CAM)

// Server HTTP per streaming
#include <esp_http_server.h>
httpd_handle_t stream_httpd = NULL;

unsigned long flashTimer = 0;
const unsigned long FLASH_TIMEOUT = 300000; // 5 minuti (300 secondi)
bool flashAttivo = false;

// Funzioni per gestione flash
void accendiFlash(){
  digitalWrite(LED_FLASH_PIN, HIGH);
  flashAttivo = true;
  flashTimer = millis();
}

void spegniFlash(){
  digitalWrite(LED_FLASH_PIN, LOW);
  flashAttivo = false;
}

void toggleFlash() {
  if (flashAttivo) {
    spegniFlash();
  } else {
    accendiFlash();
  }
}

// Handler per pagina di streaming (MJPEG stream)
esp_err_t stream_handler(httpd_req_t *req) {
  camera_fb_t * fb = NULL;
  esp_err_t res = ESP_OK;

  // Imposta header HTTP per streaming MJPEG
  httpd_resp_set_type(req, "multipart/x-mixed-replace; boundary=FRAME");
  httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");

  Serial.println("Stream: client connesso, inizio invio frames...");
  while(true) {
    fb = esp_camera_fb_get();
    if (!fb) {
      Serial.println("Errore acquisizione frame camera");
      res = ESP_FAIL;
    } else {
      // Invia boundary e header per frame
      char partHeader[64];
      size_t headerLen = snprintf(partHeader, sizeof(partHeader),
                        "--FRAME\r\nContent-Type: image/jpeg\r\nContent-Length: %u\r\n\r\n", fb->len);
      res = httpd_resp_send_chunk(req, partHeader, headerLen);
      if(res == ESP_OK) {
        // Invia i dati dell'immagine
        res = httpd_resp_send_chunk(req, (const char*)fb->buf, fb->len);
      }
      // Invia terminatore di frame
      if(res == ESP_OK) {
        res = httpd_resp_send_chunk(req, "\r\n", 2);
      }
    }

    // Rilascia il buffer (ritorna al driver)
    if(fb) esp_camera_fb_return(fb);
    if(res != ESP_OK) {
      // Se c'è un errore, esce dal loop
      break;
    }
  }

  Serial.println("Stream: client disconnesso.");
  httpd_resp_send_chunk(req, "--FRAME--\r\n", 10);
  return res;
}

esp_err_t index_handler(httpd_req_t *req) {
  const char* resp_str = 
    "<!DOCTYPE html><html>"
    "<head><title>ESP32-CAM</title>"
    "<style>"
      "body { text-align: center; font-family: Arial, sans-serif; }"
      "#stream { max-width:100%; height:auto; }"
      ".container { margin-top:20px; }"
      ".btn { font-size: 20px; padding: 10px; margin: 10px; cursor:pointer; }"
    "</style>"
    "</head>"
    "<body>"
      "<h1>ESP32-CAM Streaming</h1>"
      "<img id='stream' src='/stream' />"
      "<div class='container'>"
        "<button class='btn' onclick=\"fetch('/flashon').then(()=>alert('Flash acceso'))\">🔦 Flash ON</button>"
        "<button class='btn' onclick=\"fetch('/flashoff').then(()=>alert('Flash spento'))\">❌ Flash OFF</button>"
      "</div>"
    "</body></html>";
  httpd_resp_send(req, resp_str, strlen(resp_str));
  return ESP_OK;
}

// Avvia server HTTP e registra gli endpoint
void startCameraServer() {
  httpd_config_t config = HTTPD_DEFAULT_CONFIG();
  config.server_port = 80;
  config.ctrl_port = 32768;

  if (httpd_start(&stream_httpd, &config) == ESP_OK) {
    // URI per streaming
    httpd_uri_t stream_uri = {
      .uri       = "/stream",
      .method    = HTTP_GET,
      .handler   = stream_handler,
      .user_ctx  = NULL
    };
    httpd_register_uri_handler(stream_httpd, &stream_uri);

    // URI per index
    httpd_uri_t index_uri = {
      .uri       = "/",
      .method    = HTTP_GET,
      .handler   = index_handler,
      .user_ctx  = NULL
    };
    httpd_register_uri_handler(stream_httpd, &index_uri);

    // URI per accendere LED Flash
    httpd_uri_t flash_on_uri = {
      .uri       = "/flashon",
      .method    = HTTP_GET,
      .handler   = [](httpd_req_t *req) -> esp_err_t {
        digitalWrite(LED_FLASH_PIN, HIGH);
        flashAttivo = true;
        flashTimer = millis();
        httpd_resp_sendstr(req, "Flash ON");
        return ESP_OK;
      },
      .user_ctx  = NULL
    };
    httpd_register_uri_handler(stream_httpd, &flash_on_uri);

    // URI per spegnere LED Flash
    httpd_uri_t flash_off_uri = {
      .uri       = "/flashoff",
      .method    = HTTP_GET,
      .handler   = [](httpd_req_t *req) -> esp_err_t {
        digitalWrite(LED_FLASH_PIN, LOW);
        flashAttivo = false;
        httpd_resp_sendstr(req, "Flash OFF");
        return ESP_OK;
      },
      .user_ctx  = NULL
    };
    httpd_register_uri_handler(stream_httpd, &flash_off_uri);
  }
}

// Funzione per inviare foto su Telegram (ritorna true se ok)
bool sendPhotoTelegram(const String& chat_id) {
  Serial.println("Preparazione foto per Telegram...");
  accendiFlash();              // Accendi flash prima della foto
  delay(1200);                  // pausa per stabilizzare la luce
  // Scatta una foto (gettando la prima immagine se necessario)
  camera_fb_t * fb = esp_camera_fb_get();
  spegniFlash();               // Spegni il flash dopo aver acquisito l'immagine
  if(!fb) {
    Serial.println("Foto fallita (frame buffer nullo)");
    return false;
  }
  // (Opzionale) scarta il primo frame se necessario: 
  // esp_camera_fb_return(fb);
  // fb = esp_camera_fb_get();

  // Configura connessione HTTPS al server Telegram
  WiFiClientSecure client;
  client.setInsecure();  // disabilita verifica certificato per semplicità
  if (!client.connect("api.telegram.org", 443)) {
    Serial.println("Connessione a Telegram API fallita");
    esp_camera_fb_return(fb);
    return false;
  }
  Serial.println("Connesso a Telegram API, invio foto...");

  String head = "--BOUNDARY\r\nContent-Disposition: form-data; name=\"chat_id\";\r\n\r\n" 
              + chat_id +"\r\n--BOUNDARY\r\nContent-Disposition: form-data; name=\"photo\"; filename=\"cam.jpg\"\r\nContent-Type: image/jpeg\r\n\r\n";
  String tail = "\r\n--BOUNDARY--\r\n";


  // Componi richiesta HTTP POST
  client.print(String("POST /bot") + BOT_TOKEN + "/sendPhoto HTTP/1.1\r\n");
  client.print("Host: api.telegram.org\r\n");
  client.print("Content-Type: multipart/form-data; boundary=BOUNDARY\r\n");

  // Calcola lunghezza contenuto (header + immagine + footer)
  size_t contentLength = head.length() + fb->len + tail.length();
  client.print("Content-Length: " + String(contentLength) + "\r\n\r\n");

  // Invia head
  client.print(head);

  // Invia il buffer dell'immagine a blocchi
  size_t bytesSent = 0;
  while (bytesSent < fb->len) {
    size_t chunkSize = 1024;
    if (bytesSent + chunkSize > fb->len) {
      chunkSize = fb->len - bytesSent;
    }
    client.write(fb->buf + bytesSent, chunkSize);
    bytesSent += chunkSize;
  }

  // Invia footer
  client.print(tail);

  // Libera il frame buffer della foto
  esp_camera_fb_return(fb);

  // Legge la risposta dal server (per assicurarsi che Telegram abbia ricevuto)
  String response;
  int timeout = 10000;
  long startTime = millis();
  while (client.connected() && millis() - startTime < timeout) {
    while (client.available()) {
      char c = client.read();
      response += c;
    }
    if (response.indexOf("\"ok\":true") != -1) {
      // Telegram ha risposto con ok (foto inviata con successo)
      break;
    }
  }
  client.stop();
  Serial.println("Risposta Telegram: " + response);  
  // Controlla se nell'output c'è "ok":true
  return (response.indexOf("\"ok\":true") != -1);
}

// Gestisce nuovi messaggi/callback dal bot
void handleNewMessages(int numNewMessages) {
  for (int i = 0; i < numNewMessages; i++) {
    String chat_id = bot.messages[i].chat_id;
    String text    = bot.messages[i].text;
    String from_id = bot.messages[i].from_id;

    if (bot.messages[i].type == F("callback_query")) {
      // Gestione pressione pulsanti inline
      String data = bot.messages[i].text;  
      Serial.printf("Callback dal bot: %s\n", data.c_str());
      if (data == "get_photo") {
        // Utente ha premuto "Photo"
        if(sendPhotoTelegram(chat_id)) {
          bot.sendMessage(chat_id, "Foto inviata ✅", "");
        } else {
          bot.sendMessage(chat_id, "Errore nell'invio della foto ❌", "");
        }
      } 
      else if (data == "toggle_flash") {
        toggleFlash();
        bot.sendMessage(chat_id, flashAttivo ? "🔦 Flash acceso" : "❌ Flash spento", "");
      }
      // (Opzionale: si potrebbe gestire un callback per avviare/fermare streaming se non usiamo URL)
    } 
    else {
      // Gestione normali messaggi testuali
      Serial.printf("Messaggio da %s: %s\n", chat_id.c_str(), text.c_str());
      if (!isAuthorized(chat_id)) {
        // Ignora messaggi da ID non autorizzati
        Serial.println("Utente non autorizzato, messaggio ignorato.");
        continue;
      }
      if (text == "/start") {
        // Messaggio di benvenuto e tastiera con bottoni
        String welcome = "🤖 *AquilaCam*\n";
        welcome += "Benvenuto! 🐱\n";
        welcome += "Usa /photo per ricevere una foto 📷 o /stream per vedere il live streaming 🔴.\n";
        welcome += "Oppure usa i bottoni qui sotto:";
        // Definisce tastiera inline con pulsanti Photo e Stream
        String keyboardJson = "["
        "[{\"text\":\"📷 Photo\",\"callback_data\":\"get_photo\"},"
        "{\"text\":\"🔴 Stream\",\"url\":\"http://NOMESERVIZIO.zapto.org:8080/stream\"}],"
        "[{\"text\":\"🔦 Toggle Flash\",\"callback_data\":\"toggle_flash\"}]""]";    
        bot.sendMessageWithInlineKeyboard(chat_id, welcome, "Markdown", keyboardJson);
      }
      else if (text == "/photo") {
        // Comando testuale /photo
        if(sendPhotoTelegram(chat_id)) {
          bot.sendMessage(chat_id, "📸 Ecco la foto richiesta:", "");
          // La foto verrà visualizzata direttamente grazie alla chiamata API
        } else {
          bot.sendMessage(chat_id, "Errore: impossibile scattare/inviare la foto.", "");
        }
      }
      else if (text == "/stream") {
        // Comando testuale /stream - risponde con link streaming
        String streamMsg = "🔴 Streaming avviato.\nApri questo link nel browser:\n";
        streamMsg += "http://NOMESERVIZIO.zapto.org:8080/stream";  // Usa il tuo hostname
        bot.sendMessage(chat_id, streamMsg, "");
      }
      else {
        // Altri comandi non riconosciuti
        bot.sendMessage(chat_id, "Comando non valido. Usa /start per vedere le opzioni.", "");
      }
    }
  }
}

void setup() {
  // Serial debug
  Serial.begin(115200);
  Serial.println();
  Serial.println("Avvio ESP32-CAM...");

  // Configura pin LED flash come output e spegne il flash (LED attivo basso)
  pinMode(LED_FLASH_PIN, OUTPUT);
  digitalWrite(LED_FLASH_PIN, LOW);

  // Configurazione della camera
  camera_config_t config;
  config.ledc_channel = LEDC_CHANNEL_0;
  config.ledc_timer   = LEDC_TIMER_0;
  config.pin_d0       = Y2_GPIO_NUM;
  config.pin_d1       = Y3_GPIO_NUM;
  config.pin_d2       = Y4_GPIO_NUM;
  config.pin_d3       = Y5_GPIO_NUM;
  config.pin_d4       = Y6_GPIO_NUM;
  config.pin_d5       = Y7_GPIO_NUM;
  config.pin_d6       = Y8_GPIO_NUM;
  config.pin_d7       = Y9_GPIO_NUM;
  config.pin_xclk     = XCLK_GPIO_NUM;
  config.pin_pclk     = PCLK_GPIO_NUM;
  config.pin_vsync    = VSYNC_GPIO_NUM;
  config.pin_href     = HREF_GPIO_NUM;
  config.pin_sccb_sda = SIOD_GPIO_NUM;
  config.pin_sccb_scl = SIOC_GPIO_NUM;
  config.pin_pwdn     = PWDN_GPIO_NUM;
  config.pin_reset    = RESET_GPIO_NUM;
  config.xclk_freq_hz = 20000000;
  config.pixel_format = PIXFORMAT_JPEG;
  // Se si vuole usare riconoscimento volti, usare PIXFORMAT_RGB565 e risoluzione CIF o QVGA

  if(psramFound()){
    // PSRAM disponibile: abilitare frame buffer in PSRAM e alta risoluzione
    config.frame_size = FRAMESIZE_UXGA;  // 1600x1200
    config.jpeg_quality = 10;           // qualità alta (basso valore)
    config.fb_count = 2;                // due buffer frame (miglior flusso con PSRAM)
    config.fb_location = CAMERA_FB_IN_PSRAM;
    Serial.println("PSRAM OK - Frame UXGA, qualità JPEG 10, doppio buffer");
  } else {
    // No PSRAM: usare risoluzione più bassa per sicurezza
    config.frame_size = FRAMESIZE_SVGA; // 800x600
    config.jpeg_quality = 12;          // qualità leggermente inferiore
    config.fb_count = 1;
    config.fb_location = CAMERA_FB_IN_DRAM;
    Serial.println("PSRAM non trovata - Frame SVGA, qualità JPEG 12, singolo buffer");
  }

  // Inizializza la camera
  esp_err_t camErr = esp_camera_init(&config);
  if(camErr != ESP_OK) {
    Serial.printf("Errore di inizializzazione camera: 0x%x\n", camErr);
    // Se fallisce, riavvia dopo breve attesa
    delay(5000);
    ESP.restart();
  }
  Serial.println("Camera inizializzata con successo");

  // Connessione Wi-Fi
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  Serial.printf("Connessione a WiFi %s", WIFI_SSID);
  // Disabilita power save per migliorare latenza streaming
  WiFi.setSleep(false);
  // Attendi connessione
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nWiFi connesso!");
  Serial.print("Indirizzo IP ottenuto: ");
  Serial.println(WiFi.localIP());

  // Imposta il client Telegram per accettare certificati (usiamo setInsecure altrove)
  telegramClient.setInsecure();
  // Avvia server streaming
  startCameraServer();
  Serial.println("Server streaming avviato. Visita /stream per vedere il video.");
  // Messaggio di startup (facoltativo)
  // Invia il messaggio di startup a tutti gli utenti autorizzati
for (String id : authorizedIds) {
    bot.sendMessage(id, "🤖 ESP32-CAM online. Invia /start per comandi.", "");
}
}

void loop() {
  // Gestisce i messaggi del bot Telegram
  if (millis() - lastBotCheck > BOT_CHECK_INTERVAL) {
    int numNewMessages = bot.getUpdates(bot.last_message_received + 1);
    while(numNewMessages) {
      handleNewMessages(numNewMessages);
      numNewMessages = bot.getUpdates(bot.last_message_received + 1);
    }
    lastBotCheck = millis();
  }

  // spegni flash dopo 5 minuti per sicurezza
  if (flashAttivo && (millis() - flashTimer >= FLASH_TIMEOUT)) {
  spegniFlash();
  flashAttivo = false;
  Serial.println("Flash spento automaticamente per sicurezza.");
  }

}

