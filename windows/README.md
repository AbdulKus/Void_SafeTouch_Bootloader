# SafeTouch Windows Login

The Windows part contains two native x64 components:

- `SafeTouchSetup.exe` enrolls one local/domain-style Windows account, verifies
  its password once, provisions the attached SafeTouch and writes
  `%ProgramData%\SafeTouch\credentials.dat`;
- `SafeTouchCredentialProvider.dll` adds an independent SafeTouch tile to
  LogonUI. It does not wrap, replace or disable Microsoft's password provider.

## Build

Install Visual Studio 2022 Build Tools with **Desktop development with C++** and
the Windows 10/11 SDK, then run from an x64 Developer PowerShell:

```powershell
.\build-windows.ps1 -Configuration Release
```

The project intentionally uses only Windows inbox APIs (HID, SetupAPI, CNG,
DPAPI and Credential Provider COM); no kernel driver or third-party runtime is
required. Sign the EXE and DLL with your organization’s Authenticode certificate
before production deployment.

## Install and enroll

Keep a second administrator account and verify its password before installing
any development Credential Provider. From elevated PowerShell:

First flash the login application while the Void Bootloader (`1209:B007`) is
active, then physically reconnect USB so the application enumerates as
`1209:B008`:

```powershell
..\.venv\Scripts\python.exe ..\voidtool.py upload `
  ..\examples\windows-login\windows-login.vbi --boot
```

Then install and enroll:

```powershell
.\install.ps1
& "$env:ProgramFiles\SafeTouch\SafeTouchSetup.exe"
```

For an already enrolled device, hold **both** SafeTouch buttons while starting
the enrollment command. Setup accepts cards for which the firmware can obtain
only an ATR-derived fingerprint, so every readable ISO 7816 card can be
enrolled. ATR values are commonly shared by many cards; use `--require-emv` to
require a card-specific EMV identifier instead.

After setup, lock the workstation with `Win+L`. The provider starts monitoring
SafeTouch in the background even when the normal password tile is visible.
Inserting the registered card makes **SafeTouch** the default tile; pressing
GREEN completes the challenge-response and asks LogonUI to submit the standard
Windows credentials automatically. The status changes through `Insert card`,
`Reading card...`, `Press GREEN on SafeTouch`, and `Signing in...`.

The SafeTouch LCD also tracks the physical card while idle: it displays
`INSERT CARD` when the slot is empty and `CARD INSERTED` while a card is in the
slot. A rejected card displays `REMOVE CARD`; removing it rearms the same
challenge for another card.

## Credential file

The password is AES-256-GCM encrypted. Its wrapping key is derived from the
device secret and is not stored in the file. A separate proof-verification key
is machine-bound with DPAPI, and the file DACL grants full access only to
SYSTEM and Administrators. The provider zeroes its plaintext password and
wrapping-key working buffers immediately after creating the standard Windows
credential serialization.

Changing the Windows password invalidates the stored credential; rerun setup.
Automatic workstation locking on card removal is not enabled in this version;
it can be added later as a separate user-session service without changing the
Credential Provider protocol.

## Removal

```powershell
.\uninstall.ps1
# Add -PurgeCredentials only when the stored enrollment file should be deleted.
```

## Security boundary

This revision of SafeTouch has no secure element and the repository deliberately
does not set the AT91 security bit. A physical attacker with JTAG access can
extract or replace firmware and device configuration. Enrollment also provisions
the random device secret over the local USB cable. The challenge-response
protocol prevents replay and a fabricated `AUTH_OK` from a device that lacks the
secret, but it is not a defense against a physically compromised terminal or
malware running as SYSTEM/Administrator. The ISO7816 card identifier is a factor,
not a card-held secret.

The current card transport implements ISO7816 T=0. Cards that expose only T=1,
or whose stable application identifier cannot be read without additional
authentication, fall back to ATR and are accepted by default. Such cards can
share the same fingerprint; use `--require-emv` when strict card-specific
binding is required.
