#include <SPI.h>
#include <MFRC522.h>   
#include <DHT.h>       
#include <WiFi.h>      
#include <Wire.h>      
#include "HT_SSD1306Wire.h" 
#include <Firebase_ESP_Client.h> 
#include <addons/TokenHelper.h> 
#include <time.h>      

// Configuracoes do Firebase
#define FIREBASE_HOST "SEU_FIREBASE_HOST_AQUI"        // Substitua pelo Host do seu Firebase Realtime Database
#define FIREBASE_API_KEY "SUA_FIREBASE_API_KEY_AQUI"  // Substitua pela Chave de API do seu projeto Firebase
#define USER_EMAIL "SEU_EMAIL_FIREBASE_AQUI"          // Substitua pelo email do usuario Firebase para autenticacao
#define USER_PASSWORD "SUA_SENHA_FIREBASE_AQUI"       // Substitua pela senha do usuario Firebase para autenticacao

// Configurações WiFi
#define WIFI_SSID "SEU_WIFI_SSID_AQUI"    // Substitua pelo nome (SSID) da sua rede Wi-Fi
#define WIFI_PASSWORD "SUA_SENHA_WIFI_AQUI" // Substitua pela senha da sua rede Wi-Fi

// --- Definições dos Pinos de Hardware ---
// RFID
#define PIN_RFID_RST 33
#define PIN_RFID_SS 5
#define PIN_RFID_SCK 18
#define PIN_RFID_MOSI 12
#define PIN_RFID_MISO 21

// Display OLED (Heltec V2)
#define PIN_OLED_SDA 4
#define PIN_OLED_SCL 15
#define PIN_OLED_RST 16
#define PIN_VEXT 21 // Pino Vext em Heltec V2 (pode ser o mesmo do MISO)

// Sensor DHT
#define PIN_DHT 32

// Objetos Globais
SSD1306Wire oledDisplay(0x3c, 500000, PIN_OLED_SDA, PIN_OLED_SCL, GEOMETRY_128_64, PIN_OLED_RST);
MFRC522 mfrc522(PIN_RFID_SS, PIN_RFID_RST);
DHT dht(PIN_DHT, DHT11);

FirebaseData fbdo;
FirebaseData fbdoGet;
FirebaseAuth auth;
FirebaseConfig config;

// UIDs autorizados
const String AUTHORIZED_UIDS[] = {
  "0E052D03",
  "032AAF02",
  "04D4E5F6",
  "047A8B9C",
  "2D082D03",
  "8BA5AE02"
};
const uint8_t NUM_AUTHORIZED_UIDS = sizeof(AUTHORIZED_UIDS) / sizeof(AUTHORIZED_UIDS[0]);

// Estados e Variáveis do Sistema
unsigned long lastSensorRead = 0;
const long SENSOR_INTERVAL = 10000;

// Definição da sala atual do dispositivo (este ESP32 está na Sala 101)
const String CURRENT_ROOM_PATH = "/salas/sala101";
const String OTHER_ROOM_PATH = "/salas/sala102"; // Sala a ser verificada para presença

// Protótipos das Funções
void displayMessage(String line1, String line2);
void displayAccessDeniedOtherRoomSequence(String roomName);
void displayAccessInfo(String uid, String tipoAcesso, int numPessoas);
void displaySensorData(float temp, float hum);
String getUID();
bool isAuthorized(String uid);
bool isPresentInOtherRoom(String uid, String otherRoomPath);
void updateFirebaseSensors(float temp, float umid, String roomPath);
int getPeopleCountFromFirebase(String roomPath); // Função para buscar a contagem

// Funções de Inicialização
void initDisplay();
void initRFID();
void initDHT();
void initWiFi();
void initFirebase();

// Setup
void setup() {
  Serial.begin(115200);
  delay(100);

  // Inicializa todos os módulos e serviços
  initDisplay();
  initRFID();
  initDHT();
  initWiFi();
  initFirebase();
}

// Loop Principal
void loop() {
  handleRFID();
  handleSensors();
}

// --- Funções de Inicialização ---
void initDisplay() {
  // Ativa Vext, essencial para o display Heltec
  pinMode(PIN_VEXT, OUTPUT);
  digitalWrite(PIN_VEXT, LOW);
  delay(100);

  oledDisplay.init();
  oledDisplay.clear();
  oledDisplay.setFont(ArialMT_Plain_16);
  oledDisplay.setTextAlignment(TEXT_ALIGN_CENTER);
  oledDisplay.drawString(64, 24, "Iniciando...");
  oledDisplay.display();
  delay(1000);
}

void initRFID() {
  SPI.begin(PIN_RFID_SCK, PIN_RFID_MISO, PIN_RFID_MOSI, PIN_RFID_SS);
  mfrc522.PCD_Init();
  Serial.println("RFID inicializado");
  displayMessage("Aproxime", "o cartao");
}

void initDHT() {
  dht.begin();
  Serial.println("Sensor DHT inicializado");
}

void initWiFi() {
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  Serial.print("Conectando WiFi");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
    oledDisplay.clear();
    oledDisplay.drawString(64, 24, "Conectando WiFi...");
    oledDisplay.display();
  }
  Serial.println("\nConectado! IP: " + WiFi.localIP().toString());
  oledDisplay.clear();
  oledDisplay.drawString(64, 24, "WiFi Conectado!");
  oledDisplay.display();
  delay(1500);
}

void initFirebase() {
  config.host = FIREBASE_HOST;
  config.api_key = FIREBASE_API_KEY;

  auth.user.email = USER_EMAIL;
  auth.user.password = USER_PASSWORD;

  // tokenStatusCallback é fornecida pela biblioteca Firebase_ESP_Client.
  config.token_status_callback = tokenStatusCallback;

  Firebase.begin(&config, &auth);
  Firebase.reconnectWiFi(true);

  // Define buffer sizes para otimizar a comunicação com o Firebase
  fbdo.setBSSLBufferSize(4096, 1024);
  fbdo.setResponseSize(2048);
  fbdoGet.setBSSLBufferSize(4096, 1024);
  fbdoGet.setResponseSize(2048);

  // Configuração do Horário de São Paulo (NTP)
  const char* ntpServer = "pool.ntp.org";
  const long gmtOffset_sec = -10800; // GMT-3 para São Paulo
  const int daylightOffset_sec = 0;

  Serial.print("Configurando tempo com NTP...");
  configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);

  // Espera até que o tempo seja sincronizado
  struct tm timeinfo;
  if (!getLocalTime(&timeinfo)) {
    Serial.println(" Falha ao obter a hora via NTP.");
    displayMessage("Erro Tempo", "Verificar NTP");
  } else {
    Serial.println(" Tempo sincronizado com NTP.");
    Serial.printf("Hora atual: %s\n", asctime(&timeinfo));
  }

  
  int pessoasIniciais = getPeopleCountFromFirebase(CURRENT_ROOM_PATH);
  Serial.printf("Pessoas presentes na %s (recuperado do Firebase ao iniciar): %d\n", CURRENT_ROOM_PATH.c_str(), pessoasIniciais);

}

// --- Lógica Principal (loop) ---
void handleRFID() {
  // Verifica se há um novo cartão presente E se a leitura serial do cartão foi bem-sucedida
  if (mfrc522.PICC_IsNewCardPresent() && mfrc522.PICC_ReadCardSerial()) {
    String uid = getUID();
    Serial.printf("UID lido: %s\n", uid.c_str());

    if (isAuthorized(uid)) { // Verifica se o UID está na lista de autorizados
      int pessoasPresentesSala101 = getPeopleCountFromFirebase(CURRENT_ROOM_PATH);
      // 1. Verificar se a pessoa já está presente na sala atual (Sala 1) no Firebase
      bool presenteNaSalaAtual = false;
      // Tenta obter o nó do UID dentro da lista de "presentes" da sala atual
      if (Firebase.RTDB.get(&fbdoGet, CURRENT_ROOM_PATH + "/presentes/" + uid)) {
        if (fbdoGet.dataType() == "string") {
          presenteNaSalaAtual = true;
        }
      } else {
        // Se houver erro ou o nó não existir (e não for um erro de "not found"), loga o erro
        if (fbdoGet.errorReason().length() > 0 && fbdoGet.errorReason() != "NOT FOUND") {
          // Serial.printf("Erro ao verificar UID %s na sala atual: %s\n", uid.c_str(), fbdoGet.errorReason().c_str());
        }
      }

      if (presenteNaSalaAtual) {
        // --- Lógica de SAÍDA ---
        // Se a pessoa já está presente na Sala 101, ela está saindo
        if (Firebase.RTDB.deleteNode(&fbdo, CURRENT_ROOM_PATH + "/presentes/" + uid)) {
          pessoasPresentesSala101--; // Decrementa a contagem local
          if (pessoasPresentesSala101 < 0) pessoasPresentesSala101 = 0; // Garante que não fique negativo

          // Atualiza a contagem de pessoas e o status de ocupação no Firebase
          Firebase.RTDB.setInt(&fbdo, CURRENT_ROOM_PATH + "/numPessoas", pessoasPresentesSala101);
          Firebase.RTDB.setBool(&fbdo, CURRENT_ROOM_PATH + "/ocupada", (pessoasPresentesSala101 > 0)); // Ocupada se > 0 pessoas

          displayAccessInfo(uid, "SAIDA", pessoasPresentesSala101); // Exibe mensagem de saída
          Serial.printf("SAIDA autorizada para UID %s. Pessoas na sala 101: %d\n", uid.c_str(), pessoasPresentesSala101);
        } else {
          displayMessage("ERRO SAIDA", "Firebase!"); // Mensagem de erro no display
          Serial.printf("Erro ao remover UID %s do Firebase: %s\n", uid.c_str(), fbdo.errorReason().c_str());
        }
      } else {
        // --- Lógica de ENTRADA ---
        // Pessoa está tentando entrar na Sala 101
        // 2. Verificar se a pessoa está presente na Sala 102 (para evitar dupla contagem)
        bool presenteNaSala102 = isPresentInOtherRoom(uid, OTHER_ROOM_PATH);

        if (presenteNaSala102) {
          // Acesso negado se já está na Sala 102
          displayAccessDeniedOtherRoomSequence("Sala 102");
          Serial.printf("Acesso negado para UID %s. Já está na Sala 102.\n", uid.c_str());
        } else {
          // Pessoa pode entrar na Sala 101 (não está na sala atual, nem na outra sala)
          String presentesPath = CURRENT_ROOM_PATH + "/presentes/" + uid;

          // Obtém o timestamp atual do NTP em milissegundos para Firebase
          struct tm timeinfo;
          time_t now;
          time(&now); // Obtém o tempo atual em segundos desde 1970
          localtime_r(&now, &timeinfo); // Converte para a estrutura de tempo local

          // Salva o UID com um timestamp (Unix timestamp em milissegundos) no Firebase
          if (Firebase.RTDB.setString(&fbdo, presentesPath, String(now * 1000))) {
            pessoasPresentesSala101++; // Incrementa a contagem local
            // Atualiza a contagem de pessoas e o status de ocupação no Firebase
            Firebase.RTDB.setInt(&fbdo, CURRENT_ROOM_PATH + "/numPessoas", pessoasPresentesSala101);
            Firebase.RTDB.setBool(&fbdo, CURRENT_ROOM_PATH + "/ocupada", true); // Sala ocupada

            displayAccessInfo(uid, "ENTRADA", pessoasPresentesSala101); // Exibe mensagem de entrada
            Serial.printf("ENTRADA autorizada para UID %s. Pessoas na sala 101: %d\n", uid.c_str(), pessoasPresentesSala101);
          } else {
            displayMessage("ERRO ENTRADA", "Firebase!"); // Mensagem de erro no display
            Serial.printf("Erro ao adicionar UID %s ao Firebase: %s\n", uid.c_str(), fbdo.errorReason().c_str());
          }
        }
      }
    } else {
      displayMessage("ACESSO", "Cartao invalido!"); // Cartão não autorizado
      Serial.printf("UID %s invalido ou nao autorizado.\n", uid.c_str());
    }

    // Para o processo de leitura do cartão e permite nova leitura
    mfrc522.PICC_HaltA();
    mfrc522.PCD_StopCrypto1();
    delay(500); // Pequena pausa para evitar leituras múltiplas e dar tempo para o Firebase
  }
}

void handleSensors() {
  unsigned long now = millis();
  // Verifica se o intervalo para leitura do sensor já passou
  if (now - lastSensorRead >= SENSOR_INTERVAL) {
    lastSensorRead = now; // Atualiza o tempo da última leitura

    float temp = dht.readTemperature(); // Lê a temperatura
    float umid = dht.readHumidity();    // Lê a umidade

    if (!isnan(temp) && !isnan(umid)) { // Verifica se as leituras são válidas
      displaySensorData(temp, umid);
      updateFirebaseSensors(temp, umid, CURRENT_ROOM_PATH); // Envia para o Firebase
    } else {
      Serial.println("Falha ao ler do sensor DHT!"); // Mensagem de erro se a leitura falhar
    }
  }
}

// --- Funções Auxiliares Firebase ---

// Função para buscar o número de pessoas diretamente do Firebase
int getPeopleCountFromFirebase(String roomPath) {
  if (Firebase.RTDB.getInt(&fbdoGet, roomPath + "/numPessoas")) {
    return fbdoGet.intData();
  } else {
    // Se falhar a leitura (e.g., nó não existe), retorna 0 e imprime o erro.
    // Isso é importante para a primeira vez que o nó numPessoas é acessado.
    if (fbdoGet.errorReason().length() > 0 && fbdoGet.errorReason() != "NOT FOUND") {
        Serial.printf("Falha ao ler numPessoas em %s (assumindo 0): %s\n", roomPath.c_str(), fbdoGet.errorReason().c_str());
    } else {
        // Serial.printf("numPessoas nao encontrado em %s, assumindo 0.\n", roomPath.c_str());
    }
    return 0;
  }
}

// Verifica se um UID está presente em outra sala no Firebase
bool isPresentInOtherRoom(String uid, String otherRoomPath) {
  String path = otherRoomPath + "/presentes/" + uid;
  // Serial.printf("Verificando se UID %s esta em %s\n", uid.c_str(), path.c_str());
  if (Firebase.RTDB.get(&fbdoGet, path)) {
    if (fbdoGet.dataType() == "string") { // Se o nó existe e é uma string (timestamp)
      Serial.printf("UID %s ENCONTRADO em %s.\n", uid.c_str(), otherRoomPath.c_str());
      return true;
    } else {
      Serial.printf("UID %s NAO ENCONTRADO em %s (tipo de dado: %s).\n", uid.c_str(), otherRoomPath.c_str(), fbdoGet.dataType().c_str());
    }
  } else {
    // Se houver erro ou o nó não existir, assumimos que não está presente na outra sala.
    if (fbdoGet.errorReason().length() > 0 && fbdoGet.errorReason() != "NOT FOUND") {
      // Serial.printf("Erro ou no nao existe ao verificar UID %s em %s: %s\n", uid.c_str(), otherRoomPath.c_str(), fbdoGet.errorReason().c_str());
    }
  }
  return false;
}

// Envia dados de sensor para o Firebase com timestamp do NTP
void updateFirebaseSensors(float temp, float umid, String roomPath) {
  // Obtém a hora atual do NTP
  struct tm timeinfo;
  if (!getLocalTime(&timeinfo)) {
    Serial.println(" Falha ao obter a hora local para sensor.");
    return; // Não envia dados se o tempo não estiver sincronizado
  }

  char buffer[9]; // Formato HH:MM:SS\0
  strftime(buffer, sizeof(buffer), "%H:%M:%S", &timeinfo);
  String timestamp = String(buffer);

  // Caminhos para temperatura e umidade com base no timestamp
  String tempPath = roomPath + "/temperatura/" + timestamp;
  String umidPath = roomPath + "/umidade/" + timestamp;

  // Envia temperatura para o Firebase
  if (!Firebase.RTDB.setFloat(&fbdo, tempPath, temp)) {
    Serial.printf("Falha ao enviar temp para Firebase: %s\n", fbdo.errorReason().c_str());
  }

  // Envia umidade para o Firebase
  if (!Firebase.RTDB.setFloat(&fbdo, umidPath, umid)) {
    Serial.printf("Falha ao enviar umid para Firebase: %s\n", fbdo.errorReason().c_str());
  }
}

// --- Funções Auxiliares Gerais ---

// Retorna o UID do cartão lido SEM ESPAÇOS
String getUID() {
  String uid;
  for (byte i = 0; i < mfrc522.uid.size; i++) {
    if (mfrc522.uid.uidByte[i] < 0x10) uid += "0"; // Adiciona um '0' à esquerda se o byte for menor que 0x10 (ex: 0F)
    uid += String(mfrc522.uid.uidByte[i], HEX); // Converte o byte para hexadecimal e adiciona
  }
  uid.toUpperCase(); // Converte para maiúsculas
  return uid;
}

// Verifica se o UID está na lista de UIDs autorizados
bool isAuthorized(String uid) {
  for (uint8_t i = 0; i < NUM_AUTHORIZED_UIDS; i++) {
    if (uid.equals(AUTHORIZED_UIDS[i])) { // Compara o UID lido com os UIDs autorizados
      return true;
    }
  }
  return false;
}

// Exibe uma mensagem de duas linhas no display OLED
void displayMessage(String line1, String line2) {
  oledDisplay.clear();
  oledDisplay.setFont(ArialMT_Plain_24);
  oledDisplay.setTextAlignment(TEXT_ALIGN_CENTER);
  oledDisplay.drawString(64, 10, line1); // Primeira linha, centralizada no Y
  oledDisplay.setFont(ArialMT_Plain_16);
  oledDisplay.drawString(64, 40, line2); // Segunda linha, abaixo da primeira
  oledDisplay.display();
}

// Função para exibir a sequência de mensagens de acesso negado quando já está em outra sala
void displayAccessDeniedOtherRoomSequence(String roomName) {
  // Primeira parte: "ACESSO NEGADO"
  oledDisplay.clear();
  oledDisplay.setTextAlignment(TEXT_ALIGN_CENTER);
  oledDisplay.setFont(ArialMT_Plain_16);
  oledDisplay.drawString(64, 10, "ACESSO");
  oledDisplay.drawString(64, 35, "NEGADO");
  oledDisplay.display();
  delay(1500); // Exibe por 1.5 segundos

  // Segunda parte: "Presente Sala X"
  oledDisplay.clear();
  oledDisplay.setFont(ArialMT_Plain_16);
  oledDisplay.drawString(64, 10, "Presente");
  oledDisplay.drawString(64, 35, roomName);
  oledDisplay.display();
  delay(2000); // Exibe por 2 segundos

  displayMessage("Aproxime", "o cartao");
}

// Exibe informações de acesso (entrada/saída) e contagem de pessoas no display OLED
void displayAccessInfo(String uid, String tipoAcesso, int numPessoas) {
  oledDisplay.clear();
  oledDisplay.setTextAlignment(TEXT_ALIGN_CENTER);

  oledDisplay.setFont(ArialMT_Plain_10); // Fonte menor para o UID (sempre visível no topo)
  oledDisplay.drawString(64, 0, "UID: " + uid);

  if (tipoAcesso == "ENTRADA") {
    // --- Primeira tela: "ENTRADA AUTORIZADA" ---
    oledDisplay.setFont(ArialMT_Plain_16);
    oledDisplay.drawString(64, 15, "ENTRADA");
    oledDisplay.drawString(64, 35, "AUTORIZADA");
    oledDisplay.display();
    delay(1500); // Exibe por 1.5 segundos

    // --- Segunda tela: "PRESENTES: X" (limpa a tela antes) ---
    oledDisplay.clear();
    oledDisplay.setFont(ArialMT_Plain_16);
    oledDisplay.drawString(64, 20, "PRESENTES:");
    oledDisplay.setFont(ArialMT_Plain_24);
    oledDisplay.drawString(64, 40, String(numPessoas));
    oledDisplay.display();
    delay(2000); // Exibe por 2 segundos

  } else if (tipoAcesso == "SAIDA") {
    // --- Primeira tela: "SAIDA AUTORIZADA" ---
    oledDisplay.setFont(ArialMT_Plain_16);
    oledDisplay.drawString(64, 15, "SAIDA");
    oledDisplay.drawString(64, 35, "AUTORIZADA");
    oledDisplay.display();
    delay(1500);

    // --- Segunda tela: "TOTAL: X" (limpa a tela antes) ---
    oledDisplay.clear();
    oledDisplay.setFont(ArialMT_Plain_16);
    oledDisplay.drawString(64, 20, "TOTAL:");
    oledDisplay.setFont(ArialMT_Plain_24);
    oledDisplay.drawString(64, 40, String(numPessoas));
    oledDisplay.display();
    delay(2000);
  }

  // Sempre retorna para a tela inicial após a sequência de acesso
  displayMessage("Aproxime", "o cartao");
}

// Exibe dados de sensor (temperatura e umidade) no display OLED
void displaySensorData(float temp, float hum) {
  oledDisplay.clear();
  oledDisplay.setFont(ArialMT_Plain_16);
  oledDisplay.setTextAlignment(TEXT_ALIGN_CENTER);
  oledDisplay.drawString(64, 16, "Temp: " + String(temp, 1) + " C");
  oledDisplay.drawString(64, 40, "Umid: " + String(hum, 1) + " %");
  oledDisplay.display();
}