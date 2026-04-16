# Speed-Net

Monitora sua velocidade de internet e tráfego de rede em tempo real, exibindo os dados no OLED do STM32WB5MM-DK.

## Como funciona

```
[Python FastAPI] --serial--> [STM32WB5MM-DK] --> [OLED SSD1315]
```

- **Python** roda o speedtest, mede RX/TX em tempo real e envia JSON via serial a cada 1s
- **STM32** recebe o JSON e renderiza no display OLED 128x64

---

## Requisitos

- Python 3.10+
- PlatformIO CLI
- STM32WB5MM-DK conectado via USB (ST-Link)

---

## Setup

### 1. Clonar e criar ambiente virtual

```bash
cd speed-net
python3 -m venv .venv
source .venv/bin/activate        # Mac/Linux
# .\.venv\Scripts\activate       # Windows
pip install -r requirements.txt
```

### 2. Gravar firmware no STM32

```bash
cd firmware
pio run --target upload
cd ..
```

> **Windows**: o upload requer STM32CubeProgrammer. Use `flash.bat` no lugar do comando acima.

### 3. Descobrir a porta serial

**Mac:**
```bash
ls /dev/cu.*
# Procura algo como /dev/cu.usbmodem1234
```

**Linux:**
```bash
ls /dev/ttyACM*
# Normalmente /dev/ttyACM0
```

**Windows:**
```
Gerenciador de Dispositivos > Portas (COM e LPT)
# Normalmente COM3
```

### 4. Rodar a API

**Mac/Linux** (auto-detecta a porta):
```bash
source .venv/bin/activate
python3 main.py
```

**Mac** (se não detectar automaticamente):
```bash
export SERIAL_PORT=/dev/cu.usbmodem1234
python3 main.py
```

**Windows:**
```powershell
$env:SERIAL_PORT="COM3"
python main.py
```

---

## Variáveis de ambiente

| Variável | Padrão | Descrição |
|---|---|---|
| `SERIAL_PORT` | auto-detect | Porta serial do STM32 |
| `BAUD_RATE` | `115200` | Baud rate |
| `PORT` | `8000` | Porta HTTP da API |
| `SPEEDTEST_INTERVAL` | `300` | Intervalo entre speedtests (segundos) |

---

## Endpoints da API

| Método | Rota | Descrição |
|---|---|---|
| `GET` | `/status` | Retorna os dados atuais |
| `POST` | `/speedtest` | Dispara um speedtest imediato |

Exemplo:
```bash
curl http://localhost:8000/status
```

---

## Display OLED

```
┌─────────────────────────┐
│ SPEED-NET          [OK] │
│─────────────────────────│
│ ↓ 380.6 Mbps           │
│ ↑ 341.1 Mbps           │
│ ~ 5 ms                 │
│─────────────────────────│
│ RX:1.23      TX:0.45   │
└─────────────────────────┘
```

- `↓` Download (speedtest)
- `↑` Upload (speedtest)
- `~` Ping
- `RX/TX` Tráfego em tempo real (atualiza a cada 1s)
- Badge: `[OK]` pronto · `[..]` testando · `[!!]` erro

---

## Notas por plataforma

### Mac / Linux
- ST-Link funciona nativamente, sem driver adicional
- Porta serial detectada automaticamente no Linux

### Windows
- Requer instalação do driver oficial ST-Link: [STSW-LINK009](https://www.st.com/en/development-tools/stsw-link009.html)
- Upload via `flash.bat` (usa STM32CubeProgrammer)
- Comunicação serial pode ser instável via ST-Link VCP — preferir Mac/Linux
