/* ===================================================================================
Arduino Firmware for:
Flood/Water Detection
Laser Detection
Trip Wire Detection

No RTOS was used.
Real-time Concurrency was achieved using TTHread
Original code w/ the license: 
http://www.microchipc.com/sourcecode/tiny_threads/tthread.h

Notes:
Modified TThread to be able to accomodate 65K lines

Author: Jeff

License: CC BY 4.0
=================================================================================== */

// -+-+-+-+-+-+-+-+
// -|T|T|h|r|e|a|d|
// -+-+-+-+-+-+-+-+

#define TT_DEF(fn)              \
unsigned short tt_var_##fn=0;   \
void tt_##fn(void)

#define TT_BEGIN(fn)            \
    switch (tt_var_##fn)     {  \
        case 0:                 \
            do {} while(0)

#define TT_END(fn)  }           \
    tt_var_##fn = 0xFFFF

#define TT_KILL(fn) \
    tt_var_##fn = 0xFFFF

#define TT_INIT(fn) \
    tt_var_##fn = 0

#define TT_WAIT_WHILE(fn, condition)                                \
    tt_var_##fn = ((__LINE__ % (65534))+1); case ((__LINE__ % (65534))+1):  \
    if ((condition)) return

#define TT_WAIT_UNTIL(fn, condition)                                \
    tt_var_##fn = ((__LINE__ % (65534))+1); case ((__LINE__ % (65534))+1):  \
    if (!(condition)) return

#define TT_SCHED(fn) \
    if (tt_var_##fn!=0xFFFF) tt_##fn()

#define TT_SWITCH(fn) \
    tt_var_##fn = ((__LINE__ % (65534))+1); return; case ((__LINE__ % (65534))+1): \
    do {} while(0)

#define TT_IS_RUNNING(fn) (tt_var_##fn != 0xFFFF)


// -+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
// -|I|n|c|l|u|d|e|s|-|D|e|f|i|n|e|s|-|D|e|c|l|a|r|a|t|i|o|n|s|
// -+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+

#include <avr/wdt.h>
#include <TimerObject.h>


#define STR_TITLE  "Intrusion Detection System by Jeff v00.80.02"

HardwareSerial *mHardwareSerial;

#define DEBUG_MSG(stringChar)  mHardwareSerial->println(stringChar);
//#define DEBUG_MSG(stringChar)  ;

#define POWER_LED_PIN       13

// Alarm
#define FLOOD_SENSOR_IN     3
//#define FLOOD_SENSOR_ALARM_OUT    12

// Theft
#define LASER_IN        7
#define LASER_CONTROL_OUT   10
#define LASER_CONTROL_OUT_INV   9
#define GENERAL_ALARM_OUT   8

#define LASER_DIAGNOSTIC_MODE_IN    2

#define HUMAN_HEAT_DETECTION    4

//#define DEFAULT_ALARM_IN_SECS 5 // 5 seconds alarm
#define DEFAULT_ALARM_IN_SECS (60*3) // 3 minutes


// -+-+-+-+-+-+-+-+-+-+-+-+
// -|T|i|m|e|r|-|S|e|t|u|p|
// -+-+-+-+-+-+-+-+-+-+-+-+

#define TIMER_TICK_MSEC     1

TimerObject *timer1 = new TimerObject(TIMER_TICK_MSEC);

volatile unsigned int ledTimerTickCtr, floodSensorTimerTick, laserAlarmTick, generalAlarmTick;

bool targetTimeCheck(u16 timeTarget, u16 tickCtr)
    {
    return (tickCtr * TIMER_TICK_MSEC) >= timeTarget;
    }

void TimerTickCallBack()
    {
    ledTimerTickCtr++;
    floodSensorTimerTick++;
    laserAlarmTick++;
    generalAlarmTick++;
    }

// -+-+-+-+-+-+-+-+-+
// -|L|E|D|-|T|a|s|k|
// -+-+-+-+-+-+-+-+-+

void SetPowerLed(int val)
    {
    digitalWrite(POWER_LED_PIN, val);
    }

TT_DEF(LED_TASK)
    {
    TT_BEGIN(LED_TASK);
    DEBUG_MSG("BEGIN LED TASK");
    while (1)
        {
        wdt_reset();
        SetPowerLed(HIGH); // turn the LED on (HIGH is the voltage level)
        ledTimerTickCtr = 0;
        TT_WAIT_UNTIL(LED_TASK, targetTimeCheck(250, ledTimerTickCtr));
        SetPowerLed(LOW); // turn the LED off by making the voltage LOW
        ledTimerTickCtr = 0;
        TT_WAIT_UNTIL(LED_TASK, targetTimeCheck(1500, ledTimerTickCtr));
        }
    TT_END(LED_TASK);
    }


// -+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
// -|A|b|s|t|r|a|c|t|-|A|l|a|r|m|-|T|r|i|g|g|e|r|
// -+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+

class ABaseAlarmTrigger
    {
    public:
    virtual void SetAlarmTrigger() =0;
    virtual bool IsAlarmTriggerInProgress()=0;
    virtual void SetGeneralAlarmOut(int val)=0;
    virtual void KillAlarmTrigger() = 0;
    virtual void Setup()=0;
    };

// -+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
// -|A|l|a|r|m|-|T|r|i|g|g|e|r|-|C|l|a|s|s|
// -+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+

class AlarmTriggerv1: public ABaseAlarmTrigger
    {
    private:
    volatile bool bAlarmInProgress = false;

    public:

    void Setup()
        {
        pinMode(GENERAL_ALARM_OUT, OUTPUT);
        }

    void SetAlarmTrigger()
        {
        bAlarmInProgress=true;
        }

    bool IsAlarmTriggerInProgress()
        {
        return bAlarmInProgress;
        }

    void SetGeneralAlarmOut(int val)
        {
        digitalWrite(GENERAL_ALARM_OUT, val);
        }

    void KillAlarmTrigger()
        {
        bAlarmInProgress=false;
        }
    };


// -+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
// -|A|l|a|r|m|-|N|o|-|O|p|e|r|a|t|i|o|n|
// -+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+

class DummyAlarmTriggerNoOp: public ABaseAlarmTrigger
    {
    public:

    void Setup()
        {
        }

    void SetAlarmTrigger()
        {
        }

    bool IsAlarmTriggerInProgress()
        {
        return false;
        }

    void SetGeneralAlarmOut(int val)
        {
        }

    void KillAlarmTrigger()
        {
        }

    };

static AlarmTriggerv1 alarmTrig = AlarmTriggerv1();
static DummyAlarmTriggerNoOp dummyalarmTrig = DummyAlarmTriggerNoOp();
static ABaseAlarmTrigger *mABaseAlarmTrigger=&alarmTrig;
//static ABaseAlarmTrigger *mABaseAlarmTrigger=&dummyalarmTrig;

// -+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
// -|A|l|a|r|m|-|T|r|i|g|g|e|r|-|T|a|s|k|
// -+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+

TT_DEF(ALARM_TRIGGER)
    {
    TT_BEGIN(ALARM_TRIGGER);

    digitalWrite(GENERAL_ALARM_OUT, LOW); // turn the LED off by making the voltage LOW
    DEBUG_MSG("BEGIN ALARM TRIGGER TASK");

    while (1)
    {
    if(mABaseAlarmTrigger->IsAlarmTriggerInProgress())
        {
        static u16 alarmCount;

        DEBUG_MSG("General Alarm Triggered");

        for(alarmCount =0 ; alarmCount < DEFAULT_ALARM_IN_SECS ; alarmCount++)
        {
        mABaseAlarmTrigger->SetGeneralAlarmOut(HIGH);
        generalAlarmTick = 0;
        TT_WAIT_UNTIL(ALARM_TRIGGER,
            targetTimeCheck(250, generalAlarmTick));
        mABaseAlarmTrigger->SetGeneralAlarmOut(LOW);
        generalAlarmTick = 0;
        TT_WAIT_UNTIL(ALARM_TRIGGER,
            targetTimeCheck(750, generalAlarmTick));
        //DEBUG_MSG("Laser Alarm Triggered End" + String(alarmCount, DEC)   );
        }

        DEBUG_MSG("Laser Alarm Exit" + String(alarmCount, DEC)   );
        //bAlarmInProgress = false; // kill the alarm
        mABaseAlarmTrigger->KillAlarmTrigger();
        }

    TT_SWITCH(ALARM_TRIGGER);
    }
    TT_END(ALARM_TRIGGER);
    }


// -+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
// -|A|b|s|t|r|a|c|t|-|L|o|g|i|c|-|B|a|s|e|d|-|S|e|n|s|i|n|g|
// -+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+


class ABaseLogicBasedSensing
    {
    public:
    virtual bool IsDetected()=0;
    virtual String GetStringTrigger()=0;
    virtual String IntroString()=0;
    };


// -+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
// -|F|l|o|o|d|-|S|e|n|s|i|n|g|-|C|l|a|s|s|
// -+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+

class FloodSensingv1:public ABaseLogicBasedSensing
    {
    public:
    bool IsDetected()
        {
        return (digitalRead(FLOOD_SENSOR_IN) == HIGH);
        }

    String GetStringTrigger()
        {
        return "Flood Sensor Alarm Triggered";
        }

    String IntroString()
        {
        return "I am Flood Sensingv1";
        }
    };



// -+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
// -|H|e|a|t|-|D|e|t|e|c|t|i|o|n|-|C|l|a|s|s|
// -+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+

// This is used for PIR detection
class HeatDetectionv1:public ABaseLogicBasedSensing
    {
    public:
    bool IsDetected()
        {
        return (digitalRead(HUMAN_HEAT_DETECTION) == HIGH);
        }

    String GetStringTrigger()
        {
        return "Human Heat Alarm Triggered";
        }

    String IntroString()
        {
        return "I am HeatDetectionv1";
        }
    };


// -+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
// -|D|u|m|m|y|-|D|e|t|e|c|t|i|o|n|-|C|l|a|s|s|
// -+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+

class DummyDetection:public ABaseLogicBasedSensing
    {
    public:
    bool IsDetected()
        {
        return false;
        }

    String GetStringTrigger()
        {
        return "Dummy Detection";
        }

    String IntroString()
        {
        return "I am Dummy Detector";
        }

    };

FloodSensingv1 floodSense=FloodSensingv1();
HeatDetectionv1 heatDetection=HeatDetectionv1();
DummyDetection dummyDetection=DummyDetection();


#define LOGIC_BASED_SENSING_NUM_ELEM    2
ABaseLogicBasedSensing *mABaseLogicBasedSensing[]={&floodSense,&heatDetection};
//ABaseLogicBasedSensing *mABaseLogicBasedSensing[]={&dummyDetection,&dummyDetection};


// -+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
// -|L|o|g|i|c|-|B|a|s|e|d|-|T|a|s|k|
// -+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+

TT_DEF(LOGIC_BASED_TASK)
    {
    TT_BEGIN(LOGIC_BASED_TASK);
    static int ctr;
    DEBUG_MSG("BEGIN FLOOD TASK");

    for(ctr=0;ctr<LOGIC_BASED_SENSING_NUM_ELEM;ctr++)
        {
        DEBUG_MSG(mABaseLogicBasedSensing[ctr]->IntroString());
        }

    while (1)
    {

    for(ctr=0;ctr<LOGIC_BASED_SENSING_NUM_ELEM;ctr++)
        {
        if(mABaseLogicBasedSensing[ctr]->IsDetected())
        {
        DEBUG_MSG(mABaseLogicBasedSensing[ctr]->GetStringTrigger());
        mABaseAlarmTrigger->SetAlarmTrigger();
        }
        }


    if(mABaseAlarmTrigger->IsAlarmTriggerInProgress())
        {
        TT_WAIT_WHILE(LOGIC_BASED_TASK, mABaseAlarmTrigger->IsAlarmTriggerInProgress());
        }

    floodSensorTimerTick = 0;
    TT_WAIT_UNTIL(LOGIC_BASED_TASK, targetTimeCheck(500,floodSensorTimerTick) );
    }
    TT_END(LOGIC_BASED_TASK);
    }


// -+-+-+-+-+-+-+-+-+-+-+-+
// -|L|a|s|e|r|-|A|l|a|r|m|
// -+-+-+-+-+-+-+-+-+-+-+-+


//#define LASER_ELEM_NUM  4
//const int LASER_INPUT_ARR[] = {4,5,6,7};

#define LASER_ELEM_NUM  1
const int LASER_INPUT_ARR[] = {7};


class ABaseLaserDetection
    {
public:
    //virtual void toggleLaserOut(int val)=0;

    virtual void toggleLaserOn()=0;
    virtual void toggleLaserOff()=0;

    virtual bool isLaserDetected(int expected)=0;
    virtual void setup()=0;
    virtual bool shouldEnterDiagMode()=0;

    virtual void enableLaserDetectionDebugMsg()=0;
    };


// -+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
// -|L|a|s|e|r|-|A|n|a|l|o|g|-|D|e|t|e|c|t|i|o|n|
// -+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+


#define OPTO_AN_IN  A3


class LaserDetectionAnalogV1:public ABaseLaserDetection
    {
private:
    bool mDebugMessageEnabled = false;


public:
    void toggleLaserOut(int val)
        {
        digitalWrite(LASER_CONTROL_OUT, val);
        digitalWrite(LASER_CONTROL_OUT_INV, (val==HIGH)?LOW:HIGH  );
        }

    void enableLaserDetectionDebugMsg()
        {
        mDebugMessageEnabled=true;
        }


    void toggleLaserOn()
        {
        toggleLaserOut(LOW);
        }

    void toggleLaserOff()
        {
        toggleLaserOut(HIGH);
        }


    bool isLaserDetected(int expected)
        {
        int val;
        val = analogRead(OPTO_AN_IN);

        if(mDebugMessageEnabled)
            DEBUG_MSG("AnalogRead: " + String(val, DEC));

        if(val<350)
            {
            return true;
            }
        return false;
        }

    void setup()
        {
        int ctr;
        pinMode(LASER_CONTROL_OUT_INV, OUTPUT);
        pinMode(LASER_CONTROL_OUT, OUTPUT);


        pinMode(OPTO_AN_IN,INPUT);

        pinMode(LASER_DIAGNOSTIC_MODE_IN, INPUT_PULLUP);

        }

    bool shouldEnterDiagMode()
        {
        return digitalRead(LASER_DIAGNOSTIC_MODE_IN)==LOW;
        }
    };

const int NOT_IMPT = -1;

//LaserDetectionV1 laserDetectionV1 = LaserDetectionV1();
LaserDetectionAnalogV1 laserDetectionAnalogV1 = LaserDetectionAnalogV1();
//ABaseLaserDetection *mABaseLaserDetection = &laserDetectionV1;
ABaseLaserDetection *mABaseLaserDetection = &laserDetectionAnalogV1;

// -+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
// -|L|a|s|e|r|-|A|l|a|r|m|-|T|a|s|k|
// -+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+

TT_DEF(LASER_ALARM_TASK)
    {
    static bool bTriggerAlarm;
    static u8 laserDetectCount;

    TT_BEGIN(LASER_ALARM_TASK);
    DEBUG_MSG("BEGIN LASER TASK");

    while (1)
    {
    //DEBUG_MSG("BEGIN LASER SENSE");
    bTriggerAlarm = false;
    laserDetectCount = 0;

    // turn off laser
    mABaseLaserDetection->toggleLaserOff();

    laserAlarmTick = 0;
    TT_WAIT_UNTIL(LASER_ALARM_TASK, targetTimeCheck(40, laserAlarmTick));

    if (mABaseLaserDetection->isLaserDetected(NOT_IMPT)) // if not laser detected trigger alarm
        {
        DEBUG_MSG("I Expect you to be all high");
        bTriggerAlarm = true;
        goto ALARM_PROCESS;
        }

    // turn on laser
    mABaseLaserDetection->toggleLaserOn();

    laserAlarmTick = 0;

    while(true)
        {
        if (mABaseLaserDetection->isLaserDetected(NOT_IMPT))
            {
            break;
            }

        if(laserAlarmTick>20) // 20 msec passed
            {
            DEBUG_MSG("laserDetectCount>n");
            bTriggerAlarm = true;
            goto ALARM_PROCESS; // Note: use GOTO sensibly and sparingly
            }

        TT_SWITCH(LASER_ALARM_TASK);
        }

    ALARM_PROCESS:

    mABaseLaserDetection->toggleLaserOff();
    if (bTriggerAlarm)
        {
        DEBUG_MSG("Laser Sensor Alarm Triggered");
        mABaseAlarmTrigger->SetAlarmTrigger();
        }

    if(mABaseAlarmTrigger->IsAlarmTriggerInProgress())
        {
        TT_WAIT_WHILE(LASER_ALARM_TASK, mABaseAlarmTrigger->IsAlarmTriggerInProgress());
        }

    }
    TT_END(LASER_ALARM_TASK);
    }



// -+-+-+-+-+-+
// -|S|e|t|u|p|
// -+-+-+-+-+-+

bool bEnterDiagnosticMode=false;

// the setup function runs once when you press reset or power the board
void setup()
    {
    static int ctr;

    mHardwareSerial = &Serial;
    mHardwareSerial->begin(115200);

    DEBUG_MSG(STR_TITLE);

    wdt_disable();
    wdt_enable (WDTO_4S);
    // wdt reset was placed under Power Led Task

    timer1->setOnTimer(&TimerTickCallBack);
    timer1->Start();


    // we make sure all pin is set to input pull up
    for(ctr=2;ctr<=13;ctr++)
        {
        pinMode(ctr, INPUT_PULLUP);
        }

    pinMode(A0, INPUT_PULLUP);
    pinMode(A1, INPUT_PULLUP);
    pinMode(A2, INPUT_PULLUP);
    pinMode(A3, INPUT_PULLUP);
    pinMode(A4, INPUT_PULLUP);
    pinMode(A5, INPUT_PULLUP);
    pinMode(A6, INPUT_PULLUP);
    pinMode(A7, INPUT_PULLUP);


    // power
    pinMode(POWER_LED_PIN, OUTPUT);

    // laser
    mABaseLaserDetection->setup();

    mABaseAlarmTrigger->Setup();

    delay(500);

    bEnterDiagnosticMode=mABaseLaserDetection->shouldEnterDiagMode();
    }

// -+-+-+-+-+-+-+-+-+-+-+-+-+-+
// -|I|n|f|i|n|i|t|e|-|L|o|o|p|
// -+-+-+-+-+-+-+-+-+-+-+-+-+-+

void loop()
    {
    if(bEnterDiagnosticMode)
        {
        mABaseAlarmTrigger->SetGeneralAlarmOut(HIGH);
        delay(500);
        mABaseAlarmTrigger->SetGeneralAlarmOut(LOW);

        mABaseLaserDetection->enableLaserDetectionDebugMsg();
        mABaseLaserDetection->toggleLaserOn();

        while(1)
            {
            wdt_reset();
            if(mABaseLaserDetection->isLaserDetected(LOW))
                {
                DEBUG_MSG("LASER DETECTED");
                SetPowerLed(HIGH);
                }
            else
                {
                DEBUG_MSG("LASER NOT DETECTED");
                SetPowerLed(LOW);
                }
            delay(250);
            }
        }
    TT_SCHED(LED_TASK);
    TT_SCHED(LOGIC_BASED_TASK);
    TT_SCHED(LASER_ALARM_TASK);
    TT_SCHED(ALARM_TRIGGER);
    timer1->Update();
    }


// END OF FILE
