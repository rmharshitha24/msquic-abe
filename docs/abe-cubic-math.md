# CUBIC growth mathematics

`CubicComputeK` and `CubicComputeWindow` (`src/core/cubic.c`) compute K and the
cubic window from a beta passed in as a parameter (hundredths: 70 means 0.70).
At beta = 70 they reproduce the original output exactly.

## Formulas (RFC 8312)

W is in packets, t in seconds, C = 0.4.

$$K = \sqrt[3]{\frac{W_{max}(1-\beta)}{C}} \qquad W_{cubic}(t) = C\,(t-K)^3 + W_{max}$$

The RFC evaluates the window at t + RTT, so `CubicComputeWindow` adds the
smoothed RTT to the elapsed time.

## Original integer form

Beta = 0.7 is stored as 7 and C = 0.4 as 4 (both 10x multiples):

$$K_{ms} = \left( \mathrm{CubeRoot}\!\left( \frac{\lfloor W_{max}/MSS \rfloor \cdot (10-7) \cdot 2^{9}}{4} \right) \cdot 1000 \right) \gg 3$$

The factors of 10 cancel: ((10 - 10b)/10) / (10C/10) = (10 - 10b) / 10C.
Shifting the radicand left by 9 multiplies the cube root by 8 (2^9 = 8^3), so the
final `>> 3` removes that factor. The result is seconds x 1000, which is
milliseconds.

## Hundredths scaling

With B = 100 x beta:

$$\frac{(100-B)/100}{(10C)/10} = \frac{100-B}{10 \cdot (10C)} = \frac{100-B}{40}$$

The code uses `(100 - BetaHundredths)` in the numerator and
`TEN_TIMES_C_CUBIC * 10` in the denominator. K keeps its unit (milliseconds).

## Why beta = 70 is bit-exact

Per packet: 30 x 512 / 40 = 384, and the original 3 x 512 / 4 = 384. Both are
exact integers with no remainder. The order of operations is unchanged:
floor(W/MSS), multiply, shift by 9, divide, cube root, x1000, `>> 3`.

## Rounding decisions

- Divide by MSS first and truncate, as the original does. A window under one
  packet gives K = 0.
- The only division after the shift is by 40. It is exact at beta = 70 and
  truncates toward zero at other values.
- `CubeRoot` floors, so K can be low by up to one cube-root unit (125 ms).

## Overflow

- The original multiplied in 32 bits and wraps above about 2.79 million packets
  (about 3.5 GB at MSS 1280). The new code multiplies in 64 bits.
- `CubeRoot` takes 32 bits. The radicand is floor(W/MSS) x (100 - B) x 12.8. For
  a 32-bit WindowMax and MSS >= 1200 it fits whenever B >= 7, so the realistic
  range (50 to 90) never saturates.
- Below B = 7 with a window near 4 GB the radicand is capped at 2^32 - 1, so K
  tops out at 203,125 ms instead of wrapping.
- `DeltaT` in the growth function is capped at 2,500,000 ms. That arithmetic was
  moved unchanged.

## K at realistic windows (MSS 1280)

| WindowMax | K at beta 70 (loss) | K at beta 85 (ECN) |
| --- | --- | --- |
| 64 KB | 3,250 ms | 2,625 ms |
| 1 MB | 8,500 ms | 6,625 ms |
| 10 MB | 18,250 ms | 14,500 ms |
| 100 MB | 39,375 ms | 31,250 ms |
| 1 GB | 85,625 ms | 68,000 ms |

A higher beta means a shallower back-off, so there is less to regrow and K is
shorter.
