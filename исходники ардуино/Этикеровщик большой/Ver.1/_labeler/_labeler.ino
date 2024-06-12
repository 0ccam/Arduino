/*
 * Эта часть программы похожа на прошивку контроллера, работающего с датчиками и двигателем.
 * Контроллер общается с терминалом через последовательный порт.
 */

#include <EEPROM.h>
#define max_feed_steps 20000 // Максимальное (безаварийное) количество шагов подачи этикетки.

byte vars[8];
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
volatile byte protruding_label;      // Выступающая часть этикетки (вылет);
volatile byte feed_delay;    // Задержка подачи.
volatile unsigned int feed_speed;    // Скорость подачи.
volatile unsigned int lgs_threshold; // label gap sensor threshold - порог срабатывания датчика разрыва этикетки.

volatile byte queue[8] = {0,0,0,0,0,0,0,0}; // Очередь срабатываний датчика бутылок.
volatile unsigned int step_counter = 0;
volatile boolean stepper = false; // Если установлен флаг, работает ШД.
volatile boolean tick_flag = false; // Флаг полушага.
volatile boolean lgs_flag = false; // Если установлен флаг, поступил сигнал от ДРЭ.
byte LState = 255;

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

void setup(){
  Serial.begin(9600);
  pinMode (A6, INPUT_PULLUP);
  pinMode (2, INPUT_PULLUP);
  pinMode (LED_BUILTIN, OUTPUT);
  pinMode (3, INPUT_PULLUP); // Выход драйвера PUL ШД
  digitalWrite(3, HIGH);
}


void loop(){

// Приём данных по последовательному порту.
while(Serial.available()) {
  byte raw = Serial.read();
  if (raw == 0xEA){ LState = 0; } // Запустить программу.
  if (raw == 0xEF){ LState = 1; } // Остановить программу.
  // Прочесть переменную из массива с индексом 0xF(i).
  if (raw == 0xF0){ Serial.write(vars[0]); }
  if (raw == 0xF1){ Serial.write(vars[1]); }
  if (raw == 0xF2){ Serial.write(vars[2]); }
  if (raw == 0xF3){ Serial.write(vars[3]); }
  if (raw == 0xF4){ Serial.write(vars[4]); }
  if (raw == 0xF5){ Serial.write(vars[5]); }
  if (raw == 0xF6){ Serial.write(vars[6]); }
  if (raw == 0xF7){ Serial.write(vars[7]); }
  byte adr = raw >> 5;
  vars[adr] = (raw & 0x10) ? ((raw << 4) | (vars[adr] & 0x0F)) : ((raw & 0xF) | (vars[adr] & 0xF0));
}

//if (vars[7] == 0xA){ LState = 0; vars[7] = 0xB; } // 0xEA - Команда запуска.  0xXB - Запущено.
//if (vars[7] == 0xF){ LState = 1; vars[7] = 0xE; } // 0xEF - Команда останова. 0xXE - Остановлено.

vars[4] = (byte)(analogRead(A6) >> 2);

// LState == 255 - Пустое состояние, ожидание события.

// Пуск. Загрука данных и запуск прерываний.
if (LState == 0){
  protruding_label = vars[0];
  feed_delay = (!vars[1]) ? 1 : vars[1];
  feed_speed = (unsigned int)(0xFFFF/(vars[2] + 1));
  Serial.write(feed_speed>>8);
  Serial.write(feed_speed&0xFF);
  lgs_threshold = vars[3];
  T2_start();
  T1_start();
  attachInterrupt(0, BottleSensor, FALLING); // Назначение обработчика прерывания по падающему фронту на ножке D2.
  LState = 255;
}

// Стоп. Остановка прерываний, сброс обработчиков.
if (LState == 1){
  T2_stop();
  T1_stop();
  detachInterrupt(0);
  LState = 255;
}

// Подача основной части этикетки.
if (LState == 2){
  if (step_counter == max_feed_steps){
    vars[6] |= 0x1; //Serial.println("Ошибка 1: Достигнуто максимальное число шагов подачи этикетки.");
    stepper = false;
    step_counter = 0; // Сбросить счётчик шагов.
    LState = 255;
  }
  if (vars[4] < vars[3]){ // Если значение АЦП ниже установленного порога - сработал ДРЭ.
    //Serial.println("Поступил сигнал от ДРЭ.");
    step_counter = 0; // Сбросить счётчик шагов.
    LState = 3;
  }
}

// Подача вылета.
if (LState == 3){
  if (step_counter == protruding_label){
    stepper = false;
    //Serial.println("Done!");
    step_counter = 0; // Сбросить счётчик шагов.
    LState = 255;
  }
}

}

void BottleSensor(){ // Обработчик датчика бутылки.
  /* Ищет свободное место в очереди, и добавляет в неё
   * новое событие от датчика бутылки со значением
   * времени задержи до подачи этикетки.
   */
  //Serial.println("Поступил сигнал от датчика бутылки.");
  Serial.write(0xCC);
  for (byte i = 0; i <= 8; i++){
    if (i == 8){ // Счетчик вышел за пределы очереди.
      vars[6] |= 0x2; //Serial.println("Ошибка 2: Очередь заполнена.");
      //Error(0);
      break;
    }
    if (queue[i] == 0){ // Первая попавшаяся ячейка пуста.
      queue[i] = feed_delay;
      break;
    }
  }
}

ISR(TIMER2_OVF_vect){ // Обработчик прерывания по таймеру2. Срабатывает 30 раз в секунду.
  for (byte i = 0; i < 8; i++){
    if (queue[i] != 0){
      if (queue[i] == 1){
        if (stepper){
          vars[6] |= 0x4; //Serial.println("Ошибка 3. Частая подача бутылки!");
          //Error(1);
        }else{
          //Serial.println("Начата подача этикетки");
          stepper = true;
          LState = 2;
        }
      }
      queue[i]--;
    }
  }
}

ISR(TIMER1_COMPA_vect) { // Обработчик прерывания по таймеру 1.
  T1_start(feed_speed); // Первым делом - задание паузы до следующего тика.
  if(stepper){ // Если разрешена работа:
    digitalWrite(LED_BUILTIN, !tick_flag); // отправить эту часть полушага драйверу,
    if (tick_flag){ // на втором полушаге считаем шаги:
      step_counter++; // Увеличиваем счётчик шагов,
    }
    tick_flag = !tick_flag; // выбрать следующий полушаг.
  }else if (tick_flag){
    tick_flag = false;
    digitalWrite(LED_BUILTIN, LOW);
  }
}
