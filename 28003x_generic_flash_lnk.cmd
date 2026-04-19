MEMORY
{
   BEGIN            : origin = 0x00080000, length = 0x00000002
   BOOT_RSVD        : origin = 0x00000002, length = 0x00000126

   RAMM0            : origin = 0x00000128, length = 0x000002D8
   RAMM1            : origin = 0x00000400, length = 0x000003F8
   // RAMM1_RSVD       : origin = 0x000007F8, length = 0x00000008 /* Reserve and do not use for code as per the errata advisory "Memory: Prefetching Beyond Valid Memory" */

   //**************************************************
   // Please update VCLA_initilaize() function when the 
   // local shared RAMs (LSxRAM) have been modified
   //**************************************************
   RAMLS01          : origin = 0x00008000, length = 0x00001000
   //RAMLS234         : origin = 0x00009000, length = 0x00001800
   RAMLS2345         : origin = 0x00009000, length = 0x00002000
   //RAMLS5           : origin = 0x0000A800, length = 0x00000800
   RAMLS6           : origin = 0x0000B000, length = 0x00000800
   RAMLS7           : origin = 0x0000B800, length = 0x00000800

   RAMGS0123		: origin = 0x0000C000, length = 0x00003FF8
   // RAMGS3_RSVD      : origin = 0x0000FFF8, length = 0x00000008 /* Reserve and do not use for code as per the errata advisory "Memory: Prefetching Beyond Valid Memory" */

   BOOTROM          : origin = 0x003F8000, length = 0x00007FC0
   SECURE_ROM       : origin = 0x003F2000, length = 0x00006000

   RESET            : origin = 0x003FFFC0, length = 0x00000002

   /* Flash sectors */
   FLASH            : origin = 0x080000 + 2, length = 0x2DFFF - 2
   // FLASH_BANK0_SEC15_RSVD     : origin = 0x0AFFF0, length = 0x000010  /* Reserve and do not use for code as per the errata advisory "Memory: Prefetching Beyond Valid Memory" */

   CLA1_MSGRAMLOW  	: origin = 0x001480, length = 0x000080
   CLA1_MSGRAMHIGH  : origin = 0x001500, length = 0x000080
}

SECTIONS
{
   codestart        : > BEGIN,  ALIGN(8)

   .text 			: >> FLASH, ALIGN(8)
   .cinit           : > FLASH,  ALIGN(8)
   .switch          : > FLASH,  ALIGN(8)

   .reset           : > RESET,  TYPE = DSECT /* not used, */

   .stack           : > RAMM1

   .init_array      : > FLASH,  ALIGN(8)
   .bss             : >> RAMLS7 | RAMGS0123
   .bss:output		: > RAMLS6
   .bss:cio         : > RAMLS01
   .data			: > RAMLS7 | RAMGS0123
   .sysmem          : > RAMLS7
   .const			: >> FLASH, ALIGN(8)

   ramgs0 : > RAMGS0123
   ramgs1 : > RAMGS0123

   /* Allocate IQ math areas */
   IQmath           : > FLASH, ALIGN(8)
   IQmathTables     : > FLASH, ALIGN(8)

   GROUP
   {
       .TI.ramfunc
       { -l FAPI_F28003x_EABI_v1.58.10.lib }

   } 				 LOAD = FLASH,
                     RUN = RAMLS01,
                     LOAD_START(RamfuncsLoadStart),
                     LOAD_SIZE(RamfuncsLoadSize),
                     LOAD_END(RamfuncsLoadEnd),
                     RUN_START(RamfuncsRunStart),
                     RUN_SIZE(RamfuncsRunSize),
                     RUN_END(RamfuncsRunEnd),
                     ALIGN(8)

   /* CLA specific sections */
   Cla1Prog        : LOAD = FLASH,
                     RUN = RAMLS2345,
                     LOAD_START(Cla1ProgLoadStart),
                     RUN_START(Cla1ProgRunStart),
                     LOAD_SIZE(Cla1ProgLoadSize),
                     ALIGN(8)

   .scratchpad     : > RAMLS6
   .bss_cla        : > RAMLS6
   Cla1ToCpuMsgRAM : > CLA1_MSGRAMLOW
   CpuToCla1MsgRAM : > CLA1_MSGRAMHIGH
   Cla1DataRam     : > RAMLS6
   cla_shared      : > RAMLS6
   // CLADataLS1      : >> RAMLS5 | RAMLS6
   CLADataLS1      : >> RAMLS6

   CLA1mathTables  : LOAD = FLASH,
                 	 RUN = RAMLS6,
                 	 LOAD_START(Cla1mathTablesLoadStart),
                 	 RUN_START(Cla1mathTablesRunStart)
                 	 LOAD_SIZE(Cla1mathTablesLoadSize),
                 	 ALIGN(8)

   .const_cla      : LOAD = FLASH,
                     RUN = RAMLS6,
                     RUN_START(Cla1ConstRunStart),
                     LOAD_START(Cla1ConstLoadStart),
                     LOAD_SIZE(Cla1ConstLoadSize),
                     ALIGN(8)
}
