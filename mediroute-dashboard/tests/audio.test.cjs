const test = require('node:test');
const assert = require('node:assert/strict');
const vm = require('node:vm');
const fs = require('node:fs');
function application() {
  const elements = new Map();
  const context = vm.createContext({
    document:{getElementById(id){if(!elements.has(id)) elements.set(id,{value:'',hidden:false,textContent:''});return elements.get(id);},querySelectorAll(){return[];}},
    window:{addEventListener(){}},
    fetch:async()=>({ok:true,json:async()=>({enrolled:true,authenticated:false,models_ready:true})}),
    btoa:value=>Buffer.from(value,'binary').toString('base64'),
    setTimeout,clearTimeout,
  });
  vm.runInContext(fs.readFileSync('static/app.js','utf8'),context);
  return context;
}
test('48 kHz input becomes 16 kHz little-endian PCM with correct amplitude',()=>{
  const result=vm.runInContext('encodePCM([new Float32Array(48000).fill(0.5)],48000)',application());
  const pcm=Buffer.from(result,'base64');
  assert.equal(pcm.length,32000);
  assert.equal(pcm.readInt16LE(0),16384);
});
test('recording is capped at 30 seconds and samples are clipped',()=>{
  const result=vm.runInContext('encodePCM([new Float32Array(16000*31).fill(-2)],16000)',application());
  const pcm=Buffer.from(result,'base64');
  assert.equal(pcm.length,960000);assert.equal(pcm.readInt16LE(0),-32768);
});
test('audio worklet flushes its final samples and sends acknowledgement',()=>{
  let Processor;
  const sent=[];
  const context=vm.createContext({AudioWorkletProcessor:class{constructor(){this.port={postMessage:value=>sent.push(value)};}},registerProcessor(name,cls){Processor=cls;}});
  vm.runInContext(fs.readFileSync('static/pcm-capture.js','utf8'),context);
  const processor=new Processor();
  processor.process([[new Float32Array(128).fill(0.5)]]);
  processor.port.onmessage({data:'flush'});
  assert.equal(sent[0].samples.length,128);assert.equal(sent[1].flushed,true);
  assert.equal(processor.process([]),false);
});
