#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <EEPROM.h>
#include <OneButton.h>

LiquidCrystal_I2C lcd(0x27, 16, 2);

const int PINO_BOTAO = 2;
const int MAX_TAREFAS = 10;
const int TAM_TAREFA = 16;
const int ENDERECO_QUANTIDADE = 0;
const int ENDERECO_TAREFAS = 1;

OneButton botao(PINO_BOTAO, true, true);
// true, true = ativo em LOW, com pull-up interno

int totalTarefas = 0;
int tarefaAtual = 0;

void setup() {
  Serial.begin(9600);
  lcd.init();
  lcd.backlight();

  totalTarefas = EEPROM.read(ENDERECO_QUANTIDADE);
  if (totalTarefas > MAX_TAREFAS) totalTarefas = 0;

  // Configura os callbacks do botão
  botao.attachClick(proximaTarefa);
  botao.attachDoubleClick(concluirTarefa);
  botao.attachLongPressStart(limparTudo);
  botao.setPressMs(1000);  // segurar 1s = clique longo

  exibirTarefa();
  imprimirAjuda();
}

void loop() {
  botao.tick();  // precisa ser chamado sempre

  if (Serial.available()) {
    String comando = Serial.readStringUntil('\n');
    comando.trim();
    processarComando(comando);
  }
}

// === Ações do botão ===

void proximaTarefa() {
  if (totalTarefas > 0) {
    tarefaAtual = (tarefaAtual + 1) % totalTarefas;
    exibirTarefa();
  }
}

void concluirTarefa() {
  if (totalTarefas == 0) return;

  // Mostra feedback rápido
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Concluida!");
  delay(700);

  // Remove a tarefa atual (mesma lógica do DEL)
  for (int i = tarefaAtual; i < totalTarefas - 1; i++) {
    char buffer[TAM_TAREFA + 1];
    lerTarefa(i + 1, buffer);
    escreverTarefa(i, buffer);
  }
  totalTarefas--;
  EEPROM.update(ENDERECO_QUANTIDADE, totalTarefas);
  if (tarefaAtual >= totalTarefas) tarefaAtual = 0;

  Serial.println("Tarefa concluida!");
  exibirTarefa();
}

void limparTudo() {
  totalTarefas = 0;
  tarefaAtual = 0;
  EEPROM.update(ENDERECO_QUANTIDADE, 0);

  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Lista limpa!");
  delay(900);

  Serial.println("Todas as tarefas apagadas");
  exibirTarefa();
}

// === Display ===

void exibirTarefa() {
  lcd.clear();
  if (totalTarefas == 0) {
    lcd.setCursor(0, 0);
    lcd.print("Nenhuma tarefa");
    return;
  }
  lcd.setCursor(0, 0);
  lcd.print("Tarefa ");
  lcd.print(tarefaAtual + 1);
  lcd.print("/");
  lcd.print(totalTarefas);

  lcd.setCursor(0, 1);
  char buffer[TAM_TAREFA + 1];
  lerTarefa(tarefaAtual, buffer);
  lcd.print(buffer);
}

// === EEPROM ===

void lerTarefa(int indice, char* buffer) {
  int endereco = ENDERECO_TAREFAS + (indice * TAM_TAREFA);
  for (int i = 0; i < TAM_TAREFA; i++) {
    buffer[i] = EEPROM.read(endereco + i);
  }
  buffer[TAM_TAREFA] = '\0';
}

void escreverTarefa(int indice, const char* texto) {
  int endereco = ENDERECO_TAREFAS + (indice * TAM_TAREFA);
  for (int i = 0; i < TAM_TAREFA; i++) {
    if (i < strlen(texto)) {
      EEPROM.update(endereco + i, texto[i]);
    } else {
      EEPROM.update(endereco + i, ' ');
    }
  }
}

// === Comandos via Serial ===

void processarComando(String cmd) {
  if (cmd.equalsIgnoreCase("LIST")) {
    if (totalTarefas == 0) {
      Serial.println("Nenhuma tarefa cadastrada");
    } else {
      Serial.println("=== Tarefas ===");
      for (int i = 0; i < totalTarefas; i++) {
        char buffer[TAM_TAREFA + 1];
        lerTarefa(i, buffer);
        Serial.print(i + 1);
        Serial.print(". ");
        Serial.println(buffer);
      }
    }
  }
  else if (cmd.startsWith("ADD:") || cmd.startsWith("ADD ")) {
    if (totalTarefas >= MAX_TAREFAS) {
      Serial.println("Erro: limite de 10 tarefas atingido");
      return;
    }
    String texto = cmd.substring(4);
    texto.trim();
    if (texto.length() == 0) {
      Serial.println("Erro: tarefa vazia");
      return;
    }
    if (texto.length() > TAM_TAREFA) {
      texto = texto.substring(0, TAM_TAREFA);
      Serial.println("Aviso: tarefa truncada para 16 caracteres");
    }
    char buffer[TAM_TAREFA + 1];
    texto.toCharArray(buffer, TAM_TAREFA + 1);
    escreverTarefa(totalTarefas, buffer);
    totalTarefas++;
    EEPROM.update(ENDERECO_QUANTIDADE, totalTarefas);
    Serial.print("Adicionada: ");
    Serial.println(buffer);
    exibirTarefa();
  }
  else if (cmd.equalsIgnoreCase("CLEAR")) {
    limparTudo();
  }
  else if (cmd.startsWith("DEL:") || cmd.startsWith("DEL ")) {
    int num = cmd.substring(4).toInt();
    if (num < 1 || num > totalTarefas) {
      Serial.println("Erro: numero invalido");
      return;
    }
    for (int i = num - 1; i < totalTarefas - 1; i++) {
      char buffer[TAM_TAREFA + 1];
      lerTarefa(i + 1, buffer);
      escreverTarefa(i, buffer);
    }
    totalTarefas--;
    EEPROM.update(ENDERECO_QUANTIDADE, totalTarefas);
    if (tarefaAtual >= totalTarefas) tarefaAtual = 0;
    Serial.print("Tarefa ");
    Serial.print(num);
    Serial.println(" apagada");
    exibirTarefa();
  }
  else if (cmd.equalsIgnoreCase("HELP")) {
    imprimirAjuda();
  }
  else if (cmd.length() > 0) {
    Serial.println("Comando desconhecido. Digite HELP");
  }
}

void imprimirAjuda() {
  Serial.println();
  Serial.println("=== Comandos ===");
  Serial.println("LIST           - lista todas as tarefas");
  Serial.println("ADD: <texto>   - adiciona tarefa (max 16 chars)");
  Serial.println("DEL: <numero>  - apaga tarefa pelo numero");
  Serial.println("CLEAR          - apaga todas as tarefas");
  Serial.println("HELP           - mostra esta ajuda");
  Serial.println();
  Serial.println("=== Botao ===");
  Serial.println("Clique         - proxima tarefa");
  Serial.println("Duplo clique   - marca atual como concluida");
  Serial.println("Segurar 1s     - apaga todas");
  Serial.println();
}
