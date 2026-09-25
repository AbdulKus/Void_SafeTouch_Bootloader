# SafeTouch Windows login protocol v1

The login application enumerates as vendor-defined HID `1209:B008`. Reports are
64 bytes and do not contain a report ID on the wire. `1209` is the community
VID and `B008` is experimental; obtain an assigned PID before distribution.

## Enrollment

The elevated setup application generates a random 256-bit `device_secret` with
Windows CNG. `ENROLL_BEGIN` transfers the secret, an LCD display name and the
ATR-fallback policy. The firmware reads the inserted ISO7816 card, prefers a
fingerprint over EMV AID plus PAN/track-2 data, asks for GREEN confirmation and
writes the secret and card fingerprint to alternating 128-byte Flash pages.
Re-enrollment requires both buttons to be held when the command arrives.

Derived values are domain separated:

```text
auth_key = HMAC-SHA256(device_secret, "SafeTouch auth key v1")
wrap_key = HMAC-SHA256(device_secret, "SafeTouch wrap key v1")
device_id = first16(SHA256(device_secret || "SafeTouch device id v1"))
```

The Windows password is encrypted with AES-256-GCM under `wrap_key`. The file
contains neither `device_secret` nor `wrap_key`.

## Authentication

The provider generates a fresh 32-byte CNG nonce and sends `AUTH_BEGIN`. Once
the card fingerprint matches and GREEN is pressed, the firmware returns:

```text
proof = first16(HMAC-SHA256(auth_key,
        "SafeTouch auth proof v1" || nonce || card_id || device_id))
wrap_key
```

The provider verifies `proof` in constant time before it accepts `wrap_key`.
An old reply cannot be replayed because its proof is bound to a fresh nonce.
The key then authenticates/decrypts `credentials.dat`; an invented wrapping key
fails the GCM tag. The response key, decrypted password, and serialization
working buffers are explicitly zeroed after use.

## Report layout

All unused bytes are zero.

| Command | Request | Reply |
|---|---|---|
| `01 INFO` | command | result, state, device ID `[4..19]`, card ID `[20..35]`, display name `[36..51]`, version `[52..53]` |
| `10 ENROLL_BEGIN` | flags `[2]`, name length `[3]`, secret `[4..35]`, name `[36..51]` | result and state |
| `11 STATUS` | command | result, state, card source; on success proof `[4..19]`, wrapping key `[20..51]` |
| `12 CANCEL` | command | result and state |
| `20 AUTH_BEGIN` | nonce `[4..35]` | result and state |

The card identifier is deliberately not treated as secret. If EMV identification
is unavailable, setup rejects the ATR-only fallback unless the operator opts in.
