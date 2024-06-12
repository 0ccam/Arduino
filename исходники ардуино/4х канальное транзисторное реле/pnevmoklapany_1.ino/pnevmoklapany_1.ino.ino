#include <Wire.h>                                             // Подключаем библиотеку для работы с аппаратной шиной I2C.
#include <iarduino_I2C_Relay.h>                               // Подключаем библиотеку для работы с реле и силовыми ключами.
#include "timer-api.h"
iarduino_I2C_Relay pwrkey(0x09);                              // Объявляем объект pwrkey для работы с функциями и методами библиотеки iarduino_I2C_Relay, указывая адрес модуля на шине I2C.
                                                              // Если объявить объект без указания адреса (iarduino_I2C_Relay pwrkey;), то адрес будет найден автоматически.
#define       work  500                                       // Время активности выходных каналов.
#define       InpA  4                                         // Входной пин канала А.
#define       InpB  5                                         // Входной пин канала В.
byte          A = 0, B = 0;                                   // Переменные состояния каналов А и В.
unsigned int  Ta = 0, Tb = 0;                                 // Счётчики времени каналов А и В.

void setup(){                                                 //
    pwrkey.begin();                                           // Инициируем работу с модулем.
    pwrkey.digitalWrite(ALL_CHANNEL, LOW);                    // Выключаем все каналы модуля.
    pwrkey.setCurrentProtection(ALL_CHANNEL, 0.5, CURRENT_DISABLE); // Отключение каналов при нагрузке свыше 0.5А
    pinMode (InpA, INPUT_PULLUP);                             // Установка пинов на вход с подтягаванием к плюсу питания через внутренний резистор.
    pinMode (InpB, INPUT_PULLUP); 
    timer_init_ISR_1KHz(TIMER_DEFAULT);                       // Инициализация таймера с частотой 1КГц.
}           

void timer_handle_interrupts(int timer) {                     // Обработчик прерывания по таймеру.
    Ta++;
    Tb++;
}


void loop(){     
                                               


switch(A){ //// Канал A ////

  case 0: // Ожидание низкого фронта входного импульса. Канал A.
  
      if (!digitalRead(InpA)){
        pwrkey.digitalWrite(1,HIGH); // Реле 1 включить
        Ta = 0;
        A = 1;
        }
      break;
      
  case 1: // Ожидание истечения времени work. Канал A.
  
      if (Ta >= work){
        pwrkey.digitalWrite(1,LOW); // Реле 1 выключить
        Ta = 0;
        A = 2;
        }
      break;
      
      
  case 2: // Ожидание высокого фронта входного импульса. Канал A.
  
      if (digitalRead(InpA)){
        pwrkey.digitalWrite(2,HIGH);  // Реле 2 включить
        Ta = 0;
        A = 3;
        }
      break;
      
  case 3: // Ожидание истечения времени work. Канал A.
  
      if (Ta >= work){
        pwrkey.digitalWrite(2,LOW); // Реле 2 выключить
        Ta = 0;
        A = 0;
        }
      break;
      
}

   
switch (B){ //// Канал B ////

  case 0: // Ожидание низкого фронта входного импульса. Канал B.
  
      if (!digitalRead(InpB)){
        pwrkey.digitalWrite(3,HIGH); // Реле 3 включить
        Tb = 0;
        B = 1;
        }
      break;
      
  case 1: // Ожидание истечения времени work. Канал B.
  
      if (Tb >= work){
        pwrkey.digitalWrite(3,LOW); // Реле 3 выключить
        Tb = 0;
        B = 2;
        }
      break;
      
      
  case 2: // Ожидание высокого фронта входного импульса. Канал B.
  
      if (digitalRead(InpB)){
        pwrkey.digitalWrite(4,HIGH);  // Реле 4 включить
        Tb = 0;
        B = 3;
        }
      break;
      
  case 3:// Ожидание истечения времени work. Канал B.
  
      if (Tb >= work){
        pwrkey.digitalWrite(4,LOW); // Реле 4 выключить
        Tb = 0;
        B = 0;
        }
      break;
      

}}                      
