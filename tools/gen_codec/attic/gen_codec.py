#!/usr/bin/env python3
"""
Protocol Toolkit Code Generator

Working implementation using Lark parser for PDL to C code generation.
"""

import argparse
import sys
from pathlib import Path
from typing import Dict, List, Optional
from dataclasses import dataclass

try:
    from lark import Lark, Transformer, v_args, Tree, Token
except ImportError as e:
    print(f"Missing required dependency: {e}")
    print("Install with: pip install lark")
    sys.exit(1)

# Working PDL Grammar
PDL_GRAMMAR = r"""
start: definition*

definition: "def" CNAME "=" type_definition

type_definition: primitive_type | message_type | constant_type

primitive_type: "{" "type" ":" base_type ("," attribute)* "}"
message_type: "{" "type" ":" "message" ("," attribute)* "}"
constant_type: "{" "type" ":" CNAME "," "const" ":" value "}"

attribute: byte_order_attr | fields_attr
byte_order_attr: "byte_order" ":" "[" NUMBER ("," NUMBER)* "]"
fields_attr: "fields" ":" "[" field* "]"

field: CNAME ":" field_type ","?

field_type: CNAME | array_type | bit_field_type

array_type: CNAME "[" CNAME "]"

bit_field_type: "{" "type" ":" "bit_field" "," "source" ":" "{" bit_field_attrs "}" "}"
bit_field_attrs: bit_field_attr ("," bit_field_attr)*
bit_field_attr: "container" ":" CNAME | "start_bit" ":" NUMBER | "length" ":" NUMBER

base_type: "u8" | "u16" | "u32" | "u64" | "i8" | "i16" | "i32" | "i64" | "f32" | "f64"

value: NUMBER | HEX_NUMBER

CNAME: /[a-zA-Z_][a-zA-Z0-9_]*/
NUMBER: /[0-9]+/
HEX_NUMBER: /0x[0-9a-fA-F]+/

%import common.WS
%ignore WS
%import common.CPP_COMMENT
%ignore CPP_COMMENT
"""

@dataclass 
class Field:
    name: str
    field_type: str
    type_ref: Optional[str] = None
    element_type: Optional[str] = None
    size: Optional[str] = None
    bit_field_source: Optional[Dict] = None

@dataclass
class Definition:
    name: str
    def_type: str
    base_type: Optional[str] = None
    byte_order: Optional[List[int]] = None
    fields: Optional[List[Field]] = None
    const_type: Optional[str] = None
    const_value: Optional[int] = None

class PDLTransformer(Transformer):
    def start(self, items):
        return items

    def definition(self, items):
        name = str(items[0])
        type_def = items[1]
        return Definition(name, **type_def)

    def primitive_type(self, items):
        base_type = str(items[0])
        result = {"def_type": "primitive", "base_type": base_type}
        
        for item in items[1:]:
            if isinstance(item, dict):
                result.update(item)
        
        return result

    def message_type(self, items):
        result = {"def_type": "message", "fields": []}
        
        for item in items:
            if isinstance(item, dict) and "fields" in item:
                result["fields"] = item["fields"]
        
        return result

    def constant_type(self, items):
        const_type = str(items[0])
        const_value = items[1]
        return {
            "def_type": "constant", 
            "const_type": const_type, 
            "const_value": const_value
        }

    def byte_order_attr(self, items):
        numbers = [int(str(n)) for n in items]
        return {"byte_order": numbers}

    def fields_attr(self, items):
        return {"fields": items}

    def field(self, items):
        name = str(items[0])
        field_type_info = items[1]
        
        if isinstance(field_type_info, str):
            return Field(name, "type_ref", type_ref=field_type_info)
        elif isinstance(field_type_info, dict):
            if field_type_info["field_type"] == "array":
                return Field(
                    name, "array", 
                    element_type=field_type_info["element_type"],
                    size=field_type_info["size"]
                )
            elif field_type_info["field_type"] == "bit_field":
                return Field(
                    name, "bit_field",
                    bit_field_source=field_type_info["source"]
                )
        
        return Field(name, "unknown")

    def field_type(self, items):
        if len(items) == 1 and isinstance(items[0], str):
            return str(items[0])
        return items[0]

    def array_type(self, items):
        element_type = str(items[0])
        size = str(items[1])
        return {"field_type": "array", "element_type": element_type, "size": size}

    def bit_field_type(self, items):
        source = {}
        for item in items:
            if isinstance(item, dict):
                source.update(item)
        return {"field_type": "bit_field", "source": source}

    def bit_field_attrs(self, items):
        result = {}
        for item in items:
            if isinstance(item, dict):
                result.update(item)
        return result

    def bit_field_attr(self, items):
        key = str(items[0])
        if key in ["start_bit", "length"]:
            return {key: int(str(items[1]))}
        else:
            return {key: str(items[1])}

    def value(self, items):
        val_str = str(items[0])
        if val_str.startswith('0x'):
            return int(val_str, 16)
        return int(val_str)

class CodeGenerator:
    def __init__(self, namespace: str = "ptk"):
        self.namespace = namespace
        self.message_type_counter = 1

    def get_c_type(self, pdl_type: str) -> str:
        type_map = {
            'u8': 'uint8_t', 'u16': 'uint16_t', 'u32': 'uint32_t', 'u64': 'uint64_t',
            'i8': 'int8_t', 'i16': 'int16_t', 'i32': 'int32_t', 'i64': 'int64_t',
            'f32': 'float', 'f64': 'double'
        }
        return type_map.get(pdl_type, pdl_type + '_t')

    def get_bit_field_type(self, bit_length: int) -> str:
        if bit_length <= 8:
            return 'uint8_t'
        elif bit_length <= 16:
            return 'uint16_t'
        elif bit_length <= 32:
            return 'uint32_t'
        else:
            return 'uint64_t'

    def generate_header(self, definitions: List[Definition], filename: str) -> str:
        messages = [d for d in definitions if d.def_type == "message"]
        constants = [d for d in definitions if d.def_type == "constant"]

        lines = [
            f"#ifndef {self.namespace.upper()}_{filename.upper()}_H",
            f"#define {self.namespace.upper()}_{filename.upper()}_H",
            "",
            '#include "ptk_array.h"',
            '#include "ptk_allocator.h"',
            '#include "ptk_buf.h"',
            '#include "ptk_codec.h"',
            '#include "ptk_err.h"',
            '#include "ptk_log.h"',
            "#include <stdint.h>",
            "#include <stdbool.h>",
            "#include <stddef.h>",
            "",
            "#ifdef __cplusplus",
            'extern "C" {',
            "#endif",
        ]

        if messages:
            lines.extend([
                "",
                "/* Message Type Enumeration */",
                "typedef enum {"
            ])
            
            for i, msg in enumerate(messages, 1):
                lines.append(f"    {msg.name.upper()}_MESSAGE_TYPE = {i},")
            
            lines.append("} message_type_t;")

        if constants:
            lines.extend(["", "/* Constants */"])
            for const in constants:
                c_type = self.get_c_type(const.const_type)
                lines.append(f"#define {const.name.upper()} (({c_type}){const.const_value})")

        for msg in messages:
            lines.extend([
                "",
                f"/* {msg.name} message definition */",
                f"typedef struct {msg.name}_t {{",
                "    const int message_type;"
            ])

            if msg.fields:
                for field in msg.fields:
                    if field.field_type == "type_ref":
                        c_type = self.get_c_type(field.type_ref)
                        lines.append(f"    {c_type} {field.name};")
                    elif field.field_type == "array":
                        element_c_type = self.get_c_type(field.element_type)
                        lines.append(f"    {element_c_type}_array_t *{field.name};")
                    elif field.field_type == "bit_field":
                        length = field.bit_field_source.get("length", 1)
                        c_type = self.get_bit_field_type(length)
                        lines.append(f"    {c_type} {field.name};")

            lines.extend([
                f"}} {msg.name}_t;",
                "",
                "/* Constructor/Destructor */",
                f"ptk_err {msg.name}_create(ptk_allocator_t *alloc, {msg.name}_t **instance);",
                f"void {msg.name}_dispose(ptk_allocator_t *alloc, {msg.name}_t *instance);",
                "",
                "/* Encode/Decode */",
                f"ptk_err {msg.name}_encode(ptk_allocator_t *alloc, ptk_buf_t *buf, const {msg.name}_t *instance);",
                f"ptk_err {msg.name}_decode(ptk_allocator_t *alloc, {msg.name}_t **instance, ptk_buf_t *buf);"
            ])

            # Array accessors
            if msg.fields:
                array_fields = [f for f in msg.fields if f.field_type == "array"]
                if array_fields:
                    lines.extend(["", "/* Array Accessors */"])
                    for field in array_fields:
                        element_type = self.get_c_type(field.element_type)
                        lines.extend([
                            f"ptk_err {msg.name}_get_{field.name}_element(const {msg.name}_t *msg, size_t index, {element_type} *value);",
                            f"ptk_err {msg.name}_set_{field.name}_element({msg.name}_t *msg, size_t index, {element_type} value);",
                            f"size_t {msg.name}_get_{field.name}_length(const {msg.name}_t *msg);"
                        ])

                # Bit field accessors
                bit_fields = [f for f in msg.fields if f.field_type == "bit_field"]
                if bit_fields:
                    lines.extend(["", "/* Bit Field Accessors */"])
                    for field in bit_fields:
                        source = field.bit_field_source
                        length = source.get("length", 1)
                        c_type = self.get_bit_field_type(length)
                        container = source.get("container", "")
                        start_bit = source.get("start_bit", 0)
                        
                        lines.extend([
                            f"static inline {c_type} {msg.name}_get_{field.name}(const {msg.name}_t *msg) {{",
                            f"    return (msg->{container} >> {start_bit}) & ((1 << {length}) - 1);",
                            "}",
                            "",
                            f"static inline void {msg.name}_set_{field.name}({msg.name}_t *msg, {c_type} value) {{",
                            f"    msg->{container} &= ~(((1 << {length}) - 1) << {start_bit});",
                            f"    msg->{container} |= (value & ((1 << {length}) - 1)) << {start_bit};",
                            f"    msg->{field.name} = value;",
                            "}"
                        ])

        lines.extend([
            "",
            "#ifdef __cplusplus",
            "}",
            "#endif",
            "",
            f"#endif /* {self.namespace.upper()}_{filename.upper()}_H */"
        ])

        return "\n".join(lines)

def parse_args():
    parser = argparse.ArgumentParser(description="Protocol Toolkit Code Generator")
    parser.add_argument('input_files', nargs='+', help='PDL files to process')
    parser.add_argument('-o', '--output-dir', default='./generated', help='Output directory')
    parser.add_argument('-n', '--namespace', help='C namespace prefix')
    parser.add_argument('-v', '--verbose', action='store_true', help='Verbose output')
    parser.add_argument('--version', action='version', version='%(prog)s 1.0.0')
    return parser.parse_args()

def main():
    args = parse_args()
    
    output_dir = Path(args.output_dir)
    output_dir.mkdir(parents=True, exist_ok=True)
    
    parser = Lark(PDL_GRAMMAR, parser='lalr', transformer=PDLTransformer())
    
    for input_file in args.input_files:
        input_path = Path(input_file)
        
        if not input_path.exists():
            print(f"Error: File not found: {input_file}", file=sys.stderr)
            continue
        
        if args.verbose:
            print(f"Processing {input_file}...")
        
        try:
            with open(input_path, 'r') as f:
                content = f.read()
            
            definitions = parser.parse(content)
            
            namespace = args.namespace or input_path.stem
            codegen = CodeGenerator(namespace)
            
            header_content = codegen.generate_header(definitions, input_path.stem)
            header_file = output_dir / f"{input_path.stem}.h"
            
            with open(header_file, 'w') as f:
                f.write(header_content)
            
            if args.verbose:
                print(f"Generated {header_file}")
                messages = [d for d in definitions if d.def_type == "message"]
                constants = [d for d in definitions if d.def_type == "constant"]
                print(f"  - {len(messages)} messages")
                print(f"  - {len(constants)} constants")
        
        except Exception as e:
            print(f"Error processing {input_file}: {e}", file=sys.stderr)
            if args.verbose:
                import traceback
                traceback.print_exc()

    if args.verbose:
        print("Code generation complete!")

if __name__ == '__main__':
    main()