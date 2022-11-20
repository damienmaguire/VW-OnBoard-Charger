# VW-OnBoard-Charger
Hacking and reverse engineering of the VW Onboard AC chargers to permit use in EV Conversion projects.

Charger #1 from a 2021 Seat Mii part number : 5QE915681BP

Data connector has +12v, GND, PT CAN, HYB CAN and two HVIL loop wires.

Two CAN busses running at 500kbps.

Sending 0x570 DLC8 0xFF 0x00 0x00 0x00 0x00 0x00 0x00 on PT CAN at 1 sec intervals wakes charger

Charger then sends data on Hybrid CAN interface.

ID 0x1BFE2204 on HYB CAN seems to contain HVDC voltage

ID 0x1BFE2206 on HYB CAN seems to contain AC Mains L1 - N voltage

ID 0x1BFE2207 on HYB CAN seems to contain AC Mains L2 - N voltage


Charger #2 From a Golf PHEV part number : 5QE915682AK

Two CAN busses running at 500kbps.

No wake up required.

20/11/22 : Added a very crude arduino due sketch to run the phev charger. Fails after a few seconds.

-Added CAN log from one such test.

-Video with details released soon.


