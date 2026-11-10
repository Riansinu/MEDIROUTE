const $ = id => document.getElementById(id);
let stream, busy = false, enrolled = false;
let recording = null, micPhase = 'idle', micGeneration = 0;
async function api(path, body) {
  const response = await fetch(path, body === undefined ? {} : {method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify(body)});
  const data = await response.json();
  if (!response.ok) throw new Error(data.error || 'Request failed. Please retry.');
  return data;
}
function stopCamera() {
  stream?.getTracks().forEach(track => track.stop()); stream = null;
  $('camera').srcObject = null; $('camera-placeholder').hidden = false; $('face-submit').disabled = true;
}
async function refreshSession() {
  const state = await api('/api/session'); enrolled = state.enrolled;
  $('auth').hidden = state.authenticated; $('workspace').hidden = !state.authenticated; $('logout').hidden = !state.authenticated;
  $('setup').hidden = enrolled; $('recovery').hidden = !enrolled;
  $('auth-title').textContent = enrolled ? 'Welcome back' : 'Create your face profile';
  $('face-submit').textContent = enrolled ? 'Verify & enter' : 'Enrol & enter';
  $('user-name').textContent = state.name || '';
  $('answer-mode').textContent = state.mode;
  if (!state.models_ready) $('auth-message').textContent = 'Face models are missing. Run setup_models.py, then refresh.';
  if (state.authenticated) {
    stopCamera(); $('auth-message').textContent = '';
    try {
      const data=await api('/api/topics');
      const picker=$('topic-picker'); picker.replaceChildren();
      const placeholder=document.createElement('option');placeholder.value='';placeholder.textContent='Explore '+data.topics.length+' project topics…';picker.append(placeholder);
      for(const topic of data.topics){const option=document.createElement('option');option.value=topic.question;option.textContent=topic.title;picker.append(option);}
    } catch(error) { $('voice-status').textContent=error.message; }
  }
}
$('enable-camera').onclick = async () => {
  $('enable-camera').disabled = true;
  $('auth-message').textContent = 'Requesting camera access. Allow the browser permission prompt to continue.';
  try {
    stopCamera();
    if (!navigator.mediaDevices?.getUserMedia) {
      const error = new Error('Camera capture is unavailable in this browser.');
      error.name = 'CameraUnsupported';
      throw error;
    }
    try {
      stream = await navigator.mediaDevices.getUserMedia({video:{width:{ideal:640},height:{ideal:480},facingMode:'user'},audio:false});
    } catch (error) {
      if (error.name !== 'OverconstrainedError') throw error;
      stream = await navigator.mediaDevices.getUserMedia({video:true,audio:false});
    }
    $('camera').srcObject = stream; await $('camera').play();
    $('camera-placeholder').hidden = true; $('face-submit').disabled = false;
    $('auth-message').textContent = 'Camera ready. Look at the lens, then select '+(enrolled?'Verify & enter.':'Enrol & enter.');
  } catch (error) {
    stopCamera();
    const guidance = {
      CameraUnsupported: 'This browser does not expose camera capture. Open http://localhost:8765 in a separate Chrome or Edge window.',
      NotAllowedError: 'Camera permission was denied or blocked by the browser or Windows. Allow camera access for this site. If no permission prompt appears here, open http://localhost:8765 in a separate Chrome or Edge window and try there.',
      SecurityError: 'Camera access is disabled by the browser security policy. Open http://localhost:8765 in a separate Chrome or Edge window.',
      NotFoundError: 'No camera was found. Connect or enable your webcam, then try again.',
      NotReadableError: 'The camera could not start. Close other camera apps, check the camera privacy switch and Windows camera permissions, then retry.',
      AbortError: 'Camera startup was interrupted. Close other camera apps and try again.',
      OverconstrainedError: 'The webcam could not supply a supported video stream. Try another camera.'
    };
    const detail = error.name || 'CameraError';
    $('auth-message').textContent = (guidance[detail] || 'Camera preview failed. Try opening http://localhost:8765 in a separate Chrome or Edge window.') +
      ' [' + detail + (error.message ? ': ' + error.message : '') + ']' +
      (enrolled ? ' You can also sign in with your recovery password.' : ' Face enrolment must complete before recovery login is available.');
  } finally { $('enable-camera').disabled = false; }
};
function capture() {
  const video = $('camera');
  if (!stream || !video.videoWidth) throw new Error('Enable the camera and wait for the preview first.');
  const canvas = document.createElement('canvas');
  const scale = Math.min(1,640/video.videoWidth);
  canvas.width = Math.round(video.videoWidth*scale); canvas.height = Math.round(video.videoHeight*scale);
  canvas.getContext('2d').drawImage(video,0,0,canvas.width,canvas.height);
  return canvas.toDataURL('image/jpeg',0.9);
}
$('face-submit').onclick = async () => {
  $('face-submit').disabled = true; $('auth-message').textContent = 'Checking your face…';
  try {
    const image = capture();
    if (enrolled) await api('/api/login',{image,method:'face'});
    else await api('/api/enrol',{image,name:$('name').value,token:$('token').value,password:$('new-password').value,consent:$('consent').checked});
    $('new-password').value = ''; $('token').value = ''; await refreshSession();
  } catch(error) { $('auth-message').textContent = error.message; }
  finally { $('face-submit').disabled = !stream; }
};
$('password-submit').onclick = async () => {
  try { await api('/api/login',{method:'password',password:$('password').value}); $('password').value=''; await refreshSession(); }
  catch(error) { $('auth-message').textContent = error.message; }
};
function resetWorkspace() {
  speechSynthesis.cancel(); cancelRecording(); stopCamera();
  $('robot-frame').removeAttribute('src'); $('robot-frame').hidden = true; $('robot-placeholder').hidden = false;
  $('conversation').replaceChildren(); $('question').value = '';
}
$('logout').onclick = async () => { try { await api('/api/logout',{}); resetWorkspace(); await refreshSession(); } catch(error) { $('voice-status').textContent=error.message; } };
$('load-robot').onclick = () => { $('robot-frame').src='/robot-dashboard'; $('robot-frame').hidden=false; $('robot-placeholder').hidden=true; };
function message(text, type, sources=[]) {
  const element = document.createElement('article'); element.className='message '+type+'-message'; element.textContent=text;
  if (sources.length) { const source = document.createElement('small'); source.textContent='Source: '+sources.join(' · '); element.append(source); }
  $('conversation').append(element); $('conversation').scrollTop=$('conversation').scrollHeight;
}
async function ask(question) {
  if (busy || micPhase !== 'idle' || !question.trim()) return;
  busy=true; $('send').disabled=true; $('mic').disabled=true; speechSynthesis.cancel();
  message(question,'user'); $('question').value=''; $('voice-status').textContent='Finding an answer…';
  try {
    const answer = await api('/api/ask',{question});
    // A sign-out during a slow AI response must not reveal it on the login page.
    if ($('workspace').hidden) return;
    message(answer.answer,'assistant',answer.sources); $('answer-mode').textContent=answer.mode;
    if (answer.suggestions) {
      const suggestions=$('suggestions');suggestions.replaceChildren();
      for(const question of answer.suggestions){const button=document.createElement('button');button.textContent=question;button.onclick=()=>ask(question);suggestions.append(button);}
    }
    $('voice-status').textContent=answer.notice || '';
    if ($('speak').checked && 'speechSynthesis' in window) { const speech=new SpeechSynthesisUtterance(answer.answer); speech.rate=1; speechSynthesis.speak(speech); }
  } catch(error) { $('voice-status').textContent=error.message; }
  finally { busy=false; $('send').disabled=false; $('mic').disabled=false; }
}
$('ask-form').onsubmit = event => { event.preventDefault(); ask($('question').value); };
document.querySelectorAll('[data-question]').forEach(button=>button.onclick=()=>ask(button.dataset.question));
$('topic-picker').onchange=event=>{if(event.target.value)ask(event.target.value);};
$('stop-audio').onclick=()=>speechSynthesis.cancel();
$('speak').onchange=()=>{if (!$('speak').checked) speechSynthesis.cancel();};
function disposeRecording(item) {
  if (!item) return;
  clearTimeout(item.timer);
  item.media?.getTracks().forEach(track=>track.stop());
  item.source?.disconnect(); item.node?.disconnect();
  if (item.context && item.context.state !== 'closed') item.context.close().catch(()=>{});
}
function cancelRecording() {
  micGeneration++;
  disposeRecording(recording); recording=null; micPhase='idle';
  $('mic').textContent='● Speak'; $('mic').disabled=false; $('send').disabled=false;
}
function encodePCM(chunks, rate) {
  const length=chunks.reduce((total,chunk)=>total+chunk.length,0);
  const samples=new Float32Array(length);
  let offset=0; for (const chunk of chunks) { samples.set(chunk,offset); offset+=chunk.length; }
  const count=Math.min(480000,Math.floor(length*16000/rate));
  const buffer=new ArrayBuffer(count*2), view=new DataView(buffer);
  for (let i=0;i<count;i++) {
    const start=Math.floor(i*rate/16000), end=Math.min(length,Math.max(start+1,Math.floor((i+1)*rate/16000)));
    let sum=0; for (let j=start;j<end;j++) sum+=samples[j];
    const value=Math.max(-1,Math.min(1,sum/(end-start)));
    view.setInt16(i*2,Math.round(value*(value<0?32768:32767)),true);
  }
  const bytes=new Uint8Array(buffer); let binary='';
  for (let i=0;i<bytes.length;i+=8192) binary+=String.fromCharCode(...bytes.subarray(i,i+8192));
  return btoa(binary);
}
async function finishRecording() {
  if (micPhase!=='recording' || !recording) return;
  const item=recording, generation=micGeneration;
  micPhase='transcribing'; clearTimeout(item.timer); $('mic').disabled=true;
  $('mic').textContent='Transcribing…'; $('voice-status').textContent='Transcribing on this computer…';
  try {
    await new Promise(resolve=>{item.flushed=resolve;item.node.port.postMessage('flush');setTimeout(resolve,400);});
    disposeRecording(item); recording=null;
    if (generation!==micGeneration) return;
    const result=await api('/api/transcribe',{audio:encodePCM(item.chunks,item.context.sampleRate)});
    if (generation!==micGeneration || $('workspace').hidden) return;
    $('question').value=result.text;
    $('voice-status').textContent='Review your question, then press Ask. Audio was transcribed locally.';
  } catch(error) { if (generation===micGeneration) $('voice-status').textContent=error.message; }
  finally {
    disposeRecording(item);
    if (generation===micGeneration) { recording=null;micPhase='idle';$('mic').disabled=false;$('mic').textContent='● Speak';$('send').disabled=false; }
  }
}
$('mic').onclick=async()=>{
  if (micPhase==='recording') { await finishRecording();return; }
  if (busy || micPhase!=='idle') return;
  speechSynthesis.cancel();
  const generation=++micGeneration;
  const item={chunks:[],total:0}; recording=item; micPhase='starting';
  $('mic').disabled=true; $('send').disabled=true;
  $('voice-status').textContent='Allow microphone access when prompted.';
  try {
    if (!navigator.mediaDevices?.getUserMedia || !window.AudioWorkletNode) throw new Error('Microphone capture is unavailable here. Open http://localhost:8765 in Chrome or Edge.');
    item.context=new AudioContext(); await item.context.resume();
    item.media=await navigator.mediaDevices.getUserMedia({audio:{echoCancellation:true,noiseSuppression:true,channelCount:1},video:false});
    if (generation!==micGeneration) {disposeRecording(item);return;}
    await item.context.audioWorklet.addModule('/assets/pcm-capture.js');
    if (generation!==micGeneration) {disposeRecording(item);return;}
    item.node=new AudioWorkletNode(item.context,'pcm-capture');
    item.node.port.onmessage=event=>{
      if (event.data.flushed) { item.flushed?.();return; }
      if (generation!==micGeneration) return;
      const remaining=Math.max(0,Math.floor(item.context.sampleRate*30)-item.total);
      const chunk=event.data.samples.subarray(0,remaining);
      if (chunk.length) { item.chunks.push(chunk);item.total+=chunk.length; }
      if (micPhase==='recording') {
        let power=0;for (const sample of chunk) power+=sample*sample;
        const level=chunk.length?Math.min(100,Math.round(Math.sqrt(power/chunk.length)*400)):0;
        $('voice-status').textContent='Listening · '+Math.floor(item.total/item.context.sampleRate)+' / 30 sec · Mic level '+level+'% — click Stop to transcribe.';
        if (!remaining || item.total>=item.context.sampleRate*30) finishRecording();
      }
    };
    item.source=item.context.createMediaStreamSource(item.media);
    item.source.connect(item.node); item.node.connect(item.context.destination);
    // The worklet produces silent output, so microphone audio is never played back.
    micPhase='recording';$('mic').textContent='■ Stop';$('mic').disabled=false;
    item.timer=setTimeout(finishRecording,30000);
    $('voice-status').textContent='Listening locally… speak, then click Stop.';
  } catch(error) {
    disposeRecording(item);
    if (generation!==micGeneration) return;
    recording=null;micPhase='idle';$('mic').disabled=false;$('send').disabled=false;$('mic').textContent='● Speak';
    const help={NotAllowedError:'Microphone access is blocked. Allow it in browser site permissions and Windows microphone settings.',NotFoundError:'No microphone was found. Connect or enable a microphone.',NotReadableError:'Microphone could not start. Close other recording apps and try again.'};
    $('voice-status').textContent=(help[error.name] || error.message)+' You can also type your question.';
  }
};
$('delete-profile').onclick=async()=>{
  if (!confirm('Delete your local face profile? You will need to enrol again.')) return;
  try{await api('/api/delete-profile',{password:$('delete-password').value});$('delete-password').value='';resetWorkspace();await refreshSession();}
  catch(error){$('privacy-message').textContent=error.message;}
};
window.addEventListener('pagehide',()=>{stopCamera();speechSynthesis.cancel();cancelRecording();});
refreshSession().catch(error=>{$('auth-message').textContent=error.message;});
