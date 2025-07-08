# Protocol Toolkit Code Generator Documentation

This directory contains the specification and documentation for the PDL (Protocol Definition Language) code generator.

## Files

- **`gen_codec_specifications.md`** - Complete specification for the code generator implementation
- **`pdl_syntax.md`** - PDL syntax examples and field patterns  
- **`pdl_grammar.ebnf`** - Formal EBNF grammar definition for PDL
- **`simplified_schema.md`** - Simplified schema for direct struct generation

## Overview

The PDL code generator transforms Protocol Definition Language files into type-safe C code for parsing and generating binary protocol messages. It generates direct struct access patterns with safe array handling and bit field manipulation for industrial communication protocols.

## Key Features

- **Direct Struct Access**: Generated structs with direct field access (no vtables)
- **Type Safety**: Bounds-checked array accessors and type-specific functions
- **Bit Fields**: Automatic type sizing and safe bit field extraction
- **Memory Safety**: Allocator-based memory management with arena patterns
- **Protocol Agnostic**: Support for different endianness, container types, and bit patterns

## Implementation

The generator is implemented in Python 3.8+ using the Lark parsing library for robust PDL parsing and Jinja2-style templates for C code generation.