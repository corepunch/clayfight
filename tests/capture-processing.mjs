// Run with node tests/capture-processing.mjs
import assert from 'node:assert/strict';
import {segment,placement} from '../tools/capture/processing.mjs';
const w=640,h=480,px=new Uint8ClampedArray(w*h*4).fill(255);
const rect=(x0,y0,x1,y1,color)=>{for(let y=y0;y<y1;y++)for(let x=x0;x<x1;x++)px.set([...color,255],(y*w+x)*4);};
rect(270,100,370,430,[190,40,55]);rect(300,125,315,140,[255,255,255]); // pale eye enclosed by red clay
rect(355,220,490,250,[190,40,55]); // extended attack
rect(20,20,22,22,[30,30,30]); // background dust
const cut=segment(px,w,h);
assert.equal(cut.bounds.x1,489);assert.equal(cut.bounds.y1,429);
assert.equal(cut.pixels[(130*w+305)*4+3],255,'white eye remains opaque');
assert.equal(cut.pixels[(20*w+20)*4+3],0,'dust is discarded');
assert.equal(cut.pixels[3],0,'backdrop is transparent');
const ref={scale:300/330,headWidth:80};
const a=placement(cut.bounds,ref,{zoom:100,headWidth:80},'handheld');
const smaller={...cut.bounds,x0:135,x1:244,y0:50,y1:214,foot:160};
const b=placement(smaller,ref,{zoom:100,headWidth:40},'handheld');
assert.equal(a.scale*2,b.scale,'handheld head measurement compensates for camera distance');
assert.equal(a.y+cut.bounds.y1*a.scale,430,'feet sit on the common ground');
assert.equal(b.y+smaller.y1*b.scale,430,'smaller capture has the same ground');
assert.throws(()=>placement(cut.bounds,ref,{zoom:100},'handheld'),/две точки/);
assert.throws(()=>segment(new Uint8ClampedArray(w*h*4).fill(255),w,h),/не найдена/);
console.log('capture processing: background removal, white eyes, dust, full punch, shared ground and handheld scale passed');
