# Server Configuration

## Command Line Arguments

The ab_server simulator accepts the following command line arguments:

```bash
./ab_server --type <plc_type> --path <cip_path> --tag <tag_definition> [--tag <tag_definition> ...]

Options:
  --type     PLC type: PLC5, SLC500, ControlLogix, CompactLogix
  --path     CIP routing path (e.g., "1,0" for slot 0)
  --tag      Tag definition (use multiple --tag options for multiple tags)
  --port     TCP port (default: 44818)
  --verbose  Enable debug output

Examples:
  ./ab_server --type ControlLogix --path "1,0" --tag "DINT_TAG:DINT[100]" --tag "REAL_TAG:REAL:[123,45]"
  ./ab_server --type PLC5 --tag "N7[10]" --tag "B3[1]"
```

## Supported PLC Types

| PLC Type     | CIP Support | PCCC Support | Default Tags                       |
| ------------ | ----------- | ------------ | ---------------------------------- |
| PLC5         | Limited     | Full         | N7:0-100, B3:0-100, F8:0-50        |
| SLC500       | Limited     | Full         | N7:0-100, B3:0-100, F:0-50, L:0-50 |
| ControlLogix | Full        | None         | DINT_TAG, REAL_TAG, STRING_TAG     |
|              | BOOL_TAG    |
| CompactLogix | Full        | None         | Same as ControlLogix               |
| OMRON        | Full        | None         | Same as ControlLogix               |



## Tag Simulation

The server maintains an internal tag database with the following capabilities:

- **Data Types**: BOOL, SINT, INT, DINT, LINT, USINT, UINT, UDINT, ULINT, REAL, LREAL, STRING
- **Arrays**: Multi-dimensional arrays supported
- **Boolean Arrays**:
  - Rockwell PLCs: Use 32-bit DWORD packing (type 0xD3)
  - OMRON PLCs: Use 16-bit WORD packing ( but reports type 0xC1)
  - Bit ordering: LSB = array index 0, MSB = highest index in word/dword
- **Structures**: UDT (User Defined Types) simulation not supported
- **Memory Areas**: File-based addressing for PCCC (N, B, F, L files)
  - All data file types are arrays.  B file type size is given in bits not words.
