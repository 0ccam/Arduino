 // Подключаем библиотеку для связи по I2C
#include <Wire.h>

// Адрес датчика весов
int temp_address = 0x10;
long c = 0;
byte data[2] = {0x00, 0x02};
byte msg[5];

void setup() {
    // Открываем моединение с компьютером
  Serial.begin(9600);
    // Открываем соединение по I2C
  Wire.begin();
}

void loop() {
    // Отправляем запрос
    // Начинаем общение с датчиком
  Wire.beginTransmission(8);
    // Запрашиваем нулевой регистр
  Wire.write(data, 2);
    // Выполняем передачу
  Wire.endTransmission();
    // Запрашиваем 4 байт
  Wire.requestFrom(8, 5);
    msg[0] = Wire.read();     // статус: 1-получай данные, 2-команда принята, 3-неверная команда.
    msg[1] = Wire.read();     // 0 - вес нестабилен, 1 - вес стабилен.
    msg[2] = Wire.read();     // младший байт.
    msg[3] = Wire.read();     // средний байт.
    msg[4] = Wire.read();     // старший байт + знак.
    c = msg[4];
    c = (c << 8) + msg[3];
    c = (c << 8) + msg[2];
    c <<= 8;
    c /= 2048; // приведение к граммам.
    c -= 1199;
    Serial.println(c);
  Serial.println("_");
    //Небольшая задержка, чтобы данные обновлялись не слишком часто
  delay(500); 
}
