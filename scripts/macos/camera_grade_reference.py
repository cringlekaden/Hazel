#!/usr/bin/env python3
"""Generate Hazel's independent thermal white-balance input fixtures."""
import argparse
import csv
import hashlib
import io
import math
from pathlib import Path

CMF_SHA256 = "fa663e3535a7e0763a745993a1f0a192eb0275ac46ad2d1befd7626841e713c1"
# Normalized XYZ -> linear sRGB, derived from D65 and Rec.709 primaries.
XYZ_TO_RGB = (
    (3.2409699419, -1.5373831776, -.4986107603),
    (-.9692436363, 1.8759675015, .0415550574),
    (.0556300797, -.2039769589, 1.0569715142),
)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("matching_functions", type=Path,
                        help="CIE_xyz_1931_2deg.csv from CIE DS xvudnb9b")
    args = parser.parse_args()
    source = args.matching_functions.read_bytes()
    if hashlib.sha256(source).hexdigest() != CMF_SHA256:
        parser.error("The CIE matching functions differ from the published dataset checksum")
    samples = [tuple(map(float, row)) for row in csv.reader(io.StringIO(source.decode("ascii")))]
    print("Kelvin,R,G,B")
    for kelvin in (2000, 2856, 3200, 4000):
        # Planck's law per unit wavelength. The arbitrary common scale cancels
        # when the integrated tristimulus vector is normalized to Y=0.2.
        radiance = [(560 / nm) ** 5 / math.expm1(1.438776877e7 / (nm * kelvin))
                    for nm, *_ in samples]
        xyz = [math.fsum(channel * power for channel, power in zip(values, radiance))
               for values in list(zip(*samples))[1:]]
        rgb = [math.fsum(channel * weight for channel, weight in zip(xyz, row)) * .2 / xyz[1]
               for row in XYZ_TO_RGB]
        print(kelvin, *(f"{channel:.9f}" for channel in rgb), sep=",")
    print("6504,0.2,0.2,0.2")  # Ideal 6504 K D65 white, specified in the destination gamut.


if __name__ == "__main__":
    main()
