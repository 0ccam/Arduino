#include "TM1637.h" // Подключаем библиотеку
#define CLK 11 // К этому пину подключаем CLK
#define DIO 12 // К этому пину подключаем DIO

TM1637  disp(CLK,DIO);
int     Button = 0;
boolean Button_Trigger = true;
boolean Keyboard_State = false;
int     Work_State = 0; // 0 - Ожидание ввода, 1 - Работа, 2 - Пауза/Выполнено.
boolean Display_Active = true;
boolean Display_Trigger = true;
int     Display = 0;
int     Limit = 0;
int     Count = 0;
boolean Pause_Trigger = false;
int     Pause_Counter = 0;
boolean Load_Trigger = true;
boolean Load_Active = false;
boolean Meter_Trigger = false;
long int Meter_Counter = 0;
boolean Meter_State = false;
boolean Meter_Impulse = false;
int     Time = 0;
int     Old_Time = 0;
unsigned long time;



////////////////////////////////////////////////////////////////////////////
// Функции
////////////////////////////////////////////////////////////////////////////

boolean Press_Digital_Button(){
  if ((Keyboard_State)&&(Button_Trigger)&&(Button < 10)){
    Button_Trigger = false;
    return true;} else return false;}

boolean Press_Star_Button(){
  if ((Keyboard_State)&&(Button_Trigger)&&(Button == 14)){
    Button_Trigger = false;
    return true;} else return false;}

boolean Press_Sharp_Button(){
  if ((Keyboard_State)&&(Button_Trigger)&&(Button == 15)){
    Button_Trigger = false;
    return true;} else return false;}

boolean Press_D_Button(){
  if ((Keyboard_State)&&(Button_Trigger)&&(Button == 13)){
    Button_Trigger = false;
    return true;} else return false;}

void Display_On(){
  Display_Active = true;
  Display_Trigger = true;}

void Display_Off(){
  Display_Active = false;
  Display_Trigger = true;}

void Display_Inverting(){
  Display_Active = !Display_Active;
  Display_Trigger = true;}

void Display_Print(long int Digit){
  Display = Digit;
  Display_Trigger = true;}

void Load_On(){
  Load_Active = true;
  Load_Trigger = true;}

void Load_Off(){
  Load_Active = false;
  Load_Trigger = true;}

////////////////////////////////////////////////////////////////////////////
// Начальные установки
////////////////////////////////////////////////////////////////////////////

void setup()
{
  Serial.begin(9600);
  // Устанавливаем яркость от 0 до 7
  disp.set(5);
  disp.init(D4056A);
  // Подтягивание входов к плюсу через резисторы
  pinMode (13, OUTPUT); // Светодиод
  pinMode (10, INPUT_PULLUP); // Счетчик
  pinMode (A0, INPUT_PULLUP);
  pinMode (A1, INPUT_PULLUP);
  pinMode (A2, INPUT_PULLUP);
  pinMode (A3, INPUT_PULLUP);
  pinMode (2, OUTPUT);
  pinMode (3, OUTPUT);
  pinMode (4, OUTPUT);
  pinMode (5, OUTPUT);
  digitalWrite(2,HIGH);
  digitalWrite(3,HIGH);
  digitalWrite(4,HIGH);
  digitalWrite(5,HIGH);}


////////////////////////////////////////////////////////////////////////////
////////////////////////////// Главный цикл ////////////////////////////////
////////////////////////////////////////////////////////////////////////////

void loop(){

  Serial.print("Time: ");
  time = millis();
  //выводит количество миллисекунд с момента начала выполнения программы
  Serial.println(time);

////////////////////////////////////////////////////////////////////////////
// Опрос клавиатуры
////////////////////////////////////////////////////////////////////////////

  Keyboard_State = false;

      //Serial.println(millis()-Time, DEC);
      //Time = millis();

    digitalWrite(2,LOW);
  if      (!digitalRead(A0)){Button = 1; Keyboard_State = true;}
  else if (!digitalRead(A1)){Button = 2; Keyboard_State = true;}
  else if (!digitalRead(A2)){Button = 3; Keyboard_State = true;}
  else if (!digitalRead(A3)){Button = 10;Keyboard_State = true;}
    digitalWrite(2,HIGH);

    digitalWrite(3,LOW);
  if      (!digitalRead(A0)){Button = 4; Keyboard_State = true;}
  else if (!digitalRead(A1)){Button = 5; Keyboard_State = true;}
  else if (!digitalRead(A2)){Button = 6; Keyboard_State = true;}
  else if (!digitalRead(A3)){Button = 11;Keyboard_State = true;}
    digitalWrite(3,HIGH);

    digitalWrite(4,LOW);
  if      (!digitalRead(A0)){Button = 7; Keyboard_State = true;}
  else if (!digitalRead(A1)){Button = 8; Keyboard_State = true;}
  else if (!digitalRead(A2)){Button = 9; Keyboard_State = true;}
  else if (!digitalRead(A3)){Button = 12;Keyboard_State = true;}
    digitalWrite(4,HIGH);

    digitalWrite(5,LOW);
  if      (!digitalRead(A0)){Button = 14;Keyboard_State = true;}
  else if (!digitalRead(A1)){Button = 0; Keyboard_State = true;}
  else if (!digitalRead(A2)){Button = 15;Keyboard_State = true;}
  else if (!digitalRead(A3)){Button = 13;Keyboard_State = true;} 
    digitalWrite(5,HIGH);

  if ((!Keyboard_State)&&(!Button_Trigger)){Button_Trigger = true;}


////////////////////////////////////////////////////////////////////////////
// Состояние 0. Ввод данных
////////////////////////////////////////////////////////////////////////////

  if (Work_State == 0){
    
    if (Press_Digital_Button()&&(Display < 1000)){
      Display_Print(Display * 10 + Button);}

    if (Press_Sharp_Button()&&(Display > 0)){
      Display_Print(Display/10);}

    if (Press_Star_Button()&&(Display > 0)){
      Work_State = 1;
      Load_On();
      Limit = Display;
      Display_Print(0);}}

////////////////////////////////////////////////////////////////////////////
// Состояние 1. Работа
////////////////////////////////////////////////////////////////////////////

  if (Work_State == 1){

    //Meter_State = false;
        
    if (!digitalRead(10)) {Meter_Impulse = true;}
    else {Meter_Impulse = false;}
    
    if ((Meter_State)&&(Meter_Impulse)){Meter_Counter++; Meter_State = false;}
    if ((!Meter_State)&&(!Meter_Impulse)){Meter_State = true;}

    Display_Print((long int)Meter_Counter/408);
    //Serial.println("========");
    //Serial.println(Meter_Counter);
    //Serial.println(Display);
    if ((Display > (Limit - 1))||Press_Sharp_Button()){
      Work_State = 2;
      Load_Off();}}


////////////////////////////////////////////////////////////////////////////
// Состояние 2. Пауза/Выполнено
////////////////////////////////////////////////////////////////////////////

  if (Work_State == 2){
    
    Pause_Counter++;
    
    if (Pause_Counter == 3000){
      Pause_Counter = 0;
      Display_Inverting();}

    if ((Display < Limit)&&Press_Star_Button()){
      Display_On();
      Work_State = 1;
      Load_On();}

    if (Press_Sharp_Button()){
      Display_On();
      Display_Print(0);
      Limit = 0;
      Work_State = 0;
      Meter_Counter = 0;}}


////////////////////////////////////////////////////////////////////////////
// Управление дисплеем
////////////////////////////////////////////////////////////////////////////

 if ((Display_Active)&&(Display_Trigger)){
  disp.display(Display);
  Display_Trigger = false;}

 if ((!Display_Active)&&(Display_Trigger)){
  disp.clearDisplay();
  Display_Trigger = false;}

 if ((Load_Active)&&(Load_Trigger)){
  digitalWrite(13,HIGH);
  Load_Trigger = false;}

if ((!Load_Active)&&(Load_Trigger)){
  digitalWrite(13,LOW);
  Load_Trigger = false;}

}
