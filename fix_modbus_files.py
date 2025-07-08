#!/usr/bin/env python3

import re
import os

def clean_modbus_file(file_path):
    """Clean up a modbus file that has formatting issues from the previous script."""

    with open(file_path, 'r') as f:
        content = f.read()

    # Fix the variable redefinition issue by removing duplicate 'ptk_err err' declarations
    # Keep only the first one in each function

    # Pattern to find functions with multiple err declarations
    function_pattern = re.compile(r'(ptk_err\s+\w+[^{]*\{[^}]*?)(\s*ptk_err\s+err\s*=[^;]*;[^}]*?)(\s*ptk_err\s+err\s*=[^;]*;)', re.DOTALL)

    def fix_function(match):
        # Replace subsequent 'ptk_err err =' with just 'err ='
        function_content = match.group(0)
        # Fix all subsequent declarations
        fixed = re.sub(r'\n\s*ptk_err\s+err\s*=', '\n    err =', function_content)
        return fixed

    # Apply the fix repeatedly until no more matches
    while function_pattern.search(content):
        content = function_pattern.sub(fix_function, content)

    # Fix broken return statements
    content = re.sub(r';\s*return err;\s*\n\s*}', ';\n    return err;\n}', content)
    content = re.sub(r'{\s*return err;\s*\n\s*}', '{\n        return err;\n    }', content)

    # Fix formatting issues - remove extra blank lines and fix indentation
    content = re.sub(r'\n\s*\n\s*\n', '\n\n', content)
    content = re.sub(r'{\s*\n\s*\n\s*([^}])', r'{\n        \1', content)
    content = re.sub(r'\n\s*\n\s*}', '\n}', content)

    # Fix broken function calls like "modbus_send_frame(conn, pdu_buf);        return err;"
    content = re.sub(r'modbus_send_frame\(conn, pdu_buf\);\s*return err;', 'return modbus_send_frame(conn, pdu_buf);', content)

    with open(file_path, 'w') as f:
        f.write(content)

    print(f"Cleaned up {file_path}")

def main():
    files_to_clean = [
        "/Users/kyle/Projects/protocol_toolkit/src/examples/modbus/modbus_holding_registers.c",
        "/Users/kyle/Projects/protocol_toolkit/src/examples/modbus/modbus_input_registers.c",
        "/Users/kyle/Projects/protocol_toolkit/src/examples/modbus/modbus_discrete_inputs.c",
        "/Users/kyle/Projects/protocol_toolkit/src/examples/modbus/modbus_coils.c"
    ]

    for file_path in files_to_clean:
        if os.path.exists(file_path):
            clean_modbus_file(file_path)
        else:
            print(f"File not found: {file_path}")

if __name__ == "__main__":
    main()
