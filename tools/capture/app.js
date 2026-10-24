import {segment, placement} from '/processing.js';
const $ = id => document.getElementById(id);
const screens = ['setup','shoot','review','done'];
const token = new URLSearchParams(location.search).get('token') || sessionStorage.getItem('capture-token') || '';
if(token) sessionStorage.setItem('capture-token',token);
// Keep pairing in the URL when switching devices; never request external assets.
let poses=[], step=0, frames={}, draftName='', mode='tripod', current=null, sourceImage=null, stream=null, recording=false, showingRaw=false, markPoints=null, clipBlob=null, clipURL=null, storage=null, workVersion=0;
const ctx=$('preview').getContext('2d',{willReadFrequently:true});
function show(id) { for(const s of screens) $(s).hidden=s!==id; }
function message(id,text,error=false) { $(id).textContent=text; $(id).classList.toggle('error',error); }
function canvas(w=640,h=480) { const c=document.createElement('canvas');c.width=w;c.height=h;return c; }
function loadImage(url) { return new Promise((resolve,reject)=>{const i=new Image();i.onload=()=>resolve(i);i.onerror=()=>reject(new Error('Не удалось открыть фото. Выберите JPEG или PNG.'));i.src=url;}); }
function readFile(file) { return new Promise((resolve,reject)=>{const r=new FileReader();r.onload=()=>resolve(r.result);r.onerror=()=>reject(new Error('Не удалось прочитать файл'));r.readAsDataURL(file);}); }
function blank() { ctx.fillStyle='#272720';ctx.fillRect(0,0,640,480); }
function guideSVG(pose) {
  const crouch=pose==='crouch', down=pose==='down', portrait=pose==='portrait';
  const hy=crouch?225:125, shoulder=hy+55, hips=crouch?355:300;
  let arms=[[250,shoulder+85,300,shoulder+30],[378,shoulder+25,420,shoulder+10]], legs=[[260,410],[370,410]];
  if(pose==='breathe') arms=[[245,shoulder+75,290,shoulder+15],[385,shoulder+15,425,shoulder]];
  if(pose==='punch1') arms=[[225,shoulder+55,210,shoulder-5],[375,shoulder+35,410,shoulder+10]];
  if(pose==='punch2') arms=[[250,shoulder+90,290,shoulder+35],[410,shoulder+4,510,shoulder-5]];
  if(pose==='kick') legs=[[255,410],[490,305]];
  if(pose==='walk1'||pose==='walk3') legs=pose==='walk1'?[[235,410],[405,410]]:[[400,410],[240,410]];
  if(pose==='walk2'||pose==='walk4') legs=pose==='walk2'?[[295,410],[340,380]]:[[290,380],[350,410]];
  if(pose==='jump') legs=[[260,355],[390,350]];
  if(pose==='block') arms=[[275,shoulder+35,335,hy-10],[390,shoulder+35,390,hy-15]];
  if(pose==='hit') arms=[[205,shoulder+25,205,shoulder-40],[415,shoulder+35,455,shoulder+70]];
  if(pose==='win') arms=[[230,hy+5,245,hy-60],[405,hy+5,420,hy-60]];
  const limb=(x,y,a)=>`<path d="M${x} ${y} Q${a[0]} ${a[1]} ${a[2]} ${a[3]}" stroke="white" stroke-width="30"/><path d="M${x} ${y} Q${a[0]} ${a[1]} ${a[2]} ${a[3]}" stroke="#77776e" stroke-width="24"/>`;
  let transform=down?'translate(90 160) rotate(82 320 270) scale(.77)':portrait?'translate(-435 -145) scale(2.35)':'translate(64 86) scale(.8)';
  return `<svg viewBox="0 0 640 480" xmlns="http://www.w3.org/2000/svg"><g fill="none" stroke="white" stroke-width="2" stroke-dasharray="8 8"><path d="M22 430H618"/><path d="M22 50H618"/><path d="M320 28V452" opacity=".3"/></g><g transform="${transform}" stroke-linecap="round" stroke-linejoin="round"><path d="M280 ${shoulder} Q245 ${shoulder+70} 279 ${hips} L350 ${hips} Q390 ${shoulder+65} 365 ${shoulder}Z" fill="#ffffff12" stroke="white" stroke-width="3"/>${limb(291,hips,[285,hips+60,...legs[0]])}${limb(344,hips,[365,hips+55,...legs[1]])}${limb(280,shoulder+5,arms[0])}${limb(365,shoulder+5,arms[1])}<path d="M286 ${hy+16} Q270 ${hy-35} 309 ${hy-43} L305 ${hy-66} L330 ${hy-44} Q372 ${hy-49} 379 ${hy-12} L403 ${hy+4} Q403 ${hy+29} 369 ${hy+27} L365 ${hy+58} L299 ${hy+58}Z" stroke="white" stroke-width="3" fill="#ffffff10"/><circle cx="362" cy="${hy-10}" r="5" fill="white"/><path d="M378 ${hy+14}H394" stroke="white" stroke-width="2"/></g></svg>`;
}

async function openDB() {
  return new Promise((resolve,reject)=>{const r=indexedDB.open('clayfight-capture',1);r.onupgradeneeded=()=>r.result.createObjectStore('draft');r.onsuccess=()=>resolve(r.result);r.onerror=()=>reject(r.error);});
}
async function db(action,value) {
  if(!storage) return null;
  return new Promise((resolve,reject)=>{const tx=storage.transaction('draft',action==='get'?'readonly':'readwrite'), store=tx.objectStore('draft');let r=action==='get'?store.get('current'):action==='delete'?store.delete('current'):store.put(value,'current');let result;r.onsuccess=()=>{result=r.result;};r.onerror=()=>reject(r.error);tx.oncomplete=()=>resolve(result);tx.onabort=()=>reject(tx.error||new Error('Не удалось сохранить черновик'));});
}
async function saveDraft() {
  try { await db('put',{name:draftName,mode,frames,step,version:1}); $('draft-status').textContent=storage?'Черновик сохранён на этом устройстве.':'Черновик хранится только до закрытия страницы.'; }
  catch { $('draft-status').textContent='Не хватает места для черновика. Не закрывайте страницу до публикации.'; }
}
function stopCamera() { if(recording) return; if(stream) stream.getTracks().forEach(t=>t.stop());stream=null;$('live').srcObject=null;$('live').hidden=true;$('snap').hidden=$('record').hidden=true; }
function updateSteps() {
  $('steps').replaceChildren();
  poses.forEach((p,i)=>{const b=document.createElement('button');b.textContent=`${frames[p.id]?'✓':i+1} ${p.title}`;b.classList.toggle('active',i===step);b.classList.toggle('complete',!!frames[p.id]);b.onclick=()=>{if(!recording) enterStep(i);};$('steps').append(b);});
}
async function enterStep(index) {
  workVersion++;step=index;current=frames[poses[step].id]?structuredClone(frames[poses[step].id]):null;sourceImage=null;markPoints=null;showingRaw=false;
  if(clipURL) URL.revokeObjectURL(clipURL);clipURL=null;clipBlob=null;$('clip').removeAttribute('src');$('clip-area').hidden=true;
  show('shoot');$('step-count').textContent=`ПОЗА ${step+1} ИЗ ${poses.length} · ${draftName}`;$('pose-title').textContent=poses[step].title;$('hint').textContent=poses[step].hint;
  $('guide').innerHTML=guideSVG(poses[step].id);$('accept').disabled=true;$('alignment').hidden=!current;$('previous').disabled=step===0;
  $('view-tag').textContent='НАПРАВЛЯЮЩАЯ ПОЗЫ';$('viewfinder').classList.remove('marking');$('mark-head').hidden=poses[step].id==='portrait';
  $('align-hint').textContent=mode==='handheld'?'Отметьте левый и правый край головы: её ширина поможет убрать разницу дистанции. При сильном изменении ракурса переснимите.':'Сохраняйте дистанцию и зум камеры. Масштаб всех поз берём из первой стойки; ноги выравниваем автоматически.';
  message('status','');blank();
  if(current) { sourceImage=await loadImage(current.source);syncControls();await render(); }
  else { $('live').hidden=!stream; $('preview').hidden=!!stream; }
  updateSteps();window.scrollTo({top:0,behavior:'smooth'});
}
function syncControls() {
  const a=current.alignment;
  $('zoom').value=a.zoom;$('shift').value=a.shift;$('threshold').value=a.threshold;
  $('head-status').textContent=a.headWidth?`Ширина головы: ${Math.round(a.headWidth)}. Можно отметить заново.`:'';
}
function sourcePixels(img) {
  const c=canvas(),cctx=c.getContext('2d',{willReadFrequently:true});cctx.fillStyle='white';cctx.fillRect(0,0,640,480);
  const sc=Math.min(640/img.width,480/img.height);cctx.drawImage(img,(640-img.width*sc)/2,(480-img.height*sc)/2,img.width*sc,img.height*sc);
  return cctx.getImageData(0,0,640,480);
}
async function referenceFor(bounds) {
  if(poses[step].id==='stand') return {scale:300/(bounds.y1-bounds.y0+1),headWidth:current.alignment.headWidth};
  const standing=frames.stand;
  if(!standing) throw new Error('Сначала сохраните позу «Готовность»: по ней определяем общий масштаб.');
  const img=await loadImage(standing.source), data=sourcePixels(img);
  const ref=segment(data.data,640,480,standing.alignment.threshold).bounds;
  return {scale:300/(ref.y1-ref.y0+1),headWidth:standing.alignment.headWidth};
}
async function render() {
  const version=++workVersion;
  if(!sourceImage||!current) return;
  $('live').hidden=true;$('preview').hidden=false;$('alignment').hidden=false;
  const data=sourcePixels(sourceImage);blank();$('accept').disabled=true;
  try {
    const figure=segment(data.data,640,480,current.alignment.threshold);
    if(showingRaw||markPoints) {
      ctx.putImageData(data,0,0);
      if(markPoints) {
        ctx.fillStyle='#ffc15d';for(const p of markPoints){ctx.beginPath();ctx.arc(p.x,p.y,7,0,Math.PI*2);ctx.fill();}
        return;
      }
    }
    const ref=await referenceFor(figure.bounds);
    if(version!==workVersion)return;
    const place=placement(figure.bounds,ref,current.alignment,mode,poses[step].id==='portrait');
    const cut=canvas();cut.getContext('2d').putImageData(new ImageData(figure.pixels,640,480),0,0);
    const result=canvas(),rctx=result.getContext('2d');rctx.fillStyle='white';rctx.fillRect(0,0,640,480);rctx.drawImage(cut,place.x,place.y,640*place.scale,480*place.scale);
    current.image=result.toDataURL('image/png');
    if(!showingRaw) {ctx.fillStyle='#44463d';ctx.fillRect(0,0,640,480);ctx.drawImage(cut,place.x,place.y,640*place.scale,480*place.scale);}
    $('accept').disabled=false;message('status','Проверьте контур: глаза, пальцы и хвост должны сохраниться.');
  } catch(e) {if(version===workVersion){ctx.putImageData(data,0,0);message('status',e.message,true);}}
}
async function setSource(data,video=null) {
  sourceImage=await loadImage(data);current={source:data,video,alignment:{zoom:100,shift:0,threshold:38,headWidth:0}};
  showingRaw=false;markPoints=null;syncControls();await render();
}
function snapshot(source) {
  const w=source.videoWidth||source.naturalWidth, h=source.videoHeight||source.naturalHeight;
  if(!w||!h) throw new Error('Камера ещё не готова. Подождите секунду.');
  const ratio=Math.min(1,1280/Math.max(w,h));const c=canvas(Math.round(w*ratio),Math.round(h*ratio));c.getContext('2d').drawImage(source,0,0,c.width,c.height);return c.toDataURL('image/jpeg',.92);
}
function busy(value) {recording=value; for(const id of ['snap','record','photo','video-file','accept','previous','overview','camera','retake']) $(id).disabled=value; $('steps').inert=value;}
async function camera() {
  try {
    if(!navigator.mediaDevices?.getUserMedia) throw new Error('Для камеры с силуэтом на телефоне нужен HTTPS. Здесь можно нажать «Снять / выбрать фото» и совместить кадр с силуэтом после съёмки.');
    stopCamera();stream=await navigator.mediaDevices.getUserMedia({video:{facingMode:{ideal:'environment'},width:{ideal:1280},height:{ideal:960},aspectRatio:{ideal:4/3}},audio:false});
    $('live').srcObject=stream;await $('live').play();$('live').hidden=false;$('preview').hidden=true;$('snap').hidden=false;$('record').hidden=typeof MediaRecorder==='undefined';
    message('status','Камера готова. Уберите руки из кадра перед снимком.');
  } catch(e) {message('status',e.name==='NotAllowedError'?'Доступ к камере закрыт. Разрешите камеру в браузере или выберите фото.':e.message,true);}
}
async function accept() {
  if(!current?.image)return;
  frames[poses[step].id]=structuredClone(current);await saveDraft();
  if(step+1<poses.length) await enterStep(step+1);else await review();
}
async function regenerate() {
  // A retaken stand changes the reference for every body pose. Regenerate all
  // accepted outputs so editing it cannot silently publish mixed scales.
  if(!frames.stand) return;
  const refImg=await loadImage(frames.stand.source),raw=sourcePixels(refImg);
  const refBounds=segment(raw.data,640,480,frames.stand.alignment.threshold).bounds;
  const ref={scale:300/(refBounds.y1-refBounds.y0+1),headWidth:frames.stand.alignment.headWidth};
  for(const p of poses) {
    const f=frames[p.id];if(!f)continue;
    const img=await loadImage(f.source),data=sourcePixels(img),figure=segment(data.data,640,480,f.alignment.threshold);
    const pos=placement(figure.bounds,ref,f.alignment,mode,p.id==='portrait');
    const cut=canvas(),out=canvas();cut.getContext('2d').putImageData(new ImageData(figure.pixels,640,480),0,0);
    const o=out.getContext('2d');o.fillStyle='white';o.fillRect(0,0,640,480);o.drawImage(cut,pos.x,pos.y,640*pos.scale,480*pos.scale);f.image=out.toDataURL('image/png');
  }
}
async function review() {
  stopCamera();show('review');$('review-name').textContent=draftName;message('publish-status','');
  $('publish').disabled=true;
  try {await regenerate();$('publish').disabled=!poses.every(p=>frames[p.id]);await saveDraft();}
  catch(e){message('publish-status',e.message,true);}
  $('gallery').replaceChildren();
  poses.forEach((p,i)=>{const b=document.createElement('button');if(frames[p.id]){const img=document.createElement('img');img.src=frames[p.id].image;img.alt=p.title;b.append(img);}else{const missing=document.createElement('div');missing.className='missing';missing.textContent='Нужно снять';b.append(missing);}const label=document.createElement('span');label.textContent=p.title;b.append(label);b.onclick=()=>enterStep(i);$('gallery').append(b);});
  if(!poses.every(p=>frames[p.id]))message('publish-status','Снимите все позы и отдельный портрет. Нажмите на недостающий кадр.');
  window.scrollTo({top:0,behavior:'smooth'});
}
$('start').onclick=async()=>{
  const name=$('name').value.trim().toUpperCase();
  if(!/^[A-Z][A-Z0-9_]{0,15}$/.test(name)){message('setup-status','Введите имя латинскими буквами, например GROM.',true);$('name').focus();return;}
  if(!poses.length)return;draftName=name;mode=document.querySelector('input[name=mode]:checked').value;frames={};await saveDraft();await enterStep(0);
};
$('resume').onclick=async()=>{const saved=await db('get');if(saved){draftName=saved.name;mode=saved.mode;frames=saved.frames;const missing=poses.findIndex(p=>!frames[p.id]);if(missing<0)await review();else await enterStep(missing);}};
$('camera').onclick=camera;
$('snap').onclick=async()=>{try{await setSource(snapshot($('live')));}catch(e){message('status',e.message,true);}};
$('photo').onchange=async e=>{
  const file=e.target.files[0];if(!file)return;
  try{if(file.size>25*1024*1024)throw new Error('Фото слишком большое. Выберите файл до 25 МБ.');const img=await loadImage(await readFile(file));await setSource(snapshot(img));}
  catch(e){message('status',e.message,true);}finally{e.target.value='';}
};
$('video-file').onchange=async e=>{const file=e.target.files[0];if(!file)return;try{if(file.size>12*1024*1024)throw new Error('Видео слишком большое. Выберите короткий ролик до 12 МБ.');if(clipURL)URL.revokeObjectURL(clipURL);clipBlob=file;clipURL=URL.createObjectURL(file);$('clip').src=clipURL;$('clip-area').hidden=false;}catch(error){message('status',error.message,true);}finally{e.target.value='';}};
$('use-frame').onclick=async()=>{try{if(!clipBlob)return;await setSource(snapshot($('clip')),await readFile(clipBlob));}catch(e){message('status',e.message,true);}};
$('record').onclick=async()=>{
  if(!stream||recording)return;
  let recorder;
  try{
    const mime=['video/mp4','video/webm;codecs=vp8','video/webm'].find(t=>MediaRecorder.isTypeSupported(t));
    recorder=new MediaRecorder(stream,mime?{mimeType:mime,videoBitsPerSecond:2000000}:{videoBitsPerSecond:2000000});
    const chunks=[];recorder.ondataavailable=e=>{if(e.data.size)chunks.push(e.data);};
    const stopped=new Promise((resolve,reject)=>{recorder.onstop=resolve;recorder.onerror=()=>reject(new Error('Не удалось записать видео. Сделайте фото или выберите ролик.'));});
    busy(true);$('countdown').hidden=false;recorder.start();
    for(let n=3;n>0;n--){$('countdown').textContent=n;await new Promise(r=>setTimeout(r,1000));}
    const shot=snapshot($('live'));recorder.stop();await stopped;
    const video=new Blob(chunks,{type:recorder.mimeType.split(';')[0]});
    if(video.size>12*1024*1024)throw new Error('Видео слишком большое. Сделайте короткий ролик.');
    clipBlob=video;if(clipURL)URL.revokeObjectURL(clipURL);clipURL=URL.createObjectURL(video);$('clip').src=clipURL;$('clip-area').hidden=false;
    await setSource(shot,await readFile(video));
  }catch(e){if(recorder?.state==='recording')recorder.stop();message('status',e.message,true);}
  finally{busy(false);$('countdown').hidden=true;await render();}
};
$('mark-head').onclick=()=>{if(!sourceImage)return;markPoints=[];showingRaw=true;$('viewfinder').classList.add('marking');message('status','Коснитесь левого, затем правого края головы (без рогов).');render();$('viewfinder').scrollIntoView({behavior:'smooth',block:'center'});};
$('preview').onclick=async e=>{
  if(!markPoints)return;const r=$('preview').getBoundingClientRect();markPoints.push({x:(e.clientX-r.left)*640/r.width,y:(e.clientY-r.top)*480/r.height});
  if(markPoints.length===2){const width=Math.abs(markPoints[1].x-markPoints[0].x);if(width<8){markPoints=[];message('status','Точки слишком близко. Отметьте всю ширину головы.',true);render();return;}current.alignment.headWidth=width;markPoints=null;showingRaw=false;$('viewfinder').classList.remove('marking');syncControls();}
  await render();
};
for(const id of ['zoom','shift','threshold'])$(id).oninput=()=>{if(current){current.alignment[id]=Number($(id).value);render();}};
$('compare').onclick=()=>{showingRaw=!showingRaw;$('compare').textContent=showingRaw?'Показать результат':'Показать исходник';render();};
$('opacity').oninput=()=>{$('guide').style.opacity=Number($('opacity').value)/100;};
$('retake').onclick=()=>{delete frames[poses[step].id];saveDraft();updateSteps();current=null;sourceImage=null;markPoints=null;$('viewfinder').classList.remove('marking');$('alignment').hidden=true;$('accept').disabled=true;blank();$('live').hidden=!stream;$('preview').hidden=!!stream;message('status','Снимите позу ещё раз или выберите другое фото.');};
$('accept').onclick=accept;$('previous').onclick=()=>{if(step)enterStep(step-1);};$('overview').onclick=review;$('back-shoot').onclick=()=>enterStep(step);
$('publish').onclick=async()=>{
  $('publish').disabled=true;message('publish-status','Готовим анимации и портрет. Проверяем бойца в движке игры…');
  try{await regenerate();const response=await fetch('/api/fighters',{method:'POST',headers:{'Content-Type':'application/json','X-Capture-Token':token},body:JSON.stringify({name:draftName,mode,frames})});const result=await response.json();if(!response.ok)throw new Error(result.error||'Не удалось сохранить бойца');$('done-message').textContent=`${result.name} готов к бою. Все фотографии и записанные видео сохранены.`;show('done');await db('delete');}
  catch(e){message('publish-status',e.message,true);$('publish').disabled=false;}
};
$('another').onclick=()=>{frames={};draftName='';$('name').value='';$('resume').hidden=true;show('setup');};
window.addEventListener('pagehide',()=>stopCamera());
try{
  const response=await fetch('/api/poses',{headers:{'X-Capture-Token':token}}), config=await response.json();
  if(!response.ok)throw new Error(config.error);poses=config.poses;
  try{storage=await openDB();const saved=await db('get');$('resume').hidden=!(saved?.version===1);if(saved)$('resume').textContent=`Продолжить: ${saved.name}`;}catch{message('setup-status','Браузер не сохраняет черновики. Не закрывайте страницу до завершения.');}
}catch(e){message('setup-status',e.message,true);$('start').disabled=true;}
