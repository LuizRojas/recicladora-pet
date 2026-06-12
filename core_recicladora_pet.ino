#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <TimerOne.h>

// --- CONFIGURAÇÕES DO DISPLAY ---
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);

// --- PINOS SHIELD CNC (Eixo X) ---
#define STEP_PIN 2
#define DIR_PIN 5
#define EN_PIN 8

// --- PINO DO BOTÃO ---
#define BUTTON_PIN A3 

// --- VARIÁVEIS DE CONTROLE DO MOTOR ---
volatile long stepDelay = 0; 
volatile bool motorAtivo = false;

// --- VARIÁVEIS DO BOTÃO, VELOCIDADE E SENTIDO ---
int estagioAtual = 0; // 0 = Desligado, 1 = 20%, 2 = 40%, etc.
unsigned long tempoBotaoPressionado = 0;
bool botaoPressionadoAnteriormente = false;
bool sentidoAvanco = true; // true = Avanço (Padrão), false = Reverso (Invertido)

void setup() {
  pinMode(BUTTON_PIN, INPUT_PULLUP);

  // Configura Pinos do Motor
  pinMode(STEP_PIN, OUTPUT);
  pinMode(DIR_PIN, OUTPUT);
  pinMode(EN_PIN, OUTPUT);
  digitalWrite(EN_PIN, LOW);   // Drivers ativados com força total de retenção
  
  // Define sentido inicial
  digitalWrite(DIR_PIN, HIGH); 

  // Inicializa Display e corrige orientação física
  if(!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) { for(;;); }
  display.setRotation(2); 

  // Inicializa o "Processo Paralelo" do Motor
  Timer1.initialize(100); 
  Timer1.attachInterrupt(controladorMotor);
  
  definirVelocidade(0);
}

// --- PROCESSO EM PARALELO (INTERRUPÇÃO DO MOTOR) ---
void controladorMotor() {
  if (!motorAtivo) return;

  static unsigned long lastStepTime = 0;
  unsigned long currentTime = micros();

  if (currentTime - lastStepTime >= stepDelay) {
    digitalWrite(STEP_PIN, HIGH);
    delayMicroseconds(2); 
    digitalWrite(STEP_PIN, LOW);
    lastStepTime = currentTime;
  }
}

void loop() {
  bool botaoApertadoAgora = (digitalRead(BUTTON_PIN) == LOW);

  // --- DETECÇÃO DE BORDA (QUANDO O BOTÃO É PRESSIONADO) ---
  if (botaoApertadoAgora && !botaoPressionadoAnteriormente) {
    tempoBotaoPressionado = millis(); 
    botaoPressionadoAnteriormente = true;
  }

  // --- SE O BOTÃO FOR SOLTO ---
  if (!botaoApertadoAgora && botaoPressionadoAnteriormente) {
    unsigned long duracaoClique = millis() - tempoBotaoPressionado;

    // Se foi um clique rápido (menos de 1.5 segundos) e NÃO estava em processo de segurar prolongado
    if (duracaoClique < 1500) {
      estagioAtual++;
      if (estagioAtual > 5) estagioAtual = 1; 
      definirVelocidade(estagioAtual);
    }
    
    botaoPressionadoAnteriormente = false;
  }

  // --- SE CONTINUAR SEGURANDO O BOTÃO (AÇÃO PROLONGADA) ---
  if (botaoApertadoAgora && botaoPressionadoAnteriormente) {
    unsigned long tempoSegurando = millis() - tempoBotaoPressionado;

    // CASO 1: O motor está RODANDO -> Segurar desliga o motor
    if (estagioAtual > 0 && tempoSegurando >= 1500) {
      estagioAtual = 0; 
      definirVelocidade(estagioAtual);
      // Espera soltar o botão para evitar cliques fantasmas
      while(digitalRead(BUTTON_PIN) == LOW) { delay(10); }
      botaoPressionadoAnteriormente = false;
    }
    
    // CASO 2: O motor já está EM TRAVA (0%) -> Segurar por 2 segundos INVERTE a rotação
    else if (estagioAtual == 0 && tempoSegurando >= 2000) {
      sentidoAvanco = !sentidoAvanco; // Inverte a variável de sentido
      
      // Aplica a inversão diretamente no pino Físico de Direção da Shield
      if (sentidoAvanco) {
        digitalWrite(DIR_PIN, HIGH); // Avanço padrão
      } else {
        digitalWrite(DIR_PIN, LOW);  // Reverso
      }
      
      // Pisca a tela rapidamente para dar um feedback visual de que o sentido mudou
      display.clearDisplay();
      display.display();
      delay(150);
      
      // Espera soltar o botão
      while(digitalRead(BUTTON_PIN) == LOW) { delay(10); }
      botaoPressionadoAnteriormente = false;
    }
  }

  // --- ATUALIZAÇÃO DO DISPLAY ---
  static unsigned long tDisplay = 0;
  if (millis() - tDisplay > 200) { 
    tDisplay = millis();
    desenharOLED();
  }
}

// --- TRADUZ OS ESTÁGIOS EM VELOCIDADE REAL (DELAYS) ---
void definirVelocidade(int estagio) {
  switch (estagio) {
    case 0: 
      motorAtivo = false;
      break;
    case 1: 
      motorAtivo = true;
      stepDelay = 2000; 
      break;
    case 2: 
      motorAtivo = true;
      stepDelay = 1000;
      break;
    case 3: 
      motorAtivo = true;
      stepDelay = 800;
      break;
    case 4: 
      motorAtivo = true;
      stepDelay = 650;
      break;
    case 5: 
      motorAtivo = true;
      stepDelay = 450; 
      break;
  }
}

void desenharOLED() {
  display.clearDisplay();
  display.setTextColor(WHITE);
  
  // Cabeçalho (Faixa Amarela)
  display.setTextSize(1);
  display.setCursor(20, 0);
  display.print("RECICLADORA PET");
  display.drawFastHLine(0, 12, 128, WHITE);

  // Corpo (Faixa Azul)
  display.setCursor(0, 22);
  display.print("Status: ");
  if (estagioAtual == 0) {
    display.print("MOTOR PARADO");
  } else {
    display.print("TRACIONANDO");
  }

  display.setCursor(0, 34);
  display.print("Sentido: ");
  if (sentidoAvanco) {
    display.print("AVANCO (->)");
  } else {
    display.print("REVERSO (<-)");
  }

  // Velocidade Grande
  display.setTextSize(2);
  display.setCursor(0, 48);
  
  switch(estagioAtual) {
    case 0: display.print("0% (TRAVA)"); break;
    case 1: display.print("20% VEL"); break;
    case 2: display.print("40% VEL"); break;
    case 3: display.print("50% VEL"); break;
    case 4: display.print("60% VEL"); break;
    case 5: display.print("80% VEL"); break;
  }

  display.display(); 
}