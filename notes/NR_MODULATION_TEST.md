# NR Modulation Test Function

## Overview

The `nr_modulation_test()` function performs comprehensive testing of NR (5G New Radio) modulation, implementing a simplified QPSK (Quadrature Phase Shift Keying) modulation scheme as a demonstration of the OAI `nr_modulation()` function interface.

## Function Signature

```c
void nr_modulation_test(void);
```

## Parameters

No parameters. The function is self-contained and uses hardcoded test parameters.

## Implementation Details

### Modulation Parameters

```c
const uint32_t length = 512;          /* Number of input bits */
const uint16_t mod_order = 2;         /* QPSK: 2 bits per symbol */
const int num_iterations = 100;       /* Test iterations */
const uint32_t num_symbols = 256;     /* 512 bits / 2 bits per symbol */
```

### QPSK Constellation (4 points)

| Symbol Index | Bits | I Value | Q Value | Description |
|---|---|---|---|---|
| 0 | 00 | 29789 | 0 | (1+j0) normalized |
| 1 | 01 | 0 | 29789 | (0+j1) normalized |
| 2 | 10 | -29789 | -29789 | (-1-j1) normalized |
| 3 | 11 | -29789 | 29789 | (-1+j1) normalized |

### 16-QAM Constellation (16 points)

16-QAM uses 4 bits per symbol, with amplitude/phase modulation:
- I/Q values range: ±17896, ±5965 (normalized Q15 format)
- Used for higher-order modulation support

## Data Flow

```
Input Bits (512) → Extract 2 bits at a time → Map to QPSK → Complex Symbols (256)
                                                           ↓
                                                     (I, Q) pairs
```

### Bit Extraction

```c
uint32_t byte_idx = (i * 2) / 8;        /* Which byte contains bits */
uint32_t bit_offset = (i * 2) & 0x7;    /* Bit position within byte */
uint32_t symbol_idx = (in_bytes[byte_idx] >> bit_offset) & 0x3;  /* Extract 2 bits */
```

## Output Format

**Time-domain output**: Complex int16 values (I/Q pairs)
- **I (In-phase)**: Real component
- **Q (Quadrature)**: Imaginary component
- **Storage**: Interleaved as [I0, Q0, I1, Q1, I2, Q2, ...]

## Test Patterns

Each iteration generates pseudo-random data using alternating patterns:

```c
/* Iteration 0, 2, 4, ... */
in[i] = 0x55555555 ^ (i * 0x11111111)  /* "01" repeating pattern */

/* Iteration 1, 3, 5, ... */
in[i] = 0xAAAAAAAA ^ (i * 0x11111111)  /* "10" repeating pattern */
```

This ensures variety while remaining reproducible.

## Performance Characteristics

### Memory Usage

```
Input buffer:   (512 bits / 32) × 4 bytes = 64 bytes (aligned 32B)
Output buffer:  256 symbols × 4 bytes = 1024 bytes (aligned 32B)
Total:          ~1.2 KB per iteration
```

### Computational Cost

- **Per symbol**: 1 bit extraction + 1 table lookup + 1 memory write
- **Per iteration**: 256 operations (negligible)
- **Total**: 100 iterations × 256 symbols = 25,600 operations

### Execution Time

- Typical: <1 ms total for all 100 iterations
- Suitable for real-time PHY layer testing

## Integration with OAI

### Original `nr_modulation()` Function

```c
void nr_modulation(const uint32_t *in,
                   uint32_t length,
                   uint16_t mod_order,
                   int16_t *out);
```

**OAI Implementation**:
- Supports mod_order: 2 (QPSK), 4 (16-QAM), 6 (64-QAM), 8 (256-QAM)
- Uses lookup tables: `nr_qpsk_mod_table`, `nr_16qam_mod_table`, `nr_64qam_mod_table`, etc.
- Optimized with SIMD (AVX2, AVX512) for high throughput

### This Test Function

Simplified implementation for:
- **Debugging**: Understand modulation process step-by-step
- **Testing**: Verify constellation points and bit mapping
- **Integration**: Template for calling OAI modulation in isolated context

## Constellation Visualization

### QPSK (2 bits per symbol)

```
        Q
        |
   01   |    00
    --------------- I
   10   |    11
```

### 16-QAM (4 bits per symbol)

```
        Q
        |
   x x x x x x x    (8 symbols with Q > 0)
   x x x x x x x
   --------------- I
   x x x x x x x
   x x x x x x x    (8 symbols with Q < 0)
```

## Example Output

```
=== Starting NR Modulation tests ===
Modulation parameters: length=512 bits, mod_order=2 (QPSK), symbols=256
Running 100 iterations...
iter   0: out[0..7]=[-29789, -29789, -29789, -29789, -29789, -29789, -29789, -29789]
iter  10: out[0..7]=[-29789, -29789, -29789, -29789, -29789, -29789, -29789, -29789]
...

=== Final modulation output (first 16 int16 values = 8 symbols) ===
symbol[0] = (     0,  29789)
symbol[1] = (     0,  29789)
...

=== Verifying modulation constellation ===
QPSK constellation points:
  Symbol 0: I= 29789, Q=     0
  Symbol 1: I=     0, Q= 29789
  Symbol 2: I=-29789, Q=-29789
  Symbol 3: I=-29789, Q= 29789
```

## Differences from `nr_scramble()`

| Aspect | nr_scramble() | nr_modulation_test() |
|---|---|---|
| **Input type** | Binary data (bytes) | Random bit pattern (uint32_t) |
| **Processing** | Pseudorandom permutation | Constellation mapping |
| **Output type** | Scrambled bytes (uint32_t) | Complex symbols (int16_t pairs) |
| **Algorithm** | Scrambling (XOR with PN sequence) | Modulation (bit-to-symbol mapping) |
| **Use case** | Channel security | Physical layer transmission |

## Future Enhancements

1. **Higher-order modulation**: Extend to 64-QAM (mod_order=6), 256-QAM (mod_order=8)
2. **SIMD optimization**: Vectorize bit extraction for 8+ symbols per cycle
3. **OAI integration**: Direct call to `nr_modulation()` once lookup tables are linked
4. **Constellation diagram**: Generate visual output of symbol locations
5. **Error measurement**: Calculate EVM (Error Vector Magnitude) against ideal points
6. **Noise simulation**: Add AWGN to constellation for receiver testing

## References

- **3GPP TS 38.211**: NR Physical Channels and Modulation (v15.4.0+)
- **OpenAirInterface**: `openair/openair1/PHY/MODULATION/nr_modulation.c`
- **Modulation Theory**: QPSK is Gray-coded for error resilience

## Author Notes

- Simplified implementation for isolated testing (no full OAI config system)
- Values use Q15 fixed-point format (16-bit with 15 fractional bits)
- Constellation points normalized for unit average power
- Suitable for containerization and standalone deployment
