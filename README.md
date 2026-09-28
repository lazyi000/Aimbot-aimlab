# 🎯 aimlab

Macro de detecção de cor em C++ com painel de controle (GUI) em Win32 + OpenGL + **Dear ImGui**. Captura a tela, busca pixels da cor alvo, move o mouse até eles com velocidade limitada e dispara clique esquerdo quando o centro da tela coincide com a cor alvo.

![License](https://img.shields.io/badge/license-MIT-yellow.svg)
![Language](https://img.shields.io/badge/C%2B%2B-17-blue.svg)
![Platform](https://img.shields.io/badge/platform-Windows%2010%2F11-lightgrey.svg)
![Vendor](https://img.shields.io/badge/imgui-v1.90-green.svg)
![Build](https://img.shields.io/github/actions/workflow/status/lazyi000/Aimbot-aimlab/build.yml)

> ### ⚠️ Aviso importante
>
> Use este projeto **apenas em ambientes próprios / single-player**. Ferramentas de automação desse tipo violam os termos de serviço de jogos online e podem causar **banimento permanente**. O autor não se responsabiliza pelo uso indevido.

---

## ✨ Recursos

* **GUI moderna** (Dear ImGui) — sempre no topo, arrastável e auto-redimensionável
* **Botão grande ON/OFF** + **hotkey global** `Ctrl + PageUp` para ligar/desligar
* **Color picker / debug** — botão "Sample at center" que amostra a cor média do centro da tela
* **Detecção de cor** por distância RGB com tolerância configurável e filtro de ruído 3×3
* **Movimento do mouse** com velocidade máxima em px/frame (evita overshoot) e trava em alvo ("lock")
* **Auto-clique** com debounce: dispara uma vez ao entrar na cor, repete só após sair e voltar
* **Seleção de janela alvo** (window picker) + leitura de memória de processo (`ReadProcessMemory`)
* **Presets salvos** — perfis de configuração com nomes (persistidos em `user_presets.dat`)
* **i18n** — interface em **Português** e **Inglês**
* Dois métodos de injeção de mouse: `mouse_event` (padrão) e `SendInput`
* Status em tempo real: FPS, frames, cliques, último alvo encontrado

---

## 🚀 Começando — do zero até usar

### 1. Baixar o projeto

**Opção A — Git (recomendado):**

```bat
git clone https://github.com/lazyi000/Aimbot-aimlab.git
cd aimlab
```

**Opção B — ZIP:** no GitHub, clique em **Code → Download ZIP** e extraia em uma pasta.

### 2. Instalar os pré-requisitos

Você precisa do **MinGW-w64 (`g++`)** para compilar. Windows 10/11 de 64 bits.

**Opção 1 — WinLibs (mais simples):**

1. Acesse [WinLibs](https://winlibs.com/) e baixe o pacote *Win64* mais recente (ex.: GCC + UCRT runtime).
2. Extraia o conteúdo para `C:\mingw64`.
3. Adicione `C:\mingw64\bin` ao **PATH** do Windows (Configurações → Sistema → Sobre → Configurações avançadas → Variáveis de ambiente → selecione `Path` → Editar → Novo → cole `C:\mingw64\bin`).
4. Abra um **novo terminal** e confirme:

```bat
g++ --version
```

**Opção 2 — MSYS2:**

1. Instale o [MSYS2](https://www.msys2.org/).
2. No terminal do MSYS2:

```bat
pacman -S mingw-w64-ucrt-x86_64-gcc
```

3. Adicione `C:\msys64\ucrt64\bin` ao **PATH** (mesmo passo da Opção 1) e confirme com `g++ --version`.

### 3. Compilar

**Opção A — script prontinho (recomendado):**

```bat
build.bat
```

**Opção B — manual (mesmo comando que o script usa):**

```bat
g++ -O2 -std=c++17 -Isrc -Ithird_party\imgui ^
    src\main.cpp ^
    src\capture\color_match.cpp src\capture\color_sample.cpp src\capture\screen_capture.cpp ^
    src\core\aim_worker.cpp ^
    src\input\hotkey.cpp src\input\memory_reader.cpp src\input\mouse_input.cpp ^
    src\ui\logo.cpp src\ui\window_picker.cpp ^
    third_party\imgui\imgui.cpp third_party\imgui\imgui_draw.cpp ^
    third_party\imgui\imgui_tables.cpp third_party\imgui\imgui_widgets.cpp ^
    third_party\imgui\imgui_impl_win32.cpp third_party\imgui\imgui_impl_opengl3.cpp ^
    -lopengl32 -lgdi32 -luser32 -ldwmapi -lole32 -lwindowscodecs ^
    -static-libgcc -static-libstdc++ -mwindows ^
    -o aimlab.exe
```

✅ Resultado: **`aimlab.exe`** (~13 MB) na raiz do projeto — **sem DLLs externas**, pode rodar em qualquer Windows.

**Compilação automática (CI):** o repositório tem uma [GitHub Action](.github/workflows/build.yml) que compila o projeto a cada push e disponibiliza o `.exe` como artefato no GitHub (botão *Actions* → run mais recente → *Artifacts*).

### 4. Usar

1. **Execute `aimlab.exe`** — a janela abre no centro da tela, sempre no topo. Arraste pela faixa superior (30 px).
2. **Escolha a cor alvo**: ajuste R/G/B manualmente, ou posicione o crosshair no alvo e clique em **Sample at center** (a cor média do centro é definida como alvo).
3. **Configure a detecção**:

   * **Tolerance** — distância RGB máxima para o pixel ser considerado alvo (25 para cores sólidas; 60+ para tons de pele/reflexos).
   * **Min neighbors (3×3)** — filtra pixels isolados (1 aceita qualquer match; 3+ exige um cluster 3×3).
4. **Configure o movimento**:

   * **Max px / frame** — pixels por frame (25–40 suave, 80+ snappy).
   * **Input method** — `mouse_event` na maioria dos jogos; troque para `SendInput` se o input for ignorado.
5. **Auto-clique** — deixe marcado para disparar quando o pixel central bater com a cor.
6. **Ligue** clicando no botão grande ou com `Ctrl + PageUp`. O painel mostra o status em tempo real.

### 5. (Opcional) Ícone personalizado

O script `scripts/embed_icon.ps1` embute um PNG como ícone do executável:

```powershell
powershell -ExecutionPolicy Bypass -File scripts/embed_icon.ps1 -ExePath aimlab.exe -PngPath meu_icone.png
```

---

## 📁 Estrutura do projeto

```text
aimlab/
├── .github/workflows/build.yml   # CI: compila e gera artefato a cada push
├── src/
│   ├── main.cpp                  # WinMain, OpenGL, ImGui, GUI, message loop
│   ├── config.h                  # Settings + ConfigStore (thread-safe)
│   ├── i18n.h                    # Traduções PT/EN (tr("key"))
│   ├── capture/                  # Captura e detecção de cor
│   │   ├── screen_capture.*      #   GDI BitBlt da tela
│   │   ├── color_sample.*        #   Amostrador RGB no centro
│   │   └── color_match.*         #   Detector RGB + nearest-to-anchor
│   ├── core/
│   │   └── aim_worker.*           # Thread de captura → detecção → aim → click
│   ├── input/
│   │   ├── hotkey.*               # RegisterHotKey Ctrl+PageUp
│   │   ├── mouse_input.*          # mouse_event + SendInput
│   │   └── memory_reader.*        # Leitura de memória de processo (ReadProcessMemory)
│   └── ui/
│       ├── logo.*                 # Carrega PNG (WIC) para textura GL / ícone
│       └── window_picker.*        # Seletor de janela alvo
├── third_party/
│   └── imgui/                     # Dear ImGui v1.90 (vendorado, licença MIT própria)
├── scripts/
│   └── embed_icon.ps1             # Helper de build
├── build.bat                      # Compilação com MinGW (um clique)
├── .gitignore                     # Artefatos de build/runtime fora do repo
├── LICENSE                        # MIT
└── README.md
```

> A pasta `launcher/` (app auxiliar separado) existe no ambiente de desenvolvimento,
> mas fica **fora deste repositório**.

## ⚙️ Como funciona

A cada frame, o `AimWorker` (thread separada):

1. **Captura** a tela via `BitBlt` (GDI) — ou usa a janela alvo selecionada no window picker.
2. **Procura** o pixel mais próximo do centro da tela cuja distância RGB à cor alvo é ≤ tolerância (com filtro 3×3 de vizinhos mínimos).
3. **Move** o cursor em direção ao alvo, limitado por `Max px / frame` (evita overshoot) e trava em "lock" na posição persistente.
4. **Dispara** um único clique quando o pixel central coincide com a cor alvo (cooldown de 150 ms).

A GUI é independente: lê `Settings` (atômicos + mutex) e exibe status. A hotkey `Ctrl + PageUp` liga/desliga `aimEnabled` no `ConfigStore`, e o worker reage no próximo frame.

## 🤝 Como contribuir

1. Faça um **fork** do repositório.
2. Crie uma branch:

```bash
git checkout -b minha-feature
```

3. **Compile localmente** com `build.bat` para garantir que não quebrou nada.
4. Abra um **Pull Request** com uma descrição clara.

Sugestões de melhorias: captura via DirectX/WGC (mais rápido que GDI), detecção em outro espaço de cor (HSV), mais métodos de input.

## 🧪 Limitações conhecidas

* Captura de tela via GDI é o gargalo: ~30–60 FPS em 1920×1080. Em 4K, reduza a área de captura.
* Jogos com **Raw Input exclusivo** podem ignorar o input gerado. Tente alternar `mouse_event` ↔ `SendInput`.
* Detecção em espaço RGB — gradientes sutis/transparências exigem ajuste fino de `Tolerance`.
* O "centro da tela" é fixo (resolução ÷ 2) e não rastreia o crosshair do jogo (proposital: o macro só conhece movimentos **relativos** que ele mesmo enviou).
* `user_presets.dat` e `imgui.ini` são gerados em runtime e não fazem parte do repositório.

## 👤 Créditos

Feito com ❤️ por **vkzin**.

## 📄 Licença

Distribuído sob a **Licença MIT** — veja o arquivo [LICENSE](LICENSE). O Dear ImGui possui licença MIT própria (arquivo `third_party/imgui/LICENSE.txt`).

**Use por sua conta e risco. Não use em jogos online.**
