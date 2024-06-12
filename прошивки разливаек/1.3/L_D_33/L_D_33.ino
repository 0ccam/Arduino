/* Программа модной разливайки */

#include "TTT.h"
#include "ini.h"
#include "bottles.h"

#define DPR     2
#define RunButton 4
#define VESPER  6

volatile unsigned int DPRctr;


void WaitRunButton(void)
{
  while( digitalRead(RunButton)){;} // Ожидание отпускания кнопки пуск.
  while(!digitalRead(RunButton)){;} // Ожидание нажатия кнопки пуск.
}


void Tachometer()
{
  
  if (DPRctr < Nfull_imp){
    /* Пересчёт задания длительности шага по каждому импульсу от насоса. */
    float tmp = (Timp * Kflw) + Kcor;
    /* Пропуск импульсов помехи. 
    * Длительность ипульса от ДПР при 50Гц на насосе - 14,33 мс.
    * Всё что меньше Tstep_min - помеха.
    */
    if (tmp < Tstep_min){
      Serial.println("Помеха от ДПР");
    } else {
      Serial.println(DPRctr);
      if (tmp > Tstep_max){
        tmp = Tstep_max;
      }
      Tstep = (byte)tmp;
      Timp = 0;
      
      DPRctr++; // Инкремент счётчика импульсов от насоса.
  
      if (DPRctr == (int)Nfull_imp){
        digitalWrite(VESPER, LOW); // Выключить насос.
        Tstep = Tstep_min;
        Serial.println("Насос выключен");
        detachInterrupt(0);
      }
    }
  } else {
    Serial.println("Насос перекрутил");
  }
}

void setup()
{
  Serial.begin(9600);
  pinMode(DPR, INPUT);
  pinMode(RunButton, INPUT);
  pinMode(VESPER, OUTPUT);
  digitalWrite(VESPER, LOW); // Выключить насос.
  pinMode(LED_BUILTIN, OUTPUT);
  TTTInit();

  /* Ожидание нажатия кнопки пуск. */
  WaitRunButton();
  //Serial.println("Установка начальной высоты.");
  
  #ifdef WASHMODE
    if (Hcur_mm != Hmax_mm) GOTO(Hmax_mm, Tstep_min);
  #else
    if (Hcur_mm != Htop_mm) GOTO(Htop_mm, Tstep_min);
  #endif
}

void loop()
{
  /* Ожидание нажатия кнопки пуск. */
  WaitRunButton();
  
  /* Режим промывки. */
  #if defined(WASHMODE)
  
    digitalWrite(VESPER, !digitalRead(VESPER)); // Включить/выключить насос.

  /* Режим розлива с погружением. */
  #elif defined(DIVEMODE)
  
    GOTO(Hbttm_mm, Tstep_min); // Опускание вилки.
    DPRctr = 0; // Сброс счётчика импульсов от ДПР насоса.
    attachInterrupt(0, Tachometer, FALLING);  // Пин 2, ДПР.
    digitalWrite(VESPER, HIGH); // Включить насос.
    Serial.println("Налив подушки");
    while(DPRctr < Npllw_imp){;} // Ожидание налива подушки.
    Serial.println("Подъём!");
    GOTO(Hfull_mm, Tstep_max); // Подъём вилки.
    TTTPause(50); // Пауза после налива.
    while(!TaskDone){;} // Ждать в вечном цикле выполнения задания.
    GOTO(Htop_mm, 30);

  /* Режим розлива без погружения. */
  #elif defined(NODIVEMODE)

    DPRctr = 0;
    attachInterrupt(0, Tachometer, FALLING);  // Пин 2, ДПР.
    digitalWrite(VESPER, HIGH); // Включить насос.
    //Serial.println("Налив бутылки");
    while(digitalRead(VESPER)){;} // Ожидание налива бутылки.

  #endif
  
}
