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

#define VERSION "1.0"
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
int f_test = 0;
int f_sna = 0;
unsigned char sna_header[27];

int decode(char* fname, int flags); /* decode the file */
int fgetbit(FILE *f, int x); /* x!=0 to fast forward to the next byte */
int fputbit(FILE *f, int b); /* -1 if you need to flush the byte */
int fputbitlen(FILE *f, int l); /* write length */
int fgetbitlen(FILE *f); /* read length */

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
     printf("\t-b to save blocks as separate files\n");
     printf("\t-lN to limit length of matches (default value is 4 for SHAFF0 and 2 for SHAFF1)\n");
     printf("\t-xHH to set prefix byte other than FF (applicable only to SHAFF0)\n");
     printf("\nDecoding options:\n");
     printf("\t-d to decode SHAFF0 or SHAFF1 compressed file to file\n");
     printf("\t-c to decode SHAFF0 or SHAFF1 compressed file to screen\n");
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
     printf("\nERROR: Can't open file '%s'\n",fname);
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
       printf("\nERROR: Invalid SNA file '%s'\n",fname);
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
   printf("\nERROR: Can't open file '%s' for writing\n",fname);
   return -3;
 }
 printf("Version of format to use: SHAFF%i\n",ver);
 if(ver!=0 && ver!=1)
 {
   if(f!=NULL) fclose(f);
   if(fo!=NULL) fclose(fo);
   printf("\nERROR: Invalid version number!\n");
   return -10;
 }
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
 n = sz&BLOCKMS;
 k = (sz/BLOCKSZ)+(n?1:0);
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
         printf("\nERROR: Too many elements...\n");
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
         printf("\nERROR: %i out of range (p=%i)\n",j,p);
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
       printf("\nERROR: %i collisions detected within range #%4.4X...#%4.4X\n",
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
            printf("\nERROR: index %i out of range at #%4.4X\n",j,i);
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
         else if(ver==1) /* SHAFF1 */
         {
           kk = k; /* Estimate effectiveness of compression */
           w = -1;
           while(kk){kk>>=1;w++;}
           e = 6 + w + w;
           for(kk=0;kk<k;kk++)
           {
            if(kk==0) w = copies[j].oldval;
            else w = one_block[i+kk]&255;
            if(w&0x80) e-=9;
            else e-=8;
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
              printf("\nERROR: literal %i out of range at #%4.4X\n",ll,i);
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
         else if(ver==1) /* SHAFF1 */
         {
           if(one_block[i]==b)
           {
#ifdef DEBUG
             printf("Use last byte #%2.2X\n",b);
#endif
             xcount++;
             fputbit(fo,1);
             fputbit(fo,1);
             fputbit(fo,0);
             fputbit(fo,0);
             fputbit(fo,0);
#ifdef DEBUG1
             printf("X last byte 0x%2.2X %c\n",b,(b>32)?b:' ');
#endif
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
   if(ver==1) /* SHAFF1 */
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
 l = ftell(fo);
 if(f!=NULL)
    printf("\nCompressed file size: %i bytes (%i%%)\n",l,l*100/ftell(f));
 if(f!=NULL) fclose(f);
 if(fo!=NULL) fclose(fo);
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
   printf("\nERROR: Length %i is too long\n",l);
   return 0;
 }
 return 1;
}

int fgetbit(FILE *f, int x)
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

int fgetbitlen(FILE *f)
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

int decode(char* fname, int flags)
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
   printf("\nERROR: Can't open file '%s'\n",fname);
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
   printf("\nERROR: Ivalid file '%s'\n",fname);
   return -102;
 }
 version = fgetc(f)-'0';
 if(version<0 || version>1)
 {
   fclose(f);
   printf("\nERROR: Unsupported version %i\n",version);
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
   printf("\nERROR: File '%s' doesn't have data in it!\n");
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
   printf("\nERROR: Can't open file '%s'\n",fnew);
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
                    printf("\nERROR: Something wrong with size (%i)...\n",i);
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
              printf("\nERROR: Something wrong with offset (%i)...\n",offset);
           }
           else
           {
              do
              {
                if(j>=cursize || i>=cursize)
                {
                  printf("\nERROR: Something wrong with size (%i->%i)...\n",j,i);
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
   else if(version==1)
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
            /* Literal <0x80 */
            bitbuf <<= 6;
            bitbuf |= fgetbit(f,0)<<5;
            bitbuf |= fgetbit(f,0)<<4;
            bitbuf |= fgetbit(f,0)<<3;
            bitbuf |= fgetbit(f,0)<<2;
            bitbuf |= fgetbit(f,0)<<1;
            bitbuf |= fgetbit(f,0);
            character = buf[i++] = bitbuf;
            break;
         case 2: /* 10xxxxxxx */
            /* Literal >=0x80 */
            bitbuf = 0x80;
            bitbuf |= fgetbit(f,0)<<6;
            bitbuf |= fgetbit(f,0)<<5;
            bitbuf |= fgetbit(f,0)<<4;
            bitbuf |= fgetbit(f,0)<<3;
            bitbuf |= fgetbit(f,0)<<2;
            bitbuf |= fgetbit(f,0)<<1;
            bitbuf |= fgetbit(f,0);
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
                  printf("\nERROR: Something wrong with offset (%i)...\n",offset);
               }
               else
               {
                  do
                  {
                    if(j>=cursize || i>=cursize)
                    {
                      printf("\nERROR: Something wrong with size (%i->%i)....\n",j,i);
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
               printf("\nERROR: Something wrong with size (%i)...\n",i);
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
