#include "LedControl.h"
#include "FontLEDClock.h"
#include "StackArray.h"

#define BUTTON_BACK   3
#define BUTTON_UP     7
#define BUTTON_DOWN   8
#define BUTTON_ENTER  9

#define CS_PIN        10
#define CLK_PIN       13
#define DIN_PIN       11

#define BUZZER_PIN    5
//////////////////////////////////////////////////////////////////////// Variable Setup ////////////////////////////////////////////////////////////////////////
LedControl lc = LedControl(11,13,10,4);
short ledIntensity = 8;

enum{                                       // create enum list for map state with number
  STATE_HOME = 0,
  STATE_MENUMAIN,
  
  START_MAIN_STATE,
    STATE_MENUSET,
    STATE_STOPWATCH,
    STATE_COUNTDOWN,
  END_MAIN_STATE,
  
  START_SUB_STATE_SET,
      STATE_SETTIME,
      STATE_SETALARM,
  END_SUB_STATE_SET,
};
StackArray<byte> state;                       // create <stack>state to keep state
byte selectState = STATE_HOME;                // create selectState to use in MAINMENU, MENUSET State for select next state to go

struct Clock{                                 // create clock
  byte hour = 0;
  byte minute = 0;
  byte second = 0;
}clock;
struct Alarmclock{                            // create alarmclock
  byte hour = 0;
  byte minute = 0;
  byte second = 0;
  bool enable = false;
}alarmclock;
struct Stopwatch{                             // create stopwatch
  byte minute = 0;
  byte second = 0;
  unsigned millisec = 0;
  bool running = false;
}stopwatch;
struct Countdown{                             // create countdown
  short hour = 0;
  short minute = 0;
  short second = 0;
  bool running = false;
}countdown;
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////// Interrupt Service Routine /////////////////////////////////////////////////////////////
float timer1_start = 3036;                    // timer start point #second : 65535-62500 (+1) = 3035 (+1))
ISR(TIMER1_OVF_vect)                          // Main Clock timer              
{
  TCNT1 = timer1_start;                       // preload timer
  clock.second++;
  if(clock.second==60){
    clock.second = 0;
    clock.minute++;
  }
  if(clock.minute==60){
    clock.minute = 0;
    clock.hour++;
  }
  if(clock.hour==24){
    clock.hour = 0;
  }
}
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////// Setup Program /////////////////////////////////////////////////////////////////////////
void setup(){ 
  initPin();
  initTimer();
  initLed();
  initState();
  Serial.begin(9600);
}
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////// Main Program //////////////////////////////////////////////////////////////////////////
void loop(){
  //------------ get top of <stack>state to currentState ------------
  unsigned currentState = state.peek();
    
  //---------------------- STATE_HOME -------------------------------
  if(currentState == STATE_HOME){           
    show_clock();                                           // show main clock counting
    show_alarm_enable_bar();                                // show enable bar when alarmclock is enable
    
    set_intensity();
    if(digitalRead(BUTTON_BACK)==LOW){
      delay(120);                                           // debouncing 120ms
      state.push(STATE_MENUMAIN);                           // push STATE_MENUMAIN to <stack>state
      clear_display();
    }

    if(alarmclock.enable){
      if(clock.hour == alarmclock.hour && clock.minute == alarmclock.minute){
        alert_alarm();
        if(clock.second==59 || digitalRead(BUTTON_ENTER)==LOW){
          alarmclock.enable = false;
        }
      }
    }
  } 
  
  //---------------------- STATE_MENUMAIN ----------------------------
  else if(currentState == STATE_MENUMAIN){  
    selector_state(START_MAIN_STATE,END_MAIN_STATE);        // show MAINMENU list for go to next state
  }
  
  //---------------------- STATE_MENUSET -----------------------------
  else if(currentState == STATE_MENUSET){
    selector_state(START_SUB_STATE_SET,END_SUB_STATE_SET);  // show SETMENU list for go to next state
  }
  
  //---------------------- STATE_SETTIME -----------------------------
  else if(currentState == STATE_SETTIME){  
    loop_set_clock();                                       // set clock use loop in func
  }
  
  //---------------------- STATE_SETALARM ----------------------------
  else if(currentState == STATE_SETALARM){  
    loop_set_alarm();                                       // set alarmclock use loop in func
  }
  
  //---------------------- STATE_STOPWATCH ---------------------------
  else if(currentState == STATE_STOPWATCH){
    loop_stopwatch();                                       // run stopwatch with loop in func
  }
  
  //---------------------- STATE_COUNTDOWN ---------------------------
  else if(currentState == STATE_COUNTDOWN){ 
    loop_countdown();                                       // run countdown with loop in func
  }
}
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////// User Define Func //////////////////////////////////////////////////////////////////////
/********************* Initialization **************************/
void initPin(){                                      // func to set initPin
  pinMode(BUTTON_BACK,INPUT_PULLUP);
  pinMode(BUTTON_UP,INPUT_PULLUP);
  pinMode(BUTTON_DOWN,INPUT_PULLUP);
  pinMode(BUTTON_ENTER,INPUT_PULLUP);
  pinMode(BUZZER_PIN,OUTPUT);
}
void initTimer(){                                    // func to set initTimer1
  noInterrupts();
  TCCR1A = 0;
  TCCR1B = 0;
  TCNT1 = timer1_start;          
  TCCR1B = TCCR1B|(1 << CS12);  
  TIMSK1 = TIMSK1|(1 << TOIE1); 
  interrupts();
}
void initLed(){                                      // func to set initLed
  byte devices = lc.getDeviceCount();
  for(byte address=0; address<devices; address++){
    lc.shutdown(address,false);
    lc.setIntensity(address,8);
    lc.clearDisplay(address);
  }
}
void initState(){                                    // func to set initState
  state.push(STATE_HOME);                                     // push STATE_HOME to <stack>state for initial
}

/********************* Function in STATE_HOME ******************/
void show_clock(){                                   // func to show mainclock count on Led Dotmatrix 8x32
  print_char( 2, 1, clock.hour / 10 + '0');                   // print hour 
  print_char( 6, 1, clock.hour % 10 + '0');
  
  print_char(12, 1, clock.minute / 10 + '0');                 // print minute
  print_char(17, 1, clock.minute % 10 + '0');
  
  print_char(23, 1, clock.second / 10 + '0');                 // print second
  print_char(27, 1, clock.second % 10 + '0');

  if(clock.second % 2 != 0){                                  // print ':' with brink
    print_char( 9, 1, ' ');
    print_char( 20, 1, ' ');  
  }
  else{
    print_char( 9, 1, ':');
    print_char( 20, 1, ':');
  }
}

/********************* Function in MENUMAIN, MENUSET ***********/
void selector_state(byte startstate, byte endstate){  // func to create & manage list for MENUMAIN, MENUSET state
  //------------ scope range of list state from enum  ---------------
  if(selectState<=startstate){                                 
    selectState = startstate+1;
  }
  else if(selectState>=endstate){
    selectState = endstate-1;
  }
  
  //------------ display seletState on Led Dotmatix 8x32 --------------
  show_selectstate();  

  //---------------------- get input ----------------------------------
  if(digitalRead(BUTTON_UP)==LOW){
    delay(120);                                                 // debouncing 120ms
    selectState--;
  }
  else if(digitalRead(BUTTON_DOWN)==LOW){
    delay(120);                                                 // debouncing 120ms
    selectState++;
  }
  else if(digitalRead(BUTTON_ENTER)==LOW){
    delay(120);                                                 // debouncing 120ms
    state.push(selectState);
    clear_display();
  }
  else if(digitalRead(BUTTON_BACK)==LOW){
    delay(120);                                                 // debouncing 120ms
    state.pop();                                                // pop top of <stack>state when go back state
    selectState = state.peek();                                 // selectState point to new top of <stack>state
    clear_display();
  }
}
void show_selectstate(){                              // func to show selectState on Led Dotmatix 8x32
  //---------------------- main menu list ---------------------------
  if(selectState==STATE_MENUSET){
    print_char(2,1,'S');
    print_char(6,1,'E');
    print_char(10,1,'T');
    print_char(14,1,' ');
    print_char(18,1,' ');
  }
  else if(selectState==STATE_STOPWATCH){
    print_char(2,1,'S');
    print_char(6,1,'T');
    print_char(10,1,'O');
    print_char(14,1,'P');
    print_char(18,1,' ');
  }
  else if(selectState==STATE_COUNTDOWN){
    print_char(2,1,'C');
    print_char(6,1,'O');
    print_char(10,1,'U');
    print_char(14,1,'N');
    print_char(18,1,'T');
  }

  //---------------------- set menu list ----------------------------
  else if(selectState==STATE_SETTIME){
    print_char(2,1,'S');
    print_char(6,1,'E');
    print_char(10,1,'T');
    
    plot(14,2,true);
    plot(14,3,true);
    plot(14,4,true);
    plot(15,3,true);

    print_char(17,1,'T');
    print_char(21,1,'I');
    print_char(25,1,'M');
    print_char(29,1,'E');
  }
  else if(selectState==STATE_SETALARM){
    print_char(2,1,'S');
    print_char(6,1,'E');
    print_char(10,1,'T');
    
    plot(14,2,true);
    plot(14,3,true);
    plot(14,4,true);
    plot(15,3,true);
    
    print_char(17,1,'A');
    print_char(21,1,'L');
    print_char(25,1,'R');
    print_char(29,1,'M');
  }
}

/********************* Function in STATE_SETTIME ***************/
void loop_set_clock(){                                // func to set clock with loop
  bool setting = true;
  short settingClock[3];
  settingClock[0] = clock.hour;     
  settingClock[1] = clock.minute;   
  settingClock[2] = clock.second; 

  byte index = 0;
  while(setting){
    //---------------------- display set clock ----------------------------
    print_char( 2, 1, settingClock[0] / 10 + '0');          
    print_char( 6, 1, settingClock[0] % 10 + '0');
    print_char( 9, 1, ':');
    print_char(12, 1, settingClock[1] / 10 + '0'); 
    print_char(17, 1, settingClock[1] % 10 + '0');
    print_char( 20, 1, ':'); 
    print_char(23, 1, settingClock[2] / 10 + '0'); 
    print_char(27, 1, settingClock[2] % 10 + '0');
    delay(80);                                                  //delay display and debouncing 80 ms
    if(index == 0){
      print_char(2,1,' ');
      print_char(6,1,' ');
    }
    else if(index == 1){
      print_char(12,1,' ');
      print_char(17,1,' ');
    }
    else if(index == 2){
      print_char(23,1,' ');
      print_char(27,1,' ');
    }
    delay(40);                                                  //delay display and debouncing 40 ms

    //---------------------- get input ----------------------------------
    if(digitalRead(BUTTON_UP)==LOW){
      settingClock[index]++;
      if(settingClock[0]>23){
        settingClock[0] = 0;
      }
      if(settingClock[1]>59){
        settingClock[1] = 0;
      }
      if(settingClock[2]>59){
        settingClock[2] = 0;
      }
    }
    if(digitalRead(BUTTON_DOWN)==LOW){
      settingClock[index]--;
      if(settingClock[0]<0){
        settingClock[0] = 23;
      }
      if(settingClock[1]<0){
        settingClock[1] = 59;
      }
      if(settingClock[2]<0){
        settingClock[2] = 59;
      }
    }
    if(digitalRead(BUTTON_ENTER)==LOW){
      index++;                                                 // increase index for setting 
      if(index>2){
        index = 0;
      } 
    }
    if(digitalRead(BUTTON_BACK)==LOW){
      clock.hour = settingClock[0];                            // assign setting hour to main clock
      clock.minute = settingClock[1];                          // assign setting minute to main clock
      clock.second = settingClock[2];                          // assign setting second to main clock
      setting = false;                                         // set setting false to [exit] loop
      
      state.pop();                                             // pop top of <stack>state when go back state
      clear_display();
    }
  }
}

/********************* Function in STATE_SETALARM **************/
void loop_set_alarm(){                                // func to set alarmclock with loop
  //------------- display alarm enable status  ------------------------
  show_alarm_enable_bar();                                     // show alarm enable status   
  
  bool setting = true;
  short settingClock[3];
  settingClock[0] = alarmclock.hour;     
  settingClock[1] = alarmclock.minute;   
  settingClock[2] = alarmclock.second; 

  byte index = 0;
  while(setting){
    //---------------------- display set alarmclock -----------------------
    print_char( 2, 1, settingClock[0] / 10 + '0');          
    print_char( 6, 1, settingClock[0] % 10 + '0');
    print_char( 9, 1, ':');
    print_char(12, 1, settingClock[1] / 10 + '0'); 
    print_char(17, 1, settingClock[1] % 10 + '0');
    print_char( 20, 1, ':'); 
    print_char(23, 1, settingClock[2] / 10 + '0'); 
    print_char(27, 1, settingClock[2] % 10 + '0');
    delay(80);                                                  //delay display and debouncing 80 ms
    if(index == 0){
      print_char(2,1,' ');
      print_char(6,1,' ');
    }
    else if(index == 1){
      print_char(12,1,' ');
      print_char(17,1,' ');
    }
    else if(index == 2){
      print_char(23,1,' ');
      print_char(27,1,' ');
    }
    delay(40);                                                  // delay display and debouncing 40 ms

    //---------------------- get input ----------------------------------
    if(digitalRead(BUTTON_UP)==LOW){
      settingClock[index]++;
      if(settingClock[0]>23){
        settingClock[0] = 0;
      }
      if(settingClock[1]>59){
        settingClock[1] = 0;
      }
      if(settingClock[2]>59){
        settingClock[2] = 0;
      }
    }
    if(digitalRead(BUTTON_DOWN)==LOW){
      settingClock[index]--;
      if(settingClock[0]<0){
        settingClock[0] = 23;
      }
      if(settingClock[1]<0){
        settingClock[1] = 59;
      }
      if(settingClock[2]<0){
        settingClock[2] = 59;
      }
    }
    if(digitalRead(BUTTON_ENTER)==LOW){
      unsigned long pressed = millis();
      while(digitalRead(BUTTON_ENTER)==LOW && millis()-pressed < 1000){   // wait pressed Enter
      }
      
      if(millis()-pressed < 1000){                                // if pressed < 1s
        index++;                                                  // increase index for set alarm 
        if(index>2){
          index = 0;
        }
      } 
      else{                                                       // if pressed >= 1s
        alarmclock.hour = settingClock[0];                        // assign setalarm hour to alarmclock
        alarmclock.minute = settingClock[1];                      // assign setalarm hour to alarmclock
        alarmclock.second = settingClock[2];                      // assign setalarm hour to alarmclock
        
        alarmclock.enable = !alarmclock.enable;                   // set enable status of alarmclock [on->off , off->on]
        show_alarm_enable_bar();                                  // show alarm enable status again because status changed
      }
    }
    if(digitalRead(BUTTON_BACK)==LOW){
      setting = false;                                            // set setting false to [exit] loop
      
      state.pop();                                                // pop top of <stack>state when go back state
      clear_display();
    }
  }
}
void show_alarm_enable_bar(){                          // func to show alarm enable status 
  for(byte x=2;x<30;x++){
    plot(x,7,alarmclock.enable);
  }
}
void alert_alarm(){                                    // func to action alert alarm 
  fade_down();
  tone(BUZZER_PIN,200);
  
  delay(200);
  
  fade_up();
  noTone(BUZZER_PIN);
}

/********************* Function in STATE_STOPWATCH *************/
void loop_stopwatch(){                                 // func to run stopwatch with loop
  //------------- display stopwatch  ----------------------------------
  show_stopwatch();                                            // show stopwatch value
  
  unsigned long startMillis;
  //---------------------- get input ----------------------------------
  if(digitalRead(BUTTON_ENTER)==LOW){     
    delay(120);                                                // debouncing 120ms
    stopwatch.running = true;                                  // set stopwatch.running true for [enter] loop
    startMillis = millis();
  }
  while(stopwatch.running){
    stopwatch.millisec += millis()-startMillis;
    startMillis = millis();
    
    if(stopwatch.millisec >= 1000){
      stopwatch.millisec = stopwatch.millisec - 1000;
      stopwatch.second++;
    }
    if(stopwatch.second == 60){
      stopwatch.second = 0;
      stopwatch.minute++;
    }
    /*if minute >= 60 this stopwatch still run and display that minute such as minute at 71,12,99*/
    show_stopwatch();                                           // show stopwatch value again because value changed
    
    //---------------------- get input ----------------------------------
    if(digitalRead(BUTTON_ENTER)==LOW){
      delay(120);                                               // debouncing 120ms
      stopwatch.running = false;                                // set stopwatch.running false for [exit] loop
    }
  }

  //---------------------- get input ----------------------------------
  if(digitalRead(BUTTON_UP)==LOW && digitalRead(BUTTON_DOWN)==LOW){
    delay(120);                                                 // debouncing 120ms
    reset_stopwatch();
    show_stopwatch();
  }
  if(digitalRead(BUTTON_BACK)==LOW){
    delay(120);                                                 // debouncing 120ms
    state.pop();                                                // pop top of <stack>state when go back state
    clear_display();
  }
}
void show_stopwatch(){                                 // func to show stopwatch count on Led Dotmatrix 8x32   
  print_char( 2, 1, stopwatch.minute / 10 + '0');               //print minute
  print_char( 6, 1, stopwatch.minute % 10 + '0');
  
  print_char(12, 1, stopwatch.second / 10 + '0');               //print second
  print_char(17, 1, stopwatch.second % 10 + '0');
  
  print_char(23, 1, stopwatch.millisec / 100 + '0');            //print millisec
  print_char(27, 1, (stopwatch.millisec % 100)/10 + '0');

  if(stopwatch.second % 2 != 0){                                //print ':' with brink
    print_char( 9, 1, ' ');
    print_char( 20, 1, ' ');  
  }
  else{
    print_char( 9, 1, ':');
    print_char( 20, 1, ':');
  }
}
void reset_stopwatch(){                                 // func to reset stopwatch      
  stopwatch.minute = 0;
  stopwatch.second = 0;
  stopwatch.millisec = 0;
}

/********************* Function in STATE_COUNTDOWN *************/
void loop_countdown(){                                  // func to run countdown with loop
  //------------- display countdown  ----------------------------------
  show_countdown();                                             // show countdown value
  unsigned long startMillis;
  
  //---------------------- get input ----------------------------------
  unsigned long pressed;                                          
  if (digitalRead(BUTTON_ENTER) == LOW){                        // check when prees Enter
    pressed = millis();
    while(digitalRead(BUTTON_ENTER) == LOW && millis() - pressed < 1000){             //wait pressed Enter
    }

    if(millis() - pressed >= 1000){                                                   // if pressed Enter >= 1s
      loop_set_countdown();                                                           // [enter] loop to set start countdown time
    }
    else if(countdown.hour != 0 || countdown.minute != 0 || countdown.second != 0){   // if preesed Enter < 1s
      countdown.running = true;                                                       // set countdown.running true for [Enter] countdown loop
      startMillis = millis();                                                         // remember start millis() 
    }
  }
  
  
  while(countdown.running) {                                                          
    if (countdown.hour == 0 && countdown.minute == 0 && countdown.second == 0 && countdown.running == true) { 
      alert_countdown();                                                             
      countdown.running = false;
    }
    else{
      if(millis() - startMillis >= 1000) {
        startMillis = millis() + (1000 - (millis() - startMillis));
        countdown.second--;
      }
      if(countdown.second < 0){
        countdown.second = 59;
        countdown.minute--;
      }
      if(countdown.minute < 0){
        countdown.minute = 59;
        countdown.hour--;
      }
    }
    show_countdown();                                           // show countdown value again because value changed

    if(digitalRead(BUTTON_ENTER) == LOW){
      delay(120);                                               // debouncing 120ms
      countdown.running = false;                                // set countdown.running false for [Exit] countdown loop
    }
  }

  if(digitalRead(BUTTON_UP) == LOW && digitalRead(BUTTON_DOWN) == LOW){
    delay(120);                                                 // debouncing 120ms
    reset_countdown();                                          // reset stopwatch
    show_countdown();                                           // show countdown value again because value changed
  }
  if(digitalRead(BUTTON_BACK) == LOW){
    delay(120);                                                 // debouncing 120ms
    state.pop();                                                // pop top of <stack>state when go back state
    clear_display();
  }
}

void loop_set_countdown(){                             // func to set countdown with loop
  bool setting = true;
  short settingClock[3];
  settingClock[0] = countdown.hour;
  settingClock[1] = countdown.minute;
  settingClock[2] = countdown.second;

  byte index = 0;
  while(setting){
    //---------------------- display set countdown -----------------------
    print_char( 2, 1, settingClock[0] / 10 + '0');
    print_char( 6, 1, settingClock[0] % 10 + '0');
    print_char( 9, 1, ':');
    print_char(12, 1, settingClock[1] / 10 + '0');
    print_char(17, 1, settingClock[1] % 10 + '0');
    print_char( 20, 1, ':');
    print_char(23, 1, settingClock[2] / 10 + '0');
    print_char(27, 1, settingClock[2] % 10 + '0');
    delay(80);                                                  // delay display and debouncing 80 ms
    if(index == 0){
      print_char(2, 1, ' ');
      print_char(6, 1, ' ');
    }
    else if(index == 1){
      print_char(12, 1, ' ');
      print_char(17, 1, ' ');
    }
    else if(index == 2){
      print_char(23, 1, ' ');
      print_char(27, 1, ' ');
    }
    delay(40);                                                  // delay display and debouncing 40 ms

    //---------------------- get input ----------------------------------
    if(digitalRead(BUTTON_UP) == LOW){
      settingClock[index]++;
      if(settingClock[0] > 23){
        settingClock[0] = 0;
      }
      if(settingClock[1] > 59){
        settingClock[1] = 0;
      }
      if(settingClock[2] > 59){
        settingClock[2] = 0;
      }
    }
    if(digitalRead(BUTTON_DOWN) == LOW){
      settingClock[index]--;
      if(settingClock[0] < 0){
        settingClock[0] = 23;
      }
      if(settingClock[1] < 0){
        settingClock[1] = 59;
      }
      if(settingClock[2] < 0){
        settingClock[2] = 59;
      }
    }
    if(digitalRead(BUTTON_ENTER) == LOW){
      index++;
      if(index > 2){
        index = 0;
      }
    }
    if(digitalRead(BUTTON_BACK) == LOW){
      delay(120);                                               // debouncing 120ms
      countdown.hour = settingClock[0];                         // assign setcountdown hour to countdownclock
      countdown.minute = settingClock[1];                       // assign setcountdown minute to countdownclock
      countdown.second = settingClock[2];                       // assign setcountdown second to countdownclock
      setting = false;                                          // set setting false to [exit] loop
    }   
  }
}

void show_countdown(){
  print_char( 2, 1, countdown.hour / 10 + '0');                 // print hour
  print_char( 6, 1, countdown.hour % 10 + '0');

  print_char(12, 1, countdown.minute / 10 + '0');               // print minute
  print_char(17, 1, countdown.minute % 10 + '0');

  print_char(23, 1, countdown.second / 10 + '0');               // print second
  print_char(27, 1, countdown.second % 10 + '0');

  if(countdown.second % 2 != 0){                                // print ':' with brink
    print_char( 9, 1, ' ');
    print_char( 20, 1, ' ');
  }
  else{
    print_char( 9, 1, ':');
    print_char( 20, 1, ':');
  }
}

void alert_countdown(){                                 // func to action alert countdown end   
  while(digitalRead(BUTTON_ENTER)!=LOW){
    fade_down();
    tone(BUZZER_PIN, millis() % 2001 + 500);                      //tone in frequency range [500 to 2500]

    delay(200);

    fade_up();
    noTone(BUZZER_PIN);
  }
}

void reset_countdown(){                                 // func to reset coundown
  countdown.hour = 0;
  countdown.minute = 0;
  countdown.second = 0;
}

/********************* Display *********************************/
/*credit by Thana Hongsuwan MEDIUM */
void plot(unsigned x ,unsigned y ,bool value){
  byte address;
  if(x >= 0 && x <= 7){
    address = 3;
  }
  if(x >= 8 && x <= 15){
    address = 2;
    x = x - 8;
  }
  if(x >= 16 && x <= 23){
    address = 1;
    x = x - 16;
  }
  if(x >= 24 && x <= 34){
    address = 0;
    x = x - 24;
  }

  lc.setLed(address,y,x,value);
}
/*credit by Thana Hongsuwan MEDIUM */
void print_char(unsigned x ,unsigned y ,char c){
  unsigned dots;
  if (c >= 'A' && c <= 'Z' || (c >= 'a' && c <= 'z') ) { c &= 0x1F; }   // A-Z maps to 1-26 
  else if (c >= '0' && c <= '9') { c = (c - '0') + 32; }
  else if (c == ' ') { c = 0;  }                                        // space 
  else if (c == '.') { c = 27; }                                        // full stop 
  else if (c == ':') { c = 28; }                                        // colon 
  else if (c == '\''){ c = 29; }                                        // single quote mark 
  else if (c == '!') { c = 30; }                                        // single quote mark 
  else if (c == '?') { c = 31; }                                        // single quote mark 

  for (uint8_t col = 0; col < 3; col++) {
    dots = pgm_read_byte_near(&mytinyfont[c][col]);
    for (uint8_t row = 0; row < 5; row++) {
      if (dots & (16 >> row))
        plot(x + col, y + row, true);
      else
        plot(x + col, y + row, false);
    }
  }
}
/*credit by Thana Hongsuwan MEDIUM */
void fade_up(){
  for(short i=0;i<ledIntensity;i++){
    for(byte address=0;address<4;address++){
      lc.setIntensity(address,i);
    }
  }
}
/*credit by Thana Hongsuwan MEDIUM */
void fade_down(){
  for(short i=ledIntensity;i>=0;i--){
    for(byte address=0;address<4;address++){
      lc.setIntensity(address,i);
    }
  }
}
/*credit by Thana Hongsuwan MEDIUM */
void clear_display(){
  for(byte address=0; address<4; address++){
    lc.clearDisplay(address);
  }
}
void set_intensity(){
  if(digitalRead(BUTTON_UP)==LOW){
    ledIntensity++;
  }
  else if(digitalRead(BUTTON_DOWN)==LOW){
    ledIntensity--;
  }

  if(ledIntensity<0){
    ledIntensity = 0;
  }
  else if(ledIntensity>15){
    ledIntensity = 15;
  }
  
  for(byte address=0; address<4; address++){
    lc.setIntensity(address,ledIntensity);
  }
}
