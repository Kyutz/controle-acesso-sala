# Controle de Acesso e Monitoramento de Sala - Campus Inteligente

## Descrição do Projeto
Sistema IoT para controle de acesso e monitoramento ambiental de salas em um Campus Inteligente. Utiliza ESP32 (RFID, DHT11, OLED) para gerenciar entrada/saída, coletar dados e exibir status. Dados em tempo real e rastreamento de ocupação multi-salas via Firebase Realtime Database.

## Funcionalidades

* **Controle de Acesso RFID:**
    * Leitura de cartões RFID (UID) e verificação de usuários autorizados.
    * Lógica de entrada e saída automática, com contagem de pessoas presentes na sala.
* **Monitoramento Ambiental:**
    * Leitura de temperatura e umidade via sensor DHT11.
    * Exibição dos dados no display OLED.
* **Display OLED Integrado:**
    * Mensagens de status no display (ex.: "Aproxime o cartão", "ACESSO APROVADO!", "ACESSO NEGADO!").
    * Exibição de dados dos sensores e informações de acesso.
* **Conectividade e Nuvem (Firebase):**
    * Conexão Wi-Fi do ESP32 para acesso à internet.
    * Envio de dados de acesso e ambientais para o Firebase Realtime Database em tempo real.
* **Logica de Ocupacao Multi-Sala:**
    * Verificação da presença de usuários em outras salas via Firebase, prevenindo acesso duplo.

## Hardware Utilizado

* Microcontrolador: ESP32 Heltec V2 (com display OLED integrado)
* Módulo RFID: RC522
* Sensor: DHT11 (versão de 4 pinos)
* Fios Jumper
* Protoboard

## Pinagem (Conexões Fisicas)

Certifique-se de que todas as conexões estão firmes e corretas.

* **RFID-RC522 para ESP32:**
    * RC522 SCK -> ESP32 GPIO 18
    * RC522 MISO -> ESP32 GPIO 21
    * RC522 MOSI -> ESP32 GPIO 12
    * RC522 SS (SDA) -> ESP32 GPIO 5
    * RC522 RST -> ESP32 GPIO 33
    * RC522 VCC -> ESP32 3.3V
    * RC522 GND -> ESP32 GND

* **Sensor DHT11 para ESP32:**
    * DHT11 VCC -> ESP32 3.3V
    * DHT11 DATA -> ESP32 GPIO 32
    * DHT11 GND -> ESP32 GND

* **Display OLED (Integrado ao Heltec V2):**
    * Pinagem interna a placa (SDA: GPIO 4, SCL: GPIO 15, RST: GPIO 16, Vext: GPIO 21).

## Software e Bibliotecas

* **Ambiente de Desenvolvimento:** Arduino IDE
* **Placa:** ESP32 Board Package
* **Bibliotecas (via Gerenciador de Bibliotecas da Arduino IDE):**
    * `MFRC522` (por Miguel Balboa)
    * `DHT sensor library` (por Adafruit)
    * `HT_SSD1306Wire` (especifica da Heltec)
    * `Firebase_ESP_Client` (por Mobizt)

## Configuracao do Ambiente

1.  **Instalação das Bibliotecas:**
    * No Arduino IDE, vá em `Sketch > Incluir Biblioteca > Gerenciar Bibliotecas...`.
    * Pesquise e instale as bibliotecas listadas acima.
2.  **Configuracao do Projeto Firebase:**
    * No [Firebase Console](https://console.firebase.google.com/), crie seu projeto.
    * Adicione um **Aplicativo Web** para obter `FIREBASE_API_KEY` e `FIREBASE_HOST`.
    * No **Realtime Database**, defina regras de segurança para `".read": "true", ".write": "true"` (para testes).
    * Em **Authentication**, ative o método `Email/Senha` e crie um usuário (`USER_EMAIL`, `USER_PASSWORD`).
    * Garanta que a estrutura base `/salas/sala101/presentes` e `/salas/sala102/presentes` exista no seu Realtime Database.
3.  **Atualize as Credenciais no Codigo:**
    * No seu arquivo `.ino`, atualize os defines:
        * `WIFI_SSID` e `WIFI_PASSWORD` (sua rede Wi-Fi).
        * `FIREBASE_API_KEY`, `FIREBASE_HOST`, `USER_EMAIL`, `USER_PASSWORD` (suas credenciais do Firebase).
        * `CURRENT_ROOM_PATH` para `/salas/sala102` e `OTHER_ROOM_PATH` para `/salas/sala101` (para este dispositivo).
        * `AUTHORIZED_UIDS` com os UIDs reais dos seus cartoes.

## Como Usar

1.  **Faca o Upload do Codigo:** Carregue o firmware para o seu ESP32 utilizando a Arduino IDE.
2.  **Observe a Inicializacao:** Acompanhe o processo no Monitor Serial e no display OLED.
3.  **Interaja com o RFID:** Aproxime um cartao RFID do leitor para testar o controle de acesso. O display e o Monitor Serial exibirão o status.
4.  **Monitore os Dados Online:** Acesse seu **Firebase Console > Realtime Database** para ver os dados de acessos, ocupacao, temperatura e umidade sendo atualizados em tempo real.

## Colaboradores

* Thallys Henrique Martins 
* Willian Brandao de Souza 
* Samuel Luiz Freitas Ferreira 
