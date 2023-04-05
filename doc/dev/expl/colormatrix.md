YCbCr to RGB color matrix computation
=====================================


## Mathematic formulas

Video range constants:

```
Yoff    = 16
Yrange  = 219
UVrange = 224
```

Full range constants:

```
Yoff    = 0
Yrange  = 255
UVrange = 255
```

8-bit quantized Y,Cb,Cr:

```
Yq  = Round((Y *255 - Yoff) / Yrange)
Cbq = Round((Cb*255 - 128) / UVrange)
Crq = Round((Cr*255 - 128) / UVrange)
```

BT.709 formula (deciphered from the constants in the paper):

```
Yq  = Kr*R + Kg*G + Kb*B
Cbq = (B-Yq) / (2*(1-Kb))
Crq = (R-Yq) / (2*(1-Kr))
```

**Note**: we also have `Kg == 1 - Kb - Kr`

Inverting the equations to get `R`, `G`, `B`:

```
R = Yq + Crq * 2*(1-Kr)
G = Yq/Kg - Kr*R/Kg - Kb*B/Kg
B = Yq + Cbq * 2*(1-Kb)
```

Expressing them according to `Y`, `Cr`, `Cb` (*WARNING*: quantization rounding is ignored):

```
R = Yq + Crq * 2*(1-Kr)
  = (Y * 255 - Yoff) / Yrange + (Cr*255 - 128) * 2*(1-Kr) / UVrange
  = Y * 255/Yrange - Yoff/Yrange + Cr * 255*2*(1-Kr)/UVrange - 128*2*(1-Kr)/UVrange
  = Y * 255/Yrange + Cr * 255*2*(1-Kr)/UVrange + (-Yoff/Yrange - 128*2*(1-Kr)/UVrange)
        `---.----'        `--------.---------'   `-----------------.-----------------'
         Y factor              Cr factor                        Offset
```

```
G = Yq/Kg - Kr*R/Kg - Kb*B/Kg
  = Yq/Kg - Kr*(Yq + Crq * 2*(1-Kr))/Kg - Kb*(Yq + Cbq * 2*(1-Kb))/Kg
  = Yq/Kg - Kr*Yq/Kg - Crq * Kr*2*(1-Kr)/Kg - Kb*Yq/Kg - Cbq * Kb*2*(1-Kb)/Kg
  = Yq/Kg - Kr*Yq/Kg - Kb*Yq/Kg - Cbq * Kb*2*(1-Kb)/Kg - Crq * Kr*2*(1-Kr)/Kg
  = Yq*(1 - Kr - Kb)/Kg + Cbq * -Kb*2*(1-Kb)/Kg + Crq * -Kr*2*(1-Kr)/Kg
  = (Y * 255 - Yoff) * (1 - Kr - Kb)/Kg / Yrange + Cbq * -Kb*2*(1-Kb)/Kg + Crq * -Kr*2*(1-Kr)/Kg
  = Y * 255*(1-Kr-Kb)/(Yrange*Kg) + Cbq * -Kb*2*(1-Kb)/Kg + Crq * -Kr*2*(1-Kr)/Kg - Yoff*(1-Kr-Kb)/(Yrange*Kg)
  = Y * 255*(1-Kr-Kb)/(Yrange*Kg) + (Cb*255 - 128) / UVrange * -Kb*2*(1-Kb)/Kg + (Cr*255 - 128) / UVrange * -Kr*2*(1-Kr)/Kg - Yoff*(1-Kr-Kb)/(Yrange*Kg)
  = Y * 255*(1-Kr-Kb)/(Yrange*Kg) + Cb * 255*-Kb*2*(1-Kb)/Kg/UVrange + Cr * 255*-Kr*2*(1-Kr)/Kg/UVrange - (Yoff*(1-Kr-Kb)/(Yrange*Kg) + 128*-Kb*2*(1-Kb)/Kg/UVrange + 128*-Kr*2*(1-Kr)/Kg/UVrange)
  = Y * 255*Kg/(Yrange*Kg) + Cb * -255*Kb*2*(1-Kb)/(Kg*UVrange) + Cr * -255*Kr*2*(1-Kr)/(Kg*UVrange) + (-Yoff*Kg/(Yrange*Kg) + (128*Kb*2*(1-Kb) + 128*Kr*2*(1-Kr))/(UVrange*Kg)
  = Y * 255*/Yrange + Cb * -255*Kb*2*(1-Kb)/(Kg*UVrange) + Cr * -255*Kr*2*(1-Kr)/(Kg*UVrange) + (-Yoff/Yrange + (128*2*Kb*((1-Kb) + (1-Kr)))/(UVrange*Kg)
        `----.----'        `-------------.-------------'        `------------.--------------'   `--------------------------.----------------------------'
          Y factor                    Cb factor                            Cr factor                                    Offset
```

```
B = Yq + Cbq * 2*(1-Kb)
  = <similarly to R>
  = Y * 255/Yrange + Cb * 255*2*(1-Kb)/UVrange + (-Yoff/Yrange - 128*2*(1-Kr)/UVrange)
        `---.----'        `--------.---------'   `-----------------.-----------------'
         Y factor              Cb factor                        Offset
```


## Conversion matrix

Using previous expressions, we can craft the following `4x4` matrix:

| Red       | Green     | Blue      | Alpha |
| :-------: | :-------: | :-------: | :---: |
| Y factor  | Y factor  | Y factor  |   0   |
| Cb factor | Cb factor | Cb factor |   0   |
| Cr factor | Cr factor | Cr factor |   0   |
| Offset    | Offset    | Offset    |   1   |

Which can be used with `colormatrix * vec4(yuv, 1.0)` to obtain the RGB value.


## Exact values computation

Using intermediate variables to factor out common parts of the formulas, and
with the help of the Python `fractions` module, we can make the following code
to compute accurate matrices:

```python
from fractions import Fraction


K_constants = dict(
    bt601=(
        Fraction(299, 1000),
        Fraction(587, 1000),
        Fraction(114, 1000),
    ),
    bt709=(
        Fraction(2126, 10000),
        Fraction(7152, 10000),
        Fraction( 722, 10000),
    ),
    bt2020=(
        Fraction(2627, 10000),
        Fraction(6780, 10000),
        Fraction( 593, 10000),
    ),
)


def _get_matrix(name, video_range=True):
    Kr, Kg, Kb = K_constants[name]
    assert Kg == 1 - Kb - Kr
    y_off, y_range, uv_range = (16, 219, 224) if video_range else (0, 255, 255)

    y_factor = Fraction(255, y_range)
    r_scale  = Fraction(2 * (1 - Kr), uv_range)
    b_scale  = Fraction(2 * (1 - Kb), uv_range)
    g_scale  = Fraction(2, Kg * uv_range)
    y_off    = Fraction(-y_off, y_range)

    r_y_factor = g_y_factor = b_y_factor = y_factor

    r_cb_factor = 0
    g_cb_factor = -255 * g_scale * Kb * (1 - Kb)
    b_cb_factor =  255 * b_scale

    r_cr_factor =  255 * r_scale
    g_cr_factor = -255 * g_scale * Kr * (1 - Kr)
    b_cr_factor = 0

    r_off = y_off - 128 * r_scale
    g_off = y_off + 128 * g_scale * (Kb * (1 - Kb) + Kr * (1 - Kr))
    b_off = y_off - 128 * b_scale

    return [
        r_y_factor,  g_y_factor,  b_y_factor,  Fraction(0),
        r_cb_factor, g_cb_factor, b_cb_factor, Fraction(0),
        r_cr_factor, g_cr_factor, b_cr_factor, Fraction(0),
        r_off,       g_off,       b_off,       Fraction(1),
    ]
```

This code is re-written in C in `libnopegl`, with a `float` precision approximation.


## Accuracy testing

The matrices found in `test_colorconv` are generated using the following code:

```python
_matrix_tpl = '''        [{}] = {{
            {:>16}., {:>20}., {:>16}., {}.,
            {:>16}., {:>20}., {:>16}., {}.,
            {:>16}., {:>20}., {:>16}., {}.,
            {:>16}., {:>20}., {:>16}., {}.,
        }},'''

_range_tpl = '''    [{}] = {{
{}
    }},'''


def _gen_tables():
    cspaces = ("bt601", "bt709", "bt2020")
    ranges = []
    for r in (True, False):
        matrices = []
        for i, cspace in enumerate(cspaces):
            matrices.append(_get_matrix(cspace, video_range=r))
        matrix_defs = '\n'.join(_matrix_tpl.format(i, *mat) for i, mat in enumerate(matrices))
        ranges.append(matrix_defs)
    defs = '\n'.join(_range_tpl.format(i, matrices) for i, matrices in enumerate(ranges))
    print(defs)


if __name__ == "__main__":
    _gen_tables()
```

These matrices are used as a reference to evaluate the precision of the C code.
Using fractions instead of float in C has been considered (and tested) but
requires an uncessary complexity (overflow handling in particular) for marginal
accuracy benefits.
