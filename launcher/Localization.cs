using System.Globalization;

namespace Tecmo98;

// These keys describe presentation only. Persisted graphics/controller values,
// device identities and the native launch protocol never depend on the language.
public static class L {
 public static string Language { get; set; } = "en";
 public static string Normalize(string? language) => language == "es" ? "es" : "en";
 public static readonly IReadOnlyDictionary<string,string> English = new Dictionary<string,string> {
  ["Idioma"]="Language",
  ["Windows nativo · {0}"]="Native Windows · {0}",
  ["Volver al menú principal"]="Back to main menu",
  ["Volver (Esc)"]="Back (Esc)",
  ["Tab / flechas: navegar     Enter: aceptar     Esc: volver"]="Tab / arrows: navigate     Enter: accept     Esc: back",
  ["General"]="General", ["Controles"]="Controls", ["Gráficos"]="Graphics", ["Sonido"]="Sound",
  ["Seleccionar ROM"]="Select ROM", ["Opciones"]="Settings", ["Salir"]="Exit",
  ["Partida en curso"]="Game running", ["Cerrar partida"]="Close game",
  ["Juego preparado. Elige Start."]="Game ready. Select Start.",
  ["Selecciona tu ROM para comenzar."]="Select your ROM to get started.",
  ["Cierra la partida antes de cambiar la ROM."]="Close the game before changing the ROM.",
  ["Seleccionar tu ROM de TECMO World Cup 98"]="Select your TECMO World Cup 98 ROM",
  ["ROM del juego (ZIP)|*.zip"]="Game ROM (ZIP)|*.zip",
  ["Verificando y preparando la ROM…"]="Verifying and preparing the ROM…",
  ["ROM lista. Elige Start para jugar."]="ROM ready. Select Start to play.",
  ["Preferencias"]="Preferences",
  ["Permitir mandos en segundo plano"]="Allow controller input in the background",
  ["Si lo desactivas, el juego solo recibe entradas cuando su ventana está activa."]="When disabled, the game only receives input while its window is active.",
  ["Los ajustes se guardan automáticamente. Imagen y sonido se aplican a la siguiente partida."]="Settings save automatically. Graphics and sound changes apply the next time you start the game.",
  ["Modo de pantalla"]="Window mode", ["Ventana"]="Windowed", ["Pantalla completa"]="Fullscreen",
  ["Tamaño de ventana"]="Window size", ["Proporción"]="Aspect ratio", ["Estirar"]="Stretch",
  ["Tratamiento de imagen"]="Image filter", ["Nítido"]="Sharp", ["Suave"]="Smooth",
  ["Nítido conserva los píxeles. Suave suaviza el escalado. Pixelart 2x suaviza bordes de píxeles antes de ampliar; puede cambiar detalles pequeños."]="Sharp preserves pixels. Smooth softens scaling. Pixelart 2x smooths pixel edges before upscaling; it may alter small details.",
  ["Original mantiene 4:3. Pantalla completa usa el tamaño del monitor. Se conserva la resolución interna y la velocidad del juego."]="Original keeps the 4:3 aspect ratio. Fullscreen uses your display size. Internal resolution and game speed stay the same.",
  ["Volumen general"]="Master volume",
  ["Volumen general, de 0 a 100 por ciento"]="Master volume, 0 to 100 percent",
  ["Silenciar (mute)"]="Mute",
  ["0 % es silencio. Mute conserva tu nivel para recuperarlo al desactivarlo. Se aplica en la siguiente partida."]="0% is silent. Mute keeps your volume level so you can restore it by unmuting. Changes apply the next time you start the game.",
  ["Salida predeterminada de Windows. Este ajuste solo cambia el volumen del juego."]="Uses the default Windows audio output. This setting only changes the game volume.",
  ["Solo teclado"]="Keyboard only", ["Mando guardado · desconectado"]="Saved controller · disconnected",
  ["Jugador 1"]="Player 1", ["Jugador 2"]="Player 2", ["Jugador 3"]="Player 3", ["Jugador 4"]="Player 4",
  ["Ese mando está asignado a otro jugador."]="That controller is assigned to another player.",
  ["Restablecer"]="Reset to defaults", ["Mando"]="Controller", ["Teclado"]="Keyboard",
  ["Elige una acción y pulsa el botón o tecla que quieras usar. Esc cancela."]="Choose an action, then press the button or key you want to use. Esc cancels.",
  ["Arriba"]="Up", ["Abajo"]="Down", ["Izquierda"]="Left", ["Derecha"]="Right",
  ["Botón A"]="Button A", ["Botón B"]="Button B", ["Botón C"]="Button C", ["Botón D"]="Button D", ["Crédito"]="Coin",
  ["Botón {0}"]="Button {0}", ["Eje {0} {1}"]="Axis {0} {1}", ["Cruceta {0} · {1}"]="D-pad {0} · {1}",
  ["Sin asignar"]="Unassigned", ["Asignar"]="Bind",
  ["Quitar esta asignación"]="Clear this binding", ["Quitar asignación de {0}"]="Clear binding for {0}",
  ["{0} · cambiar asignación"]="{0} · change binding", ["{0}, cambiar asignación"]="{0}, change binding",
  ["Prueba tu teclado"]="Test your keyboard", ["Prueba tu mando"]="Test your controller",
  ["Pulsa una tecla para ver su acción."]="Press a key to see its action.",
  ["Conecta un mando y selecciónalo arriba."]="Connect a controller and select it above.",
  ["Lectura del mando"]="Controller backend", ["Direcciones"]="Movement input", ["Asignadas"]="Bindings only",
  ["Izquierdo"]="Left stick", ["Derecho"]="Right stick", ["Símbolos"]="Button icons", ["Generic"]="Generic",
  ["Zona muerta · {0:P0}"]="Deadzone · {0:P0}", ["Guardar diagnóstico"]="Save diagnostics",
  ["Opciones avanzadas y diagnóstico"]="Advanced options and diagnostics",
  ["J3/J4: tres botones y Start. Los créditos se insertan desde J1/J2."]="P3/P4: three buttons and Start. Insert coins using P1/P2.",
  ["Pulsa una tecla para {0}. Esc cancela."]="Press a key for {0}. Esc cancels.",
  ["Suelta el mando y pulsa una entrada para {0}. Esc cancela."]="Release the controller, then press an input for {0}. Esc cancels.",
  ["{0} asignado al jugador 1. Prueba sus entradas en Controles."]="{0} assigned to player 1. Test its inputs in Controls.",
  ["Error de lectura de mandos"]="Controller input error", ["Sin mandos detectados"]="No controllers detected",
  ["1 mando conectado"]="1 controller connected", ["{0} mandos conectados"]="{0} controllers connected",
  ["Cambió el orden de dos mandos iguales. Vuelve a asignarlos a cada jugador."]="Two identical controllers changed order. Assign them to their players again.",
  ["Partida cerrada. Lista para jugar otra vez."]="Game closed. Ready to play again.",
  ["El juego terminó. Revisa el registro de la última sesión si hubo un problema."]="The game exited. Check the latest session log if something went wrong.",
  ["Sin señal. Conecta el mando o selecciona uno de la lista."]="No input. Connect a controller or select one from the list.",
  ["normalizado"]="standard mapping", ["asignación manual"]="manual mapping",
  ["{0} · {1}\nUSB {2}:{3} · {4} botones · {5} ejes · {6} HAT\nBotones: 0x{7:X}  HAT: {8}\nEjes: {9}"]="{0} · {1}\nUSB {2}:{3} · {4} buttons · {5} axes · {6} HAT\nButtons: 0x{7:X}  HAT: {8}\nAxes: {9}",
  ["DirectInput compartido"]="Shared DirectInput", [" · unidad {0}"]=" · unit {0}",
  ["Se agotó la espera. Vuelve a elegir una acción."]="Timed out. Choose an action again.",
  ["Mando desconectado. No se guardó la asignación."]="Controller disconnected. The binding was not saved.",
  ["Ahora pulsa el botón o dirección."]="Now press the button or direction.",
  ["Esa entrada ya está asignada. Suelta el mando y elige otra."]="That input is already bound. Release the controller and choose another.",
  ["Asignación guardada."]="Binding saved.", ["Asignación cancelada."]="Binding cancelled.",
  ["Elige otra tecla."]="Choose another key.", ["Esa tecla ya tiene una acción. Elige otra."]="That key already has an action. Choose another.",
  ["Tecla guardada."]="Key saved.", ["Diagnóstico guardado en este equipo."]="Diagnostics saved on this computer.",
  ["Partida abierta. Esc en el juego vuelve al menú."]="Game started. Press Esc in the game to return to the menu.",
  ["Entrada: "]="Input: ", ["No se puede iniciar: "]="Cannot start: ", ["Falta preparar: "]="Missing required files: ",
  ["Ya hay una partida abierta."]="A game is already running.",
  ["Selecciona un archivo que exista."]="Select an existing file.",
  ["El ZIP contiene demasiados archivos para este juego."]="The ZIP contains too many files for this game.",
  ["El archivo no tiene el tamaño esperado."]="The file does not have the expected size.",
  ["No coincide con la revisión compatible. Faltan o difieren: "]="Unsupported ROM revision. Missing or different files: ",
  ["El cartucho preparado no coincide con la revisión compatible."]="The prepared cartridge does not match the supported revision.",
  ["ROM del juego"]="Game ROM", ["BIOS en Documentos"]="BIOS in Documentos", ["Perfil de arranque en Documentos"]="Boot profile in Documentos", ["Ejecutable nativo"]="Native executable",
  ["Falta el lector DirectInput. Usa SDL o recompila el lector."]="The DirectInput helper is missing. Use SDL or rebuild the helper.",
  ["No se pudo cargar SDL3"]="Could not load SDL3",
  ["No se pudo crear la ventana de adquisición."]="Could not create the controller acquisition window.",
  ["No se pudieron leer los ajustes. Se conservó una copia y se restauraron los valores iniciales."]="Settings could not be read. A backup was kept and defaults were restored."
 };
 static readonly Dictionary<string,string> Spanish = English.GroupBy(p=>p.Value).ToDictionary(g=>g.Key,g=>g.First().Key);
 public static string T(string key) {
  bool spanish=Language=="es";
  if((spanish?Spanish:English).TryGetValue(key,out var value)) return value;
  // Preserve variable details (file names, OS/driver errors) after message prefixes.
  foreach(var pair in English.Where(p=>p.Key.EndsWith(": ")))
   foreach(var prefix in new[]{pair.Key,pair.Value})
    if(key.StartsWith(prefix,StringComparison.Ordinal))return (spanish?pair.Key:pair.Value)+string.Join(", ",key[prefix.Length..].Split(", ").Select(T));
  return key;
 }
 public static string F(string key,params object[] args) => args.Length==0?T(key):string.Format(CultureInfo.GetCultureInfo(Language),T(key),args);
}
