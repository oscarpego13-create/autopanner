# AutoPanner VST3/AU Plugin

Plugin de auto-paneo con LFO, ruido blanco y spikes. Diseño idéntico al mockup HTML.

## Cómo compilar (macOS — sin instalar nada localmente)

1. **Crea un repositorio en GitHub** (puede ser privado o público).

2. **Sube todos los archivos de esta carpeta** al repo:
   ```
   git init
   git add .
   git commit -m "init"
   git remote add origin https://github.com/TU_USUARIO/TU_REPO.git
   git push -u origin main
   ```

3. **GitHub Actions se lanza automáticamente** al hacer push.  
   Ve a `Actions` → workflow "Build AutoPanner (macOS)" → espera ~10 min.

4. **Descarga los artefactos** desde la pestaña Actions al terminar:
   - `AutoPanner-VST3-macOS.zip` → descomprime → instala `AutoPanner.vst3` en `/Library/Audio/Plug-Ins/VST3/`
   - `AutoPanner-AU-macOS.zip` → descomprime → instala `AutoPanner.component` en `/Library/Audio/Plug-Ins/Components/`

5. Abre tu DAW y escanea los plugins.

## Parámetros

| Parámetro | Rango | Descripción |
|-----------|-------|-------------|
| Rate | 0–1 (log → 0.1–10 Hz) | Frecuencia del LFO |
| Depth | 0–100 % | Amplitud del barrido de paneo |
| Shape | Sine / Tri / Square | Forma de onda del LFO |
| Phase | –180°…+180° | Offset de fase (arrastra el display) |
| Noise Cantidad | 0–100 % | Amplitud del ruido blanco |
| Noise Velocidad | 0–100 % | Velocidad de variación del ruido |
| Spike Cantidad | 0–100 % | Amplitud de los spikes |
| Spike Densidad | 0–100 % | Probabilidad de aparición de spikes |
| Sync | Off/On | Sincroniza Rate al tempo del host (BPM) |

## Requisitos del host

- macOS 11+ (Apple Silicon o Intel)
- DAW compatible con VST3 o AU (Ableton, Logic, Studio One, Reaper, etc.)
- Stereo in → Stereo out
