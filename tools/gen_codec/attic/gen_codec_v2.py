#!/usr/bin/env python3
"""
Protocol Toolkit Code Generator v2

Simplified implementation focusing on the core functionality.
"""

import argparse
import os
import sys
from pathlib import Path
from typing import Dict, List, Optional, Union, Any
from dataclasses import dataclass
import re

# Simple PDL parser using regex
class SimplePDLParser:
    """Simple regex-based PDL parser for basic structures"""
    
    def __init__(self):
        self.definitions = {}
        self.constants = {}
        
    def parse_file(self, content: str) -> Dict:
        """Parse PDL file content"""
        results = {
            'imports': [],
            'definitions': [],
            'constants': []
        }
        
        # Remove comments
        content = re.sub(r'//.*$', '', content, flags=re.MULTILINE)
        
        # Find all definitions
        def_pattern = r'def\s+(\w+)\s*=\s*\{([^}]+)\}'
        for match in re.finditer(def_pattern, content, re.MULTILINE | re.DOTALL):
            name = match.group(1)
            body = match.group(2).strip()
            
            definition = self.parse_definition_body(name, body)
            if definition:
                results['definitions'].append(definition)
        
        return results
    
    def parse_definition_body(self, name: str, body: str) -> Optional[Dict]:
        """Parse the body of a definition"""
        
        # Parse key-value pairs
        attrs = {}
        
        # Simple attribute parsing
        attr_pattern = r'(\w+)\s*:\s*([^,}]+)'
        for match in re.finditer(attr_pattern, body):
            key = match.group(1).strip()
            value = match.group(2).strip()
            attrs[key] = self.parse_value(value)
        
        # Determine type based on attributes
        if 'type' in attrs:
            type_val = attrs['type']
            
            if type_val == 'message':
                return {
                    'name': name,
                    'type': 'message',
                    'fields': self.parse_fields(attrs.get('fields', '[]'))
                }
            elif type_val == 'bit_field':
                return {
                    'name': name,
                    'type': 'bit_field',
                    'source': attrs.get('source', {})
                }
            elif type_val in ['u8', 'u16', 'u32', 'u64', 'i8', 'i16', 'i32', 'i64', 'f32', 'f64']:
                result = {
                    'name': name,
                    'type': 'primitive',
                    'base_type': type_val
                }
                if 'byte_order' in attrs:
                    result['byte_order'] = attrs['byte_order']
                if 'const' in attrs:
                    result['const'] = attrs['const']
                return result
            else:
                # Type reference or constant
                if 'const' in attrs:
                    return {
                        'name': name,
                        'type': 'constant',
                        'type_ref': type_val,
                        'value': attrs['const']
                    }
                else:
                    return {
                        'name': name,
                        'type': 'type_ref',
                        'ref': type_val
                    }
        
        return None
    
    def parse_value(self, value: str) -> Any:
        """Parse a value (string, number, array, etc.)"""
        value = value.strip()
        
        # Array of numbers like [0, 1, 2, 3]
        if value.startswith('[') and value.endswith(']'):
            inner = value[1:-1].strip()
            if inner:
                items = [item.strip() for item in inner.split(',')]
                try:
                    return [int(item) for item in items]
                except ValueError:
                    return items
            return []
        
        # Hex number
        if value.startswith('0x'):
            try:
                return int(value, 16)
            except ValueError:
                return value
        
        # Decimal number
        try:
            return int(value)
        except ValueError:
            pass
        
        # Float
        try:
            return float(value)
        except ValueError:
            pass
        
        # String literal
        if value.startswith('"') and value.endswith('"'):
            return value[1:-1]
        
        # Boolean
        if value in ['true', 'false']:
            return value == 'true'
        
        # Object like { container: flags, start_bit: 0, length: 1 }
        if value.startswith('{') and value.endswith('}'):
            inner = value[1:-1].strip()
            obj = {}
            attr_pattern = r'(\w+)\s*:\s*([^,}]+)'
            for match in re.finditer(attr_pattern, inner):
                k = match.group(1).strip()
                v = match.group(2).strip()
                obj[k] = self.parse_value(v)
            return obj
        
        # Raw identifier or expression
        return value
    
    def parse_fields(self, fields_str: str) -> List[Dict]:
        """Parse fields array"""
        fields_str = fields_str.strip()
        if not fields_str.startswith('[') or not fields_str.endswith(']'):
            return []
        
        inner = fields_str[1:-1].strip()
        if not inner:
            return []
        
        fields = []
        
        # Split on commas, but handle nested structures
        field_parts = []
        depth = 0
        current = ""
        
        for char in inner:
            if char in '{[':
                depth += 1
            elif char in '}]':
                depth -= 1
            elif char == ',' and depth == 0:
                field_parts.append(current.strip())
                current = ""
                continue
            current += char
        
        if current.strip():
            field_parts.append(current.strip())
        
        for field_part in field_parts:
            field_part = field_part.strip()
            if ':' in field_part:
                name_part, type_part = field_part.split(':', 1)
                name = name_part.strip()
                type_spec = type_part.strip()
                
                field = {'name': name}
                
                # Parse type specification
                if type_spec.startswith('{') and type_spec.endswith('}'):
                    # Inline type definition
                    type_body = type_spec[1:-1].strip()
                    type_def = self.parse_definition_body(name + "_type", type_body)
                    if type_def:
                        field.update(type_def)
                        field['name'] = name  # Restore original name
                else:
                    # Check for array syntax like u8[length]
                    array_match = re.match(r'(\w+)\[([^\]]+)\]', type_spec)
                    if array_match:
                        element_type = array_match.group(1)
                        size = array_match.group(2).strip()
                        field['type'] = 'array'
                        field['element_type'] = element_type
                        field['size'] = size
                    else:
                        # Simple type reference
                        field['type'] = 'type_ref'
                        field['ref'] = type_spec
                
                fields.append(field)
        
        return fields

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

class CodeGenerator:
    """Generates C code from parsed PDL"""
    
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
    
    def process(self, parsed_data: Dict) -> Dict:
        """Process parsed data and generate code structures"""
        
        generated = {
            'messages': [],
            'constants': [],
            'namespace': self.namespace
        }
        
        for defn in parsed_data['definitions']:
            if defn['type'] == 'message':
                msg = self.generate_message(defn)
                generated['messages'].append(msg)
                self.message_type_counter += 1
            elif defn['type'] == 'constant':
                const = self.generate_constant(defn)
                generated['constants'].append(const)
        
        return generated
    
    def generate_message(self, msg_def: Dict) -> GeneratedMessage:
        """Generate message structure"""
        
        fields = []
        has_arrays = False
        has_bit_fields = False
        
        for field_def in msg_def.get('fields', []):
            field = self.generate_field(field_def)
            fields.append(field)
            
            if field.is_array:
                has_arrays = True
            if field.is_bit_field:
                has_bit_fields = True
        
        return GeneratedMessage(
            name=msg_def['name'],
            fields=fields,
            message_type_id=self.message_type_counter,
            has_arrays=has_arrays,
            has_bit_fields=has_bit_fields
        )
    
    def generate_field(self, field_def: Dict) -> GeneratedField:
        """Generate field structure"""
        
        name = field_def['name']
        field_type = field_def.get('type', 'type_ref')
        
        if field_type == 'bit_field':
            source = field_def.get('source', {})
            length = source.get('length', 1)
            return GeneratedField(
                name=name,
                c_type=self.get_bit_field_type(length),
                is_bit_field=True,
                bit_field_info={
                    'container': source.get('container', ''),
                    'start_bit': source.get('start_bit', 0),
                    'length': length
                }
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
        
        else:
            # Default fallback
            return GeneratedField(
                name=name,
                c_type='uint8_t'
            )
    
    def generate_constant(self, const_def: Dict) -> Dict:
        """Generate constant definition"""
        return {
            'name': const_def['name'].upper(),
            'type': self.get_c_type(const_def.get('type_ref', 'u32')),
            'value': const_def.get('value', 0)
        }
    
    def generate_header(self, data: Dict, filename: str = "generated") -> str:
        """Generate C header file"""
        
        template = f'''#ifndef {self.namespace.upper()}_{filename.upper()}_H
#define {self.namespace.upper()}_{filename.upper()}_H

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
extern "C" {{
#endif

/* Message Type Enumeration */
typedef enum {{'''

        for message in data['messages']:
            template += f'''
    {message.name.upper()}_MESSAGE_TYPE = {message.message_type_id},'''

        template += '''
} message_type_t;

/* Constants */'''

        for const in data['constants']:
            template += f'''
#define {const['name']} (({const['type']}){const['value']})'''

        template += '\n'

        for message in data['messages']:
            template += f'''
/* {message.name} message definition */
typedef struct {message.name}_t {{
    const int message_type;'''
            
            for field in message.fields:
                template += f'''
    {field.c_type} {field.name};'''
            
            template += f'''
}} {message.name}_t;

/* Constructor/Destructor */
ptk_err {message.name}_create(ptk_allocator_t *alloc, {message.name}_t **instance);
void {message.name}_dispose(ptk_allocator_t *alloc, {message.name}_t *instance);

/* Encode/Decode */
ptk_err {message.name}_encode(ptk_allocator_t *alloc, ptk_buf_t *buf, const {message.name}_t *instance);
ptk_err {message.name}_decode(ptk_allocator_t *alloc, {message.name}_t **instance, ptk_buf_t *buf);'''

            if message.has_arrays:
                template += f'''

/* Array Accessors */'''
                for field in message.fields:
                    if field.is_array:
                        element_type = field.c_type.replace('_array_t *', '')
                        template += f'''
ptk_err {message.name}_get_{field.name}_element(const {message.name}_t *msg, size_t index, {element_type} *value);
ptk_err {message.name}_set_{field.name}_element({message.name}_t *msg, size_t index, {element_type} value);
size_t {message.name}_get_{field.name}_length(const {message.name}_t *msg);'''

            if message.has_bit_fields:
                template += f'''

/* Bit Field Accessors */'''
                for field in message.fields:
                    if field.is_bit_field:
                        info = field.bit_field_info
                        template += f'''
static inline {field.c_type} {message.name}_get_{field.name}(const {message.name}_t *msg) {{
    return (msg->{info['container']} >> {info['start_bit']}) & ((1 << {info['length']}) - 1);
}}

static inline void {message.name}_set_{field.name}({message.name}_t *msg, {field.c_type} value) {{
    msg->{info['container']} &= ~(((1 << {info['length']}) - 1) << {info['start_bit']});
    msg->{info['container']} |= (value & ((1 << {info['length']}) - 1)) << {info['start_bit']};
    msg->{field.name} = value;
}}'''

        template += '''

#ifdef __cplusplus
}
#endif

#endif /* ''' + f'{self.namespace.upper()}_{filename.upper()}_H' + ''' */
'''

        return template

def parse_args():
    """Parse command line arguments"""
    parser = argparse.ArgumentParser(
        description="Protocol Toolkit Code Generator v2",
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
    parser.add_argument('--version', action='version', version='%(prog)s 2.0.0')
    
    return parser.parse_args()

def main():
    """Main entry point"""
    args = parse_args()
    
    # Create output directory
    output_dir = Path(args.output_dir)
    output_dir.mkdir(parents=True, exist_ok=True)
    
    # Initialize parser
    parser = SimplePDLParser()
    
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
            
            parsed_data = parser.parse_file(content)
            
            if args.verbose:
                print(f"Parsed {len(parsed_data['definitions'])} definitions")
            
            # Generate namespace from filename if not provided
            namespace = args.namespace or input_path.stem
            
            # Generate code
            codegen = CodeGenerator(namespace)
            generated_data = codegen.process(parsed_data)
            
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