volatile bool intFlag = false;   // флаг

void setup() {
  Serial.begin(9600); // открыли порт для связи
  // подключили кнопку на D2 и GND
  pinMode(2, INPUT_PULLUP);
  pinMode(LED_BUILTIN, OUTPUT);
  attachInterrupt(0, buttonTick, FALLING);
}

void buttonTick() {
  intFlag = true;   // подняли флаг прерывания
}

void loop() {
  if (intFlag) {
    intFlag = false;    // сбрасываем
    // совершаем какие-то действия
    Serial.println("Interrupt!");
    digitalWrite(LED_BUILTIN, HIGH);   // turn the LED on (HIGH is the voltage level)
    delay(1000);                       // wait for a second
    digitalWrite(LED_BUILTIN, LOW);
  }  
}
