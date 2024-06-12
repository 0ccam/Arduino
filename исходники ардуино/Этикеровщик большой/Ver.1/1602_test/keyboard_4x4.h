// Символы клавиатуры.
#define A 10 // A
#define B 11 // B
#define C 12 // C
#define D 13 // D
#define X 14 // *
#define Y 15 // #
#define null 255 // Клавиша не нажата.
// Условные адреса пинов строк.
#define row_1 1
#define row_2 2
#define row_3 4
#define row_4 8
// Условные адреса пинов столбцов.
#define column_1 1
#define column_2 2
#define column_3 4
#define column_4 8
// Типы значений нажатых клавиш.
#define number 0
#define comand 1

byte column; // Значение верхней части порта D. Хранит номер столбца нажатой клавиши.

struct Key {
  boolean pressed = false; // Факт перехода значения клавиш от null к иному значению.
  boolean type; // Тип значения нажатой клавиши.
  byte value; // Значение нажатой клавиши в текущей итерации главного цикла.
  byte old = null; // Значение нажатой клавиши в прошлой итерации главного цикла.
};

Key key;

byte get_column(byte pin){
  PORTB |= B00001111; // Подтянуть нижние 4 пина порта В к плюсу.
  PORTB &= ~pin; // Подтянуть пин по указанному адресу к нулю.
  delay(1); // Подождать, пока пройдут переходные процессы в цепи клавиатуры.
  return ((~PIND) >> 4) & B00001111; // Прочесть порт D.; 
}

void keyboard_init() {
  // Конфигурация пинов напрямую через порты позволяет сэкономить около 200 байт памяти.
  /* Красные провода, столбцы, PD4-PD7, входы с подтяжкой к плюсу через резисторы.
   * Пины Arduino Nano 4,5,6,7.
   */
  DDRD  &= B00001111; // Сбросить вехние 4 пина порта D. Установить на вход.
  PORTD |= B11110000; // Подтянуть верхние 4 пина порта D к плюсу через внутренние резисторы.
  /* Чёрные провода, строки, PB0-PB3, выходы с подтяжкой к плюсу.
   * Пины Arduino Nano 8,9,10,11.
   */
  DDRB  |= B00001111; // Установить нижние 4 пина порта В на выход.
}

void keyboard_listen() {
  /* В неачале новой итерации главного цикла клавиши принимаются не нажатыми.
   *  Вся обработка событий, связанных с нажатием клавиш, должна выполняться
   *  в течении одной итерации главного цикла.
   */
  key.value = null; // Нет значения клавиши.
  key.pressed = false; // Нет факта нажатия.
 
  // Опрос первой строки по столбцам.  
  column = (get_column(row_1));
  if (column == column_1){ key.value = 1; key.type = number; }
  if (column == column_2){ key.value = 2; key.type = number; }
  if (column == column_3){ key.value = 3; key.type = number; }
  if (column == column_4){ key.value = A; key.type = comand; }
  // Опрос второй строки по столбцам.
  column = (get_column(row_2));
  if (column == column_1){ key.value = 4; key.type = number; }
  if (column == column_2){ key.value = 5; key.type = number; }
  if (column == column_3){ key.value = 6; key.type = number; }
  if (column == column_4){ key.value = B; key.type = comand; }
  // Опрос третьей строки по столбцам.
  column = (get_column(row_3));
  if (column == column_1){ key.value = 7; key.type = number; }
  if (column == column_2){ key.value = 8; key.type = number; }
  if (column == column_3){ key.value = 9; key.type = number; }
  if (column == column_4){ key.value = C; key.type = comand; }  
  // Опрос четвёртой строки по столбцам.
  column = (get_column(row_4));
  if (column == column_1){ key.value = X; key.type = comand; }
  if (column == column_2){ key.value = 0; key.type = number; }
  if (column == column_3){ key.value = Y; key.type = comand; }
  if (column == column_4){ key.value = D; key.type = comand; }
  
  
  /* Фактом события нажатия клавиши является не её значение отличное от значения null,
   * а переход от null к одному из 16-ти значений в течении двух итераций главного цикла.
   */
  // Если значение прошлой итерации null, а этой отлично от null, значит это момент нажатия клавиши.
  if ((key.old == null) && (key.value != null)) key.pressed = true; // Установить флаг события.
  key.old = key.value; // Сохранить значение нажатой клавиши для сдедующего цикла.

}
