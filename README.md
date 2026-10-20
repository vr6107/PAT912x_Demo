# PAT912x Demo

A demo project based on the AT32F425 MCU and the PAT912x optical flow sensor.

## Repository Structure

- `01-AT32F425-PAT912x/`: Firmware project (CMake)
- `02-Hardware/`: Hardware resources
  - `images/`: Photos and diagrams
  - `schematic/`: Schematics (PDF)

## Hardware

- MCU: Artery AT32F425
- Sensor: PixArt PAT912x optical flow sensor
- Interface: I2C (update to match your design)

<img src="02-Hardware/images/1.jpg" width="300" alt="Hardware 1">
<img src="02-Hardware/images/2.jpg" width="300" alt="Hardware 2">
<img src="02-Hardware/images/3.jpg" width="300" alt="Hardware 3">


## Build

```bash
cd 01-AT32F425-PAT912x
cmake -S . -B build
cmake --build build

## COM Print Data
Format:
$<timestamp>,<sensor>,<dx>,<dy>,<tx_mm>,<ty_mm>,<sq>\r\n

Fields:

  timestamp  uint32   ms     Reset to 0 when the header is sent, then
                             incremented by 5 each loop. Not affected
                             by the 'R' command.
  sensor     string   -      Fixed text "PAT912x"
  dx         int16    count  Raw X increment for this frame
                             (12-bit signed, range +/-2047)
  dy         int16    count  Raw Y increment for this frame
                             (12-bit signed, range +/-2047)
  tx_mm      decimal  mm     Accumulated X displacement,
                             fixed 3 decimal places
  ty_mm      decimal  mm     Accumulated Y displacement,
                             fixed 3 decimal places
  sq         uint8    -      Image quality (higher is better).
                             Holds the last valid value while the
                             sensor is stationary.

Example:
$125,PAT912x,-3,5,-0.076,0.127,42\r\n



