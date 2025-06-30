#include <WiFi.h>
#include <HTTPClient.h>
#include <Wire.h>
#include <Adafruit_SSD1306.h>
#include <Keypad.h>

// === CONFIGURAÇÕES DO PROJETO ===

// Wi-Fi
const char* ssid = "Formacao";
const char* password = "FDocentesCEIMM";

// URL do seu Google Apps Script (publicado com doGet e doPost)
String scriptURL = "https://script.google.com/macros/s/AKfycbycAZERUq5iAtD5W8e5ojxniSVQm1jGeNNXSXLYBSrF454s03CA9Fw4t2_fM7znzi4lgw/exec";

// OLED 128x64 I2C
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET    -1
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

// Teclado 4x4
const byte ROWS = 4;
const byte COLS = 4;
char keys[ROWS][COLS] = {
  {'1','2','3','A'},
  {'4','5','6','B'},
  {'7','8','9','C'},
  {'*','0','#','D'}
};
byte rowPins[ROWS] = {32, 33, 25, 26}; // ajustar conforme conexão
byte colPins[COLS] = {27, 14, 12, 13}; // ajustar conforme conexão
Keypad keypad = Keypad(makeKeymap(keys), rowPins, colPins, ROWS, COLS);

// Sensores ultrassônicos HC-SR04
// Sensor 1 - Lixeira 1
const int trigPin1 = 5;
const int echoPin1 = 18;
// Sensor 2 - Lixeira 2
const int trigPin2 = 4;
const int echoPin2 = 19;

// Buzzer
const int buzzerPin = 23;

// Variáveis para código do aluno
String codigoAluno = "";
bool esperandoDeposito = false;
int lixeiraEsperada = 0; // 1 ou 2

// Tempo de leitura ultrassônico para confirmar depósito (em ms)
const unsigned long tempoEsperaDeposito = 10000;  // 10 segundos

// Variáveis para controle do fluxo
int etapa = 0;  // 0 = digitar código, 1 = escolher lixeira, 2 = esperando depósito
unsigned long startMillis = 0;

void setup() {
  Serial.begin(9600);
  delay(100);

  // Inicializa I2C explicitamente com SDA = 21, SCL = 22
  Wire.begin(21, 22);

  // Inicializa Wi-Fi
  WiFi.begin(ssid, password);
  Serial.print("Conectando ao Wi-Fi");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nWi-Fi conectado!");

  // Inicializa OLED com endereço 0x3C
  if(!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
    Serial.println("Falha na inicializacao do OLED");
    while(1);
  }
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);

  // Inicializa sensores ultrassônicos
  pinMode(trigPin1, OUTPUT);
  pinMode(echoPin1, INPUT);
  pinMode(trigPin2, OUTPUT);
  pinMode(echoPin2, INPUT);

  // Inicializa buzzer
  pinMode(buzzerPin, OUTPUT);
  digitalWrite(buzzerPin, LOW);

  mostrarMensagem("Digite o codigo:");
}

void loop() {
  char key = keypad.getKey();
  
  if (etapa == 0) {
    if (key) {
      if (key == '#') {
        if (codigoAluno.length() > 0) {
          etapa = 1;  // passar para escolher lixeira
          mostrarMensagem("Cod: " + codigoAluno);
          mostrarMensagem2("Escolha lixeira 1 ou 2");
          beep(1);
        } else {
          beep(3);
          mostrarMensagem("Codigo vazio!");
          delay(1000);
          mostrarMensagem("Digite o codigo:");
        }
      } else if (key == '*') {
        if (codigoAluno.length() > 0) {
          codigoAluno.remove(codigoAluno.length() - 1);
          mostrarMensagem("Digite o codigo:");
          mostrarMensagem2(codigoAluno);
        } else {
          beep(2);
        }
      } else {
        if (codigoAluno.length() < 10) {
          codigoAluno += key;
          mostrarMensagem("Digite o codigo:");
          mostrarMensagem2(codigoAluno);
          beep(1);
        } else {
          beep(2);
        }
      }
    }
  } 
  else if (etapa == 1) {  // escolha da lixeira
    if (key == '1' || key == '2') {
      lixeiraEsperada = key - '0';  // converte char para int
      esperandoDeposito = true;
      etapa = 2;
      mostrarMensagem("Deposite na lixeira " + String(lixeiraEsperada));
      mostrarMensagem2("");
      beep(1);
      Serial.println("Esperando deposito na lixeira " + String(lixeiraEsperada));
      startMillis = millis();  // reset timer para depósito
    }
  }
  else if (etapa == 2) {  // esperando depósito
    if (millis() - startMillis > tempoEsperaDeposito) {
      mostrarMensagem("Deposito nao detectado");
      beep(4);
      delay(2000);
      limparEstado();
      etapa = 0;
      startMillis = millis();  // reset timer
    }
    if (verificarDeposito(lixeiraEsperada)) {
      beep(3);
      mostrarMensagem("Deposito OK!");
      enviarPontuacao(codigoAluno, lixeiraEsperada);
      delay(2000);
      limparEstado();
      etapa = 0;
      startMillis = millis();  // reset timer
    }
  }
}

// Mostra mensagem na primeira linha do OLED
void mostrarMensagem(String msg) {
  display.clearDisplay();
  display.setCursor(0,0);
  display.print(msg);
  display.display();
}

// Mostra mensagem na segunda linha do OLED
void mostrarMensagem2(String msg) {
  display.setCursor(0,20);
  display.print(msg);
  display.display();
}

// Faz um beep curto com o buzzer
void beep(int quantidade) {
  for (int i=0; i < quantidade; i++) {
    digitalWrite(buzzerPin, HIGH);
    delay(150);
    digitalWrite(buzzerPin, LOW);
    delay(150);
  }
}

// Função para medir distância do sensor ultrassônico
long medirDistancia(int trigPin, int echoPin) {
  digitalWrite(trigPin, LOW);
  delayMicroseconds(2);
  digitalWrite(trigPin, HIGH);
  delayMicroseconds(10);
  digitalWrite(trigPin, LOW);
  long duration = pulseIn(echoPin, HIGH, 30000); // timeout 30ms
  long distance = duration * 0.034 / 2; // cm
  return distance;
}

// Verifica se o depósito foi realizado na lixeira esperada
bool verificarDeposito(int lixeira) {
  long dist1 = medirDistancia(trigPin1, echoPin1);
  long dist2 = medirDistancia(trigPin2, echoPin2);

  Serial.print("Distancia 1: "); Serial.print(dist1); Serial.print(" cm, ");
  Serial.print("Distancia 2: "); Serial.print(dist2); Serial.println(" cm");

  // Ajuste o valor do limite conforme a distância real na montagem
  const int limiteDeposito = 15; // cm

  if (lixeira == 1 && dist1 < limiteDeposito) {
    return true;
  }
  if (lixeira == 2 && dist2 < limiteDeposito) {
    return true;
  }
  return false;
}

// Envia pontuação para a planilha Google via Apps Script
void enviarPontuacao(String codigo, int lixeira) {
  if (WiFi.status() == WL_CONNECTED) {
    HTTPClient http;

    String codigoEncoded = urlEncode(codigo);
    String lixeiraStr = "Lixeira " + String(lixeira);
    String lixeiraEncoded = urlEncode(lixeiraStr);

    String url = scriptURL + "?codigo=" + codigoEncoded + "&lixeira=" + lixeiraEncoded + "&pontos=1";

    Serial.println("Enviando para: " + url);

    http.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);
    http.begin(url);
    int httpCode = http.GET();

    if (httpCode > 0) {
      Serial.println("HTTP code: " + String(httpCode));
      String payload = http.getString();
      Serial.println("Resposta: " + payload);
    } else {
      Serial.println("Erro HTTP");
    }

    http.end();
  } else {
    Serial.println("Wi-Fi desconectado");
  }
}

// Função para codificar string para URL
String urlEncode(const String &str) {
  String encodedString = "";
  char c;
  char code0;
  char code1;
  for (int i = 0; i < str.length(); i++) {
    c = str.charAt(i);
    if (isalnum(c)) {
      encodedString += c;
    } else {
      encodedString += '%';
      code0 = (c >> 4) & 0xF;
      code1 = c & 0xF;
      encodedString += (code0 > 9 ? (code0 - 10 + 'A') : (code0 + '0'));
      encodedString += (code1 > 9 ? (code1 - 10 + 'A') : (code1 + '0'));
    }
  }
  return encodedString;
}

// Limpa variáveis e volta para o estado inicial
void limparEstado() {
  codigoAluno = "";
  esperandoDeposito = false;
  lixeiraEsperada = 0;
  mostrarMensagem("Digite o codigo:");
  mostrarMensagem2("");
}
