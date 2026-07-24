# Syok Dub Siren GB

Version: 1.0.0

Syok Dub Siren GB is a dub siren homebrew ROM for the original DMG Game Boy, built with GBDK-2020.

This is an unofficial homebrew project and is not affiliated with Nintendo Co., Ltd. Game Boy is a trademark of Nintendo.

Japanese documentation is available in [README.ja.md](README.ja.md).

## Download

The release ROM is:

```text
build/syok-dub-siren-gb.gb
```

For public distribution, attach `syok-dub-siren-gb.gb` to the GitHub Release.

The ROM header title is set to `SYOKDUBSIRENGB` because the Game Boy ROM header title field is limited to 16 characters.

## Controls

Use direct button combinations to control the synth parameters. Hold A to play sound.

| Control | Action |
|---|---|
| A | Hold to play sound |
| Up / Down | Change LFO waveform |
| A + Left / Right | Adjust base pitch while playing |
| B + Left / Right | Adjust LFO rate |
| A + B + Left / Right | Adjust LFO rate while playing |
| Select | Decrease LFO depth |
| Start | Increase LFO depth |
| Start + Select | Open or close HELP |

The on-screen waveform display updates according to the selected waveform and LFO depth. A sprite marker shows the current LFO position.

## Parameters

| Parameter | Range | Default | Notes |
|---|---:|---:|---|
| Pitch | 130 Hz to 2,100 Hz | 440 Hz | Base frequency before LFO modulation |
| LFO Depth | 0 Hz to 600 Hz | 400 Hz | Maximum pitch modulation depth |
| LFO Rate | approx. 0.159 Hz to 12.73 Hz | 19 / 80 | Step-based rate control |
| LFO Wave | SINE, SQUARE, SAW, REV SAW | SQUARE | Selected with Up / Down |

## Build

Install GBDK-2020 first. If `lcc` is available through your environment, run:

```sh
make
```

If GBDK is not in your PATH, set `GBDK_HOME`:

```sh
make GBDK_HOME=/path/to/gbdk
```

On this Windows development environment, the PowerShell build script can also be used:

```powershell
powershell -ExecutionPolicy Bypass -File .\build.ps1
```

The generated ROM is written to:

```text
build/syok-dub-siren-gb.gb
```

## Implementation Notes

- Uses Game Boy APU channel 1 directly.
- Uses `NR10` to `NR14` and `NR50` to `NR52`.
- Pitch is converted to the Game Boy 11-bit frequency register using the `131072 / (2048 - freq)` relationship.
- LFO waveforms use precomputed 256-step tables for SINE, SQUARE, SAW, and REV SAW.
- SINE, SAW, and REV SAW use 16-bit phase interpolation for smoother modulation.
- While playing, channel 1 envelope volume is set to the maximum 15/15, and master left/right volume is set to 7/7.
- The waveform display is rendered into background tiles and scaled by LFO depth.

## Known Limitations

- Parameters are changed in digital steps, unlike analog potentiometers.
- Audio behavior may vary slightly between real hardware, flash carts, and emulators.
- The sine table is optimized by mirroring a 64-value quarter wave into a 256-step waveform.
- Start is reserved for increasing LFO depth, so waveform changes are handled by Up / Down only.

## License

MIT License. See [LICENSE](LICENSE).