# SafeVoid Windows Login

The Windows part contains two native x64 components:

- `SafeVoidSetup.exe` is a self-contained elevated GUI installer. It embeds,
  extracts and registers the provider DLL, provisions the attached SafeVoid,
  manages the primary and backup cards, verifies the Windows password once and
  writes `%ProgramData%\SafeVoid\credentials.dat`;
- `SafeVoidCredentialProvider.dll` adds an independent SafeVoid tile to
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

Then launch the single setup executable. Windows requests administrator rights;
the app installs itself and the embedded DLL into `Program Files\SafeVoid` and
registers the Credential Provider automatically:

```powershell
.\build\Release\SafeVoidSetup.exe
```

The minimal dark UI shows component, USB and card status. Select the Windows
account, enter its password and use **Set up primary card**. To replace an
existing enrollment, hold both SafeVoid buttons while confirming the action.
After the primary card is ready, **Add backup card** stores a second independent
card identifier without changing the encrypted Windows credential. Adding or
replacing the backup also requires both physical buttons, followed by GREEN.

Setup accepts cards for which the firmware can obtain only an ATR-derived
fingerprint, so every readable ISO 7816 card can be enrolled. ATR values are
commonly shared by many cards and should not be treated as a card-held secret.

After setup, lock the workstation with `Win+L`. The provider starts monitoring
SafeVoid in the background even when the normal password tile is visible.
Inserting the registered card makes **SafeVoid** the default tile; pressing
GREEN completes the challenge-response and asks LogonUI to submit the standard
Windows credentials automatically. The status changes through `Insert card`,
`Reading card...`, `Press GREEN on SafeVoid`, and `Signing in...`.

The SafeVoid LCD also tracks the physical card while idle: it displays
`INSERT CARD` when the slot is empty and `CARD INSERTED` while a card is in the
slot. A rejected card displays `REMOVE CARD`; removing it rearms the same
challenge for another card.

The button lighting follows the same state: RED means that the card slot is
empty (or that access was denied), GREEN means a card is present, and both
button lights turn on while `CONFIRM? YES/NO` is displayed.

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

This revision of SafeVoid has no secure element and the repository deliberately
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
