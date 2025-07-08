#!/usr/bin/env python3
"""
Protocol Toolkit Code Generator - Final Working Version

Simple but functional PDL to C code generator using Lark parser.
"""

import argparse
import os
import sys
from pathlib import Path
from typing import Dict, List, Optional, Union, Any
from dataclasses import dataclass

try:
    from lark import Lark, Transformer, v_args
except ImportError as e:
    print(f"Missing required dependency: {e}")
    print("Install with: pip install lark")
    sys.exit(1)

# Simplified PDL Grammar that actually works
PDL_GRAMMAR = r"""
start: item*

item: definition

definition: "def" CNAME "=" type_def

type_def: primitive_def | message_def | constant_def

primitive_def: "{" "type" ":" base_type ("," primitive_attr)* "}"
primitive_attr: "byte_order" ":" "[" NUMBER ("," NUMBER)* "]"

message_def: "{" "type" ":" "message" ("," "fields" ":" field_list)? "}"

constant_def: "{" "type" ":" CNAME "," "const" ":" value "}"

field_list: "[" (field_def ("," field_def)*)? "]"

field_def: CNAME ":" field_type

field_type: CNAME 
          | array_type
          | bit_field_def

array_type: CNAME "[" CNAME "]"

bit_field_def: "{" "type" ":" "bit_field" "," "source" ":" bit_source "}"

bit_source: "{" "container" ":" CNAME "," "start_bit" ":" NUMBER "," "length" ":" NUMBER "}"

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
class Definition:
    name: str
    def_type: str
    attributes: Dict[str, Any]

@dataclass
class GeneratedField:
    name: str
    c_type: str
    is_array: bool = False
    is_bit_field: bool = False
    bit_field_info: Optional[Dict] = None

@dataclass
class GeneratedMessage:
    name: str
    fields: List[GeneratedField]
    message_type_id: int

class PDLTransformer(Transformer):
    """Transform parse tree to Definition objects"""
    
    @v_args(inline=True)
    def start(self, *items):
        return list(items)
    
    @v_args(inline=True)
    def item(self, definition):
        return definition
    
    @v_args(inline=True)
    def definition(self, name, type_def):
        return Definition(str(name), type_def['type'], type_def)
    
    @v_args(inline=True)
    def primitive_def(self, base_type, *attrs):
        result = {'type': 'primitive', 'base_type': str(base_type)}
        for attr in attrs:
            result.update(attr)
        return result
    
    @v_args(inline=True)
    def primitive_attr(self, *numbers):
        return {'byte_order': [int(str(n)) for n in numbers]}
    
    @v_args(inline=True)
    def message_def(self, *args):
        result = {'type': 'message', 'fields': []}
        for arg in args:
            if isinstance(arg, list):  # field list
                result['fields'] = arg
        return result
    
    @v_args(inline=True)
    def constant_def(self, type_name, value):
        return {
            'type': 'constant',
            'type_ref': str(type_name),
            'value': self._convert_value(value)
        }
    
    @v_args(inline=True)
    def field_list(self, *fields):
        return list(fields) if fields else []
    
    @v_args(inline=True)
    def field_def(self, name, field_type):
        return {'name': str(name), **field_type}
    
    @v_args(inline=True)
    def field_type(self, type_ref):
        if isinstance(type_ref, dict):
            return type_ref
        return {'field_type': 'type_ref', 'ref': str(type_ref)}
    
    @v_args(inline=True)
    def array_type(self, element_type, size):
        return {
            'field_type': 'array',
            'element_type': str(element_type),
            'size': str(size)
        }
    
    @v_args(inline=True)
    def bit_field_def(self, source):
        return {'field_type': 'bit_field', 'source': source}
    
    @v_args(inline=True)
    def bit_source(self, container, start_bit, length):
        return {
            'container': str(container),
            'start_bit': int(str(start_bit)),
            'length': int(str(length))
        }
    
    @v_args(inline=True)
    def value(self, val):
        return self._convert_value(val)
    
    def _convert_value(self, val):
        val_str = str(val)
        if val_str.startswith('0x'):
            return int(val_str, 16)
        return int(val_str)

class CodeGenerator:
    """Generate C code from definitions"""
    
    def __init__(self, namespace: str = "ptk"):
        self.namespace = namespace
        self.message_type_counter = 1
    
    def get_c_type(self, pdl_type: str) -> str:
        """Map PDL types to C types"""
        type_map = {
            'u8': 'uint8_t',
            'u16': 'uint16_t', 
            'u32': 'uint32_t',
            'u64': 'uint64_t',
            'i8': 'int8_t',
            'i16': 'int16_t',
            'i32': 'int32_t',
            'i64': 'int64_t',
            'f32': 'float',
            'f64': 'double'
        }
        return type_map.get(pdl_type, pdl_type + '_t')
    
    def get_bit_field_type(self, bit_length: int) -> str:
        """Get appropriate C type for bit field based on length"""
        if bit_length <= 8:
            return 'uint8_t'
        elif bit_length <= 16:
            return 'uint16_t'
        elif bit_length <= 32:
            return 'uint32_t'
        else:
            return 'uint64_t'
    
    def process(self, definitions: List[Definition]) -> Dict:
        """Process definitions and generate code data"""
        
        messages = []
        constants = []
        
        for defn in definitions:
            if defn.def_type == 'message':
                msg = self.generate_message(defn)
                messages.append(msg)
                self.message_type_counter += 1
            elif defn.def_type == 'constant':
                const = self.generate_constant(defn)
                constants.append(const)
        
        return {
            'messages': messages,
            'constants': constants,
            'namespace': self.namespace
        }
    
    def generate_message(self, defn: Definition) -> GeneratedMessage:
        """Generate message structure"""
        
        fields = []
        
        for field_def in defn.attributes.get('fields', []):
            field = self.generate_field(field_def)
            if field:
                fields.append(field)
        
        return GeneratedMessage(
            name=defn.name,
            fields=fields,
            message_type_id=self.message_type_counter
        )
    
    def generate_field(self, field_def: Dict) -> Optional[GeneratedField]:
        """Generate field structure"""
        
        name = field_def['name']
        field_type = field_def.get('field_type', 'type_ref')
        
        if field_type == 'bit_field':
            source = field_def.get('source', {})
            length = source.get('length', 1)
            return GeneratedField(
                name=name,
                c_type=self.get_bit_field_type(length),
                is_bit_field=True,
                bit_field_info=source
            )
        
        elif field_type == 'array':
            element_type = field_def.get('element_type', 'u8')
            element_c_type = self.get_c_type(element_type)
            return GeneratedField(
                name=name,
                c_type=f"{element_c_type}_array_t *",
                is_array=True
            )
        
        elif field_type == 'type_ref':
            ref = field_def.get('ref', 'u8')
            c_type = self.get_c_type(ref)
            return GeneratedField(
                name=name,
                c_type=c_type
            )
        
        return None
    
    def generate_constant(self, defn: Definition) -> Dict:
        """Generate constant definition"""
        return {
            'name': defn.name.upper(),
            'type': self.get_c_type(defn.attributes.get('type_ref', 'u32')),
            'value': defn.attributes.get('value', 0)
        }
    
    def generate_header(self, data: Dict, filename: str = "generated") -> str:
        """Generate C header file"""
        
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
            "",
            "/* Message Type Enumeration */",
            "typedef enum {"
        ]
        
        for message in data['messages']:
            lines.append(f"    {message.name.upper()}_MESSAGE_TYPE = {message.message_type_id},")
        
        lines.extend([
            "} message_type_t;",
            "",
            "/* Constants */"
        ])
        
        for const in data['constants']:
            lines.append(f"#define {const['name']} (({const['type']}){const['value']})")
        
        for message in data['messages']:
            lines.extend([
                "",
                f"/* {message.name} message definition */",
                f"typedef struct {message.name}_t {{",
                "    const int message_type;"
            ])
            
            for field in message.fields:
                lines.append(f"    {field.c_type} {field.name};")
            
            lines.extend([
                f"}} {message.name}_t;",
                "",
                "/* Constructor/Destructor */",
                f"ptk_err {message.name}_create(ptk_allocator_t *alloc, {message.name}_t **instance);",
                f"void {message.name}_dispose(ptk_allocator_t *alloc, {message.name}_t *instance);",
                "",
                "/* Encode/Decode */",
                f"ptk_err {message.name}_encode(ptk_allocator_t *alloc, ptk_buf_t *buf, const {message.name}_t *instance);",
                f"ptk_err {message.name}_decode(ptk_allocator_t *alloc, {message.name}_t **instance, ptk_buf_t *buf);"
            ])
            
            # Array accessors
            array_fields = [f for f in message.fields if f.is_array]
            if array_fields:
                lines.append("")
                lines.append("/* Array Accessors */")
                for field in array_fields:
                    element_type = field.c_type.replace('_array_t *', '')
                    lines.extend([
                        f"ptk_err {message.name}_get_{field.name}_element(const {message.name}_t *msg, size_t index, {element_type} *value);",
                        f"ptk_err {message.name}_set_{field.name}_element({message.name}_t *msg, size_t index, {element_type} value);",
                        f"size_t {message.name}_get_{field.name}_length(const {message.name}_t *msg);"
                    ])
            
            # Bit field accessors
            bit_fields = [f for f in message.fields if f.is_bit_field]
            if bit_fields:
                lines.append("")
                lines.append("/* Bit Field Accessors */")
                for field in bit_fields:
                    info = field.bit_field_info
                    lines.extend([
                        f"static inline {field.c_type} {message.name}_get_{field.name}(const {message.name}_t *msg) {{",
                        f"    return (msg->{info['container']} >> {info['start_bit']}) & ((1 << {info['length']}) - 1);",
                        "}",
                        "",
                        f"static inline void {message.name}_set_{field.name}({message.name}_t *msg, {field.c_type} value) {{",
                        f"    msg->{info['container']} &= ~(((1 << {info['length']}) - 1) << {info['start_bit']});",
                        f"    msg->{info['container']} |= (value & ((1 << {info['length']}) - 1)) << {info['start_bit']};",
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
    """Parse command line arguments"""
    parser = argparse.ArgumentParser(
        description="Protocol Toolkit Code Generator",
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog="""
Examples:
  %(prog)s modbus.pdl
  %(prog)s -o ./protocols -n eip protocols/ethernet_ip.pdl
        """
    )
    
    parser.add_argument('input_files', nargs='+', metavar='FILE',
                       help='PDL files to process')
    parser.add_argument('-o', '--output-dir', default='./generated',
                       help='Output directory for generated files (default: ./generated)')
    parser.add_argument('-n', '--namespace', 
                       help='C namespace prefix for generated code (default: derived from filename)')
    parser.add_argument('-v', '--verbose', action='store_true',
                       help='Enable verbose output')
    parser.add_argument('--version', action='version', version='%(prog)s 1.0.0')
    
    return parser.parse_args()

def main():
    """Main entry point"""
    args = parse_args()
    
    # Create output directory
    output_dir = Path(args.output_dir)
    output_dir.mkdir(parents=True, exist_ok=True)
    
    # Initialize parser
    parser = Lark(PDL_GRAMMAR, parser='lalr', transformer=PDLTransformer())
    
    # Process each input file
    for input_file in args.input_files:
        input_path = Path(input_file)
        
        if not input_path.exists():
            print(f"Error: File not found: {input_file}", file=sys.stderr)
            continue
        
        if args.verbose:
            print(f"Processing {input_file}...")
        
        try:
            # Read and parse PDL file
            with open(input_path, 'r') as f:
                content = f.read()
            
            definitions = parser.parse(content)
            
            if args.verbose:
                print(f"Parsed {len(definitions)} definitions")
            
            # Generate namespace from filename if not provided
            namespace = args.namespace or input_path.stem
            
            # Generate code
            codegen = CodeGenerator(namespace)
            generated_data = codegen.process(definitions)
            
            # Write header file
            header_content = codegen.generate_header(generated_data, input_path.stem)
            header_file = output_dir / f"{input_path.stem}.h"
            
            with open(header_file, 'w') as f:
                f.write(header_content)
            
            if args.verbose:
                print(f"Generated {header_file}")
                print(f"  - {len(generated_data['messages'])} messages")
                print(f"  - {len(generated_data['constants'])} constants")
            
        except Exception as e:
            print(f"Error processing {input_file}: {e}", file=sys.stderr)
            if args.verbose:
                import traceback
                traceback.print_exc()
            continue
    
    if args.verbose:
        print("Code generation complete!")

if __name__ == '__main__':
    main()