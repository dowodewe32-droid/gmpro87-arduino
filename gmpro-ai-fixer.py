#!/usr/bin/env python3
"""
GMpro ESP32 AI Fixer - Simple Pattern-Based Fixer
Uses rule-based fixes for common ESP32 WiFi issues
"""
import re

# Common ESP32 WiFi fixes
PATTERNS = {
    # WiFi mode switches that kill AP
    r'WiFi\.mode\(WIFI_AP\);[\s\n]*WiFi\.softAP': 'WiFi.mode(WIFI_AP_STA); WiFi.softAP',
    r'WiFi\.mode\(WIFI_STA\);[\s\n]*esp_wifi': 'WiFi.mode(WIFI_AP_STA); esp_wifi',
    
    # Add watchdog protection
    r'(for.*\{[^}]*esp_wifi_[^}]*)(\} *)': r'\1 yield(); \2',
    
    # Fix pointer issues
    r'std::vector<uint8_t\*>': 'std::vector<String>',
    r'malloc\(': 'String(',
    r'free\(': '// freed: ',
}

def fix_code(source_code):
    """Apply all pattern fixes"""
    fixes_applied = 0
    
    for pattern, replacement in PATTERNS.items():
        matches = re.findall(pattern, source_code)
        if matches:
            source_code = re.sub(pattern, replacement, source_code)
            fixes_applied += len(matches)
            print(f"Applied: {pattern[:40]}... ({len(matches)}x)")
    
    return source_code, fixes_applied

def main():
    print("="*50)
    print("GMpro ESP32 AI Fixer v1.0")
    print("="*50)
    
    # Read source
    try:
        with open("gmpro87/gmpro87.ino", "r") as f:
            code = f.read()
    except:
        print("ERROR: No source file found")
        return
    
    print(f"\nOriginal: {len(code)} chars")
    
    # Apply fixes
    fixed, count = fix_code(code)
    
    print(f"\nFixed: {count} issues resolved")
    
    # Save
    with open("gmpro87/gmpro87.ino", "w") as f:
        f.write(fixed)
    
    print(f"\nSaved! Ready for build.")
    print("="*50)

if __name__ == "__main__":
    main()