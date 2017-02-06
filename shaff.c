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

#define VERSION "1.0beta"
#define GLOBALSTAT
#define DEBUG
#define DEBUG1

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
int f_invert = 0;
int f_test = 0;
int f_sna = 0;
unsigned char sna_header[27];

int decode(char* fname, int flags);

int main(int argc, char **argv)
{
 FILE *f,*fo;
 int i,j,k,m,n,o,oo,p,w,z,e,b,d,dd,ll,sz,bsz,pt=0;
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
     printf("\t-i to invert literals in output bitstream (applicable only to SHAFF1)\n");
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
         case 'i': f_invert = 1; break;
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
 if(ver==1 && f_invert) printf("Iversion for literals is set\n");
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
       if(!cor_table[i] || cor_table[i]==0xFFFFFFFF) continue;
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
         cor_table[j] &= 0xFFFFFFFF<<(32-(o&31));
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
   fputc(xbyte,fo); /* Prefix byte */
#ifdef DEBUG1
   printf("X prefix %2.2X\n",xbyte);
#endif
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
           if(e >= 0)
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
           if(o==d || o==dd || o==-1)
           {
             if(o==-1)
             {
#ifdef DEBUG
               printf("Use \"multiply\" distance %i/#%4.4X\n",o,o&0xFFFF);
#endif
             }
             else if(o==d)
             {
#ifdef DEBUG
               printf("Use 1st stored distance %i/#%4.4X\n",o,o&0xFFFF);
#endif
             }
             else if(o==dd)
             {
#ifdef DEBUG
               printf("Use 2nd stored distance %i/#%4.4X\n",o,o&0xFFFF);
#endif
             }
//             csz -= 12;
           }
           else
           {
             if(o >= -1345)
             {
//               csz -= 3;
             }
             else if(o >= -321)
             {
//               csz -= 2;
             }
             else if(o >= -65)
             {
//               csz -= 3;
             }
           }
           if(o!=dd && o!=-1){dd=d;d=o;}
         }
/*
         k = m;
         o = -1;
         while(k){k>>=1;o++;}
         isz += o+o;
         copies[cur_copy].sizebits = o+o;

         csz += copies[j].sizebits; ?????
*/
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
//           csz += 6;
           }
           else
           {
             if(one_block[i]&0x80)
             {
//                csz += 9;
             }
             else
             {
//                csz += 8;
             }
           }
           b = one_block[i];
         }
       }
     }
   }
//   printf("Byte-stream compression: %i%% (%i -> %i)\n",asz*100/bsz,bsz,asz);
//   printf("Bit-stream compression: %i%% (%i -> %i)\n",(csz>>3)*100/bsz,bsz,(csz>>3)+1);
//   fsz += asz;
//   gsz += (csz>>3)+((csz&7)?1:0);
#ifdef DEBUG1
   printf("X end of block\n\n");
#endif
   xcount++;
   if(ver==0)
   {
     fputc(xbyte,fo);
     fputc(0xC0,fo);
     fputc(0x00,fo);
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
   printf("Literal balance: %i vs %i\nRarest literals:",w,z);
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
/*   if(ver==1)  */
   {
      if(z > w)
         printf("You may get %i bytes less (%i%%) if set inversion for literals (option -i)\n",
             (z-w)>>3,100*(z-w)/8/ftell(fo));
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
 printf("Working time: %im%is\nGood bye!\n\n",(int)((t2-t1)/60),(int)((t2-t1)%60));
 return 0;
}

int decode(char* fname, int flags)
{
 FILE *f,*fo;
 char fnew[100];
 static unsigned char buf[BLOCKSZ];
 unsigned int u,filesize;
 signed short offset,lastoffset;
 int i,j,version,nblocks,lastsize,character,iblock,cursize,length,xff,nerr=0;
#ifdef DEBUG
 printf("Decode file '%s' with flags 0x%2.2X\n",fname,flags);
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
                if(j>=cursize)
                {
                  printf("\nERROR: Something wrong with size (%i)....\n",j);
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
     printf("\nERROR: format SHAFF1 is not yet supported!\n");
   }
   fwrite(buf,cursize,1,fo);
 }
 if(!(flags&1)) fclose(fo);
 fclose(f);
 return 0;
}
