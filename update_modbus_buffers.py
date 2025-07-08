#!/usr/bin/env python3

import re
import os
import sys

def update_buffer_usage(file_path):
    """Update buffer usage in a Modbus file to use shared buffer."""

    with open(file_path, 'r') as f:
        content = f.read()

    # Pattern 1: Replace buffer creation with shared buffer usage
    # This handles most cases where we create a buffer, use it, and dispose it
    pattern1 = re.compile(
        r'(\s+)// Create buffer with capacity for.*?\n'
        r'\s+ptk_buf \*pdu_buf = ptk_buf_create\(conn->allocator, [^)]+\);\n'
        r'\s+if \(!pdu_buf\) \{\s*\n'
        r'\s+return PTK_ERR_NO_RESOURCES;\s*\n'
        r'\s+\}',
        re.MULTILINE | re.DOTALL
    )

    replacement1 = r'''\1// Use shared buffer for PDU
\1ptk_buf_t *pdu_buf = conn->buffer;
\1
\1// Reset buffer for new operation
\1ptk_err err = ptk_buf_set_start(pdu_buf, 0);
\1if (err != PTK_OK) {
\1    return err;
\1}
\1err = ptk_buf_set_end(pdu_buf, 0);
\1if (err != PTK_OK) {
\1    return err;
\1}'''

    content = pattern1.sub(replacement1, content)

    # Pattern 2: Remove ptk_buf_dispose calls and update error handling
    # Remove ptk_buf_dispose(pdu_buf); from error cases
    pattern2 = re.compile(r'\s+ptk_buf_dispose\(pdu_buf\);\s*\n\s+return err;')
    replacement2 = r'        return err;'
    content = pattern2.sub(replacement2, content)

    # Remove ptk_buf_dispose(pdu_buf); from normal cases
    pattern3 = re.compile(r'\s+err = modbus_send_frame\(conn, pdu_buf\);\s*\n\s+ptk_buf_dispose\(pdu_buf\);\s*\n\s+return err;')
    replacement3 = r'    return modbus_send_frame(conn, pdu_buf);'
    content = pattern3.sub(replacement3, content)

    # Handle recv functions that might have different patterns
    pattern4 = re.compile(
        r'(\s+)// Create buffer.*?\n'
        r'\s+ptk_buf \*pdu_buf = ptk_buf_create\([^)]+\);\n'
        r'\s+if \(!pdu_buf\) \{\s*\n'
        r'\s+return PTK_ERR_NO_RESOURCES;\s*\n'
        r'\s+\}',
        re.MULTILINE | re.DOTALL
    )

    content = pattern4.sub(replacement1, content)

    with open(file_path, 'w') as f:
        f.write(content)

    print(f"Updated {file_path}")

def main():
    files_to_update = [
        "/Users/kyle/Projects/protocol_toolkit/src/examples/modbus/modbus_holding_registers.c",
        "/Users/kyle/Projects/protocol_toolkit/src/examples/modbus/modbus_input_registers.c",
        "/Users/kyle/Projects/protocol_toolkit/src/examples/modbus/modbus_discrete_inputs.c",
        "/Users/kyle/Projects/protocol_toolkit/src/examples/modbus/modbus_coils.c"
    ]

    for file_path in files_to_update:
        if os.path.exists(file_path):
            update_buffer_usage(file_path)
        else:
            print(f"File not found: {file_path}")

if __name__ == "__main__":
    main()
