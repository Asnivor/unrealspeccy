#include "std.h"

#include "emul.h"
#include "vars.h"
#include "snapshot.h"
#include "init.h"

#include "util.h"



#ifdef _M_IX86
void __cdecl fillCpuString(char *dst)
{
   __asm {
      push esi
      push edi
      push ebx
      mov esi, [dst]
      mov edi, 80000000h
      mov eax, edi
      mov byte ptr [esi], al
      mov byte ptr [esi+12], al
      mov byte ptr [esi+48], al
      cpuid
      cmp eax, 80000004h
      jb simple
      lea eax,[edi+2]
      cpuid
      mov dword ptr [esi+0],eax
      mov dword ptr [esi+4],ebx
      mov dword ptr [esi+8],ecx
      mov dword ptr [esi+12],edx
      lea eax,[edi+3]
      cpuid
      mov dword ptr [esi+16],eax
      mov dword ptr [esi+20],ebx
      mov dword ptr [esi+24],ecx
      mov dword ptr [esi+28],edx
      lea eax,[edi+4]
      cpuid
      mov dword ptr [esi+32],eax
      mov dword ptr [esi+36],ebx
      mov dword ptr [esi+40],ecx
      mov dword ptr [esi+44],edx
      jmp done
  simple:
      xor eax,eax
      cpuid
      mov dword ptr [esi+0],ebx
      mov dword ptr [esi+4],edx
      mov dword ptr [esi+8],ecx
  done:
      pop ebx
      pop edi
      pop esi
   }
}

unsigned cpuid(unsigned _eax, int ext)
{
   __asm {
      push ebx
      mov eax, _eax
      cpuid
      pop ebx
      cmp [ext],0
      jz noext
      mov eax, edx
   noext:
      mov _eax, eax
   }
   return _eax;
}
#endif

unsigned __int64 GetCPUFrequency()
{
   LARGE_INTEGER Frequency;
   LARGE_INTEGER Start;
   LARGE_INTEGER Stop;
   unsigned long long c1, c2, c3, c4, c;
   timeBeginPeriod(1);
   QueryPerformanceFrequency(&Frequency);
   Sleep(20);

   c1 = rdtsc();
   QueryPerformanceCounter(&Start);
   c2 = rdtsc();
   Sleep(500);
   c3 = rdtsc();
   QueryPerformanceCounter(&Stop);
   c4 = rdtsc();
   timeEndPeriod(1);
   c = c3 - c2 + (c4-c3)/2 + (c2-c1)/2;
   Start.QuadPart = Stop.QuadPart - Start.QuadPart;

   return ((c * Frequency.QuadPart) / Start.QuadPart);
}

void trim(char *dst)
{
   unsigned i = strlen(dst);
   // trim right spaces
   while (i && isspace(dst[i-1])) i--;
   dst[i] = 0;
   // trim left spaces
   for (i = 0; isspace(dst[i]); i++);
   strcpy(dst, dst+i);
}


const char clrline[] = "\r\t\t\t\t\t\t\t\t\t       \r";

#define savetab(x) {FILE *ff=fopen("tab","wb");fwrite(x,sizeof(x),1,ff);fclose(ff);}

#define tohex(a) ((a) < 10 ? (a)+'0' : (a)-10+'A')

const char nop = 0;
const char * const nil = &nop;

int ishex(char c)
{
   return (isdigit(c) || (tolower(c) >= 'a' && tolower(c) <= 'f'));
}

unsigned char hex(char p)
{
   p = tolower(p);
   return (p < 'a') ? p-'0' : p-'a'+10;
}

unsigned char hex(const char *p)
{
   return 0x10*hex(p[0]) + hex(p[1]);
}

unsigned process_msgs()
{
   MSG msg; unsigned key = 0;
   while (PeekMessage(&msg, 0, 0, 0, PM_REMOVE))
   {
/*
      if (msg.message == WM_NCLBUTTONDOWN)
          continue;
*/
      // mouse messages must be processed further
      if (msg.message == WM_LBUTTONDOWN)
      {
          mousepos = msg.lParam;
          key = VK_LMB;
      }
      if (msg.message == WM_RBUTTONDOWN)
      {
          mousepos = msg.lParam | 0x80000000;
          key = VK_RMB;
      }
      if (msg.message == WM_MBUTTONDOWN)
      {
          mousepos = msg.lParam | 0x80000000;
          key = VK_MMB;
      }

      if (msg.message == WM_MOUSEWHEEL) // [vv] Process mouse whell only in client area
      {
          POINT Pos = { GET_X_LPARAM(msg.lParam), GET_Y_LPARAM(msg.lParam) };

          RECT ClientRect;
          GetClientRect(msg.hwnd, &ClientRect);
          MapWindowPoints(msg.hwnd, HWND_DESKTOP, (LPPOINT)&ClientRect, 2);

          if(PtInRect(&ClientRect, Pos))
              key = GET_WHEEL_DELTA_WPARAM(msg.wParam) < 0 ? VK_MWD : VK_MWU;
      }

      // WM_KEYDOWN and WM_SYSKEYDOWN must not be dispatched,
      // bcoz window will be closed on alt-f4
      if (msg.message == WM_KEYDOWN || msg.message == WM_SYSKEYDOWN)
      {
         if (conf.atm.xt_kbd)
             input.atm51.setkey(msg.lParam >> 16, 1);
         switch (( msg.lParam>>16)&0x1FF)
         {
            case 0x02a: kbdpcEX[0]=kbdpcEX[0]^0x01|0x80; break;
            case 0x036: kbdpcEX[1]=kbdpcEX[1]^0x01|0x80; break;
            case 0x01d: kbdpcEX[2]=kbdpcEX[2]^0x01|0x80; break;
            case 0x11d: kbdpcEX[3]=kbdpcEX[3]^0x01|0x80; break;
            case 0x038: kbdpcEX[4]=kbdpcEX[4]^0x01|0x80; break;
            case 0x138: kbdpcEX[5]=kbdpcEX[5]^0x01|0x80; break;
         } //Dexus
//         printf("%s, WM_KEYDOWN, WM_SYSKEYDOWN\n", __FUNCTION__);
         key = msg.wParam;
      }
      else if (msg.message == WM_KEYUP || msg.message == WM_SYSKEYUP)
      {
         if (conf.atm.xt_kbd) input.atm51.setkey(msg.lParam >> 16, 0);
         switch (( msg.lParam>>16)&0x1FF)
         {
            case 0x02a: kbdpcEX[0]&=0x01; kbdpcEX[1]&=0x01; break;
            case 0x036: kbdpcEX[0]&=0x01; kbdpcEX[1]&=0x01; break;
            case 0x01d: kbdpcEX[2]&=0x01; break;
            case 0x11d: kbdpcEX[3]&=0x01; break;
            case 0x038: kbdpcEX[4]&=0x01; break;
            case 0x138: kbdpcEX[5]&=0x01; break;
         } //Dexus

//         printf("%s, WM_KEYUP, WM_SYSKEYUP\n", __FUNCTION__);
         DispatchMessage(&msg); //Dexus
      }
      else
         DispatchMessage(&msg);
   }
   return key;
}

void eat() // eat messages
{
   Sleep(20); while (process_msgs()) Sleep(10);
}

char dispatch_more(action *table)
{
   if (!table)
       return -1;

   kbdpc[0] = 0x80; // nil button is always pressed

//   __debugbreak();
   while (table->name)
   {
//      printf("%02X|%02X|%02X|%02X\n", table->k1, table->k2, table->k3, table->k4);
      unsigned k[4] = { table->k1, table->k2, table->k3, table->k4 };
      unsigned b[4];

      for(unsigned i =0; i< 4; i++)
      {
          switch(k[i])
          {
          case DIK_MENU:
              b[i] = kbdpc[DIK_LMENU] | kbdpc[DIK_RMENU];
          break;
          case DIK_CONTROL:
              b[i] = kbdpc[DIK_LCONTROL] | kbdpc[DIK_RCONTROL];
          break;
          case DIK_SHIFT:
              b[i] = kbdpc[DIK_LSHIFT] | kbdpc[DIK_RSHIFT];
          break;
          default:
              b[i] = kbdpc[k[i]];
          }
      }

      if (b[0] & b[1] & b[2] & b[3] & 0x80)
      {
         table->func();
         return 1;
      }
      table++;
   }
   return -1;
}

char dispatch(action *table)
{
   if (*droppedFile)
   {
       trd_toload = 0;
       loadsnap(droppedFile);
       *droppedFile = 0;
   }
   if(!input.readdevices())
       return 0;

   dispatch_more(table);
   return 1;
}

bool wcmatch(char *string, char *wc)
{
   for (;;wc++, string++) {
      if (!*string && !*wc) return 1;
      if (*wc == '?') { if (*string) continue; else return 0; }
      if (*wc == '*') {
         for (wc++; *string; string++)
            if (wcmatch(string, wc)) return 1;
         return 0;
      }
      if (tolower(*string) != tolower(*wc)) return 0;
   }
}

void dump1(BYTE *p, unsigned sz)
{
   while (sz) {
      printf("\t");
      unsigned chunk = (sz > 16)? 16 : sz;
      unsigned i; //Alone Coder 0.36.7
      for (/*unsigned*/ i = 0; i < chunk; i++) printf("%02X ", p[i]);
      for (; i < 16; i++) printf("   ");
      for (i = 0; i < chunk; i++) printf("%c", (p[i] < 0x20)? '.' : p[i]);
      printf("\n");
      sz -= chunk, p += chunk;
   }
   printf("\n");
}

void color(int ink)
{
   SetConsoleTextAttribute(GetStdHandle(STD_OUTPUT_HANDLE), ink);
}

void err_win32(DWORD errcode)
{
   if (errcode == 0xFFFFFFFF) errcode = GetLastError();

   char msg[512];
   if (!FormatMessage(FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS, 0, errcode,
                  MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
                  msg, sizeof(msg), 0)) *msg = 0;

   trim(msg); CharToOem(msg, msg);

   color();
   printf((*msg)? "win32 error message: " : "win32 error code: ");
   color(CONSCLR_ERRCODE);
   if (*msg) printf("%s\n", msg); else printf("%04X\n", errcode);
   color();
}

void errmsg(const char *err, const char *str)
{
   color();
   printf("error: ");
   color(CONSCLR_ERROR);
   printf(err, str);
   color();
   printf("\n");
}

void __declspec(noreturn) errexit(const char *err, const char *str)
{
   errmsg(err, str);
   exit();
}


extern "C" void * _ReturnAddress(void);

#ifndef __INTEL_COMPILER
#pragma intrinsic(_ReturnAddress)
#pragma intrinsic(_byteswap_ulong)
#pragma intrinsic(_byteswap_ushort)
#endif
