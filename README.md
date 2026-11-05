# MSC VOXITY 2.0 — Vocal processor VST3 (Windows)

Plugin VST3 de voz para FL Studio y cualquier DAW de Windows, hecho con **JUCE 8 + CMake**.
Interfaz basada en tu arte de referencia (`docs/reference/MSC_VOX_2_0_reference.png`).

Se instala como **"MSC VOXITY 2.0.vst3"**, así que **no sobrescribe** un "MSC VOX.vst3" anterior.

## Cómo compilarlo (sin instalar nada)

1. Crea un repositorio **nuevo** en GitHub (vacío).
2. Sube **el contenido** de esta carpeta (no la carpeta en sí) a la raíz del repo. Debe quedar `CMakeLists.txt`, `Source/`, `Resources/`, `.github/` ... en la raíz.
   - Web: *Add file → Upload files* y arrastra todo. Asegúrate de que se sube la carpeta oculta `.github`.
   - O con git: `git init && git add . && git commit -m "MSC VOXITY 2.0" && git branch -M main && git remote add origin <url> && git push -u origin main`
3. Abre la pestaña **Actions** del repo. El workflow **Build Windows VST3** arranca solo (también puedes lanzarlo con *Run workflow*). Tarda unos 10–20 min la primera vez (descarga JUCE y compila).
4. Cuando termine en verde, entra a la ejecución y baja **MSC_VOXITY_2.0_VST3_Windows** desde *Artifacts*.
5. Descomprime y copia la carpeta **`MSC VOXITY 2.0.vst3`** a `C:\Program Files\Common Files\VST3\`.
6. En FL Studio: *Options → Manage plugins → Find installed plugins* (Verify plugins) y búscalo como efecto.

Si el workflow falla, copia el error del paso rojo y pásamelo para corregirlo.

## Qué incluye

| Módulo | Implementación |
|---|---|
| **Auto-Tune** | Detección de tono YIN + cuantización a escala (8 escalas, 12 notas raíz) + desplazador PSOLA estéreo. Modos Robotic / Natural / Smooth / Hard, Amount y Retune Speed |
| Compressor | Feed-forward, soft knee, enlazado estéreo, makeup |
| De-Esser | Banda lateral band-pass con frecuencia, umbral, reducción y amount |
| EQ | Low shelf 120 Hz, peak 1.5 kHz, high shelf 8 kHz + curva en vivo |
| Air | High shelf 11 kHz + excitador armónico |
| Distortion | Drive/Tone/Mix con sobremuestreo 2x |
| Saturation | Tube / Tape / Transistor, 2x, bloqueo de DC |
| Chorus | 2 voces por canal, Rate/Depth/Width/Mix |
| Delay | Sync al tempo del DAW (1/1–1/32, punteado, tresillo), feedback, tono, ping-pong |
| Reverb | Freeverb con RT60, Hall/Room/Plate/Chamber, damping, pre-delay |
| Stereo Widener | Mid/Side |
| Master | Input/Output gain, Wet/Dry (con compensación de latencia), medidores IN/OUT |

* **8 presets** de fábrica (`Resources/Presets/*.json`), también accesibles como programas del DAW. SAVE/LOAD guardan presets `.mscvox`.
* **6 temas** de interfaz (`Resources/Themes/*.json`): Purple, Blue, Green, Pink, Gold, Cyan.
* **Animación de personajes**: cantante y guitarrista (recortes con borde suave extraídos de tu imagen) se mueven con el nivel de salida; estrellas que parpadean.
* **Latencia**: ~7 ms fija (reportada al host). El plugin está pensado para correcciones de hasta ±3 semitonos, que es lo que produce la cuantización a escala.
* En el panel Auto-Tune, **SCALE** elige el tipo de escala y **NOTE** la nota raíz (C + Minor = Do menor).

## Estructura

```
CMakeLists.txt            proyecto JUCE/CMake (descarga JUCE 8.0.6 automáticamente)
.github/workflows/        build-windows-vst3.yml  -> compila y empaqueta el VST3
Source/                   plugin (procesador, editor, parámetros, presets, temas)
Source/DSP/               DSP en C++ puro (autotune, compresor, reverb, ...)
Resources/Images|Presets|Themes   arte, presets y temas incrustados en el binario
scripts/                  generate_assets.py, generate_data.py, verify_project.py, build_windows.bat
tests/dsp_test.cpp        pruebas del DSP sin JUCE
docs/reference/           imagen de referencia
```

## Pruebas del DSP en local (opcional)

```
g++ -std=c++17 -O2 -ISource/DSP tests/dsp_test.cpp -o dsp_test && ./dsp_test
```

## Regenerar arte / presets (opcional)

```
pip install pillow numpy
python scripts/generate_assets.py
python scripts/generate_data.py
```
