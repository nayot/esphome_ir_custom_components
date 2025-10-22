#!/usr/bin/env python3

import re
from typing import List

# Define the acceptable timing ranges for 0 and 1 bits (in microseconds)
# Based on your previous data:
# ~1100 total time for 0 (Pulse ~660, Space ~460)
# ~2200 total time for 1 (Pulse ~660, Space ~1560)
# We look only at the duration of the *space* for classification, as pulse is consistent.
# A space for '0' is ~430-500. A space for '1' is ~1530-1600.
SPACE_ZERO_MAX = 800  # Max space time (negative value) for a '0' bit (e.g., -460)
SPACE_ONE_MIN = 1400  # Min space time (negative value) for a '1' bit (e.g., -1560)

def clean_raw_ir_code(log_text: str) -> List[int]:
    """
    Cleans raw ESPHome IR log output and returns a list of integer timings.
    """
    # 1. Remove all text inside square brackets (metadata)
    text_without_metadata = re.sub(r'\[.*?\]', '', log_text)
    
    # 2. Find all numbers in the cleaned text.
    # We strip the terminator (-10000) if present, as it's not part of the signal.
    numbers = [int(n) for n in re.findall(r'-?\d+', text_without_metadata) if int(n) > -5000]
    
    # 3. Clean up the final element if the array size is unexpected (e.g., 132 or 133 elements)
    # The actual data length for this signal type is 131 elements (65 pairs of pulse/space + 1 final pulse).
    # If size > 131, we assume the extra element(s) are noise and keep only the first 131.
    if len(numbers) > 131:
        numbers = numbers[:131]
        
    return numbers

def decode_carrier_binary(raw_timings: List[int]) -> str:
    """
    Decodes the raw pulse/space timings into a binary bitstream (64 bits).
    Assumes Carrier/Midea protocol: Header + 64 data bits.
    """
    if len(raw_timings) < 131:
        return f"Error: Received {len(raw_timings)} elements. Expected 131 (Header + 64 data bits + final pulse)."

    # The actual data stream starts after the Header Pulse (0) and Header Space (1).
    # The data bits are formed by pairs starting at index 2 (the 3rd element).
    data_bits = []

    # Iterate through the data pairs (Pulse + Space)
    # The loop stops before the final single pulse (at index 130), as it's not a bit.
    for i in range(2, len(raw_timings) - 1, 2):
        # We only need the duration of the SPACE (negative number) to classify the bit.
        space_duration = abs(raw_timings[i + 1])
        
        if space_duration < SPACE_ZERO_MAX:
            # Short space (approx 460us) = Bit 0
            data_bits.append('0')
        elif space_duration >= SPACE_ONE_MIN:
            # Long space (approx 1560us) = Bit 1
            data_bits.append('1')
        else:
            # If timing is ambiguous, report an error or use a fallback (here, we treat as 0)
            data_bits.append('?') # Use '?' for unknown/ambiguous
            
    # The Carrier signal is known to transmit 8 bytes (64 bits) of data.
    # We take the first 64 bits only.
    if len(data_bits) < 64:
        return f"Error: Decoded only {len(data_bits)} bits."
        
    # Group into 8 bytes (64 bits) for readability
    binary_string = "".join(data_bits[:64])
    
    formatted_binary = " ".join([binary_string[i:i+8] for i in range(0, 64, 8)])

    return formatted_binary

if __name__ == "__main__":
    while True:
        signal = input("decode: ") 
        print("--- Decoding Carrier/Midea Signal ---")
        
        # 1. Clean the raw data
        raw_list = clean_raw_ir_code(signal)
        print(f"Cleaned List Size: {len(raw_list)} elements")
        
        # 2. Decode to binary
        binary_result = decode_carrier_binary(raw_list)
        print("-------------------------------------")
        print(f"Decoded 64-bit Binary (8 Bytes):")
        print(binary_result)

        # Convert to Hex (Optional but helpful)
        if '?' not in binary_result and len(binary_result.replace(" ", "")) == 64:
            hex_bytes = ""
            binary_only = binary_result.replace(" ", "")
            for i in range(0, 64, 8):
                byte = binary_only[i:i+8]
                hex_bytes += f"{int(byte, 2):02X} "
            print(f"Decoded Hex (8 Bytes):")
            print(hex_bytes.strip())