// tts processor realtime resampler
class TTSProcessor extends AudioWorkletProcessor {
    constructor() {
        super();
        this._buf      = null;   // raw samples
        this._pos      = 0.0;    // fractional read position
        this._pitch    = 1.0;    // pitch ratio (pitchhz / 105)
        this._speed    = 1.0;    // speed multiplier
        this._playing  = false;  // playing flag

        this.port.onmessage = (e) => {
            const d = e.data;
            if (d.type === 'load') {
                // load buffer and start
                this._buf     = d.buf;
                this._pos     = 0.0;
                this._pitch   = d.pitch;
                this._speed   = d.speed;
                this._playing = true;
            } else if (d.type === 'params') {
                // update params
                this._pitch = d.pitch;
                this._speed = d.speed;
            } else if (d.type === 'stop') {
                // stop and clear
                this._playing = false;
                this._buf = null;
            }
        };
    }

    process(inputs, outputs) {
        const out = outputs[0][0];
        if (!out) return true;

        // output silence when not playing
        if (!this._playing || !this._buf) {
            out.fill(0);
            return true;
        }

        const buf  = this._buf;
        const len  = buf.length;
        const step = this._speed * this._pitch;   // advance step

        // linear interpolation resampling
        for (let i = 0; i < out.length; i++) {
            if (this._pos >= len - 1) {
                // finish and signal end
                for (let j = i; j < out.length; j++) out[j] = 0;
                this._playing = false;
                this.port.postMessage({ type: 'ended' });
                return true;
            }

            const lo  = Math.floor(this._pos);
            const hi  = Math.min(lo + 1, len - 1);
            const t   = this._pos - lo;
            out[i]    = buf[lo] * (1 - t) + buf[hi] * t;
            this._pos += step;
        }

        return true;
    }
}

registerProcessor('tts-processor', TTSProcessor);
