#define N_OUT 4

uint8_t pins_out[N_OUT] = {6, 7, 5, 4}; // массив пинов - выходов для управления диммирующими модулями
uint8_t pin_int = 2; // пин для внешнего прерывания от детектора нуля

volatile uint8_t states[N_OUT] = {250, 250, 250, 250}; // массив текущих состояний на диммере
volatile uint8_t tic;

// библиотечные функции
void (*func)();
volatile uint16_t dub_tcnt1;

ISR(TIMER1_OVF_vect) 
{
  TCNT1 = dub_tcnt1;    
  (*func)();
}

void StartTimer1(void (*isr)(), uint32_t set_us) {  
  TIMSK1 &= ~(1 << TOIE1); // запретить прерывания по таймеру1
  
  func = *isr;  // указатель на функцию
  TCCR1A = 0;   // timer1 off
  TCCR1B = 0;   // prescaler off (1 << CTC1)-3й бит

  TCCR1B = (1 << CS10);
  
  dub_tcnt1 = 65584 - (set_us << 4);
  TCNT1 = 0;

  TIMSK1 |= (1 << TOIE1); // разрешаем прерывание по переполнению таймера 
  sei();   
}

//********************обработчики прерываний*******************************
void halfcycle() { 
  tic++;  //счетчик  

  for (int i = 0; i < N_OUT; i++) {
    if (states[i] < tic)
      digitalWrite(pins_out[i], 1);
  }
}

// обработка внешнего прерывания. Сработает по переднему фронту
void  detect_up() {
  tic = 0;
  
  TIMSK1 |= (1 << TOIE1); //запустить таймер (resume)  
  attachInterrupt(0, detect_down, HIGH);  //перепрограммировать прерывание на другой обработчик
}  

// обработка внешнего прерывания. Сработает по заднему фронту
void detect_down() {
  TIMSK1 &= ~(1 << TOIE1); //остановить таймер (stop)
  
  //логический ноль на выходы
  for (int i = 0; i < N_OUT; i++)
    digitalWrite(pins_out[i], 0);
  
  tic = 0;
  
  attachInterrupt(0, detect_up, LOW); //перепрограммировать прерывание на другой обработчик
} 

void fade(int dimmer, int start_v, int end_v, int step_v, int t) {
  for (int i = start_v; i != end_v; i += step_v) {
    states[dimmer % N_OUT] = i;
    delay(t);
  }
}

void setup() { 
  for (int i = 0; i < N_OUT; i++) {
    pinMode(pins_out[i], OUTPUT);
    digitalWrite(pins_out[i], 0);
  }
  
  pinMode(pin_int, INPUT);

  Serial.begin(9600);
  
  attachInterrupt(0, detect_up, LOW);  // настроить срабатывание прерывания interrupt0 на pin 2 на низкий уровень
  StartTimer1(halfcycle, 40); //время для одного разряда ШИМ
  
  TIMSK1 &= ~(1 << TOIE1); // остановить таймер
}

void loop() {
  if (Serial.available()) {
    char buf[128];  
    int n = Serial.readBytesUntil('\n', buf, sizeof(buf) - 1);  
    buf[n] = '\0';

    int tmp[N_OUT];
    char cmd[20], cmd2[20];

    sscanf(buf, "%s %s %d %d %d %d", &cmd, &cmd2, &tmp[0], &tmp[1], &tmp[2], &tmp[3]);

    if (strcmp(cmd, "set") + strcmp(cmd2, "rgb") == 0) {
      for (int i = 0; i < N_OUT; i++)    
        states[i] = map(tmp[i], 0, 255, 240, 0);
    }
  }
}  
