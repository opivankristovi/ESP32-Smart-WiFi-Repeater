## 2026-10-15 - Fix Missing Input Validation for MQTT Port
**Learning:** The MQTT port must be properly bounded and clamped between 1 and 65535 instead of just checking for zero value when parsing web input in C++.
**Action:** Always sanitize integer-based user inputs properly to prevent logic errors and unexpected behaviors.
