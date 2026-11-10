class PCMCapture extends AudioWorkletProcessor {
  constructor() {
    super();
    this.buffer = [];
    this.port.onmessage = event => {
      if (event.data === 'flush') {
        if (this.buffer.length) this.port.postMessage({samples: new Float32Array(this.buffer)});
        this.buffer = [];
        this.port.postMessage({flushed: true});
        this.finished = true;
      }
    };
  }
  process(inputs) {
    if (this.finished) return false;
    const channel = inputs[0]?.[0];
    if (channel) {
      for (const value of channel) this.buffer.push(value);
      if (this.buffer.length >= 2048) {
        this.port.postMessage({samples: new Float32Array(this.buffer)});
        this.buffer = [];
      }
    }
    return true;
  }
}
registerProcessor('pcm-capture', PCMCapture);
