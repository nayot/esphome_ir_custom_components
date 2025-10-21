#!/usr/bin/env python3

import re

def clean_raw_ir_code(log_text: str) -> str:
    """
    Cleans raw ESPHome IR log output by first removing all metadata
    in square brackets, then extracting the remaining numbers.
    """
    
    # 1. NEW STEP: Remove all text inside square brackets.
    #    The regex \[.*?\] finds a literal '[', then any character
    #    non-greedily (the '.*?'), then a literal ']'.
    text_without_metadata = re.sub(r'\[.*?\]', '', log_text)
    
    # 2. Find all numbers in the *cleaned* text.
    numbers = re.findall(r'-?\d+', text_without_metadata)
    
    # 3. Join and format
    joined_numbers = ", ".join(numbers)
    return f"[{joined_numbers}]"

if __name__ == "__main__":
    while True:
        text = input("Your code: ")
        result = clean_raw_ir_code(text)
        print("------------------------------------------------")
        print(result)

