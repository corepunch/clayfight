// Pure pixel operations, also exercised by tests/capture-processing.mjs.
// Background flood-fill preserves pale eyes inside the clay silhouette.
export function segment(rgba, width, height, threshold = 38) {
  const count = width * height, border = [[], [], []];
  for (let i = 0; i < 80; i++) {
    const x = Math.floor((i % 20) * (width - 1) / 19);
    const y = Math.floor((i % 20) * (height - 1) / 19);
    const p = i < 20 ? x : i < 40 ? (height - 1) * width + x : i < 60 ? y * width : y * width + width - 1;
    for (let c = 0; c < 3; c++) border[c].push(rgba[p * 4 + c]);
  }
  const bg = border.map(values => values.sort((a,b) => a-b)[40]);
  const seen = new Uint8Array(count), queue = new Int32Array(count);
  let start = 0, end = 0;
  function isBackground(p) {
    if (rgba[p * 4 + 3] < 128) return true;
    const r = rgba[p*4] - bg[0], g = rgba[p*4+1] - bg[1], b = rgba[p*4+2] - bg[2];
    return Math.hypot((r-g)*.75, (b-g)*.75, (r+g+b)/6) < threshold;
  }
  function add(p) { if (!seen[p] && isBackground(p)) { seen[p] = 1; queue[end++] = p; } }
  for (let x=0; x<width; x++) { add(x); add((height-1)*width+x); }
  for (let y=0; y<height; y++) { add(y*width); add(y*width+width-1); }
  while (start < end) {
    const p = queue[start++], x = p % width;
    if (x) add(p-1); if (x+1<width) add(p+1);
    if (p>=width) add(p-width); if (p+width<count) add(p+width);
  }
  // Keep the main connected clay figure; dust outside it is discarded.
  const mask = new Uint8Array(count);
  let largest = [];
  for (let p=0; p<count; p++) {
    if (seen[p]) continue;
    start = 0; end = 1; queue[0] = p; seen[p] = 1;
    function visit(q) { if (!seen[q]) { seen[q]=1; queue[end++]=q; } }
    while (start<end) {
      const q=queue[start++], x=q%width;
      if(x) visit(q-1); if(x+1<width) visit(q+1);
      if(q>=width) visit(q-width); if(q+width<count) visit(q+width);
    }
    if(end>largest.length) largest=queue.slice(0,end);
  }
  if (largest.length < count * .006) throw new Error('Фигурка не найдена. Выберите контрастный однотонный фон или уменьшите отделение фона.');
  let x0=width,y0=height,x1=0,y1=0;
  for(const p of largest) { mask[p]=1; const x=p%width,y=Math.floor(p/width); x0=Math.min(x0,x);x1=Math.max(x1,x);y0=Math.min(y0,y);y1=Math.max(y1,y); }
  if (x0<3 || y0<3 || x1>width-4 || y1>height-4) throw new Error('Фигурка касается края кадра. Отодвиньте камеру: нужны целые ноги, голова, руки и хвост.');
  let footSum=0, footCount=0;
  for(const p of largest) if(Math.floor(p/width)>y1-Math.max(2,(y1-y0)/16)) { footSum+=p%width; footCount++; }
  const clean = new Uint8ClampedArray(rgba);
  for(let p=0; p<count; p++) if(!mask[p]) clean[p*4+3]=0;
  return {pixels:clean, bounds:{x0,y0,x1,y1,foot:footCount?footSum/footCount:(x0+x1)/2}, area:largest.length};
}

export function placement(bounds, reference, alignment, mode, portrait=false) {
  const head = alignment.headWidth || 0;
  if(mode==='handheld' && !portrait && head<8) throw new Error('Отметьте две точки по краям головы — это линейка для съёмки с рук.');
  let scale = portrait ? Math.min(390/(bounds.x1-bounds.x0+1),360/(bounds.y1-bounds.y0+1)) : reference.scale;
  if(mode==='handheld' && !portrait) scale *= reference.headWidth / head;
  scale *= (alignment.zoom || 100)/100;
  const x = (portrait ? 320-(bounds.x0+bounds.x1)*scale/2 : 320-bounds.foot*scale) + (alignment.shift||0);
  const y = 430-bounds.y1*scale;
  if(x+bounds.x0*scale<12 || x+bounds.x1*scale>628 || y+bounds.y0*scale<12)
    throw new Error('После выравнивания поза выходит за границы. Уменьшите масштаб или оставьте больше места при съёмке.');
  return {scale,x,y};
}
