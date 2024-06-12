////////////////////////////////////////////////////////////////////////////
// Прошивка этикеровщика маленькой линии. 
////////////////////////////////////////////////////////////////////////////

#include <Arduino.h>
#include <TM1637TinyDisplay.h>
#include "EEPROM.h"

#define CLK           12  // К этому пину подключаем CLK
#define DIO           11  // К этому пину подключаем DIO
#define StepperMotor  10  // К этому пину подключаем шаговый двигатель
#define Digit_Button  9   // Максимальный циферный символ на клавиатуре
#define A_Button      10  // Символ A на клавиатуре
#define B_Button      11  // Символ B на клавиатуре
#define C_Button      12  // Символ C на клавиатуре
#define D_Button      13  // Символ D на клавиатуре
#define Star_Button   14  // Символ * на клавиатуре
#define Sharp_Button  15  // Символ # на клавиатуре

TM1637TinyDisplay disp(CLK, DIO);

boolean Restart = true;
byte    Setings[4];
char    *ABCD[] = {"A","b","C","d"};
int     Blink = 0;
byte    Button = 255; // Текущее значение клавиатуры.
byte    T_Button = 255; // Значение клавиатуры на предыдущей итерации.
byte    btn; // Значение нажатой кнопки длительностью в одну итеррацию (событие).
boolean Bottle = false;
boolean T_Bottle = false;
byte    Work_State = 1; //
byte    Param = 0;
int     temp;
byte    Speed; // Скорость подачи растёт от 99 к 0.
byte    Delay; // Задержка подачи этикетки.
byte    Vylet; // Величина вылета.
byte    Rezerv; // 
boolean draw = true; // Флаг необходимости перерисовки дисплея.

////////////////////////////////////////////////////////////////////////////
// Функции
////////////////////////////////////////////////////////////////////////////

byte Press_Button(){
  
  if (T_Button != Button)  
    return Button;
  return 255;}

void Sto_Shagov(byte s){
  
  for (int j = 0; j < 100; j++){
    digitalWrite(StepperMotor, HIGH);
    delayMicroseconds(255 - s);
    digitalWrite(StepperMotor, LOW);
    delayMicroseconds(255 - s);}}
    
////////////////////////////////////////////////////////////////////////////
// Начальные установки
////////////////////////////////////////////////////////////////////////////

void setup(){
  
  Serial.begin(9600);
  // Устанавливаем яркость
  disp.setBrightness(0x0f);
  disp.clear();
  pinMode (StepperMotor, OUTPUT); // Шаговый двигатель
  pinMode (13, OUTPUT); // Светодиод на плате
  pinMode (A0, INPUT_PULLUP); // Датчик бутылки A0
  pinMode (A1, INPUT); // Датчик разрыва этикетки A1
  pinMode (A2, OUTPUT);
  pinMode (A3, OUTPUT);// Выход датировщика.
  pinMode (A6, OUTPUT);// Выход драйвера ШД
  digitalWrite(A2,HIGH);
  // Столбцы клавиатуры на входы. Подтягивание входов к плюсу через внутренние резисторы
  pinMode (9, INPUT_PULLUP); // 1
  pinMode (8, INPUT_PULLUP); // 2
  pinMode (7, INPUT_PULLUP); // 3
  pinMode (6, INPUT_PULLUP); // 4
  // Ряды клавиатуры на выходы.
  pinMode (5, OUTPUT); // 1
  pinMode (4, OUTPUT); // 2
  pinMode (3, OUTPUT); // 3
  pinMode (2, OUTPUT); // 4
  digitalWrite(2,HIGH);
  digitalWrite(3,HIGH);
  digitalWrite(4,HIGH);
  digitalWrite(5,HIGH);
  digitalWrite(A3, LOW); 
  }

////////////////////////////// Главный цикл ////////////////////////////////

void loop(){

// Чтение параметров из долговременной памяти при запуске программы.

  if (Restart){
    for(int i=0; i<4; i++){
      Setings[i] = EEPROM.read(i);}
    Restart = false;}

//////////////////// Опрос датчика разрыва этикетки ///////////////////

  //int X = analogRead(A1);
  //Serial.println(X);
  if (analogRead(A1) > 300) {
    digitalWrite(13,HIGH); // Зажечь светодиод на плате.
    digitalWrite(A3,HIGH); // Низкий уровень на датировщик.
    }
  else {
    digitalWrite(13, LOW); // Потушить светодиод на плате.
    digitalWrite(A3, LOW); // Высокий уровень на датировщик.
    }

////////////////////////// Опрос клавиатуры ///////////////////////////

  T_Button = Button;
  Button = 255; // Отпущенные кнопки.

  digitalWrite(5,LOW);
  if      (!digitalRead(9)) Button = 1;
  else if (!digitalRead(8)) Button = 2;
  else if (!digitalRead(7)) Button = 3;
  else if (!digitalRead(6)) Button = 10;
  digitalWrite(5,HIGH);

  digitalWrite(4,LOW);
  if      (!digitalRead(9)) Button = 4;
  else if (!digitalRead(8)) Button = 5;
  else if (!digitalRead(7)) Button = 6;
  else if (!digitalRead(6)) Button = 11;
  digitalWrite(4,HIGH);

  digitalWrite(3,LOW);
  if      (!digitalRead(9)) Button = 7;
  else if (!digitalRead(8)) Button = 8;
  else if (!digitalRead(7)) Button = 9;
  else if (!digitalRead(6)) Button = 12;
  digitalWrite(3,HIGH);

  digitalWrite(2,LOW);
  if      (!digitalRead(9)) Button = 14;
  else if (!digitalRead(8)) Button = 0;
  else if (!digitalRead(7)) Button = 15;
  else if (!digitalRead(6)) Button = 13;
  digitalWrite(2,HIGH);

  btn = 255; // Событие отсутствует.
  if ((T_Button == 255) && (Button != 255)) { // Если значение кнопки изменилось с пустого на нажатое:
    btn = Button; // Присвоить событию значение нажатой кнопки.
  }


///////////// Состояние 1. Главное меню. Выбор сетинга. /////////////////

  if (Work_State == 1) {

    if (draw){ // Процедура перерисовки дисплея
      disp.showString(ABCD[Param], 1, 0);        // Char, length=1, position=0
      disp.showNumber(int(Setings[Param]), false, 3, 1); // int num, bool leading_zero = false, uint8_t length = MAXDIGITS, uint8_t pos = 2 
      draw = false; // Сбросить флаг
    }

    switch (btn) {
      
      case 255: break;
      
      case A_Button: {
        Param = 0;
        draw = true;
        break;}
        
      case B_Button: {
        Param = 1;
        draw = true;
        break;}
        
      case C_Button: {
        Param = 2;
        draw = true;
        break;}
        
      case D_Button: {
        Param = 3;
        draw = true;
        break;}

      case Star_Button: { // Переключение в рабочий режим.
        for(int i=0; i<4; i++){ // Сохранение параметров в долговременную память.
          EEPROM.update(i, Setings[i]);}
        disp.showString("____");
        Speed  = Setings[0]; // A
        Delay  = Setings[1]; // b
        Vylet  = Setings[2]; // C
        Rezerv = Setings[3]; // d
        Work_State = 0;
        break;}

      case Sharp_Button: { // Удаление младших разрядов. 
        Setings[Param] = byte(Setings[Param]/10);
        draw = true;
        break;}

      default: { // Ввод цифр.
        temp = Setings[Param]*10+Button;
        if (temp > 255) temp = 255;
        Setings[Param] = byte(temp);
        draw = true;
        break;}}
    
  }


/////////////////// Состояние 0. Работа. Ожидание. ///////////////////////////

  if (Work_State == 0){

    if (Press_Button() == Sharp_Button){
      Work_State = 1;
      draw = true;   
    }

   Bottle = !digitalRead(A0); // Сигнал от датчика бутылки
   
   if (!Bottle && T_Bottle){ // Если появилась бутылка
     delay(Delay*10); // Пауза до 2.55с
     for (int i = 0; i < 100; i++){// начать шагать, но не более 100х100 шагов
       Sto_Shagov(Speed);
       if (analogRead(A1) < 350){// если виден разрыв между этикетками,
         for (int j = 0; j < Vylet; j++){
           Sto_Shagov(Speed);// шагать до достижения значения вылета этикетки    
         }
         disp.showNumber(i, false, 3, 1);
         break; // выйти из цикла и ждать новую бутылку
         } } }

    T_Bottle = Bottle;
  
  }
}
