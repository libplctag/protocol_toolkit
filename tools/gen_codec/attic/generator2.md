# PDL2: Protocol Description Language (CUE-Based)

This document defines the **PDL2** format: a structured schema language for describing binary protocols using a CUE-inspired syntax.

PDL2 enables:

- Typed and ordered field layouts
- Discriminated unions (based on field values)
- Field constraints and hooks
- Byte-order remapping via `byte_order_map`
- Alias definitions for layout reuse

---

## Field Definition

A field may be defined as either:

### 1. Shorthand form

```cue
field1: uint16
```

- Treated as a field with type only
- No hooks, constraints, or remapping

### 2. Structured form

```cue
field1: {
  type: uint16
  byte_order_map: [1, 0]
  validate: field1 >= 0 && field1 <= 100
  pre_decode: decode_func
  post_encode: encode_func
}
```

- Allows per-field behavior
- Function hooks are emitted in code generation

---

## Aliases

Aliases define reusable layout patterns and byte mappings, without behavior.

```cue
aliases: {
  u16_le: { type: uint16, byte_order_map: [0, 1] }
  u16_be: { type: uint16, byte_order_map: [1, 0] }

  f32_plc: { type: float32, byte_order_map: [2, 3, 0, 1] }

  plc5_str_data: {
    type: [uint8; 82]
    byte_order_map: [
      1,0, 3,2, 5,4, 7,6, 9,8, 11,10, 13,12, 15,14,
      17,16, 19,18, 21,20, 23,22, 25,24, 27,26, 29,28,
      31,30, 33,32, 35,34, 37,36, 39,38, 41,40, 43,42,
      45,44, 47,46, 49,48, 51,50, 53,52, 55,54, 57,56,
      59,58, 61,60, 63,62, 65,64, 67,66, 69,68, 71,70,
      73,72, 75,74, 77,76, 79,78, 81,80
    ]
  }
}
```

Usage example:

```cue
field1: u16_be
field2: plc5_str_data
```

---

## Discriminated Unions

You can define variant layouts using a `oneof` structure gated by field values.

```cue
Message: {
  header: Header
  payload: {
    type: oneof {
      LoginRequest  | header.type == 0x01
      LoginReply    | header.type == 0x02
      DataMessage   | header.type == 0x03 && header.flag == 0xAA
    }
  }
}
```

Each variant is parsed/encoded when its condition matches.

---

## Full Example

```cue
Header: {
  type: uint16
  length: {
    type: uint16
    auto_size: "payload"
  }
  session_id: {
    type: u32_le
    post_decode: validate_session
  }
}

LoginRequest: {
  username_len: uint8
  username: [...uint8] & len(username) == username_len
}

LoginReply: {
  status: uint8
}

DataMessage: {
  record_count: uint8
  records: [...Record] & len(records) == record_count
}

Record: {
  tag_id: uint16
  value: f32_plc
}
```

---

## Grammar (EBNF)

```ebnf
pdl2           ::= { alias_block | type_decl }

alias_block    ::= "aliases:" "{" { alias_entry } "}"
alias_entry    ::= identifier ":" "{" "type:" type_name
                     [ "," "byte_order_map:" byte_map ]
                   "}"

type_decl      ::= identifier ":" struct_type
struct_type    ::= "{" { field_decl | union_decl } "}"

field_decl     ::= identifier ":" ( type_name | field_struct )

field_struct   ::= "{" "type:" type_name
                   [ "," "byte_order_map:" byte_map ]
                   [ "," "auto_size:" string ]
                   [ "," "size_from:" identifier ]
                   [ "," "validate:" expr ]
                   [ "," "const:" literal ]
                   [ "," "pre_decode:" identifier ]
                   [ "," "post_decode:" identifier ]
                   [ "," "pre_encode:" identifier ]
                   [ "," "post_encode:" identifier ]
                 "}"

union_decl     ::= identifier ":" "{" "type:" "oneof" "{" { variant_decl } "}" "}"
variant_decl   ::= type_name "|" expr

type_name      ::= identifier
byte_map       ::= "[" int { "," int } "]"
identifier     ::= letter { letter | digit | "_" }
string         ::= '"' { printable_char } '"'
literal        ::= int | string
int            ::= digit { digit }

expr           ::= disjunction
disjunction    ::= conjunction { "||" conjunction }
conjunction    ::= comparison { "&&" comparison }
comparison     ::= sum [ comp_op sum ]
comp_op        ::= "==" | "!=" | ">" | "<" | ">=" | "<="
sum            ::= term { ("+" | "-") term }
term           ::= factor { ("*" | "/") factor }
factor         ::= int | identifier | "(" expr ")"

letter         ::= "A".."Z" | "a".."z"
digit          ::= "0".."9"
printable_char ::= any ASCII character except '"' or newline
```

---

## Modbus TCP Example

```cue
// modbus_tcp.cue
package modbus

aliases: {
  u16_be: { type: uint16, byte_order_map: [1, 0] }
  u32_be: { type: uint32, byte_order_map: [3, 2, 1, 0] }
}

ModbusTCP: {
  header: MBAPHeader
  pdu: {
    type: oneof {
      ReadBitsRequest     | pdu.function_code == 0x01 || pdu.function_code == 0x02
      ReadRegistersRequest| pdu.function_code == 0x03 || pdu.function_code == 0x04
      WriteCoilRequest    | pdu.function_code == 0x05
      WriteRegRequest     | pdu.function_code == 0x06
      WriteCoilsRequest   | pdu.function_code == 0x0F
      WriteRegsRequest    | pdu.function_code == 0x10
    }
  }
}

MBAPHeader: {
  transaction_id: u16_be
  protocol_id:    u16_be & const: 0
  length:         u16_be & auto_size: "pdu"
  unit_id:        uint8
}

ReadBitsRequest: {
  function_code: uint8 & (== 0x01 || == 0x02)
  address:       u16_be
  quantity:      u16_be
}

ReadRegistersRequest: {
  function_code: uint8 & (== 0x03 || == 0x04)
  address:       u16_be
  quantity:      u16_be
}

WriteCoilRequest: {
  function_code: uint8 & == 0x05
  address:       u16_be
  value:         u16_be
}

WriteRegRequest: {
  function_code: uint8 & == 0x06
  address:       u16_be
  value:         u16_be
}

WriteCoilsRequest: {
  function_code: uint8 & == 0x0F
  address:       u16_be
  quantity:      u16_be
  byte_count:    uint8 & auto_size: "data"
  data:          [...uint8] & len(data) == byte_count
}

WriteRegsRequest: {
  function_code: uint8 & == 0x10
  address:       u16_be
  quantity:      u16_be
  byte_count:    uint8 & auto_size: "data"
  data:          [...uint8] & len(data) == byte_count
}
```
