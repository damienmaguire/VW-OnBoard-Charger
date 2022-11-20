/*
VW PHEV Charger test code V1
Special thanks to https://github.com/crasbe for figuring out the VW CRC.

Copyright 2022 
Damien Maguire
*/

#include <due_can.h>  
#include <due_wire.h> 
#include <DueTimer.h>  
#include <Metro.h>

#define SerialDEBUG SerialUSB
 template<class T> inline Print &operator <<(Print &obj, T arg) { obj.print(arg); return obj; } //Allow streaming


CAN_FRAME outframe;  //A structured variable according to due_can library for transmitting CAN data.
CAN_FRAME inFrame;    //structure to keep inbound inFrames


byte vag_cnt=0x80;
byte vag_cnt097=0x00;
byte msg097=0x09;
byte vag_cnt3A6=0x00;
byte vag_cnt3AF=0x00;
byte vag_cnt489=0x00;
byte vag_cnt124=0x00;
byte vag_cnt6A3=0x00;
byte vag_cnt6A4=0x00;
byte msg187_1=0x00;
byte msg187_2=0x00;
byte time20MS=2;
byte time500MS=5;
byte chg_amps=0x00;
float Version=1.00;
uint16_t BMSAmps=0;

struct ChargerStatus {
  uint16_t ACvoltage = 0;
  uint16_t HVVoltage = 0;
  uint16_t MaxHV = 0;
  int8_t temperature = 0;
  uint8_t mode = 0;
  uint8_t current=0;
  uint8_t targetcurrent=0;
  uint8_t MaxACAmps=0;
  uint8_t ISetPnt=0;
  uint8_t PPLim=0;
} charger_status;

struct ChargerControl {
  uint16_t HVDCSetpnt=0;
  uint16_t IDCSetpnt = 0;
  uint8_t modeSet = 0;
  bool active=0;
} charger_params;

int led = 13;         //onboard led for diagnosis

Metro timer_10ms=Metro(10); 
Metro timer_100ms=Metro(100); 

void setup() 
  {
  Can0.begin(CAN_BPS_500K);   // Charger CAN
  Can1.begin(CAN_BPS_500K);   // External CAN
  Can0.watchFor();
  Can1.watchFor();
  Serial.begin(115200);  
  pinMode(led, OUTPUT);
  }

  
  
void loop()
{ 
checkCAN();
checkforinput();
if(timer_10ms.check()) Msgs10ms();
if(timer_100ms.check()) Msgs100ms();
}





void Msgs10ms()                       //10ms messages here
{
        outframe.id = 0x097;            //0x097 on Hyb CAN is from BMS. 10ms.
        outframe.length = 8;            //
        outframe.extended = 0;          //
        outframe.rtr=1;                 //
        BMSAmps=(charger_status.current+511)*4;
        //BMSAmps=(3+511)*4;
        outframe.data.bytes[0]=0x00;
        outframe.data.bytes[1]=((BMSAmps&0xf)<<4|vag_cnt097);//BMS reported current. 0xc7f=0A.
        outframe.data.bytes[2]=(BMSAmps>>4)&0xff;//BMS reported current. 0xc7f=0A.
        outframe.data.bytes[3]=0xCF;//BMS Reported voltage.0x5cf=371V.12bits unsigned.
                                    //BMS Reported voltage. BMS mode. 0=HV inactive , 1=Drive hv active, 2=balancing, 3=external charging, 4=AC charge,5=Error, 6=DC Charge, 7=Init.
        if(charger_params.active)
        {
          if(msg097==0) outframe.data.bytes[4]=0x45;//hv up for AC charge
          else outframe.data.bytes[4]=0x15;//hv up for drive
          if(msg097!=0) msg097--;
        }
        
        if(!charger_params.active)
        {
          outframe.data.bytes[4]=0x05;
          msg097=0x09;
        } 
        outframe.data.bytes[5]=0x03;
        outframe.data.bytes[6]=0x00;
        outframe.data.bytes[7]=0x00;
        outframe.data.bytes[0]=vw_crc_calc(outframe.data.bytes, outframe.length, outframe.id);
        Can0.sendFrame(outframe);    //send on pt can
            vag_cnt097++;
        if(vag_cnt097>0x0F) vag_cnt097=0x00;

      if(time20MS==0)
      {
        outframe.id = 0x124;            //0x124 on hyb can at 20ms
        outframe.length = 8;            //BMS reported max charge and drive currents
        outframe.extended = 0;          //
        outframe.rtr=1;                 //
        outframe.data.bytes[0]=0x00;
        outframe.data.bytes[1]=vag_cnt124;
        outframe.data.bytes[2]=0x98;//we dont worry about the drive current just leave at 330A.
        outframe.data.bytes[3]=0x4F;
        outframe.data.bytes[4]=0x4A;
        outframe.data.bytes[5]=((charger_params.IDCSetpnt&0x3F)<<2)|0x01;
        outframe.data.bytes[6]=0x00;
        outframe.data.bytes[7]=0x5D;
        outframe.data.bytes[0]=vw_crc_calc(outframe.data.bytes, outframe.length, outframe.id);
        Can0.sendFrame(outframe);    //send on pt can
            vag_cnt124++;
        if(vag_cnt124>0x0F) vag_cnt124=0x00;
        time20MS=2;
      }
      time20MS--;
//
//


}
    



void Msgs100ms()                      ////100ms messages here
{
        digitalWrite(13,!digitalRead(13));//blink led every time we fire this interrrupt.
        outframe.id = 0x187;            // PT CAN. EV gearshift msg from vag dbc. 
        outframe.length = 8;            //
        outframe.extended = 0;          //
        outframe.rtr=1;                 //
        outframe.data.bytes[0]=0x00;
        if(charger_params.active)
        {
          outframe.data.bytes[1]=0x80 | vag_cnt;//switches to 8(counter) from 0(counter) when start of charge requested.
          if(msg187_2!=0) msg187_2--;
          if(msg187_2==0)
          {
            outframe.data.bytes[7]=msg187_1;//ramp here for some reason??
            if(msg187_1<0xfd)msg187_1++;
          }
          
        }
        if(!charger_params.active)
        {
          msg187_1=0;
          msg187_2=6;
          outframe.data.bytes[1]=vag_cnt;
          outframe.data.bytes[7]=msg187_1;//ramp here for some reason??
        }
        outframe.data.bytes[2]=0x12;
        outframe.data.bytes[3]=0x00;
        outframe.data.bytes[4]=0x00;
        outframe.data.bytes[5]=0x00;
        outframe.data.bytes[6]=0x30;
        
        outframe.data.bytes[0]=vw_crc_calc(outframe.data.bytes, outframe.length, outframe.id);
        Can0.sendFrame(outframe);    //send on pt can
            vag_cnt++;
        if(vag_cnt>0x0f) vag_cnt=0x00;


         outframe.id = 0x3A6;            //Command message to charger.
        outframe.length = 8;            //
        outframe.extended = 0;          //
        outframe.rtr=1;                 //
        outframe.data.bytes[0]=0x00;
        outframe.data.bytes[1]=vag_cnt3A6;
        if(charger_params.active) outframe.data.bytes[2]=0x41;//unknown change from 10 to 40 then 41 when on charge.
        if(!charger_params.active) outframe.data.bytes[2]=0x10;
        outframe.data.bytes[3]=0x00;
                                      //Target mode in bits 2,3,4. 0=Standby, 1=AC charge, 3=DC charge, 7=init. so in this case 0x04=AC Charge.0x1c=init.goes from 0 to 4.
        if(charger_params.active) outframe.data.bytes[4]=0x04;
        if(!charger_params.active) outframe.data.bytes[4]=0x00;                            
        outframe.data.bytes[5]=0x01;
        outframe.data.bytes[6]=0x00;
        outframe.data.bytes[7]=0x00;
        outframe.data.bytes[0]=vw_crc_calc(outframe.data.bytes, outframe.length, outframe.id);
        Can0.sendFrame(outframe);    //
            vag_cnt3A6++;
        if(vag_cnt3A6>0x0f) vag_cnt3A6=0x00;    

        outframe.id = 0x3AF;            //this msg is important
        outframe.length = 8;            //
        outframe.extended = 0;          //DCDC converter 12v voltage.
        outframe.rtr=1;                 //
        outframe.data.bytes[0]=0x00;
        outframe.data.bytes[1]=(0x6<<4|vag_cnt3AF);
        outframe.data.bytes[2]=0x09;
        outframe.data.bytes[3]=0x21;
        outframe.data.bytes[4]=0xD8;
        outframe.data.bytes[5]=0xB1;
        outframe.data.bytes[6]=0x00;
        outframe.data.bytes[7]=0x00;
        outframe.data.bytes[0]=vw_crc_calc(outframe.data.bytes, outframe.length, outframe.id);
        Can0.sendFrame(outframe);    //
            vag_cnt3AF++;
        if(vag_cnt3AF>0x0f) vag_cnt3AF=0x00; 

        outframe.id = 0x489;            //BMS MSG. When used will cause cp to lock.
        outframe.length = 8;            //Will try as a static for now...
        outframe.extended = 0;          //
        outframe.rtr=1;                 //
        outframe.data.bytes[0]=0x00;
        outframe.data.bytes[1]=vag_cnt489;
        outframe.data.bytes[2]=0x00;
        outframe.data.bytes[3]=0xF4;
        outframe.data.bytes[4]=0x25;
        outframe.data.bytes[5]=0xCE;
        outframe.data.bytes[6]=0x2F;
        outframe.data.bytes[7]=0x00;
        outframe.data.bytes[0]=vw_crc_calc(outframe.data.bytes, outframe.length, outframe.id);
        Can0.sendFrame(outframe);    //
            vag_cnt489++;
        if(vag_cnt489>0x0f) vag_cnt489=0x00;

      if(time500MS==0)
      {
        outframe.id = 0x6A3;            
        outframe.length = 8;            //
        outframe.extended = 0;          //Max DC charge voltage. Min start charge voltage
        outframe.rtr=1;                 //
        outframe.data.bytes[0]=0x00;
        outframe.data.bytes[1]=((charger_params.HVDCSetpnt&0xF)<<4)|vag_cnt6A3;
        outframe.data.bytes[2]=(charger_params.HVDCSetpnt>>4)&0x3F;
        outframe.data.bytes[3]=0x3E;
        outframe.data.bytes[4]=0x6C;
        outframe.data.bytes[5]=0x66;
        outframe.data.bytes[6]=0x03;
        outframe.data.bytes[7]=0x00;
        outframe.data.bytes[0]=vw_crc_calc(outframe.data.bytes, outframe.length, outframe.id);
        Can0.sendFrame(outframe);    //
            vag_cnt6A3++;
        if(vag_cnt6A3>0x0f) vag_cnt6A3=0x00;

        outframe.id = 0x6A4;            
        outframe.length = 8;            //
        outframe.extended = 0;          //
        outframe.rtr=1;                 //
        outframe.data.bytes[0]=0x00;
        outframe.data.bytes[1]=vag_cnt6A4;
        outframe.data.bytes[2]=0x42;
        outframe.data.bytes[3]=0x3F;
        outframe.data.bytes[4]=0x43;
        outframe.data.bytes[5]=0x03;
        outframe.data.bytes[6]=0x00;
        outframe.data.bytes[7]=0x00;
        outframe.data.bytes[0]=vw_crc_calc(outframe.data.bytes, outframe.length, outframe.id);
        Can0.sendFrame(outframe);    //
            vag_cnt6A4++;
        if(vag_cnt6A4>0x0f) vag_cnt6A4=0x00;


        time500MS=5; 
      }
      time500MS--;
// 0x3A6, 0x3AF on hyb can at 100ms
//0x6A3 , 0x6A4 on Hyb can at 500ms



}






void checkCAN()
{
//msgs from charger :
//PT CAN :0x594. Bytes 1 and 3 look to contain currents. Byte 5 may be AC volts.
//0x12DD5472 , 0x17331102 , 0x17332510 0x17F00044 , 0x1B000044
//0x17331102 nothing interesting.possible temps?
//0x17332510 lots of spikey data on all bytes
//0x17F00044 nothing interesting.possible temps?
//0x1B000044 some movement on byte 2.

//HYB CAN :0x38B , 0x564 , 0x565 , 0x566 0x67E , 0x12DD5472 , 0x1A555457 , 0x1A555490 , 0x1A555491
//0x38B contains AC voltages and currents.
//0x564 charger mode,AC voltage,DC voltage,DC current,charger temperature,charger power loss.
//0x565 Charger max current (taken from cable lim / pwm lim on hybrid model) also charger target mode.
//0x566 Target current, Max HV voltage (taken from bms msg)
//0x67E Charge port info.
//0x1A555457 noisy msg. no notable ramps.
//0x1A555490 the quiet msg. no ramps. few small steps.
//0x1A555491 as above.




  if(Can0.available())
  {
    Can0.read(inFrame);

    if(inFrame.id ==0x564)
    {
      charger_status.ACvoltage=(inFrame.data.bytes[2]*2);//charger AC voltage is 9 bits from bit 15 to 23 (byte 1 bit 7 and all of byte 2)
      charger_status.HVVoltage=((inFrame.data.bytes[4]<<8) | inFrame.data.bytes[3])&0x3ff;//HVDC volts 10 bit from bit 24 to 33. Byte 3 and bits 0 and 1 of byte 4.
      charger_status.temperature=(inFrame.data.bytes[6]-40);//temp in byte 6 40 offset.
      charger_status.current=(((((inFrame.data.bytes[5]<<8) | inFrame.data.bytes[4])>>2)&0x3ff)*0.2)-102;//10 bit bytes 4 and 5.
      charger_status.mode=((inFrame.data.bytes[1])>>4)&0x07;//mode in byte 1. Bits 12-14. 0=standby,1=AC charging,3=DC charging,4=Precharge,5=Fail,7=init.
    }

    if(inFrame.id ==0x565)
    {
      charger_status.MaxACAmps=((inFrame.data.bytes[3])&0x7f)*0.5;
    }

    if(inFrame.id ==0x566)
    {
      charger_status.MaxHV=(((inFrame.data.bytes[6]<<8) | inFrame.data.bytes[5])>>4)&0x3ff;
      charger_status.ISetPnt=(((((inFrame.data.bytes[5]<<8) | inFrame.data.bytes[4])>>2)&0x3ff)*0.2)-102;
    }

    if(inFrame.id ==0x67E)
    {
      charger_status.PPLim=(inFrame.data.bytes[4])&0x07;//0=13A,1=20A,2=32A,3=63A,6=init,7=failed.
    }

  }

  
}

void checkforinput()
{ 
  //Checks for keyboard input from Native port 
   if (SerialDEBUG.available()) 
     {
      int inByte = SerialDEBUG.read();
      switch (inByte)
         {
     
          case 'd':     //Print data received from charger
                PrintRawData();
            break;
            
          case 'D':     //Print charger set points
                PrintSetting();
            break;
          
          case 'i':     //Set max HV Current
              SetHVI();
            break;
          case 'q':     //Set Max HV voltage
              SetHVV();
            break;
          case 'a':     //Activate charger     
              ActivateCharger();
            break;            
           
          case 's':     //Deactivate charger     
              DeactivateCharger();
            break;            
           

          case '?':     //Print a menu describing these functions
              printMenu();
            break;
         
          }    
      }
}

void SetHVI()
{
  SerialDEBUG.println("");
   SerialDEBUG.print("Configured HV Current setpoint: ");
     if (SerialDEBUG.available()) {
    charger_params.IDCSetpnt = SerialDEBUG.parseInt();
  }
  if(charger_params.IDCSetpnt>50) charger_params.IDCSetpnt=50;//limit to max 50amps
  SerialDEBUG.println(charger_params.IDCSetpnt);
}

void SetHVV()
{
  SerialDEBUG.println("");
  SerialDEBUG.print("Configured HV Voltage setpoint: ");
     if (SerialDEBUG.available()) {
    charger_params.HVDCSetpnt = SerialDEBUG.parseInt();
  }
  if(charger_params.HVDCSetpnt>500) charger_params.HVDCSetpnt=500;//limit to max 500volts
  SerialDEBUG.println(charger_params.HVDCSetpnt);
}

void ActivateCharger()
{   
    charger_params.active=1;
    charger_params.modeSet=1;
    //Timer3.attachInterrupt(Msgs10ms).start(10000); // Can sending ints
    //Timer4.attachInterrupt(Msgs100ms).start(100000);
    SerialDEBUG.println("Charger activated");
}

void DeactivateCharger()
{
    charger_params.active=0;
    charger_params.modeSet=0;
    //Timer3.detachInterrupt();
    //Timer4.detachInterrupt();
    SerialDEBUG.println("Charger deactivated");
}


void printMenu()
{
   SerialDEBUG<<"\f\n=========== EVBMW VW Charger Controller "<<Version<<" ==============\n************ List of Available Commands ************\n\n";
   SerialDEBUG<<"  ?  - Print this menu\n ";
   SerialDEBUG<<"  d - Print recieved data from charger\n";
   SerialDEBUG<<"  D - Print configuration data\n";
   SerialDEBUG<<"  i  - Set max HV Current e.g. typing i5 followed by enter sets max current to 5Amps\n ";
   SerialDEBUG<<"  q  - Set max HV Voltage e.g. typing q400 followed by enter sets max HV Voltage to 400Volts\n ";
   SerialDEBUG<<"  a  - Activate charger.\n ";
   SerialDEBUG<<"  s  - Deactivate charger.\n ";   
   SerialDEBUG<<"**************************************************************\n==============================================================\n\n";
   
}

//////////////////////////////////////////////////////////////////////////////

void PrintRawData()
{
  SerialDEBUG.println("");
  SerialDEBUG.println("***************************************************************************************************");
  SerialDEBUG.print("ACVolts: ");
  SerialDEBUG.println(charger_status.ACvoltage);
  SerialDEBUG.print("HVDCVolts: ");
  SerialDEBUG.println(charger_status.HVVoltage);
  SerialDEBUG.print("Charger Temperature: ");
  SerialDEBUG.println(charger_status.temperature);
  SerialDEBUG.print("Current charger mode: ");//0=standby,1=AC charging,3=DC charging,4=Precharge,5=Fail,7=init.
  if(charger_status.mode==0) SerialDEBUG.println("Standby");
  if(charger_status.mode==1) SerialDEBUG.println("AC Charging");
  if(charger_status.mode==3) SerialDEBUG.println("DC Charging");
  if(charger_status.mode==4) SerialDEBUG.println("Precharge");
  if(charger_status.mode==5) SerialDEBUG.println("Failed");
  if(charger_status.mode==7) SerialDEBUG.println("Init");
  SerialDEBUG.print("Max available AC current: ");
  SerialDEBUG.println(charger_status.MaxACAmps);
  SerialDEBUG.print("Max HV Voltage : ");
  SerialDEBUG.println(charger_status.MaxHV); 
  SerialDEBUG.print("HV I: ");
  SerialDEBUG.println(charger_status.ISetPnt); 
  SerialDEBUG.println("***************************************************************************************************");  
}


void PrintSetting()
{
  SerialDEBUG.println("");
  SerialDEBUG.println("***************************************************************************************************");
  SerialDEBUG.print("DC Volts Setpoint: ");
  SerialDEBUG.println(charger_params.HVDCSetpnt);
  SerialDEBUG.print("DC Current Setpoint: ");
  SerialDEBUG.println(charger_params.IDCSetpnt);
  SerialDEBUG.print("Charger Target mode: ");
  SerialDEBUG.println(charger_params.modeSet); 
  SerialDEBUG.println("***************************************************************************************************");  
}

uint8_t vw_crc_calc(uint8_t* inputBytes, uint8_t length, uint16_t address)
{
    
    const uint8_t poly = 0x2F;
    const uint8_t xor_output = 0xFF;
    // VAG Magic Bytes
    const uint8_t MB0097[16] = { 0x3C, 0x54, 0xCF, 0xA3, 0x81, 0x93, 0x0B, 0xC7, 0x3E, 0xDF, 0x1C, 0xB0, 0xA7, 0x25, 0xD3, 0xD8 };
    const uint8_t MB0124[16] = { 0x12, 0x7E, 0x34, 0x16, 0x25, 0x8F, 0x8E, 0x35, 0xBA, 0x7F, 0xEA, 0x59, 0x4C, 0xF0, 0x88, 0x15 };
    const uint8_t MB0187[16] = { 0x7F, 0xED, 0x17, 0xC2, 0x7C, 0xEB, 0x44, 0x21, 0x01, 0xFA, 0xDB, 0x15, 0x4A, 0x6B, 0x23, 0x05 };
    const uint8_t MB03A6[16] = { 0xB6, 0x1C, 0xC1, 0x23, 0x6D, 0x8B, 0x0C, 0x51, 0x38, 0x32, 0x24, 0xA8, 0x3F, 0x3A, 0xA4, 0x02 };
    const uint8_t MB03AF[16] = { 0x94, 0x6A, 0xB5, 0x38, 0x8A, 0xB4, 0xAB, 0x27, 0xCB, 0x22, 0x88, 0xEF, 0xA3, 0xE1, 0xD0, 0xBB };
    const uint8_t MB0489[16] = { 0x7A, 0x13, 0xF4, 0xE5, 0xC9, 0x07, 0x21, 0xDD, 0x6F, 0x94, 0x63, 0x9B, 0xD2, 0x93, 0x42, 0x33 };
    const uint8_t MB06A3[16] = { 0xC1, 0x8B, 0x38, 0xA8, 0xA4, 0x27, 0xEB, 0xC8, 0xEF, 0x05, 0x9A, 0xBB, 0x39, 0xF7, 0x80, 0xA7 };
    const uint8_t MB06A4[16] = { 0xC7, 0xD8, 0xF1, 0xC4, 0xE3, 0x5E, 0x9A, 0xE2, 0xA1, 0xCB, 0x02, 0x4F, 0x57, 0x4E, 0x8E, 0xE4 };

    uint8_t crc = 0xFF;
    uint8_t magicByte = 0x00;
    uint8_t counter = inputBytes[1] & 0x0F; // only the low byte of the couner is relevant

    switch (address) 
    {
    case 0x0097: // ??
        magicByte = MB0097[counter];
        break;
    case 0x0124: // ??
        magicByte = MB0124[counter];
        break;
    case 0x0187: // EV_Gearshift "Gear" selection data for EVs with no gearbox
        magicByte = MB0187[counter];
        break;
    case 0x03A6: // ??
        magicByte = MB03A6[counter];
        break;
    case 0x03AF: // ??
        magicByte = MB03AF[counter];
        break;
    case 0x0489: // ??
        magicByte = MB0489[counter];
        break;        
    case 0x06A3: // ??
        magicByte = MB06A3[counter];
        break;
    case 0x06A4: // ??
        magicByte = MB06A4[counter];
        break;
    default: // this won't lead to correct CRC checksums
        magicByte = 0x00;
        break;
    }

    for (uint8_t i = 1; i < length + 1; i++)
    {
        // We skip the empty CRC position and start at the timer
        // The last element is the VAG magic byte for address 0x187 depending on the counter value.
        if (i < length)
            crc ^= inputBytes[i];
        else
            crc ^= magicByte;

        for (uint8_t j = 0; j < 8; j++)
        {
            if (crc & 0x80)
                crc = (crc << 1) ^ poly;
            else
                crc = (crc << 1);
        }
    }

    crc ^= xor_output;

    inputBytes[0] = crc; // set the CRC checksum directly in the output bytes
    return crc;
}
