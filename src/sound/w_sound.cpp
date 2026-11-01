// Decode arcade sound commands through the original CAGE DSP program.
// The samples are cached in memory; the only source data is the three chips.
#include "cage_dsp.hpp"
#include <memory>
extern "C" {
#include "platform/platform.h"
}
static std::unique_ptr<tms320c3x_device> ready;
static sfxdata_t samples[512];
static char directory[768];
extern "C" void W_SoundDirectory(const char *path)
{
    snprintf(directory,sizeof directory,"%s",path);
}
extern "C" const sfxdata_t *W_ArcadeSound(int command)
{
    if(command<0x8020 || command>=0x8200)return nullptr;
    sfxdata_t *sample=&samples[command-0x8000];
    if(sample->data)return sample;
    if(!ready){
        ready=std::make_unique<tms320c3x_device>();ready->boot_load(directory);
        ready->run(16934400/2);
        ready->command_write(0x8006);ready->run(16934400/20);
        ready->command_write(160);ready->run(16934400/20);
        ready->pcm.clear();
    }
    auto dsp=std::make_unique<tms320c3x_device>(*ready);
    dsp->command_write(command);
    bool audible=false;
    int silence=0;
    for(int tick=0;tick<40;tick++){
        size_t start=dsp->pcm.size();dsp->run(16934400/10);
        int peak=0;
        for(size_t i=start;i<dsp->pcm.size();i++)peak=std::max(peak,abs(int(dsp->pcm[i])));
        if(peak>64){audible=true;silence=0;}else silence++;
        if(audible && silence>=3)break;
    }
    if(!audible)I_Error("Arcade sound command %#x produced no audio",command);
    std::vector<uint8_t> mono;
    for(size_t i=0;i+3<dsp->pcm.size();i+=4){
        int v=0;for(int ch=0;ch<4;ch++)v+=dsp->pcm[i+ch];
        mono.push_back(std::clamp(128+v/512,0,255));
    }
    size_t first=0,last=mono.size();
    while(first<last && mono[first]==128)first++;
    while(last>first && mono[last-1]==128)last--;
    sample->length=int(last-first);sample->rate=dsp->rate;
    byte *data=static_cast<byte *>(I_Malloc(sample->length));
    memcpy(data,mono.data()+first,sample->length);sample->data=data;
    return sample;
}
