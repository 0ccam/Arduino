/*
 * Эта часть программы похожа на прошивку контроллера терминала.
 */

#include <EEPROM.h>
#include <LiquidCrystal.h> /* подключаем встроенную в Arduino IDE библиотеку для дисплея LCD 16x2 */
#include <SoftwareSerial.h> // Программный UART.
#include "keyboard_4x4.h"
//#include "labeler.h"

#define Menu_Draw 0
#define Menu_Wait 1
#define Edit_Draw 2
#define Edit_Wait 3
#define Work_Draw 10
#define Work_Wait 11

#define DELAY 0 // Задержка подачи
#define SPEED 1 // Скорость подачи
#define MFEED 2 // Максимальная подача
#define TRSLD 3 // Порог ДРЭ
#define LADGE 4 // Вылет этикетки
#define WORK  5 // Работа
#define ERRR  6 // Ошибка

#define Jamp(state) State = state;
#define __state__(state) if (State == state)
#define __pressed__ if (key.pressed)
#define __key__(key, state) if (check_key(key, state))
#define Clear lcd.clear();
#define Print(row, pos, data) lcd.setCursor(pos, row); lcd.print(data);
#define Write(row, pos, data) lcd.setCursor(pos, row); lcd.write(data);

byte str0[] = {0xA4,0x61,0xE3,0x65,0x70,0xB6,0xBA,0x61,0x10,0xBE,0x6F,0xE3,0x61,0xC0,0xB8,0x00}; // Задержка подачи
byte str1[] = {0x43,0xBA,0x6F,0x70,0x6F,0x63,0xBF,0xC4,0x10,0xBE,0x6F,0xE3,0x61,0xC0,0xB8,0x00}; // Скорость подачи
byte str2[] = {0x4D,0x61,0xBA,0x63,0x2E,0x10,0xA8,0x6F,0xE3,0x61,0xC0,0x61,0x00}; // Максимальная подача при отсутствии данных от датчика разрыва этикетки.
byte str3[] = {0xA8,0x6F,0x70,0x6F,0xB4,0x10,0xE0,0x50,0xAF,0x00}; // Порог ДРЭ
byte str4[] = {0x42,0xC3,0xBB,0x65,0xBF,0x10,0xC5,0xBF,0xB8,0xBA,0x65,0xBF,0xBA,0xB8,0x00}; // Вылет этикетки
byte str5[] = {0x50,0x61,0xB2,0x6F,0xBF,0x61,0x00}; // Работа
byte str6[] = {0x4F,0xC1,0xB8,0xB2,0xBA,0x61,0x10,0x00}; // Ошибка
char* str[7] = {(char*)str0, (char*)str1, (char*)str2, (char*)str3, (char*)str4, (char*)str5, (char*)str6};


LiquidCrystal lcd(19, 18, 17, 16, 15, 14); /* номер вывода дисплея(вывод Arduino): RS(A5),E(A4),D4(A3),D5(A2),D6(A1),D7(A0) */
SoftwareSerial SoftSerial(2, 3); // RX, TX Программный UART.

byte State = Menu_Draw; // Начальное состояние конечного автомата.
byte pointer = 0;
byte temp;


boolean check_key (byte k, byte s){
  if (key.value == k){
    key.pressed = false;
    State = s;
    return true;
  } return false;
}

void Receive(byte raw){
  byte head = (raw >> 4) & 0x0F;
  byte tail = raw & 0x0F;
  if (head == 0xE){ 
    Print( 1, 0, str[ERRR] ) // Печать ошибки.
    lcd.print(tail);
  }
}

void Send(byte msg){
  Serial.write(msg);
  SoftSerial.write(msg);
}

void SendTwoNibble(byte head, byte data){
  Send( data >> 4 & 0x0F | head );
  Send( data & 0x0F | head );
}

void setup() {
  Serial.begin(9600);
  SoftSerial.begin(4800);
  lcd.begin(16, 2);// указываем тип дисплея LCD 16X2
  keyboard_init();
}

void loop() {
  
  keyboard_listen();

__state__( Menu_Draw ){ // Меню. Отрисовка.
  Clear
  Print( 0, 0, str[pointer] )
  Print( 1, 0, EEPROM[pointer] )
  Jamp( Menu_Wait )
}

__state__( Menu_Wait ){ // Меню. Ожидание ввода.
  __pressed__{
    __key__( X, Work_Draw ){} //Работа
    __key__( Y, Edit_Draw ){ temp = EEPROM[pointer]; }// Редактировать
    __key__( C, Menu_Draw ){ if (pointer > 3){ pointer = 0; }else{ pointer++; } } // Вверх 
    __key__( D, Menu_Draw ){ if (pointer < 1){ pointer = 4; }else{ pointer--; } } // Вниз
  }
}

__state__( Edit_Draw ){ // Редактировать. Отрисовка.
  Clear
  Print( 0, 0, str[pointer] )
  Write( 1, 0, 0x3E )
  Print( 1, 1, temp )
  Jamp( Edit_Wait )
}

__state__( Edit_Wait ){ // Редактировать. Ожидание ввода.
  __pressed__{
    __key__( X, Menu_Draw ){ EEPROM[pointer] = temp; } // Сохранить и выйти.
    __key__( Y, Menu_Draw ){} // Выйти без сохранения.
    __key__( C, Edit_Draw ){ if (temp == 255){ temp = 0; }else{ temp++; } } // Вверх 
    __key__( D, Edit_Draw ){ if (temp == 0){ temp = 255; }else{ temp--; } } // Вниз
  }
}

__state__( Work_Draw ){ // Работа. Отрисовка
  Clear
  Print( 0, 0, str[WORK] )
  Jamp( Work_Wait )
  // При переходе в рабочий режим загрузка настроек.
  SendTwoNibble( 0x00, EEPROM[DELAY] ); // Задержка подачи
  SendTwoNibble( 0x10, EEPROM[SPEED] ); // Скорость подачи
  SendTwoNibble( 0x20, EEPROM[MFEED] ); // Максимальная подача
  SendTwoNibble( 0x30, EEPROM[TRSLD] ); // Порог ДРЭ
  SendTwoNibble( 0x40, EEPROM[LADGE] ); // Вылет этикетки
  Send( 0xBA ); // Старт.
}

__state__( Work_Wait ){ // Меню. Ожидание ввода. 
  __pressed__{
    key.pressed = false;
    Send( 0xBF ); // Стоп.
    Jamp( Menu_Draw )
  }

  // Приём данных по последовательным портам.
  while(Serial.available()) { Receive(Serial.read()); }
  while(SoftSerial.available()) { Receive(SoftSerial.read()); }

}
}
