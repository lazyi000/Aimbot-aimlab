#pragma once

// ----- Internationalization (i18n) for the control panel -----
//
// The UI is built around a single tr("key") call. The current language
// is read from ConfigStore and is persisted as part of the Settings
// struct. main.cpp syncs it into g_currentLanguage at the start of
// every frame so tr() can be a one-argument inline lookup.

#include <cstring>
#include "config.h"

namespace aimlab { namespace i18n {

struct Entry {
    const char* key;
    const char* en;
    const char* pt;
};

// Set every frame by DrawControlPanel() before the UI is built.
inline Language g_currentLanguage = Language::Portuguese;

// Lookup a translation by key. Falls back to the key itself on miss
// (so missing translations show up as "?key?" in the UI during dev).
inline const char* tr(const char* key) {
    static const Entry table[] = {
        // ===== Title / chrome =====
        {"title",                "MacroBot aimlab",                        "MacroBot aimlab"},
        {"subtitle",             "By vkzin",                               "Por vkzin"},
        {"discord",              "Discord",                                "Discord"},
        {"close",                "Close  (Ctrl + Esc)",                    "Fechar  (Ctrl + Esc)"},
        {"aim_on",               "  AIM IS ON  ",                          "  MIRA LIGADA  "},
        {"aim_off",              "  AIM IS OFF  ",                         "  MIRA DESLIGADA  "},

        // ===== Hotkey =====
        {"hotkey_label",         "  hotkey:",                              "  tecla:"},
        {"hotkey_change",        "Change...",                              "Mudar..."},
        {"hotkey_capture",       "Press a key combo...   (ESC to cancel)", "Pressione uma combinação...   (ESC para cancelar)"},

        // ===== Settings / language =====
        {"section_settings",     "Settings",                               "Configurações"},
        {"language",             "Language",                               "Idioma"},
        {"lang_english",         "English",                                "Inglês"},
        {"lang_portuguese",      "Português",                              "Português"},
        {"search_region",        "Search region (px)",                     "Região de busca (px)"},
        {"search_region_tt",     "When a target lock is active, only this\n"
                                 "many pixels around the lock are captured\n"
                                 "and scanned (instead of the whole screen).\n"
                                 "Bigger = finds new targets faster but slower.",
                                                                  "Quando o lock de alvo está ativo, só essa\n"
                                                                  "quantidade de pixels ao redor do lock é\n"
                                                                  "capturada e varrida (em vez da tela toda).\n"
                                                                  "Maior = acha alvos novos mais rápido, mas mais lento."},
        {"frame_delay",          "Frame delay (ms)",                       "Atraso entre quadros (ms)"},
        {"frame_delay_tt",       "Sleep between worker iterations.\n"
                                 "Lower = higher FPS but more CPU.\n"
                                 "0 = no sleep (busy-loop, ~1000+ FPS).",
                                                                  "Pausa entre iterações do worker.\n"
                                                                  "Menor = mais FPS, mais CPU.\n"
                                                                  "0 = sem pausa (busy-loop, 1000+ FPS)."},

        // ===== Presets =====
        {"section_presets",      "Presets",                                "Perfis"},
        {"preset_apply",         "Apply",                                  "Aplicar"},
        {"preset_save",          "Save current...",                        "Salvar atual..."},
        {"preset_separator",     "  - user saved -",                       "  - salvos pelo usuário -"},
        {"preset_modal_title",   "Save preset",                            "Salvar perfil"},
        {"preset_name",          "Name:",                                  "Nome:"},
        {"preset_current",       "Current:  R=%d G=%d B=%d   tol=%d   speed=%d",
                                                             "Atual:  R=%d G=%d B=%d   tol=%d   veloc.=%d"},
        {"save",                 "Save",                                   "Salvar"},
        {"cancel",               "Cancel",                                 "Cancelar"},
        {"default",              "Default",                                "Padrão"},
        {"aggressive",           "Aggressive",                             "Agressivo"},
        {"conservative",         "Conservative",                           "Conservador"},
        {"sniper",               "Sniper",                                 "Sniper"},

        // ===== Target window =====
        {"section_target_window","Target window",                          "Janela alvo"},
        {"scope_full",           "Full screen",                            "Tela inteira"},
        {"scope_window",         "Specific window",                        "Janela específica"},
        {"select_window",        "Select window...",                       "Escolher janela..."},
        {"active",               " [active]",                              " [ativo]"},
        {"not_found",            " [not found]",                           " [não encontrada]"},
        {"no_window_selected",   "(no window selected)",                   "(nenhuma janela escolhida)"},
        {"scope_tooltip",        "Full screen: capture the primary monitor.\n"
                                 "Specific window: capture & aim inside a chosen\n"
                                 "target window only (e.g. the game).",
                                                                  "Tela inteira: captura o monitor principal.\n"
                                                                  "Janela específica: captura e mira só dentro de uma\n"
                                                                  "janela alvo (ex.: o jogo)."},
        {"select_window_tooltip","Open the window picker (task-manager style):\n"
                                 "choose the window the aimbot should capture.",
                                                                  "Abre o seletor de janelas (estilo gerenciador de tarefas):\n"
                                                                  "escolha a janela que o aimbot deve capturar."},
        {"searches_whole_monitor","(searches the whole primary monitor)",  "(busca no monitor principal inteiro)"},

        // ===== Target color =====
        {"section_target_color", "Target color",                           "Cor do alvo"},
        {"sample_radius",        "Sample radius (px)",                     "Raio de amostragem (px)"},
        {"sample_at_center",     "Sample at center",                       "Amostrar no centro"},
        {"sample_tooltip",       "Captures the screen once, averages a (2r+1)px\n"
                                 "box around the screen center, and sets the\n"
                                 "target color to that average.",
                                                                  "Captura a tela uma vez, tira a média de uma caixa de\n"
                                                                  "(2r+1)px ao redor do centro da tela e define a cor\n"
                                                                  "alvo como essa média."},

        // ===== Detection =====
        {"section_detection",    "Detection",                              "Detecção"},
        {"color_tolerance",      "Color tolerance",                        "Tolerância de cor"},
        {"color_tolerance_tt",   "Per-channel RGB distance (squared, summed)\n"
                                 "at which a pixel still counts as the target color.",
                                                                  "Distância RGB por canal (ao quadrado, somada) na\n"
                                                                  "qual um pixel ainda conta como a cor alvo."},
        {"min_neighbors",        "Min pixel neighbors",                    "Mín. de vizinhos (px)"},
        {"min_neighbors_tt",     "How many of the 8 surrounding pixels must also\n"
                                 "match the target color (filters isolated noise).",
                                                                  "Quantos dos 8 pixels ao redor também precisam bater\n"
                                                                  "com a cor alvo (filtra ruído isolado)."},
        {"cluster_label",        "  Target cluster (where the aim actually points):",
                                                                  "  Cluster alvo (onde a mira realmente aponta):"},
        {"cluster_radius",       "Cluster radius (px)",                    "Raio do cluster (px)"},
        {"cluster_radius_tt",    "Window radius used to count matching pixels\n"
                                 "around each candidate. Bigger = stickier to large blobs.",
                                                                  "Raio da janela usada para contar pixels iguais ao\n"
                                                                  "redor de cada candidato. Maior = mais grudado em blobs grandes."},
        {"min_cluster",          "Min cluster size",                       "Tamanho mín. do cluster"},
        {"min_cluster_tt",       "Minimum density of matching pixels required\n"
                                 "before a match is considered a real target.",
                                                                  "Densidade mínima de pixels necessária para uma\n"
                                                                  "correspondência ser considerada um alvo real."},

        // ===== Target lock (sticky) =====
        {"lock_label",           "  Target lock (sticky aim — stops bouncing between targets):",
                                                                  "  Lock de alvo (mira fixa — evita oscilar entre alvos):"},
        {"sticky",               "Sticky target",                          "Alvo fixo"},
        {"sticky_tt",            "Once the aimbot picks a target it keeps aiming at it\n"
                                 "even when other matching blobs appear nearby. Prevents\n"
                                 "the high-power bounce you get with 2-3 same-color dots.",
                                                                  "Depois que o aimbot escolhe um alvo ele continua\n"
                                                                  "mirando nele mesmo que outros blobs iguais apareçam\n"
                                                                  "perto. Evita o 'bounce' com 2-3 bolinhas da mesma cor."},
        {"stick_radius",         "Stick radius (px)",                      "Raio de fixação (px)"},
        {"stick_radius_tt",      "How far (px) a fresh detection can be from the\n"
                                 "current lock and still be considered 'the same blob'.",
                                                                  "A que distância (px) uma detecção nova pode estar\n"
                                                                  "do lock atual e ainda ser 'a mesma bolinha'."},
        {"switch_threshold",     "Switch threshold",                       "Limite de troca"},
        {"switch_threshold_tt",  "How aggressively the aimbot will switch to a new target.\n"
                                 "0 = never switch (fully committed), 100 = switch on the\n"
                                 "first closer candidate. 50 = new must be ~2x closer.",
                                                                  "Quão agressivo o aimbot troca para um alvo novo.\n"
                                                                  "0 = nunca troca (totalmente fixo), 100 = troca no\n"
                                                                  "primeiro candidato mais perto. 50 = novo precisa ser ~2x mais perto."},
        {"max_miss",             "Max miss frames",                        "Máx. de quadros sem detecção"},
        {"max_miss_tt",          "Drop the target lock after this many frames without\n"
                                 "a successful detection (target gone / off-screen).",
                                                                  "Descarta o lock do alvo após esse tanto de quadros\n"
                                                                  "sem detecção (alvo sumiu / saiu da tela)."},

        // ===== Aim movement =====
        {"section_movement",     "Aim movement",                           "Movimento da mira"},
        {"aim_speed",            "Aim speed (px / frame)",                 "Velocidade da mira (px / quadro)"},
        {"aim_speed_tt",         "Maximum pixels moved per frame at long range.",
                                                                  "Máximo de pixels por quadro a longa distância."},
        {"aim_power",            "Aim power (%)",                          "Força da mira (%)"},
        {"aim_power_tt",         "Multiplier on the per-frame movement.\n"
                                 "150 = 1.5x, 200 = 2x. Higher = more aggressive.",
                                                                  "Multiplicador do movimento por quadro.\n"
                                                                  "150 = 1.5x, 200 = 2x. Maior = mais agressivo."},
        {"deadzone",             "Deadzone (px)",                          "Zona morta (px)"},
        {"deadzone_tt",          "If the target is within this many pixels of the\n"
                                 "crosshair, the mouse does not move at all (perfect stop).",
                                                                  "Se o alvo estiver a essa quantidade de pixels do\n"
                                                                  "crosshair, o mouse não se move (parada perfeita)."},
        {"snap_close",           "Snap close (no slow-down near target)",  "Travar ao chegar perto (sem desacelerar)"},
        {"snap_close_tt",        "When the target is close (~2x the speed cap),\n"
                                 "snap to it in a single frame instead of crawling.",
                                                                  "Quando o alvo está perto (~2x a velocidade máx.),\n"
                                                                  "vai direto nele em um quadro em vez de se arrastar."},
        {"input_method",         "Mouse input method",                     "Método de entrada do mouse"},
        {"method_mouse_event",   "mouse_event",                            "mouse_event"},
        {"method_sendinput",     "SendInput",                              "SendInput"},

        // ===== Auto-click =====
        {"section_autoclick",    "Auto-click",                             "Clique automático"},
        {"auto_click",           "Click when target is near the crosshair","Clicar quando o alvo está perto do crosshair"},
        {"auto_click_tt",        "Fires a left-click when the detected target is\n"
                                 "within Auto-click distance of the screen center.",
                                                                  "Dispara um clique esquerdo quando o alvo detectado\n"
                                                                  "está dentro da distância de auto-clique do centro."},
        {"auto_click_distance",  "Auto-click distance (px)",               "Distância do auto-clique (px)"},

        // ===== Status =====
        {"section_status",       "Status",                                 "Status"},
        {"status_fps",           "FPS                 %d",                  "FPS                 %d"},
        {"status_frames",        "Frames              %lld",               "Quadros             %lld"},
        {"status_clicks",        "Clicks fired        %lld",               "Cliques disparados  %lld"},
        {"status_errors",        "Capture errors      %lld",               "Erros de captura    %lld"},
        {"status_last_found",    "Last target         (%d, %d)  size=%d  dist=%d px",
                                                                  "Último alvo         (%d, %d)  tam=%d  dist=%d px"},
        {"status_last_none",     "Last target         (none)",             "Último alvo         (nenhum)"},
        {"status_center",        "Center match        %s",                 "Centro coincide     %s"},
        {"status_lock",          "Target lock         (%d, %d)",           "Lock de alvo        (%d, %d)"},
        {"status_lock_none",     "Target lock         (none)",             "Lock de alvo        (nenhum)"},
        {"status_yes",           "YES",                                    "SIM"},
        {"status_no",            "no",                                     "não"},

        // ===== Window picker modal =====
        {"picker_title",         "Select target window",                   "Escolher janela alvo"},
        {"picker_filter_hint",   "Filter by title or class...",            "Filtrar por título ou classe..."},
        {"picker_refresh",       "Refresh",                                "Atualizar"},
        {"picker_col_title",     "Title",                                  "Título"},
        {"picker_col_class",     "Class",                                  "Classe"},
        {"picker_col_pid",       "PID",                                    "PID"},
        {"picker_col_size",      "Size",                                   "Tamanho"},
        {"picker_minimized",     "  (minimized)",                          "  (minimizada)"},
        {"picker_dblclick",      "Double-click to select",                 "Clique duplo para selecionar"},
        {"picker_count",         "%zu window(s) visible",                  "%zu janela(s) visível(eis)"},
        {"picker_close",         "Close",                                  "Fechar"},
    };

    if (!key) return "";
    for (const auto& e : table) {
        if (std::strcmp(e.key, key) == 0) {
            return (g_currentLanguage == Language::Portuguese) ? e.pt : e.en;
        }
    }
    return key;
}

// Format helper: tr("key", ...) substitutes into the translated string.
// Usage: trf("status_fps", fps) — returns std::string.
// This is a small convenience: a full printf-style translator is out
// of scope; if you need a different format string per language, use
// the raw tr("key") result and sprintf it yourself.
#include <cstdio>
#include <string>
template<typename... Args>
inline std::string trf(const char* key, Args... args) {
    char buf[512];
    std::snprintf(buf, sizeof(buf), tr(key), args...);
    return std::string(buf);
}

}}  // namespace aimlab::i18n
