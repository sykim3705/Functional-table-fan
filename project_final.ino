#include <LiquidCrystal_I2C.h>
#include <Wire.h>

LiquidCrystal_I2C lcd(0x3f, 16, 2); // I2C LCD 객체 선언


// 센서 핀 정보
const int LEDpin_mode1 = 12; // LED pin number about mode1, PWM에 연결
const int LEDpin_timermode = 11; // timermode일 때만 켜지는 LED

const int POT = 0; // 가변저항 pin number, analog포트에 연결

const int colPin[4] = {28, 26, 24, 22};
const int rowPin[4] = {30, 32, 34, 36};
const int keymap[4][4] = {
	{1, 2, 3, 'A'}, // A : mode1 버튼
	{4, 5, 6, 'B'}, // B : mode2 버튼
	{7, 8, 9, 'C'}, // C : timermode 버튼
	{'*', 0, '#', 'D'} // D : reset 버튼
};

const int EN = 4;
const int MC1 = 3;
const int MC2 = 2;

const int echo_pin = 9;
const int trigger_pin = 10;

const int FSR_AnalogPin = 1;


// 사용자 설정 함수
void motor(int speed); // motor의 세기를 조절하는 함수
void brake(); // motor를 정지상태로 환원하는 함수
void LED_intensity(); // PWM으로 밝기를 조절하는 함수
void select_mode(); // keypad값을 입력받아 mode1과 mode2나 timermode를 구분
void countdown_timermode(); // timermode에서 countdown실시


// logical parameter
float var_resistant = 0;
int velocity = 0; // mode 1 에서 모터 속도 조절
int delta = 0; // mode 2 에서 모터 속도 조절

long lastDebounce = 0; // debounce 조절 변수
char key = 0; // keymap의 값을 읽는 변수
int temp = 0; // 버튼 입력을 받을 때 사용되는 변수

int FSR_Reading = 0; // analogRead로 fsr의 값을 받는 변수

boolean flag_mode[2] = {false, false}; // mode 1, mode 2를 구분하는 boolean형의 flag
boolean flag_timermode = false; // flag_timermode가 true일 때만 타이머 동작
boolean flag_timer_setting = false; // true는 초기 분/초 값을 setting할 때만 작동
boolean flag_time_over = false; // true는 분/초 값이 0일 때만 작동
boolean flag_timer_interrupt = false; // true는 time over가 되거나 중간에 reset 버튼을 눌렀을 때만 작동
boolean flag_reset_question = false; // reset 모드에서 key값에 1또는 2를 입력받을 때 까지 reset할건지 여부를 물음
boolean flag_inner_switch = false; // true는 키 'A', 'B' 값을 입력받았을 때 작동

int minute = 0, second = 1; // 분/초 정보 저장
int decreasing_100ms_welcome = 10; // welcome출력에서 100ms 10cycle씩 감소
int decreasing_100ms_mode2 = 30; // mode2 calibrizing시 100ms 30cycle(=3s)씩 감소
int decreasing_100ms_timerMode = 10; // countdown함수에서 100ms 10cycle씩 감소
int decreasing_100ms_timer_setting = 10; // timer setup이 완료되면 100ms 10cycle씩 감소
int decreasing_100ms_time_over = 10; // 정해진 시간이 만료되면 100ms 10cycle씩 감소
int decreasing_100ms_reset_answer = 1; // reset모드에서 yes나 no를 눌렀을 때 버튼 1회당 1씩 감소
int minute_and_second_array[4] = {0, 0, 0, 0}; // 분/초 정보를 한 숫자씩 저장하는 array
int i = 0; // timermode에서 시간을 입력받을 때 이용

float duration = 0; // pulse의 길이를 받는 함수
int distance_initial = 0; // 초기 거리값 측정
int distance_final = 0; // 나중 거리값 측정



void setup() {
  	pinMode(LEDpin_mode1, OUTPUT);

	pinMode(EN, OUTPUT);
	pinMode(MC1, OUTPUT);
	pinMode(MC2, OUTPUT);
	brake();

	pinMode(echo_pin, INPUT);
	pinMode(trigger_pin, OUTPUT);

	for(int index = 0; index < 4; index++) {
		pinMode(rowPin[index], INPUT_PULLUP);
		pinMode(colPin[index], OUTPUT);
		digitalWrite(colPin[index], HIGH);
	}

	lcd.init();
	lcd.cursor();
}

void loop() {
	FSR_Reading = analogRead(FSR_AnalogPin); // 압력센서에 값을 받아들인다

	brake(); // motor 정지
	
	// 압력센서 값이 일정 이상으로 넘어오지 않으면 lcd끄고 waiting... 출력
	lcd.noBacklight();
	lcd.clear();
	lcd.setCursor(2, 0);
	lcd.print("Waiting...");
	lcd.setCursor(0, 1);
	lcd.print("Sit down please");

 	delay(100); // 100ms 단위로 갱신

	while(FSR_Reading > 100) { // 압력센서에 특정값(100) 이상으로 들어온다면
		FSR_Reading = analogRead(FSR_AnalogPin); // 압력센서에 값을 받아들인다

		decreasing_100ms_mode2 = 10; // 껐다 다시 킬 경우 mode2는 처음부터 다시 1초지연 시킴
		
		// mode 1, mode 2 flag 초기화
		flag_mode[0] = false;
		flag_mode[1] = false;

		lcd.clear();
		lcd.backlight();
		lcd.setCursor(2, 0);
		lcd.print("Welcome!");

		decreasing_100ms_welcome--;

		if(decreasing_100ms_welcome < 0) {
			flag_inner_switch = true;
			decreasing_100ms_welcome = 10;
		}

		delay(100);
		
		while(flag_inner_switch == true) {
			FSR_Reading = analogRead(FSR_AnalogPin); // 압력센서에 압력값을 실시간으로 받아들인다
			if(FSR_Reading < 100) {
				flag_inner_switch = false;
			}

			select_mode();

			if(flag_mode[0] == false && flag_mode[1] == false) { // default 창
				brake();

				if(flag_timer_interrupt == false) {
					if(flag_timer_setting == false && flag_time_over == false) { // timer를 setting하거나 종료되었을 때 진입하지 않음
						lcd.clear();
						lcd.setCursor(1, 0);
						lcd.print("Select mode:");
						lcd.setCursor(0, 1);
						lcd.print("S4:mod1 S8:mod2");

						if(flag_timermode == true) { // timermode가 작동중일 때 countdown실시
							countdown_timermode();
						}
					}
				}
			}

			// mode 1 : 가변저항의 크기에 따라 바람의 세기 / LED 밝기 조절
			if(flag_mode[0] == true) {
				var_resistant = analogRead(POT);
				velocity = map(var_resistant, 0, 1023, 180, 250);
				motor(velocity);
				LED_intensity(velocity);

				if(flag_timer_interrupt == false) { // reset버튼이 눌려지지 않았다면
					if(flag_timermode == false) { // non-timermode
						lcd.clear();
						lcd.setCursor(0, 0);
						lcd.print("mode1 selected");
						lcd.setCursor(0, 1);
					}

					if(flag_timermode == true) { // timermode
						lcd.clear();
						lcd.setCursor(0, 0);
						lcd.print("mode1 timermode");
						lcd.setCursor(2, 1);
						lcd.print(minute); // 남은 분
						lcd.setCursor(4, 1);
						lcd.print("m");
						lcd.setCursor(6, 1);
						lcd.print(second); // 남은 초
						lcd.setCursor(8, 1);
						lcd.print("s left");

						countdown_timermode();
					}
				}
			}

			if(flag_mode[0] == false) { // mode 1을 실행시키지 않을 때
				LED_intensity(0); // LED의 밝기를 0으로 setting할 것
			}


			// mode 2 : distance calibration, 거리에따라 LED광량/바람세기 자동조절
			if(flag_mode[1] == true) {
				if(flag_timer_interrupt == false) {
					if(decreasing_100ms_mode2 > 0) {
						brake();

						// 거리측정(distance_initial)
						digitalWrite(trigger_pin, HIGH);
						delayMicroseconds(10);
						digitalWrite(trigger_pin, LOW);

						duration = pulseIn(echo_pin, HIGH);
						distance_initial = duration / 1000000 * 34000 / 2;

						int left_time = (decreasing_100ms_mode2 + 10) / 10;

						lcd.clear();
						lcd.setCursor(1, 0);
						lcd.print("mod2 Calibrize");
						lcd.setCursor(0, 1);
						lcd.print(distance_initial); // 거리값을 출력
						lcd.setCursor(3, 1);
						lcd.print("cm");
						lcd.setCursor(6, 1);
						lcd.print(left_time);
						lcd.setCursor(7, 1);
						lcd.print("s left");

						decreasing_100ms_mode2--;

						if(distance_initial > 25) {
							distance_initial = 25;
						}
					}
				
					else {
						// 거리값 입력받음(distance_final)
						digitalWrite(trigger_pin, HIGH);
						delayMicroseconds(10);
						digitalWrite(trigger_pin, LOW);
				
						duration = pulseIn(echo_pin, HIGH);
						distance_final = duration / 1000000 * 34000 / 2;

						// 풍량계산식
						int delta = 210 + (250 - 210)*(distance_final - distance_initial)/10;
						int temp = 0;

						if(delta < 170) {
							motor(170);
							temp = 170;
						}

						else if(delta > 250) {
							motor(250);
							temp = 250;
						}

						else {
							motor(delta);
							temp = delta;
						}

						int temp_intensity = map(temp, 170, 255, 0, 250);
						LED_intensity(temp_intensity);

						if(flag_timermode == false) { // non-timermode
							lcd.clear();
							lcd.setCursor(1, 0);
							lcd.print("mode2 working");
						}

						if(flag_timermode == true) { // timermode
							lcd.clear();
							lcd.setCursor(0, 0);
							lcd.print("mode2 timermode");
							lcd.setCursor(2, 1);
							lcd.print(minute); // 남은 분
							lcd.setCursor(4, 1);
							lcd.print("m");
							lcd.setCursor(6, 1);
							lcd.print(second); // 남은 초
							lcd.setCursor(8, 1);
							lcd.print("s left");

							countdown_timermode();
						}
					}
				}
			}

			
			// timermode : 사용자가 직접 분/초를 keypad로 입력하여 setting. 특히 이 구문은 초기값을 setting할 때만 쓰임 
			if(flag_timermode == true) {
				analogWrite(LEDpin_timermode, 255); // timermode일 때만 켜지는 LED를 킴

				if(flag_timer_setting == true) {
					if(i >= 0 && i <= 3) {
						lcd.clear();
						lcd.setCursor(0, 0);
						lcd.print("TimerMode SETUP");

						// 2, 3 : min 입력, 4 : "m"
						lcd.setCursor(2, 1);
						lcd.print(minute_and_second_array[0]);
						lcd.setCursor(3, 1);
						lcd.print(minute_and_second_array[1]);
						lcd.setCursor(4, 1);
						lcd.print("m");

						// 6, 7 : sec 입력, 8 : "s"
						lcd.setCursor(6, 1);
						lcd.print(minute_and_second_array[2]);
						lcd.setCursor(7, 1);
						lcd.print(minute_and_second_array[3]);
						lcd.setCursor(8, 1);
						lcd.print("s");

						if(i == 0) {
							lcd.setCursor(2, 1);
							lcd.blink();
						}

						if(i == 1) {
							lcd.setCursor(3, 1);
							lcd.blink();
						}

						if(i == 2) {
							lcd.setCursor(6, 1);
							lcd.blink();
						}

						if(i == 3) {
							lcd.setCursor(7, 1);
							lcd.blink();
						}
					}
					
					else if(i == 4) {
						minute = 10*minute_and_second_array[0] + minute_and_second_array[1];
						second = 10*minute_and_second_array[2] + minute_and_second_array[3];
						
						lcd.clear();
						lcd.setCursor(0, 0);
						lcd.print("Setup Completed!");
						lcd.setCursor(0, 1);
						lcd.print("Wait 1 second...");
						
						decreasing_100ms_timer_setting--;

						if(decreasing_100ms_timer_setting < 0) { // 1초 지나면
							flag_timer_setting = false;
							decreasing_100ms_timer_setting = 10;
							i = 0;
						}
					}
				}

				// setup된 countdown이 모두 끝나면
				if(flag_time_over == true) {
					digitalWrite(LEDpin_timermode, LOW); // 시간이 다 되면 timermode의 LED를 끔

					// flag상으로 default모드에 진입
					flag_mode[0] = false;
					flag_mode[1] = false;

					// LCD에 "time is OVER" 출력
					lcd.clear();
					lcd.setCursor(1, 0);
					lcd.print("time is OVER!");
					
					decreasing_100ms_time_over--;

					if(decreasing_100ms_time_over < 0) {
						// default모드로 진입
						flag_mode[0] = false;
						flag_mode[1] = false;
						flag_timermode = false;
						flag_timer_interrupt = false;
						flag_time_over = false;

						decreasing_100ms_time_over = 10;
					}
				}
			}

			if(flag_timermode == false) { // timermode를 실행시키지 않을 때
				analogWrite(LEDpin_timermode, 0); // timermode임을 알려주는 LED를 끈다
			}


			// reset : 시간 초기화 버튼이 눌려지면 작동, mode1을 제외한 나머지 동작은 모두 일시정지
			if(flag_timer_interrupt == true) {
				if(flag_reset_question == true) { // reset버튼을 누르거나 key가 입력되지 않을 때 진입
					// 여부묻기
					lcd.clear();
					lcd.setCursor(5, 0);
					lcd.print("Reset?");
					lcd.setCursor(0, 1);
					lcd.print("*:YES #:NO");
				}

				if(key == '*') { // yes를 누르면
					// default로 환원, flag_timer_interrupt는 false로 환원
					flag_reset_question = false;

					minute = 0;
					second = 1;

					lcd.clear();
					lcd.setCursor(4, 0);
					lcd.print("Reseted!");
					lcd.setCursor(0, 1);
					lcd.print("press * button..");
						
					decreasing_100ms_reset_answer--;

					if(decreasing_100ms_reset_answer < 0) { // '*' 버튼을 한 번 더 입력받으면
						flag_mode[0] = false;
						flag_mode[1] = false;
						flag_timermode = false;
						flag_timer_setting = false;
						flag_timer_interrupt = false;
						decreasing_100ms_reset_answer = 1;
					}
				}

				if(key == '#') { // No를 누르면
					// 전의 flag상태로 환원, flag_timer_interrupt는 false로 환원
					flag_reset_question = false;

					lcd.clear();
					lcd.setCursor(4, 0);
					lcd.print("Canceled!");
					lcd.setCursor(0, 1);
					lcd.print("press # button..");
						
					decreasing_100ms_reset_answer--;

					if(decreasing_100ms_reset_answer < 0) { // '#' 버튼을 한 번 더 입력받으면
						flag_timer_interrupt = false;
						decreasing_100ms_reset_answer = 1;
					}
				}
			}

			delay(100); // 100ms 단위로 갱신
		}
	}
}



/* ------------------------ 사용자 정의 함수 	------------------------ */

void motor(int speed) {
	digitalWrite(EN, LOW);
	digitalWrite(MC1, HIGH);
	digitalWrite(MC2, LOW);
	analogWrite(EN, speed);
}

void brake() {
	digitalWrite(EN, LOW);
	digitalWrite(MC1, LOW);
	digitalWrite(MC2, LOW);
	analogWrite(EN, HIGH);
}

void LED_intensity(int rate) {
	analogWrite(LEDpin_mode1, rate);
}

void select_mode() {
	key = -1;

	for(int column = 0; column < 4; column++) {
		digitalWrite(colPin[column], LOW);

		for(int row = 0; row < 4; row++) {
			if(digitalRead(rowPin[row]) == LOW) {
				if(millis() - lastDebounce > 200) {
					lastDebounce = millis();
					key = keymap[row][column];
				}
			}
		}

		digitalWrite(colPin[column], HIGH);
	}

	if(key != 0) {
		//key판별
		switch(key) {
			case 'A': // mode 1 버튼
				flag_mode[1] = false;
				flag_mode[0] = true;
				break;

			case 'B': // mode 2 버튼
				decreasing_100ms_mode2 = 30;
				distance_initial = 0;
				distance_final = 0;
				flag_mode[0] = false;
				flag_mode[1] = true;
				break;

			case 'C': // timermode 버튼
				flag_mode[0] = false;
				flag_mode[1] = false;
				flag_timermode = true;
				flag_timer_setting = true;
				break;

			case 'D': // reset 버튼
				flag_timer_interrupt = true;
				flag_reset_question = true;
				break;

			case '*':
				break;

			case '#':
				break;

			default:
				break;
		}
	}
	
	if(key >= 0 && key <= 9) {
		if(flag_timer_setting == true) {
			if(i == 2) {
				if(key >= 0 && key <= 5) {
					minute_and_second_array[i] = key;
					i++;
				}
			}

			else {
				minute_and_second_array[i] = key;
				i++;
			}
		}
	}
}

void countdown_timermode() {
	decreasing_100ms_timerMode--;

	if(decreasing_100ms_timerMode < 0) {
		decreasing_100ms_timerMode = 10;
		second--;
	}

	if(second < 0) {
		second = 59;
		minute--;
	}

	if(minute < 0) {
		flag_time_over = true;
	}
}
