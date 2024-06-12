#include <Wire.h>

#define  address    0x26

#define  row_1      0b11101111
#define  row_2      0b11011111
#define  row_3      0b10111111
#define  row_4      0b01111111
#define  column_no  0b11111111
#define  column_1   0b11111110
#define  column_2   0b11111101
#define  column_3   0b11111011
#define  column_4   0b11110111

#define _A_       10
#define _B_       11
#define _C_       12
#define _D_       13
#define _Star_    14
#define _Sharp_   15
#define _Null_    255
#define _Error_   250

byte read_port(byte row);
 
void setup(){
    Wire.begin();    
    Serial.begin(9600);
} 

void loop(){
  byte i = get_key();
  if (i != _Null_) Serial.println(i);
}


byte read_port(byte adr, byte row){

  // Передача данных по I2C в PCF8574T.
  Wire.beginTransmission(adr);  // Начало процедуры передачи.
  Wire.write(0xFF);             // Добавить в очередь для отправки байт конфигурации порта с подтяжкой всех пинов к плюсу..
  Wire.write(row);              // Добавить в очередь для отправки байт конфигурации порта с нужным номером строки.
  Wire.endTransmission();       // Отправить данные из очереди, и завершить передачу.
  
  // Чтение данных по I2C из PCF8574T.
  Wire.requestFrom(adr, 1);     // Запросить 1 байт у устройства с адресом adr
  return Wire.read();           // Прочесть и вернуть байт состояния порта.

}

byte get_key(){

  byte input;
  byte key = _Null_;
  static byte old_key;

  input = read_port(address, row_1) | 0xF0;
  if      (input == column_no) {;}
  else if (input == column_1) key = 1;
  else if (input == column_2) key = 2;
  else if (input == column_3) key = 3;
  else if (input == column_4) key = _A_;
  else key = _Error_;

  input = read_port(address, row_2) | 0xF0;
  if      (input == column_no) {;}
  else if (input == column_1) key = 4;
  else if (input == column_2) key = 5;
  else if (input == column_3) key = 6;
  else if (input == column_4) key = _B_;
  else key = _Error_;

  input = read_port(address, row_3) | 0xF0;
  if      (input == column_no) {;}
  else if (input == column_1) key = 7;
  else if (input == column_2) key = 8;
  else if (input == column_3) key = 9;
  else if (input == column_4) key = _C_;
  else key = _Error_;

  input = read_port(address, row_4) | 0xF0;
  if      (input == column_no) {;}
  else if (input == column_1) key = _Star_;
  else if (input == column_2) key = 0;
  else if (input == column_3) key = _Sharp_;
  else if (input == column_4) key = _D_;
  else key = _Error_;

  if (old_key == _Null_ && key != _Null_){
    old_key = key;
    return key;
  }else{
    old_key = key;
    return _Null_;
  }
  
}
