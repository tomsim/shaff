/* shaff.c - simple LZ-like archiver (10/06/2013 - 10/10/2013)

   Part of NedoPC SDK (software development kit for simple devices)

   Copyright (C) 2013, Alexander A. Shabarshin <ashabarshin@gmail.com>

   This program is free software: you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation, either version 3 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program. If not, see <http://www.gnu.org/licenses/>.
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define DEBUG
#define HUFF257
#define VERSION "1.0"
#define COR16K 16384/32*16384
#define MAXELE 10000

unsigned long cor_table[COR16K];
short one_block[16384];
struct one_ele { short address, offset, size; } elems[MAXELE];
int cur_ele = 0;
int f_test = 0;
int f_sna = 0;
unsigned char sna_header[27];

/* Description of SNA format from WorldOfSpectrum web-site:

   Offset   Size   Description
   ------------------------------------------------------------------------
   0        1      byte   I
   1        8      word   HL',DE',BC',AF'
   9        10     word   HL,DE,BC,IY,IX
   19       1      byte   Interrupt (bit 2 contains IFF2, 1=EI/0=DI)
   20       1      byte   R
   21       4      words  AF,SP
   25       1      byte   IntMode (0=IM0/1=IM1/2=IM2)
   26       1      byte   BorderColor (0..7, not used by Spectrum 1.7)
   27       49152  bytes  RAM dump 16384..65535
   ------------------------------------------------------------------------
   Total: 49179 bytes

   Source: http://www.worldofspectrum.org/faq/reference/formats.htm
*/

int left_zero(unsigned long u)
{
 return ((u&0x80000000UL)==0)?1:0;
}

int right_zero(unsigned long u)
{
 return ((u&0x00000001UL)==0)?1:0;
}

int all_ones(unsigned long u)
{
 return (u==0xFFFFFFFFUL)?1:0;
}

int left_ones(unsigned long u)
{
 if((u&0x80000000UL)!=0x80000000UL) return 0;
 if((u&0xC0000000UL)!=0xC0000000UL) return 1;
 if((u&0xE0000000UL)!=0xE0000000UL) return 2;
 if((u&0xF0000000UL)!=0xF0000000UL) return 3;
 if((u&0xF8000000UL)!=0xF8000000UL) return 4;
 if((u&0xFC000000UL)!=0xFC000000UL) return 5;
 if((u&0xFE000000UL)!=0xFE000000UL) return 6;
 if((u&0xFF000000UL)!=0xFF000000UL) return 7;
 if((u&0xFF800000UL)!=0xFF800000UL) return 8;
 if((u&0xFFC00000UL)!=0xFFC00000UL) return 9;
 if((u&0xFFE00000UL)!=0xFFE00000UL) return 10;
 if((u&0xFFF00000UL)!=0xFFF00000UL) return 11;
 if((u&0xFFF80000UL)!=0xFFF80000UL) return 12;
 if((u&0xFFFC0000UL)!=0xFFFC0000UL) return 13;
 if((u&0xFFFE0000UL)!=0xFFFE0000UL) return 14;
 if((u&0xFFFF0000UL)!=0xFFFF0000UL) return 15;
 if((u&0xFFFF8000UL)!=0xFFFF8000UL) return 16;
 if((u&0xFFFFC000UL)!=0xFFFFC000UL) return 17;
 if((u&0xFFFFE000UL)!=0xFFFFE000UL) return 18;
 if((u&0xFFFFF000UL)!=0xFFFFF000UL) return 19;
 if((u&0xFFFFF800UL)!=0xFFFFF800UL) return 20;
 if((u&0xFFFFFC00UL)!=0xFFFFFC00UL) return 21;
 if((u&0xFFFFFE00UL)!=0xFFFFFE00UL) return 22;
 if((u&0xFFFFFF00UL)!=0xFFFFFF00UL) return 23;
 if((u&0xFFFFFF80UL)!=0xFFFFFF80UL) return 24;
 if((u&0xFFFFFFC0UL)!=0xFFFFFFC0UL) return 25;
 if((u&0xFFFFFFE0UL)!=0xFFFFFFE0UL) return 26;
 if((u&0xFFFFFFF0UL)!=0xFFFFFFF0UL) return 27;
 if((u&0xFFFFFFF8UL)!=0xFFFFFFF8UL) return 28;
 if((u&0xFFFFFFFCUL)!=0xFFFFFFFCUL) return 29;
 if((u&0xFFFFFFFEUL)!=0xFFFFFFFEUL) return 30;
 if((u&0xFFFFFFFFUL)!=0xFFFFFFFFUL) return 31;
 return 32;
}

int right_ones(unsigned long u)
{
 if((u&0x00000001UL)!=0x00000001UL) return 0;
 if((u&0x00000003UL)!=0x00000003UL) return 1;
 if((u&0x00000007UL)!=0x00000007UL) return 2;
 if((u&0x0000000FUL)!=0x0000000FUL) return 3;
 if((u&0x0000001FUL)!=0x0000001FUL) return 4;
 if((u&0x0000003FUL)!=0x0000003FUL) return 5;
 if((u&0x0000007FUL)!=0x0000007FUL) return 6;
 if((u&0x000000FFUL)!=0x000000FFUL) return 7;
 if((u&0x000001FFUL)!=0x000001FFUL) return 8;
 if((u&0x000003FFUL)!=0x000003FFUL) return 9;
 if((u&0x000007FFUL)!=0x000007FFUL) return 10;
 if((u&0x00000FFFUL)!=0x00000FFFUL) return 11;
 if((u&0x00001FFFUL)!=0x00001FFFUL) return 12;
 if((u&0x00003FFFUL)!=0x00003FFFUL) return 13;
 if((u&0x00007FFFUL)!=0x00007FFFUL) return 14;
 if((u&0x0000FFFFUL)!=0x0000FFFFUL) return 15;
 if((u&0x0001FFFFUL)!=0x0001FFFFUL) return 16;
 if((u&0x0003FFFFUL)!=0x0003FFFFUL) return 17;
 if((u&0x0007FFFfUL)!=0x0007FFFFUL) return 18;
 if((u&0x000FFFFFUL)!=0x000FFFFFUL) return 19;
 if((u&0x001FFFFFUL)!=0x001FFFFFUL) return 20;
 if((u&0x003FFFFFUL)!=0x003FFFFFUL) return 21;
 if((u&0x007FFFFFUL)!=0x007FFFFFUL) return 22;
 if((u&0x00FFFFFFUL)!=0x00FFFFFFUL) return 23;
 if((u&0x01FFFFFFUL)!=0x01FFFFFFUL) return 24;
 if((u&0x03FFFFFFUL)!=0x03FFFFFFUL) return 25;
 if((u&0x07FFFFFFUL)!=0x07FFFFFFUL) return 26;
 if((u&0x0FFFFFFFUL)!=0x0FFFFFFFUL) return 27;
 if((u&0x1FFFFFFFUL)!=0x1FFFFFFFUL) return 28;
 if((u&0x3FFFFFFFUL)!=0x3FFFFFFFUL) return 29;
 if((u&0x7FFFFFFFUL)!=0x7FFFFFFFUL) return 30;
 if((u&0xFFFFFFFFUL)!=0xFFFFFFFFUL) return 31;
 return 32;
}

int main(int argc, char **argv)
{
 FILE *f;
 int i,j,k,m,n,o,p,w,e,sz,asz,bsz,fsz,pt=0;
 unsigned long l;
 char *po,fname[100];

 printf("\nSHAFF v" VERSION " (C) 2013, Alexander A. Shabarshin <ashabarshin@gmail.com>\n");
 if(!f_test)
 {
   if(argc<2)
   {
     printf("\nUsage:\n\tshaff file.bin [options]\n\n");
     return 0;
   }
   strncpy(fname,argv[1],100);
   fname[99] = 0;
   if(!strcmp(fname,"test")) f_test = 1;
 }
 fsz = 12;
 if(f_test)
 {
   sz = 0;
   for(i=0;i<10;i++) one_block[sz++]='a';
   for(i=0;i<40;i++) one_block[sz++]='b';
   for(i=0;i<50;i++) one_block[sz++]='c';
   for(i=0;i<4;i++)  one_block[sz++]='0'+i;
   for(i=0;i<20;i++){one_block[sz++]='d';one_block[sz++]='e';}
   for(i=0;i<10;i++) one_block[sz++]='0'+i;
   for(i=0;i<60;i++) one_block[sz++]='a';
   for(i=0;i<6;i++){one_block[sz++]='d';one_block[sz++]='e';}
   for(i=0;i<30;i++) one_block[sz++]='f';
 }
 else
 {
   printf("\nOpening file '%s'\n",fname);
   f = fopen(fname,"rb");
   if(f==NULL)
   {
     printf("ERROR: Can't open file '%s'\n",fname);
     return -1;
   }
   if(strstr(fname,".sna")!=NULL || strstr(fname,".SNA")!=NULL) f_sna = 1;
   fseek(f,0,SEEK_END);
   sz = ftell(f);
   printf("Original file size: %i bytes (SNA=%c)\n",sz,f_sna?'Y':'N');
   fseek(f,0,SEEK_SET);
   if(f_sna && sz!=49179)
   {
     fclose(f);
     printf("ERROR: Invalid SNA file '%s'\n",fname);
     return -2;
   }
   if(f_sna)
   {
     fread(sna_header,1,27,f);
     pt = 27;
     fsz += 30;
   }
 }
 n = 0;
 while(pt < sz)
 {
   printf("\nBlock %i:\n",++n);
   printf("Cleaning %i words...\n",COR16K);
   memset(cor_table,0,sizeof(unsigned long)*COR16K);
   printf("Cleaning Ok\n");
   for(i=(f_test?sz:0);i<16384;i++) one_block[i]=-1;
   k = sz - pt;
   if(k>16384) k=16384;
   printf("Reading %i bytes\n",k);
   if(!f_test)
   {
     for(i=0;i<k;i++)
     {
       one_block[i] = fgetc(f);
       pt++;
     }
   }
   else pt = sz;
   asz = bsz = k;
   printf("Analysis...\n");
   for(k=1;k<bsz;k++)
   {
     o = k<<9;
     for(i=0;i<16384;i+=32)
     {
       m = 0;
       for(j=0;j<32;j++)
       {
         m <<= 1;
         if(i+j+k < 16384 &&
            one_block[i+j]>=0 && one_block[i+j+k]>=0 &&
            one_block[i+j]==one_block[i+j+k]) m |= 1;
       }
       cor_table[o++] = m;
/*
       if(m) printf("%8.8X=%8.8X\n",o-1,m);
*/
     }
   }
   cur_ele = 0;
   while(1)
   {
     m = 0;
     o = 0;
     k = 0;
     j = -1;
     for(i=0;i<COR16K;i++)
     {
       if(j<0)
       {
         if(!cor_table[i]) continue;
         k = right_ones(cor_table[i]);
         j = (i<<5)+32-k;
         continue;
       }
       if(all_ones(cor_table[i]))
       {
         k += 32;
         continue;
       }
       k += left_ones(cor_table[i]);
       if(k > m)
       {
         m = k;
         o = j;
       }
       j = -1;
       i--;
     }
     w = 0;
     for(i=0;i<COR16K;i++)
     {
       k = 0;
       for(j=31;j>=0;j--)
       {
         p = 0;
         if(cor_table[i]&(1<<j))
         {
           k++;
           if(j!=0) continue;
           p = 1;
         }
         if(k>m)
         {
           if(p) o=(i<<5)+32-k;
           else o=(i<<5)+31-j-k;
           m = k;
           w++;
         }
         k = 0;
       }
     }

     if(m<=3) break;
     p = m;
     if((o&16383)+m > bsz) m = bsz-(o&16383);
     e = (o&16383)+(o>>14);
/*
     printf("o=%7.7X|%02d m=%i (was %i) w=%i bsz=%i (%i,%i)\n",o>>5,o&31,m,p,w,bsz,(o&16383)+p,e);
*/
     if(e < bsz)
     {
       elems[cur_ele].address = e;
       elems[cur_ele].offset = -(o>>14);
       elems[cur_ele].size = m;
       k = asz;
       asz -= m;
       asz += 3;
       if(m>=192) asz++;
       if((o>>14)>=196) asz++;
       if(asz >= k)
       {
         asz = k;
       }
       else
       {
#ifdef DEBUG
         printf("Sequence %i bytes from #%4.4X is identical to offset %i/#%4.4X (estimation: %i)\n",
           elems[cur_ele].size,elems[cur_ele].address,elems[cur_ele].offset,((int)elems[cur_ele].offset)&0xFFFF,asz);
#endif
         cur_ele++;
       }
     }

     o = e;
     for(p=0;p<bsz;p++)
     {
      k = m;
      if((o&16383)+k > bsz) k=bsz-(o&16383);
      if(!((k+(o&31))>>5))
      {
       j = (p<<9)|((o>>5)&511);
       i = o&31;
       l = 1<<(31-i);
       w = 0;
       while(k>0)
       {
         w |= l;
         l >>= 1;
         k--;
       }
/*
       if(p<3||p==256) printf("j=%8.8X w=%8.8X (o=%7.7X|%02d)\n",j,w,o>>5,o&31);
*/
       cor_table[j] &= ~w;
      }
      else for(i=0;i<=((m+(o&31))>>5);i++)
      {
       if(k==0) break;
       j = ((p<<9)|((o>>5)&511))+i;
/*
       if(p>3||p==256) printf("j=%8.8X i=%i k=%i (o=%7.7X|%02d)\n",j,i,k,o>>5,o&31);
*/
       if(i==0)
       {
         switch(o&31)
         {
           case 31: cor_table[j]&=0xFFFFFFFEUL; break;
           case 30: cor_table[j]&=0xFFFFFFFCUL; break;
           case 29: cor_table[j]&=0xFFFFFFF8UL; break;
           case 28: cor_table[j]&=0xFFFFFFF0UL; break;
           case 27: cor_table[j]&=0xFFFFFFE0UL; break;
           case 26: cor_table[j]&=0xFFFFFFC0UL; break;
           case 25: cor_table[j]&=0xFFFFFF80UL; break;
           case 24: cor_table[j]&=0xFFFFFF00UL; break;
           case 23: cor_table[j]&=0xFFFFFE00UL; break;
           case 22: cor_table[j]&=0xFFFFFC00UL; break;
           case 21: cor_table[j]&=0xFFFFF800UL; break;
           case 20: cor_table[j]&=0xFFFFF000UL; break;
           case 19: cor_table[j]&=0xFFFFE000UL; break;
           case 18: cor_table[j]&=0xFFFFC000UL; break;
           case 17: cor_table[j]&=0xFFFF8000UL; break;
           case 16: cor_table[j]&=0xFFFF0000UL; break;
           case 15: cor_table[j]&=0xFFFE0000UL; break;
           case 14: cor_table[j]&=0xFFFC0000UL; break;
           case 13: cor_table[j]&=0xFFF80000UL; break;
           case 12: cor_table[j]&=0xFFF00000UL; break;
           case 11: cor_table[j]&=0xFFE00000UL; break;
           case 10: cor_table[j]&=0xFFC00000UL; break;
           case  9: cor_table[j]&=0xFF800000UL; break;
           case  8: cor_table[j]&=0xFF000000UL; break;
           case  7: cor_table[j]&=0xFE000000UL; break;
           case  6: cor_table[j]&=0xFC000000UL; break;
           case  5: cor_table[j]&=0xF8000000UL; break;
           case  4: cor_table[j]&=0xF0000000UL; break;
           case  3: cor_table[j]&=0xE0000000UL; break;
           case  2: cor_table[j]&=0xC0000000UL; break;
           case  1: cor_table[j]&=0x80000000UL; break;
           case  0: cor_table[j] = 0; break;
         }
         k -= 32-(o&31);
         continue;
       }
       if(k < 32)
       {
         switch(k&31)
         {
           case  0: cor_table[j] = 0; break;
           case  1: cor_table[j]&=0x7FFFFFFFUL; break;
           case  2: cor_table[j]&=0x3FFFFFFFUL; break;
           case  3: cor_table[j]&=0x1FFFFFFFUL; break;
           case  4: cor_table[j]&=0x0FFFFFFFUL; break;
           case  5: cor_table[j]&=0x07FFFFFFUL; break;
           case  6: cor_table[j]&=0x03FFFFFFUL; break;
           case  7: cor_table[j]&=0x01FFFFFFUL; break;
           case  8: cor_table[j]&=0x00FFFFFFUL; break;
           case  9: cor_table[j]&=0x007FFFFFUL; break;
           case 10: cor_table[j]&=0x003FFFFFUL; break;
           case 11: cor_table[j]&=0x001FFFFFUL; break;
           case 12: cor_table[j]&=0x000FFFFFUL; break;
           case 13: cor_table[j]&=0x0007FFFFUL; break;
           case 14: cor_table[j]&=0x0003FFFFUL; break;
           case 15: cor_table[j]&=0x0001FFFFUL; break;
           case 16: cor_table[j]&=0x0000FFFFUL; break;
           case 17: cor_table[j]&=0x00007FFFUL; break;
           case 18: cor_table[j]&=0x00003FFFUL; break;
           case 19: cor_table[j]&=0x00001FFFUL; break;
           case 20: cor_table[j]&=0x00000FFFUL; break;
           case 21: cor_table[j]&=0x000007FFUL; break;
           case 22: cor_table[j]&=0x000003FFUL; break;
           case 23: cor_table[j]&=0x000001FFUL; break;
           case 24: cor_table[j]&=0x000000FFUL; break;
           case 25: cor_table[j]&=0x0000007FUL; break;
           case 26: cor_table[j]&=0x0000003FUL; break;
           case 27: cor_table[j]&=0x0000001FUL; break;
           case 28: cor_table[j]&=0x0000000FUL; break;
           case 29: cor_table[j]&=0x00000007UL; break;
           case 30: cor_table[j]&=0x00000003UL; break;
           case 31: cor_table[j]&=0x00000001UL; break;
         }
         k = 0;
         break;
       }
       if(j>=COR16K)
       {
         printf("ERROR: %i out of range (p=%i)\n",j,p);
         return -3;
       }
       cor_table[j] = 0;
       k -= 32;
      }
      if(!(o&16383)&&!m) break;
      else
      {
        if(o&16383)
             o--;
        else m--;
      }
     }
/*
     printf("cor_table after cleanup:\n");
     for(i=0;i<COR16K;i++)
     {
        if(cor_table[i]) printf("%8.8X=%8.8X\n",i,cor_table[i]);
     }
*/
   }
   printf("Number of detected sequences: %i\n",cur_ele);
   printf("Estimated compression: %i%% (%i -> %i)\n",asz*100/bsz,bsz,asz);
   for(i=0;i<cur_ele;i++)
   {
     j = elems[i].address;
     k = elems[i].size;
     e = 0;
     if(one_block[j]<0 || one_block[j]>255) e++;
     one_block[j++] = 1000 + i;
     while(--k)
     {
       if(one_block[j]<0 || one_block[j]>255) e++;
       one_block[j++] = -1;
     }
     if(e) printf("ERROR: %i collisions detected at range #%4.4X...#%4.4X\n",
        e,elems[i].address,elems[i].address+elems[i].size-1);
   }
   asz = 3;
   for(i=0;i<bsz;i++)
   {
     if(one_block[i]>=0)
     {
       if(one_block[i]>255)
       {
         asz+=3;
         j = one_block[i]-1000;
         o = elems[j].offset;
         k = elems[j].size;
         if(o < -191) asz++;
         if(k > 195) asz++;
       }
       else
       {
         if(one_block[i]==255) asz++;
         asz++;
       }
     }
   }
#if 1
   while(1)
   {
    memset(cor_table,0,sizeof(unsigned long)*COR16K);
    for(i=0;i<bsz-3;i++)
    {
     if(one_block[i]>=0 && one_block[i]<255)
     {
       if(one_block[i+1]>=0 && one_block[i+1]<255)
       {
         k = 0xFF0000|(one_block[i+1]<<8)|one_block[i];
         j = k>>1;
         if(k&1) cor_table[j]+=0x10000;
         else cor_table[j]++;
         if(one_block[i+2]>=0 && one_block[i+2]<255)
         {
            k = (one_block[i+2]<<16)|(k&0xFFFF);
            j = k>>1;
            if(k&1) cor_table[j]+=0x10000;
            else cor_table[j]++;
         }
       }
     }
    }
    j = k = 0;
    for(i=0;i<COR16K;i++)
    {
     o = cor_table[i]&0xFFFF;
     if(o>k){j=i<<1;k=2*o;if(((i>>16)&0xFF)==0xFF)k-=o;}
     o = (cor_table[i]>>16)&0xFFFF;
     if(o>k){j=(i<<1)+1;k=2*o;if(((i>>16)&0xFF)==0xFF)k-=o;}
    }
    if(k<4) break;
    printf("Detected triade is #%2.2X #%2.2X #%2.2X - %i bytes saved\n",j&0xFF,(j>>8)&0xFF,(j>>16)&0xFF,k);
    for(i=0;i<bsz-3;i++)
    {
     if(one_block[i]==(j&0xFF) && one_block[i+1]==((j>>8)&0xFF) && (((j>>16)&0xFF)==0xFF||one_block[i+2]==((j>>16)&0xFF)))
     {
        one_block[i] = 30000;
        one_block[i+1] = -1;
        if(j<0xFF0000) one_block[i+2] = -1;
     }
    }
    asz -= k-4;
   }
   printf("Done with triades\n");
#endif
   printf("Actual compression: %i%% (%i -> %i)\n",asz*100/bsz,bsz,asz);
   fsz += asz;
 }
 if(!f_test) fclose(f);

 printf("\nCompressed file size: %i bytes\n",fsz);

 printf("\nGood bye!\n\n");
 return 0;
}
