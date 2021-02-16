extern "C" void main(){
   *((char*)0xb8000) = 'H'; // First      Char  : "H"
   *((char*)0xb8002) = 'E'; // Second     Char  : "E"
   *((char*)0xb8004) = 'L'; // Third      Char  : "L"
   *((char*)0xb8006) = 'L'; // Fourth     Char  : "L"
   *((char*)0xb8008) = 'O'; // Fifth      Char  : "O"
   *((char*)0xb8010) = ' '; // Sixth      Char  : " "
   *((char*)0xb8012) = 'W'; // Seventh    Char  : "W"
   *((char*)0xb8014) = 'O'; // Eighth     Char  : "O"
   *((char*)0xb8016) = 'R'; // Ninth      Char  : "R"
   *((char*)0xb8018) = 'L'; // Tenth      Char  : "L"
   *((char*)0xb8020) = 'D'; // Eleventh   Char  : "D"
   *((char*)0xb8022) = '!'; // Twelfth    Char  : "!"
   return;
}
