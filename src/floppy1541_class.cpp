//////////////////////////////////////////////////
//                                              //
// Emu64                                        //
// von Thorsten Kattanek                        //
//                                              //
// #file: floppy1541_class.cpp                  //
//                                              //
// Dieser Sourcecode ist Copyright geschützt!   //
// Geistiges Eigentum von Th.Kattanek           //
//                                              //
// Letzte Änderung am 25.08.2019        		//
// www.emu64.de                                 //
//                                              //
//////////////////////////////////////////////////

#include "./floppy1541_class.h"

#include <QDebug>

const uint8_t Floppy1541::num_sectors[] = {21,21,21,21,21,21,21,21,21,21,21,21,21,21,21,21,21,19,19,19,19,19,19,19,18,18,18,18,18,18,17,17,17,17,17,17,17,17,17,17,17,17};
const uint8_t Floppy1541::d64_track_zone[] = {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
                   1,1,1,1,1,1,1,
                   2,2,2,2,2,2,
                   3,3,3,3,3,3,3,3,3,3};
const uint8_t Floppy1541::d64_sector_gap[] = {12, 21, 16, 13};  // von GPZ Code übermommen imggen
                                                                // 1,10,5,2; // Meine alten Werte



Floppy1541::Floppy1541(bool *reset, int samplerate, int buffersize, bool *floppy_found_breakpoint):
    FloppyEnabled(false)

{
    RESET = reset;
    GCR_PTR = 0;
    breakgroup_count = 0;

    CycleCounter = 0;

    cpu = new MOS6502();
    via1 = new MOS6522(0);
    via2 = new MOS6522(1);

    FoundBreakpoint = floppy_found_breakpoint;

    cpu->ReadProcTbl = ReadProcTbl;
    cpu->WriteProcTbl = WriteProcTbl;
    cpu->History = History;
    cpu->HistoryPointer = &HistoryPointer;
    cpu->Breakpoints = Breakpoints;
    cpu->BreakStatus = &BreakStatus;
    cpu->BreakWerte = BreakWerte;
    HistoryPointer = 0;

    via1->FloppyIEC = &FloppyIECLocal;

    for(int i=0;i<256;i++)
    {
        ReadProcTbl[i] = std::bind(&Floppy1541::ReadNoMem,this,std::placeholders::_1);
        WriteProcTbl[i] = std::bind(&Floppy1541::WriteNoMem,this,std::placeholders::_1,std::placeholders::_2);
    }

    for(int i=0;i<8;i++)
    {
        ReadProcTbl[i] = std::bind(&Floppy1541::ReadRam,this,std::placeholders::_1);
        WriteProcTbl[i] = std::bind(&Floppy1541::WriteRam,this,std::placeholders::_1,std::placeholders::_2);
    }

    for(int i=0;i<64;i++)
    {
         ReadProcTbl[i+0xC0] = std::bind(&Floppy1541::ReadRom,this,std::placeholders::_1);
    }

    ReadProcTbl[0x18] = std::bind(&MOS6522::ReadIO,via1,std::placeholders::_1);
    WriteProcTbl[0x18] = std::bind(&MOS6522::WriteIO,via1,std::placeholders::_1,std::placeholders::_2);

    ReadProcTbl[0x1C] = std::bind(&MOS6522::ReadIO,via2,std::placeholders::_1);
    WriteProcTbl[0x1C] = std::bind(&MOS6522::WriteIO,via2,std::placeholders::_1,std::placeholders::_2);

    via2->SyncFound = std::bind(&Floppy1541::SyncFound,this);
    via2->ReadGCRByte = std::bind(&Floppy1541::ReadGCRByte,this);
    via2->WriteGCRByte = std::bind(&Floppy1541::WriteGCRByte,this,std::placeholders::_1);
    via2->SpurDec = std::bind(&Floppy1541::SpurDec,this);
    via2->SpurInc = std::bind(&Floppy1541::SpurInc,this);

    via1->TriggerInterrupt = std::bind(&MOS6502::TriggerInterrupt,cpu,std::placeholders::_1);
    via1->ClearInterrupt = std::bind(&MOS6502::ClearInterrupt,cpu,std::placeholders::_1);

    via2->TriggerInterrupt = std::bind(&MOS6502::TriggerInterrupt,cpu,std::placeholders::_1);
    via2->ClearInterrupt = std::bind(&MOS6502::ClearInterrupt,cpu,std::placeholders::_1);

    Jumper = 0;
    via1->Jumper = &Jumper;
    WriteProtect = WriteProtectAkt = false;
    via2->WriteProtect = &WriteProtect;
    via2->DiskMotorOn = &DiskMotorOn;

    DiskChangeSimState = 0;

    cpu->RESET = reset;
    via1->RESET = reset;
    via2->RESET = reset;

    for(int i=0;i<(174848);i++) D64Image[i]=0x00;
    D64ImageToGCRImage();
    UnLoadDiskImage();

    AktHalbSpur = 1;

    /// Für Floppysound ///

    Volume = 0.3f;

    FloppySoundsLoaded = false;
    FloppySoundEnabled = false;

    Samplerate = (double)samplerate;
    FreqConvAddWert = ((double)1.0)/((double)985248.0/Samplerate);
    FreqConvCounter = 0.0;
    SoundBufferPos = 0;
    SoundBufferSize = buffersize;
    SoundBuffer = new short[SoundBufferSize*2];
    for(int i=0;i<SoundBufferSize*2;i++) SoundBuffer[i] = 0;
}

Floppy1541::~Floppy1541()
{
    CheckImageWrite();

    FloppySoundEnabled = false;
    FloppyEnabled = false;
    delete SoundBuffer;
    delete cpu;
    delete via1;
    delete via2;
}

void Floppy1541::SetEnableFloppy(bool status)
{
    if((!FloppyEnabled) && (status))
    {
        MotorStatusOld = false;
        DiskMotorOn = false;

        CycleCounter = 0;

        cpu->Reset();
        via1->Reset();
        via2->Reset();

        StepperIncWait = true;

        AktHalbSpur = -1; //1
        GCR_PTR = GCRSpurStart = GCRImage + ((AktHalbSpur)) * GCR_TRACK_SIZE;
        GCRSpurEnde = GCRSpurStart + TrackSize[AktHalbSpur];
    }
    else
    {
        MotorLoopStatus = 0;
        StepperLoopStatus = 0;
    }

    FloppyEnabled = status;
    if(!status)
        FloppyIECLocal = 0xFF;
}

bool Floppy1541::GetEnableFloppy()
{
    return FloppyEnabled;
}

void Floppy1541::SetEnableFloppySound(bool status)
{
    if(status)
    {
        FloppySoundEnabled = status;
    }
    else
    {
        FloppySoundEnabled = status;
        for(int i=0;i<SoundBufferSize;i++) SoundBuffer[i] = 0;
    }
}

bool Floppy1541::GetEnableFloppySound()
{
    return FloppySoundEnabled;
}

void* Floppy1541::GetSoundBuffer()
{
    return SoundBuffer;
}

void Floppy1541::ZeroSoundBufferPos()
{
    SoundBufferPos = 0;
}

void Floppy1541::SetFloppySoundVolume(float_t volume)
{
    Volume = volume;
}

bool Floppy1541::LoadDiskImage(const char* filename)
{
    FILE *file;
    char EXT[4];
    size_t reading_elements;

    int len = (int)strlen(filename);

    strcpy(EXT,filename+len-3); /* FIXME: crash if len < 3 */

    EXT[0]=toupper(EXT[0]);
    EXT[1]=toupper(EXT[1]);
    EXT[2]=toupper(EXT[2]);

    CheckImageWrite();
    strcpy(ImageFileName,filename);

    if(0==strcmp("D64",EXT))
    {
        // Diskwechsel simulieren
        StartDiskChange();

        file = fopen(filename, "rb");
        if (file == NULL)
        {
            return false;
        }

        for(int i=0;i<(665952);i++) GCRImage[i]=0x00;

        reading_elements = fread (D64Image,1,174848,file);
        if(reading_elements != 174848)
        {
            fclose(file);
            return false;
        }

        fclose(file);
        D64ImageToGCRImage();

        GCR_PTR = GCRSpurStart = GCRImage + ((AktHalbSpur)) * GCR_TRACK_SIZE;
        GCRSpurEnde = GCRSpurStart + TrackSize[AktHalbSpur];

        ImageWriteStatus = false;
        ImageTyp = D64;

        SyncFoundCount = 0;

        return true;
    }

    if(0==strcmp("G64",EXT))
    {
        char  kennung[9];
        char  version;
        char  trackanzahl;
        unsigned short  tracksize;
        uint32_t trackpos[84];
        uint32_t trackspeed[84];

        // Diskwechsel simulieren
        StartDiskChange();

        file = fopen(filename, "rb");
        if (file == NULL)
        {
            return false;
        }

        for(int i=0;i<(665952);i++) GCRImage[i]=0x00;

        reading_elements = fread (kennung,1,8,file);
        kennung[8]=0;

        if(0 != strcmp("GCR-1541",kennung))
        {
            //MessageBox(0,"Fehlerhaftes G64 Image","Error!",0);
            return false;
        }

        reading_elements = fread (&version,1,1,file);
        if(version != 0)
        {
            //MessageBox(0,"Dies Version unterstützt nur G64 Images Version 1","Error!",0);
            return false;
        }

        reading_elements = fread (&trackanzahl,1,1,file);
        if(trackanzahl > 84)
        {
            //MessageBox(0,"Das Image enthält zuviele Tracks!","Error!",0);
        }

        reading_elements = fread (&tracksize,1,2,file);
        if(tracksize != 7928)
        {
            //MessageBox(0,"Unbekannte Trackgröße","Error!",0);
        }

        reading_elements = fread (&trackpos,4,84,file);
        reading_elements = fread (&trackspeed,4,84,file);

        for(int i=0;i<trackanzahl;i++)
        {
            if(trackpos[i] != 0)
            {
                fseek(file,trackpos[i],SEEK_SET);
                reading_elements = fread(&TrackSize[i],1,2,file);
                reading_elements = fread(GCRImage+(i*tracksize),1,tracksize-2,file);
            }
        }

        fclose(file);

        ImageWriteStatus = false;
        ImageTyp = G64;

        SyncFoundCount = 0;

        return true;
    }
    return false;
}

void Floppy1541::UnLoadDiskImage()
{
    CheckImageWrite();
    for(int i=0;i<174848;i++) D64Image[i] = 0;
    for(int i=0;i<665952;i++) GCRImage[i] = 0;
    WriteProtect = false;
}

inline void Floppy1541::CheckImageWrite()
{
    FILE *File;

    if(ImageWriteStatus && !WriteProtectAkt)
    {
        switch(ImageTyp)
        {
        case D64:
            GCRImageToD64Image();

            File = fopen (ImageFileName, "wb");
            if(File == NULL)
            {
                //MessageBox(0,"Fehler beim schreiben des Aktuellen Images","Emu64 Fehler",0);
                return;
            }

            fwrite(D64Image,1,174848,File);

            fclose(File);
            break;

        case G64:
            break;
        }
    }
}

inline void Floppy1541::D64ImageToGCRImage()
{
    for (int SPUR=1; SPUR<=35; SPUR++)
    {
        TrackSize[(SPUR-1)*2] = GCR_SECTOR_SIZE*num_sectors[SPUR-1];
        TrackSize[((SPUR-1)*2)+1] = GCR_SECTOR_SIZE*num_sectors[SPUR-1];
        for(int SECTOR=0; SECTOR<num_sectors[SPUR-1];SECTOR++)
        {
            SectorToGCR(SPUR,SECTOR);
        }
    }
    ImageWriteStatus=false;
}

inline void Floppy1541::SectorToGCR(unsigned int spur, unsigned int sektor)
{
    static unsigned char id1 = 0;
    static unsigned char id2 = 0;
    unsigned char block[256];
    unsigned char buffer[4];
    unsigned char *P = GCRImage + ((spur-1)*2) * GCR_TRACK_SIZE + sektor * GCR_SECTOR_SIZE;

    unsigned short TRACK_INDEX[]={0,0,21,42,63,84,105,126,147,168,189,210,231,252,273,294,315,336,357,376,395,414,433,452,471,490,508,526,544,562,580,598,615,632,649,666,683};
    int TEMP;
    TEMP=TRACK_INDEX[spur]+(sektor);
    TEMP*=256;

    for (int z=0;z<256;z++) block[z]=D64Image[TEMP+z];

    // Create GCR header (15 Bytes)
    // SYNC
    *P++ = 0xFF;								// SYNC
    //*P++ = 0xFF;								// SYNC
    //*P++ = 0xFF;								// SYNC
    //*P++ = 0xFF;								// SYNC
    //*P++ = 0xFF;								// SYNC
    buffer[0] = 0x08;							// Header mark
    buffer[1] = sektor ^ spur ^ id2 ^ id1;		// Checksum
    buffer[2] = sektor;
    buffer[3] = spur;
    ConvertToGCR(buffer, P);
    buffer[0] = id2;
    buffer[1] = id1;
    buffer[2] = 0x0F;
    buffer[3] = 0x0F;
    ConvertToGCR(buffer, P+5);
    P += 9;

    // Create GCR data (338 Bytes)
    unsigned char SUM;
    // SYNC
    *P++ = 0xFF;                                // SYNC
    //*P++ = 0xFF;								// SYNC
    //*P++ = 0xFF;								// SYNC
    //*P++ = 0xFF;								// SYNC
    //*P++ = 0xFF;								// SYNC
    buffer[0] = 0x07;							// Data mark
    SUM = buffer[1] = block[0];
    SUM ^= buffer[2] = block[1];
    SUM ^= buffer[3] = block[2];
    ConvertToGCR(buffer, P);
    P += 5;

    for (int i=3; i<255; i+=4)
    {
        SUM ^= buffer[0] = block[i];
        SUM ^= buffer[1] = block[i+1];
        SUM ^= buffer[2] = block[i+2];
        SUM ^= buffer[3] = block[i+3];
        ConvertToGCR(buffer, P);
        P += 5;
    }

    SUM ^= buffer[0] = block[255];
    buffer[1] = SUM;							// Checksum
    buffer[2] = 0;
    buffer[3] = 0;
    ConvertToGCR(buffer, P);
    P += 5;

    unsigned char gap_size = d64_sector_gap[d64_track_zone[spur]];
    memset(P, 0x55, gap_size);							// Gap
}

inline void Floppy1541::ConvertToGCR(unsigned char *source_buffer, unsigned char *destination_buffer)
{
    const unsigned short int GCR_TBL[16] = {0x0a, 0x0b, 0x12, 0x13, 0x0e, 0x0f, 0x16, 0x17,0x09, 0x19, 0x1a, 0x1b, 0x0d, 0x1d, 0x1e, 0x15};
    unsigned short int tmp;

    tmp = (GCR_TBL[*source_buffer >> 4] << 5) | GCR_TBL[*source_buffer & 15];
    *destination_buffer++ = tmp >> 2;
    *destination_buffer = (tmp << 6) & 0xc0;
    source_buffer++;

    tmp = (GCR_TBL[*source_buffer >> 4] << 5) | GCR_TBL[*source_buffer & 15];
    *destination_buffer++ |= (tmp >> 4) & 0x3f;
    *destination_buffer = (tmp << 4) & 0xf0;
    source_buffer++;

    tmp = (GCR_TBL[*source_buffer >> 4] << 5) | GCR_TBL[*source_buffer & 15];
    *destination_buffer++ |= (tmp >> 6) & 0x0f;
    *destination_buffer = (tmp << 2) & 0xfc;
    source_buffer++;

    tmp = (GCR_TBL[*source_buffer >> 4] << 5) | GCR_TBL[*source_buffer & 15];
    *destination_buffer++ |= (tmp >> 8) & 0x03;
    *destination_buffer = (unsigned char)tmp;
}

inline void Floppy1541::GCRImageToD64Image()
{
    for (int spur=1; spur<=35; spur++)
        for(int sector=0; sector<num_sectors[spur-1];sector++)
            GCRToSector(spur,sector);
}

inline void Floppy1541::GCRToSector(unsigned int spur, unsigned int sektor)
{
    const unsigned short TRACK_INDEX[]={0,0,21,42,63,84,105,126,147,168,189,210,231,252,273,294,315,336,357,376,395,414,433,452,471,490,508,526,544,562,580,598,615,632,649,666,683};
    unsigned char BUFFER[4];

    unsigned char *gcr = GCRImage + ((spur-1)*2) * GCR_TRACK_SIZE + sektor * GCR_SECTOR_SIZE;
    unsigned char *d64 = D64Image + ((TRACK_INDEX[spur]+sektor)*256);

    gcr += 11;

    ConvertToD64(gcr,BUFFER);
    *d64++ = BUFFER[1];
    *d64++ = BUFFER[2];
    *d64++ = BUFFER[3];
    gcr += 5;

    for (int i=3; i<255; i+=4)
    {
        ConvertToD64(gcr,BUFFER);
        *d64++ = BUFFER[0];
        *d64++ = BUFFER[1];
        *d64++ = BUFFER[2];
        *d64++ = BUFFER[3];
        gcr += 5;
    }

    ConvertToD64(gcr,BUFFER);
    *d64 = BUFFER[0];
}

inline void Floppy1541::ConvertToD64(unsigned char *source_buffer, unsigned char *destination_buffer)
{
    static unsigned char CONV_TBL[32]={32,32,32,32,32,32,32,32,32,8,0,1,32,12,4,5,32,32,2,3,32,15,6,7,32,9,10,11,32,13,14,32};
    unsigned char GCR5;
    unsigned char TMP1;

    GCR5=source_buffer[0]>>3;
    TMP1=CONV_TBL[GCR5&31]<<4;
    GCR5=(source_buffer[0]<<2)|(source_buffer[1]>>6);
    TMP1|=CONV_TBL[GCR5&31];
    destination_buffer[0]=TMP1;

    GCR5=source_buffer[1]>>1;
    TMP1=CONV_TBL[GCR5&31]<<4;
    GCR5=(source_buffer[1]<<4)|(source_buffer[2]>>4);
    TMP1|=CONV_TBL[GCR5&31];
    destination_buffer[1]=TMP1;

    GCR5=(source_buffer[2]<<1)|(source_buffer[3]>>7);
    TMP1=CONV_TBL[GCR5&31]<<4;
    GCR5=source_buffer[3]>>2;
    TMP1|=CONV_TBL[GCR5&31];
    destination_buffer[2]=TMP1;

    GCR5=(source_buffer[3]<<3)|(source_buffer[4]>>5);
    TMP1=CONV_TBL[GCR5&31]<<4;
    GCR5=source_buffer[4];
    TMP1|=CONV_TBL[GCR5&31];
    destination_buffer[3]=TMP1;
}

void Floppy1541::SetC64IEC(uint8_t *iec)
{
    via1->C64IEC = iec;
}

void Floppy1541::SetDeviceNumber(uint8_t number)
{
    if((number < 8) || (number > 11)) return;
    if(Jumper == (number-8)) return;

    Jumper = number - 8;
    cpu->Reset();
}

void Floppy1541::SetWriteProtect(bool status)
{
    /* qDebug("Set WriteProtect [%d], Status: %d",Jumper, status); */

    WriteProtect = WriteProtectAkt = status;
}

void Floppy1541::GetFloppyInfo(FLOPPY_INFO *fi)
{
    fi->Sektor = RAM[0x19];
    fi->Spur = (AktHalbSpur+1)>>1;

    unsigned char tmp = via2->GetIO_Zero();
    fi->Motor = !!(tmp&4);
    fi->Data = !!(tmp&8);
}

bool Floppy1541::LoadDosRom(const char *filename)
{
    FILE *file;
    file = fopen(filename, "rb");
    if (file == NULL)
    {
        return false;
    }

    if(0x4000 != fread (ROM,1,0x4000,file))
    {
        return false;
    }

    fclose(file);
    return true;
}

int Floppy1541::LoadFloppySounds(const char *motor_sound_filename, const char *motor_on_sound_filename, const char *motor_off_sound_filename, const char *anschlag_sound_filename, const char *stepper_dec_sound_filename, const char *Stepper_inc_sound_filename)
{
    FILE* File;
    size_t reading_elements;

    FloppySoundsLoaded = false;

    File = fopen(motor_on_sound_filename, "rb");
    if (File == NULL)
    {
            return 0x01;
    }

    fseek(File,0,SEEK_END);
    FloppySound00Size = ftell(File)>>1;
    fseek(File,0,SEEK_SET);

    FloppySound00 = new unsigned short[FloppySound00Size];
    reading_elements = fread(FloppySound00,2,FloppySound00Size,File);
    fclose(File);

    if(reading_elements != (unsigned int)FloppySound00Size)
        return 0x01;

    File = fopen(motor_sound_filename, "rb");
    if (File == NULL)
    {
            delete[] FloppySound00;
            return 0x02;
    }

    fseek(File,0,SEEK_END);
    FloppySound01Size = ftell(File)>>1;
    fseek(File,0,SEEK_SET);

    FloppySound01 = new unsigned short[FloppySound01Size];
    reading_elements = fread(FloppySound01,2,FloppySound01Size,File);
    fclose(File);

    File = fopen(motor_off_sound_filename, "rb");
    if (File == NULL)
    {
            delete[] FloppySound00;
            delete[] FloppySound01;
            return 0x03;
    }

    fseek(File,0,SEEK_END);
    FloppySound02Size = ftell(File)>>1;
    fseek(File,0,SEEK_SET);

    FloppySound02 = new unsigned short[FloppySound02Size];
    reading_elements = fread(FloppySound02,2,FloppySound02Size,File);
    fclose(File);

    File = fopen(Stepper_inc_sound_filename, "rb");
    if (File == NULL)
    {
            delete[] FloppySound00;
            delete[] FloppySound01;
            delete[] FloppySound02;
            return 0x04;
    }

    fseek(File,0,SEEK_END);
    FloppySound03Size = ftell(File)>>1;
    fseek(File,0,SEEK_SET);

    FloppySound03 = new unsigned short[FloppySound03Size];
    reading_elements = fread(FloppySound03,2,FloppySound03Size,File);
    fclose(File);

    File = fopen(stepper_dec_sound_filename, "rb");
    if (File == NULL)
    {
            delete[] FloppySound00;
            delete[] FloppySound01;
            delete[] FloppySound02;
            delete[] FloppySound03;
            return 0x05;
    }

    fseek(File,0,SEEK_END);
    FloppySound04Size = ftell(File)>>1;
    fseek(File,0,SEEK_SET);

    FloppySound04 = new unsigned short[FloppySound04Size];
    reading_elements = fread(FloppySound04,2,FloppySound04Size,File);
    fclose(File);

    File = fopen(anschlag_sound_filename, "rb");
    if (File == NULL)
    {
            delete[] FloppySound00;
            delete[] FloppySound01;
            delete[] FloppySound02;
            delete[] FloppySound03;
            delete[] FloppySound04;
            return 0x06;
    }

    fseek(File,0,SEEK_END);
    FloppySound05Size = ftell(File)>>1;
    fseek(File,0,SEEK_SET);

    FloppySound05 = new unsigned short[FloppySound05Size];
    reading_elements = fread(FloppySound05,2,FloppySound05Size,File);
    fclose(File);

    FloppySoundsLoaded = true;
    return 0;
}

void Floppy1541::ResetCycleCounter()
{
    CycleCounter = 0;
}

uint8_t *Floppy1541::GetRamPointer()
{
    return RAM;
}

bool Floppy1541::OneCycle()
{
    if(FloppySoundEnabled)
    {
        if(DiskMotorOn != MotorStatusOld)
        {
            if(DiskMotorOn)
            {
                MotorLoopStatus = 1;
                FloppySound00Pos = 0;
            }
            else
            {
                MotorLoopStatus = 3;
                FloppySound02Pos = 0;
            }
        }
        MotorStatusOld = DiskMotorOn;

        if(StepperInc)
        {
            StepperInc = false;
            StepperLoopStatus = 1;
            FloppySound03Pos = 0;
        }

        if(StepperDec)
        {
            StepperDec = false;
            StepperLoopStatus = 2;
            FloppySound04Pos = 0;
        }

        if(StepperAnschlag)
        {
            StepperAnschlag = false;
            StepperLoopStatus = 3;
            FloppySound05Pos = 0;
        }

        FreqConvCounter+=FreqConvAddWert;
        if(FreqConvCounter>=(double)1.0)
        {
            FreqConvCounter-=(double)1.0;

            if(FloppySoundsLoaded && FloppyEnabled)
                RenderFloppySound();
            else {
                SoundBuffer[SoundBufferPos] = SoundBuffer[SoundBufferPos+1] = 0;
                SoundBufferPos += 2;
                if(SoundBufferPos >= SoundBufferSize*2) SoundBufferPos = 0;
            }
        }
    }

    // Disk Wechsel Simulieren
    if(DiskChangeSimState != 0)
    {
        DiskChangeSimCycleCounter--;
        if(DiskChangeSimCycleCounter == 0)
        {
            DiskChangeSimState--;
            if(DiskChangeSimState != 0)
            {
                WriteProtect = !WriteProtect;
                DiskChangeSimCycleCounter = DISK_CHANGE_STATE_CYCLES;
            }
            else WriteProtect = WriteProtectAkt;
        }
    }

    CycleCounter++;

    via1->OneZyklus();
    via2->OneZyklus();

    if(via2->GetIO_Zero()&4) cpu->SET_SR_BIT6();

    if((VIA1_IRQ == true) || (VIA2_IRQ == true)) IRQ = true;
    else IRQ = false;

    if(!*RESET)
    {
        StepperIncWait = true;
        AktHalbSpur = -1; //1
        GCR_PTR = GCRSpurStart = GCRImage + ((AktHalbSpur)) * GCR_TRACK_SIZE;
        GCRSpurEnde = GCRSpurStart + TrackSize[AktHalbSpur];
    }

    bool ret = cpu->OneZyklus();
    if(BreakStatus != 0) *FoundBreakpoint = true;
    return ret;
}

uint8_t Floppy1541::ReadByte(uint16_t address)
{
    return ReadProcTbl[(address)>>8](address);
}

void Floppy1541::WriteByte(uint16_t address, uint8_t value)
{
    WriteProcTbl[(address)>>8](address,value);
}

void Floppy1541::GetCpuReg(REG_STRUCT *reg,IREG_STRUCT *ireg)
{
    if(!FloppyEnabled) return;
    cpu->GetInterneRegister(ireg);
    ireg->cycle_counter = CycleCounter;
    cpu->GetRegister(reg);
}

void Floppy1541::SetCpuReg(REG_STRUCT *reg)
{
    cpu->SetRegister(reg);
}

void Floppy1541::SetResetReady(bool* ResetReady, uint16_t ResetReadyAdr)
{
    cpu->ResetReady = ResetReady;
    cpu->ResetReadyAdr = ResetReadyAdr;
}

void Floppy1541::WriteNoMem(uint16_t address, uint8_t value)
{
}

uint8_t Floppy1541::ReadNoMem(uint16_t address)
{
    return 0x0;
}

void Floppy1541::WriteRam(uint16_t address, uint8_t value)
{
    RAM[address] = value;
}

uint8_t Floppy1541::ReadRam(uint16_t address)
{
    return RAM[address];
}

uint8_t Floppy1541::ReadRom(uint16_t address)
{
    return ROM[address-0xC000];
}

bool Floppy1541::SyncFound()
{
    // bool found = false;

    if (AktHalbSpur >= ((NUM_TRACKS-1) * 2)) return false;

    // NEU TEST
    /*
    if(*GCR_PTR == 0xFF)
    {
        SyncFoundCount++;
        if(SyncFoundCount == 5)
        {
            found = true;
            GCR_PTR --;
        }
    }
    else
    {
        SyncFoundCount = 0;
        GCR_PTR ++;	// Rotate disk
        if (GCR_PTR == GCRSpurEnde) GCR_PTR = GCRSpurStart;
    }

    return found;
    */

    if(*GCR_PTR == 0xFF)
    {
L1:
        GCR_PTR++;
        if (GCR_PTR == GCRSpurEnde) GCR_PTR = GCRSpurStart;
        if(*GCR_PTR == 0xFF) goto L1;
        GCR_PTR--;
        if(GCR_PTR < GCRSpurStart) GCR_PTR = GCRSpurEnde;
        return true;
    }
    else
    {
        GCR_PTR ++;	// Rotate disk
        if (GCR_PTR == GCRSpurEnde) GCR_PTR = GCRSpurStart;
        return false;
    }
}

uint8_t Floppy1541::ReadGCRByte()
{
    AktGCRWert = *GCR_PTR++;	// Rotate disk
    if (GCR_PTR >= GCRSpurEnde) GCR_PTR = GCRSpurStart;
    return	AktGCRWert;
}

void Floppy1541::WriteGCRByte(uint8_t value)
{
    ImageWriteStatus = true;

    GCR_PTR++;	// Rotate disk
    *GCR_PTR = value;

    if (GCR_PTR == GCRSpurEnde) GCR_PTR = GCRSpurStart;
}

void Floppy1541::SpurInc()
{
    if (AktHalbSpur == ((NUM_TRACKS-1) * 2)) return;

    AktHalbSpur++;
    GCR_PTR = GCRSpurStart = GCRImage + ((AktHalbSpur)) * GCR_TRACK_SIZE;
    GCRSpurEnde = GCRSpurStart + TrackSize[AktHalbSpur];

    if(StepperIncWait)
        StepperIncWait = false;
    else StepperInc = true;
}

void Floppy1541::SpurDec()
{
    if (AktHalbSpur  == 0)
    {
        StepperAnschlag = true;
        return;
    }

    AktHalbSpur--;

    GCR_PTR = GCRSpurStart = GCRImage + ((AktHalbSpur)) * GCR_TRACK_SIZE;
    GCRSpurEnde = GCRSpurStart + TrackSize[AktHalbSpur];

    StepperDec = true;
}

void Floppy1541::RenderFloppySound()
{
    switch(MotorLoopStatus)
    {
    case 0:
        SoundBuffer[SoundBufferPos] = 0;
        break;
    case 1:
        SoundBuffer[SoundBufferPos] = FloppySound00[FloppySound00Pos];
        FloppySound00Pos++;
        if(FloppySound00Pos>=FloppySound00Size)
        {
            FloppySound01Pos = 0;
            MotorLoopStatus = 2;
        }
        break;
    case 2:
        SoundBuffer[SoundBufferPos] = FloppySound01[FloppySound01Pos];
        FloppySound01Pos++;
        if(FloppySound01Pos>=FloppySound01Size) FloppySound01Pos = 0;
        break;
    case 3:
        SoundBuffer[SoundBufferPos] = FloppySound02[FloppySound02Pos];
        FloppySound02Pos++;
        if(FloppySound02Pos>=FloppySound02Size)
        {
            MotorLoopStatus = 0;
        }
        break;
    }

    switch(StepperLoopStatus)
    {
    case 0:
            SoundBuffer[SoundBufferPos] += 0;
            break;
    case 1:
        SoundBuffer[SoundBufferPos] += FloppySound03[FloppySound03Pos];
        FloppySound03Pos++;
        if(FloppySound03Pos>=FloppySound03Size)
        {
            StepperLoopStatus = 0;
        }
        break;
    case 2:
        SoundBuffer[SoundBufferPos] += FloppySound04[FloppySound04Pos];
        FloppySound04Pos++;
        if(FloppySound04Pos>=FloppySound04Size)
        {
            StepperLoopStatus = 0;
        }
        break;
    case 3:
        SoundBuffer[SoundBufferPos] += FloppySound05[FloppySound05Pos];
        FloppySound05Pos++;
        if(FloppySound05Pos>=FloppySound05Size)
        {
            StepperLoopStatus = 0;
        }
        break;
    }

    SoundBuffer[SoundBufferPos] =  (short)(SoundBuffer[SoundBufferPos] * Volume);
    SoundBuffer[SoundBufferPos+1] = SoundBuffer[SoundBufferPos];
    SoundBufferPos += 2;

    //SoundBufferPos++;

    if(SoundBufferPos >= SoundBufferSize*2) SoundBufferPos = 0;
}

void Floppy1541::StartDiskChange()
{
    DiskChangeSimState = DISK_CHANGE_STATE_COUNTS;
    DiskChangeSimCycleCounter = DISK_CHANGE_STATE_CYCLES;
    WriteProtect = !WriteProtect;
}

int16_t Floppy1541::AddBreakGroup()
{
    if(breakgroup_count == MAX_BREAK_GROUP_NUM) return -1;

    BreakGroup[breakgroup_count] = new BREAK_GROUP;
    memset(BreakGroup[breakgroup_count],0,sizeof(BREAK_GROUP));
    BreakGroup[breakgroup_count]->iRZZyklus = 1;
    breakgroup_count ++;
    return breakgroup_count - 1;
}

void Floppy1541::DelBreakGroup(int index)
{
    if(index >= breakgroup_count) return;
    delete BreakGroup[index];
    breakgroup_count--;
    for(int i = index; i < breakgroup_count; i++) BreakGroup[i] = BreakGroup[i+1];
    UpdateBreakGroup();
}

BREAK_GROUP* Floppy1541::GetBreakGroup(int index)
{
    if(index >= breakgroup_count) return 0;
    return BreakGroup[index];
}

void Floppy1541::UpdateBreakGroup()
{
    for(int i=0; i<0x10000;i++) Breakpoints[i] = 0;
    for(int i=0;i<breakgroup_count;i++)
    {
        BREAK_GROUP* bg = BreakGroup[i];
        if(bg->Enable)
        {
            if(bg->bPC) Breakpoints[bg->iPC] |= 1;
            if(bg->bAC) Breakpoints[bg->iAC] |= 2;
            if(bg->bXR) Breakpoints[bg->iXR] |= 4;
            if(bg->bYR) Breakpoints[bg->iYR] |= 8;
            if(bg->bRAdresse) Breakpoints[bg->iRAdresse] |= 16;
            if(bg->bWAdresse) Breakpoints[bg->iWAdresse] |= 32;
            if(bg->bRWert) Breakpoints[bg->iRWert] |= 64;
            if(bg->bWWert) Breakpoints[bg->iWWert] |= 128;
            if(bg->bRZ) Breakpoints[bg->iRZ] |= 256;
            if(bg->bRZZyklus) Breakpoints[bg->iRZZyklus] |= 512;
        }
    }
}

void Floppy1541::DeleteAllBreakGroups()
{
    for(int i=0;i<breakgroup_count;i++)
    {
        delete BreakGroup[i];
    }
    breakgroup_count = 0;
    UpdateBreakGroup();
}

int Floppy1541::GetBreakGroupCount()
{
    return breakgroup_count;
}

int Floppy1541::LoadBreakGroups(const char *filename)
{
    FILE *file;
    char Kennung[10];
    char Version;
    unsigned char Groupanzahl;
    size_t reading_elements;

    DeleteAllBreakGroups();

    file = fopen (filename, "rb");
    if (file == NULL)
    {
            /// Datei konnte nicht geöffnet werden ///
            return -1;
    }

    /// Kennung ///
    reading_elements = fread(Kennung,sizeof(Kennung),1,file);
    if(0 != strcmp("EMU64_BPT",Kennung))
    {
        /// Kein Emu64 Format ///
        fclose(file);
        return -2;
    }

    /// Version ///
    reading_elements = fread(&Version,sizeof(Version),1,file);
    if(Version > 1) return -3;

    /// Groupanzahl ///
    reading_elements = fread(&Groupanzahl,sizeof(Groupanzahl),1,file);
    if(reading_elements != 1)
        return -5;

    if(Groupanzahl == 0) return -4;

    /// Groups ///
    for(int ii=0;ii<Groupanzahl;ii++)
    {
        int i = AddBreakGroup();
        reading_elements = fread(BreakGroup[i]->Name,sizeof(BreakGroup[i]->Name),1,file);
        reading_elements = fread(&BreakGroup[i]->Enable,sizeof(BreakGroup[i]->Enable),1,file);
        reading_elements = fread(&BreakGroup[i]->bPC,sizeof(BreakGroup[i]->bPC),1,file);
        reading_elements = fread(&BreakGroup[i]->iPC,sizeof(BreakGroup[i]->iPC),1,file);
        reading_elements = fread(&BreakGroup[i]->bAC,sizeof(BreakGroup[i]->bAC),1,file);
        reading_elements = fread(&BreakGroup[i]->iAC,sizeof(BreakGroup[i]->iAC),1,file);
        reading_elements = fread(&BreakGroup[i]->bXR,sizeof(BreakGroup[i]->bXR),1,file);
        reading_elements = fread(&BreakGroup[i]->iXR,sizeof(BreakGroup[i]->iXR),1,file);
        reading_elements = fread(&BreakGroup[i]->bYR,sizeof(BreakGroup[i]->bYR),1,file);
        reading_elements = fread(&BreakGroup[i]->iYR,sizeof(BreakGroup[i]->iYR),1,file);
        reading_elements = fread(&BreakGroup[i]->bRAdresse,sizeof(BreakGroup[i]->bRAdresse),1,file);
        reading_elements = fread(&BreakGroup[i]->iRAdresse,sizeof(BreakGroup[i]->iRAdresse),1,file);
        reading_elements = fread(&BreakGroup[i]->bWAdresse,sizeof(BreakGroup[i]->bWAdresse),1,file);
        reading_elements = fread(&BreakGroup[i]->iWAdresse,sizeof(BreakGroup[i]->iWAdresse),1,file);
        reading_elements = fread(&BreakGroup[i]->bRWert,sizeof(BreakGroup[i]->bRWert),1,file);
        reading_elements = fread(&BreakGroup[i]->iRWert,sizeof(BreakGroup[i]->iRWert),1,file);
        reading_elements = fread(&BreakGroup[i]->bWWert,sizeof(BreakGroup[i]->bWWert),1,file);
        reading_elements = fread(&BreakGroup[i]->iWWert,sizeof(BreakGroup[i]->iWWert),1,file);
        reading_elements = fread(&BreakGroup[i]->bRZ,sizeof(BreakGroup[i]->bRZ),1,file);
        reading_elements = fread(&BreakGroup[i]->iRZ,sizeof(BreakGroup[i]->iRZ),1,file);
        reading_elements = fread(&BreakGroup[i]->bRZZyklus,sizeof(BreakGroup[i]->bRZZyklus),1,file);
        reading_elements = fread(&BreakGroup[i]->iRZZyklus,sizeof(BreakGroup[i]->iRZZyklus),1,file);
    }
    return 0;
}

bool Floppy1541::SaveBreakGroups(const char *filename)
{
    FILE *file;
    char Kennung[]  = "EMU64_BPT";
    char Version    = 1;

    file = fopen (filename, "wb");
    if (file == NULL)
    {
            return false;
    }

    /// Kennung ///
    fwrite(Kennung,sizeof(Kennung),1,file);

    /// Version ///
    fwrite(&Version,sizeof(Version),1,file);

    /// Groupanzahl ///
    fwrite(&breakgroup_count,sizeof(breakgroup_count),1,file);

    /// Groups ///
    for(int i=0;i<breakgroup_count;i++)
    {
        fwrite(BreakGroup[i]->Name,sizeof(BreakGroup[i]->Name),1,file);
        fwrite(&BreakGroup[i]->Enable,sizeof(BreakGroup[i]->Enable),1,file);
        fwrite(&BreakGroup[i]->bPC,sizeof(BreakGroup[i]->bPC),1,file);
        fwrite(&BreakGroup[i]->iPC,sizeof(BreakGroup[i]->iPC),1,file);
        fwrite(&BreakGroup[i]->bAC,sizeof(BreakGroup[i]->bAC),1,file);
        fwrite(&BreakGroup[i]->iAC,sizeof(BreakGroup[i]->iAC),1,file);
        fwrite(&BreakGroup[i]->bXR,sizeof(BreakGroup[i]->bXR),1,file);
        fwrite(&BreakGroup[i]->iXR,sizeof(BreakGroup[i]->iXR),1,file);
        fwrite(&BreakGroup[i]->bYR,sizeof(BreakGroup[i]->bYR),1,file);
        fwrite(&BreakGroup[i]->iYR,sizeof(BreakGroup[i]->iYR),1,file);
        fwrite(&BreakGroup[i]->bRAdresse,sizeof(BreakGroup[i]->bRAdresse),1,file);
        fwrite(&BreakGroup[i]->iRAdresse,sizeof(BreakGroup[i]->iRAdresse),1,file);
        fwrite(&BreakGroup[i]->bWAdresse,sizeof(BreakGroup[i]->bWAdresse),1,file);
        fwrite(&BreakGroup[i]->iWAdresse,sizeof(BreakGroup[i]->iWAdresse),1,file);
        fwrite(&BreakGroup[i]->bRWert,sizeof(BreakGroup[i]->bRWert),1,file);
        fwrite(&BreakGroup[i]->iRWert,sizeof(BreakGroup[i]->iRWert),1,file);
        fwrite(&BreakGroup[i]->bWWert,sizeof(BreakGroup[i]->bWWert),1,file);
        fwrite(&BreakGroup[i]->iWWert,sizeof(BreakGroup[i]->iWWert),1,file);
        fwrite(&BreakGroup[i]->bRZ,sizeof(BreakGroup[i]->bRZ),1,file);
        fwrite(&BreakGroup[i]->iRZ,sizeof(BreakGroup[i]->iRZ),1,file);
        fwrite(&BreakGroup[i]->bRZZyklus,sizeof(BreakGroup[i]->bRZZyklus),1,file);
        fwrite(&BreakGroup[i]->iRZZyklus,sizeof(BreakGroup[i]->iRZZyklus),1,file);
    }

    fclose(file);
    return true;
}

bool Floppy1541::CheckBreakpoints()
{
    int BreaksIO = 0;

    for (int i=0;i<breakgroup_count;i++)
    {
        BREAK_GROUP* bg = BreakGroup[i];
        int count1 = 0;
        int count2 = 0;

        if(bg->Enable)
        {
            if(bg->bPC)
            {
                count1++;
                if((BreakStatus&1) && (BreakWerte[0] == bg->iPC)) count2++;
            }
            if(bg->bAC)
            {
                count1++;
                if((BreakStatus&2) && (BreakWerte[1] == bg->iAC)) count2++;
            }
            if(bg->bXR)
            {
                count1++;
                if((BreakStatus&4) && (BreakWerte[2] == bg->iXR)) count2++;
            }
            if(bg->bYR)
            {
                count1++;
                if((BreakStatus&8) && (BreakWerte[3] == bg->iYR)) count2++;
            }
            if(bg->bRAdresse)
            {
                count1++;
                if((BreakStatus&16) && (BreakWerte[4] == bg->iRAdresse)) count2++;
            }
            if(bg->bWAdresse)
            {
                count1++;
                if((BreakStatus&32) && (BreakWerte[5] == bg->iWAdresse)) count2++;
            }
            if(bg->bRWert)
            {
                count1++;
                if((BreakStatus&64) && (BreakWerte[6] == bg->iRWert)) count2++;
            }
            if(bg->bWWert)
            {
                count1++;
                if((BreakStatus&128) && (BreakWerte[7] == bg->iWWert)) count2++;
            }
        }
        if((count1 == count2) && (count1 > 0))
        {
            BreakGroup[i]->bTrue = true;
            BreaksIO++;
        }
        else BreakGroup[i]->bTrue = false;
    }
    BreakStatus = 0;

    if(BreaksIO > 0)
    {
        return true;
    }
    else return false;
}
