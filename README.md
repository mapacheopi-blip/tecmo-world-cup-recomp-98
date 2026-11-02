# TECMO WORLD CUP RECOMP 98

Port nativo experimental de **TECMO World Cup '98 / World Soccer '98** para
Windows x64, basado en recompilación estática y servicios de plataforma nativos.
El juego ejecuta código C++ compilado para PC; el launcher no abre MAME ni un
intérprete de CPU.

**La descarga no contiene la ROM del juego. Cada persona debe aportar su copia
compatible de `twcup98.zip`.** Los archivos preparados a partir de esa ROM se
generan únicamente en su equipo. La BIOS y el estado residente de firmware se
incluyen por separado y conservan sus derechos originales: [avisos](NOTICE.md).

## Descargar y jugar

1. Descarga el ZIP de Windows desde [Releases](https://github.com/mapacheopi-blip/tecmo-world-cup-recomp-98/releases).
2. Extrae **toda** la carpeta en una ubicación donde puedas guardar archivos.
3. Abre `TecmoWorldCupRecomp98.exe`.
4. Pulsa **Seleccionar ROM** y elige tu `twcup98.zip`.
5. Tras verificar los archivos, el mismo botón cambia a **Start**.

No necesitas compilar ni instalar Ghidra, MAME o el SDK de .NET para jugar con
el paquete de Windows. No copies una ROM al repositorio ni descargues el ZIP
de código fuente esperando encontrar un ejecutable: usa el paquete de Releases.

## Requisitos

- Windows x64 compatible con .NET 10; el paquete lleva su runtime.
- GPU/controlador compatible con Direct3D 11 y presentación DXGI flip de Windows 10+.
- Tu copia de la revisión **JUET 980410 V1.000**, verificada por contenido.

Se ha probado localmente con una RTX 5060 Ti y DualSense. Eso no certifica todos
los equipos y mandos. No se publica un mínimo de CPU inventado.

## Funciones

- Menú principal: Seleccionar ROM / Start, Controles, Opciones y Salir.
- Nítido, Suave y **Pixelart 2x** (Scale2x en GPU), DirectX 11 y VSync.
- Ventana o pantalla completa sin bordes; proporción original 4:3 o estirada.
- SDL3 y alternativa DirectInput, reasignación de botones/teclado, zona muerta,
  perfiles por dispositivo y monitor de entrada.
- Audio de 0 a 100 % y mute, independiente del volumen de Windows.
- Juego/audio separados de la ventana y de la lectura del archivo de controles.
- Preferencias y ROM preparada locales en `DataNative/`; registros por sesión.

La velocidad y frecuencia original del juego se conservan (~59,8 imágenes/s).
Pixelart 2x modifica el escalado, no crea geometría HD ni aumenta la resolución
interna. A 144 Hz fijos pueden alternarse dos y tres refrescos por imagen. No
hay VRR, interpolación, RTX Remix ni sistema de mods implementados.

## Controles iniciales

| Acción | Teclado J1 | DualSense predeterminado |
|---|---|---|
| Movimiento | Flechas | Cruceta / stick izquierdo |
| Botones A/B/C/D | Z / X / C / V | × / ○ / □ / △ |
| Inicio | 1 | Options |
| Crédito | 5 | Create / Share |
| Volver al launcher | Esc | Cerrar ventana del juego |

Las asignaciones se pueden cambiar. La caducidad de entrada de 250 ms y la
pérdida de foco sueltan contactos; el segundo plano es configurable. Hay
asignaciones hasta J4, sujetas a las conexiones originales de la placa. No se
certifica aquí una partida multijugador completa con todos los dispositivos.

## ROM compatible

Los nombres antiguos dentro del ZIP también sirven; se comprueban tamaño y SHA-1.

| Chip | Bytes | SHA-1 |
|---|---:|---|
| epr20819.24 | 1048576 | f66cc955181720661a0334fe67fa5750ddf9758b |
| mpr20821.12 | 4194304 | 5fcaf4257f3639cb3aa407d2936f616499a09d97 |
| mpr20822.13 | 4194304 | d5689ac8aad63509febe9aa4077351be09b2d8d4 |
| mpr20823.14 | 4194304 | c387d03ba27580c62ac0bf780915fdf41552df6f |
| mpr20824.15 | 4194304 | 310961a5f114fea8938a3f514dffd5231e910a5a |

El ZIP original se lee, no se modifica. Después de importar, los datos locales
preparados permiten jugar sin conservar el ZIP en la misma ruta. Esos datos
siguen siendo datos del juego: tampoco se publican en GitHub.

## Estado y límites

La versión nativa jugable fue aceptada por el mantenedor tras pruebas reales.
Las verificaciones sintéticas cubren píxeles, transporte de entrada, audio y
pausas de presentación; no demuestran fidelidad completa del hardware original,
todos los efectos o un torneo completo. El arranque usa firmware residente
preparado y no representa todavía un encendido completo de BIOS.

Direct3D 11 es la ruta normal; GDI se conserva como recuperación. Una pausa que
afecte al sistema o al controlador de audio aún puede interrumpir el sonido.
Reporta errores sin adjuntar ROM, cartuchos preparados o volcados de RAM.

## Desarrollo

Lee [BUILDING.md](BUILDING.md) para compilar desde tu ROM. Las traducciones C++
generadas y el IR se reconstruyen localmente y están excluidos de Git. La CI
prueba las partes que no necesitan ROM; no finge ejecutar una partida sin ella.

## Créditos y licencia

Código propio bajo **MIT**. Componentes externos, firmware y arte original
conservan sus derechos/licencias: [NOTICE.md](NOTICE.md).

- SDL3, .NET y Vortice.DirectInput para entrada e interfaz de Windows.
- Referencias SCSP/315-5881 de MAME, con avisos BSD conservados.
- Ghidra para análisis y generación de traducciones durante el desarrollo.
- [REA](https://github.com/morluto/rea) como herramienta de investigación.
- [Snowboard Kids: Recompiled](https://github.com/cdlewis/snowboardkids-recomp)
  como referencia de presentación y flujo de selección de ROM. No somos un
  fork ni usamos N64Recomp/RT64 para ejecutar este juego de Sega ST-V.

Proyecto no oficial, sin afiliación con los titulares de TECMO o Sega.
