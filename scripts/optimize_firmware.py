# AI GENERATED CODE !!!!!

#!/usr/bin/env python3

"""
Firmware Optimization Script for Thermal Camera Project
"""

Import ('env')
import os
import sys

class Colors:
    HEADER = '\033[95m'
    OKBLUE = '\033[94m'
    OKCYAN = '\033[96m'
    OKGREEN = '\033[92m'
    WARNING = '\033[93m'
    FAIL = '\033[91m'
    ENDC = '\033[0m'
    BOLD = '\033[1m'
    UNDERLINE = '\033[4m'

def print_colored(text, color):
    """Print colored text to terminal"""
    print(f"{color}{text}{Colors.ENDC}")

def analyze_memory(source, target, env):
    """Analyze memory usage after build"""
    print_colored("\n" + "="*60, Colors.HEADER)
    print_colored("  THERMAL CAMERA - MEMORY ANALYSIS", Colors.HEADER)
    print_colored("="*60, Colors.HEADER)
    
    firmware_path = str(target[0])
    
    if not os.path.exists(firmware_path):
        print_colored("⚠ Firmware file not found!", Colors.WARNING)
        return
    
    firmware_size = os.path.getsize(firmware_path)
    firmware_size_kb = firmware_size / 1024
    
    print_colored(f"\n Firmware Size: {firmware_size_kb:.2f} KB ({firmware_size} bytes)", Colors.OKBLUE)

    flash_sizes = {
        "4MB": 4 * 1024 * 1024,
        "8MB": 8 * 1024 * 1024,
        "16MB": 16 * 1024 * 1024
    }
    flash_size = flash_sizes.get(env.GetProjectOption("board_upload.flash_size", "4MB"), flash_sizes["4MB"])
    flash_usage_percent = (firmware_size / flash_size) * 100
    
    print_colored(f"Flash Usage: {flash_usage_percent:.2f}%", Colors.OKCYAN)
    if flash_usage_percent > 90:
        print_colored("WARNING: Flash usage is very high!", Colors.FAIL)
    elif flash_usage_percent > 75:
        print_colored("Flash usage is getting high", Colors.WARNING)
    else:
        print_colored("Flash usage is healthy", Colors.OKGREEN)
    size_tool = env.get("SIZETOOL", "xtensa-esp32s3-elf-size")
    
    try:
        import subprocess
        result = subprocess.run(
            [size_tool, firmware_path],
            capture_output=True,
            text=True
        )
        
        if result.returncode == 0:
            print_colored("\n Section Breakdown:", Colors.HEADER)
            print(result.stdout)
            
            lines = result.stdout.strip().split('\n')
            if len(lines) >= 2:
                values = lines[1].split()
                if len(values) >= 3:
                    text_size = int(values[0])
                    data_size = int(values[1])
                    bss_size = int(values[2])
                    
                    total_ram = data_size + bss_size
                    available_ram = 512 * 1024
                    ram_usage_percent = (total_ram / available_ram) * 100
                    
                    print_colored(f"\n Estimated RAM Usage:", Colors.OKBLUE)
                    print_colored(f"   Data: {data_size} bytes", Colors.OKCYAN)
                    print_colored(f"   BSS:  {bss_size} bytes", Colors.OKCYAN)
                    print_colored(f"   Total: {total_ram} bytes ({ram_usage_percent:.2f}%)", Colors.OKCYAN)
                    
                    if ram_usage_percent > 80:
                        print_colored("WARNING: RAM usage is high!", Colors.FAIL)
                    elif ram_usage_percent > 60:
                        print_colored("RAM usage is moderate", Colors.WARNING)
                    else:
                        print_colored("RAM usage is healthy", Colors.OKGREEN)
    
    except Exception as e:
        print_colored(f"\nCould not analyze sections: {e}", Colors.WARNING)
    
    print_colored("\n" + "="*60 + "\n", Colors.HEADER)

def optimize_build_flags(env):
    print_colored("\n Applying firmware optimizations...", Colors.OKBLUE)
    optimization_flags = [
        "-Os",                          # Optimize for size
        "-ffunction-sections",          # Place each function in its own section
        "-fdata-sections",              # Place each data in its own section
        "-Wl,--gc-sections",            # Remove unused sections
        "-fno-exceptions",              # Disable C++ exceptions (saves space)
        "-fno-rtti",                    # Disable runtime type info (saves space)
        "-fmerge-all-constants",        # Merge identical constants
        "-fno-threadsafe-statics",      # Disable thread-safe statics
        "-flto",                        # Link-time optimization
    ]
    
    # Apply flags
    for flag in optimization_flags:
        if flag not in env['CCFLAGS']:
            env.Append(CCFLAGS=[flag])
        if flag.startswith("-Wl,") and flag not in env['LINKFLAGS']:
            env.Append(LINKFLAGS=[flag])
    
    print_colored(" Optimization flags applied", Colors.OKGREEN)

def pre_build_info(source, target, env):
    """Display build information before compilation"""
    print_colored("\n" + "="*60, Colors.HEADER)
    print_colored("  THERMAL CAMERA - BUILD START", Colors.HEADER)
    print_colored("="*60, Colors.HEADER)
    
    print_colored(f"\n Platform: {env.get('PIOPLATFORM')}", Colors.OKBLUE)
    print_colored(f" Board: {env.get('BOARD')}", Colors.OKBLUE)
    print_colored(f" Framework: {env.get('PIOFRAMEWORK')[0]}", Colors.OKBLUE)
    
    print_colored("\n  Build Configuration:", Colors.OKCYAN)
    print_colored(f"   Optimization: {env.get('BUILD_FLAGS', ['N/A'])[0] if env.get('BUILD_FLAGS') else 'N/A'}", Colors.OKCYAN)
    
    print_colored("\n" + "="*60 + "\n", Colors.HEADER)

def post_build_actions(source, target, env):
    """Actions to perform after successful build"""
    print_colored("\n Build completed successfully!", Colors.OKGREEN)
    analyze_memory(source, target, env)
    print_colored(" Optimization Tips:", Colors.HEADER)
    tips = [
        "Use const for read-only data to store in flash instead of RAM",
        "Consider using PROGMEM for large constant arrays",
        "Reduce frame buffer size if memory is tight",
        "Disable features you don't need (edge detection, etc.)",
        "Use static functions when possible",
    ]
    for tip in tips:
        print_colored(f"  {tip}", Colors.OKCYAN)
    print()

optimize_build_flags(env)
env.AddPreAction("buildprog", pre_build_info)
env.AddPostAction("buildprog", post_build_actions)

def analyze_size_only(target, source, env):
    """Custom target to analyze size without rebuilding"""
    firmware_path = os.path.join(env.subst("$BUILD_DIR"), "firmware.bin")
    if os.path.exists(firmware_path):
        analyze_memory(None, [firmware_path], env)
    else:
        print_colored("No firmware found. Build first!", Colors.WARNING)
env.AlwaysBuild(env.Alias("analyze", None, analyze_size_only))

print_colored("Optimization script loaded", Colors.OKGREEN)