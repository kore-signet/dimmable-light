#include "dimmable_light.h"

#include "hw_timer.h"

struct PinBri{
  uint8_t pin;
  uint8_t delay;
};

/**
 * Temporary struct to provide the interrupt a memory concurrent-safe
 */
static struct PinBri pinBri[DimmableLight::N];

/**
 * Number of lights already managed in the current semi-wave
 */
static uint8_t lightManaged=0;
/**
 * This function manage the triac/dimmer in a single semi-period (that is 10ms @50Hz)
 * This function will be called multiple times per semi-period (in case of multi 
 * lamps with different at least a different brightness value).
 */
void zero_cross_int(){
  // This is kind of optimization software, but not electrical:
  // This avoid to wait 10micros in a interrupt or setting interrupt 
  // to turn off the PIN (this last solution could be evaluated...)
  for(int i=0;i<DimmableLight::nLights;i++){
    digitalWrite(pinBri[i].pin,LOW);
  }

  // Update the structures, if needed
  if(DimmableLight::newBrightness && !DimmableLight::updatingStruct){
  	DimmableLight::newBrightness=false;
  	for(int i=0;i<DimmableLight::nLights;i++){
  		pinBri[i].pin=DimmableLight::lights[i]->pin;
  		pinBri[i].delay=DimmableLight::lights[i]->brightness;
  	}
  }

  lightManaged = 0;
  
  // This for is intended for full brightness
  for(int lightManaged=0;pinBri[lightManaged].delay<150;lightManaged++){
  	digitalWrite(pinBri[lightManaged].pin,HIGH);
  }
  
  hw_timer_arm(pinBri[lightManaged].delay);  
}

/**
 * Timer routine to turn on one or more lights
 */
void activateLights(){
	// Alternative way to manage the pin, it should become low after the triac started
	//delayMicroseconds(10);
	//digitalWrite(AC_LOADS[phase],LOW);

	// This condition means:
	// trigger immediately is there is not time to active the timer for the next light (i.e delay differene less than 150microseconds)
	for(; (lightManaged<DimmableLight::nLights-1 && pinBri[lightManaged].delay<pinBri[lightManaged+1].delay+150); lightManaged++){
		digitalWrite(pinBri[lightManaged].pin, HIGH);
		// Turn on the following light, it this really similar to the previous... 
		//Some lights could be turned on 2 times, but of course it hasn't any effect
		digitalWrite(pinBri[lightManaged+1].pin, HIGH);
	}

	digitalWrite(pinBri[lightManaged].pin, HIGH);
	lightManaged++;

	if(lightManaged<DimmableLight::nLights){
	hw_timer_arm(pinBri[lightManaged].delay-pinBri[lightManaged-1].delay);
	}
}

void DimmableLight::begin(){
	pinMode(digitalPinToInterrupt(syncPin), INPUT);
	attachInterrupt(digitalPinToInterrupt(syncPin), zero_cross_int, RISING);

	// FRC1 is a low priority timer, it can't interrupt other ISR
	hw_timer_init(FRC1_SOURCE, 0);
	hw_timer_set_func(activateLights);
}

void DimmableLight::setBrightness(uint8_t newBri){
  	brightness=10000-(int)(newBri)*10000/255;

	// reorder the array to speed up the interrupt.
	// Thi mini-algorithm works on a different memory area wrt the interrupt,
	// so it is concurrent-safe code

  	updatingStruct=true;
    // Array example, iit is always ordered
    // [45,678,5000,7500,9000]
    if(newBri>brightness){
    	bool done=false;
    	for(int i=posIntoArray+1;i<N && !done;i++){
    		if(newBri<lights[i]->brightness){
    			// perform the exchange between posIntoArray
    			DimmableLight *temp=lights[posIntoArray];
    			lights[posIntoArray]=lights[i];
    			lights[i]=temp;
    			// update posinto array
    			lights[posIntoArray]->posIntoArray=i;
    			// this->posIntoArray=posIntoArray
    			lights[i]->posIntoArray=posIntoArray;
    			done=true;
    		}
    	}
    }else if(newBri>brightness){
    	bool done=false;
    	for(int i=posIntoArray-1;i>0  && !done;i--){
    		if(newBri>lights[i]->brightness){
    			// perform the exchange between posIntoArray
    			DimmableLight *temp=lights[posIntoArray];
    			lights[posIntoArray]=lights[i];
    			lights[i]=temp;
    			// update posinto array
    			lights[posIntoArray]->posIntoArray=i;
    			// this->posIntoArray=posIntoArray
    			lights[i]->posIntoArray=posIntoArray;
    			done=true;
    		}
    	}
    }else{
    	Serial.println("No need to perform the exchange, the brightness is the same!");
    }
    updatingStruct=false;
    
	Serial.println(String("Brightness (in ms to wait): ") + brightness);
}

DimmableLight::DimmableLight(int pin)
								:pin(pin),brightness(0){
	if(nLights<N-1){
		updatingStruct=true;
		
		posIntoArray=nLights;
		nLights++;
		lights[posIntoArray]=this;
		
		pinBri[updatingStruct].pin;
		pinBri[updatingStruct].delay=10000;
		
		updatingStruct=false;
	}else{
		// return error or exception
	}
}

DimmableLight::~DimmableLight(){
	// Recompact the array
	updatingStruct=true;
	nLights--;
	//remove the light from the static pinBri array
	Serial.println("I should implement the array shrinking");
	updatingStruct=false;
}

uint8_t DimmableLight::nLights = 0;
DimmableLight* DimmableLight::lights[DimmableLight::N] = {nullptr};
bool DimmableLight::newBrightness = false;
bool DimmableLight::updatingStruct = false;
uint8_t DimmableLight::syncPin = D7;