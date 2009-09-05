
void cpu_info()
{
   char idstr[64]; fillCpuString(idstr); trim(idstr);
   unsigned cpuver = cpuid(1,0);
   unsigned features = cpuid(1,1);
   temp.mmx = (features >> 23) & 1;
   temp.sse2 = (features >> 26) & 1;
   temp.cpufq = GetCPUFrequency();

   color(CONSCLR_HARDITEM); printf("cpu: ");

   color(CONSCLR_HARDINFO);
   printf("%s ", idstr);

   color(CONSCLR_HARDITEM);
   printf("%d.%d.%d [MMX:%s,SSE2:%s] ",
      (cpuver>>8) & 0x0F, (cpuver>>4) & 0x0F, cpuver & 0x0F,
      temp.mmx ? "YES" : "NO",
      temp.sse2 ? "YES" : "NO");

   color(CONSCLR_HARDINFO);
   printf("at %d MHz\n", (unsigned)(temp.cpufq/1000000));

#ifdef MOD_SSE2
   if (!temp.sse2) {
      color(CONSCLR_WARNING);
      printf("warning: this is an SSE2 build, recompile or download non-P4 version\n");
   }
#else MOD_SSE2
   if (temp.sse2) {
      color(CONSCLR_WARNING);
      printf("warning: SSE2 disabled in compile-time, recompile or download P4 version\n");
   }
#endif
}

void restrict_version(char legacy)
{
   static const char vererror[] = "unsupported OS version";

   if (!legacy) errexit(vererror);

   #ifdef MOD_9X
   static const char verwarning[] =
      "detected windows version is not supported\r\n"
      "by current version of UnrealSpeccy and untested.\r\n\r\n"
      "you may click 'YES' to continue on your own risk.\r\n"
      "in this case you will experience crashes under\r\n"
      "some conditions. it's an OS-specific problem, please\r\n"
      "don't report it and consider using NT-based system";
   if (MessageBox(0, verwarning, vererror, MB_ICONERROR | MB_YESNO | MB_DEFBUTTON2) == IDNO) exit();
   #endif
}

void init_all(int argc, char **argv)
{
   cpu_info();

   char *config = 0, legacy = 0;
   for (int i = 0; i < argc; i++) {
      if (argv[i][0] != '/' && argv[i][0] != '-') continue;
      if (!stricmp(argv[i]+1, "i") && i+1 < argc)
         config = argv[i+1], i++;
      #ifdef MOD_9X
      if (argv[i][1] == '9') legacy = 1;
      #endif
   }
   if (GetVersion() >> 31) restrict_version(legacy);

   init_z80tables();
   video_permanent_tables();
   init_ie_help();
   load_config(config);
   make_samples();
   #ifdef MOD_GS
   init_gs();
   #endif
   init_leds();
   init_tape();
   init_hdd_cd();
   start_dx();
   applyconfig();
   main_reset();
   for (; argc; argc--, argv++) {
      if (**argv == '-' || **argv == '/') {
         if (argc > 1 && !stricmp(argv[0]+1, "i")) argc--, argv++;
         continue;
      }
      char fname[0x200], *temp;
      GetFullPathName(*argv, sizeof fname, fname, &temp);
      if (!loadsnap(fname)) errmsg("error loading <%s>", *argv), load_errors = 1;
   }
/*
   if (load_errors) {
      printf("press ESC to exit, any other key to continue...");
      char c = getch(); printf(clrline);
      if (c == 0x1B) exit(0);
   }
*/
   SetCurrentDirectory(conf.workdir);
   timeBeginPeriod(1);
}

void __declspec(noreturn) exit()
{
   exitflag = 1;
   if (savesndtype) savesnddialog();
   done_tape();
   done_dx();
   done_leds();
   save_nv();
   modem.close();
   done_ie_help();
   timeEndPeriod(1);
   color();
   printf("\nsee you later!");
   if (!nowait) SetConsoleTitle("press a key..."), getch();
   ExitProcess(0);
}