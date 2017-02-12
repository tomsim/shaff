/* shaff.c - simple LZ77-like compression (started 6-Oct-2013)

   Part of NedoPC SDK (software development kit for simple devices)

   Copyright (C) 2013,2017 A.A.Shabarshin <me@shaos.net>

   This program is free software: you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation, either version 3 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be usef,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICAR PURPOSE. See the
   GNU General Public License for more details.

   You shod have received a copy of the GNU General Public License
   along with this program. If not, see <http://www.gnu.org/licenses/>.
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define VERSION "1.1"
#define GLOBALSTAT
/*
#define DEBUG
#define DEBUG1
*/

/* Code is written to be executed on 32-bit or 64-bit systems with 32-bit int */

#define BLOCKBZ 14
#define BLOCKSZ 16384
#define BLOCKMS 16383
#define BLOCKMH 511
#define BLOCKBH 9
#define COR16K ((BLOCKSZ/32)*BLOCKSZ)
#define MAXCOPIES 15000

unsigned int cor_table[COR16K]; /* 8MB of globals */
short one_block[BLOCKSZ]; /* State of every byte in current block */
struct one_copy { short address, offset, size, oldval; } copies[MAXCOPIES];
unsigned int literalstat[256]; /* Literal statistics */
int cur_copy = 0;
int ver = 0;
int lim = 0;
int xbyte = 255;
int f_decode = 0;
int f_blocks = 0;
int f_stdout = 0;
int f_english = 0;
int f_test = 0;
int f_sna = 0;
unsigned char sna_header[27];

int decode(char* fname, int flags); /* decode the file */
int dehuf(FILE* f, int bits, unsigned char* curhuf); /* dehuffman one byte */
int bihuf(int simb, unsigned char* curhuf, unsigned char** pb); /* how many bits? */
int dohuf(FILE* f, int simb, unsigned char* curhuf); /* enhuffman one byte */
int fgetbit(FILE *f, int x); /* x!=0 to fast forward to the next byte */
int fputbit(FILE *f, int b); /* -1 if you need to flush the byte */
int fputbitlen(FILE *f, int l); /* write length */
int fgetbitlen(FILE *f); /* read length */

/* Precalculated Huffman table for English text (options -2 -e) */
unsigned char enghuf[] = {
/* === 4 bits === */ 0xFF,4,
/* 0x65 e 0001 */
   0x65,0x10,
/* === 5 bits === */ 0xFF,5,
/* 0x20   00000 */
   0x20,0x00,
/* 0x61 a 10111 */
   0x61,0xB8,
/* 0x73 s 10110 */
   0x73,0xB0,
/* 0x6F o 10100 */
   0x6F,0xA0,
/* 0x69 i 01111 */
   0x69,0x78,
/* 0x72 r 01110 */
   0x72,0x70,
/* 0x6E n 01011 */
   0x6E,0x58,
/* 0x74 t 01010 */
   0x74,0x50,
/* 0x6C l 01000 */
   0x6C,0x40,
/* 0x64 d 01100 */
   0x64,0x60,
/* === 6 bits === */ 0xFF,6,
/* 0x6D m 101010 */
   0x6D,0xA8,
/* 0x75 u 100111 */
   0x75,0x9C,
/* 0x70 p 100110 */
   0x70,0x98,
/* 0x2C , 100100 */
   0x2C,0x90,
/* 0x63 c 100010 */
   0x63,0x88,
/* 0x79 y 100001 */
   0x79,0x84,
/* 0x66 f 011011 */
   0x66,0x6C,
/* 0x77 w 011010 */
   0x77,0x68,
/* 0x67 g 010010 */
   0x67,0x48,
/* 0x68 h 001110 */
   0x68,0x38,
/* 0x22 " 001101 */
   0x22,0x34,
/* 0x62 b 001100 */
   0x62,0x30,
/* 0x2D - 001011 */
   0x2D,0x2C,
/* 0x0D   001001 */
   0x0D,0x24,
/* 0x0A   001000 */
   0x0A,0x20,
/* === 7 bits === */ 0xFF,7,
/* 0x6B k 1010110 */
   0x6B,0xAC,
/* 0x27 ' 1001010 */
   0x27,0x94,
/* 0x5F _ 1000111 */
   0x5F,0x8E,
/* 0x2E . 1000110 */
   0x2E,0x8C,
/* 0x49 I 1000001 */
   0x49,0x82,
/* 0x21 ! 0011110 */
   0x21,0x3C,
/* 0x76 v 0010100 */
   0x76,0x28,
/* 0x41 A 0000110 */
   0x41,0x0C,
/* 0x54 T 0000100 */
   0x54,0x08,
/* === 8 bits === */ 0xFF,8,
/* 0x3B ; 10101110 */
   0x3B,0xAE,
/* 0x3F ? 10010110 */
   0x3F,0x96,
/* 0x45 E 01001111 */
   0x45,0x4F,
/* 0x53 S 01001110 */
   0x53,0x4E,
/* 0x57 W 00111111 */
   0x57,0x3F,
/* 0x4E N 00101011 */
   0x4E,0x2B,
/* 0x44 D 00001111 */
   0x44,0x0F,
/* 0x4F O 00001110 */
   0x4F,0x0E,
/* 0x43 C 00001011 */
   0x43,0x0B,
/* 0x78 x 00001010 */
   0x78,0x0A,
/* === 9 bits === */ 0xFF,9,
/* 0x48 H 100101111 */
   0x48,0x97,0x80,
/* 0x52 R 100101110 */
   0x52,0x97,0x00,
/* 0x50 P 100000011 */
   0x50,0x81,0x80,
/* 0x46 F 100000010 */
   0x46,0x81,0x00,
/* 0x4D M 100000000 */
   0x4D,0x80,0x00,
/* 0x4C L 010011011 */
   0x4C,0x4D,0x80,
/* 0x3A : 010011010 */
   0x3A,0x4D,0x00,
/* 0x6A j 001111100 */
   0x6A,0x3E,0x00,
/* === 10 bits === */ 0xFF,10,
/* 0x7A z 1010111111 */
   0x7A,0xAF,0xC0,
/* 0x29 ) 1010111110 */
   0x29,0xAF,0x80,
/* 0x28 ( 1010111101 */
   0x28,0xAF,0x40,
/* 0x59 Y 1000000011 */
   0x59,0x80,0xC0,
/* 0x4B K 1000000010 */
   0x4B,0x80,0x80,
/* 0x56 V 0011111011 */
   0x56,0x3E,0xC0,
/* 0x42 B 0011111010 */
   0x42,0x3E,0x80,
/* 0x47 G 0010101011 */
   0x47,0x2A,0xC0,
/* 0x71 q 0010101001 */
   0x71,0x2A,0x40,
/* 0x55 U 0010101000 */
   0x55,0x2A,0x00,
/* === 11 bits === */ 0xFF,11,
/* 0x5D ] 10101111000 */
   0x5D,0xAF,0x00,
/* 0x5B [ 01001100101 */
   0x5B,0x4C,0xA0,
/* 0x51-Q 01001100100 */
   0x51,0x4C,0x80,
/* === 12 bits === */ 0xFF,12,
/* 0x4A-J 101011110010 */
   0x4A,0xAF,0x20,
/* 0x09   101011110011 */
   0x09,0xAF,0x30,
/* === 13 bits === */ 0xFF,13,
/* 0x1A   0010101010110 */
   0x1A,0x2A,0xB0,
/* 0x1B   0010101010111 */
   0x1B,0x2A,0xB8,
/* 0x58 X 0010101010001 */
   0x58,0x2A,0x88,
/* 0x7E ~ 0010101010000 */
   0x7E,0x2A,0x80,
/* 0x24 $ 0010101010101 */
   0x24,0x2A,0xA8,
/* 0x23 # 0010101010100 */
   0x23,0x2A,0xA0,
/* 0x7D } 0010101010011 */
   0x7D,0x2A,0x98,
/* 0x7C | 0010101010010 */
   0x7C,0x2A,0x90,
/* 0x5E ^ 0100110011111 */
   0x5E,0x4C,0xF8,
/* 0x5C \ 0100110011110 */
   0x5C,0x4C,0xF0,
/* 0x7B { 0100110011101 */
   0x7B,0x4C,0xE8,
/* 0x60 ` 0100110011100 */
   0x60,0x4C,0xE0,
/* 0x3E > 0100110011011 */
   0x3E,0x4C,0xD8,
/* 0x3D = 0100110011010 */
   0x3D,0x4C,0xD0,
/* 0x5A Z 0100110011001 */
   0x5A,0x4C,0xC8,
/* 0x40 @ 0100110011000 */
   0x40,0x4C,0xC0,
/* 0x3C < 0100110001111 */
   0x3C,0x4C,0x78,
/* 0x2F / 0100110001110 */
   0x2F,0x4C,0x70,
/* 0x2B + 0100110001101 */
   0x2B,0x4C,0x68,
/* 0x2A * 0100110001100 */
   0x2A,0x4C,0x60,
/* 0x26 & 0100110001011 */
   0x26,0x4C,0x58,
/* 0x25 % 0100110001010 */
   0x25,0x4C,0x50,
/* 0x39 9 0100110001001 */
   0x39,0x4C,0x48,
/* 0x38 8 0100110001000 */
   0x38,0x4C,0x40,
/* 0x37 7 0100110000111 */
   0x37,0x4C,0x38,
/* 0x36 6 0100110000110 */
   0x36,0x4C,0x30,
/* 0x35 5 0100110000101 */
   0x35,0x4C,0x28,
/* 0x34 4 0100110000100 */
   0x34,0x4C,0x20,
/* 0x33 3 0100110000011 */
   0x33,0x4C,0x18,
/* 0x32 2 0100110000010 */
   0x32,0x4C,0x10,
/* 0x31 1 0100110000001 */
   0x31,0x4C,0x08,
/* 0x30 0 0100110000000 */
   0x30,0x4C,0x00,
/* === end of table === */ 0xFF,0xFF
};

int main(int argc, char **argv)
{
 FILE *f,*fo;
 int i,j,k,m,n,o,oo,p,w,z,e,b,d,dd,ll,kk,sz,bsz,pt=0;
 unsigned int l,curo,lcount,xcount,ui;
 unsigned long t1,t2;
 char *po,fname[100];

 if(argc<2 || argv[1][0]!='-' || argv[1][1]!='c')
    printf("\nSHAFF v" VERSION " (C) 2013,2017 A.A.Shabarshin <me@shaos.net>\n\n");

 t1 = time(NULL);

 if(!f_test)
 {
   if(argc<2)
   {
     printf("\nUsage:\n\tshaff [options] filename\n");
     printf("\nEncoding options:\n");
     printf("\t-0 to use SHAFF0 file format (by default)\n");
     printf("\t-1 to use SHAFF1 file format\n");
     printf("\t-2 to use SHAFF2 file format (experimental)\n");
     printf("\t-b to save blocks as separate files\n");
     printf("\t-lN to limit length of matches (default value is 4 for SHAFF0 and 2 for SHAFF1/2)\n");
     printf("\t-xHH to set prefix byte other than FF (applicable only to SHAFF0 file format)\n");
     printf("\t-e to set default table for English text (applicable only to SHAFF2 file format)\n");
     printf("\nDecoding options:\n");
     printf("\t-d to decode compressed SHAFF file to file\n");
     printf("\t-c to decode compressed SHAFF file to screen\n");
     printf("\n");
     return 0;
   }
   for(i=1;i<argc;i++)
   {
     if(argv[i][0]=='-')
     {
       switch(argv[i][1])
       {
         case '0': ver = 0; break;
         case '1': ver = 1; break;
         case '2': ver = 2; break;
         case 'e': f_english = 1; break;
         case 'd': f_decode = 1; break;
         case 'b': f_blocks = 1; break;
         case 'c': f_stdout = 1; f_decode = 1; break;
         case 'l': lim = atoi(&argv[i][2]); break;
         case 'x': xbyte = strtol(&argv[i][2],NULL,16); break;
       }
     }
     else strncpy(fname,argv[i],100);
   }
   fname[99] = 0;
   if(!strcmp(fname,"TEST")) f_test = 1;
 }
 if(f_decode)
 {
   return decode(fname,f_stdout);
 }
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
   for(i=0;i<6;i++) {one_block[sz++]='d';one_block[sz++]='e';}
   for(i=0;i<30;i++) one_block[sz++]=0xFF;
   f = fo = NULL;
 }
 else
 {
   printf("Opening input file '%s'\n",fname);
   f = fopen(fname,"rb");
   if(f==NULL)
   {
     printf("\nERROR: Can't open file '%s'\n\n",fname);
     return -1;
   }
   if(strstr(fname,".sna")!=NULL || strstr(fname,".SNA")!=NULL) f_sna = 1;
   fseek(f,0,SEEK_END);
   sz = ftell(f);
   fseek(f,0,SEEK_SET);
   printf("Original file size: %i bytes (SNA=%c)\n",sz,f_sna?'Y':'N');
   if(f_sna)
   {
     if(sz!=49179)
     {
       if(f!=NULL) fclose(f);
       printf("\nERROR: Invalid SNA file '%s'\n\n",fname);
       return -2;
     }
     fread(sna_header,1,27,f);
     sz -= 27; /* pt is still 0 */
     po = strstr(fname,".sna");
     if(po!=NULL){po[1]='S';po[2]='N';po[3]='A';}
   }
 }
 strcat(fname,"FF");
 printf("Opening output file '%s'\n",fname);
 fo = fopen(fname,"wb");
 if(fo==NULL)
 {
   if(f!=NULL) fclose(f);
   printf("\nERROR: Can't open file '%s' for writing\n\n",fname);
   return -3;
 }
 printf("Chosen method: SHAFF%i\n",ver);
 fprintf(fo,"SHAFF%i",ver);
 fputc(0,fo);
 if(!f_sna) fputc(12,fo);
 else fputc(42,fo);
 if(!lim)
 {
    if(ver==0)
         lim = 4;
    else lim = 2;
 }
 printf("Minimal length to detect: %i bytes\n",lim);
 if(xbyte<0 || xbyte>255) xbyte = 255;
 if(ver==0) printf("Prefix byte for references: 0x%2.2X\n",xbyte);
 if(ver==2)
 {
    if(f_english) printf("Assume that input is English text\n");
    else
    {
       if(f!=NULL) fclose(f);
       if(fo!=NULL) fclose(fo);
       printf("\nERROR: Experimental support for SHAFF2 requires option -e (English text)\n\n");
       return -11;
    }
 }
 n = sz&BLOCKMS;
 k = (sz/BLOCKSZ)+(n?1:0);
 if(f_blocks)
 {
    if(n>=1000000)
    {
       if(f!=NULL) fclose(f);
       if(fo!=NULL) fclose(fo);
       printf("\nERROR: Too many blocks (%i)!\n\n",n);
       return -11;
    }
    else if(n>=100000) f_blocks = 6;
    else if(n>=10000) f_blocks = 5;
    else if(n>=1000) f_blocks = 4;
    else if(n>=100) f_blocks = 3;
    else if(n>=10) f_blocks = 2;
    else f_blocks = 1;
    printf("Store blocks in separate files with %i-digit suffix\n",f_blocks);
    for(i=0;i<f_blocks;i++) strcat(fname,"0");
 }
 printf("Number of blocks to encode: %i\n",k);
 fputc((k>>8)&255,fo);fputc(k&255,fo);
 if(n==0) n=BLOCKSZ;
 printf("Size of the last block: %i\n",n);
 fputc((n>>8)&255,fo);fputc(n&255,fo);
 if(f_sna)
 {
   fprintf(fo,"SNA");
   fwrite(sna_header,1,27,fo);
 }
#ifdef GLOBALSTAT
 memset(literalstat,0,sizeof(unsigned int)*256);
#endif
 n = 0;
 while(pt < sz)
 {
   printf("\nBlock %i:\n",++n);
   memset(cor_table,0,sizeof(unsigned int)*COR16K);
   for(i=(f_test?sz:0);i<BLOCKSZ;i++) one_block[i]=-1;
   if(f_blocks)
   {
      k = strlen(fname) - f_blocks;
      switch(f_blocks)
      {
        case 1: sprintf(&fname[k],"%1d",n); break;
        case 2: sprintf(&fname[k],"%02d",n); break;
        case 3: sprintf(&fname[k],"%03d",n); break;
        case 4: sprintf(&fname[k],"%04d",n); break;
        case 5: sprintf(&fname[k],"%05d",n); break;
        case 6: sprintf(&fname[k],"%06d",n); break;
      }
      if(fo!=NULL) fclose(fo);
      printf("Opening output partial file '%s'\n",fname);
      fo = fopen(fname,"wb");
      if(fo==NULL)
      {
        if(f!=NULL) fclose(f);
        printf("\nERROR: Can't open partial file '%s' for writing\n\n",fname);
        return -8;
      }
   }
   curo = ftell(fo);
   printf("Current offset %u\n",curo);
   k = sz - pt;
   if(k>BLOCKSZ) k=BLOCKSZ;
   printf("Reading %i bytes...\n",k);
   if(!f_test)
   {
     for(i=0;i<k;i++)
     {
       one_block[i] = fgetc(f);
       pt++;
     }
   }
   else pt = sz;
   bsz = k;
   printf("Autocorrelation...\n");
   for(k=1;k<bsz;k++)
   {
     o = k<<BLOCKBH;
     for(i=0;i<BLOCKSZ;i+=32)
     {
       m = 0;
       for(j=0;j<32;j++)
       {
         m <<= 1;
         if(i+j+k < BLOCKSZ &&
            one_block[i+j]>=0 && one_block[i+j+k]>=0 &&
            one_block[i+j]==one_block[i+j+k]) m |= 1;
       }
       cor_table[o++] = m;
     }
   }
   printf("Greedy algorithm...\n");
   cur_copy = 0;
   while(1)
   {
     m = o = k = 0;
     j = -1;
     /* check for overlapping sequences */
     for(i=0;i<COR16K;i++)
     {
       ui = cor_table[i];
       if(j<0)
       {
         if(!ui) continue;
         for(k=0;k<32;k++)
         {
           if(!(ui&0x00000001)) break;
           ui >>= 1;
         }
         j = (i<<5)+32-k;
         continue;
       }
       if(ui==0xFFFFFFFF)
       {
         k += 32;
         continue;
       }
       for(w=0;w<32;w++)
       {
         if(!(ui&0x80000000)) break;
         ui <<= 1;
       }
       k += w;
       if(k > m)
       {
         m = k;
         o = j;
       }
       j = -1;
       i--;
     }
     /* check for internal sequences if overlapping m<32 */
     if(m<32) for(i=0;i<COR16K;i++)
     {
       ui = cor_table[i];
       if(ui==0x00000000 || ui==0xFFFFFFFF) continue;
       k = 0;
       for(j=31;j>=0;j--,ui<<=1)
       {
         p = 0;
         if(ui&0x80000000)
         {
           k++;
           if(j) continue;
           p = 1;
         }
         if(k>m)
         {
           if(p) o=(i<<5)+32-k;
           else o=(i<<5)+31-j-k;
           m = k;
         }
         k = 0;
       }
     }
     if(m<lim) break;
     oo = o&BLOCKMS;
     if(oo+m > bsz) m=bsz-oo;
     e = oo+(o>>BLOCKBZ);
     if(e < bsz)
     {
       copies[cur_copy].address = e;
       copies[cur_copy].offset = -(o>>BLOCKBZ);
       copies[cur_copy].size = m;
#ifdef DEBUG
       printf("Matched address=#%4.4X size=%i offset=%i/#%4.4X\n",
              copies[cur_copy].address,copies[cur_copy].size,copies[cur_copy].offset,((int)copies[cur_copy].offset)&0xFFFF);
#else
       printf("%i ",copies[cur_copy].size);fflush(stdout);
#endif
       if(++cur_copy>=MAXCOPIES)
       {
         if(f!=NULL) fclose(f);
         if(fo!=NULL) fclose(fo);
         printf("\nERROR: Too many elements...\n\n");
         return -4;
       }
     }
     o = e;
     for(p=0;p<bsz;p++)
     {
      k = m;
      oo = o&BLOCKMS;
      if(oo+k > bsz) k=bsz-oo;
      if(!((k+(o&31))>>5))
      {
       j = (p<<BLOCKBH)|((o>>5)&BLOCKMH);
       i = o&31;
       l = 1<<(31-i);
       w = 0;
       while(k>0)
       {
         w |= l;
         l >>= 1;
         k--;
       }
       cor_table[j] &= ~w;
      }
      else for(i=0;i<=((m+(o&31))>>5);i++)
      {
       if(k==0) break;
       j = ((p<<BLOCKBH)|((o>>5)&BLOCKMH))+i;
       if(i==0)
       {
         if(!(o&31)) cor_table[j] = 0;
         else cor_table[j] &= 0xFFFFFFFF<<(32-(o&31));
         k -= 32-(o&31);
         continue;
       }
       if(k < 32)
       {
         /* k is 1...31 at this point */
         cor_table[j] &= (1<<(32-k))-1;
         k = 0;
         break;
       }
       if(j>=COR16K)
       {
         if(f!=NULL) fclose(f);
         if(fo!=NULL) fclose(fo);
         printf("\nERROR: %i out of range (p=%i)\n\n",j,p);
         return -5;
       }
       cor_table[j] = 0;
       k -= 32;
      }
      if(!(o&BLOCKMS) && !m) break;
      else
      {
        if(o&BLOCKMS)
             o--;
        else m--;
      }
     }
   }
   printf("Number of matches: %i\n",cur_copy);
   for(i=0;i<cur_copy;i++)
   {
     j = copies[i].address;
     k = copies[i].size;
     e = 0;
     if(one_block[j]<0 || one_block[j]>255) e++;
     copies[i].oldval = one_block[j];
     one_block[j++] = 1000 + i;
     while(--k)
     {
       if(one_block[j]<0 || one_block[j]>255) e++;
       one_block[j++] -= 256;
     }
     if(e)
     {
       if(f!=NULL) fclose(f);
       if(fo!=NULL) fclose(fo);
       printf("\nERROR: %i collisions detected within range #%4.4X...#%4.4X\n\n",
             e,copies[i].address,copies[i].address+copies[i].size-1);
       return -6;
     }
   }
   printf("Writing data to the file...\n");
   b = -1;
   d = dd = 0;
   xcount = lcount = 0;
#ifndef GLOBALSTAT
   memset(literalstat,0,sizeof(unsigned int)*256);
#endif
   if(ver==0)
   {
      fputc(xbyte,fo); /* Prefix byte */
#ifdef DEBUG1
      printf("X prefix %2.2X\n",xbyte);
#endif
   }
   for(i=0;i<bsz;i++)
   {
     if(one_block[i]>=0)
     {
       if(one_block[i]>255) /* Reference */
       {
         j = one_block[i]-1000;
         if(j<0 || j>=MAXCOPIES)
         {
            if(f!=NULL) fclose(f);
            if(fo!=NULL) fclose(fo);
            printf("\nERROR: index %i out of range at #%4.4X\n\n",j,i);
            return -7;
         }
         o = copies[j].offset;
         k = copies[j].size;
         if(ver==0) /* SHAFF0 */
         {
           e = 3 - k; /* Estimate effectiveness of compression */
           if(o < -190) e++;
           if(k > 195) e++;
           if(copies[j].oldval==xbyte) e--;
           for(ll=i+1;ll<i+k;ll++)
           {
             if((one_block[ll]&255)==xbyte) e--;
           }
           if(e >= 0) /* Force literals */
           {
             for(e=0;e<k;e++)
             {
                if(e==0) ll = copies[j].oldval;
                else ll = one_block[i+e]&255;
#ifdef DEBUG1
                printf("L 0x%2.2X %c <<< #%4.4X [%i]\n",ll,(ll>32)?ll:' ',i,e);
#endif
                lcount++;
                literalstat[ll]++;
                fputc(ll,fo);
                if(ll==xbyte) fputc(0,fo);
             }
           }
           else
           {
#ifdef DEBUG1
             printf("X 0x%4.4X (%i) %i\n",o&0xFFFF,o,k);
#endif
             xcount++;
             fputc(xbyte,fo);
             o = -o;
             if(o==d)
             {
#ifdef DEBUG
               printf("Use stored distance %i\n",o);
#endif
               fputc(0xBF,fo);
             }
             else if(o < 191)
             {
               fputc(o,fo);
             }
             else /* o >= 191 */
             {
               d = o; /* store distance for possible future use */
               o = (-o) & 0xFFFF;
               fputc(o>>8,fo);
               fputc(o&255,fo);
             }
             if(k < 132)
             {
               fputc(k+124,fo);
             }
             else if(k < 196)
             {
               fputc(k-68,fo);
             }
             else
             {
               fputc(k>>8,fo);
               fputc(k&255,fo);
             }
           }
         }
         else /* SHAFF1 and SHAFF2 */
         {
           kk = k; /* Estimate effectiveness of compression */
           w = -1;
           while(kk){kk>>=1;w++;}
           e = 6 + w + w;
           for(kk=0;kk<k;kk++)
           {
            if(kk==0) w = copies[j].oldval;
            else w = one_block[i+kk]&255;
            if(ver==1)
            {
               if(w&0x80) e-=9;
               else e-=8;
            }
            else e-=bihuf(w,enghuf,NULL); /* version 2 */
           }
           ll = 0;
           if(o==d || o==dd || o==-1)
           {
            if(e > 0) ll = e;
            else
            {
             fputbit(fo,1);
             fputbit(fo,1);
             fputbit(fo,0);
             fputbit(fo,0);
             if(o==-1)
             {
#ifdef DEBUG
               printf("Use \"multiply\" distance %i/#%4.4X\n",o,o&0xFFFF);
#endif
               fputbit(fo,1);
               fputbit(fo,1);
               fputbitlen(fo,k);
             }
             else if(o==d)
             {
#ifdef DEBUG
               printf("Use 1st stored distance %i/#%4.4X\n",o,o&0xFFFF);
#endif
               fputbit(fo,0);
               fputbit(fo,1);
               fputbitlen(fo,k);
             }
             else if(o==dd)
             {
#ifdef DEBUG
               printf("Use 2nd stored distance %i/#%4.4X\n",o,o&0xFFFF);
#endif
               fputbit(fo,1);
               fputbit(fo,0);
               fputbitlen(fo,k);
             }
            }
           }
           else
           {
             if(o >= -65)
             {
                e += 4;
                if(e > 0) ll = e;
                else
                {
                   fputbit(fo,1);
                   fputbit(fo,1);
                   fputbit(fo,0);
                   fputbit(fo,1);
                   w = -o-2;
                   fputbit(fo,w&0x20);
                   fputbit(fo,w&0x10);
                   fputbit(fo,w&0x08);
                   fputbit(fo,w&0x04);
                   fputbit(fo,w&0x02);
                   fputbit(fo,w&0x01);
                   fputbitlen(fo,k);
                }
             }
             else if(o >= -321)
             {
                e += 7;
                if(e > 0) ll = e;
                else
                {
                   fputbit(fo,1);
                   fputbit(fo,1);
                   fputbit(fo,1);
                   fputbit(fo,0);
                   fputbit(fo,0);
                   w = -o-66;
                   fputbit(fo,w&0x80);
                   fputbit(fo,w&0x40);
                   fputbit(fo,w&0x20);
                   fputbit(fo,w&0x10);
                   fputbit(fo,w&0x08);
                   fputbit(fo,w&0x04);
                   fputbit(fo,w&0x02);
                   fputbit(fo,w&0x01);
                   fputbitlen(fo,k);
                }
             }
             else if(o >= -1345)
             {
                e += 9;
                if(e > 0) ll = e;
                else
                {
                   fputbit(fo,1);
                   fputbit(fo,1);
                   fputbit(fo,1);
                   fputbit(fo,0);
                   fputbit(fo,1);
                   w = -o-322;
                   fputbit(fo,w&0x200);
                   fputbit(fo,w&0x100);
                   fputbit(fo,w&0x080);
                   fputbit(fo,w&0x040);
                   fputbit(fo,w&0x020);
                   fputbit(fo,w&0x010);
                   fputbit(fo,w&0x008);
                   fputbit(fo,w&0x004);
                   fputbit(fo,w&0x002);
                   fputbit(fo,w&0x001);
                   fputbitlen(fo,k);
                }
             }
             else
             {
                e += 12;
                if(e > 0) ll = e;
                else
                {
                   fputbit(fo,1);
                   fputbit(fo,1);
                   w = o&0xFFFF;
                   fputbit(fo,w&0x8000);
                   fputbit(fo,w&0x4000);
                   fputbit(fo,w&0x2000);
                   fputbit(fo,w&0x1000);
                   fputbit(fo,w&0x0800);
                   fputbit(fo,w&0x0400);
                   fputbit(fo,w&0x0200);
                   fputbit(fo,w&0x0100);
                   fputbit(fo,w&0x0080);
                   fputbit(fo,w&0x0040);
                   fputbit(fo,w&0x0020);
                   fputbit(fo,w&0x0010);
                   fputbit(fo,w&0x0008);
                   fputbit(fo,w&0x0004);
                   fputbit(fo,w&0x0002);
                   fputbit(fo,w&0x0001);
                   fputbitlen(fo,k);
                }
             }
           }
           if(!ll) /* Reference was good */
           {
             if(o!=d && o!=-1){dd=d;d=o;}
             xcount++;
#ifdef DEBUG1
             printf("X 0x%4.4X (%i) %i\n",o&0xFFFF,o,k);
#endif
           }
           else /* Force literals */
           {
            for(e=0;e<k;e++)
            {
             if(e==0) w = copies[j].oldval;
             else w = one_block[i+e]&255;
             b = w;
             lcount++;
             literalstat[w]++;
#ifdef DEBUG1
             printf("L 0x%2.2X %c <<< #%4.4X [%i] e=%i\n",w,(w>32)?w:' ',i,e,ll);
#endif
             if(ver==1)
             {
              if(w&0x80)
              {
                fputbit(fo,1);
                fputbit(fo,0);
              }
              else fputbit(fo,0);
              fputbit(fo,w&0x40);
              fputbit(fo,w&0x20);
              fputbit(fo,w&0x10);
              fputbit(fo,w&0x08);
              fputbit(fo,w&0x04);
              fputbit(fo,w&0x02);
              fputbit(fo,w&0x01);
             }
             else if(dohuf(fo,w,enghuf)) /* version 2 */
             {
              if(f!=NULL) fclose(f);
              if(fo!=NULL) fclose(fo);
              printf("\nERROR: literal 0x%2.2X can not be encoded by method 2!\n\n",w);
              return -12;
             }
            }
           }
         }
       }
       else /* Literal */
       {
         if(ver==0) /* SHAFF0 */
         {
           ll = one_block[i];
           if(ll<0 || ll>=256)
           {
              if(f!=NULL) fclose(f);
              if(fo!=NULL) fclose(fo);
              printf("\nERROR: literal %i out of range at #%4.4X\n\n",ll,i);
              return -9;
           }
#ifdef DEBUG1
           printf("L 0x%2.2X %c\n",ll,(ll>32)?ll:' ');
#endif
           lcount++;
           literalstat[ll]++;
           fputc(ll,fo);
           if(ll==xbyte) fputc(0,fo);
         }
         else /* SHAFF1 and SHAFF2 */
         {
           if(one_block[i]==b && (ver==1 || /* ver 2 */ bihuf(b,enghuf,NULL)>=6))
           {
#ifdef DEBUG
             printf("Use last byte #%2.2X\n",b);
#endif
             xcount++;
#ifdef DEBUG1
             printf("X last byte 0x%2.2X %c\n",b,(b>32)?b:' ');
#endif
             fputbit(fo,1);
             fputbit(fo,1);
             fputbit(fo,0);
             fputbit(fo,0);
             fputbit(fo,0);
             fputbit(fo,0);
           }
           else
           {
             lcount++;
             b = w = one_block[i];
             literalstat[w]++;
#ifdef DEBUG1
             printf("L 0x%2.2X %c\n",w,(w>32)?w:' ');
#endif
             if(ver==1)
             {
              if(w&0x80)
              {
                fputbit(fo,1);
                fputbit(fo,0);
              }
              else fputbit(fo,0);
              fputbit(fo,w&0x40);
              fputbit(fo,w&0x20);
              fputbit(fo,w&0x10);
              fputbit(fo,w&0x08);
              fputbit(fo,w&0x04);
              fputbit(fo,w&0x02);
              fputbit(fo,w&0x01);
             }
             else if(dohuf(fo,w,enghuf)) /* version 2 */
             {
              if(f!=NULL) fclose(f);
              if(fo!=NULL) fclose(fo);
              printf("\nERROR: literal 0x%2.2X can not be encoded by method 2!\n\n",w);
              return -12;
             }
           }
         }
       }
     }
   }
#ifdef DEBUG1
   printf("X end of block\n\n");
#endif
   xcount++;
   if(ver==0) /* SHAFF0 */
   {
     fputc(xbyte,fo);
     fputc(0xC0,fo);
     fputc(0x00,fo);
   }
   else /* SHAFF1 and SHAFF2 */
   {
     for(ll=0;ll<4;ll++) fputbit(fo,1);
     for(ll=0;ll<14;ll++) fputbit(fo,0);
     fputbit(fo,-1); /* force to write */
   }
   printf("Number of references = %u\n",xcount);
   printf("Number of literals = %u\n",lcount);
   oo = ftell(fo) - curo;
   printf("Compression: %i%% (%i -> %i)\n",oo*100/bsz,bsz,oo);
#ifdef GLOBALSTAT
   if(pt==sz) /* last block */
   {
#endif
#ifdef DEBUG
   printf("\nStatistics for literals:\n");
#endif
   oo = 2000000;
   w = z = 0;
   for(i=0;i<256;i++)
   {
#ifdef DEBUG
#ifndef DEBUG1
      if(literalstat[i])
#endif
         printf("[%03d] 0x%2.2X %c = %i\n",i,i,(i>32)?i:' ',literalstat[i]);
#endif
      if(literalstat[i] < oo) oo = literalstat[i];
      if(i < 128)
           w+=literalstat[i];
      else z+=literalstat[i];
   }
   printf("\nLiteral balance: %i vs %i\nRarest literals:",w,z);
   ll = 3;
   k = 255;
   for(i=255;i>=0;i--)
   {
      if(literalstat[i]==oo)
      {
         if(k==255) k=i;
         printf(" %2.2X",i);
         if(!(--ll)) break;
      }
   }
   printf("\n");
   if(ver==0)
   {
      if(literalstat[xbyte]!=oo)
         printf("You may get %i bytes less (%i%%) if set rarest byte as a prefix (option -x%2.2X)\n",
             literalstat[xbyte]-oo,100*(literalstat[xbyte]-oo)/ftell(fo),k);
   }
 }
#ifdef GLOBALSTAT
 }
#endif
 if(!f_blocks)
 {
   l = ftell(fo);
   if(f!=NULL)
      printf("\nCompressed file size: %i bytes (%i%%)\n",l,l*100/ftell(f));
 }
 if(f!=NULL) fclose(f);
 if(fo!=NULL) fclose(fo);
 if(pt < sz)
 {
    printf("\nERROR: Finished unexpectedly!\n\n");
    return -10;
 }
 t2 = time(NULL);
 printf("Working time: %dm%02ds\nGood bye!\n\n",(int)((t2-t1)/60),(int)((t2-t1)%60));
 return 0;
}

int fputbit(FILE *f, int b)
{
 static int buf = 0;
 static int shi = 0x80;
 int flush = 0;
 if(b < 0)
 {
    if(shi!=0x80) flush=1;
 }
 else
 {
    if(b) buf|=shi;
    shi >>= 1;
    if(!shi) flush=1;
 }
 if(flush)
 {
    fputc(buf,f);
    buf = 0;
    shi = 0x80;
    return 1;
 }
 return 0;
}

int fputbitlen(FILE *f, int l)
{
 if(l<4)
 {
   l-=2;
   fputbit(f,0);
   fputbit(f,l);
 }
 else if(l<8)
 {
   l-=4;
   fputbit(f,1);
   fputbit(f,0);
   fputbit(f,l&2);
   fputbit(f,l&1);
 }
 else if(l<16)
 {
   l-=8;
   fputbit(f,1);
   fputbit(f,1);
   fputbit(f,0);
   fputbit(f,l&4);
   fputbit(f,l&2);
   fputbit(f,l&1);
 }
 else if(l<32)
 {
   l-=16;
   fputbit(f,1);
   fputbit(f,1);
   fputbit(f,1);
   fputbit(f,0);
   fputbit(f,l&8);
   fputbit(f,l&4);
   fputbit(f,l&2);
   fputbit(f,l&1);
 }
 else if(l<64)
 {
   l-=32;
   fputbit(f,1);
   fputbit(f,1);
   fputbit(f,1);
   fputbit(f,1);
   fputbit(f,0);
   fputbit(f,l&0x10);
   fputbit(f,l&0x08);
   fputbit(f,l&0x04);
   fputbit(f,l&0x02);
   fputbit(f,l&0x01);
 }
 else if(l<128)
 {
   l-=64;
   fputbit(f,1);
   fputbit(f,1);
   fputbit(f,1);
   fputbit(f,1);
   fputbit(f,1);
   fputbit(f,0);
   fputbit(f,l&0x20);
   fputbit(f,l&0x10);
   fputbit(f,l&0x08);
   fputbit(f,l&0x04);
   fputbit(f,l&0x02);
   fputbit(f,l&0x01);
 }
 else if(l<256)
 {
   l-=128;
   fputbit(f,1);
   fputbit(f,1);
   fputbit(f,1);
   fputbit(f,1);
   fputbit(f,1);
   fputbit(f,1);
   fputbit(f,0);
   fputbit(f,l&0x40);
   fputbit(f,l&0x20);
   fputbit(f,l&0x10);
   fputbit(f,l&0x08);
   fputbit(f,l&0x04);
   fputbit(f,l&0x02);
   fputbit(f,l&0x01);
 }
 else if(l<512)
 {
   l-=256;
   fputbit(f,1);
   fputbit(f,1);
   fputbit(f,1);
   fputbit(f,1);
   fputbit(f,1);
   fputbit(f,1);
   fputbit(f,1);
   fputbit(f,0);
   fputbit(f,l&0x80);
   fputbit(f,l&0x40);
   fputbit(f,l&0x20);
   fputbit(f,l&0x10);
   fputbit(f,l&0x08);
   fputbit(f,l&0x04);
   fputbit(f,l&0x02);
   fputbit(f,l&0x01);
 }
 else if(l<1024)
 {
   l-=512;
   fputbit(f,1);
   fputbit(f,1);
   fputbit(f,1);
   fputbit(f,1);
   fputbit(f,1);
   fputbit(f,1);
   fputbit(f,1);
   fputbit(f,1);
   fputbit(f,0);
   fputbit(f,l&0x100);
   fputbit(f,l&0x080);
   fputbit(f,l&0x040);
   fputbit(f,l&0x020);
   fputbit(f,l&0x010);
   fputbit(f,l&0x008);
   fputbit(f,l&0x004);
   fputbit(f,l&0x002);
   fputbit(f,l&0x001);
 }
 else if(l<2048)
 {
   l-=1024;
   fputbit(f,1);
   fputbit(f,1);
   fputbit(f,1);
   fputbit(f,1);
   fputbit(f,1);
   fputbit(f,1);
   fputbit(f,1);
   fputbit(f,1);
   fputbit(f,1);
   fputbit(f,0);
   fputbit(f,l&0x200);
   fputbit(f,l&0x100);
   fputbit(f,l&0x080);
   fputbit(f,l&0x040);
   fputbit(f,l&0x020);
   fputbit(f,l&0x010);
   fputbit(f,l&0x008);
   fputbit(f,l&0x004);
   fputbit(f,l&0x002);
   fputbit(f,l&0x001);
 }
 else if(l<4096)
 {
   l-=2048;
   fputbit(f,1);
   fputbit(f,1);
   fputbit(f,1);
   fputbit(f,1);
   fputbit(f,1);
   fputbit(f,1);
   fputbit(f,1);
   fputbit(f,1);
   fputbit(f,1);
   fputbit(f,1);
   fputbit(f,0);
   fputbit(f,l&0x400);
   fputbit(f,l&0x200);
   fputbit(f,l&0x100);
   fputbit(f,l&0x080);
   fputbit(f,l&0x040);
   fputbit(f,l&0x020);
   fputbit(f,l&0x010);
   fputbit(f,l&0x008);
   fputbit(f,l&0x004);
   fputbit(f,l&0x002);
   fputbit(f,l&0x001);
 }
 else if(l<8192)
 {
   l-=4096;
   fputbit(f,1);
   fputbit(f,1);
   fputbit(f,1);
   fputbit(f,1);
   fputbit(f,1);
   fputbit(f,1);
   fputbit(f,1);
   fputbit(f,1);
   fputbit(f,1);
   fputbit(f,1);
   fputbit(f,1);
   fputbit(f,0);
   fputbit(f,l&0x800);
   fputbit(f,l&0x400);
   fputbit(f,l&0x200);
   fputbit(f,l&0x100);
   fputbit(f,l&0x080);
   fputbit(f,l&0x040);
   fputbit(f,l&0x020);
   fputbit(f,l&0x010);
   fputbit(f,l&0x008);
   fputbit(f,l&0x004);
   fputbit(f,l&0x002);
   fputbit(f,l&0x001);
 }
 else if(l<16384)
 {
   l-=8192;
   fputbit(f,1);
   fputbit(f,1);
   fputbit(f,1);
   fputbit(f,1);
   fputbit(f,1);
   fputbit(f,1);
   fputbit(f,1);
   fputbit(f,1);
   fputbit(f,1);
   fputbit(f,1);
   fputbit(f,1);
   fputbit(f,1);
   fputbit(f,0);
   fputbit(f,l&0x1000);
   fputbit(f,l&0x0800);
   fputbit(f,l&0x0400);
   fputbit(f,l&0x0200);
   fputbit(f,l&0x0100);
   fputbit(f,l&0x0080);
   fputbit(f,l&0x0040);
   fputbit(f,l&0x0020);
   fputbit(f,l&0x0010);
   fputbit(f,l&0x0008);
   fputbit(f,l&0x0004);
   fputbit(f,l&0x0002);
   fputbit(f,l&0x0001);
 }
 else
 {
   printf("\nERROR: Length %i is too long\n\n",l);
   return 0;
 }
 return 1;
}

int bihuf(int simb, unsigned char* curhuf, unsigned char** pb)
{
 int result = -1;
 int b,bytes = 0;
 while(1)
 {
   if(*curhuf==0xFF)
   {
      curhuf++;
      if(curhuf==0x00)
      {
         if(simb==0xFF) break;
         curhuf+=bytes+1;
      }
      else if(*curhuf==0xFF)
      {
         result = -1;
         break;
      }
      else
      {
         b = result = *curhuf;
         bytes = 0;
         while(b > 0)
         {
            bytes++;
            b-=8;
         }
         curhuf++;
      }
   }
   else
   {
      if(*curhuf==simb) break;
      curhuf+=bytes+1;
   }
 }
 if(result < 0) return 0;
 if(pb!=NULL) *pb = ++curhuf;
 return result;
}

int dohuf(FILE* f, int simb, unsigned char* curhuf)
{
 int i,j,m,bits;
 unsigned char* pb;
 bits = bihuf(simb,curhuf,&pb);
 if(bits > 0)
 {
    for(i=0;i<bits;i++)
    {
      if(!(i&7))
      {
         m = 0x80;
         j = i>>3;
      }
      fputbit(f,pb[j]&m);
      m >>= 1;
    }
    return 0;
 }
 return -1; /* not found */
}

/* <><><><><><><><> SHAFF DECODER <><><><><><><><> */

int fgetbit(FILE *f, int x) /* SHAFF1 and SHAFF2 */
{
 static int buf = 0;
 static int shi = 1;
 if(shi==1 || x)
 {
    buf = fgetc(f);
    shi = 0x80;
 }
 else shi >>= 1;
 return (buf&shi)?1:0;
}

int fgetbitlen(FILE *f) /* SHAFF1 and SHAFF2 */
{
 int lbase = 2;
 int nbit = 1;
 int len = 0;
 while(fgetbit(f,0))
 {
   lbase<<=1;
   nbit++;
 }
 while(nbit--)
 {
   len |= fgetbit(f,0);
   len <<= 1;
 }
 return lbase+(len>>1);
}

int dehuf(FILE* f, int bits, unsigned char* curhuf) /* SHAFF2 */
{
 unsigned char by[8];
 int bytes,i,e;
 int result = -1;
 int curbits = 2;
 while(1)
 {
   if(*curhuf!=0xFF){curhuf++;continue;}
   curhuf++;
   if(*curhuf==0xFF) break;
   if(*curhuf==0x00) continue;
   if(*curhuf > curbits)
   {
      while(curbits!=*curhuf)
      {
        bits <<= 1;
        bits |= fgetbit(f,0);
        curbits++;
      }
   }
   if(curbits <= 8)
   {
      bytes = 1;
      by[0] = bits<<(8-curbits);
   }
   else if(curbits <= 16)
   {
      bytes = 2;
      by[0] = bits>>(curbits-8);
      by[1] = (bits<<(16-curbits))&255;
   }
   else /* curbits > 16 */
   {
      /* TODO: fix it later */
      printf("\nERROR: Code is too long (%i bits)\n\n",curbits);
   }
   curhuf++;
   while(1)
   {
      result = *curhuf;
      if(result==0xFF)
      {
         curhuf++;
         if(*curhuf==0x00) curhuf++;
         else /* next bit count */
         {
            --curhuf;
            result = -1;
            break;
         }
      }
      curhuf++;
      e = 0;
      for(i=0;i<bytes;i++)
      {
         if(by[i]!=*curhuf) e++;
         curhuf++;
      }
      if(e==0) break; /* found */
   }
   if(result >= 0) break;
 }
 if(result < 0)
 {
   printf("\nERROR: Can't find the code 0x%8.8X\n\n",bits);
 }
 return result&255;
}

int decode(char* fname, int flags) /* SHAFF0, SHAFF1 and SHAFF2 (-e) */
{
 FILE *f,*fo;
 char fnew[100];
 static unsigned char buf[BLOCKSZ];
 unsigned int u,filesize,bitbuf;
 signed short offset,lastoffset,lastoffset2;
 int i,j,version,nblocks,lastsize,character,iblock,cursize,length,xff,nerr=0;
#ifdef DEBUG
 printf("Decode file '%s' with flags %i\n",fname,flags);
#endif
 strcpy(fnew,"stdout");
 f = fopen(fname,"rb");
 if(f==NULL)
 {
   printf("\nERROR: Can't open file '%s'\n\n",fname);
   return -101;
 }
 fseek(f,0,SEEK_END);
 filesize = ftell(f);
 fseek(f,0,SEEK_SET);
 if(fgetc(f)!='S') nerr++;
 if(fgetc(f)!='H') nerr++;
 if(fgetc(f)!='A') nerr++;
 if(fgetc(f)!='F') nerr++;
 if(fgetc(f)!='F') nerr++;
 if(nerr)
 {
   fclose(f);
   printf("\nERROR: Ivalid file '%s'\n\n",fname);
   return -102;
 }
 version = fgetc(f)-'0';
 if(version<0 || version>2)
 {
   fclose(f);
   printf("\nERROR: Unsupported version %i\n\n",version);
   return -103;
 }
 offset = fgetc(f)<<8;
 offset |= fgetc(f);
 nblocks = fgetc(f)<<8;
 nblocks |= fgetc(f);
 lastsize = fgetc(f)<<8;
 lastsize |= fgetc(f);
#ifdef DEBUG
 printf("Compression method = %i\n",version);
 printf("Offset to data = %i\n",offset);
 printf("Number of blocks = %i\n",nblocks);
 printf("Size of last block = %i\n",lastsize);
#endif
 if(offset >= filesize)
 {
   printf("\nERROR: File '%s' doesn't have data in it!\n\n",fname);
   return -104;
 }
 if(flags&1) fo = stdout;
 else
 {
   strncpy(fnew,fname,99);
   fnew[98] = 0;
   u = strlen(fnew);
   if(fnew[u-1]=='F' && fnew[u-2]=='F') fnew[u-2]=0;
   else strcat(fnew,"_");
   fo = fopen(fnew,"wb");
 }
 if(fo==NULL)
 {
   fclose(f);
   printf("\nERROR: Can't open file '%s'\n\n",fnew);
   return -105;
 }
#ifdef DEBUG
 printf("Output file: %s\n",fnew);
#endif
 if(offset > 15)
 {
   buf[0] = fgetc(f);
   buf[1] = fgetc(f);
   buf[2] = fgetc(f);
   buf[3] = 0;
   if(!strcmp(buf,"SNA"))
   {
     fread(buf,27,1,f);
     fwrite(buf,27,1,fo);
   }
 }
 fseek(f,offset,SEEK_SET);
 for(iblock=1;iblock<=nblocks;iblock++)
 {
   if(iblock < nblocks)
     cursize = BLOCKSZ;
   else
     cursize = lastsize;
#ifdef DEBUG
   printf("Block %i (%i)\n",iblock,cursize);
#endif
   if(version==0)
   {
     xff = fgetc(f); /* Read byte that should be used as a prefix */
     i = 0;
     while(i<=cursize)
     {
       character = fgetc(f);
       if(character==xff)
       {
         character = fgetc(f);
         if(character==0) /* Literal 0xFF (or other prefix if set) */
            buf[i++] = xff;
         else /* Reference */
         {
           /* Decode offset */
           if((character&0xC0)==0xC0)
           {
              offset = (((short)character)<<8)|((short)fgetc(f));
              lastoffset = offset;
              if(offset==-16384)
              {
                 /* Marker of the block end */
                 if(i!=cursize)
                 {
                    printf("\nERROR: Something wrong with size (%i)...\n\n",i);
                 }
                 break;
              }
           }
           else if(character==191)
              offset = lastoffset;
           else
              offset = -character;
           /* Decode length */
           character = fgetc(f);
           if((character&0xC0)==0)
              length = (character<<8)|fgetc(f);
           else if((character&0xC0)==64)
              length = character + 68; /* 132-64 */
           else
              length = character - 124; /* 4-128 */
           j = i + offset;
           if(j < 0)
           {
              printf("\nERROR: Something wrong with offset (%i)...\n\n",offset);
           }
           else
           {
              do
              {
                if(j>=cursize || i>=cursize)
                {
                  printf("\nERROR: Something wrong with size (%i->%i)...\n\n",j,i);
                }
                else buf[i++] = buf[j++];
              } while(--length);
           }
         }
       }
       else /* Literal */
          buf[i++] = character;
     }
   }
   else /* version 1 and 2 */
   {
     i = 0;
     length = character = 0;
     lastoffset = lastoffset2 = 0;
     bitbuf = fgetbit(f,-1)<<1; /* Force to start from the new byte */
     while(i<=cursize)
     {
       bitbuf |= fgetbit(f,0);
       switch(bitbuf)
       {
         case 0: /* 00xxxxxx */
         case 1: /* 01xxxxxx */
            if(version==2)
               bitbuf = dehuf(f,bitbuf,enghuf);
            else /* version 1 */
            {
             /* Literal <0x80 */
             bitbuf <<= 6;
             bitbuf |= fgetbit(f,0)<<5;
             bitbuf |= fgetbit(f,0)<<4;
             bitbuf |= fgetbit(f,0)<<3;
             bitbuf |= fgetbit(f,0)<<2;
             bitbuf |= fgetbit(f,0)<<1;
             bitbuf |= fgetbit(f,0);
            }
            character = buf[i++] = bitbuf;
            break;
         case 2: /* 10xxxxxxx */
            if(version==2)
               bitbuf = dehuf(f,bitbuf,enghuf);
            else /* version 1 */
            {
             /* Literal >=0x80 */
             bitbuf = 0x80;
             bitbuf |= fgetbit(f,0)<<6;
             bitbuf |= fgetbit(f,0)<<5;
             bitbuf |= fgetbit(f,0)<<4;
             bitbuf |= fgetbit(f,0)<<3;
             bitbuf |= fgetbit(f,0)<<2;
             bitbuf |= fgetbit(f,0)<<1;
             bitbuf |= fgetbit(f,0);
            }
            character = buf[i++] = bitbuf;
            break;
         case 3: /* 11... - Reference */
            length = 0;
            bitbuf = fgetbit(f,0)<<3;
            bitbuf |= fgetbit(f,0)<<2;
            bitbuf |= fgetbit(f,0)<<1;
            bitbuf |= fgetbit(f,0);
            switch(bitbuf)
            {
               case 0: /* 110000 */
                  buf[i++] = character;
                  break;
               case 1: /* 110001... */
                  offset = lastoffset;
                  length = fgetbitlen(f);
                  break;
               case 2: /* 110010... */
                  offset = lastoffset2;
                  length = fgetbitlen(f);
                  break;
               case 3: /* 110011... */
                  offset = -1;
                  length = fgetbitlen(f);
                  break;
               case 4: /* 110100... */
               case 5: /* 110101... */
               case 6: /* 110110... */
               case 7: /* 110111... */
                  bitbuf &= 3;
                  bitbuf <<= 4;
                  bitbuf |= fgetbit(f,0)<<3;
                  bitbuf |= fgetbit(f,0)<<2;
                  bitbuf |= fgetbit(f,0)<<1;
                  bitbuf |= fgetbit(f,0);
                  offset = -(int)(bitbuf + 2);
                  length = fgetbitlen(f);
                  break;
               case 8: /* 111000... */
               case 9: /* 111001... */
                  bitbuf &= 1;
                  bitbuf <<= 7;
                  bitbuf |= fgetbit(f,0)<<6;
                  bitbuf |= fgetbit(f,0)<<5;
                  bitbuf |= fgetbit(f,0)<<4;
                  bitbuf |= fgetbit(f,0)<<3;
                  bitbuf |= fgetbit(f,0)<<2;
                  bitbuf |= fgetbit(f,0)<<1;
                  bitbuf |= fgetbit(f,0);
                  offset = -(int)(bitbuf + 66);
                  length = fgetbitlen(f);
                  break;
               case 10:/* 111010... */
               case 11:/* 111011... */
                  bitbuf &= 1;
                  bitbuf <<= 9;
                  bitbuf |= fgetbit(f,0)<<8;
                  bitbuf |= fgetbit(f,0)<<7;
                  bitbuf |= fgetbit(f,0)<<6;
                  bitbuf |= fgetbit(f,0)<<5;
                  bitbuf |= fgetbit(f,0)<<4;
                  bitbuf |= fgetbit(f,0)<<3;
                  bitbuf |= fgetbit(f,0)<<2;
                  bitbuf |= fgetbit(f,0)<<1;
                  bitbuf |= fgetbit(f,0);
                  offset = -(int)(bitbuf + 322);
                  length = fgetbitlen(f);
                  break;
               case 12:/* 111100... */
               case 13:/* 111101... */
               case 14:/* 111110... */
               case 15:/* 111111... */
                  bitbuf &= 15;
                  bitbuf <<= 12;
                  bitbuf |= fgetbit(f,0)<<11;
                  bitbuf |= fgetbit(f,0)<<10;
                  bitbuf |= fgetbit(f,0)<<9;
                  bitbuf |= fgetbit(f,0)<<8;
                  bitbuf |= fgetbit(f,0)<<7;
                  bitbuf |= fgetbit(f,0)<<6;
                  bitbuf |= fgetbit(f,0)<<5;
                  bitbuf |= fgetbit(f,0)<<4;
                  bitbuf |= fgetbit(f,0)<<3;
                  bitbuf |= fgetbit(f,0)<<2;
                  bitbuf |= fgetbit(f,0)<<1;
                  bitbuf |= fgetbit(f,0);
                  offset = bitbuf;
                  if(offset == -16384) length = -1; /* Marker of the block end */
                  else length = fgetbitlen(f);
                  break;
            }
            if(length)
            {
               if(length < 0) break;
               j = i + offset;
               if(j < 0)
               {
                  printf("\nERROR: Something wrong with offset (%i)...\n\n",offset);
               }
               else
               {
                  do
                  {
                    if(j>=cursize || i>=cursize)
                    {
                      printf("\nERROR: Something wrong with size (%i->%i)....\n\n",j,i);
                    }
                    else buf[i++] = buf[j++];
                  } while(--length);
               }
               if(offset!=lastoffset && offset!=-1)
               {
                  lastoffset2 = lastoffset;
                  lastoffset = offset;
               }
            }
            break;
       }
       if(length < 0)
       {
            if(i!=cursize)
            {
               printf("\nERROR: Something wrong with size (%i)...\n\n",i);
            }
            break;
       }
       bitbuf = fgetbit(f,0)<<1;
     }
   }
   fwrite(buf,cursize,1,fo);
 }
 if(!(flags&1)) fclose(fo);
 fclose(f);
 return 0;
}
