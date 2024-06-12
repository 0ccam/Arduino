#include <EEPROM.h>
#include <SoftwareSerial.h>

// 0xE1 - Ошибка 1: Достигнуто максимальное число шагов подачи этикетки.
// 0xE2 - Ошибка 2: Очередь датчика бутылки заполнена.
// 0xE3 - Ошибка 3. Частая подача бутылки! Время следующей бутылки в очереди вышло, но предыдущая ещё клеится.

byte REGS[16]; // Массив регистров REGS вкючает в себя:

#define Pin_DB  2  // Пин датчика бутылки.
#define Pin_DRE A6 // Пин датчика разрыва этикетки.
#define Pin_PUL  12 // Пин подключения вывода PUL- драйвера ШД. PUL+ подключен на +5В.

// Регистры общего назначения:
#define FeedDelay  0 // Регистр задания задержки подачи.
#define FeedSpeed  1 // Регистр задания скорости подачи.
#define FeedMaxStp 2 // Максимальное (безаварийное) количество шагов подачи этикетки.
#define DRETreshld 3 // Регистр задания порога датчика разрыва этикетки.
#define LabelLadge 4 // Регистр задания вылета этикетки.


// Специальные регистры:
#define DREADC 0xA // Регистр текущего значения АЦП датчика разрыва этикетки.

#define StatusReg 0xC // Регистр управляющих флагов StatusFlags:
#define TICK  B00000001  // Флаг полушага шагового двигателя. TICK(0) + TICK(1) = целый шаг. Бит регистра - 0.
#define WORK  B00000010  // Флаг рабочего состояния станка (прерывания включены).
#define STPR  B10000000  // Флаг разрешения работы шагового двигателя (подачи этикетки). Бит регистра - 7.

#define SensorReg 0xD // Регистр состояния датчиков SensorFlags:
#define DB    B00000001  // Флаг срабатывания датчика бутылки. Запоминает срабатывание. Бит регистра - 0.
#define DRE   B00001000  // Флаг срабатывания датчика разрыва этикетки. Бит регистра - 3.

#define ErrorReg 0xE // Регистр ошибок ErrorFlags:
#define Err1  B00000001 // Ошибка 1: Достигнуто максимальное число шагов подачи этикетки. Бит регистра - 0.
#define Err2  B00000010 // Ошибка 2: Очередь датчика бутылки заполнена. Регистр ErrorFlags. Бит регистра - 1.
#define Err3  B00000100 // Ошибка 3. Частая подача бутылки! Бит регистра - 2.
#define Err4  B00001000 // Ошибка 4. Нет этикетки. Бит регистра - 3.

#define StateReg 0xF // Регистр состояния конечного автомата StateReg:
#define START 0 // Запуск.
#define STOP  1 // Стоп.
#define FEED  2 // Предварительная подача этикетки.
#define LEDGE 3 // Подача вылета этикетки.
#define READY 255 // Готовность к работе. Нейтральное состояние.

 // Функции-макросы по работе с регистрами.
#define Error(err) REGS[ErrorReg] |= err 
#define GetFlag(reg, flag) REGS[reg] & flag
#define SetFlag(reg, flag) REGS[reg] |= flag
#define DropFlag(reg, flag) REGS[reg] &= ~flag
#define InvertFlag(reg, flag) REGS[reg] ^= flag
#define SetState(stt) REGS[StateReg] = stt
#define State_Is(stt) REGS[StateReg] == stt

/* Массив переменных, доступных для изменения по UART.
 *  Индекс_переменной_в_массиве (адрес_верхнего_ниббла|адрес_нижнего_ниббла *при адресации по UART)
 * 
 * 0 (0x1n|0x0n) - Вылет.
 * 1 (0x3n|0x2n) - Задержка подачи.
 * 2 (0x5n|0x4n) - Скорость подачи. 
 * 3 (0x7n|0x6n) - Порог Датчика Разрыва Этикетки. 
 * 4 (0x9n|0x8n) - Текущее значение АЦП ДРЭ. 
 * 5 (0xBn|0xAn) - Пусто. 
 * 6 (0xDn|0xCn) - Регистр ошибок.
 * 7 (0xFn|0xEn) - Управляющий регистр.
 *  7Н (0xFn) - Номер регистра, последнего отправленного по запросу на чтение.
 *  7L (0xEn) - Управление состоянием программы: А - работа, F - стоп.
 *  
 *  Принятый по UART байт 0x5B запишет данные 0хВ (полбайта, ниббл) в старший(верхний) ниббл по адресу vars[2].
 *  Принятый по UART байт 0x4А запишет данные 0хА (полбайта, ниббл) в младший(нижний) ниббл по адресу vars[2].
 *  >>> vars[2] -> 0xBA
 *  Принятые по UART байты 0xEn и 0хFn не только пишут по адресу vars[7], но и являются управляющими командами.
 */

// Переменные хранения подготовленных значений регистров массива REGS:
volatile unsigned int label_ladge;    // Хранит подготовленное значение регистра LabelLadge.
volatile byte         feed_delay;     // Хранит подготовленное значение регистра FeedDelay.
volatile unsigned int feed_speed;     // Хранит подготовленное значение регистра FeedSpeed.
volatile unsigned int max_feed_steps; // Хранит подготовленное значение регистра FeedMaxStp.
// Иные переменные:
volatile byte timeout; // Время, в течении которого датчик бутылки не активен после срабатывания (подавление дребезга).
volatile byte         queue[8];       // Очередь срабатываний датчика бутылок.
volatile unsigned int step_counter;   // Счётчик шагов ШД.

SoftwareSerial SoftSerial(10, 11); // RX, TX Программный UART.

void PrintAll(){
  for (byte i = 0; i < 16; i++){
    Serial.write(REGS[i]);
  }
}

void T2_start(){
  // инициализация Timer2
  cli();                     // Запрет прерываний
  TCCR2B = 0x07;             // 0x07 - предделитель 1024 (30.64 Гц) | 0x03 - предделитель 32 (976.5625Гц).
  TIMSK2 |= (1 << TOIE2);    // Включение прерываний по переполнению
  sei();                     // Разрешение прерываний
}

void T2_stop(){
  cli();         // Запрет прерываний
  TIMSK2 = 0;    // Выключение прерываний по переполнению
  sei();         // Разрешение прерываний
}

void T1_start(unsigned int ctr=0xFFFF){
  // инициализация Timer1
  cli();                     // Запрет прерываний.
  TCCR1A = 0;                // Начальное значение счётчика.          
  OCR1A = ctr;               // Установка регистра совпадения
  TCCR1B = 0b00001001;       // CRC режим без предделителя (16МГц/ctr).
  TIMSK1 = 0b00000010;       // Включить прерывание по совпадению со счётчиком.
  sei();                     // Разрешение прерываний
}

void T1_stop(){
  cli();           // Запрет прерываний.
  TCCR1B = 0;      // Отключить тактовый генератор.
  TIMSK1 = 0;      // Выключить прерывания.
  sei();           // Разрешение прерываний
}

void Send(byte msg){
  Serial.write(msg);
  SoftSerial.write(msg);
}

void Receive(byte raw){

  byte head = (raw >> 4) & 0x0F;
  byte tail = raw & 0x0F;
  if (head < 0xA){ REGS[head] = (REGS[head] << 4) | tail; } // Регистры от 0 до 9 включительно - запись тетрады со смещением уже записанных данных влево на 4.
  else if (head == 0xA){ Send(REGS[tail]); } // Команда А - читать регистр и отправить.
  else if (head == 0xB){ // Управляющие команды.
    if (tail == 0xA){ // Запуск.
      SetState( START );
    }else // Печать всех регистров.
    if (tail == 0xB){
      PrintAll();
    }else // Сохранение настроек.
    if (tail == 0xC){ 
      EEPROM[0] = REGS[0];
      EEPROM[1] = REGS[1];
      EEPROM[2] = REGS[2];
      EEPROM[3] = REGS[3];
      EEPROM[4] = REGS[4];
    }else // Стоп.
    if (tail == 0xF){
      SetState( STOP );
    }
  }
  else if (head == 0xC){ REGS[tail] = 0x00; } // Команда С - обнулить регистр.
  else if (head == 0xD){ REGS[tail]--; } // Команда D - декремент регистра.
  else if (head == 0xE){ REGS[tail]++; } // Команда Е - инкремент регистра.
  else { REGS[tail] = 0xFF; } // Команда F - заполнить регистр единицами.
  
}

// Предварительные установки.
void setup(){
  SetState( STOP );
  timeout = 0;
  step_counter = 0;
  Serial.begin(9600);
  SoftSerial.begin(4800);
  //pinMode (Pin_DRE, INPUT_PULLUP); // Подключение датчика разрыва этикетки.
  pinMode (Pin_DB, INPUT_PULLUP); // Подключение  датчика бутылки.
  //pinMode (LED_BUILTIN, OUTPUT);
  pinMode (12, OUTPUT);   // Выход драйвера PUL- ШД
  digitalWrite(12, HIGH); // Внутренний светодиод драйвера ШД выключен.
  pinMode (8, OUTPUT);   // Выход датировщика.
  digitalWrite(8, HIGH); // Датировщик выключен.
}


void loop(){

// Приём данных по последовательному порту.
while(Serial.available()){ Receive(Serial.read()); }
while(SoftSerial.available()){ Receive(SoftSerial.read()); }


REGS[DREADC] = (byte)(analogRead(A6) >> 2);
//Serial.println(REGS[DREADC]);
if (REGS[DREADC] < REGS[DRETreshld]){ // Поступил сигнал от ДРЭ.
  SetFlag( SensorReg, DRE );
  //Serial.println(100);
  //Serial.write(0xDE); // Послать сообщение о срабатывании датчика.
}else{
  DropFlag( SensorReg, DRE );
   //Serial.println(50);
}
if(!digitalRead(2)){ // Контроль состояния датчика бутылки с установкой/сбросом флага.
  SetFlag( SensorReg, DB );
  //Serial.write(0xDB);
}else{
  DropFlag( SensorReg, DB ); // Послать сообщение о срабатывании датчика.
}



// Пуск.
if(State_Is( START )){
  /*/ Загрузка сохранённых настроек из EEPROM:
  REGS[0] = EEPROM[0];
  REGS[1] = EEPROM[1];
  REGS[2] = EEPROM[2];
  REGS[3] = EEPROM[3];
  REGS[4] = EEPROM[4];*/
  // Подготовка значений настроек:
  label_ladge = REGS[LabelLadge] * 0xFF;
  feed_delay = (!REGS[FeedDelay]) ? 1 : REGS[FeedDelay];
  feed_speed = (unsigned int)(0xFFFF/(REGS[FeedSpeed] + 1));
  max_feed_steps = REGS[FeedMaxStp] * 0xFF;
  // Обработчики прерываний:
  T2_start();
  T1_start();
  attachInterrupt(0, BottleSensor, FALLING); // Назначение обработчика прерывания по падающему фронту на ножке D2.
  SetFlag( StatusReg, WORK ); // Установить флаг рабочего состояния.
  SetState( READY );
}

// Стоп.
if(State_Is( STOP )){
  T2_stop(); // Остановка таймера 2.
  T1_stop(); // Остановка таймера 1.
  digitalWrite(Pin_PUL, HIGH); // Потушить светодиод оптрона ШД, если остался не выключен.
  digitalWrite(8, HIGH); // Датировщик выключен.
  detachInterrupt(0); //Сбросить обработчик датчика бутылки.
  DropFlag( StatusReg, WORK ); // Сбросить флаг рабочего состояния.
  DropFlag( StatusReg, STPR ); // Сбросить флаг разрешения работы ШД.
  step_counter = 0; // Обнуление счётчика шагов.
  for (byte i = 0; i < 8; i++){ // Очистить очередь датчика бутылок.
    queue[i] = 0;
  }
   SetState( READY );
}

// Подача основной части этикетки.
if(State_Is( FEED )){
  digitalWrite(8, LOW); // Датировщик включен.
  //delay(100);
  //digitalWrite(8, HIGH); // Датировщик выключен.
  if (step_counter >= max_feed_steps){
    Error( Err1 ); // Ошибка 1: Достигнуто максимальное число шагов подачи этикетки.
    Send(0xE1);
    SetState( STOP );
  }
  if (GetFlag( SensorReg, DRE )){ // Если сработал ДРЭ.
    //Send(0xDE);
    step_counter = 0; // Сбросить счётчик шагов.
    SetState( LEDGE );
  }
}

// Подача вылета.
if(State_Is( LEDGE )){
  // Если достигнуто нужное для вылета количество шагов:
  if (step_counter >= label_ladge){
    DropFlag( StatusReg, STPR ); // Запретить работу ШД.
    // Выполнено! Done!
    Send(0xAF);
    step_counter = 0; // Сбросить счётчик шагов.
    digitalWrite(8, HIGH); // Датировщик выключен.
    digitalWrite(Pin_PUL, HIGH); // Потушить светодиод оптрона ШД, если остался не выключен.
    SetState( READY );
  }
}



}

void BottleSensor(){ // Обработчик датчика бутылки.
  /* Ищет свободное место в очереди, и добавляет в неё
   * новое событие от датчика бутылки со значением
   * времени задержи до подачи этикетки.
   */
  //Send(0xDB);
  if (timeout > 0) return;
  timeout = 30;
  for (byte i = 0; i <= 8; i++){
    if (i == 8){ // Счетчик вышел за пределы очереди.
      Error( Err2 ); // Ошибка 2: Очередь датчика бутылки заполнена.
      Send(0xE2);
      SetState( STOP );
      break;
    }
    if (queue[i] == 0){ // Первая попавшаяся ячейка пуста.
      queue[i] = feed_delay;
      break;
    }
  }
}

ISR(TIMER2_OVF_vect){ // Обработчик прерывания по таймеру2. Срабатывает 30 раз в секунду.
  if (timeout > 0) timeout--;
  for (byte i = 0; i < 8; i++){
    if (queue[i] != 0){
      if (queue[i] == 1){
        if (GetFlag( StatusReg,STPR )){ // Если установлен флаг разрешения работы ШД.
          Error( Err3 ); // Ошибка 3. Частая подача бутылки!
          Send(0xE3); // Печать ошибки.
          SetState( STOP );
        }else
        if (GetFlag( SensorReg, DRE )){ // Если сработал ДРЭ.
          Error( Err4 ); // Ошибка 4. Нет этикетки!
          Send(0xE4); // Печать ошибки.
          SetState( STOP );
        }else{ 
          Send(0xAE);// Начата подача этикетки.
          SetFlag( StatusReg, STPR ); // Разрешить работу ШД.
          SetState( FEED );
        }
      }
      queue[i]--;
    }
  }
}

ISR(TIMER1_COMPA_vect) { // Обработчик прерывания по таймеру 1.
  T1_start(feed_speed); // Первым делом - задание паузы до следующего тика.
  if(GetFlag( StatusReg, STPR )){ // Если разрешена работа:
    if (GetFlag( StatusReg, TICK )){ // На единичном полушаге считаем шаги:
      digitalWrite(Pin_PUL, LOW); // Светодиод оптрона драйвера ШД включен.
      step_counter++; // Увеличиваем счётчик шагов,
    }else{
      digitalWrite(Pin_PUL, HIGH); // Светодиод оптрона светодиод драйвера ШД выключен.
    }
    InvertFlag( StatusReg, TICK ); // Инвертировать флаг полушага.
  }
}
