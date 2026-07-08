# tarefas-arduino

Sincroniza sua lista de tarefas do [TickTick](https://ticktick.com) com um
display LCD controlado por Arduino, via serial. Um botão no Arduino permite
navegar entre as tarefas, marcar como concluída e limpar tudo.

O fluxo é desacoplado por um arquivo de texto simples
(`~/.config/tarefas-arduino/tarefas.txt`, uma tarefa por linha): qualquer coisa
que escreva nele pode alimentar o display. Aqui usamos o TickTick como fonte.

```
TickTick  --tarefas-fetch-->  tarefas.txt  --tarefas-arduino-->  Arduino (LCD)
```

## Hardware

- Arduino (testado em Uno/Nano ATmega328)
- Display LCD 16x2 I2C (endereço `0x27`)
- 1 botão no pino digital 2 (ativo em LOW, pull-up interno)

Bibliotecas Arduino: `Wire`, `LiquidCrystal_I2C`, `EEPROM`, `OneButton`.
Sketch em [`arduino/tarefas/tarefas.ino`](arduino/tarefas/tarefas.ino).

> Existe também uma variante por Bluetooth (não incluída aqui) que usa o mesmo
> protocolo por uma serial Bluetooth em vez da USB.

### Protocolo serial (9600 baud)

| Comando        | Resposta esperada     | Ação                          |
| -------------- | --------------------- | ----------------------------- |
| `CLEAR`        | contém `apagadas`     | Apaga todas as tarefas        |
| `ADD: <texto>` | contém `Adicionada`   | Adiciona uma tarefa (max 16c) |

Controles físicos: clique = próxima tarefa; clique duplo = concluir; segurar 1s
= limpar tudo. As tarefas ficam salvas na EEPROM (persistem sem energia).

## Componentes de software

| Arquivo                        | Papel                                                      |
| ------------------------------ | ---------------------------------------------------------- |
| `bin/tarefas-arduino`          | Abre editor para revisar `tarefas.txt` e envia ao Arduino  |
| `bin/tarefas-fetch`            | Puxa tarefas do TickTick para `tarefas.txt`                |
| `bin/tarefas-daily`            | Wrapper não-interativo (fetch + sync) com log e notificação |
| `systemd/*.service` / `*.timer`| Executa `tarefas-daily` todo dia às 6h                      |

Dependências: `bash`, `curl`, `jq`, `python3` (para o redirect OAuth),
`xdg-open`, e um `stty` compatível (Linux).

## Instalação

```sh
# 1. Grave o sketch no Arduino (Arduino IDE ou arduino-cli).

# 2. Instale os scripts no PATH:
install -Dm755 bin/tarefas-fetch   ~/.local/bin/tarefas-fetch
install -Dm755 bin/tarefas-arduino ~/.local/bin/tarefas-arduino
install -Dm755 bin/tarefas-daily   ~/.local/bin/tarefas-daily
```

## Configurando o TickTick (uma vez)

O TickTick usa OAuth2, então é preciso registrar um app (grátis):

1. Acesse <https://developer.ticktick.com/manage> e crie um app.
2. Configure o **Redirect URI** exatamente como: `http://127.0.0.1:8080/`
3. Copie o **Client ID** e o **Client Secret**.
4. Autorize:

   ```sh
   tarefas-fetch auth      # pede as credenciais, abre o navegador, clique "Allow"
   ```

O token dura ~6 meses; quando expirar, rode `tarefas-fetch auth` de novo.
Credenciais e token são gravados em `~/.config/tarefas-arduino/` com permissão
`600` e **nunca** entram neste repositório (veja `.gitignore`).

## Uso

```sh
tarefas-fetch lists              # mostra suas listas do TickTick
tarefas-fetch use "Nome da Lista" # escolhe de qual lista puxar
tarefas-fetch use today           # ou modo "Hoje": vencidas + para hoje (+ Inbox)

tarefas-fetch                     # puxa as tarefas para tarefas.txt (top 10, truncadas em 16c)
tarefas-arduino                   # revisa no editor e sincroniza ao salvar

# Tudo de uma vez, sem prompts:
tarefas-fetch && tarefas-arduino --no-editor
```

Variáveis de ambiente úteis: `TAREFAS_PORTA` (padrão `/dev/ttyUSB0`), `EDITOR`.

## Execução automática (systemd, diária às 6h)

```sh
install -Dm644 systemd/tarefas-arduino.service ~/.config/systemd/user/tarefas-arduino.service
install -Dm644 systemd/tarefas-arduino.timer   ~/.config/systemd/user/tarefas-arduino.timer
systemctl --user daemon-reload
systemctl --user enable --now tarefas-arduino.timer

systemctl --user list-timers tarefas-arduino.timer   # confere o próximo disparo
cat ~/.config/tarefas-arduino/daily.log              # histórico de execuções
```

A máquina precisa estar ligada e o Arduino conectado às 6h. Bloqueio de tela
não atrapalha; suspensão sim (`Persistent=true` roda atrasado no próximo boot).
Falhas (token expirado, Arduino desconectado) geram notificação via
`notify-send` e ficam no log.

## Limitações conhecidas

- **Inbox**: no modo `today`, tarefas da Inbox são incluídas via o id especial
  `inbox`. Tarefas sem data de vencimento não aparecem no `today` (igual à
  smart view "Today" do próprio TickTick).
- **Fuso horário**: a comparação de "hoje" usa a parte da data (`YYYY-MM-DD`) do
  vencimento. Tarefas com horário específico perto da meia-noite podem cair no
  dia errado; tarefas de dia inteiro não são afetadas.
- Limite do display: **10 tarefas, 16 caracteres cada**.

## Licença

MIT
