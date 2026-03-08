// tts-wrapper.js

var TTSWrapper = {
    Module:null, sampleRate:16000, _ctx:null, _node:null, _rawBuf:null,
    _playing:false, _loopMode:false, _destination:null,
    _inputGain:null, _master:null, _units:{}, _chain:[],
    _pending:{ vibrato:{depth:0,rate:5,wave:'sine'}, tremolo:{depth:0,rate:4,wave:'sine'} },

    // persistent pointers for sing data (do NOT free immediately after set; WASM may keep pointer)
    _singNotesPtr:0, _singNotesLen:0,
    _singBeatsPtr:0, _singBeatsLen:0,

    init: async function(TTSModuleFactory) {
        const mod = await TTSModuleFactory(); this.Module = mod;
        try { if(typeof mod._tts_sample_rate==='function') this.sampleRate=mod._tts_sample_rate(); } catch(e){}
        console.log('tts ready. sample rate:',this.sampleRate);
    },
    setLoop:function(f){this._loopMode=!!f;this._nodePost({type:'setLoop',loop:!!f});},
    setDestination:function(n){
        this._destination=n;
        if(this._master){try{this._master.disconnect();}catch(_){}this._master.connect(n||this._ctx.destination);}
    },
    speak:async function(txt,pitchHz=105,speed=1,onEnd){
        await this._ensureCtx();
        let raw=this._synthesize(txt); if(!raw) return null;
        this._rawBuf=raw; await this._startPlayback(raw,pitchHz,speed,onEnd); return true;
    },
    stop:function(){
        if(!this._node)return;
        if(this._node._fallbackSrc){try{this._node._fallbackSrc.stop();}catch(_){}try{this._node._fallbackSrc.disconnect();}catch(_){}}
        else{this._nodePost({type:'stop'});try{this._node.disconnect();}catch(_){}}
        this._node=null;this._playing=false;
    },
    setParams:function(pitchHz=105,speed=1){
        this._setAudioParam('pitch',pitchHz/105); this._setAudioParam('speed',speed);
    },
    setVibrato:function(depth=0,rate=5,wave='sine'){
        depth=Math.max(0,Math.min(0.5,depth)); rate=Math.max(0.01,Math.min(50,rate));
        wave=['sine','triangle','square','sawtooth','noise'].includes(wave)?wave:'sine';
        this._pending.vibrato={depth,rate,wave};
        this._setAudioParam('vibratoDepth',depth); this._setAudioParam('vibratoRate',rate);
        this._nodePost({type:'vibrato',depth,rate,wave});
    },
    setTremolo:function(depth=0,rate=4,wave='sine'){
        depth=Math.max(0,Math.min(1,depth)); rate=Math.max(0.01,Math.min(50,rate));
        wave=['sine','triangle','square','sawtooth','noise'].includes(wave)?wave:'sine';
        this._pending.tremolo={depth,rate,wave};
        this._setAudioParam('tremoloDepth',depth); this._setAudioParam('tremoloRate',rate);
        this._nodePost({type:'tremolo',depth,rate,wave});
    },
    setDistortion:function(type='softclip',amount=0,wet=0,tone=1,unitId){
        const u=this._getUnit(unitId,'distortion'); if(!u)return;
        wet=Math.max(0,Math.min(1,wet));
        if(type==='off'){u.dryGain.gain.value=1;u.wetGain.gain.value=0;return;}
        u.dist.curve=this._makeDistortionCurve(type,amount);
        u.wetGain.gain.value=wet; u.dryGain.gain.value=1-wet;
        u.tone.frequency.value=500*Math.pow(40,Math.max(0,Math.min(1,tone)));
    },
    setFilter:function(freq=1000,q=1,type='lowpass',gain=0,unitId){
        const u=this._getUnit(unitId,'filter'); if(!u)return;
        u.filter.type=type; u.filter.frequency.value=freq; u.filter.Q.value=q;
        if(type==='peaking'||type==='lowshelf'||type==='highshelf') u.filter.gain.value=gain;
    },
    setChorus:function(wet=0,delayMs=20,depthMs=5,rateHz=1.5,unitId){
        const u=this._getUnit(unitId,'chorus'); if(!u)return;
        wet=Math.max(0,Math.min(1,wet));
        u.wetGain.gain.value=wet*0.5; u.dryGain.gain.value=1-wet;
        u.delayL.delayTime.value=delayMs/1000; u.delayR.delayTime.value=delayMs*1.03/1000;
        u.lfoGainL.gain.value=depthMs/1000; u.lfoGainR.gain.value=depthMs/1000;
        u.lfoL.frequency.value=rateHz; u.lfoR.frequency.value=rateHz*1.07;
    },
    setFlanger:function(wet=0,delayMs=5,depthMs=2,rateHz=0.5,feedback=0.5,unitId){
        const u=this._getUnit(unitId,'flanger'); if(!u)return;
        wet=Math.max(0,Math.min(1,wet)); feedback=Math.max(0,Math.min(0.95,feedback));
        u.wetGain.gain.value=wet; u.dryGain.gain.value=1-wet;
        u.delay.delayTime.value=delayMs/1000; u.feedback.gain.value=feedback;
        u.lfoGain.gain.value=depthMs/1000; u.lfo.frequency.value=rateHz;
    },
    setMasterGain:function(g){if(this._master)this._master.gain.value=g;},
    setWhisper:function(e){if(typeof this.Module?._tts_set_whisper==='function')this.Module._tts_set_whisper(e?1:0);},
    setSing:function(e){if(typeof this.Module?._tts_set_sing==='function')this.Module._tts_set_sing(e?1:0);},
    setEmotion:function(e){if(typeof this.Module?._tts_set_emotion==='function')this.Module._tts_set_emotion(e|0);},
    setGender:function(g){if(typeof this.Module?._tts_set_gender==='function')this.Module._tts_set_gender(g|0);},

    // NOTE: we now keep the malloc'ed pointer alive until replaced/cleared because
    // WASM implementation may keep pointers to these arrays.
    setSingNotes:function(hzArr){
        if(!this.Module||!hzArr||!hzArr.length) return;
        const mod=this.Module;
        const n=hzArr.length;
        // free previous if present
        if(this._singNotesPtr && mod._free) { try{mod._free(this._singNotesPtr);}catch(_){} }
        const ptr=mod._malloc(n*4);
        if(!ptr){ console.warn('malloc failed for sing notes'); this._singNotesPtr=0; this._singNotesLen=0; return; }
        new Float32Array(mod.HEAPF32.buffer,ptr,n).set(hzArr);
        // Inform WASM (it may store ptr internally). We DO NOT free here.
        if(typeof mod._tts_set_sing_notes==='function') mod._tts_set_sing_notes(ptr,n);
        this._singNotesPtr=ptr; this._singNotesLen=n;
    },
    setSingBeats:function(beatsArr){
        if(!this.Module||!beatsArr||!beatsArr.length) return;
        const mod=this.Module;
        const n=beatsArr.length;
        if(this._singBeatsPtr && mod._free) { try{mod._free(this._singBeatsPtr);}catch(_){} }
        const ptr=mod._malloc(n*4);
        if(!ptr){ console.warn('malloc failed for sing beats'); this._singBeatsPtr=0; this._singBeatsLen=0; return; }
        new Float32Array(mod.HEAPF32.buffer,ptr,n).set(beatsArr);
        if(typeof mod._tts_set_sing_beats==='function') mod._tts_set_sing_beats(ptr,n);
        this._singBeatsPtr=ptr; this._singBeatsLen=n;
    },
    // Call when you explicitly want to free buffers (e.g., when unloading voice or before page unload).
    clearSingData:function(){
        const mod=this.Module;
        if(!mod) return;
        if(this._singNotesPtr && mod._free){ try{mod._free(this._singNotesPtr);}catch(_){} }
        if(this._singBeatsPtr && mod._free){ try{mod._free(this._singBeatsPtr);}catch(_){} }
        this._singNotesPtr=0; this._singNotesLen=0; this._singBeatsPtr=0; this._singBeatsLen=0;
        if(typeof mod._tts_clear_sing_notes==='function') try{mod._tts_clear_sing_notes();}catch(_){}
        if(typeof mod._tts_clear_sing_beats==='function') try{mod._tts_clear_sing_beats();}catch(_){}
    },

    setSingParams:function(glide,vibDepth,vibRate){
        if(!this.Module) return;
        if(typeof this.Module._tts_set_sing_params==='function')
            this.Module._tts_set_sing_params(glide,vibDepth,vibRate);
    },

    createFxUnit:function(type){
        if(!this._ctx)return null;
        const ctx=this._ctx;
        const id=type+'_'+(Date.now()%1e9)+'_'+Math.floor(Math.random()*1000);
        let u;
        if(type==='distortion'){
            u={type,_in:ctx.createGain(),_out:ctx.createGain()};
            u.dryGain=ctx.createGain();u.dryGain.gain.value=1;
            u.wetGain=ctx.createGain();u.wetGain.gain.value=0;
            u.dist=ctx.createWaveShaper();u.dist.oversample='4x';
            u.dist.curve=this._makeDistortionCurve('off',0);
            u.tone=ctx.createBiquadFilter();u.tone.type='lowpass';u.tone.frequency.value=20000;
            u._in.connect(u.dryGain);u.dryGain.connect(u._out);
            u._in.connect(u.dist);u.dist.connect(u.tone);u.tone.connect(u.wetGain);u.wetGain.connect(u._out);
        }else if(type==='filter'){
            u={type,_in:ctx.createGain(),_out:ctx.createGain()};
            u.filter=ctx.createBiquadFilter();
            u.filter.type='lowpass';u.filter.frequency.value=1000;u.filter.Q.value=1;
            u._in.connect(u.filter);u.filter.connect(u._out);
        }else if(type==='chorus'){
            u={type,_in:ctx.createGain(),_out:ctx.createGain()};
            u.dryGain=ctx.createGain();u.dryGain.gain.value=1;
            u.wetGain=ctx.createGain();u.wetGain.gain.value=0;
            u.delayL=ctx.createDelay(0.1);u.delayL.delayTime.value=0.02;
            u.delayR=ctx.createDelay(0.1);u.delayR.delayTime.value=0.0206;
            u.lfoL=ctx.createOscillator();u.lfoL.type='sine';u.lfoL.frequency.value=1.5;
            u.lfoR=ctx.createOscillator();u.lfoR.type='sine';u.lfoR.frequency.value=1.6;
            u.lfoGainL=ctx.createGain();u.lfoGainL.gain.value=0.005;
            u.lfoGainR=ctx.createGain();u.lfoGainR.gain.value=0.005;
            u.lfoL.connect(u.lfoGainL);u.lfoGainL.connect(u.delayL.delayTime);
            u.lfoR.connect(u.lfoGainR);u.lfoGainR.connect(u.delayR.delayTime);
            u.lfoL.start();u.lfoR.start();
            u._in.connect(u.dryGain);u.dryGain.connect(u._out);
            u._in.connect(u.delayL);u.delayL.connect(u.wetGain);
            u._in.connect(u.delayR);u.delayR.connect(u.wetGain);
            u.wetGain.connect(u._out);
        }else if(type==='flanger'){
            u={type,_in:ctx.createGain(),_out:ctx.createGain()};
            u.dryGain=ctx.createGain();u.dryGain.gain.value=0.3;
            u.wetGain=ctx.createGain();u.wetGain.gain.value=0.7;
            u.delay=ctx.createDelay(0.05);u.delay.delayTime.value=0.005;
            u.feedback=ctx.createGain();u.feedback.gain.value=0.5;
            u.lfo=ctx.createOscillator();u.lfo.type='sine';u.lfo.frequency.value=0.5;
            u.lfoGain=ctx.createGain();u.lfoGain.gain.value=0.002;
            u.lfo.connect(u.lfoGain);u.lfoGain.connect(u.delay.delayTime);
            u.delay.connect(u.feedback);u.feedback.connect(u.delay);
            u._in.connect(u.dryGain);u.dryGain.connect(u._out);
            u._in.connect(u.delay);u.delay.connect(u.wetGain);u.wetGain.connect(u._out);
            u.lfo.start();
        }else{console.warn('unknown fx type',type);return null;}
        this._units[id]=u; return id;
    },
    destroyFxUnit:function(id){
        const u=this._units[id];if(!u)return;
        try{u.lfoL?.stop();}catch(_){}try{u.lfoR?.stop();}catch(_){}try{u.lfo?.stop();}catch(_){}
        try{u._in.disconnect();}catch(_){}try{u._out.disconnect();}catch(_){}
        delete this._units[id];
        this._chain=this._chain.filter(x=>x!==id);
        this._reconnectChain();
    },
    rebuildChain:function(ids){
        this._chain=ids.filter(id=>this._units[id]);
        this._reconnectChain();
    },

    renderWav:async function(pitchHz=105,speed=1){
        const raw=this._rawBuf; if(!raw)return null;
        const step=speed*(pitchHz/105);
        const outLen=Math.round(raw.length/step);
        const ctx=new OfflineAudioContext(1,outLen,this.sampleRate);
        const buffer=ctx.createBuffer(1,raw.length,this.sampleRate);
        buffer.copyToChannel(raw,0);
        const source=ctx.createBufferSource();
        source.buffer=buffer;source.playbackRate.value=step;
        const master=ctx.createGain();master.gain.value=this._master?.gain.value??1;
        master.connect(ctx.destination);
        const rs=outLen/this.sampleRate;
        let prev=source;
        for(const id of this._chain){
            const u=this._units[id];const out=ctx.createGain();
            if(u.type==='distortion'){
                const dry=ctx.createGain();dry.gain.value=u.dryGain.gain.value;
                const wet=ctx.createGain();wet.gain.value=u.wetGain.gain.value;
                const dist=ctx.createWaveShaper();dist.curve=u.dist.curve;dist.oversample='4x';
                const tone=ctx.createBiquadFilter();tone.type='lowpass';tone.frequency.value=u.tone.frequency.value;
                prev.connect(dry);dry.connect(out);prev.connect(dist);dist.connect(tone);tone.connect(wet);wet.connect(out);
            }else if(u.type==='filter'){
                const f=ctx.createBiquadFilter();f.type=u.filter.type;f.frequency.value=u.filter.frequency.value;f.Q.value=u.filter.Q.value;
                prev.connect(f);f.connect(out);
            }else if(u.type==='chorus'){
                const dry=ctx.createGain();dry.gain.value=u.dryGain.gain.value;
                const wet=ctx.createGain();wet.gain.value=u.wetGain.gain.value;
                const dl=ctx.createDelay(0.1);dl.delayTime.value=u.delayL.delayTime.value;
                const dr=ctx.createDelay(0.1);dr.delayTime.value=u.delayR.delayTime.value;
                const lL=ctx.createOscillator();lL.frequency.value=u.lfoL.frequency.value;lL.type='sine';
                const lR=ctx.createOscillator();lR.frequency.value=u.lfoR.frequency.value;lR.type='sine';
                const gL=ctx.createGain();gL.gain.value=u.lfoGainL.gain.value;
                const gR=ctx.createGain();gR.gain.value=u.lfoGainR.gain.value;
                lL.connect(gL);gL.connect(dl.delayTime);lR.connect(gR);gR.connect(dr.delayTime);
                lL.start(0);lL.stop(rs+0.1);lR.start(0);lR.stop(rs+0.1);
                prev.connect(dry);dry.connect(out);prev.connect(dl);dl.connect(wet);prev.connect(dr);dr.connect(wet);wet.connect(out);
            }else if(u.type==='flanger'){
                const dry=ctx.createGain();dry.gain.value=u.dryGain.gain.value;
                const wet=ctx.createGain();wet.gain.value=u.wetGain.gain.value;
                const del=ctx.createDelay(0.05);del.delayTime.value=u.delay.delayTime.value;
                const fb=ctx.createGain();fb.gain.value=u.feedback.gain.value;
                const lfo=ctx.createOscillator();lfo.frequency.value=u.lfo.frequency.value;lfo.type='sine';
                const lg=ctx.createGain();lg.gain.value=u.lfoGain.gain.value;
                lfo.connect(lg);lg.connect(del.delayTime);del.connect(fb);fb.connect(del);
                lfo.start(0);lfo.stop(rs+0.1);
                prev.connect(dry);dry.connect(out);prev.connect(del);del.connect(wet);wet.connect(out);
            }else{prev.connect(out);}
            prev=out;
        }
        prev.connect(master);
        const{depth:vd,rate:vr}=this._pending.vibrato;
        if(vd>0&&vr>0){const lfo=ctx.createOscillator();lfo.type='sine';lfo.frequency.value=vr;const lg=ctx.createGain();lg.gain.value=vd;lfo.connect(lg);lg.connect(source.playbackRate);lfo.start();}
        source.start();
        const rendered=await ctx.startRendering();
        return rendered.getChannelData(0).slice();
    },

    _ensureCtx:async function(){
        if(this._ctx){if(this._ctx.state==='suspended')await this._ctx.resume();return;}
        this._ctx=new(window.AudioContext||window.webkitAudioContext)({sampleRate:this.sampleRate});
        const ctx=this._ctx;
        if(ctx.audioWorklet){await ctx.audioWorklet.addModule('tts-processor.js');}
        else{console.warn('audioworklet unavailable');}
        this._inputGain=ctx.createGain();
        this._master=ctx.createGain();this._master.gain.value=1;
        this._master.connect(this._destination||ctx.destination);
        this._inputGain.connect(this._master);
    },
    _reconnectChain:function(){
        if(!this._inputGain)return;
        try{this._inputGain.disconnect();}catch(_){}
        for(const id of Object.keys(this._units)){try{this._units[id]._out.disconnect();}catch(_){}}
        const chain=this._chain.filter(id=>this._units[id]);
        if(!chain.length){this._inputGain.connect(this._master);return;}
        this._inputGain.connect(this._units[chain[0]]._in);
        for(let i=0;i<chain.length-1;i++)this._units[chain[i]]._out.connect(this._units[chain[i+1]]._in);
        this._units[chain[chain.length-1]]._out.connect(this._master);
    },
    _getUnit:function(id,type){
        if(!id){for(const uid of this._chain){if(this._units[uid]?.type===type)return this._units[uid];}return null;}
        return this._units[id]||null;
    },
    _startPlayback:async function(raw,pitchHz=105,speed=1,onEnd,loopOverride){
        await this._ensureCtx();
        if(this._node){this._nodePost({type:'stop'});try{this._node.disconnect();}catch(_){}this._node=null;}
        this._playing=true;
        const loopFlag=typeof loopOverride==='boolean'?loopOverride:this._loopMode;
        const pitchRatio=pitchHz/105;
        if(this._ctx.audioWorklet){
            const node=new AudioWorkletNode(this._ctx,'tts-processor',{numberOfOutputs:1,outputChannelCount:[1]});
            this._node=node;node.connect(this._inputGain);
            const copy=raw.slice();
            node.port.postMessage({type:'load',buf:copy,srcSampleRate:this.sampleRate,pitch:pitchRatio,speed,loop:loopFlag,
                vibratoDepth:this._pending.vibrato.depth,vibratoRate:this._pending.vibrato.rate,vibratoWave:this._pending.vibrato.wave,
                tremoloDepth:this._pending.tremolo.depth,tremoloRate:this._pending.tremolo.rate,tremoloWave:this._pending.tremolo.wave,
            },[copy.buffer]);
            this._setAudioParam('pitch',pitchRatio,node);this._setAudioParam('speed',speed,node);
            this._setAudioParam('vibratoDepth',this._pending.vibrato.depth,node);this._setAudioParam('vibratoRate',this._pending.vibrato.rate,node);
            this._setAudioParam('tremoloDepth',this._pending.tremolo.depth,node);this._setAudioParam('tremoloRate',this._pending.tremolo.rate,node);
            node.port.onmessage=(e)=>{
                if(e.data?.type==='ended'){this._playing=false;try{node.disconnect();}catch(_){}if(this._node===node)this._node=null;if(onEnd)onEnd();}
            };
        }else{
            const buf=this._ctx.createBuffer(1,raw.length,this.sampleRate);buf.copyToChannel(raw,0,0);
            const src=this._ctx.createBufferSource();src.buffer=buf;src.loop=loopFlag;src.playbackRate.value=speed*pitchRatio;
            src.connect(this._inputGain);src.start();src.onended=()=>{if(!src.loop&&onEnd)onEnd();};
            this._node={_fallbackSrc:src,port:{postMessage:()=>{}},parameters:null};
        }
    },
    _makeDistortionCurve:function(type='off',amount=0){
        const n=16384,curve=new Float32Array(n);
        if(type==='off'||amount===0){for(let i=0;i<n;i++)curve[i]=i*2/n-1;return curve;}
        const k=Math.max(0,amount);
        for(let i=0;i<n;i++){
            const x=i*2/n-1;
            switch(type){
                case 'softclip':curve[i]=((Math.PI+k)*x)/(Math.PI+k*Math.abs(x))*(1/(1+k/100));break;
                case 'hardclip':{const t=1/(1+k/100);curve[i]=Math.max(-t,Math.min(t,x))/t;break;}
                case 'bitcrush':{const s=Math.pow(2,Math.max(1,Math.min(16,k)))-1;curve[i]=Math.round(x*s)/s;break;}
                case 'foldback':{const t=1/(1+k/200);let v=x;for(let it=0;it<4;it++){if(v>t)v=2*t-v;else if(v<-t)v=-2*t-v;else break;}curve[i]=v/t;break;}
                default:curve[i]=x;
            }
        }
        return curve;
    },
    _allocString:function(str){
        const mod=this.Module;if(!mod?._malloc){console.error('wasm unavailable');return 0;}
        const enc=new TextEncoder().encode(str);const ptr=mod._malloc(enc.length+1);if(!ptr)return 0;
        let h = null;
        try{
            // prefer HEAPU8 view if available and valid
            if(mod.HEAPU8 && mod.HEAPU8.buffer) h = mod.HEAPU8;
            else if(mod.HEAP8 && mod.HEAP8.buffer) h = mod.HEAP8;
            else if(mod.HEAPF32 && mod.HEAPF32.buffer) h = new Uint8Array(mod.HEAPF32.buffer);
        }catch(e){}
        if(!h){mod._free(ptr);return 0;}
        h.set(enc,ptr); h[ptr+enc.length]=0; return ptr;
    },
    _synthesize:function(txt){
        if(!this.Module)return null;
        const mod=this.Module; const sp=this._allocString(txt); if(!sp) return null;
        let got=0; let ptr=0;
        try{
            got = mod._tts_speak(sp)|0;
        }catch(e){
            try{if(mod._free)mod._free(sp);}catch(_){}
            console.error('synthesis error (tts_speak threw):', e);
            return null;
        }
        try{ if(mod._free) mod._free(sp); }catch(_){}
        if(!got || got <= 0) return null;
        // ptr (byte offset) to float buffer
        try{ ptr = mod._tts_get_buf()|0; }catch(e){ console.error('synthesis: _tts_get_buf failed', e); return null; }
        if(!ptr) { console.error('synthesis: got zero ptr'); return null; }

        // defensive checks & prefer HEAPF32 view (Emscripten). We operate in bytes for ptr.
        try{
            // prefer mod.HEAPF32 if available and large enough
            if(mod.HEAPF32 && mod.HEAPF32.buffer && (mod.HEAPF32.length >= (ptr>>2) + got)){
                return new Float32Array(mod.HEAPF32.buffer, ptr, got).slice();
            }
            // fallback: wasmMemory.buffer if exists and large enough
            if(mod.wasmMemory && mod.wasmMemory.buffer && (mod.wasmMemory.buffer.byteLength >= ptr + got*4)){
                return new Float32Array(mod.wasmMemory.buffer, ptr, got).slice();
            }
            // fallback: HEAPU8 copy
            const u8 = mod.HEAPU8 || mod.HEAP8;
            if(u8 && u8.buffer && (u8.byteLength >= ptr + got*4)){
                const out = new Float32Array(got);
                new Uint8Array(out.buffer).set(u8.subarray(ptr, ptr + got*4));
                return out;
            }
            // last resort: try to construct from wasmMemory as Float32Array
            if(mod.wasmMemory && mod.wasmMemory.buffer){
                const all = new Float32Array(mod.wasmMemory.buffer);
                const start = ptr >> 2;
                if(all.length >= start + got) return all.slice(start, start + got);
            }
        }catch(e){
            console.error('synthesis error while reading WASM buffer:', e);
            return null;
        }
        console.error('synthesis: failed to read buffer - not enough memory or unexpected layout');
        return null;
    },
    _nodePost:function(msg){try{this._node?.port?.postMessage(msg);}catch(_){}},
    _setAudioParam:function(name,value,node){
        const n=node||this._node;if(!n)return;
        try{const p=n.parameters?.get(name);if(p)p.setValueAtTime(value,this._ctx.currentTime);else this._nodePost({type:'params',[name]:value});}
        catch(e){console.warn(`setAudioParam(${name}) failed:`,e);}
    },
};
