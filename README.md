# anymapper

x64 Windows kernel driver mapper. It loads an unsigned/arbitrary `.sys` driver
into kernel memory from user-mode by abusing [`anycall`](https://github.com/kkent030315/anycall)
through a vulnerable driver.

> This is a fork of [kkent030315/anymapper](https://github.com/kkent030315/anymapper)
> with some WIP issues fixed. See [Fixes applied](#fixes-applied).

## Overview

`anymapper` reflects a driver image into memory, applies base relocations and
resolves its imports, copies it into a kernel non-paged pool allocation, and
executes it through a real `DRIVER_OBJECT` created with `IoCreateDriver`.

All kernel work (pool allocation, memory copies, routine lookups, driver
creation) is performed with `anycall`, which achieves kernel code execution by
hooking a syscall stub via a physical-memory mapping. See the `anycall`
submodule for details.

## Requirements

- x64 Windows.
- Administrator privileges (open the anycall device and load the driver).
- The anycall vulnerable driver (`anydrv64.sys`) loaded, and test-signing
  enabled or DSE disabled.
- Visual Studio 2019/2022 with the v142 toolset (or adjust `PlatformToolset`).

## Build

Open `anymapper.sln` and build `Release | x64`.

The solution references `libanycall` as `..\anycall\libanycall\libanycall.vcxproj`.
Make sure the anycall checkout is available at that relative path, or update the
project reference in `anymapper.sln`. Build artifacts are placed in
`x64\Release\`:

| File | Description |
|---|---|
| `anymapper.exe` | Driver mapper |
| `libanycall64.lib` | Static anycall helper library |

## Usage

```sh
anymapper.exe <driver_path>
```

Example:

```sh
anymapper.exe CustomDriver.sys
```

`anymapper` initializes anycall with the default proxy
(`ntdll.dll!NtTraceControl`), maps the given driver image, resolves its
imports, creates a driver object and calls its `DriverEntry`.

## How it works

1. **Read & map the image (user-mode).** The driver file is read into a buffer
   and copied into a `VirtualAlloc`'d region of `SizeOfImage` bytes. The copy is
   clamped to `SizeOfImage`, so appended data (e.g. an Authenticode
   certificate) does not overflow the buffer.
2. **Validate.** DOS/NT signatures and 64-bit optional header are checked.
3. **Allocate kernel memory.** `ExAllocatePoolWithTag(NonPagedPool, SizeOfImage)`
   is invoked through anycall.
4. **Fix sections.** Raw section data is copied to each section's virtual
   address.
5. **Relocate.** A base relocation table (`IMAGE_DIRECTORY_ENTRY_BASERELOC`) is
   applied with `delta = kernel_buffer - ImageBase`.
6. **Resolve imports.** For every imported module, the module base is located
   with anycall; every `ntoskrnl.exe` export is resolved with
   `MmGetSystemRoutineAddress` and written into the **IAT** (`FirstThunk`).
7. **Copy to kernel.** The fully prepared image is copied into the kernel
   allocation, and the mapped entry point is
   `kernel_buffer + AddressOfEntryPoint`.
8. **Execute.** `IoCreateDriver(NULL, entry_point)` lets the kernel build a real
   `DRIVER_OBJECT` and call the mapped `DriverEntry(DriverObject, RegistryPath)`
   with valid arguments.

## Fixes applied

### anymapper

- **IAT is patched correctly.** `pe::resolve_imports` used to write the resolved
  addresses into the `OriginalFirstThunk` (import lookup table) instead of the
  `FirstThunk` (the IAT the code actually calls through). It now patches the
  IAT, falls back to `FirstThunk` when `OriginalFirstThunk` is absent, and
  skips imports by ordinal. *(Fixed: "Can't make API calls from IAT".)*
- **Relocation no longer fails.** `pe::relocate_image` returned failure when the
  image had no base relocation table, used `IsBadReadPtr`, and compared an RVA
  against an absolute range. It now treats a missing relocation table as a
  no-op (with a warning), validates the directory and every block against
  `SizeOfImage`, terminates at the directory end, and is guarded by SEH.
  *(Fixed: `failed to relocate image`.)*
- **Consistent image mapping.** The delta, the amount copied and the entry point
  were computed from two different schemes (one subtracting the header size,
  one not). The mapper now maps the full image:
  `delta = kernel_buffer - ImageBase`, copies `SizeOfImage` bytes and computes
  `entry_point = kernel_buffer + AddressOfEntryPoint`.
- **Signed-driver buffer overflow.** The file->image copy is clamped to
  `SizeOfImage` instead of copying the whole (possibly certificate-appended)
  file into a `SizeOfImage` buffer.
- **Real `DRIVER_OBJECT`.** The entry point used to be called directly with
  `MmGetSystemRoutineAddress` passed as a fake `DriverObject`, which crashes any
  driver that touches `DriverObject` (e.g. `IoCreateDevice`). The mapper now
  calls `IoCreateDriver`, so `DriverEntry` receives a genuine `DRIVER_OBJECT`
  and `RegistryPath`. *(Fixed: "Can't make API calls ... nor function
  pointer".)*
- **Mapped image is not freed on success.** The kernel allocation is
  intentionally leaked because the driver object keeps executing code from it.
  It is only freed if the driver fails to initialize.
- **Empty sections** (`SizeOfRawData == 0`, e.g. `.bss`) are skipped instead of
  issuing a zero-size `memcpy`.

### anycall (custom fork)

The bundled `anycall` fork carries fixes for newer Windows builds:

- `LoadLibraryEx(..., DONT_RESOLVE_DLL_REFERENCES)` for `ntoskrnl.exe` so
  kernel-only imports do not hang the loader.
- Physical-memory scan clamps each chunk to the range end.
- `syscall_handler` `E9` jump thunk is resolved before patching.
- `SYSCALL` macro argument placement fixed.
- `replace_systemroot` guarded against `npos`; module query buffer bounded;
  logs flushed.

## Notes and limitations

- The mapped driver is **not** inserted into `PsLoadedModuleList`, so APIs that
  enumerate loaded modules will not see it, and `DriverObject->DriverStart` may
  be `NULL` even though the code is running from the mapped image.
- `IoCreateDriver` is called with a `NULL` name, letting the kernel pick a
  default. This avoids passing user-mode pointers into kernel code.
- The kernel allocation is made with `NonPagedPool` (`NonPagedPoolExecute`).
  The pool must be executable for `DriverEntry` to run; if a system enforces
  NX pool, a bugcheck (`ATTEMPTED_EXECUTE_OF_NOEXECUTE_MEMORY`) will occur.
- Driver initialization runs once. Drivers that rely on a service registry key,
  a real loaded image, or `DriverObject` fields populated by the I/O manager may
  still misbehave.

## License

MIT. Original copyright Kento Oki, with modifications for this fork.
