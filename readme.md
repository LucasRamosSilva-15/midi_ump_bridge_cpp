# 🎹 Conversor e Analisador MIDI 1.0 para MIDI 2.0 (UMP) - Versão C++

[![Release](https://img.shields.io/badge/Release-v5.1.0-00e5ff?style=for-the-badge&logo=github)](https://github.com/LucasRamosSilva-15/midi_ump_bridge_cpp/releases)
[![C++17](https://img.shields.io/badge/Standard-C%2B%2B17-blue?style=for-the-badge&logo=c%2B%2B)](https://en.cppreference.com/w/cpp/17)
[![Qt6](https://img.shields.io/badge/GUI-Qt%206.8-41cd52?style=for-the-badge&logo=qt)](https://www.qt.io/)
[![Plataformas](https://img.shields.io/badge/Plataformas-Linux%20%7C%20Windows-555?style=for-the-badge&logo=linux)](https://github.com/LucasRamosSilva-15/midi_ump_bridge_cpp/releases)

Bem-vindo à versão de alta performance do Trabalho de Conclusão de Curso (TCC) desenvolvido no **Instituto Federal da Paraíba (IFPB)** - Campus Campina Grande.

Originalmente concebido em Python, este projeto foi portado para **C++17** utilizando o framework **Qt6** para garantir latência ultra-baixa no processamento de pacotes **UMP (Universal MIDI Packet)**, uma interface gráfica moderna e uma bancada de validação acústica em tempo real com osciloscópio digital *Dual-Trace*.

> 🔗 **Repositório original de prototipagem (Python):** [github.com/LucasRamosSilva-15/midi-ump-bridge](https://github.com/LucasRamosSilva-15/midi-ump-bridge)

---

## 🎥 Demonstração em Vídeo

- 🎬 [Demonstração Principal no Google Drive](https://drive.google.com/file/d/18PeylgyTXSZNCRt1XhIqERS3m_TACnvB/view?usp=sharing)
- 🎬 [Segunda Demonstração no Google Drive](https://drive.google.com/file/d/18YTt_jQ61VT9AU4GRTGei_6wXqis1IXu/view?usp=sharing)

---

## 📑 Sumário

- [Novidades da Versão v5.1.0](#-novidades-da-versão-v510)
- [Pacotes Portáteis Standalone (Sem Instalação)](#-pacotes-portáteis-standalone-sem-instalação)
- [Funcionalidades](#-funcionalidades)
- [O que é MIDI?](#-o-que-é-midi)
- [Sobre o Projeto](#-sobre-o-projeto)
- [Tecnologias Utilizadas](#-tecnologias-utilizadas)
- [Ecossistema de Aplicações](#-ecossistema-de-aplicações)
- [Demonstração Visual](#-demonstração-visual)
- [Estrutura do Código](#-estrutura-do-código)
- [Validação e Comparação Acústica (Dual-Trace - Seção 4.3)](#-validação-e-comparação-acústica-dual-trace---seção-43)
- [Como Rodar a Bancada Completa de Testes](#-como-rodar-a-bancada-completa-de-testes)
- [Guia de Compilação Local (Linux)](#-guia-de-compilação-local-linux)
- [Guia de Compilação Local (Windows)](#-guia-de-compilação-local-windows)
- [Hardware e Controladores de Teste](#-hardware-e-controladores-de-teste)
- [Autor e Orientação](#-autor-e-orientação)

---

## 🌟 Novidades da Versão v5.1.0

A versão **v5.1.0** consolida os requisitos experimentais e de auditoria para a defesa do TCC:

- **Modulação CC1 com Síntese Harmônica FM:** A roda de modulação (*Modulation Wheel - CC1*) agora altera tanto o período (vibrato de $\pm 2.5$ semitons) quanto a geometria da onda através de **modulação de fase harmônica (FM)** com índice $\beta = \text{depth} \times 1.8$, permitindo ver e ouvir a transição da senóide pura ($f_0$) para um timbre rico em harmônicos e cristas.
- **Controle Deslizante Interativo de CC1:** Slider integrado ao osciloscópio com suporte a ajuste por mouse, setas do teclado (Cima/Baixo) ou mensagens MIDI 1.0 / UMP externas.
- **Trigger Zero-Crossing de Fase:** Sincronização automática por cruzamento de zero ascendente, eliminando o deslizamento estroboscópico e estabilizando a onda estática na tela a 60 FPS.
- **Auto-Reconexão Inteligente de MIDI:** Detecção a quente de portas MIDI virtuais e físicas, conectando automaticamente sem necessidade de reiniciar a aplicação.
- **Blindagem Defensiva contra Falhas:** Tratamento estrito contra valores `NaN`/`Inf`, prevenção de *clipping* de áudio, validação de portas abertas e compatibilidade multiplataforma contra macros legadas do Windows (`NOMINMAX`).
- **Pacotes Portáteis Standalone:** Automação de CI/CD via GitHub Actions gerando binários independentes para Windows x64 e Linux x86_64.

---

## 📦 Pacotes Portáteis Standalone (Sem Instalação)

Não é necessário ter o compilador C++ ou o Qt6 instalados para executar o projeto. Baixe os pacotes pré-compilados diretamente na aba de **[Releases](https://github.com/LucasRamosSilva-15/midi_ump_bridge_cpp/releases)**:

### 🪟 Windows (10 / 11 de 64-bit)
1. Baixe o arquivo **`MIDI2Bridge-v5.1.0-Windows-x64.zip`**;
2. Extraia o arquivo ZIP em qualquer pasta;
3. Execute com dois cliques:
   - `MIDI2Bridge.exe` para a ponte e analisador UMP;
   - `ump_oscilloscope.exe` para o osciloscópio acústico dual-trace.
*(Todas as DLLs do Qt6, plugins SVG e bibliotecas de runtime já estão embutidas).*

### 🐧 Linux (x86_64)
1. Baixe o arquivo **`MIDI2Bridge-v5.1.0-Linux-x86_64.tar.gz`** (ou `.zip`);
2. Extraia o conteúdo:
   ```bash
   tar -xzf MIDI2Bridge-v5.1.0-Linux-x86_64.tar.gz
   cd MIDI2Bridge-v5.1.0-Linux-x86_64
   ```
3. Execute através dos scripts de inicialização inclusos:
   ```bash
   ./MIDI2Bridge.sh         # Ponte Analisadora Principal
   ./ump_oscilloscope.sh    # Osciloscópio Acústico
   ./ump_audio_sink.sh      # Receptor de Console
   ```

---

## 🚀 Funcionalidades

- **Captura em Tempo Real:** Escuta dispositivos e controladores MIDI 1.0 físicos e virtuais sem bloqueio de thread.
- **Tradução UMP MT4 & MT2:** Converte eventos legados de *Note On*, *Note Off*, *Control Change* e *Pitch Bend* para pacotes de 64 bits (*Message Type 4* - MIDI 2.0 Channel Voice) e aceita pacotes legados UMP de 32 bits (*Message Type 2*).
- **Expansão de Resolução Matemática:**
  - *Velocity* escalonada de 7 bits ($[0, 127]$) para 16 bits ($[0, 65535]$) com saturação plena conforme as diretrizes oficiais da *MIDI Association*.
  - *Pitch Bend* e *Control Change* projetados para resolução contínua de 32 bits ($[0, 4294967295]$).
- **Interface Gráfica Analisadora (Qt6):** Exibição em tempo real do log de pacotes, canais, notas, alvos, resolução original e palavras hexadecimais UMP (`word1` e `word2`).
- **Osciloscópio Digital Acústico Integrado:** Sintetizador analógico virtual e osciloscópio gráfico de 60 FPS com modo comparativo **Dual-Trace (Canal Duplo)** para confrontar o som original de referência com o som modulado.

---

## 🎼 O que é MIDI?

Se você não é da área de tecnologia musical, uma boa regra para entender o MIDI (*Musical Instrument Digital Interface*) é: **MIDI não é áudio, é partitura digital.**

Diferente de um arquivo MP3 ou WAV (que gravam ondas sonoras prontas), o protocolo MIDI transmite **instruções de controle**:
* **Qual nota foi tocada?** (identificador numérico da tecla, ex: 60 para o Dó Central);
* **Com que força a tecla foi pressionada?** (*Velocity*, indicando a intensidade de execução);
* **Quando a nota foi liberada?** (*Note Off*);
* **Controles contínuos e modulação:** Alavanca de *Pitch Bend* (desvio de afinação) ou roda de modulação (CC1).

O computador ou sintetizador recebe esses comandos e sintetiza o som instantaneamente.

---

## 🛠️ Sobre o Projeto

Esta Prova de Conceito (PoC) atua como a ponte tecnológica entre o protocolo clássico MIDI 1.0 (de resolução restrita a 7/14 bits) e o novo padrão da indústria **MIDI 2.0 UMP** (palavras estruturadas de 32/64 bits). 

O sistema elimina o histórico problema de *zipper noise* (ruído de degrau audível nas transições) e garante determinismo temporal estrito através de uma arquitetura multithread desacoplada em C++.

---

## 💻 Tecnologias Utilizadas

- **Linguagem:** C++17
- **Framework GUI:** Qt 6 (Módulos `Widgets`, `Core`, `Gui`, `Svg`)
- **Drivers de Entrada/Saída MIDI:** [RtMidi 6.0.0](https://github.com/thestk/rtmidi) (com suporte nativo a ALSA no Linux e WinMM no Windows)
- **Motor de Áudio e Síntese DSP:** [miniaudio](https://miniaud.io) (áudio analógico estéreo a 48.000 Hz sem dependências pesadas)
- **Build System:** CMake 3.16+
- **Especificação:** Padrão oficial MIDI 2.0 / Universal MIDI Packet (UMP M2-104-UM)

---

## 📦 Ecossistema de Aplicações

O projeto compila três alvos executáveis complementares:

1. **`MIDI2Bridge`:** A aplicação principal. Contém a interface gráfica skeuomórfica, gerenciador de conexões seriais/USB e motor de conversão de pacotes UMP em tempo real.
2. **`ump_oscilloscope`:** Ferramenta gráfica de laboratório que recebe os pacotes UMP via porta virtual, sintetiza o tom sonoro e exibe o osciloscópio digital em tempo real com modo comparativo *Dual-Trace*.
3. **`ump_audio_sink`:** Receptor utilitário de console leve para bancadas de teste headless e validação acústica rápida.

---

## 📸 Demonstração Visual

Abaixo, a bancada experimental completa rodando em regime de concorrência: o controlador virtual (*VMPK*), a ponte (*MIDI2Bridge*) auditando os pacotes e o osciloscópio (*ump_oscilloscope*) comparando a onda de referência com a onda modulada:

![Interface principal monitorando mensagens MIDI](capturas/Captura6.png)
![Gráfico de Pitch Bend em alta resolução](capturas/Captura7.png)
![Osciloscópio capturando a mensagem MIDI 2.0](capturas/Captura8.png)

---

## 🧪 Validação e Comparação Acústica (Dual-Trace - Seção 4.3)

O osciloscópio digital foi concebido especificamente para atender aos requisitos da **Seção 4.3 da monografia de TCC**, permitindo comparar visual e acusticamente a onda em estado fundamental com a onda modulada:

| Parâmetro de Análise | Som Original (Referência $f_0$) | Som Modulado (Pitch Bend / Modulação CC1) |
| :--- | :---: | :---: |
| **Canal no Osciloscópio** | **CH1 (Azul Tracejado)** | **CH2 (Verde Contínuo)** |
| **Forma Geométrica** | Senóide fundamental pura | Deformação harmônica FM $\sin(\phi + \beta \sin(\phi))$ |
| **Frequência e Período** | $f_0$ e $T_0 = 1/f_0$ fixados | $f(t)$ e $T(t)$ variando dinamicamente ($\Delta f$ e $\Delta T$) |
| **Modulação CC1 (Wheel)** | Desativada ($\beta = 0$, profundidade 0%) | Ativa (Índice FM $\beta$ até 1.8 + vibrato $\pm 2.5$ st + tremolo) |
| **Resolução de Entrada** | 7 bits / 14 bits (MIDI 1.0 legado) | 16 bits / 32 bits (MIDI 2.0 UMP de alta precisão) |
| **Resposta Acústica** | Tom puro senoidal estático | Timbre rico com cristas harmônicas sem *zipper noise* |

---

## 🔬 Como Rodar a Bancada Completa de Testes

Para reproduzir o experimento de validação acústica do TCC:

1. **Abra o Osciloscópio Digital:**
   ```bash
   ./build/ump_oscilloscope
   ```
   *(Ele iniciará o subsistema de áudio e criará automaticamente a porta virtual `UMP Audio Sink Input`)*.

2. **Abra a Ponte (`MIDI2Bridge`):**
   ```bash
   ./build/MIDI2Bridge
   ```
   - Em **Entrada MIDI**: Selecione o seu teclado físico ou o teclado virtual (*VMPK*);
   - Em **Saída MIDI**: Selecione **`UMP Audio Sink Input`**;
   - Clique em **Conectar / Iniciar**.

3. **Toque e Valide:**
   - Ao tocar as teclas, o som é sintetizado e a onda senoidal pura é desenhada a 60 FPS com sincronização de fase.
   - Pressione **Espaço** ou clique em **`[ 📌 Fixar Som Original ]`** para congelar a onda de referência (traço tracejado azul).
   - Movimente a alavanca de **Pitch Bend** ou ajuste a **Modulação CC1** (pelo slider na tela, setas do teclado ou no controlador MIDI).
   - A onda verde se deforma e modula em tempo real sobre a referência azul, exibindo o desvio em Hertz ($\Delta f$), a variação do período temporal ($\Delta T$) e o índice harmônico FM ($\beta$).

---

## 📁 Estrutura do Código

```text
midi_ump_bridge_cpp/
├── src/
│   ├── main.cpp              # Ponto de entrada do MIDI2Bridge
│   ├── gui.h / gui.cpp       # Janela principal em Qt6 e telemetria
│   ├── ump.h                 # Estrutura de dados UMP de 64 bits e análise binária
│   └── converter.h / .cpp    # Algoritmo de bit-shifting e upscaling matemático
├── tools/
│   ├── ump_oscilloscope_main.cpp  # Interface e lógica do osciloscópio digital
│   ├── oscilloscope.h             # Widget QPainter para renderização das ondas
│   └── ump_audio_sink.cpp         # Receptor de teste sonoro em linha de comando
├── scripts/
│   ├── package_release.sh    # Script de empacotamento standalone para Linux
│   └── package_windows.ps1   # Script de empacotamento standalone para Windows
├── third_party/
│   └── miniaudio.h           # Biblioteca de reprodução de áudio em tempo real
├── tests/
│   └── converter_tests.cpp   # Testes unitários do conversor UMP
├── capturas/                 # Imagens e evidências para a monografia do TCC
├── resources.qrc             # Recursos vetoriais e ícones SVG
└── CMakeLists.txt            # Configuração e orquestração de compilação
```

---

## 🐧 Guia de Compilação Local (Linux)

Distros baseadas em Debian/Ubuntu/Fedora:

### 1. Instalando Dependências
```bash
# Ubuntu / Debian / Pop!_OS
sudo apt update
sudo apt install build-essential cmake qt6-base-dev libqt6svg6-dev libasound2-dev

# Fedora
sudo dnf install gcc-c++ cmake qt6-qtbase-devel qt6-qtsvg-devel alsa-lib-devel
```

### 2. Compilando Todos os Alvos
```bash
mkdir -p build && cd build
cmake ..
make -j$(nproc)
```

---

## 🪟 Guia de Compilação Local (Windows)

Requer o **Visual Studio 2022** (com suporte a C++), **CMake** e **Qt6 (MSVC 2022 64-bit)**:

```powershell
mkdir build
cd build
cmake -DCMAKE_BUILD_TYPE=Release ..
cmake --build . --config Release
```

Os binários serão gerados na pasta `build\Release\`.

---

## 🎹 Hardware e Controladores de Teste

- **Teclado Físico Homologado:** Yamaha PSR E383 (USB-MIDI).
- **Controlador Virtual:** Virtual MIDI Piano Keyboard (VMPK).
- **Roteamento Serial / Virtual:** ALSA Virtual RawMIDI / virmidi (Linux) e loopMIDI (Windows).

---

## 👨‍💻 Autor e Orientação

- **Autor:** Lucas Ramos Silva ([GitHub](https://github.com/LucasRamosSilva-15))
- **Instituição:** Instituto Federal de Educação, Ciência e Tecnologia da Paraíba (IFPB) - Campus Campina Grande
- **Curso:** Engenharia de Computação / Telemática
- **Orientador:** Prof. Carlos Henrique Alencar Almeida
