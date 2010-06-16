#include "std.h"

#include "emul.h"
#include "vars.h"
#include "draw.h"
#include "memory.h"
#include "atm.h"
#include "sound.h"
#include "gs.h"
#include "sdcard.h"
#include "zc.h"
#include "tape.h"

void out(unsigned port, unsigned char val)
{
   port &= 0xFFFF;
   brk_port_out = port; brk_port_val = val;

   if (comp.flags & CF_DOSPORTS)
   {
      if (conf.ide_scheme == IDE_ATM && (port & 0x1F) == 0x0F)
      {
         if (port & 0x100) { comp.ide_write = val; return; }
      write_hdd_5:
         port >>= 5;
      write_hdd:
         port &= 7;
         if (port)
             hdd.write(port, val);
         else
             hdd.write_data(val + (comp.ide_write << 8));
         return;
      }

      if ((port & 0x18A3) == (0xFFFE & 0x18A3))
      { // SMUC
         if (conf.smuc)
         {
            if ((port & 0xA044) == (0xDFBA & 0xA044))
            { // clock
               if (comp.pFFBA & 0x80)
                   cmos_write(val);
               else
                   comp.cmos_addr = val;
               return;
            }
            if ((port & 0xA044) == (0xFFBA & 0xA044))
            { // SMUC system port
               if ((val & 1) && (conf.ide_scheme == IDE_SMUC))
                   hdd.reset();
               comp.nvram.write(val);
               comp.pFFBA = val;
               return;
            }
            if ((port & 0xA044) == (0x7FBA & 0xA044))
            {
                comp.p7FBA = val;
                return;
            }
         }
         if ((port & 0x8044) == (0xFFBE & 0x8044) && conf.ide_scheme == IDE_SMUC)
         { // FFBE, FEBE
            if(comp.pFFBA & 0x80)
            {
                if(!(port & 0x100))
                    hdd.write(8, val); // control register
                return;
            }

            if (!(port & 0x2000))
            {
                comp.ide_write = val;
                return;
            }
            port >>= 8;
                goto write_hdd;
         }
      }

      unsigned char p1 = (unsigned char)port;
      if (conf.mem_model == MM_ATM710)
      {
         if ((p1 & 0x9F) == 0x17)
         {
             set_atm_FF77(port, val);
             return;
         }
         if ((p1 & 0x9F) == 0x97)
         {
             comp.pFFF7[((comp.p7FFD & 0x10) >> 2) + ((port >> 14) & 3)] = val ^ 0xFF;
             set_banks();
             return;
         }
         if ((p1 & 0x9F) == 0x9F && !(comp.aFF77 & 0x4000))
             atm_writepal(val); // don't return - write to TR-DOS system port
      }

      if(conf.mem_model == MM_PROFI)
      {
          if((comp.p7FFD & 0x10) && (comp.pDFFD & 0x20)) // modified ports
          {
              // BDI ports
              if((p1 & 0x9F) == 0x83) // WD93 ports
              {
                  comp.wd.out((p1 & 0x60) | 0x1F, val);
                  return;
              }

              if((p1 & 0xE3) == 0x23) // port FF
              {
                  comp.wd.out(0xFF, val);
                  return;
              }

              // RTC
              if((port & 0x9F) == 0x9F && conf.cmos)
              {
                if(port & 0x20)
                {
                    comp.cmos_addr = val;
                    return;
                }
                cmos_write(val);
                return;
              }

              // IDE (AB=10101011, CB=11001011, EB=11101011)
              if ((p1 & 0x9F) == 0x8B && (conf.ide_scheme == IDE_PROFI))
              {
                  if(p1 & 0x40)
                  {    // cs1
                      if (!(p1 & 0x20))
                      {
                          comp.ide_write = val;
                          return;
                      }
                      port >>= 8;
                      goto write_hdd;              
                  }

                  // cs3
                  if(p1 & 0x20)
                  {
                      if(((port>>8) & 7) == 6)
                          hdd.write(8, val);
                      return;
                  }
              }
          }
          else
          {
              // BDI ports
              if((p1 & 0x83) == 0x03)  // WD93 ports 1F, 3F, 7F
              {
                  comp.wd.out((p1 & 0x60) | 0x1F,val);
                  return;
              } 

              if((p1 & 0xE3) == ((comp.pDFFD & 0x20) ? 0xA3 : 0xE3)) // port FF
              {
                  comp.wd.out(0xFF,val);
                  return;
              }
          }        
      }

      if ((p1 & 0x1F) == 0x1F)
      {
          comp.wd.out(p1, val);
          return;
      }
      // don't return - out to port #FE works in trdos!
   }
   else
   {
         if ((unsigned char)port == 0x1F && conf.sound.ay_scheme == AY_SCHEME_POS)
         {
             comp.active_ay = val & 1;
             return;
         }
         if (!(port & 6) && (conf.ide_scheme == IDE_NEMO || conf.ide_scheme == IDE_NEMO_A8))
         {
             unsigned hi_byte = (conf.ide_scheme == IDE_NEMO)? (port & 1) : (port & 0x100);
             if (hi_byte)
             {
                 comp.ide_write = val;
                 return;
             }
             if ((port & 0x18) == 0x08)
             {
                 if ((port & 0xE0) == 0xC0)
                     hdd.write(8, val);
                 return;
             } // CS1=0,CS0=1,reg=6
             if ((port & 0x18) != 0x10)
                 return; // invalid CS0,CS1
             goto write_hdd_5;
         }
   }

   #ifdef MOD_VID_VD
   if ((unsigned char)port == 0xDF)
   {
       comp.pVD = val;
       comp.vdbase = (comp.pVD & 4)? vdmem[comp.pVD & 3] : 0;
       return;
   }
   #endif

   // port #FE
   // scorp  xx1xxx10 /dos=1 (sc16 green)
   // others xxxxxxx0
   if (!(conf.mem_model == MM_SCORP || conf.mem_model == MM_PROFSCORP) && !(port & 1) ||
       (conf.mem_model == MM_SCORP || conf.mem_model == MM_PROFSCORP) && !(comp.flags & CF_DOSPORTS) && (port & 0x23) == (0xFE & 0x23))
   {
//[vv]      assert(!(val & 0x08));

      spkr_dig = (val & 0x10) ? conf.sound.beeper_vol : 0;
      mic_dig = (val & 0x08) ? conf.sound.micout_vol : 0;

      // speaker & mic
      if ((comp.pFE ^ val) & 0x18)
      {
//          __debugbreak();
          flush_dig_snd();
      }


      unsigned char new_border = (val & 7);
      if (conf.mem_model == MM_ATM710 || conf.mem_model == MM_ATM450)
          new_border += ((port & 8) ^ 8);
      if (comp.border_attr ^ new_border)
          update_screen();
      comp.border_attr = new_border;

      if (conf.mem_model == MM_ATM450)
          set_atm_aFE((unsigned char)port);

      if (conf.mem_model == MM_PROFI)
      {
        if(!(port & 0x80) && (comp.pDFFD & 0x80))
        {
          comp.comp_pal[(~comp.pFE) & 0xF] = ~(port>>8);
          temp.comp_pal_changed = 1;
        }
      }

      comp.pFE = val;
      // do not return! intro to navy seals (by rst7) uses out #FC for to both FE and 7FFD
   }

   // #xD
   if (!(port & 2))
   {

      if (conf.sound.covoxDD && (unsigned char)port == 0xDD)
      { // port DD - covox
//         __debugbreak();
         flush_dig_snd();
         covDD_vol = val*conf.sound.covoxDD_vol/0x100;
         return;
      }

      if (!(port & 0x8000)) // zx128 port
      {
         // 0001xxxxxxxxxx0x (bcig4)
         if ((port & 0xF002) == (0x1FFD & 0xF002) && conf.mem_model == MM_PLUS3)
             goto set1FFD;

         if ((port & 0xC003) == (0x1FFD & 0xC003) && conf.mem_model == MM_KAY)
             goto set1FFD;
                       
         // 00xxxxxxxx1xxx01 (sc16 green)
         if ((port & 0xC023) == (0x1FFD & 0xC023) && (conf.mem_model == MM_SCORP || conf.mem_model == MM_PROFSCORP))
         {
set1FFD:
            comp.p1FFD = val;
            set_banks();
            return;
         }

         // gmx
         if(port == 0x7EFD && conf.mem_model == MM_PROFSCORP)
         {
            comp.p7EFD = val;
            set_banks();
            return;
         }

         if (conf.mem_model == MM_ATM450 && (port & 0x8202) == (0x7DFD & 0x8202))
         {
             atm_writepal(val);
             return;
         }

         // if (conf.mem_model == MM_ATM710 && (port & 0x8202) != (0x7FFD & 0x8202)) return; // strict 7FFD decoding on ATM-2

         // 01xxxxxxxx1xxx01 (sc16 green)
         if ((port & 0xC023) != (0x7FFD & 0xC023) && (conf.mem_model == MM_SCORP || conf.mem_model == MM_PROFSCORP))
             return;
         // 7FFD
         if (comp.p7FFD & 0x20)
         { // 48k lock
            // #EFF7.2 forces lock
            if ((comp.pEFF7 & EFF7_LOCKMEM) && conf.mem_model == MM_PENTAGON && conf.ramsize == 1024)
                return;

            // if not pentagon-1024 or profi with #DFFD.4 set, apply lock
            if (!((conf.ramsize == 1024 && conf.mem_model == MM_PENTAGON) ||
                  (conf.mem_model == MM_PROFI && (comp.pDFFD & 0x10)))) // molodcov_alex
                return;
         }

         if ((comp.p7FFD ^ val) & 0x08)
             update_screen();

         comp.p7FFD = val;
         set_banks();
         return;
      }

      // xx0xxxxxxxxxxx0x (3.2) [vv]
      if ((port & 0x2002) == (0xDFFD & 0x2002) && conf.mem_model == MM_PROFI)
      {
          comp.pDFFD = val;
          set_banks();
          return;
      }

      if (conf.mem_model == MM_ATM450 && (port & 0x8202) == (0xFDFD & 0x8202))
      {
          comp.pFDFD = val;
          set_banks();
          return;
      }

      if ((port & 0xC0FF) == 0xC0FD)
      { // A15=A14=1, FxFD - AY select register
         if ((conf.sound.ay_scheme == AY_SCHEME_CHRV) && ((val & 0xF8) == 0xF8)) //Alone Coder
         {
             if (conf.sound.ay_chip == (SNDCHIP::CHIP_YM2203))
             {
                 fmsoundon0 = val & 4;
                 tfmstatuson0 = val & 2;
             } //Alone Coder 0.36.6
             comp.active_ay = val & 1;
         };
         unsigned n_ay = (conf.sound.ay_scheme == AY_SCHEME_QUADRO)? (port >> 12) & 1 : comp.active_ay;
         ay[n_ay].select(val);
         return;
      }

      if ((port & 0xC000)==0x8000 && conf.sound.ay_scheme)
      {  // BFFD - AY data register
         unsigned n_ay = (conf.sound.ay_scheme == AY_SCHEME_QUADRO)? (port >> 12) & 1 : comp.active_ay;
         ay[n_ay].write(temp.sndblock? 0 : cpu.t, val);
         if (conf.input.mouse == 2 && ay[n_ay].get_activereg() == 14)
             input.aymouse_wr(val);
         return;
      }
      return;
   }

   #ifdef MOD_GS
   if ((port & 0xFF) == 0x33 && conf.gs_type) // ngs
   {
       out_gs(port, val);
       return;
   }
   if ((port & 0xF7) == 0xB3 && conf.gs_type)
   {
       out_gs(port, val);
       return;
   }
   #endif

   if (conf.zc && (port & 0xFF) == 0x57)
   {
       Zc.Wr(port, val);
       return;
   }

   if (conf.sound.sd && (port & 0xAF) == 0x0F)
   { // soundrive
//      __debugbreak();
      if ((unsigned char)port == 0x0F) comp.p0F = val;
      if ((unsigned char)port == 0x1F) comp.p1F = val;
      if ((unsigned char)port == 0x4F) comp.p4F = val;
      if ((unsigned char)port == 0x5F) comp.p5F = val;
      flush_dig_snd();
      sd_l = (conf.sound.sd_vol * (comp.p0F+comp.p1F)) >> 8;
      sd_r = (conf.sound.sd_vol * (comp.p4F+comp.p5F)) >> 8;
      return;
   }
   if (conf.sound.covoxFB && !(port & 4))
   { // port FB - covox
//      __debugbreak();
      flush_dig_snd();
      covFB_vol = val*conf.sound.covoxFB_vol/0x100;
      return;
   }
   if (port == 0xEFF7 && conf.mem_model == MM_PENTAGON)
   {
      unsigned char oldpEFF7 = comp.pEFF7; //Alone Coder 0.36.4
      comp.pEFF7 = (comp.pEFF7 & conf.EFF7_mask) | (val & ~conf.EFF7_mask);
      comp.pEFF7 |= EFF7_GIGASCREEN; // [vv] disable turbo
//    if ((comp.pEFF7 ^ oldpEFF7) & EFF7_GIGASCREEN) {
//      conf.frame = frametime;
//      if ((conf.mem_model == MM_PENTAGON)&&(comp.pEFF7 & EFF7_GIGASCREEN))conf.frame = 71680;
//      apply_sound();
//    } //Alone Coder removed 0.37.1

      if (!(comp.pEFF7 & EFF7_4BPP))
      {
          temp.offset_vscroll = 0;
          temp.offset_vscroll_prev = 0;
          temp.offset_hscroll = 0;
          temp.offset_hscroll_prev = 0;
      }

      if ((comp.pEFF7 ^ oldpEFF7) & (EFF7_ROCACHE | EFF7_LOCKMEM))
          set_banks(); //Alone Coder 0.36.4
      return;
   }
   if (conf.cmos && (comp.pEFF7 & EFF7_CMOS) && conf.mem_model == MM_PENTAGON)
   {
      if (port == 0xDFF7)
      {
          comp.cmos_addr = val;
          return;
      }
      if (port == 0xBFF7)
      {
          cmos_write(val);
          return;
      }
   }
   if ((port & 0xF8FF) == 0xF8EF && modem.open_port)
       modem.write((port >> 8) & 7, val);
}

__inline unsigned char in1(unsigned port)
{
   port &= 0xFFFF;
   brk_port_in = port;

/*
   if((port & 0xFF) == 0xF0)
       __debugbreak();
*/

   if (comp.flags & CF_DOSPORTS)
   {
      if (conf.ide_scheme == IDE_ATM && (port & 0x1F) == 0x0F)
      {
         if (port & 0x100)
             return comp.ide_read;
      read_hdd_5:
         port >>= 5;
      read_hdd:
         port &= 7;
         if (port)
             return hdd.read(port);
         unsigned v = hdd.read_data();
         comp.ide_read = (unsigned char)(v >> 8);
         return (unsigned char)v;
      }

      if ((port & 0x18A3) == (0xFFFE & 0x18A3))
      { // SMUC
         if (conf.smuc)
         {
            if ((port & 0xA044) == (0xDFBA & 0xA044)) return cmos_read(); // clock
            if ((port & 0xA044) == (0xFFBA & 0xA044)) return comp.nvram.out; // SMUC system port
            if ((port & 0xA044) == (0x7FBA & 0xA044)) return comp.p7FBA | 0x3F;
            if ((port & 0xA044) == (0x5FBA & 0xA044)) return 0x3F;
            if ((port & 0xA044) == (0x5FBE & 0xA044)) return 0x57;
            if ((port & 0xA044) == (0x7FBE & 0xA044)) return 0x57;
         }
         if ((port & 0x8044) == (0xFFBE & 0x8044) && conf.ide_scheme == IDE_SMUC)
         { // FFBE, FEBE
            if(comp.pFFBA & 0x80)
            {
                if(!(port & 0x100))
                    return hdd.read(8); // alternate status
                return 0xFF; // obsolete register
            }

            if (!(port & 0x2000))
                return comp.ide_read;
            port >>= 8;
            goto read_hdd;
         }
      }

      unsigned char p1 = (unsigned char)port;

      if (conf.mem_model == MM_PROFI) // molodcov_alex
      {
          if((comp.p7FFD & 0x10) && (comp.pDFFD & 0x20))
          { // modified ports
            // BDI ports
            if((p1 & 0x9F) == 0x83)
                return comp.wd.in((p1 & 0x60) | 0x1F);  // WD93 ports (1F, 3F, 7F)
            if((p1 & 0xE3) == 0x23)
                return comp.wd.in(0xFF);                // port FF

            // RTC
            if((port & 0x9F) == 0x9F && conf.cmos)
            {
                if(!(port & 0x20))
                    return cmos_read();
            }

            // IDE
            if((p1 & 0x9F) == 0x8B && (conf.ide_scheme == IDE_PROFI))
            {
                if(p1 & 0x40) // cs1
                {
                    if (p1 & 0x20)
                        return comp.ide_read;
                    port >>= 8;
                    goto read_hdd;      
                }
            }
          }
          else
          {
              // BDI ports
              if((p1 & 0x83) == 0x03)
                  return comp.wd.in((p1 & 0x60) | 0x1F);  // WD93 ports
              if((p1 & 0xE3) == ((comp.pDFFD & 0x20) ? 0xA3 : 0xE3))
                  return comp.wd.in(0xFF);                // port FF
          }        
      }
      

      if ((p1 & 0x1F) == 0x1F)
          return comp.wd.in(p1);
   }
   else
   {
      if (!(port & 6) && (conf.ide_scheme == IDE_NEMO || conf.ide_scheme == IDE_NEMO_A8))
      {
         unsigned hi_byte = (conf.ide_scheme == IDE_NEMO)? (port & 1) : (port & 0x100);
         if(hi_byte)
             return comp.ide_read;
         comp.ide_read = 0xFF;
         if((port & 0x18) == 0x08)
             return ((port & 0xE0) == 0xC0)? hdd.read(8) : 0xFF; // CS1=0,CS0=1,reg=6
         if((port & 0x18) != 0x10)
             return 0xFF; // invalid CS0,CS1
         goto read_hdd_5;
      }
   }

   #ifdef MOD_GS
   if ((port & 0xF7) == 0xB3 && conf.gs_type)
       return in_gs(port);
   #endif

   if (conf.zc && (port & 0xFF) == 0x57)
       return Zc.Rd(port);

   if (!(port & 0x20))
   { // kempstons
      port = (port & 0xFFFF) | 0xFA00; // A13,A15 not used in decoding
      if ((port == 0xFADF || port == 0xFBDF || port == 0xFFDF) && conf.input.mouse == 1)
      { // mouse
         input.mouse_joy_led |= 1;
         if (port == 0xFBDF)
             return input.kempston_mx();
         if (port == 0xFFDF)
             return input.kempston_my();
         return input.mbuttons;
      }
      input.mouse_joy_led |= 2;
      unsigned char res = (conf.input.kjoy)? input.kjoy : 0xFF;
      if (conf.mem_model == MM_SCORP || conf.mem_model == MM_PROFSCORP)
         res = (res & 0x1F) | (comp.wd.in(0xFF) & 0xE0);
      return res;
   }

   // port #FE
   // scorp  xx1xxx10 (sc16)
   // others xxxxxxx0
   if (!(conf.mem_model == MM_SCORP || conf.mem_model == MM_PROFSCORP) && !(port & 1) ||
       (conf.mem_model == MM_SCORP || conf.mem_model == MM_PROFSCORP) && !(comp.flags & CF_DOSPORTS) && (port & 0x23) == (0xFE & 0x23))
   {
      if ((cpu.pc & 0xFFFF) == 0x0564 && bankr[0][0x0564]==0x1F && conf.tape_autostart && !comp.tape.play_pointer)
          start_tape();
      u8 val = input.read(port >> 8);
      if (conf.mem_model == MM_ATM450)
          val = (val & 0x7F) | atm450_z(cpu.t);
      return val;
   }

   if ((port & 0x8202) == (0x7FFD & 0x8202) && (conf.mem_model == MM_ATM710 || conf.ide_scheme == IDE_ATM))
   { // ATM-2 IDE+DAC/ADC
      unsigned char irq = 0x40;
      if (conf.ide_scheme == IDE_ATM) irq = (hdd.read_intrq() & 0x40);
      return irq + 0x3F;
   }

   if ((unsigned char)port == 0xFD && conf.sound.ay_scheme)
   {
      if((conf.sound.ay_scheme == AY_SCHEME_CHRV) && (conf.sound.ay_chip == (SNDCHIP::CHIP_YM2203)) && (tfmstatuson0 == 0))
          return 0x7f /*always ready*/; //Alone Coder 0.36.6
      if ((port & 0xC0FF) != 0xC0FD) return 0xFF;
      unsigned n_ay = (conf.sound.ay_scheme == AY_SCHEME_QUADRO)? (port >> 12) & 1 : comp.active_ay;
      // else FxFD - read selected AY register
      if (conf.input.mouse == 2 && ay[n_ay].get_activereg() == 14) { input.mouse_joy_led |= 1; return input.aymouse_rd(); }
      return ay[n_ay].read();
   }

//   if ((port & 0x7F) == 0x7B) { // FB/7B
   if ((port & 0x04) == 0x00)
   { // FB/7B //Alone Coder 0.36.6 (for MODPLAYi)
      if (conf.mem_model == MM_ATM450)
      {
         comp.aFB = (unsigned char)port;
         set_banks();
      }
      else if (conf.cache)
      {
         comp.flags &= ~CF_CACHEON;
         if (port & 0x80) comp.flags |= CF_CACHEON;
         set_banks();
      }
      return 0xFF;
   }

   if (port == 0xBFF7 && conf.cmos && (comp.pEFF7 & EFF7_CMOS))
       return cmos_read();

   if ((port & 0xF8FF) == 0xF8EF && modem.open_port)
       return modem.read((port >> 8) & 7);

   if ((port & 0xFF) == 0xFF)
   {
      update_screen();
      if (vmode != 2) return 0xFF; // ray is not in paper
      unsigned ula_t = (cpu.t+temp.border_add) & temp.border_and;
      return temp.base[vcurr->atr_offs + (ula_t - vcurr[-1].next_t)/4];
   }
   return 0xFF;
}

unsigned char in(unsigned port)
{
   brk_port_val = in1(port);
   return brk_port_val;
}

#undef in_trdos
#undef out_trdos
