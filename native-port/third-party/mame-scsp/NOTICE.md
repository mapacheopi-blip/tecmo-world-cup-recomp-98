# SCSP reference and adapted signal processing

Source: [MAME 0.289 scsp.cpp](https://github.com/mamedev/mame/blob/mame0289/src/devices/sound/scsp.cpp).
Copyright ElSemi and R. Belmont; BSD-3-Clause, reproduced in LICENSE.txt.

native_scsp_tables.hpp preserves the envelope timing tables. native_scsp.cpp adapts the PCM slot/envelope/loop/pan behavior to the native session clock and sample RAM. Device-framework code and CPU interpreters are not included. The current path implements direct PCM output; nonzero DSP programs, internal waveforms and modulation requests stop explicitly. Signal comparisons now replay 895,955 external register/RAM events and compare nine stereo excerpts against a separate MAME WAV. Six focused contracts cover addressing, interpolation, reverse loops, monitoring and decay hold. See ../../../evidence/NATIVE-SCSP-SIGNAL-2026-11-02.md. Floating point envelope/attenuation and untested modes remain approximations; this is not complete hardware or listening validation. A verbatim reference copy is kept at ../../../references/mame0289-scsp.cpp, outside the build.
