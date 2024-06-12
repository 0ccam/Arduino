/* Программа модной разливайки */

#include <Wire.h>                          //  Подключаем библиотеку для работы с шиной I2C
#include <EEPROM.h>
#include <LiquidCrystal_I2C.h>             //  Подключаем библиотеку для работы с LCD дисплеем по шине I2C
#include "keyboard.h"

#define BottlePIN 2 // Пин датчика бутылки
#define DREPIN 3 // Пин датчика разрыва этикетки
#define OC1A  5 // Вход таймера 1 (счётчика)
#define OC2A  11 // Выход таймера 2 (генератора)
#define RunButton 4
#define Dater_Pin 8

#define MENU 0
#define EDIT 1
#define READY 2
#define PODACHA_1 3
#define PODACHA_2 4
#define STOP 5

LiquidCrystal_I2C lcd(0x27,16,2);          //  Объявляем  объект библиотеки, указывая параметры дисплея (адрес I2C = 0x27, количество столбцов = 16, количество строк = 2)

volatile boolean feed_permit = false; // флаг разрешения подачи этикетки
volatile byte waiting_counter = 0; // счётчик задержки подачи
volatile byte stepper_done = false; // флаг выполнения задания шаговым двигателем

volatile byte Zaderjka_podachi;
volatile byte Skorost_podachi;
volatile byte Max_podacha;
volatile byte Porog_datchika_RE;
volatile byte Vylet_etiketki;

volatile byte state = MENU;
byte key;
unsigned int EditTemp;
char CurrentParam = 0;
unsigned int DPRctr  = 0;
unsigned int ParamValue;
char* Params[] = {
  "Zaderjka podachi",
  "Skorost' podachi",
  "Max podacha",
  "Porog datchika RE",
  "Vylet etiketki"
  };

struct Tusk{
  byte busy = 0;
  unsigned int pause = 0;
  void (*handler)(void);
};

Tusk Queue[4];

  
byte ParsMaxNum = sizeof(Params)/sizeof(Params[0])-1;

byte getXPosition(unsigned int value)
{
  if (value < 10) return 19;
  if (value < 100) return 18;
  if (value < 1000) return 17;
  if (value < 10000) return 16;
  if (value < 100000) return 15;
}

void printParamValue(unsigned int pv){
  lcd.setCursor(getXPosition(pv), 2);
  lcd.print(pv);
}

void InitTimers(void){
  /* Timer 0 */
  TCCR0A = 0; // Обычный режим.
  OCR0A = 255; // 
  TIMSK0 = (1<<OCIE0A); // Прерывание по совпадению.
  TCCR0B = (1<<WGM02) | (1<<CS02) | (0<<CS01) | (1<<CS00); // CRC режим, предделитель 1024.
  /* Timer 1. Сброс управляющих регистров. */
  TCNT1 = 0; // Начальное значение счётчика.
  TCCR1A = 0; // Переход в общий режим. Значение при запуске контроллера: 0b00000001 - режим ШИМ.   
  TCCR1B = 0; // Выключение таймера. Значение при запуске контроллера: 0b00000011 - запущен с предделителем 64.
  /* Timer 2. Сброс управляющих регистров. */
  TCCR2A = 0; // Переход в общий режим. Значение при запуске контроллера: 0b00000001 - режим ШИМ.  
  TCCR2B = 0; // Выключение таймера. Значение при запуске контроллера: 0b00000100 - запущен с предделителем 64.
}

void Stepper(unsigned int distance, byte speed){
  /*  Timer 1 */
  TCNT1 = 0; // Начальное значение счётчика.
  OCR1A = distance; // Порог сброса счётчика, и генерации прерывания.
  TCCR1B = (1<<WGM12) | (1<<CS12) | (1<<CS11) | (1<<CS10); // CRC режим, внешний источник тактового сигнала.
  TIMSK1 = (1 << OCIE1A); // Включить прерывание по совпадению со счётчиком.
  /* Timer 2 */
  TCNT2 = 0; // Начальное значение счётчика.
  OCR2A  = 255 - speed; // Установка длительности импульсов.
  /*  Режим генерации.
   *  COM2A0=1 - Изменение состояния вывода OC2A на противоположное при совпадении с регистром OCR2A.
   *  WGM22:0=010 - Режим обновления счётчика TCNT2 при совпадении с регистром OCR2A (СRС режим).
   */
  TCCR2A = (1<<COM2A0) | (1<<WGM21);
  TCCR2B = (1<<CS22) | (1<<CS21) | (1<<CS20); // 111 - CLK/1024 / Установка предделителя на 64, и разрешение генерации.
}

// Обработчик прерывания Таймера 1 (счётчик шагов).
ISR(TIMER1_COMPA_vect){
  TCCR2B = 0; // Остановка генератора (Таймер 2).
  TIMSK1 = 0; // Выключить прерывания Таймера 1.
  TCCR1B = (0<<CS12) | (0<<CS11) | (0<<CS10); // Выключить таймер 1.
  digitalWrite(Dater_Pin, HIGH); // Датировщик выключен.
  stepper_done = true;
}


/* Обработчик датчика бутылки */
void BottleSensor()
{
  if (waiting_counter == 0){
    waiting_counter = Zaderjka_podachi; // установить счётчик задержки подачи
  }
}


/* Обработчик прерывания Таймера 0 (системный таймер).
 Пробуждается через равные интервалы времени.
 */
ISR(TIMER0_COMPA_vect){
  if (waiting_counter == 1) feed_permit = true; // разрешить подачу этикетки
  if (waiting_counter > 0) waiting_counter--; // уменьшить значение счётчика
}


void setup(){
  Serial.begin(9600);
  pinMode(13, OUTPUT);
  pinMode (Dater_Pin, OUTPUT);   // Выход датировщика.
  digitalWrite(Dater_Pin, HIGH); // Датировщик выключен.
  //Wire.begin();
  lcd.init();                            //  Инициируем работу с LCD дисплеем
  lcd.backlight();                       //  Включаем подсветку LCD дисплея  
  pinMode(RunButton, INPUT); 
  pinMode (OC2A, OUTPUT); // Выход Таймера 2.
  pinMode(BottlePIN, INPUT); // Датчик бутылки
  pinMode(DREPIN, INPUT);
  pinMode (LED_BUILTIN, OUTPUT);
  InitTimers();
}

void loop(){

  /* Навигация по меню */
  if (MENU == state)
  {
    lcd.clear();
    lcd.setCursor(6, 0);
    lcd.print("MENU");
    lcd.setCursor(0, 1);
    lcd.print(CurrentParam+1);
    lcd.print(".");
    lcd.print(Params[CurrentParam]);
    EEPROM.get(CurrentParam*sizeof(ParamValue), ParamValue);
    printParamValue(ParamValue);
    
    for ( ; ; )
    {
      /* Опрос клавиатуры */
      key = get_key();
      if (key == _C_){
        CurrentParam++;
        if (CurrentParam > ParsMaxNum) CurrentParam = 0;
        break;
      } else
      if (key == _D_){
        CurrentParam--;
        if (CurrentParam < 0) CurrentParam = ParsMaxNum;
        break;
      } else
      if (key == _Sharp_){
        state = EDIT;
        break;
      } else
      if (key == _Star_){
        /* Чтение параметров из долговременной памяти */
        EEPROM.get(0, Zaderjka_podachi);
        EEPROM.get(2, Skorost_podachi);
        EEPROM.get(4, Max_podacha);
        EEPROM.get(6, Porog_datchika_RE);
        EEPROM.get(8, Vylet_etiketki);
        /* Включение датчика бутылки */
        attachInterrupt(0, BottleSensor, FALLING); // Назначение обработчика прерывания по падающему фронту на ножке D2.
        state = READY;
        break;
      }
    }
  }


  /* Редактирование выбранного параметра */
  if (EDIT == state)
  {
    lcd.clear();
    lcd.setCursor(8, 0);
    lcd.print("EDIT");
    lcd.setCursor(0, 1);
    lcd.print(Params[CurrentParam]);
    lcd.setCursor(0, 2);
    lcd.print(">");
    printParamValue(ParamValue);

    for ( ; ; )
    {
      /* Опрос клавиатуры */
      key = get_key();
      if (key < 10){
        long tmp = (long)ParamValue*10 + key;
        ParamValue = (tmp > 65535) ? 65535 : (unsigned int)tmp;
        break;
      } else
      if (key == _A_){
        ParamValue++;
        break;
      } else
      if (key == _B_){
        ParamValue--;
        break;
      } else
      if (key == _Sharp_){
        ParamValue /= 10;
        break;
      } else
      if (key == _Star_){
        EEPROM.put(CurrentParam * sizeof(ParamValue), ParamValue);
        state = MENU;
        break;
      }
    }
  }


  /* Позиция готовности к подаче */
  if (READY == state)
  {
    lcd.clear();
    lcd.setCursor(0, 0);
    /* Опрос ДРЭ */
    if (!digitalRead(3)){
      lcd.print("Net etiketki v DRE");
      digitalWrite(13, LOW);
    } else {
      lcd.print("Ojidanie butylki");
    }

    for ( ; ; )
    {
      digitalWrite(13, digitalRead(3));
      /* Ожидание разрешения на подачу этикетки */
      if (feed_permit){
        feed_permit = false; // сбросить флаг разрешения подачи
        state = PODACHA_1;
        break;
      }
      /* Опрос клавиатуры */
      key = get_key();
      if (key == _Sharp_){
        state = STOP;
        break;
      }
    }
  }


  /* Первая часть подачи этикетки */
  if (PODACHA_1 == state)
  {
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("Podacha 1");
    digitalWrite(Dater_Pin, LOW); // Датировщик включен.
    Stepper(Max_podacha, Skorost_podachi);

    for ( ; ; )
    {
      /* Опрос ДРЭ */
      if (!digitalRead(3)){
        state = PODACHA_2;
        break;
      }
      /* Проверка достижения максимального числа шагов подачи */
      if (stepper_done){
        stepper_done = false;
        lcd.clear();
        lcd.setCursor(0, 0);
        lcd.print("Net DRE!");
        state = READY;
        break;
      }
      /* Опрос клавиатуры */
      key = get_key();
      if (key == _Sharp_){
        state = STOP;
        break;
      }
    }
  }


  /* Вторая часть подачи этикетки */
  if (PODACHA_2 == state) 
  {
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("Podacha 2");
    /*  Timer 1 */
    TCNT1 = 0; // Начальное значение счётчика.
    OCR1A = Vylet_etiketki; // Порог сброса счётчика, и генерации прерывания.

    for ( ; ; )
    {
      /* Опрос клавиатуры */
      key = get_key();
      if (key == _Sharp_){
        state = STOP;
        break;
      }
      /* Ожидание окончания подачи */
      if (stepper_done){
        stepper_done = false;
        state = READY;
        break;
      }
    }
  }


  /* Выход из режима подачи этикетки */
  if (STOP == state)
  {
    TCCR2B = 0; // Остановка генератора (Таймер 2).
    TIMSK1 = 0; // Выключить прерывания Таймера 1.
    TCCR1B = (0<<CS12) | (0<<CS11) | (0<<CS10); // Выключить таймер 1.
    digitalWrite(Dater_Pin, HIGH); // Датировщик выключен.
    state = MENU;
  }
    
}


              
