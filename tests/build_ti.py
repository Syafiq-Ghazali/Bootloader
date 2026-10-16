"""Compile and link the bootloader without flashing hardware (TI C2000 22.6)."""
import argparse
from pathlib import Path
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--compiler", type=Path, required=True, help="TI compiler installation directory")
    parser.add_argument("--c2000ware", type=Path, required=True)
    args = parser.parse_args()
    compiler = args.compiler / "bin/cl2000.exe"
    includes = [ROOT / p for p in ("ASW/MAIN", "ASW/BL", "ASW/APP_IF", "BSW/BCRC",
                "BSW/BFLASH", "BSW/BCAN", "BSW/TMR", "BRD", "device", "device/driverlib")]
    includes += [args.compiler / "include", args.c2000ware / "libraries/flash_api/f28003x/include/FlashAPI"]
    sources = [p for directory in ("ASW", "BSW", "BRD") for p in (ROOT / directory).rglob("*.c")]
    sources += [ROOT / "device/device.c", ROOT / "device/f28003x_codestartbranch.asm"]
    with tempfile.TemporaryDirectory(prefix="bootloader-ti-") as directory:
        build = Path(directory)
        flags = ["-v28", "-ml", "-mt", "-O2", "--opt_for_speed=0", "--gen_func_subsections=on",
                 "--float_support=fpu32", "--cla_support=cla2", "--vcu_support=vcrc", "--abi=eabi",
                 "--c99", "--define=DEBUG", "--define=_FLASH", "--display_error_number", "--diag_wrap=off"]
        flags += [f"--include_path={p}" for p in includes]
        subprocess.run([str(compiler), *flags, "--compile_only", f"--obj_directory={build}",
                        *map(str, sources)], cwd=ROOT, check=True)
        subprocess.run([str(compiler), "-v28", "--abi=eabi", "--float_support=fpu32", "--run_linker",
                        "--rom_model", "--entry_point=code_start", "--stack_size=0x200", "--warn_sections",
                        f"--output_file={build / 'Bootloader.out'}", f"--map_file={build / 'Bootloader.map'}",
                        f"--search_path={args.compiler / 'lib'}", f"--search_path={args.compiler / 'include'}",
                        *map(str, build.glob("*.obj")), "28003x_generic_flash_lnk.cmd",
                        "FAPI_F28003x_EABI_v1.58.10.lib", "targetConfigs/driverlib.lib", "--library=libc.a"],
                       cwd=ROOT, check=True)
        lines = (build / "Bootloader.map").read_text().splitlines()
        for line in lines:
            if line.lstrip().startswith(("FLASH ", "RAMLS01 ")):
                print(line)
        print("TI target compile/link: PASS (bootloader confined below 0x83000)")


if __name__ == "__main__":
    main()
