/* 
 Procedures:
 1.   Turn rotary encoder. Has a soft limit of 0 - degreeLimit (inclusive, default is 90 degrees).
 2.   After spinning the rotary encoder one direction, if the limit switch is hit, the motor can only be spun the opposite direction until the limit switch is released.
 3.   All menus requiring number inputs can have numbers up to 999.
 4.   Press '*' to submit input in any menu
 5.   Inside every menu '#' can be pressed to return to the main menu, including rotary turns.
 6.   'A' can be pressed to save specific steps. blockLimit is the maximum amount of saved locations in a list
 7.   'C' can be pressed to load a saved list of saved locations
 */

// Includes
#include <Encoder.h>
#include <StepperMotor.h>
#include <Keypad.h>
#include <EEPROM.h>

// User Variables
const degreeLimit  = 90;                        // Available degrees of movement
const clickLimit   = degreeLimit / 8.1818;      // MAX amount of turns
const motorSpeed   = 2;                         // Speed of the motor during normal operation. Smaller # = faster. 2 is the max speed
const cycleSpeed   = 7;                         // Speed of the motor during cycles.           Smaller # = faster. 2 is the max speed
const stepsPerTurn = 25;                        // Sensitivity of motor
const blockLimit   = 5;                         // Amount of steps available in a chain of user define instructions. 1024 (max) / # of unique lists of instructions = blockLimit
boolean loadDelay  = false;                     // Optional delay between movements in lists
boolean softLimit  = false;                     // Soft limit of motors; True = Numbers > clickLimit are rejected

// Pin Positions
StepperMotor stepper(8, 9, 10, 11);             // Stepper pins
Encoder knob(2,3);                              // Rotary Encoder pins
int limitSwitchPin = 4;                         // Limit Switch pins
int rotaryPin = A4;                             // Pin used for the push-in button on the rotary encoder
int led = 5;                                    // Pin for an LED
const byte numRows = 4;                         // Number of rows on the keypad
const byte numCols = 4;                         // Number of columns on the keypad
byte rowPins[numRows] = {                       // Keypad row pins
  6, 7, 12, 13};
byte colPins[numCols]= {                        // Keypad column pins
  A3, A2, A1, A0};

// Variables
// Rotary Encoder
int knobPosition   = -999;
int positionRight  = -999;
int directionGoing = 0;                         // Keep track of last movement

// Motor
int totalTurn  = 0;                             // Assign the amount of steps for the motor to move 
int countSteps = 0;                             // Keep track of what step the motor is on

// Keypad
boolean comingFromSave = false;
char keyPressed;                                // Generic variable to check last key pressed
int numPressed[3] = {                           // Holds numbers before converting them into a single integer
  NULL
};
int passedInput = 000;                          // The integer passed from each function
int countkeyPressed = 0;                        // Keep track of the number of keys pressed
char keymap[numRows][numCols]=                  // Keypad layout
{      // LEFT SIDE
  {
	'1', '4', '7', '*' 
  }
  ,
  {
	'2', '5', '8', '0' 
  }
  ,
  {
	'3', '6', '9', '#' 
  }
  ,
  {
	'A', 'B', 'C', 'D'
  }
  // RIGHT SIDE
};
Keypad keypad= Keypad(makeKeymap(keymap), rowPins, colPins, numRows, numCols);    // Instantiate the keypad

void setup() {
  Serial.begin (9600);                 // Setup serial for debugging
  stepper.setPeriod(motorSpeed);       // Set the normal operation speed.
  directionGoing = 1;

  pinMode(limitSwitchPin, INPUT_PULLUP);
  digitalWrite(limitSwitchPin, HIGH);   

  pinMode(rotaryPin, INPUT_PULLUP);
  digitalWrite(rotaryPin, HIGH); 
  pinMode(led, OUTPUT);


  // Startup sequence
  // The motor will spin CCW until the limit switch is hit, that way 0 will be in the same location every time
  Serial.println("Startup sequence");
  stepper.reset();
  stepper.stop(false);
  while (digitalRead(limitSwitchPin)) {   // Run until limit switch is hit
	stepper.move(10);
  }
  stepper.stop();
  Serial.println("Motor is now home\n");
  instruction();
}

void collectNumbers() {
  while (countkeyPressed < 3) {           // Input 3 numbers
	keyPressed = keypad.waitForKey();
	if (keyPressed == '#') {              // # = Exit to menu
	  break;
	}
	else if (keyPressed == '*') {         // * = Enter
	  if (countkeyPressed < 1) {
		Serial.println("Must input a number, try again.");
	  }
	  else {
		break;
	  }
	}
	else if (keyPressed == 'B' && comingFromSave) {
	  break;
	}
	else if (keyPressed == 'A' || keyPressed == 'B' || keyPressed == 'C' || keyPressed == 'D') {    // Only numbers are allowed
	  Serial.println("Must be a number, try again.");
	  countkeyPressed = 0;
	} 
	else {     // Add numbers to the appropriate element of numPressed
	  numPressed[countkeyPressed] = keyPressed;
	  countkeyPressed++;
	  Serial.print(keyPressed);
	}
  }
}

void convert() {       // Convert char keypad input to usable int data type
  passedInput = 0;
  for (int i = 0; i < 3; i++) {           // Converts individual elements from char to int
	if (numPressed[i] != 0)
	  numPressed[i] = numPressed[i] - 48;
  }
  for (int i = 0; i < 3; i++) {           // Converts to a usable number with correct digits
	if (i == 0) {
	  if (numPressed[i] == 0) {
		continue;
	  }
	  passedInput = passedInput + numPressed[i] * 100;    // Correct 100's place
	}
	else if (i == 1) {
	  if (numPressed[i] == 0) {
		continue;
	  }
	  passedInput = passedInput + numPressed[i] * 10;    // Correct 10's place
	}
	else if (i == 2 ) {
	  if (numPressed[i] == 0) {
		continue;
	  }
	  passedInput = passedInput + numPressed[i];    // Correct 1's place
	}
  }
  if (countkeyPressed == 1) {
	passedInput = passedInput / 100;
  }
  if (countkeyPressed == 2) {
	passedInput = passedInput / 10;
  }
  countkeyPressed = 0;
  for (int i = 0; i < 3; i++) {
	numPressed[i] = NULL;
  }
}

void saveMovements() {     // Save a list of instructions; Max amount of instructions = blockLimit
  int directionsSaved = 0;
  int saveLocation;
  comingFromSave = true;
  Serial.print("Saving movements on key: ");    // Saves to user define key(s)
  collectNumbers();
  if (keyPressed != '#') {
	convert();
	countkeyPressed = 0;
	saveLocation = blockLimit * passedInput;               // Ex: saved key: 50; saveLocation(250) = blockLimit(5) * passedInput(50)
	Serial.print("\n~~Enter up to ");
	Serial.print(blockLimit);
	Serial.println(" steps to move to, \n~~or press '#' on the final location,\n~~or press 'B' to save the current position.");
	while (directionsSaved < blockLimit) {
	  if (keyPressed != '#') {
		if (directionsSaved > 0) {                                // Make the serial look clean
		  Serial.print("\tAvailable directions left: ");    // First time through
		}
		else{
		  Serial.print("Available directions left: ");      // Second time through
		}
		Serial.println(blockLimit - directionsSaved);     // Instructions left
		collectNumbers();
		if (keyPressed != 'B') {
		  convert();
		}
		else {
		  passedInput = countSteps;
		}
	  }
	  if (keyPressed == '#') {      // Insert NULL for the remaining directions and return to menu
		Serial.print("Saving direction: ");
		Serial.print(directionsSaved+1);
		Serial.println(" with a NULL value");
		EEPROM.write(saveLocation,164);
		directionsSaved++;
		saveLocation++;
	  }
	  else {                        // Save the number inputted
		Serial.print(" Saving step: ");
		Serial.print(passedInput);           
		EEPROM.write(saveLocation, passedInput);    //  Save the key and step number under a permanent solution, only reset when overwritten
		directionsSaved++;
		saveLocation++;  
	  }
	}
  }
  directionsSaved = 0;                            // Reset used variables
  Serial.println("\nExiting saving mode.\n");
  countkeyPressed = 0;
  keyPressed = NULL;
  comingFromSave = false;
}

void loadMovements() {        // Load a previously saved list of instruction
  int temp = 0;
  int saveLocation;
  Serial.println("\n~~Enter the key you would like to load");
  collectNumbers();
  convert();
  Serial.println("");
  countkeyPressed = 0;
  saveLocation = blockLimit * passedInput;       // Ex: saved key: 50; saveLocation(250) = blockLimit(5) * passedInput(50)
  for (int i = saveLocation; i < saveLocation + blockLimit; i++) {
	if (EEPROM.read(saveLocation) == 164) {      // 164 = a NULL memory location; do nothing
	  Serial.println("No save data on that key");
	  break;
	}
	temp = EEPROM.read(i);        // Read in from memory
	if (temp != 164) {            // If not null, move to the saved locations
	  loadStep(temp);
	  if (loadDelay) {
		keypad.waitForKey();
	  }
	}
  }
}

void instruction() {               // A text menu to explain to the user
  Serial.println("\n~~Press a number then '*' to go to submited input\n~~Press '#' to reset input\n~~Press 'A' to save steps\n~~Press 'C' to load a save\n~~Press 'D' to cycle between steps\n~~Spin the dial to manually go to a step\nKeys Pressed: ");
}

void CycleMovement() {            // Swivles between two user define points
  int first, second;
  boolean cycleDirection = 0;     // Keep track of direction
startCycle:                     // Tag for goto
  Serial.println("Press in the knob to exit out of cycling\n~~Enter the first number");
  collectNumbers();
  if (keyPressed != '#') {
	convert();
	countkeyPressed = 0;
	first = passedInput;
	Serial.print("\nFirst Value: ");
	Serial.println(first);
	Serial.println("~~Enter the second number");
	collectNumbers();
	if (keyPressed != '#') {
	  convert();
	  countkeyPressed = 0;
	  second = passedInput;
	  Serial.print("\nSecond Value: ");
	  Serial.println(second);
	  if (first == second || ((first > clickLimit || second > clickLimit) && softLimit))
	  {
		Serial.println("Numbers must be different and less than the soft limit.");
		goto startCycle;             // Restart input
	  }
	  Serial.println("Entering cycle");
	  stepper.setPeriod(motorSpeed);
	  loadStep(first);               // Move to the first location
	  stepper.setPeriod(cycleSpeed); // Set a special speed
	  while (digitalRead(rotaryPin)) {              // Cycle between location1 and location2
		if (cycleDirection == false) {
		  loadStep(second);
		  cycleDirection = !cycleDirection;   // Swap direction
		}
		else {
		  loadStep(first);
		  cycleDirection = !cycleDirection;   // Swap direction
		}
	  }
	  Serial.println("Exiting cycle.");
	  stepper.setPeriod(motorSpeed);          // Return to the normal speed
	}
  }
}

void keypadInput(char keyPressed) {       // Controls logic flow for each button press
  if (keyPressed == 'A' || keyPressed == 'B' || keyPressed == 'C' || keyPressed == 'D' || keyPressed == '*' || keyPressed == '#') {
	if (countkeyPressed >= 1 && keyPressed != '#' && keyPressed != '*') {     // Checks for letters pressed after numbers
	  Serial.print(keyPressed);
	  Serial.println("\nMust be a number, try again.");
	  countkeyPressed = 0;
	  keyPressed = NULL;
	  instruction();
	}
	switch (keyPressed) {
	case 'A':                                       // Save a list of user set instructions
	  Serial.println('A');
	  saveMovements();
	  instruction();
	  break;

	case 'C':                                       // Load a previously saved list of instructions
	  Serial.println('C');
	  loadMovements();
	  instruction();
	  break;

	case 'D':                                       // Swivle movements
	  Serial.println('D');
	  CycleMovement();
	  instruction();
	  break;

	case '#':
	  Serial.println("#");
	  Serial.println("\nInput was reset\n");        // Press '#' to reset input
	  countkeyPressed = 0;
	  instruction();
	  break;

	case '*':
	  if (countkeyPressed > 0) {                    // Press '*' to submit inp
		Serial.println("\nSubmitting Input");
		convert();
		Serial.println(passedInput);
		loadStep(passedInput);
	  }
	  instruction();
	  break;
	default:                                        // Required default case
	  break;
	}
  }

  else if (countkeyPressed < 3) {      // Collects number input
	numPressed[countkeyPressed] = keyPressed;
	countkeyPressed++;
	Serial.print(keyPressed);
  }
}

void loadStep(int passedInput) {                          // Single step movement
  if (keyPressed == '#') {
  }
  else if (passedInput <= clickLimit || !softLimit) {     // Limit for the keypad; to turn on: change softLimit to true

	stepper.reset();
	totalTurn = stepsPerTurn*(countSteps - passedInput);  // Go to that specific step, not just move that many steps
	countSteps = passedInput;                             // Update countSteps
	moveMotor();
  }
  else {
	Serial.println("Input is over the limit");            // Warn user
  }
}

void CWturn() {
  stepper.reset();
  totalTurn = stepsPerTurn;                // Steps per turn
  countSteps--;                            // Decrement steps
  directionGoing = 1;                      // CW was last turned
  moveMotor();
}

void CCWturn() {
  stepper.reset();
  totalTurn = stepsPerTurn*-1;             // Steps per turn
  countSteps++;                            // Increment steps
  directionGoing = 0;                      // CCW was last turned
  moveMotor();
}

void moveMotor() {
  stepper.reset();
  stepper.stop(false);
  stepper.move(totalTurn);                 // Rotate
  stepper.stop();                          // Stop after turn
  Serial.println("STEP NUMBER: ");         // Debug
  Serial.println(countSteps);
  totalTurn = 0;                           // Reset steps
  stepper.reset();
}

void loop() {
  passedInput = 000;
  int newKnob;
  newKnob = knob.read()/-4;                           // Read in encoder values less sensitive

  if (countSteps == 0 || countSteps >= clickLimit) {  // If the steps hit the limit, turn on LED
	digitalWrite(led, HIGH);
  }

  else {                                              // Turn off LED
	digitalWrite(led, LOW);
  }

  if (!digitalRead(limitSwitchPin)) {                 // Limit switch hit stops movement then allows to opposite direction 1 step
	stepper.stop(true);
	if (directionGoing == 1 && newKnob-knobPosition < 0 && countSteps < clickLimit) {        // Previous turn = CW, Current turn = CCW
	  CCWturn();
	  directionGoing = !directionGoing;
	}
	else if (directionGoing == 0 && newKnob-knobPosition > 0 && countSteps > 0) {           // Previous turn = CCW, Current turn = CW
	  CWturn();
	  directionGoing = !directionGoing;
	}
  }

  else if (newKnob-knobPosition > 0 && countSteps > 0 ) {                         // CW Rotation
	if (countSteps != 0)
	  CWturn();
  }

  else if (newKnob-knobPosition < 0 && countSteps < clickLimit) {                 // CCW Rotation
	if (countSteps != clickLimit)
	  CCWturn();
  }

  if (newKnob != knobPosition) {                                                // Update position
	knobPosition = newKnob;
  }

  keyPressed = keypad.getKey();                   // Update keypad buttons pressed
  if (keyPressed != NO_KEY) {                       // If button pressed
	keypadInput(keyPressed);
  }
}
