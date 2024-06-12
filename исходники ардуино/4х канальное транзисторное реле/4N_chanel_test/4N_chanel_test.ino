// Подключаем библиотеку для работы с силовыми ключами
#include <iarduino_I2C_Relay.h>

// Создаём объекты библиотеки
iarduino_I2C_Relay fets1(0x09);

void setup()
{
    // Инициируем работу с модулями
    fets1.begin();

    // Выключаем все каналы обоих модулей
    fets1.digitalWrite(ALL_CHANNEL, LOW);
}

void loop()
{
    // Включаем первые каналы обоих модулей
    fets1.digitalWrite(1, HIGH);
    
    // Ждём десять секунд
    delay(1000);

    // Выключаем все каналы обоих модулей
    fets1.digitalWrite(ALL_CHANNEL, LOW);
    
    delay(50);

    // Включаем вторые каналы обоих модулей
    fets1.digitalWrite(2, HIGH);
    
    // Ждём десять секунд
    delay(1000);

    // Выключаем все каналы обоих модулей
    fets1.digitalWrite(ALL_CHANNEL, LOW);
    
    delay(50);
}
