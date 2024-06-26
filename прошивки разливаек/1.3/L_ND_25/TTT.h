/* Модуль управления шаговым двигателем TTT. Изменение 02.05.2023г
 * Возможности модуля:
 * 1. Аппаратный подсчёт количества шагов.
 * 2. Управление длительностью шага.
 * 3. Автоматический плавный разгон двигателя.
 * 4. Автоматическое плавное торможение двигателя.
 * 5. Функция паузы на заданное время.
 * 
 * Предделитель микрошага (драйвер ШД) - 2000/об.
 * Всего 30 оборотов. 2000*30 = 60000 импульсов.
 * 
 * Задействует 3 таймера:
 * Timer 0 - тактовый системный. Управляет планым ускорением/торможением.
 * Timer 1 - счётчик импульсов (шагов ШД). 
 * Timer 2 - тактовый генератор с управляемой длительностию импульса (шага).
 * 
 * Для аппаратного подсчёта шагов необходимо соединить перемычкой выход Таймера 2
 * со входом Таймера 1 - Пины 11(OC2A) и 5(T1) Arduino Nano.
 */

#include "ini.h"
#include "bottles.h"


/* Номера пинов: */
#define OC2A_pin  11    // Пин выхода Таймера 2 (генератора)
#define T1_pin    5     // Пин входа Таймера 1 (счётчика)
#define DIR_pin   12    // Пин выбора направления вращения ШД.
#define UP        false // Движение шпинделя вверх
#define DOWN      true  // Движение шпинделя вниз

/* Глобальные переменные: */
volatile boolean TaskDone = false;  // Флаг выполнения задания.
volatile unsigned int Hcur_mm = Hstrt_mm; // Текущая высота сопел над столом.
volatile unsigned int Timp = 0;

/* Локальные переменные: */
volatile byte Tstep;                // Задание длительности шага.

/* Функция инициализации. */
void TTTInit()
{
  /* Инициализация пинов: */
  pinMode(OC2A_pin, OUTPUT);  // Выход Таймера 2.
  digitalWrite(OC2A_pin, LOW);
  pinMode(T1_pin, INPUT);     // Вход Таймера 1.
  pinMode(DIR_pin, OUTPUT);  // Пин выбора направления вращения ШД на выход.
  digitalWrite(DIR_pin, LOW);
  /* Выключение Таймеров 0..2 запущенных при включении контроллера под управлением загрузчика Arduino: */
  TCCR0B = 0; // Выключение таймера 0. Значение при запуске контроллера: 0b00000011 - запущен с предделителем 64.
  TCCR1B = 0; // Выключение таймера 1. Значение при запуске контроллера: 0b00000011 - запущен с предделителем 64.
  TCCR2B = 0; // Выключение таймера 2. Значение при запуске контроллера: 0b00000100 - запущен с предделителем 64.
  /* Начальные значения счётных регистров Таймеров 0..2: */
  TCNT0 = 0;
  TCNT1 = 0;
  TCNT2 = 0;
  /* Начальные значения регистров сравнения Таймеров 0..2: */
  OCR0A = 249;// Длительность тика системного таймера (1мс).
  OCR1A = 1; // Начальное значение числа шагов.
  OCR2A = 255;// Начальная длительность шага (максимальная).
  /* Установка общего режима работы Таймеров 0..2: */
  TCCR0A = 0; // Переход в общий режим. Значение при запуске контроллера: 0b00000011 - режим ШИМ.
  TCCR1A = 0; // Переход в общий режим. Значение при запуске контроллера: 0b00000001 - режим ШИМ.
  TCCR2A = 0; // Переход в общий режим. Значение при запуске контроллера: 0b00000001 - режим ШИМ.
  /* Установка обработчиков прерываний Таймеров 0 и 1 по совпадению со счётчиком: */
  TIMSK0 = (1<<OCIE0A); // Прерывание по совпадению.
  TIMSK1 = (1<<OCIE1A); // Включить прерывание по совпадению со счётчиком.
  /*  Режим генерации для Таймера 2. */
  TCCR2A = (1<<COM2A0) | (1<<WGM21);
  /*  COM2A0=1 - Изменение состояния вывода OC2A на противоположное при совпадении с регистром OCR2A.
   *  WGM22:0=010 - Режим обновления счётчика TCNT2 при совпадении с регистром OCR2A (СRС режим).
   */
  TCCR0B = (1<<WGM02) | (0<<CS02) | (1<<CS01) | (1<<CS00);  // Включить Таймер 0. CRC режим, 011 - CLK/64 
}

/* Функция вращения шаговым двигателем. */
void TTTRun(int distance_mm, byte Ts)
{
  /* Сбросить флаг выполненности задания: */
  TaskDone = false;
  /* Вычислить и задать направление движение: */
  digitalWrite(DIR_pin, (distance_mm < 0));
  /* Установить задание длительности шага: */
  Tstep = Ts;
  /* Задать фактическую длительность шага: */
  OCR2A = Tstep_max;
  /* Задать порог срабатывания Таймера 1 (счётчика шагов): */
  OCR1A = (unsigned int)((abs(distance_mm) * 200) - 1); // Порог сброса счётчика, и генерации прерывания.
  /* Включить Таймеры 0..2: */
  TCCR1B = (1<<WGM12) | (1<<CS12) | (1<<CS11) | (1<<CS10);  // Включить Таймер 1. CRC режим, внешний источник тактового сигнала.
  //TCCR0B = (1<<WGM02) | (0<<CS02) | (1<<CS01) | (1<<CS00);  // Включить Таймер 0. CRC режим, 100 - CLK/256. 011 - CLK/64
  TCCR2B = (0<<CS22)  | (1<<CS21) | (1<<CS20);              // Включить Таймер 2. Установка предделителя на 64, и разрешение генерации.
}

/* Функция паузы. */
void TTTPause(unsigned int time_ms)
{
  /*  Timer 1 */
  TaskDone = false; // Сбросить флаг выполненности задания.
  OCR1A = time_ms << 4; // Порог сброса счётчика, и генерации прерывания. Время(мс) * 16.
  TCCR1B = (1<<WGM12) | (1<<CS12) | (0<<CS11) | (1<<CS10); // CRC режим, предделитель 1024.
}

/* Обработчик прерывания Таймера 1 (счётчика) по достижению заданного количества шагов. */
ISR(TIMER1_COMPA_vect)
{
  /* Остановка Таймеров 0..2: */
  TCCR2B = 0;
  //TCCR0B = 0;
  TCCR1B = 0;
  /* Обнулить счётчики Таймеров 0..2: */
  //TCNT0 = 0;
  TCNT1 = 0;
  TCNT2 = 0;
  /* Оповестить о выполнении задания: */
  TaskDone = true;
}

/* Обработчик прерывания Таймера 0 */
ISR(TIMER0_COMPA_vect)
{
/*  В Ардуино нет возможности устанавливать обработчик прерывания
 * по переполнению Таймера 0, потому ипользуется прерывание по
 * совпадению TCNT0 с OCR0A (CRC режим).
 */

  /* Инкремент счётчика длительности импульса ДПР с ограничением переполнения. */
  if (Timp < 65535) Timp++;

  /*  Алгоритм плавного разгона. При каждом пробуждении сравнивает
   * фактическое значение длительности шага (OCR2A) с длительностью
   * в задании (Tstep). Если значения различны, приближает на единицу
   * фактическую длительность к заданию (увеличивает или уменьшает).
   */
  if (OCR2A < Tstep){
    OCR2A++;
  } else if (OCR2A > Tstep){
    OCR2A--;
  }

  /*  Алгоритм плавного торможения. Начало момента торможения
   * высчитывается автоматически следующим образом:
   * Если остаток шагов до точки останова (OCR1A-TCNT1)
   * становится меньше, чем функция от скорости движения,
   * то задание длительности шага становится максимальным.
   */
  if ((OCR1A-TCNT1) < ((255-OCR2A)*3)){
     Tstep = Tstep_max;
  }
}

void GOTO(unsigned int Htsk_mm, byte Ts){
  TTTRun((int)(Htsk_mm - Hcur_mm), Ts);
  Hcur_mm = Htsk_mm;
  while(!TaskDone){;} // Ждать в вечном цикле выполнения задания.
}
