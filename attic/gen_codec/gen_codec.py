#!/usr/bin/env python3

import re
import sys
import os
from typing import List, Dict, Tuple, Optional

class Field:
    def __init__(self, field_type: str, name: str, array_size: Optional[str] = None, is_pointer: bool = False):
        self.field_type = field_type
        self.name = name
        self.array_size = array_size
        self.is_pointer = is_pointer

    def __repr__(self):
        return f"Field({self.field_type}, {self.name}, array={self.array_size}, ptr={self.is_pointer})"

class Struct:
    def __init__(self, name: str, fields: List[Field]):
        self.name = name
        self.fields = fields

    def __repr__(self):
        return f"Struct({self.name}, {self.fields})"

class CodeGenerator:
    def __init__(self):
        self.structs: List[Struct] = []
        self.defines: List[str] = []
        self.used_types: set = set()

        # Type mappings
        self.codec_types = {
            'codec_u8': 'uint8_t',
            'codec_i8': 'int8_t',
            'codec_u16_be': 'uint16_t',
            'codec_i16_be': 'int16_t',
            'codec_u16_le': 'uint16_t',
            'codec_i16_le': 'int16_t',
            'codec_u32_be': 'uint32_t',
            'codec_i32_be': 'int32_t',
            'codec_u32_be_bs': 'uint32_t',
            'codec_i32_be_bs': 'int32_t',
            'codec_u32_le': 'uint32_t',
            'codec_i32_le': 'int32_t',
            'codec_u32_le_bs': 'uint32_t',
            'codec_i32_le_bs': 'int32_t',
            'codec_u64_be': 'uint64_t',
            'codec_i64_be': 'int64_t',
            'codec_u64_be_bs': 'uint64_t',
            'codec_i64_be_bs': 'int64_t',
            'codec_u64_le': 'uint64_t',
            'codec_i64_le': 'int64_t',
            'codec_u64_le_bs': 'uint64_t',
            'codec_i64_le_bs': 'int64_t',
            'codec_f32_be': 'float',
            'codec_f32_be_bs': 'float',
            'codec_f32_le': 'float',
            'codec_f32_le_bs': 'float',
            'codec_f64_be': 'double',
            'codec_f64_be_bs': 'double',
            'codec_f64_le': 'double',
            'codec_f64_le_bs': 'double',
        }

        self.type_sizes = {
            'codec_u8': 1, 'codec_i8': 1,
            'codec_u16_be': 2, 'codec_i16_be': 2, 'codec_u16_le': 2, 'codec_i16_le': 2,
            'codec_u32_be': 4, 'codec_i32_be': 4, 'codec_u32_be_bs': 4, 'codec_i32_be_bs': 4,
            'codec_u32_le': 4, 'codec_i32_le': 4, 'codec_u32_le_bs': 4, 'codec_i32_le_bs': 4,
            'codec_u64_be': 8, 'codec_i64_be': 8, 'codec_u64_be_bs': 8, 'codec_i64_be_bs': 8,
            'codec_u64_le': 8, 'codec_i64_le': 8, 'codec_u64_le_bs': 8, 'codec_i64_le_bs': 8,
            'codec_f32_be': 4, 'codec_f32_be_bs': 4, 'codec_f32_le': 4, 'codec_f32_le_bs': 4,
            'codec_f64_be': 8, 'codec_f64_be_bs': 8, 'codec_f64_le': 8, 'codec_f64_le_bs': 8,
        }

    def parse_file(self, filename: str):
        with open(filename, 'r') as f:
            content = f.read()

        # Remove comments
        content = re.sub(r'//.*?\n', '\n', content)
        content = re.sub(r'/\*.*?\*/', '', content, flags=re.DOTALL)

        # Extract #defines
        defines = re.findall(r'#define\s+[^\n]+', content)
        self.defines.extend(defines)

        # Extract structs
        struct_pattern = r'struct\s+(\w+)\s*\{([^}]+)\}'
        struct_matches = re.findall(struct_pattern, content, re.DOTALL)

        for struct_name, struct_body in struct_matches:
            fields = self.parse_struct_fields(struct_body)
            self.structs.append(Struct(struct_name, fields))

            # Track used types
            for field in fields:
                if field.field_type in self.codec_types:
                    self.used_types.add(field.field_type)

    def parse_struct_fields(self, struct_body: str) -> List[Field]:
        fields = []

        # Split by semicolons and process each field
        field_lines = [line.strip() for line in struct_body.split(';') if line.strip()]

        for line in field_lines:
            # Match field pattern: type name[array] or type *name
            field_match = re.match(r'(\w+|\bstruct\s+\w+)\s+(\**)(\w+)(\[([^\]]+)\])?', line.strip())

            if field_match:
                field_type = field_match.group(1)
                pointers = field_match.group(2)
                field_name = field_match.group(3)
                array_bracket = field_match.group(4)
                array_size = field_match.group(5) if array_bracket else None

                is_pointer = len(pointers) > 0

                fields.append(Field(field_type, field_name, array_size, is_pointer))

        return fields

    def generate_header(self, base_filename: str) -> str:
        header = []

        # Header guards and includes
        guard = f"{base_filename.upper()}_H"
        header.append(f"#ifndef {guard}")
        header.append(f"#define {guard}")
        header.append("")
        header.append("#include <stdint.h>")
        header.append("#include <stddef.h>")
        header.append("#include <log.h>")
        header.append("#include <buf.h>")
        header.append("")

        # Error enum
        header.append("/* Error handling enum */")
        header.append("typedef enum {")
        header.append("    CODEC_OK = 0,                    // Success")
        header.append("    CODEC_ERR_OUT_OF_BOUNDS,         // Buffer bounds exceeded")
        header.append("    CODEC_ERR_NULL_PTR,              // Null pointer passed")
        header.append("    CODEC_ERR_NO_MEMORY,             // Memory allocation failed")
        header.append("} codec_err_t;")
        header.append("")

        # Type definitions for used types
        header.append("/* Codec type definitions */")
        for codec_type in sorted(self.used_types):
            if codec_type in self.codec_types:
                header.append(f"typedef {self.codec_types[codec_type]} {codec_type};")
        header.append("")

        # Original file content
        header.append("/* Original definitions */")
        for define in self.defines:
            header.append(define)

        if self.defines:
            header.append("")

        # Struct definitions
        for struct in self.structs:
            header.append(f"struct {struct.name} {{")
            for field in struct.fields:
                if field.is_pointer:
                    if field.array_size:
                        header.append(f"    {field.field_type} *{field.name}[{field.array_size}];")
                    else:
                        header.append(f"    {field.field_type} *{field.name};")
                elif field.array_size:
                    header.append(f"    {field.field_type} {field.name}[{field.array_size}];")
                else:
                    header.append(f"    {field.field_type} {field.name};")
            header.append("};")
            header.append("")

        # Function declarations
        header.append("/* Generated function declarations */")
        for struct in self.structs:
            header.append(f"/* functions for struct {struct.name} */")
            header.append(f"codec_err_t {struct.name}_decode(struct {struct.name} **value, buf *remaining_input_buf, buf *input_buf);")
            header.append(f"codec_err_t {struct.name}_encode(buf *remaining_output_buf, buf *output_buf, const struct {struct.name} *value);")
            header.append(f"void {struct.name}_dispose(struct {struct.name} *value);")
            header.append(f"void {struct.name}_log_impl(const char *func, int line_num, ptk_log_level log_level, struct {struct.name} *value);")
            header.append("")

            # User-defined functions for pointer fields
            for field in struct.fields:
                if field.is_pointer:
                    header.append(f"/* user-defined functions for pointer field {field.name} */")
                    header.append(f"codec_err_t {struct.name}_{field.name}_decode(struct {struct.name} *value, buf *remaining_input_buf, buf *input_buf);")
                    header.append(f"codec_err_t {struct.name}_{field.name}_encode(buf *remaining_output_buf, buf *output_buf, const struct {struct.name} *value);")
                    header.append(f"void {struct.name}_{field.name}_dispose(struct {struct.name} *value);")
                    header.append(f"void {struct.name}_{field.name}_log_impl(const char *func, int line_num, ptk_log_level log_level, struct {struct.name} *value);")
                    header.append("")

        # Logging macros
        header.append("/* Logging macros */")
        for struct in self.structs:
            header.append(f"/* logging macros for struct {struct.name} */")
            for level in ['error', 'warn', 'info', 'debug', 'trace']:
                header.append(f"#define {struct.name}_log_{level}(value) do {{ if(ptk_log_level_get() <= PTK_LOG_LEVEL_{level.upper()}) {struct.name}_log_impl(__func__, __LINE__, PTK_LOG_LEVEL_{level.upper()}, value); }} while(0)")
            header.append("")

            # Macros for pointer fields
            for field in struct.fields:
                if field.is_pointer:
                    header.append(f"/* logging macros for pointer field {field.name} */")
                    for level in ['error', 'warn', 'info', 'debug', 'trace']:
                        header.append(f"#define {struct.name}_{field.name}_log_{level}(value) do {{ if(ptk_log_level_get() <= PTK_LOG_LEVEL_{level.upper()}) {struct.name}_{field.name}_log_impl(__func__, __LINE__, PTK_LOG_LEVEL_{level.upper()}, value); }} while(0)")
                    header.append("")

        header.append(f"#endif /* {guard} */")

        return '\n'.join(header)

    def get_decode_code(self, field_type: str, var_name: str, offset_var: str) -> List[str]:
        """Generate decode code for a specific type"""
        code = []
        size = self.type_sizes.get(field_type, 0)

        if field_type in ['codec_u8', 'codec_i8']:
            code.append(f"    {{")
            code.append(f"        buf_err_t err = buf_get_u8((uint8_t*)&{var_name}, input_buf, {offset_var});")
            code.append(f"        if (err != BUF_OK) {{")
            code.append(f"            if (err == BUF_ERR_OUT_OF_BOUNDS) return CODEC_ERR_OUT_OF_BOUNDS;")
            code.append(f"            if (err == BUF_ERR_NULL_PTR) return CODEC_ERR_NULL_PTR;")
            code.append(f"        }}")
            code.append(f"        {offset_var} += 1;")
            code.append(f"    }}")
        elif size > 1:
            # Multi-byte types need byte-by-byte reading
            code.append(f"    {{")
            code.append(f"        uint8_t bytes[{size}];")
            code.append(f"        for(int i = 0; i < {size}; i++) {{")
            code.append(f"            buf_err_t err = buf_get_u8(&bytes[i], input_buf, {offset_var} + i);")
            code.append(f"            if (err != BUF_OK) {{")
            code.append(f"                if (err == BUF_ERR_OUT_OF_BOUNDS) return CODEC_ERR_OUT_OF_BOUNDS;")
            code.append(f"                if (err == BUF_ERR_NULL_PTR) return CODEC_ERR_NULL_PTR;")
            code.append(f"            }}")
            code.append(f"        }}")

            # Byte order handling
            if '_be' in field_type and '_bs' not in field_type:
                # Big endian
                if size == 2:
                    code.append(f"        {var_name} = (bytes[0] << 8) | bytes[1];")
                elif size == 4:
                    code.append(f"        {var_name} = (bytes[0] << 24) | (bytes[1] << 16) | (bytes[2] << 8) | bytes[3];")
                elif size == 8:
                    code.append(f"        {var_name} = ((uint64_t)bytes[0] << 56) | ((uint64_t)bytes[1] << 48) | ((uint64_t)bytes[2] << 40) | ((uint64_t)bytes[3] << 32) |")
                    code.append(f"                    ((uint64_t)bytes[4] << 24) | ((uint64_t)bytes[5] << 16) | ((uint64_t)bytes[6] << 8) | (uint64_t)bytes[7];")
            elif '_le' in field_type and '_bs' not in field_type:
                # Little endian
                if size == 2:
                    code.append(f"        {var_name} = bytes[0] | (bytes[1] << 8);")
                elif size == 4:
                    code.append(f"        {var_name} = bytes[0] | (bytes[1] << 8) | (bytes[2] << 16) | (bytes[3] << 24);")
                elif size == 8:
                    code.append(f"        {var_name} = (uint64_t)bytes[0] | ((uint64_t)bytes[1] << 8) | ((uint64_t)bytes[2] << 16) | ((uint64_t)bytes[3] << 24) |")
                    code.append(f"                    ((uint64_t)bytes[4] << 32) | ((uint64_t)bytes[5] << 40) | ((uint64_t)bytes[6] << 48) | ((uint64_t)bytes[7] << 56);")
            elif '_be_bs' in field_type:
                # Big endian byte swapped
                if size == 4:
                    code.append(f"        {var_name} = (bytes[2] << 24) | (bytes[3] << 16) | (bytes[1] << 8) | bytes[0];")
                elif size == 8:
                    code.append(f"        {var_name} = ((uint64_t)bytes[6] << 56) | ((uint64_t)bytes[7] << 48) | ((uint64_t)bytes[4] << 40) | ((uint64_t)bytes[5] << 32) |")
                    code.append(f"                    ((uint64_t)bytes[2] << 24) | ((uint64_t)bytes[3] << 16) | ((uint64_t)bytes[0] << 8) | (uint64_t)bytes[1];")
            elif '_le_bs' in field_type:
                # Little endian byte swapped
                if size == 4:
                    code.append(f"        {var_name} = bytes[1] | (bytes[0] << 8) | (bytes[3] << 16) | (bytes[2] << 24);")
                elif size == 8:
                    code.append(f"        {var_name} = (uint64_t)bytes[1] | ((uint64_t)bytes[0] << 8) | ((uint64_t)bytes[3] << 16) | ((uint64_t)bytes[2] << 24) |")
                    code.append(f"                    ((uint64_t)bytes[5] << 32) | ((uint64_t)bytes[4] << 40) | ((uint64_t)bytes[7] << 48) | ((uint64_t)bytes[6] << 56);")

            # Handle float types
            if 'f32' in field_type or 'f64' in field_type:
                union_type = 'uint32_t' if 'f32' in field_type else 'uint64_t'
                float_type = 'float' if 'f32' in field_type else 'double'
                code.append(f"        union {{ {union_type} i; {float_type} f; }} u;")
                code.append(f"        u.i = *(({union_type}*)&{var_name});")
                code.append(f"        {var_name} = u.f;")

            code.append(f"        {offset_var} += {size};")
            code.append(f"    }}")

        return code

    def get_encode_code(self, field_type: str, var_name: str, offset_var: str) -> List[str]:
        """Generate encode code for a specific type"""
        code = []
        size = self.type_sizes.get(field_type, 0)

        if field_type in ['codec_u8', 'codec_i8']:
            code.append(f"    {{")
            code.append(f"        buf_err_t err = buf_set_u8(output_buf, {offset_var}, (uint8_t){var_name});")
            code.append(f"        if (err != BUF_OK) {{")
            code.append(f"            if (err == BUF_ERR_OUT_OF_BOUNDS) return CODEC_ERR_OUT_OF_BOUNDS;")
            code.append(f"            if (err == BUF_ERR_NULL_PTR) return CODEC_ERR_NULL_PTR;")
            code.append(f"        }}")
            code.append(f"        {offset_var} += 1;")
            code.append(f"    }}")
        elif size > 1:
            code.append(f"    {{")
            code.append(f"        uint8_t bytes[{size}];")

            # Handle float types first
            if 'f32' in field_type or 'f64' in field_type:
                union_type = 'uint32_t' if 'f32' in field_type else 'uint64_t'
                float_type = 'float' if 'f32' in field_type else 'double'
                code.append(f"        union {{ {union_type} i; {float_type} f; }} u;")
                code.append(f"        u.f = {var_name};")
                actual_var = "u.i"
            else:
                actual_var = var_name

            # Byte order handling
            if '_be' in field_type and '_bs' not in field_type:
                # Big endian
                if size == 2:
                    code.append(f"        bytes[0] = ({actual_var} >> 8) & 0xFF;")
                    code.append(f"        bytes[1] = {actual_var} & 0xFF;")
                elif size == 4:
                    code.append(f"        bytes[0] = ({actual_var} >> 24) & 0xFF;")
                    code.append(f"        bytes[1] = ({actual_var} >> 16) & 0xFF;")
                    code.append(f"        bytes[2] = ({actual_var} >> 8) & 0xFF;")
                    code.append(f"        bytes[3] = {actual_var} & 0xFF;")
                elif size == 8:
                    for i in range(8):
                        shift = (7-i) * 8
                        code.append(f"        bytes[{i}] = ({actual_var} >> {shift}) & 0xFF;")
            elif '_le' in field_type and '_bs' not in field_type:
                # Little endian
                if size == 2:
                    code.append(f"        bytes[0] = {actual_var} & 0xFF;")
                    code.append(f"        bytes[1] = ({actual_var} >> 8) & 0xFF;")
                elif size == 4:
                    code.append(f"        bytes[0] = {actual_var} & 0xFF;")
                    code.append(f"        bytes[1] = ({actual_var} >> 8) & 0xFF;")
                    code.append(f"        bytes[2] = ({actual_var} >> 16) & 0xFF;")
                    code.append(f"        bytes[3] = ({actual_var} >> 24) & 0xFF;")
                elif size == 8:
                    for i in range(8):
                        shift = i * 8
                        code.append(f"        bytes[{i}] = ({actual_var} >> {shift}) & 0xFF;")
            elif '_be_bs' in field_type:
                # Big endian byte swapped
                if size == 4:
                    code.append(f"        bytes[2] = ({actual_var} >> 24) & 0xFF;")
                    code.append(f"        bytes[3] = ({actual_var} >> 16) & 0xFF;")
                    code.append(f"        bytes[1] = ({actual_var} >> 8) & 0xFF;")
                    code.append(f"        bytes[0] = {actual_var} & 0xFF;")
                elif size == 8:
                    byte_order = [6, 7, 4, 5, 2, 3, 0, 1]
                    for i, byte_idx in enumerate(byte_order):
                        shift = (7-i) * 8
                        code.append(f"        bytes[{byte_idx}] = ({actual_var} >> {shift}) & 0xFF;")
            elif '_le_bs' in field_type:
                # Little endian byte swapped
                if size == 4:
                    code.append(f"        bytes[1] = {actual_var} & 0xFF;")
                    code.append(f"        bytes[0] = ({actual_var} >> 8) & 0xFF;")
                    code.append(f"        bytes[3] = ({actual_var} >> 16) & 0xFF;")
                    code.append(f"        bytes[2] = ({actual_var} >> 24) & 0xFF;")
                elif size == 8:
                    byte_order = [1, 0, 3, 2, 5, 4, 7, 6]
                    for i, byte_idx in enumerate(byte_order):
                        shift = i * 8
                        code.append(f"        bytes[{byte_idx}] = ({actual_var} >> {shift}) & 0xFF;")

            code.append(f"        for(int i = 0; i < {size}; i++) {{")
            code.append(f"            buf_err_t err = buf_set_u8(output_buf, {offset_var} + i, bytes[i]);")
            code.append(f"            if (err != BUF_OK) {{")
            code.append(f"                if (err == BUF_ERR_OUT_OF_BOUNDS) return CODEC_ERR_OUT_OF_BOUNDS;")
            code.append(f"                if (err == BUF_ERR_NULL_PTR) return CODEC_ERR_NULL_PTR;")
            code.append(f"            }}")
            code.append(f"        }}")
            code.append(f"        {offset_var} += {size};")
            code.append(f"    }}")

        return code

    def generate_implementation(self, base_filename: str) -> str:
        impl = []

        impl.append(f'#include "{base_filename}.h"')
        impl.append("#include <stdlib.h>")
        impl.append("#include <string.h>")
        impl.append("")

        for struct in self.structs:
            # Decode function
            impl.append(f"codec_err_t {struct.name}_decode(struct {struct.name} **value, buf *remaining_input_buf, buf *input_buf) {{")
            impl.append(f"    if (!value || !remaining_input_buf || !input_buf) {{")
            impl.append(f"        return CODEC_ERR_NULL_PTR;")
            impl.append(f"    }}")
            impl.append("")
            impl.append(f"    struct {struct.name} *result = malloc(sizeof(struct {struct.name}));")
            impl.append(f"    if (!result) {{")
            impl.append(f"        return CODEC_ERR_NO_MEMORY;")
            impl.append(f"    }}")
            impl.append(f"    memset(result, 0, sizeof(struct {struct.name}));")
            impl.append("")
            impl.append(f"    size_t offset = 0;")
            impl.append("")

            # Log input data
            impl.append(f"    info(\"Decoding {struct.name} from input\");")
            impl.append(f"    info_buf(buf_data(input_buf), buf_len(input_buf));")
            impl.append("")

            for field in struct.fields:
                if field.is_pointer:
                    impl.append(f"    // User-defined decode for pointer field {field.name}")
                    impl.append(f"    codec_err_t {field.name}_err = {struct.name}_{field.name}_decode(result, remaining_input_buf, input_buf);")
                    impl.append(f"    if ({field.name}_err != CODEC_OK) {{")
                    impl.append(f"        {struct.name}_dispose(result);")
                    impl.append(f"        return {field.name}_err;")
                    impl.append(f"    }}")
                elif field.array_size:
                    impl.append(f"    // Array field {field.name}")
                    impl.append(f"    for (int i = 0; i < ({field.array_size}); i++) {{")
                    if field.field_type.startswith('struct '):
                        struct_type = field.field_type.replace('struct ', '')
                        impl.append(f"        struct {struct_type} *elem_ptr;")
                        impl.append(f"        codec_err_t elem_err = {struct_type}_decode(&elem_ptr, remaining_input_buf, input_buf);")
                        impl.append(f"        if (elem_err != CODEC_OK) {{")
                        impl.append(f"            {struct.name}_dispose(result);")
                        impl.append(f"            return elem_err;")
                        impl.append(f"        }}")
                        impl.append(f"        result->{field.name}[i] = *elem_ptr;")
                        impl.append(f"        free(elem_ptr);")
                    else:
                        decode_lines = self.get_decode_code(field.field_type, f"result->{field.name}[i]", "offset")
                        for line in decode_lines:
                            impl.append(f"    {line}")
                    impl.append(f"    }}")
                else:
                    if field.field_type.startswith('struct '):
                        struct_type = field.field_type.replace('struct ', '')
                        impl.append(f"    // Embedded struct field {field.name}")
                        impl.append(f"    struct {struct_type} *{field.name}_ptr;")
                        impl.append(f"    codec_err_t {field.name}_err = {struct_type}_decode(&{field.name}_ptr, remaining_input_buf, input_buf);")
                        impl.append(f"    if ({field.name}_err != CODEC_OK) {{")
                        impl.append(f"        {struct.name}_dispose(result);")
                        impl.append(f"        return {field.name}_err;")
                        impl.append(f"    }}")
                        impl.append(f"    result->{field.name} = *{field.name}_ptr;")
                        impl.append(f"    free({field.name}_ptr);")
                    else:
                        decode_lines = self.get_decode_code(field.field_type, f"result->{field.name}", "offset")
                        for line in decode_lines:
                            impl.append(line)
                impl.append("")

            impl.append(f"    // Set remaining buffer")
            impl.append(f"    buf_err_t buf_err = buf_from_buf(remaining_input_buf, input_buf, offset, buf_len(input_buf) - offset);")
            impl.append(f"    if (buf_err != BUF_OK) {{")
            impl.append(f"        {struct.name}_dispose(result);")
            impl.append(f"        if (buf_err == BUF_ERR_OUT_OF_BOUNDS) return CODEC_ERR_OUT_OF_BOUNDS;")
            impl.append(f"        if (buf_err == BUF_ERR_NULL_PTR) return CODEC_ERR_NULL_PTR;")
            impl.append(f"    }}")
            impl.append("")
            impl.append(f"    *value = result;")
            impl.append(f"    {struct.name}_log_info(result);")
            impl.append(f"    return CODEC_OK;")
            impl.append(f"}}")
            impl.append("")

            # Encode function
            impl.append(f"codec_err_t {struct.name}_encode(buf *remaining_output_buf, buf *output_buf, const struct {struct.name} *value) {{")
            impl.append(f"    if (!remaining_output_buf || !output_buf || !value) {{")
            impl.append(f"        return CODEC_ERR_NULL_PTR;")
            impl.append(f"    }}")
            impl.append("")
            impl.append(f"    size_t offset = 0;")
            impl.append("")
            impl.append(f"    {struct.name}_log_info((struct {struct.name}*)value);")
            impl.append("")

            for field in struct.fields:
                if field.is_pointer:
                    impl.append(f"    // User-defined encode for pointer field {field.name}")
                    impl.append(f"    codec_err_t {field.name}_err = {struct.name}_{field.name}_encode(remaining_output_buf, output_buf, value);")
                    impl.append(f"    if ({field.name}_err != CODEC_OK) {{")
                    impl.append(f"        return {field.name}_err;")
                    impl.append(f"    }}")
                elif field.array_size:
                    impl.append(f"    // Array field {field.name}")
                    impl.append(f"    for (int i = 0; i < ({field.array_size}); i++) {{")
                    if field.field_type.startswith('struct '):
                        struct_type = field.field_type.replace('struct ', '')
                        impl.append(f"        codec_err_t elem_err = {struct_type}_encode(remaining_output_buf, output_buf, &value->{field.name}[i]);")
                        impl.append(f"        if (elem_err != CODEC_OK) {{")
                        impl.append(f"            return elem_err;")
                        impl.append(f"        }}")
                    else:
                        encode_lines = self.get_encode_code(field.field_type, f"value->{field.name}[i]", "offset")
                        for line in encode_lines:
                            impl.append(f"    {line}")
                    impl.append(f"    }}")
                else:
                    if field.field_type.startswith('struct '):
                        struct_type = field.field_type.replace('struct ', '')
                        impl.append(f"    // Embedded struct field {field.name}")
                        impl.append(f"    codec_err_t {field.name}_err = {struct_type}_encode(remaining_output_buf, output_buf, &value->{field.name});")
                        impl.append(f"    if ({field.name}_err != CODEC_OK) {{")
                        impl.append(f"        return {field.name}_err;")
                        impl.append(f"    }}")
                    else:
                        encode_lines = self.get_encode_code(field.field_type, f"value->{field.name}", "offset")
                        for line in encode_lines:
                            impl.append(line)
                impl.append("")

            impl.append(f"    // Set remaining buffer")
            impl.append(f"    buf_err_t buf_err = buf_from_buf(remaining_output_buf, output_buf, offset, buf_len(output_buf) - offset);")
            impl.append(f"    if (buf_err != BUF_OK) {{")
            impl.append(f"        if (buf_err == BUF_ERR_OUT_OF_BOUNDS) return CODEC_ERR_OUT_OF_BOUNDS;")
            impl.append(f"        if (buf_err == BUF_ERR_NULL_PTR) return CODEC_ERR_NULL_PTR;")
            impl.append(f"    }}")
            impl.append("")
            impl.append(f"    info(\"Encoded {struct.name} output\");")
            impl.append(f"    info_buf(buf_data(output_buf), offset);")
            impl.append(f"    return CODEC_OK;")
            impl.append(f"}}")
            impl.append("")

            # Dispose function
            impl.append(f"void {struct.name}_dispose(struct {struct.name} *value) {{")
            impl.append(f"    if (!value) {{")
            impl.append(f"        warn(\"Called {struct.name}_dispose with NULL pointer\");")
            impl.append(f"        return;")
            impl.append(f"    }}")
            impl.append("")
            impl.append(f"    info(\"Disposing {struct.name}\");")
            impl.append("")

            for field in struct.fields:
                if field.is_pointer:
                    impl.append(f"    {struct.name}_{field.name}_dispose(value);")
                elif field.array_size and field.field_type.startswith('struct '):
                    struct_type = field.field_type.replace('struct ', '')
                    impl.append(f"    for (int i = 0; i < ({field.array_size}); i++) {{")
                    impl.append(f"        {struct_type}_dispose(&value->{field.name}[i]);")
                    impl.append(f"    }}")
                elif field.field_type.startswith('struct '):
                    struct_type = field.field_type.replace('struct ', '')
                    impl.append(f"    {struct_type}_dispose(&value->{field.name});")

            impl.append(f"    free(value);")
            impl.append(f"}}")
            impl.append("")

            # Log function
            impl.append(f"void {struct.name}_log_impl(const char *func, int line_num, ptk_log_level log_level, struct {struct.name} *value) {{")
            impl.append(f"    if (!value) {{")
            impl.append(f"        ptk_log_impl(func, line_num, log_level, \"{struct.name}: NULL\");")
            impl.append(f"        return;")
            impl.append(f"    }}")
            impl.append("")

            for field in struct.fields:
                if field.is_pointer:
                    impl.append(f"    {struct.name}_{field.name}_log_impl(func, line_num, log_level, value);")
                elif field.array_size:
                    if field.field_type.startswith('struct '):
                        struct_type = field.field_type.replace('struct ', '')
                        impl.append(f"    for (int i = 0; i < ({field.array_size}); i++) {{")
                        impl.append(f"        {struct_type}_log_impl(func, line_num, log_level, &value->{field.name}[i]);")
                        impl.append(f"    }}")
                    else:
                        # Array logging with 16 elements per line
                        impl.append(f"    for (int i = 0; i < ({field.array_size}); i += 16) {{")
                        impl.append(f"        char buf[1024];")
                        impl.append(f"        int pos = 0;")
                        impl.append(f"        int end = (i + 16 < ({field.array_size})) ? i + 16 : ({field.array_size});")
                        impl.append(f"        pos += snprintf(buf + pos, sizeof(buf) - pos, \"{field.name}[%d-%d]: \", i, end - 1);")
                        impl.append(f"        for (int j = i; j < end; j++) {{")
                        if 'f32' in field.field_type or 'f64' in field.field_type:
                            impl.append(f"            pos += snprintf(buf + pos, sizeof(buf) - pos, \"%.6f \", value->{field.name}[j]);")
                        else:
                            impl.append(f"            pos += snprintf(buf + pos, sizeof(buf) - pos, \"0x%02X \", (unsigned char)value->{field.name}[j]);")
                        impl.append(f"        }}")
                        impl.append(f"        ptk_log_impl(func, line_num, log_level, \"%s\", buf);")
                        impl.append(f"    }}")
                elif field.field_type.startswith('struct '):
                    struct_type = field.field_type.replace('struct ', '')
                    impl.append(f"    {struct_type}_log_impl(func, line_num, log_level, &value->{field.name});")
                else:
                    if 'f32' in field.field_type or 'f64' in field.field_type:
                        impl.append(f"    ptk_log_impl(func, line_num, log_level, \"{field.name}: %.6f\", value->{field.name});")
                    else:
                        size = self.type_sizes.get(field.field_type, 1)
                        if size == 1:
                            impl.append(f"    ptk_log_impl(func, line_num, log_level, \"{field.name}: 0x%02X\", value->{field.name});")
                        elif size == 2:
                            impl.append(f"    ptk_log_impl(func, line_num, log_level, \"{field.name}: 0x%04X\", value->{field.name});")
                        elif size == 4:
                            impl.append(f"    ptk_log_impl(func, line_num, log_level, \"{field.name}: 0x%08X\", value->{field.name});")
                        elif size == 8:
                            impl.append(f"    ptk_log_impl(func, line_num, log_level, \"{field.name}: 0x%016llX\", (unsigned long long)value->{field.name});")

            impl.append(f"}}")
            impl.append("")

        return '\n'.join(impl)

def main():
    if len(sys.argv) != 2:
        print("Usage: gen_python.py <input_file.def>")
        sys.exit(1)

    input_file = sys.argv[1]
    base_name = os.path.splitext(os.path.basename(input_file))[0]

    generator = CodeGenerator()
    generator.parse_file(input_file)

    # Generate header file
    header_content = generator.generate_header(base_name)
    with open(f"{base_name}.h", 'w') as f:
        f.write(header_content)

    # Generate implementation file
    impl_content = generator.generate_implementation(base_name)
    with open(f"{base_name}.c", 'w') as f:
        f.write(impl_content)

    print(f"Generated {base_name}.h and {base_name}.c")

if __name__ == "__main__":
    main()