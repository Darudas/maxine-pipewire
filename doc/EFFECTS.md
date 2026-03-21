# Effects Reference

Complete documentation for every audio effect supported by Maxine PipeWire.

---

## Noise Removal (Denoiser)

| Property | Value |
|---|---|
| Effect ID | `denoiser` |
| SDK Selector | `NVAFX_EFFECT_DENOISER` |
| Sample Rates | 16 kHz, 48 kHz |
| Intensity | 0.0 -- 1.0 |
| VAD Support | Yes |
| Latency | ~10 ms (480 samples at 48 kHz) |
| GPU Load | Low (~2--5% on RTX 3060+) |

### What It Does

Removes stationary and non-stationary background noise from a microphone signal
in real time. Effective against keyboard clicks, mouse clicks, mechanical
keyboard noise, fan hum, air conditioning, traffic, and other environmental
sounds. Preserves speech clarity even at maximum intensity.

### When to Use

- Video calls, streaming, podcasting
- Any situation where background noise is present
- As a first stage in an effect chain (denoise before other processing)

### Parameters

- **intensity** (float, 0.0--1.0): Controls how aggressively noise is removed.
  - `0.0` -- no processing (passthrough)
  - `0.3` -- light cleanup, preserves ambient feel
  - `0.6` -- moderate, good for offices
  - `0.8` -- strong, recommended for noisy environments
  - `1.0` -- maximum suppression, may slightly color the voice

- **vad** (bool): Enable voice activity detection. When enabled, the effect
  gates to silence during non-speech segments, further reducing noise floor.

### CLI Examples

```bash
# Enable with moderate intensity
maxctl enable denoiser
maxctl set denoiser intensity 0.6

# Gaming setup: max suppression + VAD
maxctl set denoiser intensity 1.0
maxctl set denoiser vad on
```

### Tips

- Start at 0.6 and increase only if needed. Maximum intensity on quiet
  environments can introduce subtle artifacts.
- Combine with `dereverb` for a cleaner signal when recording in untreated rooms.
- At 48 kHz the denoiser uses a 480-sample frame (10 ms latency).
  At 16 kHz it uses 160 samples (also 10 ms).

---

## Room Echo Removal (Dereverb)

| Property | Value |
|---|---|
| Effect ID | `dereverb` |
| SDK Selector | `NVAFX_EFFECT_DEREVERB` |
| Sample Rates | 16 kHz, 48 kHz |
| Intensity | 0.0 -- 1.0 |
| VAD Support | Yes |
| Latency | ~10 ms |
| GPU Load | Low (~2--5%) |

### What It Does

Reduces room reverberation -- the hollow, echoey sound that comes from
recording in hard-walled rooms, bathrooms, hallways, or any untreated acoustic
space. Particularly effective at taming early reflections and late tail reverb.

### When to Use

- Recording or streaming from rooms without acoustic treatment
- Conference calls from large meeting rooms
- Improving intelligibility of speech recorded in reverberant spaces

### Parameters

- **intensity** (float, 0.0--1.0):
  - `0.0` -- no processing
  - `0.4` -- mild room correction
  - `0.7` -- strong, removes most room character
  - `1.0` -- maximum, very dry sound

- **vad** (bool): Voice activity detection, same as denoiser.

### CLI Examples

```bash
maxctl enable dereverb
maxctl set dereverb intensity 0.5
```

### Tips

- Excessive dereverb (1.0) in already-dry rooms can make speech sound unnatural.
- Pair with `denoiser` for the cleanest result. Process order matters: typically
  dereverb first, then denoise.

---

## Combined Dereverb + Denoise

| Property | Value |
|---|---|
| Effect ID | `dereverb_denoiser` |
| SDK Selector | `NVAFX_EFFECT_DEREVERB_DENOISER` |
| Sample Rates | 16 kHz, 48 kHz |
| Intensity | 0.0 -- 1.0 |
| VAD Support | Yes |
| Latency | ~10 ms |
| GPU Load | Low--Medium (~3--7%) |

### What It Does

Performs both room echo removal and noise removal in a single optimized pass.
This is more efficient than chaining the two effects separately because the SDK
runs them on the same CUDA stream with shared intermediate buffers.

### When to Use

- Whenever you need both dereverb and denoise -- prefer this over two separate
  effects for lower latency and lower GPU utilization.

### Parameters

- **intensity** (float, 0.0--1.0): Controls both effects together.

### CLI Examples

```bash
maxctl add dereverb_denoiser
maxctl set dereverb_denoiser intensity 0.7
maxctl enable dereverb_denoiser
```

---

## Echo Cancellation (AEC)

| Property | Value |
|---|---|
| Effect ID | `aec` |
| SDK Selector | `NVAFX_EFFECT_AEC` |
| Sample Rates | 16 kHz, 48 kHz |
| Intensity | 0.0 -- 1.0 |
| VAD Support | No |
| Latency | ~10 ms |
| GPU Load | Medium (~5--10%) |

### What It Does

Acoustic Echo Cancellation removes the signal that your speakers play back from
your microphone input. This prevents the far end of a call from hearing their
own voice echoed back through your speakers.

The effect takes two inputs: the near-end microphone signal and the far-end
reference signal (what your speakers are playing). It subtracts the reference
from the mic signal using a neural-network-based model.

### When to Use

- Calls without headphones (speakers + mic setup)
- Laptop built-in speakers + built-in mic
- Conference room setups with open speakers

### Parameters

- **intensity** (float, 0.0--1.0): How aggressively to cancel the echo.
  Higher values are more aggressive but risk suppressing the near-end speaker's
  voice during double-talk.

### CLI Examples

```bash
# Via the SPA plugin (automatic, configured in PipeWire conf):
# See config/60-maxine-echo-cancel.conf

# Via daemon:
maxctl add aec
maxctl set aec intensity 1.0
maxctl enable aec
```

### Tips

- AEC works best with `monitor.mode = true` in the PipeWire config so the
  reference signal is captured directly from the speaker output.
- Can be combined with the denoiser (`maxine.denoise = "true"` in the SPA
  plugin config, or add both effects in daemon mode).
- If you use headphones, you don't need AEC -- skip it and just use the denoiser.

---

## Super-Resolution

| Property | Value |
|---|---|
| Effect ID | `superres` |
| SDK Selector | `NVAFX_EFFECT_SUPERRES` |
| Sample Rates | 16 kHz (input), upsamples internally |
| Intensity | No |
| VAD Support | No |
| Latency | ~10 ms |
| GPU Load | Low (~2--4%) |

### What It Does

Upsamples narrow-band audio (e.g., phone-quality 8 kHz or 16 kHz) to
wider-bandwidth audio, synthesizing plausible high-frequency content using a
neural network. The result sounds fuller and more natural than simple
interpolation.

### When to Use

- VoIP calls that arrive at low sample rates
- Improving the quality of recorded phone calls or legacy audio
- Chaining after denoise: clean up at 16 kHz, then upsample to 48 kHz

### Parameters

None beyond the sample rate configuration.

### CLI Examples

```bash
maxctl add superres
maxctl enable superres
```

### Tips

- Super-resolution is typically used as part of a chained effect (see below).
- Input must be 16 kHz for the basic effect. Chained variants can handle 8 kHz input.

---

## Studio Voice (High Quality)

| Property | Value |
|---|---|
| Effect ID | `studio_voice_hq` |
| SDK Selector | `NVAFX_EFFECT_STUDIO_VOICE_HQ` |
| Sample Rates | 48 kHz |
| Intensity | No |
| VAD Support | No |
| Latency | ~20 ms |
| GPU Load | Medium (~5--10%) |

### What It Does

Transforms speech to sound as though it was recorded in a professional studio
with high-end equipment. Combines noise removal, dereverb, and spectral
enhancement in one model. The "high quality" variant prioritizes audio fidelity
over latency.

### When to Use

- Podcasting, voice-over recording
- Streaming where audio quality is paramount
- Post-processing recorded speech

### CLI Examples

```bash
maxctl add studio_voice_hq
maxctl enable studio_voice_hq
```

### Tips

- Requires 48 kHz input. If your source is lower, chain with super-resolution.
- Higher latency than individual effects (~20 ms vs ~10 ms).
- Do not stack with separate denoiser/dereverb -- this effect already includes them.

---

## Studio Voice (Low Latency)

| Property | Value |
|---|---|
| Effect ID | `studio_voice_ll` |
| SDK Selector | `NVAFX_EFFECT_STUDIO_VOICE_LL` |
| Sample Rates | 48 kHz |
| Intensity | No |
| VAD Support | No |
| Latency | ~10 ms |
| GPU Load | Medium (~5--8%) |

### What It Does

Same processing as Studio Voice HQ but optimized for lower latency at the cost
of slightly reduced audio quality.

### When to Use

- Live streaming, real-time voice chat, gaming
- Any situation where latency matters more than studio-grade fidelity

---

## Speaker Focus

| Property | Value |
|---|---|
| Effect ID | `speaker_focus` |
| SDK Selector | `NVAFX_EFFECT_SPEAKER_FOCUS` |
| Sample Rates | 16 kHz, 48 kHz |
| Intensity | 0.0 -- 1.0 |
| VAD Support | No |
| Latency | ~10 ms |
| GPU Load | Low--Medium (~3--7%) |

### What It Does

Isolates the primary (loudest/nearest) speaker in the microphone signal and
attenuates all other speakers. Useful when multiple people are in the room but
only one person's voice should be transmitted.

### When to Use

- Open-plan offices
- Conference rooms where only the presenter should be heard
- Streaming/recording where background conversation is distracting

### Parameters

- **intensity** (float, 0.0--1.0): How strongly secondary speakers are
  attenuated.
  - `0.0` -- no isolation
  - `0.5` -- moderate, secondary speakers audible but quieter
  - `1.0` -- maximum, secondary speakers nearly silent

### CLI Examples

```bash
maxctl add speaker_focus
maxctl set speaker_focus intensity 0.8
maxctl enable speaker_focus
```

---

## Voice Font (High Quality / Low Latency)

| Property | Value |
|---|---|
| Effect ID | `voice_font_hq` / `voice_font_ll` |
| SDK Selector | `NVAFX_EFFECT_VOICE_FONT_HIGH_QUALITY` / `NVAFX_EFFECT_VOICE_FONT_LOW_LATENCY` |
| Sample Rates | 48 kHz |
| Intensity | No |
| VAD Support | No |
| Latency | ~20 ms (HQ) / ~10 ms (LL) |
| GPU Load | Medium--High (~8--15%) |

### What It Does

Transforms the speaker's voice to match a reference voice provided as a WAV
file. Uses neural voice conversion to change vocal characteristics (pitch,
timbre) while preserving speech content and prosody.

### When to Use

- Voice anonymization
- Creative voice effects for streaming or content creation
- Accessibility (voice modification)

### Parameters

The voice font effect requires a reference audio file (WAV) to define the target
voice. This is configured in the TOML configuration:

```toml
[audio.voice_font_hq]
enabled = true
reference_audio = "/path/to/reference_voice.wav"
```

### Tips

- The reference WAV should be clean speech, 48 kHz, mono, 5--30 seconds long.
- Higher GPU load than other effects. Monitor with `nvidia-smi dmon`.
- Cannot be combined with Studio Voice (they modify the same vocal characteristics).

---

## Chained Effects

Chained effects run two processing stages inside a single SDK call, which is
more efficient than running them as separate effects.

### Denoise 16k + Super-Res 16k-to-48k

| ID | `denoiser16k_superres16kto48k` |
|---|---|

Denoises a 16 kHz input, then upsamples to 48 kHz. Ideal for cleaning up
low-rate VoIP audio and outputting at full bandwidth.

### Dereverb 16k + Super-Res 16k-to-48k

| ID | `dereverb16k_superres16kto48k` |
|---|---|

Removes room reverb from 16 kHz input, then upsamples to 48 kHz.

### Dereverb+Denoise 16k + Super-Res 16k-to-48k

| ID | `dereverb_denoiser16k_superres16kto48k` |
|---|---|

Full cleanup (reverb + noise) at 16 kHz, then upsample to 48 kHz. The most
comprehensive chain for low-rate sources.

### Super-Res 8k-to-16k + Denoise 16k

| ID | `superres8kto16k_denoiser16k` |
|---|---|

First upsamples from telephone-quality 8 kHz to 16 kHz, then denoises.

### Super-Res 8k-to-16k + Dereverb 16k

| ID | `superres8kto16k_dereverb16k` |
|---|---|

Upsamples 8 kHz to 16 kHz, then removes room reverb.

### Super-Res 8k-to-16k + Dereverb+Denoise 16k

| ID | `superres8kto16k_dereverb_denoiser16k` |
|---|---|

Upsamples 8 kHz to 16 kHz, then full cleanup.

### Denoise + Speaker Focus (16k / 48k)

| IDs | `denoiser16k_speaker_focus16k`, `denoiser48k_speaker_focus48k` |
|---|---|

Removes noise first, then isolates the primary speaker. Available at both 16 kHz
and 48 kHz.

---

## Performance Summary

| Effect | Latency (48 kHz) | VRAM | GPU % (RTX 3060) |
|---|---|---|---|
| Denoiser | ~10 ms | ~200 MB | ~2--5% |
| Dereverb | ~10 ms | ~200 MB | ~2--5% |
| Dereverb+Denoiser | ~10 ms | ~250 MB | ~3--7% |
| AEC | ~10 ms | ~300 MB | ~5--10% |
| Super-Res | ~10 ms | ~200 MB | ~2--4% |
| Studio Voice HQ | ~20 ms | ~350 MB | ~5--10% |
| Studio Voice LL | ~10 ms | ~300 MB | ~5--8% |
| Speaker Focus | ~10 ms | ~250 MB | ~3--7% |
| Voice Font HQ | ~20 ms | ~400 MB | ~8--15% |
| Voice Font LL | ~10 ms | ~350 MB | ~6--12% |

**Note:** VRAM values are approximate and include model weight storage. Actual
usage depends on the GPU architecture and SDK version. Stacking multiple effects
shares CUDA context overhead, so the total is less than the sum of individual
effects.
