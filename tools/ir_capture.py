import re
import sys

def clean_ir_code(log_text: str) -> str:
    """Removes all metadata and extracts the pure comma-separated raw IR codes."""
    
    # 1. Remove all text inside square brackets (metadata like [20:15:30])
    text_without_metadata = re.sub(r'\[.*?\]', '', log_text)
    
    # 2. Find all sequences of digits, optionally prefixed with a dash
    # This captures the raw code numbers (e.g., 9000, -4500, etc.)
    numbers = re.findall(r'-?\d+', text_without_metadata)
    
    # 3. Join all found numbers into a list string
    joined_numbers = ", ".join(numbers)
    
    # 4. Wrap the result in square brackets
    return f"[{joined_numbers}]"

def main():
    print("--- IR Code List Cleaner ---")
    print("This will prompt you to paste multiple raw code blocks.")
    
    codes = []
    i = 1
    
    while True:
        print(f"\n--- PASTE CODE BLOCK {i} ---")
        print("Paste ALL log lines below.")
        print("(Press Ctrl+D (Linux/macOS) or Ctrl+Z then Enter (Windows) when done pasting, or type 'done' instead of pasting.)")
        
        # Read the next command line input
        try:
            raw_input = sys.stdin.read(10) # Peek at the first few characters
            if not raw_input: # Check for EOF
                break
                
            # Check if the user typed 'done' before any other paste
            if raw_input.strip().lower() == 'done':
                break
                
            # If we read text, we need to read the rest of the paste
            sys.stdin.seek(0) # Reset position (since we peeked)
            raw_log_paste = sys.stdin.read()
            
        except EOFError:
            # Catch the standard EOF signal to break the loop
            break
        except Exception:
            # Handle standard input reset for subsequent loops
            sys.stdin = open('/dev/tty')
            raw_log_paste = sys.stdin.read()
            
        # Clean the text
        cleaned_code = clean_ir_code(raw_log_paste)
        
        if cleaned_code == '[]':
            print("⚠️ No raw codes found in the pasted text. Skipping.")
        else:
            codes.append(cleaned_code)
            print(f"✅ Code {i} Cleaned: {cleaned_code[:50]}... (Total {len(re.findall(r'-?\d+', cleaned_code))} values)")
            i += 1
        
        # Reset stdin for the next paste cycle (crucial for Linux/macOS)
        try:
            sys.stdin = open('/dev/tty')
        except:
            pass # Ignore if this fails (e.g., on some Windows setups)

    print("\n" + "="*50)
    print("--- FINAL CLEANED CODE LIST (COPY THIS BLOCK) ---")
    print("Paste this into the Raw Code column of your spreadsheet.")
    print("="*50)
    
    # Print the final list for copying, each code on its own line
    for code_entry in codes:
        print(code_entry)
        
    print("="*50)
    print(f"Total {len(codes)} codes captured. Done!")


if __name__ == "__main__":
    try:
        main()
    except EOFError:
        # Final catch for the user exiting the script
        main() 
    except Exception as e:
        print(f"An unexpected error occurred: {e}")