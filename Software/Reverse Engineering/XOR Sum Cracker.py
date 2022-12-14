''' XOR Sum Cracker.py

Small tool that calculates the XOR sum of a CAN messages.

Be aware: This tries to calculate the magic bytes on *ALL* messages,
    regardless if they have a CRC8 checksum or not. You can use the ID
    filter to specify relevant messages.

The xorsum function was generated by CRCBeagle.

Author: crasbe
Date: 2023
'''

################################################################################
# User Parameters

# Input Filename
inputFile = "61a_log.csv"

# Specify which messages to look at (empty list = all messages)
#IDfilter = ["0000061A"]
IDfilter = []

################################################################################

import csv

# XORsum function from CRCBeagle
def xorsum(message):
    checksum = 0
    for d in message:
        checksum ^= d
    checksum = (checksum & 0xff) ^ 0x00
    return checksum


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


for message in messages:

    # create a message list for the xorsum tool
    msg = [
        int(message["D2"], 16),
        int(message["D3"], 16),
        int(message["D4"], 16),
        int(message["D5"], 16),
        int(message["D6"], 16),
        int(message["D7"], 16),
        int(message["D8"], 16)
        ]

    refChecksum = int(message["D1"], 16)
    newChecksum = xorsum(msg)

    if(newChecksum == refChecksum):
        print("Match: ", end="")
        print("Reference Checksum: 0x{:02X}, New Checksum: 0x{:02X}".format(refChecksum, newChecksum))
    else:
        print("Mismatch: ", end="")
        print("Reference Checksum: 0x{:02X}, New Checksum: 0x{:02X}".format(refChecksum, newChecksum))


''' Example Output:

Match: Reference Checksum: 0x07, New Checksum: 0x07
Match: Reference Checksum: 0x06, New Checksum: 0x06
Match: Reference Checksum: 0x05, New Checksum: 0x05
Match: Reference Checksum: 0x04, New Checksum: 0x04
Match: Reference Checksum: 0x0B, New Checksum: 0x0B
Match: Reference Checksum: 0x0A, New Checksum: 0x0A
Match: Reference Checksum: 0x09, New Checksum: 0x09
Match: Reference Checksum: 0x08, New Checksum: 0x08
Match: Reference Checksum: 0x0F, New Checksum: 0x0F
Match: Reference Checksum: 0x0E, New Checksum: 0x0E
Match: Reference Checksum: 0x0D, New Checksum: 0x0D
Match: Reference Checksum: 0x0C, New Checksum: 0x0C
Match: Reference Checksum: 0x03, New Checksum: 0x03
Match: Reference Checksum: 0x02, New Checksum: 0x02
Match: Reference Checksum: 0x01, New Checksum: 0x01
Match: Reference Checksum: 0x00, New Checksum: 0x00
Match: Reference Checksum: 0x07, New Checksum: 0x07
Match: Reference Checksum: 0x06, New Checksum: 0x06
Match: Reference Checksum: 0x05, New Checksum: 0x05
Match: Reference Checksum: 0x04, New Checksum: 0x04
Match: Reference Checksum: 0x0B, New Checksum: 0x0B
Match: Reference Checksum: 0x0A, New Checksum: 0x0A
Match: Reference Checksum: 0x09, New Checksum: 0x09
Match: Reference Checksum: 0x08, New Checksum: 0x08
'''
