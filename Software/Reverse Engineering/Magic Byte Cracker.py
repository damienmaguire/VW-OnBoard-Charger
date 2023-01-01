''' Magic Byte Cracker.py

Small tool that "cracks" the Magic Bytes at the end of VAG CAN bus messages
to generate the correct CRC8 value.

Be aware: This tries to calculate the magic bytes on *ALL* messages,
    regardless if they have a CRC8 checksum or not. You can use the ID
    filter to specify relevant messages.

Requirements: CRCcheck Package
    pip install crccheck

Author: crasbe
Date: 2022 - 2023
'''

import csv
import operator

from crccheck.crc import Crc8Base

crc = Crc8Base

#################################################################################
# User Parameters

# Input Filename
inputFile = "../../Logs/EGolf/Fixed 260V HV.csv"


# Specify which messages to look at (empty list = all messages)
#IDfilter = ["00000564", "00000570", "0000067E"]
IDfilter = []


# CRC Parameters: AUTOSAR (VW Flavor)
crc._poly = 0x2F
crc._reflect_input = False
crc._reflect_output = False
crc._initvalue = 0xFF
#crc._initvalue = 0x0F
crc._xor_output = 0xFF

# Counter Mask (not all bits might be relevant for the CRC)
counterMask = 0x0F

################################################################################

messages = list()

with open(inputFile, newline="") as csvfile:
    reader = csv.DictReader(csvfile)

    for row in reader:
        
        # delete the elements we don't care about, makes the processing faster
        del row[None] # spurious element since all entries end with a ','
        del row["Dir"]
        del row["Bus"]
        del row["LEN"]

        # ignore extended messages and IDs not present in the filter
        if(row["Extended"] == "false"):
            if(row["ID"] in IDfilter or len(IDfilter) == 0):
            
                messages.append(row)


# open an output csv
'''outputCSV = open("log_output.csv", "w", newline="")

fieldnames = ["D1", "D2", "D3", "D4", "D5", "D6", "D7", "D8", "Magic Byte", "CRC"]

writer = csv.DictWriter(outputCSV, fieldnames)
writer.writeheader()'''

# the cracked bytes are structured as dict: {ID1 : {0 : magic, 1: magic, ...}, ID2 : ...}
cracked = dict()

for message in messages:
    # make sure we only look at elements we don't have already
    if(message["ID"] in cracked.keys()):
        if((int(message["D2"], 16) & counterMask) in cracked[message["ID"]]):
            continue

    # save the checksum we want to compare against
    referenceChecksum = int(message["D1"], 16)

    # create a message list for the CRC8 tool
    msg = [
        int(message["D2"], 16),
        int(message["D3"], 16),
        int(message["D4"], 16),
        int(message["D5"], 16),
        int(message["D6"], 16),
        int(message["D7"], 16),
        int(message["D8"], 16),
        0
        ]

    # iterate through all possible magic bytes (0x00 - 0xFF)
    for i in range(0, 256):
        msg[7] = i # set the last byte to the magic byte "under test"

        newChecksum = crc.calc(msg)

        if(newChecksum == referenceChecksum):   # we have a winner!

            # some data conversion
            newID = message["ID"]
            newCounter = int(message["D2"], 16) & counterMask
            newMagic = i
            
            print("New Magic Number (0x{:02X})) found for ID {}, Counter 0x{:02X} (0x{:02X})".format(newMagic, message["ID"], newCounter, int(message["D2"], 16)))

            # save the cracked magic number
            if(not message["ID"] in cracked.keys()):    # create a new dict, because we can't double index on first access
                cracked[message["ID"]] = dict()
                
            cracked[message["ID"]][newCounter] = newMagic

            '''writer.writerow({
                "D1" : format(int(message["D1"], 16), "02X"),
                "D2" : format(int(message["D2"], 16), "02X"),
                "D3" : format(int(message["D3"], 16), "02X"),
                "D4" : format(int(message["D4"], 16), "02X"),
                "D5" : format(int(message["D5"], 16), "02X"),
                "D6" : format(int(message["D6"], 16), "02X"),
                "D7" : format(int(message["D7"], 16), "02X"),
                "D8" : format(int(message["D8"], 16), "02X"),
                "Magic Byte" : format(newMagic, "02X"),
                "CRC" : format(newChecksum, "02X")
                })

outputCSV.close()'''

# print out the magic bytes in the correct order
for crackedID in sorted(list(cracked.keys())):

    print("ID: {}, Magic Bytes: ".format(crackedID), end="")

    for counter in sorted(list(cracked[crackedID].keys())):
        print("0x{:02X},".format(cracked[crackedID][counter]), end="")

    print("")

# iterate over all messages again to verify the magic numbers are correct
print("Verifying...")

for message in messages:

    messageID = message["ID"]
    messageCounter = int(message["D2"], 16) & counterMask

    referenceChecksum = int(message["D1"], 16)

    magicByte = cracked[messageID][messageCounter]

    # create a message list for the CRC8 tool
    msg = [
        int(message["D2"], 16),
        int(message["D3"], 16),
        int(message["D4"], 16),
        int(message["D5"], 16),
        int(message["D6"], 16),
        int(message["D7"], 16),
        int(message["D8"], 16),
        magicByte
        ]

    newChecksum = crc.calc(msg)

    if(newChecksum != referenceChecksum):
        print("Error found! Message ID: 0x{:02X}, Data: 0x{:02X}{:02X}{:02X}{:02X}{:02X}{:02X}{:02X}, ".format(
                int(message["ID"], 16), int(message["D2"], 16), int(message["D3"], 16), int(message["D4"], 16),
                int(message["D5"], 16), int(message["D6"], 16), int(message["D7"], 16), int(message["D8"], 16)) + \
              "Counter: 0x{:02X}, Magic Byte: 0x{:02X}, RefChk: 0x{:02X}, CalcChk: 0x{:02X}, Diff: 0x{:02X}".format(int(message["D2"], 16),
                   magicByte, referenceChecksum, newChecksum, (referenceChecksum - newChecksum)))
        
print("Done!")



'''
Example Output:

New Magic Number (0xE8)) found for ID 0000038B, Counter 0x00 (0x00)
New Magic Number (0x95)) found for ID 00000570, Counter 0x00 (0x00)
New Magic Number (0x24)) found for ID 00000564, Counter 0x06 (0x06)
New Magic Number (0x25)) found for ID 0000067E, Counter 0x03 (0x03)
New Magic Number (0x24)) found for ID 00000564, Counter 0x07 (0x07)
New Magic Number (0x25)) found for ID 0000067E, Counter 0x04 (0x04)
New Magic Number (0x24)) found for ID 00000564, Counter 0x08 (0x08)
New Magic Number (0x25)) found for ID 0000067E, Counter 0x05 (0x05)
New Magic Number (0x24)) found for ID 00000564, Counter 0x03 (0x73)
New Magic Number (0x24)) found for ID 00000564, Counter 0x04 (0x74)
New Magic Number (0x24)) found for ID 00000564, Counter 0x05 (0x75)
New Magic Number (0x24)) found for ID 00000564, Counter 0x09 (0x09)
New Magic Number (0x24)) found for ID 00000564, Counter 0x0A (0x0A)
New Magic Number (0x24)) found for ID 00000564, Counter 0x0B (0x0B)
New Magic Number (0x24)) found for ID 00000564, Counter 0x0C (0x0C)
New Magic Number (0x24)) found for ID 00000564, Counter 0x0D (0x0D)
New Magic Number (0x24)) found for ID 00000564, Counter 0x0E (0x0E)
New Magic Number (0x24)) found for ID 00000564, Counter 0x0F (0x0F)
New Magic Number (0x25)) found for ID 0000067E, Counter 0x06 (0x06)
New Magic Number (0x24)) found for ID 00000564, Counter 0x00 (0x00)
New Magic Number (0x25)) found for ID 0000067E, Counter 0x07 (0x07)
New Magic Number (0x24)) found for ID 00000564, Counter 0x01 (0x01)
New Magic Number (0x24)) found for ID 00000564, Counter 0x02 (0x02)
New Magic Number (0x25)) found for ID 0000067E, Counter 0x02 (0x02)
ID: 0000038B, Magic Bytes: 0xE8,
ID: 00000564, Magic Bytes: 0x24,0x24,0x24,0x24,0x24,0x24,0x24,0x24,0x24,0x24,0x24,0x24,0x24,0x24,0x24,0x24,
ID: 00000570, Magic Bytes: 0x95,
ID: 0000067E, Magic Bytes: 0x25,0x25,0x25,0x25,0x25,0x25,
Verifying...
Error found! Message ID: 0x38B, Data: 0x00FE0000000000, Counter: 0x00, Magic Byte: 0xE8, RefChk: 0x00, CalcChk: 0x61, Diff: 0x-61
Error found! Message ID: 0x38B, Data: 0x00FE0000000000, Counter: 0x00, Magic Byte: 0xE8, RefChk: 0x00, CalcChk: 0x61, Diff: 0x-61
Error found! Message ID: 0x38B, Data: 0x00FE0000000000, Counter: 0x00, Magic Byte: 0xE8, RefChk: 0x00, CalcChk: 0x61, Diff: 0x-61
Error found! Message ID: 0x38B, Data: 0x00FE0000000000, Counter: 0x00, Magic Byte: 0xE8, RefChk: 0x00, CalcChk: 0x61, Diff: 0x-61
Error found! Message ID: 0x38B, Data: 0x00FE0000000000, Counter: 0x00, Magic Byte: 0xE8, RefChk: 0x00, CalcChk: 0x61, Diff: 0x-61
Error found! Message ID: 0x38B, Data: 0x00FE0000000000, Counter: 0x00, Magic Byte: 0xE8, RefChk: 0x00, CalcChk: 0x61, Diff: 0x-61
Error found! Message ID: 0x38B, Data: 0x00FE0000000000, Counter: 0x00, Magic Byte: 0xE8, RefChk: 0x00, CalcChk: 0x61, Diff: 0x-61
Error found! Message ID: 0x38B, Data: 0x00FE0000000000, Counter: 0x00, Magic Byte: 0xE8, RefChk: 0x00, CalcChk: 0x61, Diff: 0x-61
Error found! Message ID: 0x38B, Data: 0x00FE0000000000, Counter: 0x00, Magic Byte: 0xE8, RefChk: 0x00, CalcChk: 0x61, Diff: 0x-61
Error found! Message ID: 0x38B, Data: 0x00FE0000000000, Counter: 0x00, Magic Byte: 0xE8, RefChk: 0x00, CalcChk: 0x61, Diff: 0x-61
Error found! Message ID: 0x38B, Data: 0x00FE0000000000, Counter: 0x00, Magic Byte: 0xE8, RefChk: 0x00, CalcChk: 0x61, Diff: 0x-61
Error found! Message ID: 0x38B, Data: 0x00FE0000000000, Counter: 0x00, Magic Byte: 0xE8, RefChk: 0x00, CalcChk: 0x61, Diff: 0x-61
Error found! Message ID: 0x38B, Data: 0x00FE0000000000, Counter: 0x00, Magic Byte: 0xE8, RefChk: 0x00, CalcChk: 0x61, Diff: 0x-61
Error found! Message ID: 0x38B, Data: 0x00FE0000000000, Counter: 0x00, Magic Byte: 0xE8, RefChk: 0x00, CalcChk: 0x61, Diff: 0x-61
Error found! Message ID: 0x38B, Data: 0x00FE0000000000, Counter: 0x00, Magic Byte: 0xE8, RefChk: 0x00, CalcChk: 0x61, Diff: 0x-61
Error found! Message ID: 0x38B, Data: 0x00FE0000000000, Counter: 0x00, Magic Byte: 0xE8, RefChk: 0x00, CalcChk: 0x61, Diff: 0x-61
Error found! Message ID: 0x38B, Data: 0x00FE0000000000, Counter: 0x00, Magic Byte: 0xE8, RefChk: 0x00, CalcChk: 0x61, Diff: 0x-61
Error found! Message ID: 0x38B, Data: 0x00FE0000000000, Counter: 0x00, Magic Byte: 0xE8, RefChk: 0x00, CalcChk: 0x61, Diff: 0x-61
Error found! Message ID: 0x38B, Data: 0x00FE0000000000, Counter: 0x00, Magic Byte: 0xE8, RefChk: 0x00, CalcChk: 0x61, Diff: 0x-61
Error found! Message ID: 0x38B, Data: 0x00FE0000000000, Counter: 0x00, Magic Byte: 0xE8, RefChk: 0x00, CalcChk: 0x61, Diff: 0x-61
Error found! Message ID: 0x38B, Data: 0x00FE0000000000, Counter: 0x00, Magic Byte: 0xE8, RefChk: 0x00, CalcChk: 0x61, Diff: 0x-61
Error found! Message ID: 0x38B, Data: 0x00FE0000000000, Counter: 0x00, Magic Byte: 0xE8, RefChk: 0x00, CalcChk: 0x61, Diff: 0x-61
Done!'''
