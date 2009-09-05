
struct PC_KEY
{
   unsigned char vkey;
   unsigned char normal, shifted;
   unsigned char padd;
};

const PC_KEY pc_layout[] =
{
   { '1', 0x31, 0xB1 },
   { '2', 0x32, 0xB2 },
   { '3', 0x33, 0xB3 },
   { '4', 0x34, 0xB4 },
   { '5', 0x35, 0xB5 },
   { '6', 0x45, 0xE5 },
   { '7', 0x44, 0xC5 },
   { '8', 0x43, 0xF5 },
   { '9', 0x42, 0xC3 },
   { '0', 0x41, 0xC2 },
   { 0xBD, 0xE4, 0xC1 }, // -_
   { 0xBB, 0xE2, 0xE3 }, // =+

   { 'Q', 0x21, 0x29 },
   { 'W', 0x22, 0x2A },
   { 'E', 0x23, 0x2B },
   { 'R', 0x24, 0x2C },
   { 'T', 0x25, 0x2D },
   { 'Y', 0x55, 0x5D },
   { 'U', 0x54, 0x5C },
   { 'I', 0x53, 0x5B },
   { 'O', 0x52, 0x5A },
   { 'P', 0x51, 0x59 },
   { 0xDB, 0xD5, 0x94 }, // [{
   { 0xDD, 0xD4, 0x95 }, // ]}

   { 'A', 0x11, 0x19 },
   { 'S', 0x12, 0x1A },
   { 'D', 0x13, 0x1B },
   { 'F', 0x14, 0x1C },
   { 'G', 0x15, 0x1D },
   { 'H', 0x65, 0x6D },
   { 'J', 0x64, 0x6C },
   { 'K', 0x63, 0x6B },
   { 'L', 0x62, 0x6A },
   { 0xBA, 0xD2, 0x82 }, // ;:
   { 0xDE, 0xC4, 0xD1 }, // '"

   { 'Z', 0x02, 0x0A },
   { 'X', 0x03, 0x0B },
   { 'C', 0x04, 0x0C },
   { 'V', 0x05, 0x0D },
   { 'B', 0x75, 0x7D },
   { 'N', 0x74, 0x7C },
   { 'M', 0x73, 0x7B },
   { 0xBC, 0xF4, 0xA4 }, // ,<
   { 0xBE, 0xF3, 0xA5 }, // .>
   { 0xBF, 0x85, 0x84 }, // /?
   { 0xDC, 0x93, 0x92 }, // \|
};
