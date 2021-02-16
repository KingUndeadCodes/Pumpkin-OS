extern "C" void main(){
   *((char*)0xb8000) = 'P'; // First      Char  : "H"
   *((char*)0xb8002) = 'O'; // Second     Char  : "E"
   *((char*)0xb8004) = 'G'; // Third      Char  : "L"
   *((char*)0xb8006) = '!'; // Fourth     Char  : "L"
   *((char*)0xb8008) = '!'; // Fifth      Char  : "O"
   return;
}

