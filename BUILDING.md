# Compilar desde el código fuente

Para **jugar**, usa el ZIP de Releases y tu ROM; estas herramientas son solo
para desarrollar/compilar. La ROM, el cartucho, los IR y el C++ generado nunca
se añaden al repositorio.

## Herramientas comprobadas

- Windows x64, PowerShell, Python 3.
- .NET SDK 10.
- LLVM-MinGW con `clang++`, CMake y Ninja en PATH.
- Ghidra **12.1.4** y un JDK compatible (el entorno de desarrollo usa JDK 25).
  La especificación y el generador tienen reparaciones explícitas de SH-2 y
  68000; actualizar Ghidra requiere volver a comprobar las traducciones.
- Tu copia compatible del juego.

```powershell
$env:GHIDRA_INSTALL_DIR = 'C:\Tools\ghidra_12.1.4_PUBLIC'
.\tools\build.ps1 -Rom 'C:\MisJuegos\twcup98.zip' -Jobs 4
```

El flujo verifica cinco chips por hash, reconstruye `work/cart.bin`, extrae el
driver de sonido del propio cartucho, exporta el IR con Ghidra y genera C++
estático. A continuación compila `tecmo98-native-launcher-player` y publica
el launcher autónomo y sus dependencias en `dist/windows`.

El paquete final contiene la BIOS y **solo 16 KiB** de firmware residente,
verificados por hash. No se copia el antiguo volcado completo de 1 MiB, ni la
ROM, ni `work/`, ni `DataNative/`. La importación se prueba en una copia aparte
para mantener limpio el paquete distribuido.

Se puede repetir solo el empaquetado después de compilar:

```powershell
.\tools\package.ps1
```

El empaquetador se detiene si el destino contiene `DataNative`, evitando
convertir accidentalmente una instalación usada en un paquete público.

## Comprobaciones sin ROM

```powershell
cmake -S native-port -B native-port/build-check
cmake --build native-port/build-check --config Release --target tecmo98-launcher-input-check tecmo98-contact-mailbox-check tecmo98-presentation-clock-check
dotnet build launcher/Tecmo98.csproj -c Release
python tools/audit_publication.py
```

Estas comprobaciones no ejecutan el juego. Los tests de fidelidad con datos
originales necesitan tus archivos locales. Las carpetas de compilación y
verificación están ignoradas; no se usan archivos secretos en CI pública.
