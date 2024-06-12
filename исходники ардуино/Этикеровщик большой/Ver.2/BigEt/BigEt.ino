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

#define READY 0
#define DONE 1
#define NOERROR 0
#define ERRNODRE 2
#define ERRMANYBOT 3
#define PODACHA 4
#define ERRQUEUE 5

LiquidCrystal_I2C lcd(0x27,16,2);          //  Объявляем  объект библиотеки, указывая параметры дисплея (адрес I2C = 0x27, количество столбцов = 16, количество строк = 2)

volatile boolean FlagDB = true;
volatile byte ErrNum = NOERROR;
volatile byte Zaderjka_podachi;
volatile byte Skorost_podachi;
volatile byte Max_podacha;
volatile byte Porog_datchika_RE;
volatile byte Vylet_etiketki;
boolean flag_podachi = false;
boolean flag_dre = false;
volatile byte State = READY;
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
  flag_podachi = false;
  TCCR2B = 0; // Остановка генератора (Таймер 2).
  TIMSK1 = 0; // Выключить прерывания Таймера 1.
  TCCR1B = (0<<CS12) | (0<<CS11) | (0<<CS10); // Выключить таймер 1.
  digitalWrite(Dater_Pin, HIGH); // Датировщик выключен.
  if (flag_dre){
    State = DONE;
    Serial.println("Этикетка подана.");
  } else {
    ErrNum = ERRNODRE;
    Serial.println("Ошибка. Нет сигнала от датчика разрыва этикетки!");
  }
}

void podacha(void)
{
  Serial.println("Начало подачи");
  if (flag_podachi){
    ErrNum = ERRMANYBOT;
    Serial.println("Ошибка. Частая подача бутылки!");
  }else{
    State = PODACHA;
    flag_podachi = true;
    //digitalWrite(LED_BUILTIN, LOW);
    digitalWrite(Dater_Pin, LOW); // Датировщик включен.
    Stepper(Max_podacha, Skorost_podachi);
  }
}

void BSawake(void)
{
  FlagDB = true;
  podacha();
  //attachInterrupt(0, BottleSensor, FALLING); // Назначение обработчика прерывания по падающему фронту на ножке D2.
}

void BottleSensor()
{ // Обработчик датчика бутылки.
  //detachInterrupt(0); // Запретить прерывание от датчика бутылки.
  if (FlagDB){
    FlagDB = false;
    scheduler(82, BSawake);
    //scheduler(Zaderjka_podachi, podacha);
  }
}

void scheduler(unsigned int pause, void (*handler)(void))
{
  /* Ищет свободное место в очереди, и добавляет в неё
   * новое событие от датчика бутылки со значением
   * времени задержи до подачи этикетки.
   */
  byte Qend = sizeof(Queue)/sizeof(Queue[0]);
  for (byte i = 0; i <= Qend; i++){
      Serial.print("scheduler ");
      Serial.print(i);
      Serial.print(" ");
      Serial.print(Queue[i].busy);
      Serial.print(" ");
      Serial.println(Queue[i].pause);
    if (i == Qend){ // Счетчик вышел за пределы очереди.
      ErrNum = ERRQUEUE;
      Serial.println("Ошибка: Очередь заполнена.");
      break;
    }
    if (Queue[i].busy == 0){ // Первая попавшаяся ячейка пуста.
      Queue[i].busy = 1;
      Queue[i].pause = pause;
      Queue[i].handler = handler;
      break;
    }
  }
}

// Обработчик прерывания Таймера 0 (системный таймер).
ISR(TIMER0_COMPA_vect){
  byte Qend = sizeof(Queue)/sizeof(Queue[0]);
  for (byte i = 0; i < Qend; i++){
    if (Queue[i].busy == 1){
      if (Queue[i].pause == 0){
        Queue[i].busy = 0;
        Queue[i].handler();
      } else {
        Queue[i].pause--;
      }
    }
  }
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

  Menu_Draw:

    lcd.clear();
    lcd.setCursor(6, 0);
    lcd.print("MENU");
    lcd.setCursor(0, 1);
    lcd.print(CurrentParam+1);
    lcd.print(".");
    lcd.print(Params[CurrentParam]);
    EEPROM.get(CurrentParam*sizeof(ParamValue), ParamValue);
    printParamValue(ParamValue);

  Menu_Wait:

    byte key = get_key();
    if (key == _Null_){
      goto Menu_Wait;
    } else
    if (key == _C_){
      CurrentParam++;
      if (CurrentParam > ParsMaxNum) CurrentParam = 0;
      goto Menu_Draw;
    } else
    if (key == _D_){
      CurrentParam--;
      if (CurrentParam < 0) CurrentParam = ParsMaxNum;
      goto Menu_Draw;
    } else
    if (key == _Sharp_){
      goto Edit_Draw;
    } else
    if (key == _Star_){
      goto Start;
    } else {
      goto Menu_Wait;
    }

  Edit_Draw:

    lcd.clear();
    lcd.setCursor(8, 0);
    lcd.print("EDIT");
    lcd.setCursor(0, 1);
    lcd.print(Params[CurrentParam]);
    lcd.setCursor(0, 2);
    lcd.print(">");
    printParamValue(ParamValue);

  Edit_Wait:

    key = get_key();
    EditTemp;
    if (key == _Null_){
      goto Edit_Wait;
    } else
    if (key < 10){
      long tmp = (long)ParamValue*10 + key;
      ParamValue = (tmp > 65535) ? 65535 : (unsigned int)tmp;
      goto Edit_Draw;
    } else
    if (key == _A_){
      ParamValue++;
      goto Edit_Draw;
    } else
    if (key == _B_){
      ParamValue--;
      goto Edit_Draw;
    } else
    if (key == _Sharp_){
      ParamValue /= 10;
      goto Edit_Draw;
    } else
    if (key == _Star_){
      EEPROM.put(CurrentParam * sizeof(ParamValue), ParamValue);
      goto Menu_Draw;
    } else {
      goto Edit_Wait;
    }

  Start:

    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("Ojidanie butylki");
    lcd.setCursor(0, 1);
    EEPROM.get(0, Zaderjka_podachi);
    EEPROM.get(2, Skorost_podachi);
    EEPROM.get(4, Max_podacha);
    EEPROM.get(6, Porog_datchika_RE);
    EEPROM.get(8, Vylet_etiketki);
    State = READY;
    attachInterrupt(0, BottleSensor, FALLING); // Назначение обработчика прерывания по падающему фронту на ножке D2.

  Ojidanie_butylki:

    if (State == PODACHA){
      lcd.clear();
      lcd.setCursor(0, 0);
      lcd.print("Ojidanie DRE");
      goto Ojidanie_DRE;
    }

    if (ErrNum != NOERROR){
      goto Error;
    }

    key = get_key();
    if (key == _Sharp_){
      goto Stop;
    }
    goto Ojidanie_butylki;

  Ojidanie_DRE:
  
    if (!digitalRead(3)){
      flag_dre = true;
      /*  Timer 1 */
      TCNT1 = 0; // Начальное значение счётчика.
      OCR1A = Vylet_etiketki; // Порог сброса счётчика, и генерации прерывания.
      lcd.clear();
      lcd.setCursor(0, 0);
      lcd.print("Podacha vyleta");
      goto Podacha_vyleta;
    }

    if (ErrNum != NOERROR){
      goto Error;
    }
    
    key = get_key();
    if (key == _Sharp_){
      goto Stop;
    }
    goto Ojidanie_DRE;

  Podacha_vyleta:

    if (State == DONE){
      lcd.clear();
      lcd.setCursor(0, 0);
      lcd.print("Gotovo");
      goto Start;
    }

    key = get_key();
    if (key == _Sharp_){
      goto Stop;
    }
    goto Podacha_vyleta;

  Error:
  
    detachInterrupt(0);
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("ERROR:");
    lcd.setCursor(0, 1);
    if (ErrNum == ERRNODRE){
      lcd.print("Net signala ot DRE");
    } else
    if (ErrNum == ERRMANYBOT){
      lcd.print("Chast. podacha but.");
    } else
    if (ErrNum == ERRQUEUE){
      lcd.print("Ochered' zapolnena");
    }
    ErrNum = NOERROR;

  WaitSharp:
  
    key = get_key();
    if (key == _Sharp_){
      goto Menu_Draw;
    }
    goto WaitSharp;

  Stop:
    detachInterrupt(0);
    goto Menu_Draw;
}


              
