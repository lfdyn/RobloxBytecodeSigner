# READ
## This was patched in the new version-7a0c6d10fcb34a2c. Although I found a work around, if you want to obtain this work around I am selling it @badsquidward on discord. Furthermore, I will not update this repository. 
# Signer

This package signs compiled Luau bytecode for version-cec3ad5889b447cf.

See, instead of bypassing Roblox's signing by getting the cert directly from RStudio you can now sign yourself on compile!
## Build

Add `include/` to the include path and compile these sources as C++20:

```text
src/bits.cpp
src/solver.cpp
src/signer.cpp
vendor/blake3/blake3.c
vendor/blake3/blake3_dispatch.c
vendor/blake3/blake3_portable.c
```

Define `BLAKE3_NO_SSE2`, `BLAKE3_NO_SSE41`, `BLAKE3_NO_AVX2`, and `BLAKE3_NO_AVX512` when building the portable C implementation.

## External executor usage

Create one signer during executor startup and point it at the directory containing the four files in `tables/`:

```cpp
Testing::Signer signer( R"(C:\path\to\signer\tables)" );
```

Compile source with the same Luau compiler and opcode encoder used by the target client. Pass the resulting bytes to `Sign` before giving them to the client loader:

```cpp
std::string bytecode = Compile( source );
auto signedBytecode = signer.Sign( std::span(
    reinterpret_cast<const uint8_t*>( bytecode.data( ) ), bytecode.size( ) ) );

if ( !signer.Verify( signedBytecode ) )
    throw std::runtime_error( "local signature verification failed" );

LoadIntoClient( signedBytecode.data( ), signedBytecode.size( ) );
```

`Sign` preserves the compiled payload and appends the client verifier trailer. `Verify` is a local consistency check; the client must still use the same build profile and bytecode format. The tables and native profile are specific to `0.742.0.7421053`; regenerate them after a client update.

The solver is initialized once and is safe to reuse for multiple payloads. Keep signing and loading on the VM thread, and treat a failed local verification as a hard error.

Made with love by Dyn
