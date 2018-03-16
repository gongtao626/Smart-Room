/*Name:Tao Gong
 *ID:	225787
 *Purpose: Smart room
 *Module: Joystick for option navigation, pushswitch for option selection, rgbLcd for interface displaying, LED 
 *				for air conditioner, stepMotor for door,
 */
//library import
#include "RgbLcd.h"
#include "Temperature.h"
#include "LED.h"
#include "Switch.h"
#include "WiFly.h"
#include "StepMotor.h"
//define the pin for stepMotor
#define DIR_PIN 74
#define STEP_PIN 3
#define MS1_PIN 41
#define MS2_PIN 40
#define SLP_PIN 38
//define pin for rgbLcd
#define RS_PIN 62
#define RW_PIN 63
#define E_PIN 64
#define R_PIN 65
#define G_PIN 66
#define B_PIN 67
#define DATA_PIN4 45
#define DATA_PIN5 44
#define DATA_PIN6 43
#define DATA_PIN7 42
#define LCD_COL 16
#define LCD_ROW 2
//pin for temperature
#define TEMP_ADDR 72
//pin for led
#define LED_ADDR 36

//define the pin for joystick
#define JOY_X 68
#define JOY_Y 69

//Global variable
volatile byte state = 0;	//the global variable for the door, default close
volatile byte newComing = 0;//used for door opened state , changed in interrupt
volatile byte newComing2 = 0;//used for door closing state, changed in interrupt

byte doorOperation = 0;//state for door operation

//the global variable for analog value of position
int xCenterPos = 0;
int yCenterPos = 0;
int xyTH = 100;

//Menulevel 0: main interface
//					10: sub interface
//mainMenuOption 11: A/C mode
//							 12: Door Lock mode
//							 13: Settings
//menuChange 1: option 1
//menuChange 2: option 2
//menuChange 3: option 3
int menuChange = 0;//option for each level of menu
int menuLevel = 0;//level for menu
int mainMenuOption = 0;//options for mainMenuOption

//variable for connectting device 
int connectSelected = 0;

//instance for different modules
Temperature temper;
LED led;
Switch pushSwitch;
Server server(80);
StepMotor stepMotor;
RgbLcd lcd;

//string used for communication command parsing
String str;

//credentials for wifi router
char passphrase[] = "11223344";
char ssid[] = "KIT406";

//for scrolling the display of connect device
int positionCounter = 0;

//State of different modules
byte ACMode = 0;//colse
byte DoorLockMode = 0;//close
byte DoorLockModeCommand = 1;//if the door is not closed, temp variable to store the command from interface 

//random 4 digit number for communication
long pin = 0;

//used for display information for connectting device option
int once = 0;

//Global variable for non blocking time
const unsigned long interval = 1000; //1 second
unsigned long previousMillisForInterface = 0;//for interface
unsigned long previousMillis = 0;//for door
unsigned long passedClosingTime = 0;//for door reopen when closing

void setup(){
	//Initialize the stepMotor
	stepMotor.begin(DIR_PIN, STEP_PIN, MS1_PIN, MS2_PIN, SLP_PIN);
	stepMotor.setStep(HALF_STEP);
	// attach interrupt 0 (so pin number is 2 should be wired)
	attachInterrupt(0, interrupt, RISING);
	//Initialize the WIFI
	WiFly.begin();

	temper.begin(TEMP_ADDR);//initialise temperature module
	led.begin(LED_ADDR);//initialise LED module
	pushSwitch.begin(); //initialise Switch module

	// initialise LCD module and set up the LCD's number of columns and rows
	lcd.begin(RS_PIN, RW_PIN, E_PIN, R_PIN, G_PIN, B_PIN,
						DATA_PIN4, DATA_PIN5, DATA_PIN6, DATA_PIN7,
						LCD_COL, LCD_ROW);
	lcd.onBacklightGreen();
	
	//Connect to wifi
	if (!WiFly.join(ssid, passphrase)) {
		while (1) {
		// Hang on failure.
		}
	}
	Serial.begin(9600);
	//output the IP address of the kit
	Serial.print("IP : ");
	Serial.println(WiFly.ip());
	//start the server
	server.begin();
	str.reserve(256);

	//set pinmode for x and y axis
	pinMode(JOY_X, INPUT);
	pinMode(JOY_Y, INPUT);
	//pinMode(JOY_SEL, INPUT);

	//initial position of x and y
	xCenterPos = analogRead(JOY_X);
	yCenterPos = analogRead(JOY_Y);

	lcd.print("  Main Interface");
	delay(1000);
	
}
	
void loop(){
	unsigned long currentMillis = millis();
	if(currentMillis - previousMillisForInterface > 400){
		previousMillisForInterface=currentMillis;
		//function: Communication
		if(connectSelected == 1){//in connecting device state for web interface
			communication();		
		}
		else if(connectSelected == 0){// in rgbLcd interface
			navigateOption();
		}
		//change the interface
		interfaceDisplay();
		//Select the option
		selectOption();
	
		AirConditioner();// A/C working	
	}
	if(DoorLockMode == 1){//door is unlocked(enabled)
		AutoDoor(currentMillis);
	}//end of if(DoorLockMode == 1)
}
/****************
Function: interrupt
Para:			void
Purpose:	ISR for PIR using interrupt 0: pin 2
*****************/
void interrupt()
{
	if(DoorLockMode == 1){
		if(state == 0){//first person coming
				state = 1;
				Serial.println("interrupted");
		}
		else if(state == 2){//door opened
			if(newComing == 0){
				newComing = 1;
			}
		}
		else if(state == 3){//door closing
			if(newComing2 == 0){
				newComing2 = 1;
			}
		}
	}
}
/****************
Function: Autodoor
Para:			unsigned long currentMillis
Purpose:	Excecute the door function according to assignment requirement
*****************/
void AutoDoor(unsigned long currentMillis){
		if(state == 0){//no one coming, door colsed
			if(DoorLockModeCommand == 1){//Lock door command from interface, only when state = 0
				DoorLockMode = 0;
			}
			previousMillis = currentMillis;
		}
		else if(state == 1){//Someone coming due to the change in the interrupt for PIR	
			if(currentMillis - previousMillis > 5*interval){//opened already 5 seconds
				previousMillis = currentMillis;
				stepMotor.off();//colse the step motor
				state = 2;//door opened
				doorOperation = 0;//no door moving
			}
			else if(doorOperation == 0){//Open the door one time
				stepMotor.setDirection(1);
				stepMotor.on(1);
				doorOperation = 1;//door Operating: Opening
			}
		}
		else if(state == 2){//people going through
			if(currentMillis - previousMillis > 5*interval){//5 seconds passed
				previousMillis = currentMillis;
				stepMotor.setDirection(0);//change direction for closing
				stepMotor.on(1);
				state = 3;//door closing
				doorOperation = 1;//door Operating: Closing
			}
			else{
				if(newComing == 1){//interrupt happpened due to PIR
					previousMillis = currentMillis;//update previousMillis so that another 5 seconds bigins 
					newComing = 0;
				}
			}
		}
		else if(state == 3){//door closing
			if(currentMillis - previousMillis > 5*interval){//no new person coming in 5 seconds, door colsed
				previousMillis = currentMillis;
				state = 0;// waiting for another person coming
				stepMotor.off();
				doorOperation = 0;//door closed
			}
			else{
				if(newComing2 == 1){//new person coming when the door is closing
					//passed time after closing the door
					passedClosingTime = currentMillis-previousMillis;	
					previousMillis = currentMillis;										
					stepMotor.setDirection(1);
					//newComing2 = 0;
					state = 4;
				}			
			}
		}
		else if(state == 4){//partially open 
			if(currentMillis - previousMillis > passedClosingTime){//no new person coming in 5 seconds, door colsed
					previousMillis = currentMillis;
					state = 2;//state changed to open for people going through
					stepMotor.off();
					newComing2 = 0;
			}
		}
}
/****************
Function: communication
Para:			none
Purpose:	parsing command from 10.0.0.1/KIT406
*****************/
void communication(){
		Serial.println("waiting...");
		Client client = server.available(); // check whether the client connects
		if(client)
		{
			if(client.connected()){
				str="";
				if(client.available()){
					// send a standard http response header
					client.println("HTTP/1.1 200 OK");
					client.println("Content-Type: text/html");
					client.println();
					// if there are incoming bytes available
					// from the server, read and store them:
					while(client.available())
					{
						char c = client.read();
						str += c;
					}
					if(str.substring(0,10) == "GET /?com="){//begining
						if(str.substring(10,13) == "PIN"){
							if((str.substring(13,17)).toInt() == pin){
								client.println("AK_PINOK");
							}
							else{
								client.println("AK_FAILURE");
							}
						}
						else if(str.substring(10,14) == "ACON"){
							ACMode = 1;//AirConditionMode
						}
						else if(str.substring(10,15) == "ACOFF"){
							ACMode = 0;//AirConditionMode
						}
						else if(str.substring(10,18) == "DOORLOCK"){
							DoorLockModeCommand = 1;//temp varialbe for Door lock command
						}
						else if(str.substring(10,20) == "DOORUNLOCK"){
							if(DoorLockMode == 0){
								DoorLockMode = 1;
								DoorLockModeCommand = 0;
								state = 0;	
							}
						}
					}
				}//END OF client.available
			}//end of client.connected
			//give the browser time
			client.stop();
		}//end of if(client)
}
/****************
Function: navigateOption
Para:			none
Purpose:	navigate the option of every level
*****************/
void navigateOption(){
	if(analogRead(JOY_Y) < yCenterPos - xyTH) {//move down
		if(mainMenuOption == 13){//Setting option have only two suboption
			if(menuChange <= 1){
				menuChange++;
			}
			else{
				menuChange = 0;//move to setting
			}
		}
		else{
			if(menuChange <= 2){
				menuChange++;
			}
			else{
				menuChange = 0;
			}
		}
	} 
	else if(analogRead(JOY_Y) > yCenterPos + xyTH) {//move up
		if(menuChange >= 1){
			menuChange--;
		}
		else{
			if(mainMenuOption == 13){//Setting option have only two suboption
				menuChange = 2;//move to setting
			}
			else
				menuChange = 3;
		}				
	}
}
/****************
Function: AirConditioner
Para:			none
Purpose:	control airconditioner
*****************/
void AirConditioner(){
	if(ACMode == 1){//open airconditioner
		if(state == 0){//door closed
			if(temper.getTemperatureC() > 26){
				led.AllOn();
			}
			else{//temperature <= 26
				led.AllOff();
			}
		}
		else{//door opened
			led.AllOff();
		}
	}
	else{//close airconditioner
		led.AllOff();
	}
}
/****************
Function: selectOption
Para:			none
Purpose:	select the option when navigate to some option, and change some varialbe according to current option
*****************/
//
void selectOption(){
	uint8_t pushKey = pushSwitch.getPushKey();
	if(pushKey > 0) {
		if(menuLevel == 0){
			if(menuChange == 1){
				menuLevel = 10;
				mainMenuOption = 11;
				menuChange = 0;
			}
			else if(menuChange == 2){
				menuLevel = 10;
				mainMenuOption = 12;
				menuChange = 0;				
			}
			else if(menuChange == 3){
				menuLevel = 10;
				mainMenuOption = 13;
				menuChange = 0;				
			}
		}
		else if(menuLevel = 10){
			if(mainMenuOption == 11){
				if(menuChange == 1){
					ACMode = 1;
				}
				else if(menuChange == 2){
					ACMode = 0;
				}
				else if(menuChange == 3){
					menuLevel = 0;
					mainMenuOption = 0;
					menuChange = 0;
				}
			}
			if(mainMenuOption == 12){
				if(menuChange == 1){
					DoorLockModeCommand = 1;
					/*if(DoorLockMode == 1){
						if(state == 0)
						DoorLockMode = 0;
					}*/
				}
				else if(menuChange == 2){
					//DoorLockModeCommand = 0;
					if(DoorLockMode == 0)
					{
						DoorLockMode = 1;//UNlock
						DoorLockModeCommand = 0;
						state = 0;
					}	
				}
				else if(menuChange == 3){
					menuLevel = 0;
					mainMenuOption = 0;
					menuChange = 0;					
				}				
			}
			if(mainMenuOption == 13){
				if(menuChange == 1){//connect device option in AC mode selected by joystick
					if(connectSelected == 0)//action for the key push. after first push, generate pin for display
					{
						connectSelected = 1;
						randomSeed(analogRead(0));
						pin = random(1000, 10000);
						Serial.println(pin);
						once = 1;
					}
					else{//key have been pushed for connecting device, here disconnect the device and set the interface back for connectting device next time
						connectSelected = 0;
						positionCounter = 0;
					}
				}
				else if(menuChange == 2){
					menuLevel = 0;
					mainMenuOption = 0;
					menuChange = 0;					
				}				
			}
		}
	}
}
/****************
Function: interfaceDisplay
Para:			none
Purpose:	display the interface according to current option navigation
*****************/
void interfaceDisplay(){
	if(menuLevel == 0){
		lcd.clear(); 
		//lcd.print("Main Interface"); // print the string assigned in the setup()
		//lcd.setCursor(0,1); // set the cursor in the second row of the RGBLCD
		if(menuChange == 0){
			lcd.print("->Main Interface");
			lcd.setCursor(0,1);
			lcd.print("  A/C Mode");
		}
		else if(menuChange == 1){
			lcd.print("->A/C Mode");
			lcd.setCursor(0,1);
			lcd.print("  Door Lock Mode");
		}
		else if(menuChange == 2){
			lcd.print("->Door Lock Mode");
			lcd.setCursor(0,1);
			lcd.print("  Settings");				
		}
		else if(menuChange == 3){
			lcd.print("->Settings");
			lcd.setCursor(0,1);
			lcd.print("  Main Interface");
		}
	}
	else if(menuLevel == 10){
		if(mainMenuOption == 11){
			lcd.clear(); 
			//lcd.print("A/C Mode"); // print the string assigned in the setup()
			//lcd.setCursor(0,1); // set the cursor in the second row of the RGBLCD
			if(menuChange == 0){
				lcd.print("->A/C Mode");
				lcd.setCursor(0,1);
				lcd.print("  ON");
			}
			else if(menuChange == 1){
				lcd.print("->ON");
				lcd.setCursor(0,1);
				lcd.print("  OFF");					
			}
			else if(menuChange == 2){
				lcd.print("->OFF");
				lcd.setCursor(0,1);
				lcd.print("  Back");					
			}
			else if(menuChange == 3){
				lcd.print("->Back");
				lcd.setCursor(0,1);
				lcd.print("  A/C Mode");					
			}	
		}
		else if(mainMenuOption == 12){
			lcd.clear(); 
			//lcd.print("Door Lock Mode"); // print the string assigned in the setup()
			//lcd.setCursor(0,1); // set the cursor in the second row of the RGBLCD

			if(menuChange == 0){
				lcd.print("->Door Lock Mode");
				lcd.setCursor(0,1);
				lcd.print("  LOCK");
			}
			else if(menuChange == 1){
				lcd.print("->LOCK");
				lcd.setCursor(0,1);
				lcd.print("  UNLOCK");
			}
			else if(menuChange == 2){
				lcd.print("->UNLOCK");
				lcd.setCursor(0,1);
				lcd.print("  Back");
			}
			else if(menuChange == 3){
				lcd.print("->Back");
				lcd.setCursor(0,1);
				lcd.print("  Door Lock Mode");
			}
		}
		else if(mainMenuOption == 13){
			//lcd.print("Settings"); // print the string assigned in the setup()
			//lcd.setCursor(0,1); // set the cursor in the second row of the RGBLCD
			if(menuChange == 0){
				lcd.clear(); 
				lcd.print("->Settings");
				lcd.setCursor(0,1);
				lcd.print("  Connect Device");
			}
			else if(menuChange == 1){ 

				if(connectSelected == 0){
					lcd.clear();
					lcd.print("->Connect Device");
					lcd.setCursor(0,1);
					lcd.print("  Back");
				}
				else if(connectSelected == 1){
					if(once == 1)
					{
						lcd.clear(); 
						lcd.print("PIN: ");
						lcd.print(pin);
						lcd.print(" / IP: ");
						lcd.print(WiFly.ip());
						lcd.setCursor(0,1);
						lcd.print("Go to http://10.0.0.1/KIT406");//28
						once = 0;
					}
					positionCounter++;
					if(positionCounter <= 14){
						lcd.scrollDisplayLeft();
					}
					else {
						lcd.scrollDisplayRight();
						if(positionCounter == 28)
							positionCounter = 0;
					}
				}					
			}
			else if(menuChange == 2){
				lcd.clear(); 
				lcd.print("->Back");
				lcd.setCursor(0,1);
				lcd.print("  Settings");
			}
		}	
	}
}