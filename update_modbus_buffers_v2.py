#!/usr/bin/env python3

import re
import os

def update_file_comprehensively(file_path):
    """Comprehensively update a file to use shared buffer instead of created buffers."""

    with open(file_path, 'r') as f:
        content = f.read()

    # Replace all ptk_buf_create patterns with shared buffer usage
    # Pattern to match buffer creation and setup
    buffer_create_pattern = re.compile(
        r'ptk_buf \*pdu_buf = ptk_buf_create\([^)]+\);\s*\n'
        r'(\s*)if \(!pdu_buf\) \{\s*\n'
        r'\s*return PTK_ERR_NO_RESOURCES;\s*\n'
        r'\s*\}',
        re.MULTILINE
    )

    buffer_replacement = r'''ptk_buf *pdu_buf = conn->buffer;
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

    content = buffer_create_pattern.sub(buffer_replacement, content)

    # Also handle cases where there's no error checking for ptk_buf_create
    simple_create_pattern = re.compile(r'ptk_buf \*pdu_buf = ptk_buf_create\([^)]+\);')
    simple_replacement = '''ptk_buf *pdu_buf = conn->buffer;

    // Reset buffer for new operation
    ptk_err err = ptk_buf_set_start(pdu_buf, 0);
    if (err != PTK_OK) {
        return err;
    }
    err = ptk_buf_set_end(pdu_buf, 0);
    if (err != PTK_OK) {
        return err;
    }'''

    content = simple_create_pattern.sub(simple_replacement, content)

    # Remove all ptk_buf_dispose calls
    dispose_pattern = re.compile(r'\s*ptk_buf_dispose\(pdu_buf\);\s*\n')
    content = dispose_pattern.sub('', content)

    # Clean up multiple blank lines
    content = re.sub(r'\n\s*\n\s*\n', '\n\n', content)

    with open(file_path, 'w') as f:
        f.write(content)

    print(f"Comprehensively updated {file_path}")

def main():
    files_to_update = [
        "/Users/kyle/Projects/protocol_toolkit/src/examples/modbus/modbus_holding_registers.c",
        "/Users/kyle/Projects/protocol_toolkit/src/examples/modbus/modbus_input_registers.c",
        "/Users/kyle/Projects/protocol_toolkit/src/examples/modbus/modbus_discrete_inputs.c",
        "/Users/kyle/Projects/protocol_toolkit/src/examples/modbus/modbus_coils.c"
    ]

    for file_path in files_to_update:
        if os.path.exists(file_path):
            update_file_comprehensively(file_path)
        else:
            print(f"File not found: {file_path}")

if __name__ == "__main__":
    main()
