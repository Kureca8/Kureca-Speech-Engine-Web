// tts-processor.js
// audioworklet processor for tts buffer playback with realtime effects
//
// audioparams:
//   pitch          — pitch multiplier (1.0 = original)
//   speed          — playback speed multiplier (1.0 = normal)
//   vibratoDepth   — vibrato depth (0.0–0.1)
//   vibratoRate    — vibrato lfo rate in hz
//   tremoloDepth   — tremolo depth (0.0–1.0)
//   tremoloRate    — tremolo lfo rate in hz
//
// messages (port.postMessage):
//   { type: 'load',    buf, srcSampleRate, pitch, speed, loop,
//                      vibratoDepth, vibratoRate, vibratoWave,
//                      tremoloDepth, tremoloRate, tremoloWave }
//   { type: 'stop' }
//   { type: 'params',  pitch, speed }
//   { type: 'vibrato', depth, rate, wave }  — wave: 'sine'|'triangle'|'square'|'sawtooth'|'noise'
//   { type: 'tremolo', depth, rate, wave }
//   { type: 'setLoop', loop }

class TTSProcessor extends AudioWorkletProcessor {

    // ─── AudioParam descriptors ──────────────────────────────────────────────
    static get parameterDescriptors() {
        return [
            { name: 'pitch',        defaultValue: 1.0, minValue: 0.25, maxValue: 4.0,  automationRate: 'k-rate' },
            { name: 'speed',        defaultValue: 1.0, minValue: 0.25, maxValue: 4.0,  automationRate: 'k-rate' },
            { name: 'vibratoDepth', defaultValue: 0.0, minValue: 0.0,  maxValue: 0.5,  automationRate: 'a-rate' },
            { name: 'vibratoRate',  defaultValue: 5.0, minValue: 0.1,  maxValue: 20.0, automationRate: 'k-rate' },
            { name: 'tremoloDepth', defaultValue: 0.0, minValue: 0.0,  maxValue: 1.0,  automationRate: 'a-rate' },
            { name: 'tremoloRate',  defaultValue: 4.0, minValue: 0.1,  maxValue: 20.0, automationRate: 'k-rate' },
        ];
    }

    // ─── Конструктор ─────────────────────────────────────────────────────────
    constructor() {
        super();

        // буфер и позиция воспроизведения
        this._buf     = null;
        this._pos     = 0.0;
        this._playing = false;
        this._loop    = false;

        // частоты дискретизации
        this._srcSampleRate = null;
        this._outSampleRate = sampleRate; // глобальная переменная worklet

        // vibrato (pitch modulation)
        // wave: 'sine' | 'triangle' | 'square' | 'sawtooth' | 'noise'
        this._vibPhase = 0.0;
        this._vibWave  = 'sine';

        // tremolo (amplitude modulation)
        this._tremPhase = 0.0;
        this._tremWave  = 'sine';

        // manual params fallback (when audioparams unavailable)
        this._manual = {
            pitch:     null,
            speed:     null,
            vibDepth:  null,
            vibRate:   null,
            tremDepth: null,
            tremRate:  null,
        };

        this._TWO_PI = Math.PI * 2;

        // noise state — separate random value held per lfo cycle
        this._vibNoise  = 0.0;
        this._vibNoisePrev = 0.0;
        this._tremNoise = 0.0;
        this._tremNoisePrev = 0.0;

        this.port.onmessage = (e) => this._onMessage(e.data);
    }

    // message handler
    _onMessage(d) {
        if (!d) return;

        switch (d.type) {
            case 'load':
                this._buf           = d.buf instanceof Float32Array ? d.buf : new Float32Array(d.buf);
                this._pos           = 0.0;
                this._playing       = true;
                this._srcSampleRate = d.srcSampleRate || this._outSampleRate;
                this._loop          = !!d.loop;
                this._applyManual(d);
                if (d.vibratoWave) this._vibWave  = d.vibratoWave;
                if (d.tremoloWave) this._tremWave = d.tremoloWave;
                break;

            case 'stop':
                this._playing = false;
                this._buf     = null;
                break;

            case 'params':
                this._applyManual(d);
                break;

            case 'vibrato':
                if (typeof d.depth === 'number') this._manual.vibDepth = d.depth;
                if (typeof d.rate  === 'number') this._manual.vibRate  = d.rate;
                if (d.wave) this._vibWave = d.wave;
                break;

            case 'tremolo':
                if (typeof d.depth === 'number') this._manual.tremDepth = d.depth;
                if (typeof d.rate  === 'number') this._manual.tremRate  = d.rate;
                if (d.wave) this._tremWave = d.wave;
                break;

            case 'setLoop':
                this._loop = !!d.loop;
                break;
        }
    }

    // copy manual params from message
    _applyManual(d) {
        if (typeof d.pitch        === 'number') this._manual.pitch     = d.pitch;
        if (typeof d.speed        === 'number') this._manual.speed     = d.speed;
        if (typeof d.vibratoDepth === 'number') this._manual.vibDepth  = d.vibratoDepth;
        if (typeof d.vibratoRate  === 'number') this._manual.vibRate   = d.vibratoRate;
        if (typeof d.tremoloDepth === 'number') this._manual.tremDepth = d.tremoloDepth;
        if (typeof d.tremoloRate  === 'number') this._manual.tremRate  = d.tremoloRate;
    }

    // lfo wave generator — phase ∈ [0, 2π) → value in [-1, 1]
    // noise is handled externally via sample-and-hold (see process loop)
    _lfoSample(phase, wave) {
        switch (wave) {
            case 'triangle':
                return 1.0 - Math.abs(((phase / Math.PI) % 2.0) - 1.0) * 2.0 - 1.0;
            case 'square':
                return phase < Math.PI ? 1.0 : -1.0;
            case 'sawtooth':
                return (phase / Math.PI) - 1.0;
            case 'sine':
            default:
                return Math.sin(phase);
        }
    }

    // ─── Главный цикл обработки ──────────────────────────────────────────────
    process(inputs, outputs, parameters) {
        const output = outputs[0];
        if (!output || !output[0]) return true;
        const out = output[0];

        if (!this._playing || !this._buf || this._buf.length === 0) {
            out.fill(0);
            return true;
        }

        const buf    = this._buf;
        const len    = buf.length;
        const outSR  = this._outSampleRate || sampleRate;
        const srcSR  = this._srcSampleRate || outSR;
        const m      = this._manual;

        // source/output sample rate ratio
        const rateRatio = srcSR / outSR;

        // read audioparams (k-rate → length 1, a-rate → length 128)
        const pPitch     = parameters.pitch;
        const pSpeed     = parameters.speed;
        const pVibDepth  = parameters.vibratoDepth;
        const pVibRate   = parameters.vibratoRate;
        const pTremDepth = parameters.tremoloDepth;
        const pTremRate  = parameters.tremoloRate;

        for (let i = 0; i < out.length; i++) {
            const pitch    = (pPitch.length    > 1 ? pPitch[i]    : pPitch[0])    || m.pitch    || 1.0;
            const speed    = (pSpeed.length    > 1 ? pSpeed[i]    : pSpeed[0])    || m.speed    || 1.0;
            const vibDepth = (pVibDepth.length > 1 ? pVibDepth[i] : pVibDepth[0]) ?? m.vibDepth  ?? 0.0;
            const vibRate  = (pVibRate.length  > 1 ? pVibRate[i]  : pVibRate[0])  || m.vibRate   || 5.0;
            const tremDepth= (pTremDepth.length> 1 ? pTremDepth[i]: pTremDepth[0])?? m.tremDepth ?? 0.0;
            const tremRate = (pTremRate.length > 1 ? pTremRate[i] : pTremRate[0]) || m.tremRate  || 4.0;

            // base step: how many source samples to advance per output sample
            let step = rateRatio * speed * pitch;

            // vibrato: modulate step (pitch modulation)
            if (vibDepth > 0.0) {
                const phaseInc = this._TWO_PI * vibRate / outSR;
                let lfo;
                if (this._vibWave === 'noise') {
                    // sample-and-hold: pick new random value on each cycle crossing
                    if (this._vibPhase + phaseInc >= this._TWO_PI) this._vibNoise = Math.random() * 2.0 - 1.0;
                    lfo = this._vibNoise;
                } else {
                    lfo = this._lfoSample(this._vibPhase, this._vibWave);
                }
                this._vibPhase = (this._vibPhase + phaseInc) % this._TWO_PI;
                step = step * (1.0 + vibDepth * lfo);
            }

            // end of buffer check
            if (this._pos >= len - 1) {
                if (this._loop) {
                    this._pos = Math.max(0, this._pos - len);
                } else {
                    for (let j = i; j < out.length; j++) out[j] = 0;
                    this._playing = false;
                    this.port.postMessage({ type: 'ended' });
                    return true;
                }
            }

            // linear interpolation
            const lo  = Math.floor(this._pos);
            const hi  = Math.min(lo + 1, len - 1);
            const t   = this._pos - lo;
            let sample = buf[lo] * (1 - t) + buf[hi] * t;

            // tremolo: modulate amplitude
            if (tremDepth > 0.0) {
                const phaseInc = this._TWO_PI * tremRate / outSR;
                let lfo;
                if (this._tremWave === 'noise') {
                    // sample-and-hold: pick new random value on each cycle crossing
                    if (this._tremPhase + phaseInc >= this._TWO_PI) this._tremNoise = Math.random() * 2.0 - 1.0;
                    lfo = this._tremNoise;
                } else {
                    lfo = this._lfoSample(this._tremPhase, this._tremWave);
                }
                this._tremPhase = (this._tremPhase + phaseInc) % this._TWO_PI;
                // lfo ∈ [−1, 1] → gain ∈ [1−depth, 1]
                sample = sample * (1.0 - tremDepth * 0.5 * (1.0 - lfo));
            }

            out[i] = sample;
            this._pos += step;
        }

        return true;
    }
}

registerProcessor('tts-processor', TTSProcessor);
