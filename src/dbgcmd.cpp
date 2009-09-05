
unsigned find1dlg(unsigned start) {
   static char ftext[12] = "";
   strcpy(str, ftext);
   filledframe(10,10,16,4);
   tprint(10,10,"  find string   ", FRM_HEADER);
   tprint(11,12,"text:", FFRAME_INSIDE);
   if (!inputhex(17,12,8,0)) return -1;
   strcpy(ftext, str);
   unsigned len = strlen(ftext);
   for (unsigned ptr = memadr(start+1); ptr != start; ptr = memadr(ptr+1)) {
      for (unsigned i = 0; i < len; i++)
         if (editrm(memadr(ptr+i)) != ftext[i]) break;
      if (i == len) return ptr;
   }
   tprint(11,12,"  not found   ", FFRAME_ERROR);
   debugflip();
   while (!process_msgs());
   return -1;
}
__declspec(naked) unsigned __fastcall swp(unsigned x) {
   __asm bswap ecx
   __asm mov eax,ecx
   __asm ret
}
unsigned find2dlg(unsigned start) {
   static unsigned code = 0xF3, mask = 0xFF; char ln[64];
   filledframe(10,10,16,5);
   tprint(10,10,"   find data    ", FRM_HEADER);
   sprintf(ln, "code: %08X", swp(code)); tprint(11,12,ln, FFRAME_INSIDE);
   sprintf(ln, "mask: %08X", swp(mask)); tprint(11,13,ln, FFRAME_INSIDE);
   sprintf(str, "%08X", swp(code));
   if (!inputhex(17,12,8,1)) return -1;
   sscanf(str, "%x", &code); code = swp(code);
   tprint(17,12,str, FFRAME_INSIDE);
   sprintf(str, "%08X", swp(mask));
   if (!inputhex(17,13,8,1)) return -1;
   sscanf(str, "%x", &mask); mask = swp(mask);
   for (unsigned ptr = memadr(start+1); ptr != start; ptr = memadr(ptr+1)) {
      unsigned char *cd = (unsigned char*)&code, *ms = (unsigned char*)&mask;
      for (unsigned i = 0; i < 4; i++)
         if ((editrm(memadr(ptr+i)) & ms[i]) != (cd[i] & ms[i])) break;
      if (i == 4) return ptr;
   }
   tprint(11,12,"  not found   ", FFRAME_ERROR);
   tprint(11,13,"              ", FFRAME_ERROR);
   debugflip();
   while (!process_msgs());
   return -1;
}
unsigned addr = 0, end = 0xFFFF;
char fname[20] = "";
char query_file_addr(unsigned char mode) { // mode: 0-load,1-save,2-disasm
   filledframe(6, 10, 26, 5);
   char ln[64];
   static char *titles[] = { "        load block        ",
                             "        save block        ",
                             "      disasm to file      " };
   tprint(6, 10, titles[mode], FRM_HEADER);
   sprintf(ln, "file: %s", fname); tprint(7, 12, ln, FFRAME_INSIDE);
   sprintf(ln, mode ? "start: %04X end: %04X" : "start: %04X", addr, end);
   tprint(7, 13, ln, FFRAME_INSIDE);
   strcpy(str, fname);
   if (!inputhex(13,12,18,0)) return 0;
   strcpy(fname, str); sprintf(ln, "%-18s", fname); tprint(13,12,ln,FFRAME_INSIDE);
   unsigned a1 = input4(14,13,addr); if (a1 == -1) return 0;
   addr = a1;
   tprint(14,13,str,FFRAME_INSIDE);
   unsigned e1 = 0xFFFF;
   if (mode) {
      e1 = input4(24,13,end); if (e1 == -1) return 0;
   }
   end = e1;
   return 1;
}
void saveload_dlg(unsigned char save) {
   if (!query_file_addr(save)) return;
   FILE *ff = fopen(fname, save ? "wb" : "rb");
   if (!ff) return;
   unsigned (__cdecl *fn)(void*,unsigned,unsigned,FILE*) = fread;
   if (save) fn = (unsigned(__cdecl*)(void*,unsigned,unsigned,FILE*))fwrite;
   for (unsigned a1 = addr; a1 <= end; a1++) {
      unsigned char bf = rmdbg(a1);
      if (fn(&bf, 1, 1, ff) != 1) break;
      if (!save) wmdbg(a1, bf);
   }
   end = a1-1;
   fclose(ff);
}
void mon_save() { saveload_dlg(1); }
void mon_load() { saveload_dlg(0); }

void mon_emul()
{
   dbgchk = isbrk();
   dbgbreak = 0;
}

void mon_scr(char alt)
{
   unsigned char prev_lock = conf.lockmouse; conf.lockmouse = 0;
   apply_video();
   conf.lockmouse = prev_lock;
   if (alt) comp.p7FFD ^= 8, set_banks(), draw_screen();
   flip(); if (conf.noflic) flip();
   if (alt) comp.p7FFD ^= 8, set_banks(), draw_screen(); // restore screen0 in screen buffer
   while (!process_msgs());
   temp.rflags = RF_MONITOR;
   set_video();
}

void mon_scr0() { mon_scr(0); }
void mon_scr1() { mon_scr(1); }
void mon_exitsub() {
   dbgchk = 1; dbgbreak = 0;
   dbg_stophere = rmdbg(cpu.sp)+0x100*rmdbg(cpu.sp+1);
}
void editbank() {
   unsigned x = input2(ports_x+5, ports_y+1, comp.p7FFD);
   if (x != -1) comp.p7FFD = x, set_banks();
}
void mon_nxt() {
   if (activedbg == WNDREGS) activedbg = WNDTRACE;
   else if (activedbg == WNDTRACE) activedbg = WNDMEM;
   else if (activedbg == WNDMEM) activedbg = WNDREGS;
}
void mon_prv() { mon_nxt(); mon_nxt(); }
void mon_dump() { mem_dump ^= 1; mem_sz = mem_dump ? 32:8; }

#define tool_x 18
#define tool_y 12

void mon_tool()
{
   static unsigned char unref = 0xCF;
   if (ripper) {
      OPENFILENAME ofn = { sizeof ofn };
      char savename[0x200]; *savename = 0;
      ofn.lpstrFilter = "Memory dump\0*.bin\0";
      ofn.lpstrFile = savename; ofn.nMaxFile = sizeof savename;
      ofn.lpstrTitle = "Save ripped data";
      ofn.Flags = OFN_PATHMUSTEXIST | OFN_HIDEREADONLY | OFN_OVERWRITEPROMPT;
      ofn.hwndOwner = wnd;
      ofn.lpstrDefExt = "bin";
      ofn.nFilterIndex = 1;
      if (GetSaveFileName(&ofn)) {
         for (unsigned i = 0; i < 0x10000; i++)
            snbuf[i] = (membits[i] & ripper) ? rmdbg(i) : unref;
         FILE *ff = fopen(savename, "wb");
         if (ff) fwrite(snbuf, 1, 0x10000, ff), fclose(ff);
      }
      ripper = 0;
   } else {
      filledframe(tool_x, tool_y, 17, 6);
      tprint(tool_x, tool_y, "  ripper's tool  ", FRM_HEADER);
      tprint(tool_x+1,tool_y+2, "trace reads:", FFRAME_INSIDE);
      *(unsigned*)str = 'Y';
      if (!inputhex(tool_x+15,tool_y+2,1,0)) return;
      tprint(tool_x+15,tool_y+2,str,FFRAME_INSIDE);
      if (*str == 'Y' || *str == 'y' || *str == '1') ripper |= MEMBITS_R;
      *(unsigned*)str = 'N';
      tprint(tool_x+1,tool_y+3, "trace writes:", FFRAME_INSIDE);
      if (!inputhex(tool_x+15,tool_y+3,1,0)) { ripper = 0; return; }
      tprint(tool_x+15,tool_y+3,str,FFRAME_INSIDE);
      if (*str == 'Y' || *str == 'y' || *str == '1') ripper |= MEMBITS_W;
      tprint(tool_x+1,tool_y+4, "unref. byte:", FFRAME_INSIDE);
      unsigned ub;
      if ((ub = input2(tool_x+14,tool_y+4,unref)) == -1) { ripper = 0; return; }
      unref = (unsigned char)ub;
      if (ripper) for (unsigned i = 0; i < 0x10000; i++) membits[i] &= ~(MEMBITS_R | MEMBITS_W);
   }
}

void mon_setup_dlg()
{
#ifdef MOD_SETTINGS
   setup_dlg();
   temp.rflags = RF_MONITOR;
   set_video();
#endif
}

void mon_scrshot() { show_scrshot ^= 1; }