//******************************************
// PC Engine & TurboGrafx dump code by tamanegi_taro
// April 18th 2018 Revision 1.0.1 Initial version
// August 12th 2019 Revision 1.0.2 Added Tennokoe Bank support
//
// Special thanks
// sanni - Arduino cart reader
// skaman - ROM size detection
// NO-INTRO - CRC list for game name detection
// Chris Covell - Tennokoe bank support
//
//******************************************

/******************************************
  Defines
 *****************************************/
#define HUCARD 0
#define TURBOCHIP 1

#define DETECTION_SIZE 64
#define CHKSUM_SKIP 0
#define CHKSUM_OK 1
#define CHKSUM_ERROR 2

/******************************************
   Prototype Declarations
 *****************************************/
/* Several PCE dedicated functions */
void pin_read_write_PCE(void);
void pin_init_PCE(void);
void setup_cart_PCE(void);
void reset_cart_PCE(void);
uint8_t read_byte_PCE(uint32_t address);
void write_byte_PCE(uint32_t address, uint8_t data);
uint32_t detect_rom_size_PCE(void);
void read_bank_PCE(uint32_t address_start, uint32_t address_end, uint32_t *processed_size, uint32_t total_size);
void read_rom_PCE(void);

/******************************************
   Variables
 *****************************************/
uint8_t pce_internal_mode; //0 - HuCARD, 1 - TurboChip

/******************************************
  Menu
*****************************************/
// PCE start menu
static const char pceMenuItem1[] PROGMEM = "HuCARD";
static const char pceMenuItem2[] PROGMEM = "Turbochip";
static const char pceMenuItem3[] PROGMEM = "Reset";
static const char* const menuOptionspce[] PROGMEM = {pceMenuItem1, pceMenuItem2, pceMenuItem3};

// PCE card menu items
static const char pceCartMenuItem1[] PROGMEM = "Read Rom";
static const char pceCartMenuItem2[] PROGMEM = "Read Tennokoe Bank";
static const char pceCartMenuItem3[] PROGMEM = "Write Tennokoe Bank";
static const char pceCartMenuItem4[] PROGMEM = "Reset";
static const char* const menuOptionspceCart[] PROGMEM = {pceCartMenuItem1, pceCartMenuItem2, pceCartMenuItem3, pceCartMenuItem4};

// Turbochip menu items
static const char pceTCMenuItem1[] PROGMEM = "Read Rom";
static const char pceTCMenuItem2[] PROGMEM = "Reset";
static const char* const menuOptionspceTC[] PROGMEM = {pceTCMenuItem1, pceTCMenuItem2};

// PCE start menu
void pcsMenu(void) {
  // create menu with title and 3 options to choose from
  unsigned char pceDev;
  // Copy menuOptions out of progmem
  convertPgm(menuOptionspce, 3);
  pceDev = question_box(F("Select device"), menuOptions, 3, 0);

  // wait for user choice to come back from the question box menu
  switch (pceDev)
  {
    case 0:
      //Hucard
      display_Clear();
      display_Update();
      pce_internal_mode = HUCARD;
      setup_cart_PCE();
      mode = mode_PCE;
      break;

    case 1:
      //Turbografx
      display_Clear();
      display_Update();
      pce_internal_mode = TURBOCHIP;
      setup_cart_PCE();
      mode = mode_PCE;
      break;

    case 2:
      resetArduino();
      break;
  }
}

void pin_read_write_PCE(void)
{
  // Set Address Pins to Output
  //A0-A7
  DDRF = 0xFF;
  //A8-A15
  DDRK = 0xFF;
  //A16-A19
  DDRL = (DDRL & 0xF0) | 0x0F;

  //Set Control Pin to Output CS(PL4)
  DDRL |= (1 << 4);

  //Set CS(PL4) to HIGH
  PORTL |= (1 << 4);

  // Set Control Pins to Output RST(PH0) RD(PH3) WR(PH5)
  DDRH |= (1 << 0) | (1 << 3) | (1 << 5);
  // Switch all of above to HIGH
  PORTH |= (1 << 0) | (1 << 3) | (1 << 5);

  // Set IRQ(PH4) to Input
  DDRH &= ~(1 << 4);
  // Activate Internal Pullup Resistors
  PORTH |= (1 << 4);

  // Set Data Pins (D0-D7) to Input
  DDRC = 0x00;

  // Enable Internal Pullups
  PORTC = 0xFF;

  reset_cart_PCE();
}

void pin_init_PCE(void)
{

  //Set Address Pins to input and pull up
  DDRF = 0x00;
  PORTF = 0xFF;
  DDRK = 0x00;
  PORTK = 0xFF;
  DDRL = 0x00;
  PORTL = 0xFF;
  DDRH &= ~((1 << 0) | (1 << 3) | (1 << 5) | (1 << 6));
  PORTH = (1 << 0) | (1 << 3) | (1 << 5) | (1 << 6);

  // Set IRQ(PH4) to Input
  DDRH &= ~(1 << 4);
  // Activate Internal Pullup Resistors
  PORTH |= (1 << 4);

  // Set Data Pins (D0-D7) to Input
  DDRC = 0x00;
  // Enable Internal Pullups
  PORTC = 0xFF;

}

void setup_cart_PCE(void)
{
  // Set cicrstPin(PG1) to Output
  DDRG |= (1 << 1);
  // Output a high to disable CIC
  PORTG |= (1 << 1);

  pin_init_PCE();

}

void reset_cart_PCE(void)
{
  //Set RESET as Low
  PORTH &= ~(1 << 0);
  delay(200);
  //Set RESET as High
  PORTH |= (1 << 0);
  delay(200);

}

void set_address_PCE(uint32_t address)
{
  //Set address
  PORTF = address & 0xFF;
  PORTK = (address >> 8) & 0xFF;
  PORTL = (PORTL & 0xF0) | ((address >> 16) & 0x0F);
}

uint8_t read_byte_PCE(uint32_t address)
{
  uint8_t ret;

  set_address_PCE(address);

  // Arduino running at 16Mhz -> one nop = 62.5ns -> 1000ns total
  __asm__("nop\n\t""nop\n\t""nop\n\t""nop\n\t""nop\n\t""nop\n\t""nop\n\t""nop\n\t""nop\n\t""nop\n\t""nop\n\t""nop\n\t""nop\n\t""nop\n\t""nop\n\t""nop\n\t");

  // Set CS(PL4) and RD(PH3) as LOW
  PORTL &= ~(1 << 4);
  PORTH &= ~(1 << 3);

  // Arduino running at 16Mhz -> one nop = 62.5ns -> 1000ns total
  __asm__("nop\n\t""nop\n\t""nop\n\t""nop\n\t""nop\n\t""nop\n\t""nop\n\t""nop\n\t""nop\n\t""nop\n\t""nop\n\t""nop\n\t""nop\n\t""nop\n\t""nop\n\t""nop\n\t");

  //read byte
  ret =  PINC;

  //Swap bit order for PC Engine HuCARD
  if (pce_internal_mode == HUCARD)
  {
    ret = ((ret & 0x01) << 7) | ((ret & 0x02) << 5) | ((ret & 0x04) << 3) | ((ret & 0x08) << 1) | ((ret & 0x10) >> 1) | ((ret & 0x20) >> 3) | ((ret & 0x40) >> 5) | ((ret & 0x80) >> 7);
  }

  // Set CS(PL4) and RD(PH3) as HIGH
  PORTL |= (1 << 4);
  PORTH |= (1 << 3);

  //return read data
  return ret;

}

void write_byte_PCE(uint32_t address, uint8_t data)
{
  set_address_PCE(address);

  // Arduino running at 16Mhz -> one nop = 62.5ns -> 1000ns total
  __asm__("nop\n\t""nop\n\t""nop\n\t""nop\n\t""nop\n\t""nop\n\t""nop\n\t""nop\n\t""nop\n\t""nop\n\t""nop\n\t""nop\n\t""nop\n\t""nop\n\t""nop\n\t""nop\n\t");

  //Swap bit order for PC Engine HuCARD
  if (pce_internal_mode == HUCARD)
  {
    data = ((data & 0x01) << 7) | ((data & 0x02) << 5) | ((data & 0x04) << 3) | ((data & 0x08) << 1) | ((data & 0x10) >> 1) | ((data & 0x20) >> 3) | ((data & 0x40) >> 5) | ((data & 0x80) >> 7);
  }

  //write byte
  PORTC = data;

  // Set Data Pins (D0-D7) to Output
  DDRC = 0xFF;

  // Set CS(PL4) and WR(PH5) as LOW
  PORTL &= ~(1 << 4);
  PORTH &= ~(1 << 5);

  // Arduino running at 16Mhz -> one nop = 62.5ns -> 1000ns total
  __asm__("nop\n\t""nop\n\t""nop\n\t""nop\n\t""nop\n\t""nop\n\t""nop\n\t""nop\n\t""nop\n\t""nop\n\t""nop\n\t""nop\n\t""nop\n\t""nop\n\t""nop\n\t""nop\n\t");

  // Set CS(PL4) and WR(PH5) as HIGH
  PORTL |= (1 << 4);
  PORTH |= (1 << 5);

  // Set Data Pins (D0-D7) to Input
  DDRC = 0x00;

  // Enable Internal Pullups
  PORTC = 0xFF;
}

//Confirm the size of ROM - 128Kb, 256Kb, 384Kb, 512Kb, 768Kb or 1024Kb
uint32_t detect_rom_size_PCE(void)
{
  uint32_t rom_size;
  uint8_t read_byte;
  uint8_t current_byte;
  uint8_t detect_128, detect_256, detect_512, detect_768;

  //Initialize variables
  detect_128 = 0;
  detect_256 = 0;
  detect_512 = 0;
  detect_768 = 0;

  //Set pins to read PC Engine cart
  pin_read_write_PCE();

  //Confirm where mirror address start from(128KB, 256KB, 512KB, 768, or 1024KB)
  for (current_byte = 0; current_byte < DETECTION_SIZE; current_byte++) {
    if ((current_byte != detect_128) && (current_byte != detect_256) && (current_byte != detect_512) && (current_byte != detect_768))
    {
      //If none matched, it is 1024KB
      break;
    }

    //read byte for 128KB, 256KB, 512KB detection
    read_byte = read_byte_PCE(current_byte);

    //128KB detection
    if (current_byte == detect_128)
    {
      if (read_byte_PCE(current_byte + 128UL * 1024UL) == read_byte)
      {
        detect_128++;
      }
    }

    //256KB detection
    if (current_byte == detect_256)
    {
      if (read_byte_PCE(current_byte + 256UL * 1024UL) == read_byte)
      {
        detect_256++;
      }
    }

    //512KB detection
    if (current_byte == detect_512)
    {
      if (read_byte_PCE(current_byte + 512UL * 1024UL) == read_byte)
      {
        detect_512++;
      }
    }

    //768KB detection
    read_byte = read_byte_PCE(current_byte + 512UL * 1024UL);
    if (current_byte == detect_768)
    {
      if (read_byte_PCE(current_byte + 768UL * 1024UL) == read_byte)
      {
        detect_768++;
      }
    }
  }

  //debug
  //sprintf(fileName, "%d %d %d %d", detect_128, detect_256, detect_512, detect_768); //using filename global variable as string. Initialzed in below anyways.
  //println_Msg(fileName);

  //ROM size detection by result
  if (detect_128 == DETECTION_SIZE)
  {
    rom_size = 128;
  }
  else if (detect_256 == DETECTION_SIZE)
  {
    if (detect_512 == DETECTION_SIZE)
    {
      rom_size = 256;
    }
    else
    {
      //Another confirmation for 384KB because 384KB hucard has data in 0x0--0x40000 and 0x80000--0xA0000(0x40000 is mirror of 0x00000)
      rom_size = 384;
    }
  }
  else if (detect_512 == DETECTION_SIZE)
  {
    rom_size = 512;
  }
  else if (detect_768 == DETECTION_SIZE)
  {
    rom_size = 768;
  }
  else
  {
    rom_size = 1024;
  }

  //If rom size is more than or equal to 512KB, detect Street fighter II'
  if (rom_size >= 512)
  {
    //Look for "NEC HE "
    if (read_byte_PCE(0x7FFF9) == 'N' && read_byte_PCE(0x7FFFA) == 'E'  && read_byte_PCE(0x7FFFB) == 'C'
        && read_byte_PCE(0x7FFFC) == ' ' && read_byte_PCE(0x7FFFD) == 'H' && read_byte_PCE(0x7FFFE) == 'E')
    {
      rom_size = 2560;
    }
  }

  return rom_size;
}

/* Must be address_start and address_end should be 512 byte aligned */
void read_bank_PCE(uint32_t address_start, uint32_t address_end, uint32_t *processed_size, uint32_t total_size)
{
  uint32_t currByte;
  uint16_t c;

  for (currByte = address_start; currByte < address_end; currByte += 512) {
    for (c = 0; c < 512; c++) {
      sdBuffer[c] = read_byte_PCE(currByte + c);
    }
    myFile.write(sdBuffer, 512);
    *processed_size += 512;
    draw_progressbar(*processed_size, total_size);
  }
}

//Get line from file and convert upper case to lower case
void skip_line(SdFile* readfile)
{
  int i = 0;
  char str_buf;

  while (readfile->available())
  {
    //Read 1 byte from file
    str_buf = readfile->read();

    //if end of file or newline found, execute command
    if (str_buf == '\r')
    {
      readfile->read(); //dispose \n because \r\n
      break;
    }
    i++;
  }//End while
}

//Get line from file and convert upper case to lower case
void get_line(char* str_buf, SdFile* readfile, uint8_t maxi)
{
  int i = 0;

  while (readfile->available())
  {
    //If line size is more than maximum array, limit it.
    if (i >= maxi)
    {
      i = maxi - 1;
    }

    //Read 1 byte from file
    str_buf[i] = readfile->read();

    //if end of file or newline found, execute command
    if (str_buf[i] == '\r')
    {
      str_buf[i] = '\0';
      readfile->read(); //dispose \n because \r\n
      break;
    }
    i++;
  }//End while
}

uint32_t calculate_crc32(int n, unsigned char c[], uint32_t r)
{
  int i, j;

  for (i = 0; i < n; i++) {
    r ^= c[i];
    for (j = 0; j < 8; j++)
      if (r & 1) r = (r >> 1) ^ 0xEDB88320UL;
      else       r >>= 1;
  }
  return r;
}

void crc_search(char *file_p, char *folder_p, uint32_t rom_size)
{
  SdFile rom, script;
  uint32_t r, crc, processedsize;
  char gamename[100];
  char crc_file[9], crc_search[9];
  uint8_t flag;
  flag = CHKSUM_SKIP;

  //Open list file. If no list file found, just skip
  sd.chdir("/"); //Set read directry to root
  if (script.open("PCE_CRC_LIST.txt", O_READ))
  {
    //Calculate CRC of ROM file
    sd.chdir(folder_p);
    if (rom.open(file_p, O_READ))
    {
      //Initialize flag as error
      flag = CHKSUM_ERROR;
      crc = 0xFFFFFFFFUL; //Initialize CRC
      display_Clear();
      println_Msg(F("Calculating chksum..."));
      processedsize = 0;
      draw_progressbar(0, rom_size * 1024UL); //Initialize progress bar

      while (rom.available())
      {
        r = rom.read(sdBuffer, 512);
        crc = calculate_crc32(r, sdBuffer, crc);
        processedsize += r;
        draw_progressbar(processedsize, rom_size * 1024UL);
      }

      crc = crc ^ 0xFFFFFFFFUL; //Finish CRC calculation and progress bar
      draw_progressbar(rom_size * 1024UL, rom_size * 1024UL);

      //Display calculated CRC
      sprintf(crc_file, "%08lX", crc);

      //Search for same CRC in list
      while (script.available()) {
        //Read 2 lines (game name and CRC)
        get_line(gamename, &script, 96);
        get_line(crc_search, &script, 9);
        skip_line(&script); //Skip every 3rd line

        //if checksum search successful, rename the file and end search
        if (strcmp(crc_search, crc_file) == 0)
        {
          print_Msg(F("Chksum OK "));
          println_Msg(crc_file);
          print_Msg(F("Saved to "));
          print_Msg(folder_p);
          print_Msg(F("/"));
          print_Msg(gamename);
          print_Msg(F(".pce"));
          flag = CHKSUM_OK;
          strcat(gamename, ".pce");
          rom.rename(sd.vwd(), gamename);
          break;
        }
      }
      rom.close();
    }
  }


  if (flag == CHKSUM_SKIP)
  {
    print_Msg(F("Saved to "));
    print_Msg(folder_p);
    print_Msg(F("/"));
    print_Msg(file_p);
  }
  else if (flag == CHKSUM_ERROR)
  {
    print_Msg(F("Chksum Error "));
    println_Msg(crc_file);
    print_Msg(F("Saved to "));
    print_Msg(folder_p);
    print_Msg(F("/"));
    print_Msg(file_p);
  }

  script.close();

}


void read_tennokoe_bank_PCE(void)
{
  uint32_t processed_size = 0;
  uint32_t verify_loop;
  uint8_t verify_flag = 1;

  //clear the screen
  display_Clear();

  println_Msg(F("RAM size: 8KB"));

  // Get name, add extension and convert to char array for sd lib
  strcpy(fileName, "BANKRAM");
  strcat(fileName, ".sav");

  // create a new folder for the save file
  EEPROM_readAnything(0, foldern);
  sprintf(folder, "PCE/ROM/%s/%d", romName, foldern);
  sd.mkdir(folder, true);
  sd.chdir(folder);

  println_Msg(F("Saving RAM..."));
  display_Update();

  // write new folder number back to eeprom
  foldern = foldern + 1;
  EEPROM_writeAnything(0, foldern);

  //open file on sd card
  if (!myFile.open(fileName, O_RDWR | O_CREAT)) {
    print_Error(F("Can't create file on SD"), true);
  }

  pin_read_write_PCE();

  //Initialize progress bar by setting processed size as 0
  draw_progressbar(0, 8 * 1024UL);

  //Unlock Tennokoe Bank RAM
  write_byte_PCE(0x0D0000, 0x68); //Unlock RAM sequence 1 Bank 68
  write_byte_PCE(0x0F0000, 0x0); //Unlock RAM sequence 2 Bank 78
  write_byte_PCE(0x0F0000, 0x73); //Unlock RAM sequence 3 Bank 78
  write_byte_PCE(0x0F0000, 0x73); //Unlock RAM sequence 4 Bank 78
  write_byte_PCE(0x0F0000, 0x73); //Unlock RAM sequence 5 Bank 78

  //Read Tennokoe bank RAM
  read_bank_PCE(0x080000, 0x080000 + 8 * 1024UL, &processed_size, 8 * 1024UL);

  myFile.seekSet(0);    // Go back to file beginning
  processed_size = 0;

  //Verify Tennokoe bank RAM
  for (verify_loop = 0; verify_loop < 8 * 1024UL; verify_loop++)
  {
    if (myFile.read() != read_byte_PCE(verify_loop + 0x080000))
    {
      verify_flag = 0;
      draw_progressbar(8 * 1024UL, 8 * 1024UL);
      break;
    }
    draw_progressbar(verify_loop, 8 * 1024UL);
  }

  //If verify flag is 0, verify failed
  if (verify_flag == 1)
  {
    println_Msg(F("Verify OK..."));
  }
  else
  {
    println_Msg(F("Verify failed..."));
  }

  //Lock Tennokoe Bank RAM
  write_byte_PCE(0x0D0000, 0x68); //Lock RAM sequence 1 Bank 68
  write_byte_PCE(0x0F0001, 0x0); //Lock RAM sequence 2 Bank 78
  write_byte_PCE(0x0C0001, 0x60); //Lock RAM sequence 3 Bank 60

  pin_init_PCE();

  //Close the file:
  myFile.close();

}

void write_tennokoe_bank_PCE(void)
{
  uint32_t readwrite_loop, verify_loop;
  uint32_t verify_flag = 1;

  //Display file Browser and wait user to select a file. Size must be 8KB.
  filePath[0] = '\0';
  sd.chdir("/");
  fileBrowser(F("Select RAM file"));
  // Create filepath
  sprintf(filePath, "%s/%s", filePath, fileName);
  display_Clear();

  //open file on sd card
  if (myFile.open(filePath, O_READ)) {

    fileSize = myFile.fileSize();
    if (fileSize != 8 * 1024UL) {
      println_Msg(F("File must be 1MB"));
      display_Update();
      myFile.close();
      wait();
      return;
    }

    pin_read_write_PCE();

    //Unlock Tennokoe Bank RAM
    write_byte_PCE(0x0D0000, 0x68); //Unlock RAM sequence 1 Bank 68
    write_byte_PCE(0x0F0000, 0x0); //Unlock RAM sequence 2 Bank 78
    write_byte_PCE(0x0F0000, 0x73); //Unlock RAM sequence 3 Bank 78
    write_byte_PCE(0x0F0000, 0x73); //Unlock RAM sequence 4 Bank 78
    write_byte_PCE(0x0F0000, 0x73); //Unlock RAM sequence 5 Bank 78

    //Write file to Tennokoe BANK RAM
    for (readwrite_loop = 0; readwrite_loop < 8 * 1024UL; readwrite_loop++)
    {
      write_byte_PCE(0x080000 + readwrite_loop, myFile.read());
      draw_progressbar(readwrite_loop, 8 * 1024UL);
    }

    myFile.seekSet(0);    // Go back to file beginning

    for (verify_loop = 0; verify_loop < 8 * 1024UL; verify_loop++)
    {
      if (myFile.read() != read_byte_PCE(verify_loop + 0x080000))
      {
        draw_progressbar(2 * 1024UL, 8 * 1024UL);
        verify_flag = 0;
        break;
      }
      draw_progressbar(verify_loop, 8 * 1024UL);
    }

    //If verify flag is 0, verify failed
    if (verify_flag == 1)
    {
      println_Msg(F("Verify OK..."));
    }
    else
    {
      println_Msg(F("Verify failed..."));
    }

    //Lock Tennokoe Bank RAM
    write_byte_PCE(0x0D0000, 0x68); //Lock RAM sequence 1 Bank 68
    write_byte_PCE(0x0F0001, 0x0); //Lock RAM sequence 2 Bank 78
    write_byte_PCE(0x0C0001, 0x60); //Lock RAM sequence 3 Bank 60

    pin_init_PCE();

    // Close the file:
    myFile.close();
    println_Msg(F("Finished"));
    display_Update();
    wait();
  }
  else {
    print_Error(F("File doesn't exist"), false);
  }
}

void read_rom_PCE(void)
{
  uint32_t rom_size;
  uint32_t processed_size = 0;

  //clear the screen
  display_Clear();
  rom_size = detect_rom_size_PCE();

  print_Msg(F("Detected size: "));
  print_Msg(rom_size);
  println_Msg(F("KB"));

  // Get name, add extension and convert to char array for sd lib
  strcpy(fileName, "PCEROM");
  strcat(fileName, ".pce");

  // create a new folder for the save file
  EEPROM_readAnything(0, foldern);
  sprintf(folder, "PCE/ROM/%s/%d", romName, foldern);
  sd.mkdir(folder, true);
  sd.chdir(folder);

  println_Msg(F("Saving ROM..."));
  display_Update();

  // write new folder number back to eeprom
  foldern = foldern + 1;
  EEPROM_writeAnything(0, foldern);

  //open file on sd card
  if (!myFile.open(fileName, O_RDWR | O_CREAT)) {
    print_Error(F("Can't create file on SD"), true);
  }

  pin_read_write_PCE();

  //Initialize progress bar by setting processed size as 0
  draw_progressbar(0, rom_size * 1024UL);

  if (rom_size == 384)
  {
    //Read two sections. 0x000000--0x040000 and 0x080000--0x0A0000 for 384KB
    read_bank_PCE(0, 0x40000, &processed_size, rom_size * 1024UL);
    read_bank_PCE(0x80000, 0xA0000, &processed_size, rom_size * 1024UL);
  }
  else if (rom_size == 2560)
  {
    //Dump Street fighter II' Champion Edition
    read_bank_PCE(0, 0x80000, &processed_size, rom_size * 1024UL);  //Read first bank
    write_byte_PCE(0x1FF0, 0xFF); //Display second bank
    read_bank_PCE(0x80000, 0x100000, &processed_size, rom_size * 1024UL); //Read second bank
    write_byte_PCE(0x1FF1, 0xFF); //Display third bank
    read_bank_PCE(0x80000, 0x100000, &processed_size, rom_size * 1024UL); //Read third bank
    write_byte_PCE(0x1FF2, 0xFF); //Display forth bank
    read_bank_PCE(0x80000, 0x100000, &processed_size, rom_size * 1024UL); //Read forth bank
    write_byte_PCE(0x1FF3, 0xFF); //Display fifth bank
    read_bank_PCE(0x80000, 0x100000, &processed_size, rom_size * 1024UL); //Read fifth bank
  }
  else
  {
    //Read start form 0x000000 and keep reading until end of ROM
    read_bank_PCE(0, rom_size * 1024UL, &processed_size, rom_size * 1024UL);
  }

  pin_init_PCE();

  //Close the file:
  myFile.close();

  //CRC search and rename ROM
  crc_search(fileName, folder, rom_size);

}




// PC Engine Menu
void pceMenu() {
  // create menu with title and 7 options to choose from
  unsigned char mainMenu;

  if (pce_internal_mode == HUCARD)
  {
    // Copy menuOptions out of progmem
    convertPgm(menuOptionspceCart, 4);
    mainMenu = question_box(F("PCE HuCARD menu"), menuOptions, 4, 0);

    // wait for user choice to come back from the question box menu
    switch (mainMenu)
    {
      case 0:
        display_Clear();
        // Change working dir to root
        sd.chdir("/");
        read_rom_PCE();
        break;
      case 1:
        display_Clear();
        read_tennokoe_bank_PCE();
        break;
      case 2:
        display_Clear();
        write_tennokoe_bank_PCE();
        break;
      case 3:
        resetArduino();
        break;
    }
  }
  else
  {
    // Copy menuOptions out of progmem
    convertPgm(menuOptionspceTC, 2);
    mainMenu = question_box(F("TG TurboChip menu"), menuOptions, 2, 0);

    // wait for user choice to come back from the question box menu
    switch (mainMenu)
    {
      case 0:
        display_Clear();
        // Change working dir to root
        sd.chdir("/");
        read_rom_PCE();
        break;

      case 1:
        resetArduino();
        break;
    }
  }

  println_Msg(F(""));
  println_Msg(F("Press Button..."));
  display_Update();
  wait();
}


//******************************************
// End of File
//******************************************
