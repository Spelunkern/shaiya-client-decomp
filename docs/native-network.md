# Native network runtime

`src/native/{tcp,packet,wire,login,world,connection}.*` replaces recovered socket globals,
window-message callbacks and x86 packet ownership with per-connection C++ state.
This is a component of the game port; it is not the full native game client.
The recovered client is authoritative for packet layouts and behavior. Local
servers and loopback fixtures are test aids, not alternate specifications.

## Recovered contracts

- `socket_transport.cpp` / `window_transport.cpp`: TCP carries a little-endian
  16-bit frame size, a 16-bit opcode and its body. The size word is plaintext;
  the opcode and body are encrypted when the applicable stream is enabled.
- `challenge.cpp`, `bootstrap.cpp`, `bootstrap_crypto.cpp`: fixed-size A101
  challenge fields are algorithm/length/length followed by 64 exponent and 128
  modulus bytes. Little-endian RSA has no padding scheme in this protocol.
  The reply is a byte length plus 128 bytes. The length is rounded to 32-bit limbs.
- RSA arithmetic uses OpenSSL BIGNUM, replacing the recovered x86 bigint/CRT
  boundaries. The HMAC derivation and AES/LCG primitives use recovered production
  sources. Each connection owns its counters, flags, keys and receive buffer.
- `FUN_005992f0` standard branch: A102 account/password fields are 32 and 16
  bytes. These are wire bytes, not implicitly UTF-8. The optional vendor-specific
  A106/33-byte password branch is not ported yet.
- `fun_005921b0`: A102 success contains status, account ID, auth status and a
  16-byte identity. A failure is surfaced without reading nonexistent fields.
- `fun_00592220`: A201 contains a one-byte count and 38-byte records: ID, status,
  users, capacity and a 32-byte name. Native vectors replace the original fixed
  48-entry stack array.
- `FUN_00599440` / `fun_005923d0`: A202 selects one world ID with a 32-bit version
  and returns status plus four IPv4 bytes.
- `004eb259` and `FUN_00599480`: the world connection retains key schedules,
  hashes the login nonce for fresh counters, and sends the plaintext A301 account
  ID + identity before enabling reset transport streams. Keys move between
  owners, with the moved-from crypto state cleared.
- `fun_00592420` / `fun_005712f0`: world A301 returns status, mode and a 16-byte
  seed. The seed expands into the recovered SHA-256 XOR table.
- `fun_0058d860`: occupied 0101 slots have typed scalar, appearance, stats,
  resources, equipment, name and flag fields. Empty slots retain their prefix.
  Short name tails are zero-filled; optional extended equipment is retained.
- `fun_0058e0e0` / `fixed_0058db90`: faction/max-mode and character selection
  responses update typed native state and host-owned character containers.
- `fun_00591dd0` / `00401570`: 010b quick-slot records switch subsequent inbound
  packets to the selected world mode. 0107 restores ordinary inbound transport.
  Dispatch happens between packets, even within the same TCP read.
- `mixed_0058dbd0` -> session slot 0x28 / `0055ff60`: 0105 has a
  62-byte fixed prefix: two point words, HP/MP/SP dwords, an orientation word,
  three experience dwords, money, XYZ floats and four combat counters. A
  bounded name tail of up to 25 bytes follows. The native state preserves all
  three experience words in wire order and supports the client's short tail.
- Player-owned 0526 (12 dwords), 051f (entity ID + maximum HP/MP/SP),
  0521 (current HP/MP/SP), 0213 (money), 020e (indexed combat count), and
  020b (entity ID + map word + XYZ) update host-owned state. Entity-specific
  packets for other players remain queued for their future scene owners.
  Player state is created after selection success and destroyed on logout.
- `Connection` replaces the login socket with the advertised world endpoint.
  Unported gameplay messages remain in a bounded event queue; retaining them
  does not implement their game behavior.

Undefined recovered behavior is not copied into host memory corruption:
incomplete frame lengths, oversized fields and queue overflow are rejected.
Unused RSA reply padding is zeroed instead of transmitting old stack contents.
The native send queue retains short-send remainders; the old callback discarded
them. This preserves packet bytes across normal POSIX/Winsock partial writes.

Mode-two inbound XOR is reproduced for the known 4096-byte table. Payloads over
2048 bytes require data beyond that recovered table and are explicitly rejected;
that world-protocol boundary still needs recovery. Do not treat it as complete.

## Validation

- Wire bytes match the recovered AES sender; independent connections and send /
  receive counters remain independent. Fragmentation at every byte boundary,
  coalesced frames and a plaintext-to-encrypted transition in one read are tested.
- RSA output matches an independent Python integer `pow` fixture; known Python
  HMAC material decrypts the native login request. Both AES and LCG paths and
  LCG reconnect nonce reset are exercised.
- Synthetic authentication failure, retry, success, server list and selection
  transitions preserve encryption positions. All truncated A101 body lengths
  are rejected. No real account credentials are used by these tests.
- TCP loopback transfers and echoes 900,000 bytes using small server reads /
  writes, and checks peer close and connection refusal. The same test passes on
  Linux and the Windows x86-64 executable under Wine.
- A two-server loopback fixture exercises native authentication, selection,
  socket replacement, identity transfer and world character/faction reception.
  It passes on Linux and Windows under Wine, plus Linux ASan/UBSan.
- World tests exercise occupied characters, selection failure/retry/success and
  AES -> XOR -> AES switches in one received buffer, with Python XOR fixtures.
- `genesis_login_probe` exchanges A101 over a real loopback socket with a Python
  fixture. It does not submit an account or password. The Linux and Windows
  executables both pass this fixture. Native Windows hardware remains unverified.

```sh
build-native/genesis_login_probe 127.0.0.1 30800
ctest --test-dir build-native -R 'native_login|native_wire|native_tcp' --output-on-failure
```

The first command needs an already-running compatible login server. Real-account
server interaction and gameplay are not validated by the probe or fixtures;
world transitions above use isolated synthetic servers. `genesis_native` connects
the typed login and world character catalog to the SDL/OpenGL interface. Its
application test enters a localized account, checks password masking and retry,
selects a server and character, and verifies saved-ID restoration/removal.
Initial world state and gameplay dispatch remain incomplete.

References: [OpenSSL little-endian integer encoding](https://docs.openssl.org/3.1/man3/BN_bn2bin/),
[Winsock nonblocking select](https://learn.microsoft.com/en-us/windows/win32/api/winsock2/nf-winsock2-select).
