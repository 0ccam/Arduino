#include "TM1637.h" 
#include "EEPROM.h"

#define CLK           12  // К этому пину подключаем CLK
#define DIO           11  // К этому пину подключаем DIO
#define Digit_Button  9   // Максимальный циферный символ на клавиатуре
#define A_Button      10  // Символ A на клавиатуре
#define B_Button      11  // Символ B на клавиатуре
#define C_Button      12  // Символ C на клавиатуре
#define D_Button      13  // Символ D на клавиатуре
#define Star_Button   14  // Символ * на клавиатуре
#define Sharp_Button  15  // Символ # на клавиатуре

TM1637  disp(CLK,DIO);

boolean Restart = true;
byte    Seting = 0;
byte    Set_Arr[40];
int     Blink = 0;
byte    Button = 255;
byte    T_Button = 255;
boolean Bottle = false;
boolean T_Bottle = false;
int     Work_State = 1; // 0 - Ожидание ввода, 1 - Работа, 2 - Пауза/Выполнено.
byte    Parameter = 0;
byte    Par = 0;
byte    Var_Param = 0;
byte    Speed; // Скорость подачи растёт от 99 к 0.
byte    Vylet; // Величина вылета.
byte    Razryv; // Чувствительность датчика разрыва этикетки.

////////////////////////////////////////////////////////////////////////////
// Функции
////////////////////////////////////////////////////////////////////////////

byte Press_Button(){
  if (T_Button != Button)  
       return Button;
  else return 255;
  }


void Flash_Print(){
      disp.point(POINT_ON);
      disp.display(Set_Arr[Parameter+(Seting*10)]);
      disp.display(1, Parameter);
      disp.display(0, Seting+10);
  }

void Sto_Shagov(byte s){
      for (int j = 0; j < 100; j++)
      {
        digitalWrite(13, HIGH);
        delayMicroseconds(s);
        digitalWrite(13, LOW);
        delayMicroseconds(s);
      }
  }
////////////////////////////////////////////////////////////////////////////
// Начальные установки
////////////////////////////////////////////////////////////////////////////

void setup()
{
  Serial.begin(9600);
  // Устанавливаем яркость от 0 до 7
  disp.set(5);
  disp.init(D4056A);
  pinMode (13, OUTPUT); // Шаговый двигатель и светодиод на плате
  pinMode (A0, INPUT_PULLUP); // Датчик бутылки A0
  pinMode (A1, INPUT); // Датчик этикетки A1
  // Столбцы клавиатуры
  // Подтягивание входов к плюсу через внутренние резисторы
  pinMode (9, INPUT_PULLUP); // 1
  pinMode (8, INPUT_PULLUP); // 2
  pinMode (7, INPUT_PULLUP); // 3
  pinMode (6, INPUT_PULLUP); // 4
  // Ряды клавиатуры
  pinMode (5, OUTPUT); // 1
  pinMode (4, OUTPUT); // 2
  pinMode (3, OUTPUT); // 3
  pinMode (2, OUTPUT); // 4
  digitalWrite(2,HIGH);
  digitalWrite(3,HIGH);
  digitalWrite(4,HIGH);
  digitalWrite(5,HIGH);}


////////////////////////////////////////////////////////////////////////////
////////////////////////////// Главный цикл ////////////////////////////////
////////////////////////////////////////////////////////////////////////////

void loop(){

////////////////////////////////////////////////////////////////////////////
// Чтение параметров из долговременной памяти при запуске программы.
////////////////////////////////////////////////////////////////////////////

 if (Restart){
  for(byte i=0; i<40; i++) Set_Arr[i] = EEPROM.read(i);
  Restart = false;
 }
 
////////////////////////////////////////////////////////////////////////////
// "Моргающая" переменная
////////////////////////////////////////////////////////////////////////////

  Blink++;
  if (Blink == 3000) Blink = 0;

////////////////////////////////////////////////////////////////////////////
// Опрос клавиатуры
////////////////////////////////////////////////////////////////////////////

  T_Button = Button;
  Button = 255;

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


////////////////////////////////////////////////////////////////////////////
// Состояние 1. Главное меню. Выбор сетинга.
////////////////////////////////////////////////////////////////////////////

  if (Work_State == 1){

    if (Blink == 0) {
      disp.clearDisplay();
      disp.display(Set_Arr[Parameter+(Seting*10)]);
      disp.display(1, Parameter);
      }
      
    if (Blink == 1500) Flash_Print();

    if (Press_Button() == A_Button){ T_Button = Button; Seting = 0; }
    if (Press_Button() == B_Button){ T_Button = Button; Seting = 1; }
    if (Press_Button() == C_Button){ T_Button = Button; Seting = 2; }
    if (Press_Button() == D_Button){ T_Button = Button; Seting = 3; }

    if (Press_Button() == Star_Button){
      T_Button = Button;
      disp.clearDisplay();
      disp.display(0, 10);
      disp.display(1, 11);
      disp.display(2, 12);
      disp.display(3, 13);

      Speed =  Set_Arr[Seting*10];
      Vylet = Set_Arr[Seting*10 + 1];
      Razryv = Set_Arr[Seting*10 + 2];
      
      Work_State = 0;
      }

    if (Press_Button() == Sharp_Button){ T_Button = Button; Work_State = 2; }
    
  }


////////////////////////////////////////////////////////////////////////////
// Состояние 2. Выбор параметра.
////////////////////////////////////////////////////////////////////////////

  if (Work_State == 2){ // Меню настройки, пауза

    if (Blink == 0){
      disp.clearDisplay();
      disp.display(Set_Arr[Parameter+(Seting*10)]);
      disp.display(0, Seting+10);
      }
      
    if (Blink == 1500) Flash_Print();
    
    if (Press_Button() <= Digit_Button){
      T_Button = Button;
      Parameter = Button;
      Flash_Print();
      }
      
    if (Press_Button() == Star_Button){
      T_Button = Button;
      Work_State = 1;
      }
      
    if (Press_Button() == Sharp_Button){
      T_Button = Button;
      Work_State = 3;
      }
    
  } 

////////////////////////////////////////////////////////////////////////////
// Состояние 3. Редактирование значения параметра.
////////////////////////////////////////////////////////////////////////////

  if (Work_State == 3){

    if (Blink == 0) {
      disp.clearDisplay();
      disp.display(1, Parameter);
      disp.display(0, Seting+10);
      }
      
    if (Blink == 1500) Flash_Print();

    if ((Press_Button() <= Digit_Button)&&(Set_Arr[Parameter+(Seting*10)] < 10)){
      T_Button = Button;
      Set_Arr[Parameter] = Set_Arr[Parameter]*10+Button;
      Flash_Print();
      }

    if ((Press_Button() == Sharp_Button)&&(Set_Arr[Parameter+(Seting*10)] > 0)){
      T_Button = Button;
      Set_Arr[Parameter+(Seting*10)] = (byte)Set_Arr[Parameter+(Seting*10)]/10;
      Flash_Print();
      }

    if (Press_Button() == Star_Button){
      T_Button = Button;
      EEPROM.update(Parameter, Set_Arr[Parameter+(Seting*10)]);
      Work_State = 2;
      }
    }

////////////////////////////////////////////////////////////////////////////
// Состояние 0. Работа. Ожидание.
////////////////////////////////////////////////////////////////////////////

  if (Work_State == 0){

    if (Press_Button() == Sharp_Button){
      T_Button = Button;
      Work_State = 1;     
    }

   Bottle = !digitalRead(A0);

   if (!Bottle && T_Bottle){
    
    for (int j = 0; j < 550; j++)
      {
        if (analogRead(A1) < (Razryv*10))
        {
          for (int j = 0; j < Vylet; j++) Sto_Shagov(Speed);    
          break;
         }
        Sto_Shagov(Speed);
       }
    }

    T_Bottle = Bottle;

  //delay(1000);
  
  }

}
