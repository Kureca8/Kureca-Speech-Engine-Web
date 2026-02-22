// tts wrapper core
var TTSWrapper = {
    Module:       null,
    sampleRate:   16000,
    _ctx:         null,
    _node:        null,
    _rawBuf:      null,
    _playing:     false,
    _destination: null, // external gain node

    init: async function(TTSModuleFactory) {
        // initialize module and get sample rate
        const mod = await TTSModuleFactory();
        this.Module     = mod;
        this.sampleRate = mod._tts_sample_rate();
        console.log('TTS ready, sample rate:', this.sampleRate);
    },

    _allocString: function(str) {
        // allocate and copy string to wasm heap
        const mod     = this.Module;
        const encoded = new TextEncoder().encode(str);
        const ptr     = mod._malloc(encoded.length + 1);
        if (!ptr) return 0;
        const heap = new Uint8Array(mod.HEAPF32.buffer);
        heap.set(encoded, ptr);
        heap[ptr + encoded.length] = 0;
        return ptr;
    },

    _ensureCtx: async function() {
        // create audio context and register worklet
        if (this._ctx) {
            if (this._ctx.state === 'suspended') await this._ctx.resume();
            return;
        }
        this._ctx = new AudioContext({ sampleRate: this.sampleRate });
        await this._ctx.audioWorklet.addModule('tts-processor.js');
    },

    _synthesize: function(txt) {
        // call wasm synth and return float32 buffer copy
        const mod = this.Module;
        const sp  = this._allocString(txt);
        if (!sp) return null;
        const got = mod._tts_speak(sp);
        mod._free(sp);
        if (got <= 0) return null;
        const ptr = mod._tts_get_buf();
        return new Float32Array(mod.HEAPF32.buffer, ptr, got).slice();
    },

    _startPlayback: function(raw, pitchHz, speed, onEnd) {
        // start playback using audioworklet node
        if (this._node) {
            this._node.port.postMessage({ type: 'stop' });
            this._node.disconnect();
            this._node = null;
        }

        this._playing = true;
        const node = new AudioWorkletNode(this._ctx, 'tts-processor');
        this._node = node;

        const dest = this._destination || this._ctx.destination;
        node.connect(dest);

        const copy = raw.slice();
        node.port.postMessage({
            type:  'load',
            buf:   copy,
            pitch: pitchHz / 105.0,
            speed: speed
        }, [copy.buffer]);

        node.port.onmessage = (e) => {
            if (e.data.type === 'ended') {
                this._playing = false;
                node.disconnect();
                if (this._node === node) this._node = null;
                if (onEnd) onEnd();
            }
        };
    },

    speak: async function(txt, pitchHz, speed, onEnd) {
        // ensure context then synthesize and play
        await this._ensureCtx();
        const raw = this._synthesize(txt);
        if (!raw) return;
        this._rawBuf = raw;
        this._startPlayback(raw, pitchHz, speed, onEnd);
    },

    setParams: function(pitchHz, speed) {
        // send realtime params to worklet
        if (this._node) {
            this._node.port.postMessage({
                type:  'params',
                pitch: pitchHz / 105.0,
                speed: speed
            });
        }
    },

    // calls the exported C function tts_set_whisper(int).
    // ,ust be called BEFORE speak() — the effect is baked during synthesis.
    setWhisper: function(enable) {
        if (!this.Module) return;
        if (typeof this.Module._tts_set_whisper === 'function') {
            this.Module._tts_set_whisper(enable ? 1 : 0);
        }
    },

    stop: function() {
        // stop and disconnect worklet
        if (this._node) {
            this._node.port.postMessage({ type: 'stop' });
            this._node.disconnect();
            this._node = null;
        }
        this._playing = false;
    },

    renderWav: function(pitchHz, speed) {
        // render resampled wav from raw buffer
        const raw = this._rawBuf;
        if (!raw) return null;
        const step   = speed * (pitchHz / 105.0);
        const outLen = Math.round(raw.length / step);
        const out    = new Float32Array(outLen);
        for (let i = 0; i < outLen; i++) {
            const pos = i * step;
            const lo  = Math.min(Math.floor(pos), raw.length - 1);
            const hi  = Math.min(lo + 1, raw.length - 1);
            const t   = pos - Math.floor(pos);
            out[i]    = raw[lo] * (1 - t) + raw[hi] * t;
        }
        return out;
    }
};
