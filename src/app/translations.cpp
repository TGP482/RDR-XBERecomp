// Text the settings panel shows in French, German, Italian and Spanish, SCDA's wording where it has the same row.
#include <algorithm>
#include <cctype>

#include "app/game_settings.h"
#include "app/translations.h"

namespace {

// French, German, Italian, Spanish: the LANGUAGE setting's order after English
struct Entry {
    std::string_view english;
    std::u16string_view text[4];
};
constexpr Entry kEntries[] = {
    // pages and sections
    {"RECOMP SETTINGS", {u"Paramètres Recomp", u"Recomp-Einstellungen", u"Impostazioni Recomp", u"Configuración de Recomp"}},
    {"MOUSE AND KEYBOARD", {u"Souris et clavier", u"Maus und Tastatur", u"Mouse e tastiera", u"Ratón y teclado"}},
    {"CONTROLLER", {u"Manette", u"Controller", u"Controller", u"Controlador"}},
    {"GENERAL", {u"Général", u"Allgemein", u"Generale", u"General"}},
    {"DISPLAY", {u"Affichage", u"Anzeige", u"Video", u"Pantalla"}},
    {"GRAPHICS", {u"Graphismes", u"Grafik", u"Grafica", u"Gráficos"}},
    {"TEXTURES", {u"Textures", u"Texturen", u"Texture", u"Texturas"}},
    {"MOUSE", {u"Souris", u"Maus", u"Mouse", u"Ratón"}},
    {"KEYBOARD", {u"Clavier", u"Tastatur", u"Tastiera", u"Teclado"}},
    {"GYRO", {u"Gyroscope", u"Gyroskop", u"Giroscopio", u"Giroscopio"}},
    {"ON FOOT", {u"À pied", u"Zu Fuß", u"A piedi", u"A pie"}},
    {"WEAPONS", {u"Armes", u"Waffen", u"Armi", u"Armas"}},
    {"DUEL", {u"Duel", u"Duell", u"Duello", u"Duelo"}},

    // RECOMP SETTINGS rows
    {"MONITOR", {u"Écran", u"Monitor", u"Monitor", u"Monitor"}},
    {"DISPLAY MODE", {u"Mode d'affichage", u"Anzeigemodus", u"Modalità schermo", u"Modo de pantalla"}},
    {"RESOLUTION", {u"Résolution", u"Auflösung", u"Risoluzione", u"Resolución"}},
    {"RENDER RESOLUTION", {u"Résolution de rendu", u"Render-Auflösung", u"Risoluzione di rendering", u"Resolución de renderizado"}},
    {"FIELD OF VIEW", {u"Champ de vision", u"Sichtfeld", u"Campo visivo", u"Campo de visión"}},
    {"VERTICAL SYNC", {u"Synchro verticale", u"V-Sync", u"Sincronia verticale", u"Sincronismo vertical"}},
    {"HDR OUTPUT", {u"Sortie HDR", u"HDR-Ausgabe", u"Uscita HDR", u"Salida HDR"}},
    {"MAX FRAME RATE", {u"Images par seconde max.", u"Max. Bildrate", u"Frame rate massimo", u"Fotogramas por segundo máx."}},
    {"FPS COUNTER", {u"Compteur d'images par seconde", u"FPS-Anzeige", u"Contatore FPS", u"Contador de FPS"}},
    {"FMV SCALE", {u"Échelle des cinématiques", u"Videoskalierung", u"Scala dei filmati", u"Escala de vídeos"}},
    {"COLOUR BLIND MODE", {u"Mode daltonien", u"Farbenblindmodus", u"Modalità daltonici", u"Modo daltónico"}},
    {"CONTRAST", {u"Contraste", u"Kontrast", u"Contrasto", u"Contraste"}},
    {"AMBIENT OCCLUSION", {u"Occlusion ambiante", u"Ambient Occlusion", u"Occlusione ambientale", u"Oclusión ambiental"}},
    {"ANISOTROPIC FILTERING", {u"Filtrage anisotropique", u"Anisotropische Filterung", u"Filtro anisotropico", u"Filtro anisotrópico"}},
    {"SCREEN BLUR", {u"Flou de l'écran", u"Bildschirmunschärfe", u"Sfocatura dello schermo", u"Desenfoque de pantalla"}},
    {"SKIP INTROS", {u"Passer les intros", u"Intros überspringen", u"Salta introduzione", u"Saltar introducción"}},
    {"PAUSE ON FOCUS LOSS", {u"Pause en arrière-plan", u"Pause im Hintergrund", u"Pausa in background", u"Pausa en segundo plano"}},
    {"LANGUAGE", {u"Langue", u"Sprache", u"Lingua", u"Idioma"}},
    {"AUDIO OUTPUT", {u"Sortie audio", u"Audioausgabe", u"Uscita audio", u"Salida de audio"}},
    {"TEXTURE REPLACEMENT", {u"Remplacement des textures", u"Texturersetzung", u"Sostituzione delle texture", u"Reemplazo de texturas"}},
    {"TEXTURE DUMPING", {u"Extraction des textures", u"Texturextraktion", u"Estrazione delle texture", u"Extracción de texturas"}},
    {"DUMP FORMAT", {u"Format d'extraction", u"Extraktionsformat", u"Formato di estrazione", u"Formato de extracción"}},

    // their values
    {"FULLSCREEN", {u"Plein écran", u"Vollbild", u"Schermo intero", u"Pantalla completa"}},
    {"WINDOWED", {u"Fenêtré", u"Fenster", u"Finestra", u"Ventana"}},
    {"BORDERLESS", {u"Fenêtré sans bordure", u"Randloses Fenster", u"Finestra senza bordi", u"Ventana sin bordes"}},
    {"ON", {u"Oui", u"Ein", u"Sì", u"Sí"}},
    {"OFF", {u"Non", u"Aus", u"No", u"No"}},
    {"UNLIMITED", {u"Illimité", u"Unbegrenzt", u"Illimitato", u"Ilimitado"}},
    {"REFRESH RATE", {u"Fréquence de rafraîchissement", u"Bildwiederholfrequenz", u"Frequenza di aggiornamento", u"Frecuencia de actualización"}},
    {"AUTOMATIC", {u"Automatique", u"Automatisch", u"Automatico", u"Automático"}},
    {"NOT SUPPORTED", {u"Non pris en charge", u"Nicht unterstützt", u"Non supportato", u"No compatible"}},
    {"FILL", {u"Remplir", u"Füllen", u"Riempi", u"Rellenar"}},
    {"PROTANOPIA", {u"Protanopie", u"Protanopie", u"Protanopia", u"Protanopía"}},
    {"DEUTERANOPIA", {u"Deutéranopie", u"Deuteranopie", u"Deuteranopia", u"Deuteranopía"}},
    {"TRITANOPIA", {u"Tritanopie", u"Tritanopie", u"Tritanopia", u"Tritanopía"}},
    {"MONO", {u"Mono", u"Mono", u"Mono", u"Mono"}},
    {"2.0 STEREO", {u"2.0 Stéréo", u"2.0 Stereo", u"2.0 Stereo", u"2.0 Estéreo"}},
    {"5.1 SURROUND", {u"5.1 Surround", u"5.1 Surround", u"5.1 Surround", u"5.1 Envolvente"}},
    {"(DEFAULT)", {u"(par défaut)", u"(Standard)", u"(predefinito)", u"(por defecto)"}},
    {"(RESTART)", {u"(redémarrage)", u"(Neustart)", u"(riavvio)", u"(reinicio)"}},
    {"(PRIMARY)", {u"(principal)", u"(primär)", u"(principale)", u"(principal)"}},
    {"PRIMARY", {u"Principal", u"Primär", u"Principale", u"Principal"}},
    {"ENGLISH", {u"Anglais", u"Englisch", u"Inglese", u"Inglés"}},
    {"FRENCH", {u"Français", u"Französisch", u"Francese", u"Francés"}},
    {"GERMAN", {u"Allemand", u"Deutsch", u"Tedesco", u"Alemán"}},
    {"ITALIAN", {u"Italien", u"Italienisch", u"Italiano", u"Italiano"}},
    {"SPANISH", {u"Espagnol", u"Spanisch", u"Spagnolo", u"Español"}},

    // MOUSE AND KEYBOARD and CONTROLLER rows
    {"HORIZONTAL SENSITIVITY", {u"Sensibilité horizontale", u"Horizontale Empfindlichkeit", u"Sensibilità orizzontale", u"Sensibilidad horizontal"}},
    {"VERTICAL SENSITIVITY", {u"Sensibilité verticale", u"Vertikale Empfindlichkeit", u"Sensibilità verticale", u"Sensibilidad vertical"}},
    {"AIM HORIZONTAL SENSITIVITY", {u"Sensibilité horizontale en visée", u"Horizontale Zielempfindlichkeit", u"Sensibilità orizzontale in mira", u"Sensibilidad horizontal al apuntar"}},
    {"AIM VERTICAL SENSITIVITY", {u"Sensibilité verticale en visée", u"Vertikale Zielempfindlichkeit", u"Sensibilità verticale in mira", u"Sensibilidad vertical al apuntar"}},
    {"INVERT X-AXIS", {u"Inverser l'axe X", u"X-Achse umkehren", u"Inverti asse X", u"Invertir eje X"}},
    {"INVERT Y-AXIS", {u"Inverser l'axe Y", u"Y-Achse umkehren", u"Inverti asse Y", u"Invertir eje Y"}},
    {"AIM TOGGLE", {u"Visée en bascule", u"Zielen umschalten", u"Mira a interruttore", u"Apuntar alternado"}},
    {"WALK SPEED", {u"Vitesse de marche", u"Gehgeschwindigkeit", u"Velocità camminata", u"Velocidad al caminar"}},
    {"DEADZONE", {u"Zone morte", u"Totzone", u"Zona morta", u"Zona muerta"}},
    {"VIBRATION STRENGTH", {u"Intensité des vibrations", u"Vibrationsstärke", u"Intensità vibrazione", u"Intensidad de la vibración"}},
    {"GYRO AIMING", {u"Visée gyroscopique", u"Gyro-Zielen", u"Mira giroscopica", u"Apuntado con giroscopio"}},
    {"GYRO SENSITIVITY", {u"Sensibilité du gyroscope", u"Gyro-Empfindlichkeit", u"Sensibilità del giroscopio", u"Sensibilidad del giroscopio"}},
    {"GYRO AXIS", {u"Axe du gyroscope", u"Gyro-Achse", u"Asse del giroscopio", u"Eje del giroscopio"}},
    {"GYRO BUTTON", {u"Bouton du gyroscope", u"Gyro-Taste", u"Pulsante del giroscopio", u"Botón del giroscopio"}},
    {"GYRO BUTTON MODE", {u"Mode du bouton du gyroscope", u"Gyro-Tastenmodus", u"Modalità pulsante del giroscopio", u"Modo del botón del giroscopio"}},
    {"RECENTER BUTTON", {u"Bouton de recentrage", u"Zentrieren-Taste", u"Pulsante di ricentratura", u"Botón de recentrado"}},
    {"CALIBRATE GYRO", {u"Calibrer le gyroscope", u"Gyro kalibrieren", u"Calibra il giroscopio", u"Calibrar el giroscopio"}},
    {"YAW", {u"Lacet", u"Gieren", u"Imbardata", u"Guiñada"}},
    {"ROLL", {u"Roulis", u"Rollen", u"Rollio", u"Alabeo"}},
    {"WORLD", {u"Monde", u"Welt", u"Mondo", u"Mundo"}},
    {"NONE", {u"Aucun", u"Keine", u"Nessuno", u"Ninguno"}},
    {"TOUCHPAD", {u"Pavé tactile", u"Touchpad", u"Touchpad", u"Panel táctil"}},
    {"MISC", {u"Divers", u"Sonstige", u"Varie", u"Varios"}},
    {"RIGHT PADDLE 1", {u"Palette droite 1", u"Paddle rechts 1", u"Paletta destra 1", u"Palanca derecha 1"}},
    {"LEFT PADDLE 1", {u"Palette gauche 1", u"Paddle links 1", u"Paletta sinistra 1", u"Palanca izquierda 1"}},
    {"RIGHT PADDLE 2", {u"Palette droite 2", u"Paddle rechts 2", u"Paletta destra 2", u"Palanca derecha 2"}},
    {"LEFT PADDLE 2", {u"Palette gauche 2", u"Paddle links 2", u"Paletta sinistra 2", u"Palanca izquierda 2"}},
    {"OFF WHILE HELD", {u"Désactivé si maintenu", u"Aus beim Halten", u"Disattivato se tenuto premuto", u"Desactivado al mantener"}},
    {"ON WHILE HELD", {u"Activé si maintenu", u"Ein beim Halten", u"Attivato se tenuto premuto", u"Activado al mantener"}},
    {"TOGGLE", {u"Basculer", u"Umschalten", u"Alterna", u"Alternar"}},
    {"START", {u"Démarrer", u"Starten", u"Avvia", u"Iniciar"}},
    {"KEEP STILL", {u"Ne bougez pas", u"Nicht bewegen", u"Non muovere", u"No lo muevas"}},

    // key binds; Dead Eye keeps its name
    {"MOVE FORWARD", {u"Avancer", u"Vorwärts bewegen", u"Avanti", u"Avanzar"}},
    {"MOVE BACK", {u"Reculer", u"Rückwärts bewegen", u"Indietro", u"Retroceder"}},
    {"MOVE LEFT", {u"Déplacement à gauche", u"Nach links", u"Spostati a sinistra", u"Mover a la izquierda"}},
    {"MOVE RIGHT", {u"Déplacement à droite", u"Nach rechts", u"Spostati a destra", u"Mover a la derecha"}},
    {"WALK", {u"Marcher", u"Gehen", u"Cammina", u"Caminar"}},
    {"JUMP / DODGE", {u"Sauter / esquiver", u"Springen / Ausweichen", u"Salta / Schiva", u"Saltar / Esquivar"}},
    {"CROUCH", {u"S'accroupir", u"Ducken", u"Chinati", u"Agacharse"}},
    {"ACTION / SNEAK", {u"Action / se faufiler", u"Aktion / Schleichen", u"Azione / Furtività", u"Acción / Sigilo"}},
    {"MELEE", {u"Corps à corps", u"Nahkampf", u"Corpo a corpo", u"Cuerpo a cuerpo"}},
    {"FIRE", {u"Tirer", u"Feuern", u"Fuoco", u"Disparar"}},
    {"DRAW / TARGET", {u"Dégainer / viser", u"Ziehen / Zielen", u"Estrai / Mira", u"Desenfundar / Apuntar"}},
    {"RELOAD", {u"Recharger", u"Nachladen", u"Ricarica", u"Recargar"}},
    {"SPECIAL MOVE / ZOOM", {u"Mouvement spécial / zoom", u"Spezialbewegung / Zoom", u"Mossa speciale / Zoom", u"Movimiento especial / Zoom"}},
    {"CYCLE WEAPONS", {u"Changer d'arme", u"Waffe wechseln", u"Cambia arma", u"Cambiar de arma"}},
    {"NEXT WEAPON", {u"Arme suivante", u"Nächste Waffe", u"Arma successiva", u"Arma siguiente"}},
    {"PREVIOUS WEAPON", {u"Arme précédente", u"Vorherige Waffe", u"Arma precedente", u"Arma anterior"}},
    {"GRAB", {u"Saisir", u"Greifen", u"Afferra", u"Agarrar"}},
    {"DRAW", {u"Dégainer", u"Ziehen", u"Estrai", u"Desenfundar"}},
    {"PAUSE", {u"Pause", u"Pause", u"Pausa", u"Pausa"}},
    {"BACK", {u"Retour", u"Zurück", u"Indietro", u"Atrás"}},
    {"TOGGLE HUD", {u"Afficher / masquer l'interface", u"HUD ein / aus", u"Mostra / nascondi HUD", u"Mostrar / ocultar HUD"}},
    {"TOGGLE TEXTURE REPLACEMENT", {u"Activer / désactiver le remplacement des textures", u"Texturersetzung ein / aus", u"Attiva / disattiva sostituzione delle texture", u"Activar / desactivar reemplazo de texturas"}},
    {"SETTINGS PANEL", {u"Panneau des paramètres", u"Einstellungsmenü", u"Pannello impostazioni", u"Panel de configuración"}},
    {"UNBOUND", {u"Non assigné", u"Nicht belegt", u"Non assegnato", u"Sin asignar"}},
    {"PRESS A KEY", {u"Appuyez sur une touche", u"Taste drücken", u"Premi un tasto", u"Pulsa una tecla"}},

    // the panel's own
    {"SETTINGS", {u"Paramètres", u"Einstellungen", u"Impostazioni", u"Configuración"}},
    {"DEFAULTS", {u"Par défaut", u"Standard", u"Predefiniti", u"Por defecto"}},
    {"APPLY", {u"Appliquer", u"Übernehmen", u"Applica", u"Aplicar"}},
    {"Press a key or mouse button to bind, Escape cancels", {u"Appuyez sur une touche ou un bouton de souris, Échap pour annuler", u"Taste oder Maustaste drücken, Escape bricht ab", u"Premi un tasto o un pulsante del mouse, Esc per annullare", u"Pulsa una tecla o un botón del ratón, Escape para cancelar"}},
    {"Right click or Delete clears the key", {u"Clic droit ou Suppr pour effacer la touche", u"Rechtsklick oder Entf löscht die Taste", u"Clic destro o Canc per cancellare il tasto", u"Clic derecho o Supr para borrar la tecla"}},
};

bool Same(std::string_view a, std::string_view b) {
    return a.size() == b.size() && std::equal(a.begin(), a.end(), b.begin(), [](char x, char y) {
               return std::toupper(uint8_t(x)) == std::toupper(uint8_t(y));
           });
}

bool WordChar(char c) { return std::isalnum(uint8_t(c)) != 0; }

}  // namespace

const char16_t* app::Translation(std::string_view english) {
    constexpr uint32_t kFrench = 2;
    uint32_t language = BootLanguage();
    if (language < kFrench) return nullptr;
    for (const Entry& e : kEntries) {
        if (Same(e.english, english)) return e.text[language - kFrench].data();
    }
    return nullptr;
}

std::u16string app::Translate(std::string_view text) {
    std::u16string out;
    size_t plain = 0;  // start of the text not yet out
    for (size_t i = 0; i < text.size();) {
        const char16_t* found = nullptr;
        size_t end = text.size();
        bool word_start = !i || !WordChar(text[i - 1]);
        for (; word_start && end > i; end--) {
            bool word_end = end == text.size() || !WordChar(text[end]);
            if (word_end && (found = Translation(text.substr(i, end - i)))) break;
        }
        if (!found) {
            i++;
            continue;
        }
        out.append(text.begin() + plain, text.begin() + i);
        out += found;
        i = plain = end;
    }
    out.append(text.begin() + plain, text.end());
    return out;
}
