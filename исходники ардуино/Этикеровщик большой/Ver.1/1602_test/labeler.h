volatile byte protruding_label;   // Выступающая часть этикетки (вылет);
volatile byte feed_delay;         // Задержка подачи.
volatile byte feed_speed = 2000;         // Скорость подачи.
volatile byte lgs_threshold;      // label gap sensor threshold - порог срабатывания датчика разрыва этикетки.

#define max_feed_steps 20000 // Максимальное (безаварийное) количество шагов подачи этикетки.

volatile byte queue[8] = {0,0,0,0,0,0,0,0}; // Очередь срабатываний датчика бутылок.
volatile unsigned int step_counter = 0;
volatile boolean stepper = false; // Если установлен флаг, работает ШД.
volatile boolean tick_flag = false; // Флаг полушага.
volatile boolean lgs_flag = false; // Если установлен флаг, поступил сигнал от ДРЭ.
byte LState = 0;

void T2_start(){
  // инициализация Timer2
  cli();                     // Запрет прерываний
  TCCR2B = 0x03;             // Предделитель на 32 (976.5625Гц).
  TIMSK2 |= (1 << TOIE2);    // Включение прерываний по переполнению
  sei();                     // Разрешение прерываний
}

void T2_stop(){
  cli();                     // Запрет прерываний
  TIMSK2 = 0;                // Выключение прерываний по переполнению
  sei();                     // Разрешение прерываний
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
  // инициализация Timer1
  cli();                     // Запрет прерываний.
  TCCR1B = 0;                // CRC режим без предделителя (16МГц/ctr).
  TIMSK1 = 0;                // Включить прерывание по совпадению со счётчиком.
  sei();                     // Разрешение прерываний
}

void BottleSensor(){ // Обработчик датчика бутылки.
  /* Ищет свободное место в очереди, и добавляет в неё
   * новое событие от датчика бутылки со значением
   * времени задержи до подачи этикетки.
   */
  for (byte i = 0; i <= 8; i++){
    if (i == 8){ // Счетчик вышел за пределы очереди.
      Serial.println("Ошибка: Очередь заполнена.");
      //Error(0);
      break;
    }
    if (queue[i] == 0){ // Первая попавшаяся ячейка пуста.
      queue[i] = feed_delay;
      break;
    }
  }
}

ISR(TIMER2_OVF_vect){ // Обработчик прерывания по таймеру2. Срабатывает 976 раз в секунду.
  for (byte i = 0; i < 8; i++){
    if (queue[i] != 0){
      if (queue[i] == 1){
        if (stepper){
          Serial.println("Ошибка. Частая подача бутылки!");
          //Error(1);
        }else{
          stepper = true;
          LState = 2;
        }
      }
      queue[i]--;
    }
  }
}

ISR(TIMER1_COMPA_vect) { // Обработчик прерывания по таймеру 1.
  //digitalWrite(LED_BUILTIN, HIGH);
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

void labeler_init(){
  pinMode (A6, INPUT_PULLUP);
  pinMode (2, INPUT_PULLUP);
  pinMode (LED_BUILTIN, OUTPUT);
  pinMode (3, INPUT_PULLUP); // Выход драйвера PUL ШД
  //T2_start();
  //T1_start();
  attachInterrupt(0, BottleSensor, FALLING); // Назначение обработчика прерывания по падающему фронту на ножке D2.
}

void labeler(){

// LState = 0 - Пустое состояние, ожидание события.

lgs_flag = (analogRead(A6) < lgs_threshold);
digitalWrite(LED_BUILTIN, lgs_flag);

if (LState == 1){
  if (step_counter == max_feed_steps){
    Serial.println("Ошибка: Достигнуто максимальное число шагов подачи этикетки.");
    //Error(2);
    stepper = false;
    step_counter = 0; // Сбросить счётчик шагов.
    LState = 0;
  }
  if (lgs_flag){
    Serial.println("Поступил сигнал от ДРЭ.");
    lgs_flag = false; // Сбросить флаг.
    step_counter = 0; // Сбросить счётчик шагов.
    LState = 2;
  }
}

if (LState == 2){
  if (step_counter == protruding_label){
    stepper = false;
    Serial.println("Done!");
    step_counter = 0; // Сбросить счётчик шагов.
    LState = 0;
  }
}

}
