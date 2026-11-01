// CAGE board peripherals and ROM bootstrap for offline effect decoding.
#include "cage_dsp.hpp"
static std::vector<uint8_t> file(const char *dir,const char *name) {
 char path[1024];snprintf(path,sizeof path,"%s/%s",dir,name);FILE *f=fopen(path,"rb");if(!f)fatalerror("Missing %s\n",path);
 fseek(f,0,SEEK_END);long n=ftell(f);fseek(f,0,SEEK_SET);std::vector<uint8_t> v(n);if(fread(v.data(),1,n,f)!=(size_t)n)abort();fclose(f);return v;
}
static uint32_t le(const uint8_t *p){return p[0]|p[1]<<8|p[2]<<16|uint32_t(p[3])<<24;}
void tms320c3x_device::boot_load(const char *dir) {
 boot=file(dir,"136102-1078a.11a");auto lo=file(dir,"136102-0075"),hi=file(dir,"136102-0077");if(boot.size()!=0x80000 || lo.size()!=0x200000 || hi.size()!=lo.size())fatalerror("Invalid CAGE ROM size\n");
 rom.resize(lo.size()/2);
 for(size_t i=0;i<rom.size();i++)rom[i]=lo[i*2]|lo[i*2+1]<<8|hi[i*2]<<16|uint32_t(hi[i*2+1])<<24;
 size_t p=8;uint32_t entry=0;while(p+8<=boot.size()){
  uint32_t n=le(&boot[p]),address=le(&boot[p+4]);p+=8;if(!n)break;if(!entry)entry=address;

  if(n>0x10000 || p+size_t(n)*4>boot.size())abort();
  for(unsigned i=0;i<n;i++){WMEM(address+i,le(&boot[p]));p+=4;}
 }
 m_pc=entry;m_bkmask=0;irq(1,false);
}
uint32_t tms320c3x_device::ROPCODE(offs_t a){return RMEM(a);}
uint32_t tms320c3x_device::RMEM(offs_t a) {
 a&=0xffffff;
 // TI bootstrap interrupt vectors dispatch to the program jump table.
 if(a>=1 && a<=12)return 0x809fc0+a;
 if(a<0x10000)return ram[a];
 if(a>=0x809800 && a<0x80a000)return internal[a-0x809800];
 if(a>=0xd00000 && a<0xd00000+rom.size())return rom[a-0xd00000];
 if(a>=0x400000 && a<0x400000+boot.size())return boot[a-0x400000];
 if(a>=0x808000 && a<=0x8080ff){unsigned n=a&255;uint32_t v=io[n]&65535;if(n==0)v=(v&~12)|(dma_deadline>=0?12:0);return v;}
 if(a==0xa00000){pending=false;m_r[TMR_IOF].i32[0]&=~8;irq(0,false);return command;}
 return 0;
}
void tms320c3x_device::WMEM(offs_t a,uint32_t data) {
 a&=0xffffff;
 if(a<0x10000){ram[a]=data;return;}
 if(a>=0x809800 && a<0x80a000){internal[a-0x809800]=data;return;}
 if(a==0xa00000){m_r[TMR_IOF].i32[0]|=0x80;ack_deadline=cycles+1000;return;}
 if(a>=0x808000 && a<=0x8080ff){
  unsigned n=a&255;io[n]=data;
  if(n==0 || n==4 || n==6 || n==8)dma();
  if(n==0x20 || n==0x28 || n==0x30 || n==0x38){int t=n>=0x30?1:0;if((io[0x20+t*16]&0xc0)==0xc0){if(timer_deadline[t]<0)timer_deadline[t]=cycles+2*io[0x28+t*16];}else timer_deadline[t]=-1;}
 }
}
void tms320c3x_device::irq(int n,bool set) {
 if(set)m_irq_state|=1<<n;else m_irq_state&=~(1<<n);
 if(n<4 && set)m_r[TMR_IF].i32[0]|=1<<n;
 if(n>=4 && set)m_r[TMR_IF].i32[0]|=1<<n;
 if(n<4 && !set)m_r[TMR_IF].i32[0]&=~(1<<n);
 check_irqs();
}
void tms320c3x_device::command_write(int n){command=n;pending=true;m_r[TMR_IOF].i32[0]|=8;irq(0,true);}
void tms320c3x_device::dma(){
 bool enabled=(io[0]&3)==3 && io[8]!=0;
 if(!enabled){dma_deadline=-1;return;}
 if(dma_deadline>=0)return;
 unsigned period=(io[0x40]&4?4:2)*(io[0x46]&65535)*8*(((io[0x40]>>18)&3)+1);if(!period)period=1;
 rate=16934400/period/4; // Four interleaved DAC channels on original CAGE.
 unsigned addr=io[4],inc=io[0]>>4&1;
 for(unsigned i=0;i<io[8];i++){pcm.push_back((int16_t)RMEM(addr));addr+=inc;}
 dma_end=addr;dma_deadline=cycles+(int64_t)period*io[8];
}
void tms320c3x_device::run(int count){
 int64_t end=cycles+count;
 while(cycles<end){
  if(ack_deadline>=0 && cycles>=ack_deadline){m_r[TMR_IOF].i32[0]&=~0x80;ack_deadline=-1;}
  if(dma_deadline>=0 && cycles>=dma_deadline){io[8]=0;io[4]=dma_end;dma_deadline=-1;irq(10,true);}
  for(int t=0;t<2;t++)if(timer_deadline[t]>=0 && cycles>=timer_deadline[t]){timer_deadline[t]=cycles+2*io[0x28+t*16];irq(8+t,true);}
  check_irqs();
  if(m_is_idling){cycles++;continue;}
  if((m_r[TMR_ST].i32[0]&RMFLAG) && m_pc==m_r[TMR_RE].i32[0]+1){
   if((int32_t)--m_r[TMR_RC].i32[0]>=0)m_pc=m_r[TMR_RS].i32[0];else {m_r[TMR_ST].i32[0]&=~RMFLAG;if(m_delayed){m_delayed=false;if(m_irq_pending){m_irq_pending=false;check_irqs();}}}
   continue;
  }
  m_icount=1000;execute_one();cycles+=1000-m_icount;
 }
}
