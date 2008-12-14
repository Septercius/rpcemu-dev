/*Win32 CD-ROM support via IOCTL*/

#include <windows.h>
#include <io.h>
#include "ddk/ntddcdrm.h"
#include "rpcemu.h"
#include "ide.h"
#include "arm.h"

ATAPI ioctl_atapi;

int ioctl_inited=0;
char ioctl_path[8];
void ioctl_close();
HANDLE hIOCTL;
CDROM_TOC toc;
int tocvalid=0;
#define MSFtoLBA(m,s,f)  (((((m*60)+s)*75)+f)-150)

int ioctl_open(char d);

void ioctl_playaudio(uint32_t pos, uint32_t len)
{
        uint32_t addr=pos+150;
        CDROM_PLAY_AUDIO_MSF msf;
        long size;
        msf.StartingF=(uint8_t)(addr%75); addr/=75;
        msf.StartingS=(uint8_t)(addr%60); addr/=60;
        msf.StartingM=(uint8_t)(addr);
        addr=pos+len+150;
        msf.EndingF=(uint8_t)(addr%75); addr/=75;
        msf.EndingS=(uint8_t)(addr%60); addr/=60;
        msf.EndingM=(uint8_t)(addr);
        ioctl_open(0);
        DeviceIoControl(hIOCTL,IOCTL_CDROM_PLAY_AUDIO_MSF,&msf,sizeof(msf),NULL,0,&size,NULL);
        ioctl_close();
}

void ioctl_seek(uint32_t pos)
{
        long size;
        rpclog("%08X ",pos);
        pos+=150;
        CDROM_SEEK_AUDIO_MSF msf;
        msf.F=(uint8_t)(pos%75); pos/=75;
        msf.S=(uint8_t)(pos%60); pos/=60;
        msf.M=(uint8_t)(pos);
        rpclog("Seek to %02i:%02i:%02i\n",msf.M,msf.S,msf.F);
        ioctl_open(0);
        DeviceIoControl(hIOCTL,IOCTL_CDROM_SEEK_AUDIO_MSF,&msf,sizeof(msf),NULL,0,&size,NULL);
        ioctl_close();
}

int ioctl_ready()
{
        long size;
        int temp;
        CDROM_TOC ltoc;
        ioctl_open(0);
        temp=DeviceIoControl(hIOCTL,IOCTL_CDROM_READ_TOC, NULL,0,&ltoc,sizeof(ltoc),&size,NULL);
        ioctl_close();
        if ((ltoc.TrackData[ltoc.LastTrack].Address[1] != toc.TrackData[toc.LastTrack].Address[1]) ||
            (ltoc.TrackData[ltoc.LastTrack].Address[2] != toc.TrackData[toc.LastTrack].Address[2]) ||
            (ltoc.TrackData[ltoc.LastTrack].Address[3] != toc.TrackData[toc.LastTrack].Address[3]) ||
            !tocvalid)
        {
                atapi_discchanged();
                ioctl_open(0);
                temp=DeviceIoControl(hIOCTL,IOCTL_CDROM_READ_TOC, NULL,0,&toc,sizeof(toc),&size,NULL);
                ioctl_close();
                tocvalid=1;
                return 0;
        }
        return 1;
//        return (temp)?1:0;
}

uint8_t ioctl_getcurrentsubchannel(uint8_t *b, int msf)
{
	CDROM_SUB_Q_DATA_FORMAT insub;
	SUB_Q_CHANNEL_DATA sub;
	long size;
	int pos=0;
	int c;
	uint32_t temp;
	insub.Format = IOCTL_CDROM_CURRENT_POSITION;
        ioctl_open(0);
        DeviceIoControl(hIOCTL,IOCTL_CDROM_READ_Q_CHANNEL,&insub,sizeof(insub),&sub,sizeof(sub),&size,NULL);
        ioctl_close();
        b[pos++]=sub.CurrentPosition.Control;
        b[pos++]=sub.CurrentPosition.TrackNumber;
        b[pos++]=sub.CurrentPosition.IndexNumber;
        if (msf)
        {
                for (c=0;c<4;c++) b[pos++]=sub.CurrentPosition.AbsoluteAddress[c];
                for (c=0;c<4;c++) b[pos++]=sub.CurrentPosition.TrackRelativeAddress[c];
        }
        else
        {
                temp=MSFtoLBA(sub.CurrentPosition.AbsoluteAddress[1],sub.CurrentPosition.AbsoluteAddress[2],sub.CurrentPosition.AbsoluteAddress[3]);
                b[pos++]=temp>>24;
                b[pos++]=temp>>16;
                b[pos++]=temp>>8;
                b[pos++]=temp;
                temp=MSFtoLBA(sub.CurrentPosition.TrackRelativeAddress[1],sub.CurrentPosition.TrackRelativeAddress[2],sub.CurrentPosition.TrackRelativeAddress[3]);
                b[pos++]=temp>>24;
                b[pos++]=temp>>16;
                b[pos++]=temp>>8;
                b[pos++]=temp;
        }
        return sub.CurrentPosition.Header.AudioStatus;
}

void ioctl_eject()
{
        long size;
        ioctl_open(0);
        DeviceIoControl(hIOCTL,IOCTL_STORAGE_EJECT_MEDIA,NULL,0,NULL,0,&size,NULL);
        ioctl_close();
}

void ioctl_load()
{
        long size;
        ioctl_open(0);
        DeviceIoControl(hIOCTL,IOCTL_STORAGE_LOAD_MEDIA,NULL,0,NULL,0,&size,NULL);
        ioctl_close();
}

void ioctl_pause()
{
        long size;
        ioctl_open(0);
        DeviceIoControl(hIOCTL,IOCTL_CDROM_PAUSE_AUDIO,NULL,0,NULL,0,&size,NULL);
        ioctl_close();
}

void ioctl_resume()
{
        long size;
        ioctl_open(0);
        DeviceIoControl(hIOCTL,IOCTL_CDROM_RESUME_AUDIO,NULL,0,NULL,0,&size,NULL);
        ioctl_close();
}

void ioctl_readsector(uint8_t *b, int sector)
{
        LARGE_INTEGER pos;
        long size;

        pos.QuadPart=sector*2048;
        ioctl_open(0);
        SetFilePointer(hIOCTL,pos.LowPart,&pos.HighPart,FILE_BEGIN);
        ReadFile(hIOCTL,b,2048,&size,NULL);
        ioctl_close();
}

int ioctl_readtoc(unsigned char *b, unsigned char starttrack, int msf)
{
        int len=4;
        long size;
        int c,d;
        uint32_t temp;
        ioctl_open(0);
        DeviceIoControl(hIOCTL,IOCTL_CDROM_READ_TOC, NULL,0,&toc,sizeof(toc),&size,NULL);
        ioctl_close();
        tocvalid=1;
        rpclog("Read TOC done!\n");
        b[2]=toc.FirstTrack;
        b[3]=toc.LastTrack;
        d=0;
        for (c=0;c<=toc.LastTrack;c++)
        {
                if (toc.TrackData[c].TrackNumber>=starttrack)
                {
                        d=c;
                        break;
                }
        }
        b[2]=toc.TrackData[c].TrackNumber;
        for (c=d;c<=toc.LastTrack;c++)
        {
//                rpclog("Track %02X - %02X %02X %i %i %i %i %08X\n",toc.TrackData[c].TrackNumber,toc.TrackData[c].Adr,toc.TrackData[c].Control,toc.TrackData[c].Address[0],toc.TrackData[c].Address[1],toc.TrackData[c].Address[2],toc.TrackData[c].Address[3],MSFtoLBA(toc.TrackData[c].Address[1],toc.TrackData[c].Address[2],toc.TrackData[c].Address[3]));
                b[len++]=0; /*Reserved*/
                b[len++]=(toc.TrackData[c].Adr<<4)|toc.TrackData[c].Control;
                b[len++]=toc.TrackData[c].TrackNumber;
                b[len++]=0; /*Reserved*/
                if (msf)
                {
                        b[len++]=toc.TrackData[c].Address[0];
                        b[len++]=toc.TrackData[c].Address[1];
                        b[len++]=toc.TrackData[c].Address[2];
                        b[len++]=toc.TrackData[c].Address[3];
                }
                else
                {
                        temp=MSFtoLBA(toc.TrackData[c].Address[1],toc.TrackData[c].Address[2],toc.TrackData[c].Address[3]);
                        b[len++]=temp>>24;
                        b[len++]=temp>>16;
                        b[len++]=temp>>8;
                        b[len++]=temp;
                }
        }
        b[0] = (uint8_t)(((len-4) >> 8) & 0xff);
        b[1] = (uint8_t)((len-4) & 0xff);
/*        rpclog("Table of Contents (%i bytes) : \n",size);
        rpclog("First track - %02X\n",toc.FirstTrack);
        rpclog("Last  track - %02X\n",toc.LastTrack);
        for (c=0;c<=toc.LastTrack;c++)
            rpclog("Track %02X - number %02X control %02X adr %02X address %02X %02X %02X %02X\n",c,toc.TrackData[c].TrackNumber,toc.TrackData[c].Control,toc.TrackData[c].Adr,toc.TrackData[c].Address[0],toc.TrackData[c].Address[1],toc.TrackData[c].Address[2],toc.TrackData[c].Address[3]);
        for (c=0;c<=toc.LastTrack;c++)
            rpclog("Track %02X - number %02X control %02X adr %02X address %06X\n",c,toc.TrackData[c].TrackNumber,toc.TrackData[c].Control,toc.TrackData[c].Adr,MSFtoLBA(toc.TrackData[c].Address[1],toc.TrackData[c].Address[2],toc.TrackData[c].Address[3]));*/
        return len;
}

void ioctl_stop()
{
        long size;
        ioctl_open(0);
        DeviceIoControl(hIOCTL,IOCTL_CDROM_STOP_AUDIO,NULL,0,NULL,0,&size,NULL);
        ioctl_close();
}

int ioctl_open(char d)
{
//        char s[8];
        if (!ioctl_inited)
        {
                sprintf(ioctl_path,"\\\\.\\%c:",d);
                rpclog("Path is %s\n",ioctl_path);
                tocvalid=0;
        }
        rpclog("Opening %s\n",ioctl_path);
	hIOCTL	= CreateFile(/*"\\\\.\\g:"*/ioctl_path,GENERIC_READ,FILE_SHARE_READ,NULL,OPEN_EXISTING,0,NULL);
	if (!hIOCTL)
	{
                error("IOCTL");
                dumpregs();
                exit(-1);
        }
        atapi=&ioctl_atapi;
        ioctl_inited=1;
        return 0;
}

void ioctl_close()
{
        CloseHandle(hIOCTL);
}

void ioctl_exit()
{
        ioctl_inited=0;
        tocvalid=0;
}

ATAPI ioctl_atapi=
{
        ioctl_ready,
        ioctl_readtoc,
        ioctl_getcurrentsubchannel,
        ioctl_readsector,
        ioctl_playaudio,
        ioctl_seek,
        ioctl_load,
        ioctl_eject,
        ioctl_pause,
        ioctl_resume,
        ioctl_stop,
        ioctl_exit
};
