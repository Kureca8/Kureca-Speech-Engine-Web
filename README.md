# Kureca Speech Engine — Web

**Lightweight, formant-based text-to-speech engine capable of running in the browser.**

Kureca Speech Engine Web (KSE) is a fully client-side speech synthesis engine built in C and compiled to WebAssembly.  
Without any external APIs, AI, or servers, KSE provides **real-time voice synthesis** directly in your browser.

---

## Key Features

- **Real-time speech synthesis**
- **Formant-based phoneme modeling**
- **High performance** thanks to WebAssembly
- **Custom phoneme processing**
- **Fully offline**, works in browser only
- **Animated mouth states** for fun visual feedback
- **Open-source**, MIT license
- **Real time audio resampling** for easier tuning
- **Few different voice settings presets**
- **Cool retro Windows 95 stylized interface**
- **Audio looping**
- **Synthesis history and Repeat**
- **Saving to wav**

---

## Live Demo

Check the working version:  
https://kureca8.github.io/Kureca-Speech-Engine-Web/

---

## Usage

1. You will see a text input box and a Speak button.
2. Type any text you want to hear in the input box.
3. Click Speak.
4. The engine will synthesize your text in real time.
5. The mouth animation will show closed or talking state while playing.

Enjoy your synthesized voice directly in the browser.  
KSE works fully offline in-browser. After loading, it does not require internet to synthesize speech.

---

## How It Works

KSE uses **classical formant-based synthesis**:

```
Text input → Phoneme Parser → Formant Synthesizer → Audio output
```

---

## Compiling

Requires **Emscripten**

```sh
emcc tts-web.c -O3 \
  -s WASM=1 \
  -s MODULARIZE=1 \
  -s EXPORT_NAME="TTSModule" \
  -s EXPORT_ES6=0 \
  -s ENVIRONMENT="web" \
  -s INITIAL_MEMORY=67108864 \
  -s ALLOW_MEMORY_GROWTH=1 \
  -s EXPORTED_FUNCTIONS="['_tts_sample_rate','_tts_speak','_tts_get_buf','_malloc','_free']" \
  -s EXPORTED_RUNTIME_METHODS="['cwrap','HEAPF32']" \
  -o tts.js
```

---

## Licence

MIT License - fully open source.
