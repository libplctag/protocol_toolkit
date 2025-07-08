#!/usr/bin/env python3

from gen_codec_v2 import SimplePDLParser

content = '''
def u16_le = { type: u16, byte_order: [0, 1] }
def simple_message = {
    type: message,
    fields: [
        command: u16_le,
        length: u16_le
    ]
}
'''

parser = SimplePDLParser()
result = parser.parse_file(content)

import json
print(json.dumps(result, indent=2))