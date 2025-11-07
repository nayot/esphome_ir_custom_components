
#!/usr/bin/env python3
import re

# ============================================================
# 🕒 TIMING PROFILE (matched to ESPHome C++ constants)
# ============================================================

IR_FREQUENCY = 38000               # 38 kHz carrier
HEADER_PULSE_US = 3400
HEADER_SPACE_US = -1700
PULSE_DURATION_US = 450
SPACE_ZERO_US = -420
SPACE_ONE_US = -1270
FINAL_PULSE_US = 450
SPACE_ONE_MIN_US = 850  # threshold boundary between 0 and 1

# Derived detection ranges
SPACE_0_MIN = abs(SPACE_ZERO_US) - 150
SPACE_0_MAX = abs(SPACE_ZERO_US) + 150
SPACE_1_MIN = abs(SPACE_ONE_US) - 200
SPACE_1_MAX = abs(SPACE_ONE_US) + 200

# ============================================================
# 🚀 CORE FUNCTIONS
# ============================================================

def clean_raw_ir_code(log_text: str) -> list[int]:
    """
    Cleans raw ESPHome IR log output by removing metadata
    and returning a list of integer timing values.
    """
    text = re.sub(r'\[.*?\]', '', log_text)  # remove [metadata]
    numbers = re.findall(r'-?\d+', text)
    cleaned = [int(n) for n in numbers]
    return cleaned

def decode_ir_timings(raw: list[int]) -> list[int]:
    """
    Decode Mitsubishi-style IR timings into bytes.
    Uses pulse-distance encoding:
      short space → '0'
      long space  → '1'
    """
    bits = ""
    for i in range(1, len(raw), 2):  # examine only spaces
        space = abs(raw[i])
        if SPACE_0_MIN <= space <= SPACE_0_MAX:
            bits += "0"
        elif SPACE_1_MIN <= space <= SPACE_1_MAX:
            bits += "1"

    # Convert bit string to bytes
    byte_list = []
    for i in range(0, len(bits), 8):
        chunk = bits[i:i+8]
        if len(chunk) == 8:
            byte_list.append(int(chunk, 2))
    return byte_list

def print_results(raw: list[int], byte_list: list[int]):
    print("\n------------------------------------------------")
    print("🧾 Cleaned Raw Signal:")
    print(f"[{', '.join(str(x) for x in raw)}]")
    print("------------------------------------------------")
    print(f"🔍 Decoded {len(byte_list)} bytes:")
    print(" ".join(f"{b:02X}" for b in byte_list))
    print("------------------------------------------------\n")

# ============================================================
# 🧩 MAIN LOOP
# ============================================================

if __name__ == "__main__":
    print("Paste your ESPHome raw IR log (single or multi-line).")
    print("Type 'exit' to quit.\n")

    print("Current timing profile:")
    print(f"  IR_FREQUENCY: {IR_FREQUENCY} Hz")
    print(f"  HEADER: {HEADER_PULSE_US}/{HEADER_SPACE_US} µs")
    print(f"  PULSE: {PULSE_DURATION_US} µs")
    print(f"  SPACE 0: {SPACE_0_MIN}-{SPACE_0_MAX} µs")
    print(f"  SPACE 1: {SPACE_1_MIN}-{SPACE_1_MAX} µs\n")

    while True:
        log_text = input("Your code: ").strip()
        if log_text.lower() in {"exit", "quit"}:
            break

        raw = clean_raw_ir_code(log_text)
        if not raw:
            print("⚠️ No valid numbers found.")
            continue

        bytes_ = decode_ir_timings(raw)
        print_results(raw, bytes_)