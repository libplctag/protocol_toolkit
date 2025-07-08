#!/usr/bin/env python3
"""
Protocol Toolkit Code Generator

Transforms Protocol Definition Language (PDL) files into type-safe C code
for parsing and generating binary protocol messages.
"""

import argparse
import os
import sys
from pathlib import Path
from typing import Dict, List, Optional, Union, Any
from dataclasses import dataclass, field
from enum import Enum
import json

try:
    from lark import Lark, Transformer, Tree, Token
    from jinja2 import Environment, FileSystemLoader, Template
except ImportError as e:
    print(f"Missing required dependency: {e}")
    print("Install with: pip install lark jinja2")
    sys.exit(1)

# Simplified PDL Grammar for Lark
PDL_GRAMMAR = r"""
start: protocol_file

protocol_file: (import_statement | definition)*

import_statement: "import" IDENTIFIER "=" STRING

definition: "def" IDENTIFIER "=" type_definition

type_definition: primitive_type 
               | message_type 
               | constant_type
               | bit_field_type

primitive_type: "{" "type" ":" base_type ("," "byte_order" ":" byte_order_array)? ("," "const" ":" constant)? "}"

message_type: "{" "type" ":" "message" ("," "fields" ":" field_array)? ("," "match" ":" expression_or_function)? "}"

constant_type: "{" "type" ":" type_reference "," "const" ":" constant "}"

bit_field_type: "{" "type" ":" "bit_field" "," "source" ":" bit_field_source "}"

bit_field_source: "{" "container" ":" IDENTIFIER "," "start_bit" ":" NUMBER "," "length" ":" NUMBER "}"

field_array: "[" field_list? "]"
field_list: field_definition ("," field_definition)*

field_definition: IDENTIFIER ":" field_spec

field_spec: type_reference
          | simple_array_type
          | bit_field_type

simple_array_type: type_reference "[" array_size "]"
array_size: IDENTIFIER | NUMBER

type_reference: IDENTIFIER | namespaced_identifier
namespaced_identifier: IDENTIFIER "." IDENTIFIER

base_type: "u8" | "u16" | "u32" | "u64" | "i8" | "i16" | "i32" | "i64" | "f32" | "f64"

byte_order_array: "[" number_list "]"
number_list: NUMBER ("," NUMBER)*

expression_or_function: "(" expression ")" | IDENTIFIER

expression: logical_or

logical_or: logical_and ("||" logical_and)*
logical_and: equality ("&&" equality)*
equality: relational (("==" | "!=") relational)*
relational: additive (("<" | "<=" | ">" | ">=") additive)*
additive: multiplicative (("+" | "-") multiplicative)*
multiplicative: primary (("*" | "/" | "%") primary)*

primary: IDENTIFIER
       | namespaced_identifier
       | NUMBER
       | field_reference
       | "(" expression ")"

field_reference: "$" ("." IDENTIFIER)*

constant: NUMBER | STRING | boolean_literal
boolean_literal: "true" | "false"

IDENTIFIER: /[a-zA-Z_][a-zA-Z0-9_]*/
NUMBER: /0x[0-9a-fA-F]+/ | /[0-9]+/
STRING: /"[^"]*"/

%import common.WS
%ignore WS
%import common.CPP_COMMENT
%ignore CPP_COMMENT
"""

# AST Node Classes
@dataclass
class ASTNode:
    """Base class for all AST nodes"""
    pass

@dataclass
class ImportStatement(ASTNode):
    namespace: str
    file_path: str

@dataclass
class Definition(ASTNode):
    name: str
    type_def: 'TypeDefinition'

@dataclass
class TypeDefinition(ASTNode):
    pass

@dataclass
class PrimitiveType(TypeDefinition):
    base_type: str
    byte_order: Optional[List[int]] = None
    const_value: Optional[Union[int, float, str]] = None

@dataclass
class MessageType(TypeDefinition):
    fields: List['FieldDefinition'] = field(default_factory=list)
    match_expr: Optional[str] = None

@dataclass
class ConstantType(TypeDefinition):
    type_ref: str
    value: Union[int, float, str] = 0

@dataclass
class FieldDefinition(ASTNode):
    name: str
    type_spec: TypeDefinition

@dataclass
class BitFieldType(TypeDefinition):
    container: str
    start_bit: int
    length: int

@dataclass
class SimpleArrayType(TypeDefinition):
    element_type: str
    size: Union[str, int]  # field name or constant

@dataclass
class TypeReference(TypeDefinition):
    name: str
    namespace: Optional[str] = None

# Code Generation Data Classes
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
    has_arrays: bool = False
    has_bit_fields: bool = False

class PDLTransformer(Transformer):
    """Transforms Lark parse tree into AST"""
    
    def protocol_file(self, items):
        imports = []
        definitions = []
        for item in items:
            if isinstance(item, ImportStatement):
                imports.append(item)
            elif isinstance(item, Definition):
                definitions.append(item)
        return {'imports': imports, 'definitions': definitions}
    
    def import_statement(self, items):
        namespace = str(items[0])
        file_path = str(items[1]).strip('"')
        return ImportStatement(namespace, file_path)
    
    def definition(self, items):
        name = str(items[0])
        type_def = items[1]
        return Definition(name, type_def)
    
    def primitive_type(self, items):
        base_type = str(items[0])
        byte_order = None
        const_value = None
        
        # Parse optional attributes
        i = 1
        while i < len(items):
            if isinstance(items[i], list):  # byte_order
                byte_order = items[i]
            elif isinstance(items[i], (int, float, str)):  # const
                const_value = items[i]
            i += 1
        
        return PrimitiveType(base_type, byte_order, const_value)
    
    def message_type(self, items):
        fields = []
        match_expr = None
        
        for item in items:
            if isinstance(item, list):  # fields
                fields = item
            elif isinstance(item, str):  # match expression
                match_expr = item
        
        return MessageType(fields, match_expr)
    
    def constant_type(self, items):
        type_ref = str(items[0])
        value = items[1]
        return ConstantType(type_ref, value)
    
    def field_definition(self, items):
        name = str(items[0])
        type_spec = items[1]
        return FieldDefinition(name, type_spec)
    
    def bit_field_type(self, items):
        # items[0] should be the bit_field_source dict
        source = items[0]
        return BitFieldType(
            container=source.get('container', ''),
            start_bit=source.get('start_bit', 0),
            length=source.get('length', 1)
        )
    
    def bit_field_source(self, items):
        container = str(items[0])
        start_bit = int(str(items[1]))
        length = int(str(items[2]))
        return {
            'container': container,
            'start_bit': start_bit,
            'length': length
        }
    
    def simple_array_type(self, items):
        element_type = str(items[0])
        size = items[1]
        if isinstance(size, Token):
            try:
                size = int(str(size))
            except ValueError:
                size = str(size)  # field reference
        return SimpleArrayType(element_type, size)
    
    def type_reference(self, items):
        if len(items) == 1:
            return TypeReference(str(items[0]))
        else:
            return TypeReference(str(items[1]), str(items[0]))
    
    def byte_order_array(self, items):
        return [int(str(num)) for num in items]
    
    def number_list(self, items):
        return [int(str(num)) for num in items]
    
    def field_array(self, items):
        return items
    
    def field_list(self, items):
        return items
    
    def expression_or_function(self, items):
        return str(items[0])
    
    def constant(self, items):
        val = str(items[0])
        if val.startswith('"') and val.endswith('"'):
            return val[1:-1]  # Remove quotes
        elif val.startswith('0x'):
            return int(val, 16)
        elif val in ['true', 'false']:
            return val == 'true'
        else:
            try:
                return int(val)
            except ValueError:
                return val

class CodeGenerator:
    """Generates C code from AST"""
    
    def __init__(self, namespace: str = "ptk"):
        self.namespace = namespace
        self.types: Dict[str, TypeDefinition] = {}
        self.imports: Dict[str, str] = {}
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
    
    def process_ast(self, ast: Dict) -> Dict:
        """Process AST and build type registry"""
        
        # Register imports
        for imp in ast['imports']:
            self.imports[imp.namespace] = imp.file_path
        
        # Register all types first
        for defn in ast['definitions']:
            self.types[defn.name] = defn.type_def
        
        # Generate code for each definition
        generated = {
            'messages': [],
            'constants': [],
            'namespace': self.namespace
        }
        
        for defn in ast['definitions']:
            if isinstance(defn.type_def, MessageType):
                msg = self.generate_message(defn.name, defn.type_def)
                generated['messages'].append(msg)
                self.message_type_counter += 1
            elif isinstance(defn.type_def, ConstantType):
                const = self.generate_constant(defn.name, defn.type_def)
                generated['constants'].append(const)
        
        return generated
    
    def generate_message(self, name: str, msg_type: MessageType) -> GeneratedMessage:
        """Generate C struct and accessors for message type"""
        
        fields = []
        has_arrays = False
        has_bit_fields = False
        
        for field_def in msg_type.fields:
            field = self.generate_field(field_def)
            fields.append(field)
            
            if field.is_array:
                has_arrays = True
            if field.is_bit_field:
                has_bit_fields = True
        
        return GeneratedMessage(
            name=name,
            fields=fields,
            message_type_id=self.message_type_counter,
            has_arrays=has_arrays,
            has_bit_fields=has_bit_fields
        )
    
    def generate_field(self, field_def: FieldDefinition) -> GeneratedField:
        """Generate field information"""
        
        type_spec = field_def.type_spec
        
        if isinstance(type_spec, BitFieldType):
            return GeneratedField(
                name=field_def.name,
                c_type=self.get_bit_field_type(type_spec.length),
                is_bit_field=True,
                bit_field_info={
                    'container': type_spec.container,
                    'start_bit': type_spec.start_bit,
                    'length': type_spec.length
                }
            )
        
        elif isinstance(type_spec, SimpleArrayType):
            element_c_type = self.get_c_type(type_spec.element_type)
            return GeneratedField(
                name=field_def.name,
                c_type=f"{element_c_type}_array_t *",
                is_array=True
            )
        
        elif isinstance(type_spec, TypeReference):
            c_type = self.get_c_type(type_spec.name)
            if type_spec.namespace:
                c_type = f"{type_spec.namespace}_{c_type}"
            
            return GeneratedField(
                name=field_def.name,
                c_type=c_type
            )
        
        else:
            # Default fallback
            return GeneratedField(
                name=field_def.name,
                c_type='uint8_t'
            )
    
    def generate_constant(self, name: str, const_def: ConstantType) -> Dict:
        """Generate constant definition"""
        return {
            'name': name.upper(),
            'type': self.get_c_type(const_def.type_ref),
            'value': const_def.value
        }
    
    def generate_header(self, data: Dict, filename: str = "generated") -> str:
        """Generate C header file"""
        
        template_str = '''#ifndef {{namespace|upper}}_{{filename|upper}}_H
#define {{namespace|upper}}_{{filename|upper}}_H

#include "ptk_array.h"
#include "ptk_allocator.h"
#include "ptk_buf.h"
#include "ptk_codec.h"
#include "ptk_err.h"
#include "ptk_log.h"
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Message Type Enumeration */
typedef enum {
{%- for message in messages %}
    {{message.name|upper}}_MESSAGE_TYPE = {{message.message_type_id}},
{%- endfor %}
} message_type_t;

/* Constants */
{%- for const in constants %}
#define {{const.name}} (({{const.type}}){{const.value}})
{%- endfor %}

{%- for message in messages %}

/* {{message.name}} message definition */
typedef struct {{message.name}}_t {
    const int message_type;
{%- for field in message.fields %}
    {{field.c_type}} {{field.name}};
{%- endfor %}
} {{message.name}}_t;

/* Constructor/Destructor */
ptk_err {{message.name}}_create(ptk_allocator_t *alloc, {{message.name}}_t **instance);
void {{message.name}}_dispose(ptk_allocator_t *alloc, {{message.name}}_t *instance);

/* Encode/Decode */
ptk_err {{message.name}}_encode(ptk_allocator_t *alloc, ptk_buf_t *buf, const {{message.name}}_t *instance);
ptk_err {{message.name}}_decode(ptk_allocator_t *alloc, {{message.name}}_t **instance, ptk_buf_t *buf);

{%- if message.has_arrays %}
/* Array Accessors */
{%- for field in message.fields %}
{%- if field.is_array %}
ptk_err {{message.name}}_get_{{field.name}}_element(const {{message.name}}_t *msg, size_t index, {{field.c_type.replace('_array_t *', '')}} *value);
ptk_err {{message.name}}_set_{{field.name}}_element({{message.name}}_t *msg, size_t index, {{field.c_type.replace('_array_t *', '')}} value);
size_t {{message.name}}_get_{{field.name}}_length(const {{message.name}}_t *msg);
{%- endif %}
{%- endfor %}
{%- endif %}

{%- if message.has_bit_fields %}
/* Bit Field Accessors */
{%- for field in message.fields %}
{%- if field.is_bit_field %}
static inline {{field.c_type}} {{message.name}}_get_{{field.name}}(const {{message.name}}_t *msg) {
    return (msg->{{field.bit_field_info.container}} >> {{field.bit_field_info.start_bit}}) & ((1 << {{field.bit_field_info.length}}) - 1);
}

static inline void {{message.name}}_set_{{field.name}}({{message.name}}_t *msg, {{field.c_type}} value) {
    msg->{{field.bit_field_info.container}} &= ~(((1 << {{field.bit_field_info.length}}) - 1) << {{field.bit_field_info.start_bit}});
    msg->{{field.bit_field_info.container}} |= (value & ((1 << {{field.bit_field_info.length}}) - 1)) << {{field.bit_field_info.start_bit}};
    msg->{{field.name}} = value;
}
{%- endif %}
{%- endfor %}
{%- endif %}

{%- endfor %}

#ifdef __cplusplus
}
#endif

#endif /* {{namespace|upper}}_{{filename|upper}}_H */
'''
        
        from jinja2 import Template
        template = Template(template_str)
        return template.render(**data, filename=filename)

def parse_args():
    """Parse command line arguments"""
    parser = argparse.ArgumentParser(
        description="Protocol Toolkit Code Generator",
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog="""
Examples:
  %(prog)s modbus.pdl
  %(prog)s -o ./protocols -n eip protocols/ethernet_ip.pdl
  %(prog)s -I ./common *.pdl
        """
    )
    
    parser.add_argument('input_files', nargs='+', metavar='FILE',
                       help='PDL files to process')
    parser.add_argument('-o', '--output-dir', default='./generated',
                       help='Output directory for generated files (default: ./generated)')
    parser.add_argument('-n', '--namespace', 
                       help='C namespace prefix for generated code (default: derived from filename)')
    parser.add_argument('-H', '--header-only', action='store_true',
                       help='Generate header files only (no implementation)')
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
    
    # Initialize parser and transformer separately
    parser = Lark(PDL_GRAMMAR, parser='lalr')
    transformer = PDLTransformer()
    
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
            
            parse_tree = parser.parse(content)
            ast = transformer.transform(parse_tree)
            
            # Generate namespace from filename if not provided
            namespace = args.namespace or input_path.stem
            
            # Generate code
            codegen = CodeGenerator(namespace)
            generated_data = codegen.process_ast(ast)
            
            # Write header file
            header_content = codegen.generate_header(generated_data, input_path.stem)
            header_file = output_dir / f"{input_path.stem}.h"
            
            with open(header_file, 'w') as f:
                f.write(header_content)
            
            if args.verbose:
                print(f"Generated {header_file}")
            
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